// Cat Box Pocket: a pocket sampler (Pico Plus 2 + WM8960). The whole instrument lives here,
// without any hardware: the Pico layer (../pico) only moves audio, keys, the screen and the flash.
// The same code runs on the PC in the tests (../host).
#pragma once
#include <cstdint>
#include <cstddef>

namespace pocket {

constexpr int SR = 48000;            // engine sample rate
constexpr int NSLOT = 64;            // 4 banks x 16 pads
constexpr int NBANK = 4;
constexpr int NVOICE = 8;
constexpr int NPAT = 16;
constexpr int NSTEP = 16;
constexpr int NTRIG = 8;             // sounds on one step
constexpr int NBLOCK = 160;          // sample blocks in the pool (a chop shares one block)
constexpr int NCHAIN = 64;
constexpr int NFX = 16;
constexpr int8_t NOLOCK = -128;

// keys: 0..15 pads, then the function keys, then the encoder pushes
enum Key : uint8_t { K_PAD0 = 0, K_SOUND = 16, K_PATTERN, K_BPM, K_FX, K_SHIFT, K_REC, K_PLAY, K_WRITE, K_ENCA, K_ENCB, NKEY };

// ------------------------------------------------------------------ storage
// The sample memory (PSRAM on the Pico) is one big int16 array. A block is a run of it;
// a slot plays a part of a block (chop slices share one block).
struct Block { uint32_t off = 0, len = 0; uint8_t refs = 0; };

struct Slot {
    int16_t block = -1;              // -1 = empty
    uint32_t start = 0, len = 0;     // within the block, in stored samples
    uint32_t rate = SR;              // the rate it was stored at (lo-fi = 24000)
    // sound parameters (the encoder pages)
    float pitch = 0;                 // semitones
    float filter = 0;                // -1 low-pass .. 0 open .. +1 high-pass
    float trimA = 0, trimB = 1;      // start / end within the slot (0..1)
    bool reverse = false, loopHold = false;
    bool stretch = false; uint8_t beats = 0;   // time-stretch to the tempo (beats 0 = auto)
    bool freeze = false;             // granular freeze: plays a frozen grain cloud
    float volume = 0.8f, delay = 0, reverb = 0;
    bool saved = false;              // its WAV on the disk is up to date
    bool empty () const { return block < 0 || len == 0; }
};

struct Trig { uint8_t slot = 0; int8_t pitch = NOLOCK; int8_t filter = NOLOCK; };   // filter lock in -100..100
struct Step { Trig t[NTRIG]; uint8_t n = 0; uint8_t fx = 0; };                    // fx: 0 none, 1..16 punch-in
struct Pattern { Step s[NSTEP]; };

// A block device (512-byte sectors): the flash on the Pico, a RAM image in the tests.
struct BlockDev {
    virtual ~BlockDev () = default;
    virtual uint32_t sectors () const = 0;
    virtual bool read (uint32_t sector, uint8_t* buf) = 0;
    virtual bool write (uint32_t sector, const uint8_t* buf) = 0;
    virtual void flush () {}
};

// ------------------------------------------------------------------ the engine
struct Voice {
    bool on = false, held = false;
    uint8_t slot = 0, pad = 0;
    float pos = 0, inc = 1, gain = 1, env = 0, rel = 0;
    float lp = 0, bp = 0;            // state-variable filter
    float cut = 1, hp = 0;           // filter setting for this note
    // time-stretch / freeze grains
    bool grains = false; float head = 0, headInc = 0, gPos[2] = {0, 0}, gPh[2] = {0, 0.5f}, gLen = 2400, gStep = 0;
    uint32_t age = 0;
};

class Engine {
public:
    // pool: the sample memory, count in int16 samples
    void init (int16_t* pool, uint32_t count, uint32_t seed = 12345);

    // audio: in = mono input, out = interleaved stereo; n frames
    void process (const float* in, float* out, int n);

    // ---- sounds
    void noteOn (int slot, int pad, float semis = 0, float vel = 1, int8_t filterLock = NOLOCK);
    void noteOff (int pad);
    void allOff ();
    // ---- recording (mic / line, the output = resampling, the looper)
    void recStart (int slot, bool resample);
    void recStop ();
    bool recording () const { return recSlot >= 0; }
    float recSeconds () const { return recLen / float(recRate()); }
    float memoryFree () const;                  // seconds at 48 kHz
    // ---- sequencer
    void play (bool on);
    bool playing () const { return running; }
    void toggleStep (int step, int slot, int8_t pitch = NOLOCK);
    bool hasTrig (int pat, int step, int slot) const;
    Trig* trig (int pat, int step, int slot);
    void liveRecord (int slot, int8_t pitch);    // quantised to the nearest step
    void euclid (int slot, int hits, int rotate);
    void chaos (float amount);
    void clearPattern (int pat);
    void setChain (const uint8_t* pats, int n);
    // ---- sample tools
    int chop (int slot);                         // returns the bank it filled or -1
    void clearSlot (int slot);
    bool assign (int slot, const int16_t* data, uint32_t len, uint32_t rate);   // copies into the pool
    uint32_t slotData (int slot, const int16_t** data) const;
    // ---- looper (overdub into a slot, one bar long)
    enum LoopState { LOOP_OFF, LOOP_REC, LOOP_PLAY };
    void looperNext (int slot);
    LoopState looper () const { return loopState; }

    // ---- state (the UI reads and changes these directly)
    Slot slots[NSLOT];
    Pattern pats[NPAT];
    uint8_t chain[NCHAIN] = {0}; int chainLen = 1, chainPos = 0;
    int pat = 0;                     // pattern playing
    int step = -1;                   // step playing
    float bpm = 120, swing = 0;
    float delayAmt = 0.3f, reverbAmt = 0.3f, master = 0.8f, inGain = 1;
    bool lofi = false;
    int fx = 0;                      // punch-in effect held (1..16)
    int rollSlot = -1, rollPad = 0; float rollDiv = 4;   // roll: retrigger every 1/rollDiv beat
    float level = 0;                 // output meter
    uint32_t frames = 0;

    Voice voices[NVOICE];
    Block blocks[NBLOCK];
    int16_t* mem = nullptr; uint32_t memSize = 0, memUsed = 0;

    // helpers the UI uses too
    float stepSamples () const { return SR * 60.0f / bpm / 4.0f; }
    uint32_t rng ();
    int newBlock (uint32_t len);                 // reserve len samples at the end of the pool
    void release (int block);                    // drop a reference (frees and compacts at 0)
    int recSlot = -1;
    uint32_t recRate () const { return lofi ? SR / 2 : SR; }

private:
    void startVoice (Voice& v, int slot, int pad, float semis, float vel, int8_t filterLock);
    float voiceSample (Voice& v);
    void seqTick ();
    void compact ();
    float fxProcess (float x);
    float histAt (float pos) const;

    bool running = false;
    double stepClock = 0; int nextStep = 0;
    uint32_t seed = 1;
    // recording
    bool recResample = false; int recBlock = -1; uint32_t recLen = 0, recMax = 0; float recAcc = 0; int recPhase = 0;
    double stepLenNow = 0;
    // looper
    LoopState loopState = LOOP_OFF; int loopSlot = -1; uint32_t loopPos = 0;
public:
    volatile bool busy = false;      // the pool is being moved: process() plays silence
private:
    // roll
    double rollClock = 0;
    // effects
    // (int16 buffers: the Pico has 520 KB of RAM)
    static constexpr int DLEN = SR * 3 / 4;        // delay: 0.75 s (3/16 at 60 BPM)
    int16_t dbuf[DLEN] = {0}; int dpos = 0; float dlp = 0;
    static constexpr int HLEN = SR;                // the last second of the dry mix, for the punch-in effects
    int16_t hist[HLEN] = {0}; int hpos = 0;
    int fxActive = 0; int fxStart = 0; uint32_t fxT = 0; float fxRead = 0; float fxEnv = 0;
    float crushHold = 0; int crushCount = 0;
    float fic1 = 0, fic2 = 0, sweepG = 0.1f;        // master sweep filter
    float transpose = 1;                           // octave effects
    int seqFx = 0;                                 // effect locked on the playing step
    // reverb (Freeverb-like, mono in, two outputs)
    static constexpr int NC = 4;
    float comb[NC][1700] = {{0}}; int combLen[NC] = {1116, 1188, 1277, 1356}; int combPos[NC] = {0}; float combLp[NC] = {0};
    float ap[2][560] = {{0}}; int apLen[2] = {556, 441}; int apPos[2] = {0, 0};
    float sends[2] = {0, 0};
};

// ------------------------------------------------------------------ the UI (keys, encoders, screen, LEDs)
class Ui {
public:
    explicit Ui (Engine& e) : eng (e) {}
    void key (int k, bool down);
    void encoder (int which, int delta);
    void tick (uint32_t ms);                     // timeouts, roll, messages
    void render (uint8_t* fb) const;             // 128x64, 1 bit, SSD1306 page layout (128 x 8 pages)
    void leds (uint8_t* rgb) const;              // 16 x RGB
    // requests to the platform (it clears them when done)
    bool wantSave = false, wantLoad = false;
    const char* message () const { return msgTime ? msg : nullptr; }
    void say (const char* m, uint32_t ms = 1200);

    int battery = -1;                // percent, -1 = unknown (the platform sets it)
    bool lineIn = false;             // a cable in the line jack
    bool usbDisk = false;            // USB disk mode: the computer owns the flash
    int sel = 0;                     // selected slot
    int bank = 0;
    int page = 0;                    // encoder page 0..4
    bool stepMode = false, keysMode = false;
    float chaosAmt = 0.5f;
    int euclidHits = 4, euclidRot = 0;
    bool held[NKEY] = {false};
    uint32_t now = 0;

private:
    Engine& eng;
    int heldStep = -1; bool stepTouched = false;
    uint32_t taps[4] = {0}; int ntap = 0;
    char msg[24] = {0}; uint32_t msgTime = 0;
    uint8_t chainBuf[NCHAIN]; int chainN = 0; bool patHeldPressed = false;
    bool soundTouched = false, bpmTouched = false, fxTouched = false, shiftTouched = false, recUsed = false;
    bool writeLive = false, fxTouchedWhileWrite = false;
};

// ------------------------------------------------------------------ storage: FAT16 volume with WAV files
// The flash holds a small FAT16 disk: PROJECT.CBP + A01.WAV .. D16.WAV. Over USB it is a normal disk.
namespace store {
    bool format (BlockDev& dev, const char* label = "CATPOCKET");
    bool mounted (BlockDev& dev);
    bool save (BlockDev& dev, const Engine& e, const Ui& ui);
    bool load (BlockDev& dev, Engine& e, Ui& ui);           // also takes any other WAVs into empty slots
    // tiny file API (8.3 names in the root directory)
    int list (BlockDev& dev, char names[][13], uint32_t* sizes, int max);
    bool readFile (BlockDev& dev, const char* name, uint8_t* buf, uint32_t max, uint32_t* got);
    bool writeFile (BlockDev& dev, const char* name, const uint8_t* data, uint32_t len);
    bool remove (BlockDev& dev, const char* name);
    // WAV: any PCM 8/16/24/32 or float, mono or stereo -> mono int16
    bool parseWav (const uint8_t* buf, uint32_t len, int16_t* out, uint32_t max, uint32_t* frames, uint32_t* rate);
    uint32_t makeWav (const int16_t* data, uint32_t frames, uint32_t rate, uint8_t* out, uint32_t max);
}

} // namespace pocket

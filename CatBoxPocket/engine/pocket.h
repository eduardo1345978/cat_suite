// Cat Box Pocket: a pocket sampler (Pico Plus 2 + WM8960). The whole instrument lives here,
// without any hardware: the Pico layer (../pico) only moves audio, keys, the screen and the disks.
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
constexpr int NSTEP = 64;            // longest pattern (4 pages of 16)
constexpr int NTRIG = 8;             // sounds on one step
constexpr int NBLOCK = 160;          // sample blocks in the pool (a chop shares one block)
constexpr int NCHAIN = 64;
constexpr int NSONG = 64;
constexpr int NPOLY = 8;             // sounds with their own length in one pattern
constexpr int NFX = 16;
constexpr int NPROJ = 16;
constexpr int NSCALE = 11;
constexpr int ARP_PAD = 200;         // the voice id the arpeggiator plays on
constexpr int8_t NOLOCK = -128;

// keys: 16 pads, 20 function keys, 4 encoder pushes
enum Key : uint8_t {
    K_PAD0 = 0,
    K_SOUND = 16, K_PATTERN, K_BPM, K_FX, K_SHIFT, K_REC, K_PLAY, K_WRITE,
    K_MUTE, K_COPY, K_PASTE, K_CLEAR, K_UNDO, K_SONG, K_FILL, K_ACCENT, K_KIT, K_MENU, K_SCENE, K_LEN,
    K_ENCA, K_ENCB, K_ENCC, K_ENCD, NKEY };

// step conditions (Elektron style)
enum Cond : uint8_t { C_ALWAYS, C_1_2, C_2_2, C_1_3, C_2_3, C_3_3, C_1_4, C_2_4, C_3_4, C_4_4, C_FILL, C_NOTFILL, C_FIRST, C_NOTFIRST, C_P25, C_P50, C_P75, NCOND };

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
    float volume = 0.8f, pan = 0;
    float trimA = 0, trimB = 1;      // start / end within the slot (0..1)
    bool reverse = false, loopHold = false;
    bool stretch = false; uint8_t beats = 0;   // time-stretch to the tempo (beats 0 = auto)
    bool freeze = false;             // granular freeze: plays a frozen grain cloud
    float attack = 0, decay = 0;     // seconds; decay 0 = play to the end
    uint8_t choke = 0;               // 1..4: a new sound of the group cuts the others
    float drive = 0;                 // 0..1 saturation
    float lfoRate = 2, lfoDepth = 0; uint8_t lfoTarget = 0;   // 0 pitch, 1 filter, 2 volume, 3 pan
    uint8_t vintage = 0;             // 0 off, 1 SP-1200 (12 bit 26 kHz), 2 S950 (12 bit 39 kHz), 3 8 bit 22 kHz
    float delay = 0, reverb = 0;
    bool duckSource = false;         // this sound ducks the others (sidechain)
    bool mute = false;               // muted in the sequencer
    bool saved = false;              // its WAV on the disk is up to date
    bool empty () const { return block < 0 || len == 0; }
};

struct Trig { uint8_t slot = 0; int8_t pitch = NOLOCK; int8_t filter = NOLOCK; uint8_t vel = 100; int8_t pan = NOLOCK; uint8_t cond = C_ALWAYS; };
struct Step { Trig t[NTRIG]; uint8_t n = 0; uint8_t fx = 0; };                    // fx: 0 none, 1..16 punch-in
struct Pattern {
    Step s[NSTEP];
    uint8_t len = 16;                                                             // steps
    uint8_t polySlot[NPOLY] = {0}, polyLen[NPOLY] = {0}; uint8_t nPoly = 0;        // sounds with their own length
};
struct SongRow { uint8_t pat = 0, repeats = 1; };

// a scene: what the crossfader blends to (per sound pitch, filter, volume; master delay and reverb)
struct Scene { bool set = false; float pitch[NSLOT], filter[NSLOT], volume[NSLOT]; float delay = 0, reverb = 0; };

// A block device (512-byte sectors): the flash or the microSD card on the Pico, a RAM image in the tests.
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
    float pos = 0, inc = 1, gain = 1, panL = 1, panR = 1, pan = 0, basePan = 0;
    float env = 0, attInc = 1, dec = 1, decCoef = 1, relRate = 0;   // attack / decay / release
    float lp = 0, bp = 0;            // state-variable filter
    float cut = 0, baseCut = 0, hp = 0;   // filter setting for this note and its coefficient (0 = off)
    float lfoPh = 0, lfoMul = 1, lfoGain = 1; uint32_t ctl = 0;
    float hold = 0; float holdPh = 0;   // vintage sample-and-hold
    // time-stretch / freeze grains
    bool grains = false; float head = 0, headInc = 0, gPos[2] = {0, 0}, gPh[2] = {0, 0.5f}, gLen = 2400;
    uint32_t age = 0;
};

class Engine {
public:
    // pool: the sample memory, count in int16 samples
    void init (int16_t* pool, uint32_t count, uint32_t seed = 12345);

    // audio: in = mono input, out = interleaved stereo; n frames
    void process (const float* in, float* out, int n);

    // ---- sounds
    void noteOn (int slot, int pad, float semis = 0, float vel = 1, int8_t filterLock = NOLOCK, int8_t panLock = NOLOCK);
    void noteOff (int pad);
    void allOff ();
    // ---- recording (mic / line, the output = resampling)
    void recStart (int slot, bool resample);
    void recStop ();
    bool recording () const { return recSlot >= 0; }
    bool recWaiting () const { return recSlot >= 0 && recArmed; }
    float recSeconds () const { return recLen / float (recRate ()); }
    float memoryFree () const;                  // seconds at 48 kHz
    // ---- sequencer
    void play (bool on);
    bool playing () const { return running; }
    void toggleStep (int step, int slot, int8_t pitch = NOLOCK, uint8_t vel = 100);
    bool hasTrig (int pat, int step, int slot) const;
    Trig* trig (int pat, int step, int slot);
    void liveRecord (int slot, int8_t pitch, uint8_t vel = 100);    // quantised to the nearest step
    void euclid (int slot, int hits, int rotate);
    void chaos (float amount);
    void clearPattern (int pat);
    void setChain (const uint8_t* pats, int n);
    int slotLen (int pat, int slot) const;      // the sound's own length (polymeter) or the pattern's
    void setSlotLen (int pat, int slot, int len);
    void nudge (int pat, int slot, int dir);    // slot -1 = every sound
    // song: rows of (pattern, repeats)
    SongRow song[NSONG]; int songLen = 0; bool songMode = false; int songRow = 0, songRep = 0;
    void songAdd (int pat) { if (songLen < NSONG) song[songLen++] = SongRow { (uint8_t) pat, 1 }; }
    // undo: one step back (and forward again)
    void snapshot ();
    void undo ();
    // ---- sample tools
    int chop (int slot);                         // returns the bank it filled or -1
    void clearSlot (int slot);
    bool assign (int slot, const int16_t* data, uint32_t len, uint32_t rate);   // copies into the pool
    uint32_t slotData (int slot, const int16_t** data) const;
    void trimNormalize (int slot);               // cut the silence at both ends, peak to -1 dB
    // ---- looper (overdub into a slot, one bar long)
    enum LoopState { LOOP_OFF, LOOP_REC, LOOP_PLAY };
    void looperNext (int slot);
    LoopState looper () const { return loopState; }
    // ---- scenes and crossfader
    void sceneStore (int ab);
    void sceneClear () { scenes[0].set = scenes[1].set = false; }
    // ---- keys mode helpers: scale and arpeggiator
    int padSemis (int pad) const;                // pad -> semitones in the scale
    void arpPress (int pad);
    void arpRelease (int pad);
    // ---- MIDI and sync (the platform feeds bytes in and drains bytes out)
    void midiIn (uint8_t b);
    int midiOutPop ();                           // -1 = empty
    void syncPulseIn ();                         // a pulse on the sync jack (2 per beat, like a Pocket Operator)
    bool syncPulseOut = false;                   // the platform sends a pulse and clears it
    // ---- offline rendering (export)
    uint32_t renderStart (bool wholeSong);       // stops, clears the effects and plays from the top; returns the frames
    void renderEnd ();
    uint32_t renderFrames = 0, renderMusic = 0;  // how long the export is (with / without the tail)

    // ---- state (the UI reads and changes these directly)
    Slot slots[NSLOT];
    Pattern pats[NPAT];
    uint8_t chain[NCHAIN] = {0}; int chainLen = 1, chainPos = 0;
    Scene scenes[2];
    float xfade = 0;                 // crossfader 0..1 (A..B)
    int pat = 0;                     // pattern playing
    int step = -1;                   // step playing (in the pattern)
    uint32_t loops = 0;              // how many times the pattern played since PLAY (step conditions)
    bool fill = false;               // FILL held
    float bpm = 120, swing = 0;
    float delayAmt = 0.3f, reverbAmt = 0.3f, master = 0.8f, inGain = 1;
    float compAmt = 0, duckAmt = 0;  // master compressor, sidechain ducking
    bool lofi = false, monitor = false, metronome = false, autoTrim = true, threshold = false;
    int scale = 0, root = 0;         // keys mode: scale index, root note
    int arpMode = 0; float arpDiv = 4; int arpSlot = 0;   // 0 off, 1 up, 2 down, 3 up-down, 4 random, 5 as played
    int midiChannel = 0;             // 0..15
    int syncMode = 0;                // 0 internal (sends MIDI clock), 1 follows MIDI clock, 2 follows the sync jack
    int fx = 0;                      // punch-in effect held (1..16)
    int rollSlot = -1, rollPad = 0; float rollDiv = 4;   // roll: retrigger every 1/rollDiv beat
    float level = 0, gr = 0;         // output meter, compressor gain reduction
    uint32_t frames = 0;
    volatile bool hold = false;      // the platform's audio plays silence (export, loading)
    volatile bool busy = false;      // the pool is being moved: process() plays silence
    volatile bool inProcess = false; // process() is running (the export waits for it)
    int lastArpPad = -1;

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
    static const char* condName (int c);
    static const char* scaleName (int s);
    static int scaleSize (int s);

    friend struct Renderer;
    void processRaw (const float* in, float* out, int n);   // ignores `hold` (export)

private:
    void startVoice (Voice& v, int slot, int pad, float semis, float vel, int8_t filterLock, int8_t panLock);
    float voiceSample (Voice& v);
    void voiceControl (Voice& v);
    float sceneVal (int kind, int slot, float own) const;
    void seqTick ();
    void playStep (int st, int only);
    bool condOk (const Trig& t);
    int polyIndex (int p, int slot) const;
    void fxProcess (float& l, float& r);
    float histAt (int ch, float pos) const;
    void midiOut (uint8_t a, uint8_t b, uint8_t c, int n = 3);
    void advancePattern ();
    void arpStep ();
    uint8_t polyPos[NPOLY] = {0};
    uint8_t midiOnNote[16]; int nMidiOn = 0;
    double midiClock = 0; float extHalf = 0; float extStepLen = 0;
    volatile int extPending = 0, extPulses = 0; volatile bool extStart = false, extStop = false;
    bool renderSongWas = false;

    bool running = false;
    double stepClock = 0; int nextStep = 0; uint32_t tick = 0;
    uint32_t seed = 1;
    // recording
    bool recResample = false, recArmed = false; int recBlock = -1; uint32_t recLen = 0, recMax = 0; float recAcc = 0; int recPhase = 0;
    double stepLenNow = 0;
    // looper
    LoopState loopState = LOOP_OFF; int loopSlot = -1; uint32_t loopPos = 0;
    // undo
    Pattern undoPat; Slot undoSlots[NSLOT]; int undoWhich = -1; bool undoValid = false;
    // arpeggiator
    uint8_t arpHeld[16]; int arpN = 0, arpIdx = 0, arpDir = 1; double arpClock = 0; int arpPad = -1;
    // roll
    double rollClock = 0;
    // MIDI
    uint8_t mOut[256]; volatile int mHead = 0, mTail = 0;
    uint8_t mStatus = 0, mData[2] = {0, 0}; int mCount = 0; int clockCount = 0; uint32_t lastClockFrame = 0;
    uint32_t lastPulseFrame = 0;
    // ducking / compressor / metronome
    float duckEnv = 0, compEnv = 0; float clickPh = 0, clickEnv = 0, clickHz = 1000;
    // effects
    // (int16 buffers: the Pico has 520 KB of RAM)
    static constexpr int DLEN = SR * 3 / 4;        // delay: 0.75 s (3/16 at 60 BPM)
    int16_t dbuf[DLEN] = {0}; int dpos = 0; float dlp = 0;
    static constexpr int HLEN = SR * 3 / 4;        // the last 0.75 s of the dry mix (stereo), for the punch-in effects
    int16_t hist[2][HLEN] = {{0}}; int hpos = 0;
    int fxActive = 0; int fxStart = 0; uint32_t fxT = 0; float fxRead = 0; float fxEnv = 0;
    float crushHold[2] = {0, 0}; int crushCount = 0;
    float fic1[2] = {0, 0}, fic2[2] = {0, 0}, sweepG = 0.1f;   // master sweep filter
    float transpose = 1;                           // octave effects
    int seqFx = 0;                                 // effect locked on the playing step
    // reverb (Freeverb-like, mono in, two outputs)
    static constexpr int NC = 4;
    float comb[NC][1400] = {{0}}; int combLen[NC] = {1116, 1188, 1277, 1356}; int combPos[NC] = {0}; float combLp[NC] = {0};
    float ap[2][560] = {{0}}; int apLen[2] = {556, 441}; int apPos[2] = {0, 0};
};

// ------------------------------------------------------------------ the UI (keys, encoders, screen, LEDs)
class Ui {
public:
    explicit Ui (Engine& e) : eng (e) {}
    void key (int k, bool down);
    void encoder (int which, int delta);         // 0..3 = A..D
    void fader (float x);                        // the crossfader 0..1 (A..B)
    void tick (uint32_t ms);                     // timeouts, messages
    void service ();                             // disk work (save, load, kits, export, browser): call from the main loop
    void render (uint8_t* fb) const;             // 128x64, 1 bit, page layout (128 x 8 pages)
    void leds (uint8_t* rgb) const;              // 16 x RGB
    const char* message () const { return msgTime ? msg : nullptr; }
    void say (const char* m, uint32_t ms = 1200);
    static const char* help (int key);           // what a key does (MENU + key)
    static const char* menuName (int item);

    BlockDev* disk = nullptr;        // where projects live (the microSD card or the flash)
    int project = 0;                 // 0..15
    int battery = -1;                // percent, -1 = unknown (the platform sets it)
    bool lineIn = false;             // a cable in the line jack
    bool usbDisk = false;            // USB disk mode: the computer owns the disk
    bool sdCard = false;             // the disk is the microSD card
    bool sdPresent = false;          // a card is in the slot
    int sel = 0;                     // selected slot
    int bank = 0;
    int page = 0;                    // encoder page 0..4
    int stepPage = 0;                // 0..3: which 16 steps the pads show
    bool stepMode = false, keysMode = false, fillLatch = false;
    float chaosAmt = 0.5f;
    int euclidHits = 4, euclidRot = 0;
    bool held[NKEY] = {false};
    uint32_t now = 0;
    // requests the main loop serves (service() does the disk ones; the platform the others)
    bool wantSave = false, wantLoad = false; int wantKit = -1; bool kitSave = false; int wantExport = 0;   // 1 pattern chain, 2 song
    bool wantBrowse = false; int browseLoad = -1;   // load the browsed file into this slot
    bool wantUsb = false, wantSwapDisk = false;
    // the menu and the file browser
    bool menu = false; int menuItem = 0, menuProj = 0; bool browsing = false; int browseIdx = 0;
    char files[96][13]; char fileLong[96][24]; int nFiles = 0;
    int helpKey = -1; uint32_t helpTime = 0;

private:
    Engine& eng;
    void menuChange (int d);
    void menuEnter ();
    void padDown (int pad);
    void padUp (int pad);
    int stepOf (int pad) const { return stepPage * 16 + pad; }
    uint8_t vel () const { return held[K_ACCENT] ? 127 : 100; }
    int heldStep = -1; bool stepTouched = false;
    uint32_t taps[4] = {0}; int ntap = 0;
    char msg[32] = {0}; uint32_t msgTime = 0;
    uint8_t chainBuf[NCHAIN]; int chainN = 0;
    // a modifier key that was used with something else does nothing of its own when let go
    bool used[NKEY] = {false};
    bool writeLive = false;
    float lastFader = -1;
    // clipboard: 1 step, 2 sound, 3 pattern
    int clipKind = 0; Step clipStep; Pattern clipPat; Slot clipSlot;
};

// ------------------------------------------------------------------ storage: FAT16 / FAT32 volume with WAV files
// A project = PROJnn.CBP + PnnA01.WAV .. PnnD16.WAV; kits = KITnn.CBK; exports = EXPnn.WAV.
// Over USB the disk is a normal drive; any other WAV on it can be loaded with the browser.
namespace store {
    bool format (BlockDev& dev, const char* label = "CATPOCKET", bool fat32 = false);   // FAT16 (the flash) or FAT32 (a card)
    bool mounted (BlockDev& dev);
    bool save (BlockDev& dev, const Engine& e, const Ui& ui, int project);
    bool load (BlockDev& dev, Engine& e, Ui& ui, int project);     // project 0 also takes stray WAVs into empty pads
    bool saveKit (BlockDev& dev, const Engine& e, int bank, int kit);
    bool loadKit (BlockDev& dev, Engine& e, int bank, int kit);
    bool loadWavInto (BlockDev& dev, const char* name, Engine& e, int slot);
    bool exportWav (BlockDev& dev, Engine& e, bool song, char* nameOut);
    // tiny file API (8.3 names in the root directory; long names are read for display)
    int list (BlockDev& dev, char names[][13], uint32_t* sizes, int max, char longNames[][24] = nullptr);
    bool readFile (BlockDev& dev, const char* name, uint8_t* buf, uint32_t max, uint32_t* got);
    bool writeFile (BlockDev& dev, const char* name, const uint8_t* data, uint32_t len);
    bool remove (BlockDev& dev, const char* name);
    bool exists (BlockDev& dev, const char* name);
    // WAV: any PCM 8/16/24/32 or float, mono or stereo -> mono int16
    bool parseWav (const uint8_t* buf, uint32_t len, int16_t* out, uint32_t max, uint32_t* frames, uint32_t* rate);
    uint32_t makeWav (const int16_t* data, uint32_t frames, uint32_t rate, uint8_t* out, uint32_t max);
}

} // namespace pocket

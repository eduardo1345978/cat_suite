#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

/**
    Hero-Cat's engine: a lead synth for 90s game / anime melodies.

    TWO SOUNDS (A and B) per note, mixed: classic synth waves (saw, square, supersaw, chip pulse) or
    small physical models (a plucked guitar string, a bowed string, a flute, a choir, an FM bell).

    CHORD LOCK: one key becomes a whole chord - from the song's scale (triads, 7ths, the "anime" add9) or a fixed
    shape. STRUM spreads the notes like a guitar, VOICE LEADING moves each note to the nearest one of the last chord.
    The chords (and the arp notes) can also go out as MIDI, so another plug-in can play them.

    ARP: Up / Down / Up-Down / Random, WATERFALL (notes fall from the top and keep ringing - a wash), or PATTERN
    (16 steps you draw, each with its own loudness). Swing, gate, octaves, latch. CHAOS lets it improvise:
    octave jumps, skipped steps, double hits, other chord notes.

    TEXTURE: rain, birds, a quiet room, wind chimes that play the chord's notes, wind.
    MOD: an LFO with six shapes on cutoff / pitch / pan / A-B mix / crush / volume.
    FX: drive, bit crush, chorus, delay (dotted 1/8, ping-pong), reverb, stereo width.
*/
namespace hero
{
juce::StringArray sourceNames();        // the 9 sounds
juce::StringArray chordModeNames();
juce::StringArray scaleNames();
juce::StringArray arpModeNames();
juce::StringArray rateNames();
juce::StringArray textureNames();
juce::StringArray lfoShapeNames();
juce::StringArray lfoTargetNames();
juce::StringArray keyNames();
/** "m7" for a chordKind index (-1: just the root) */
juce::String chordKindName (int kind);

enum Source { Saw, Square, Supersaw, Chip, Guitar, Strings, Flute, Choir, Bell, numSources };
enum ArpMode { ArpOff, ArpUp, ArpDown, ArpUpDown, ArpRandom, ArpWaterfall, ArpPattern };

struct Settings
{
    int srcA = Saw, srcB = -1;              // -1 = no B
    float mix = 0.0f;                       // 0 = only A, 1 = only B
    int octB = 0;                           // B: octave shift
    float detune = 0.3f, cutoff = 0.7f, reso = 0.2f, envAmt = 0.3f;
    float attack = 0.01f, decay = 0.3f, sustain = 0.8f, release = 0.3f;
    float glide = 0.0f, vibrato = 0.3f;
    int chordMode = 0, key = 0, scale = 0;
    float strum = 0.0f;
    bool voiceLead = true;
    int arpMode = 0, rate = 2, octaves = 1;
    float gate = 0.6f, swing = 0.0f, chaos = 0.0f;
    bool latch = false;
    int texture = 0; float texLevel = 0.3f;
    int lfoShape = 0, lfoTarget = 0; float lfoRate = 2.0f, lfoDepth = 0.0f;
    float drive = 0.0f, crush = 0.0f, chorus = 0.0f, delay = 0.0f, reverb = 0.0f, width = 0.0f;
    float outDb = 0.0f;
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
};

class Engine
{
public:
    Engine();
    void prepare (double sampleRate, int blockSize);
    void reset();

    /** midiIn: the keys; midiOut (may be null): the notes Hero-Cat really plays (chords, arp), for the DAW */
    void process (juce::AudioBuffer<float>&, const juce::MidiBuffer& midiIn, juce::MidiBuffer* midiOut, const Settings&);

    /** the drawn pattern: 16 steps, loudness 0 (rest) .. 1 - written by the screen, read by the audio */
    std::array<std::atomic<float>, 16> pattern;
    void randomisePattern (juce::Random&);

    /** for the screen */
    std::atomic<int> chordRoot { -1 }, chordKind { 0 }, arpStep { -1 };
    std::atomic<float> level { 0.0f }, lfoNow { 0.0f };
    std::array<std::atomic<int>, 64> noteLog;   // note | velocity << 8, newest at logPos - 1
    std::atomic<int> logPos { 0 };

    /** the notes one key becomes (with the settings), for tests */
    std::vector<int> chordFor (int key, const Settings&, const std::vector<int>& previous) const;
    int voicesSounding() const;

    static constexpr int numVoices = 16;

private:
    static constexpr int ksSize = 4096;
    struct Slot
    {
        int src = -1;
        double ph[5] {}, mph = 0.0;
        float idx = 0.0f;
        std::array<float, ksSize> ks {};
        int w = 0, excite = 0;
        float last = 0.0f, f1 = 0.0f, f2 = 0.0f, b1 = 0.0f, b2 = 0.0f, b3 = 0.0f, b4 = 0.0f, noiseLp = 0.0f;
    };
    struct Voice
    {
        bool active = false, releasing = false;
        int note = 0, owner = -1, gateLeft = -1;   // gateLeft: samples until release (-1: until the key goes up)
        float vel = 1.0f, env = 0.0f, pan = 0.5f, longRelease = 1.0f, relCoef = 0.999f;
        int stage = 0;                             // 0 attack, 1 decay/sustain, 2 release
        double hz = 440.0, targetHz = 440.0;
        double age = 0.0;
        float s1 = 0.0f, s2 = 0.0f, fenv = 0.0f;   // the filter
        Slot a, b;
    };
    struct Pending { bool on = false; int wait = 0, note = 0, owner = -1, gate = -1; float vel = 1.0f, longRelease = 1.0f; };

    void keyDown (int key, float vel, const Settings&, juce::MidiBuffer* out, int pos);
    void keyUp (int key, juce::MidiBuffer* out, int pos);
    void start (int note, float vel, int owner, int gate, float longRelease, const Settings&, juce::MidiBuffer* out, int pos);
    void startSlot (Slot&, int src, double hz, float vel);
    float renderSlot (Slot&, Voice&, double hz, const Settings&);
    void release (Voice&, juce::MidiBuffer* out, int pos);
    void arpTick (const Settings&, juce::MidiBuffer* out, int pos, int stepSamples);
    std::vector<int> arpSequence (const Settings&) const;
    void schedule (int wait, int note, float vel, int owner, int gate, float longRelease);
    void logNote (int note, float vel);
    void nameChord (const std::vector<int>&);
    float texture (const Settings&, float& r);

    double sr = 48000.0;
    std::array<Voice, numVoices> voices;
    std::array<Pending, 96> pending;
    std::vector<int> keysDown;                     // the keys the player holds
    std::vector<std::pair<int, std::vector<int>>> keyChords;   // key -> the notes it became
    std::vector<int> lastChord, pool;              // pool: the notes the arp plays
    bool latched = false, latchNow = false;
    float relSeconds = 0.3f;
    int poolCounter = 0, upDownDir = 1, waterfallPos = 0;
    double beats = 0.0, nextStep = 0.0; long long stepIndex = 0; bool clockOn = false; double lastPpq = -1.0;
    int voiceCounter = 0;
    double lastHz = 0.0; float arpVel = 0.85f;
    juce::Random rng { 4242 };

    // modulation
    double lfoPh = 0.0; float lfoHold = 0.0f, chaosWalk = 0.0f;
    // texture
    float texLp = 0.0f, texLp2 = 0.0f, brown = 0.0f, chirpT = -1.0f, chirpHz = 0.0f, chirpPan = 0.5f, windB = 0.0f, windL = 0.0f; double chirpPh = 0.0, windPh = 0.0; int chirpsLeft = 0;
    struct Ping { float env = 0.0f; double ph = 0.0, hz = 0.0; float pan = 0.5f, decay = 0.9999f; };
    std::array<Ping, 12> pings;
    // fx
    std::vector<float> chorusBuf, delayL, delayR;
    int chorusW = 0, delayW = 0;
    double chorusPh = 0.0;
    float delayLpL = 0.0f, delayLpR = 0.0f, crushHoldL = 0.0f, crushHoldR = 0.0f, crushCount = 0.0f;
    juce::Reverb verb;
    juce::SmoothedValue<float> outS;
};
} // namespace hero

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <vector>
#include "CatSounds.h"

/**
    Glitch-Cat's engine: the swarm.

    Every note you play falls apart into a swarm of "bugs" - small voices that fly round it (left / right),
    each a little out of tune with the others. A bug lives in segments: when a segment ends it picks a new
    pitch, a new place to fly to, and (when it's mad enough) starts again with a click of its envelope - so
    the swarm turns from a warm, wobbly pad (GENTLE) into rolls, ratchets and jumps (INSANE).

        bugs (synth + voice) --+
        drums (the swarm's beat, or the low keys) --+--> glitch buffer (stutter / reverse / tape stop / chop)
                                                         --> tape --> space --> volume --> limiter

    MADNESS is one big knob; four switches say what it may do: detune, rolls, stutter + reverse, odd rhythms.
    Gentle stays on Cubase's grid; past about 60 % the swarm starts to break away from it.
*/
namespace glitch
{
enum VoiceType { Whisper = 0, Vowels, Choir, Mouth, MyVoice, NumVoiceTypes };
enum BeatMode { BeatOff = 0, BeatAuto, BeatKeys, BeatBoth };
enum BeatStyle { Drill = 0, Broken, Dust, Rave, NumBeatStyles };
enum GlitchType { GlitchNone = 0, GlitchRepeat, GlitchReverse, GlitchTapeStop, GlitchChop, GlitchPitch };

juce::StringArray voiceTypeNames();
juce::StringArray beatModeNames();
juce::StringArray beatStyleNames();

/** Below this key the keys play drums (when the beat mode lets them): Cubase C2. */
constexpr int drumKeysBelow = 48;

struct Settings
{
    float madness = 0.2f;              // 0 gentle .. 1 insane
    bool detune = true, rolls = true, stutter = true, odd = true;
    float swarm = 0.5f;                // how many bugs a note becomes (2 .. 8)
    float flight = 0.5f;               // how far / fast they fly round the stereo field
    float tape = 0.3f;                 // wow, flutter, warmth
    float attack = 0.02f, release = 0.6f;    // seconds
    float synthLevel = 0.8f, tone = 0.6f, shape = 0.2f;
    float voiceLevel = 0.0f;
    int voiceType = Vowels;
    float drumLevel = 0.7f;
    int kit = 0, beatMode = BeatBoth, beatStyle = Drill;
    float space = 0.35f, volumeDb = -6.0f;
    double fallbackBpm = 120.0;
};

/** A recorded / loaded voice (mono). */
struct Sample
{
    std::vector<float> data;
    double rate = 48000.0;
    juce::String name;
};

/** What the screen shows: every bug's place, pitch and brightness. */
struct DisplayBug { std::atomic<float> x { 0.5f }, pitch { 60.0f }, bright { 0.0f }; };

class Engine
{
public:
    static constexpr int maxNotes = 12, bugsPerNote = 8;

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setSample (const Sample* s) noexcept { sample.store (s); }

    /** Host position: ppq (when playing) and tempo. */
    struct Transport { bool playing = false; double ppq = 0.0, bpm = 0.0; };

    void process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Settings&, const Transport&);

    // --- for the screen ---
    std::array<DisplayBug, maxNotes * bugsPerNote> display;
    std::array<std::atomic<int>, maxNotes> displayKey {};       // -1 = empty slot
    std::array<std::atomic<float>, 4> drumFlash {};               // kick, snare, hats, other
    std::atomic<int> glitchNow { GlitchNone };
    std::atomic<int> glitchEvents { 0 };                          // how many glitches so far (tests, screen)
    std::atomic<int> activeBugs { 0 };
    std::atomic<int> retriggers { 0 };                            // bug restarts (rolls) so far
    std::atomic<double> beatNow { 0.0 };

private:
    struct Bug
    {
        double ph = 0.0, ph2 = 0.0;           // oscillator phases (cycles)
        float offset = 0.0f, offsetTarget = 0.0f, glide = 0.001f;
        float pan = 0.5f, panTarget = 0.5f;
        double segLeft = 0.0, segLen = 1.0;   // samples
        bool retrig = false;
        float seg = 1.0f, segDecay = 1.0f, decayEnv = 1.0f;    // the segment's envelope (rolls)
        float wingPh = 0.0f, wingRate = 20.0f;
        float lp1 = 0.0f, lp2 = 0.0f;
        float vowel = 0.0f, vowelTarget = 0.0f;
        float f1a = 0.0f, f1b = 0.0f, f2a = 0.0f, f2b = 0.0f, f3a = 0.0f, f3b = 0.0f;
        double samplePos = 0.0;
        bool reverse = false;
        float ratio = 1.0f;                   // odd rhythm: this bug's own segment length
        int segCount = 0;
    };
    struct Note
    {
        bool active = false, held = false;
        int key = 60;
        float vel = 0.8f, env = 0.0f;
        uint32_t order = 0;
        std::array<Bug, bugsPerNote> bugs;
    };

    // notes
    void noteOn (int key, float vel);
    void noteOff (int key);
    void startBug (Note&, Bug&, int index, bool first);
    void renderNotes (float* l, float* r, int start, int n);

    // the beat
    struct Event { double beat; int note; float vel; };
    void scheduleBeat (double fromBeat, double toBeat);
    void addEvent (double beat, int note, float vel);
    void playDrumKey (int key, float vel, double atBeat);
    void fireEvents (double fromBeat, double toBeat, double samplesPerBeat, int blockStart, int blockLen, juce::MidiBuffer& out);

    // glitch buffer
    void glitchProcess (float* l, float* r, int n, double samplesPerBeat);
    void maybeStartGlitch (double samplesPerBeat);

    float madness() const noexcept;    // the knob + the mod wheel
    float rnd() noexcept { return random.nextFloat(); }
    float rnd2() noexcept { return random.nextFloat() * 2.0f - 1.0f; }

    double sr = 48000.0;
    Settings s;
    juce::Random random { 0x6c1c7 };
    std::array<Note, maxNotes> notes;
    uint32_t orderCounter = 0;
    std::atomic<const Sample*> sample { nullptr };
    double samplesPerBeatNow = 24000.0;
    double renderBeat = 0.0;            // the beat where the swarm is rendering now (for the shared roll choices)

    // clock
    double beat = 0.0;                  // where the swarm is (beats)
    bool clockRunning = false;
    double lastHostPpq = -1.0;
    double nextStep = 0.0;              // the next 1/16 step the beat writes (in steps)
    double takeoverUntil = -1.0;        // the low keys took the drums over until this beat
    double notesStoppedAt = -1.0;       // when the last note ended (the beat plays on to the bar's end)
    std::array<Event, 512> events;
    int numEvents = 0;
    hub::DrumKit drums;
    int kitNow = -1;
    juce::MidiBuffer drumMidi;
    juce::AudioBuffer<float> drumBuffer;

    // glitch
    std::vector<float> ringL, ringR;
    int ringMask = 0, writePos = 0;
    int glitchType = GlitchNone;
    double glitchLeft = 0.0, glitchLen = 0.0, glitchSlice = 1.0, glitchRead = 0.0, glitchSpeed = 1.0, glitchAnchor = 0.0;
    float glitchMix = 0.0f;
    double nextGlitchCheck = 0.0;       // in samples, for the free (insane) clock
    double lastGlitchStep = -1.0;

    // tape + space
    double wowPh = 0.0, flutterPh = 0.0;
    float tapeLpL = 0.0f, tapeLpR = 0.0f;
    hub::Noise hiss;
    juce::Reverb reverb;
    float lastSpace = -1.0f;
    float bend = 0.0f, modWheel = 0.0f;
};
} // namespace glitch

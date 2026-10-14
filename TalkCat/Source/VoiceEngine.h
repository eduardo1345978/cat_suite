#pragma once
#include "SafetyLimiter.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include "Vocal.h"
#include "Phonetics.h"

/**
    Talk-Cat engine: a physically modelled lead singer (glottis -> vocal tract -> lips / nose,
    run at 2x the sample rate) that sings syllables on notes.

    Two voice groups: SCORE (driven by the piano roll, with consonants placed before the beat so the
    vowel lands on it) and LIVE (MIDI / keyboard / note preview). Each group = lead + double +
    3 diatonic harmony voices. After the voices: tone (low cut, presence, air), compressor,
    saturation, tempo delay and reverb.
*/
namespace talkcat
{
constexpr int numGroups = 2;          // 0 = score, 1 = live
constexpr int voicesPerGroup = 5;     // lead, double, harmony 1-3

juce::StringArray styleNames();
juce::StringArray scaleNames();
juce::StringArray harmonyNames();     // intervals in scale steps
juce::StringArray reverbNames();
juce::StringArray attackNames();

/** Everything the engine reads from the parameters (once per block). */
struct Settings
{
    // Voice designer
    float size = 16.5f;            // vocal-tract length (cm) of the human voice
    float gender = 0.0f;           // -1 masculine .. +1 feminine (formants + source)
    float age = 0.5f;              // 0 child .. 0.5 adult .. 1 elder
    float cat = 0.0f;              // 0 human .. 1 cat (short tract, nasal, wild pitch)
    float breath = 0.15f, tension = 0.0f, rasp = 0.0f, jitter = 0.3f, nasal = 0.0f, ring = 0.3f;
    float jaw = 0.0f, lips = 0.0f, tongue = 0.0f, throat = 0.0f, bright = 0.0f;
    // Pitch expression
    float vibDepth = 30.0f, vibRate = 5.4f, vibDelay = 0.35f;   // cents, Hz, s
    float portaMs = 90.0f, overshoot = 0.3f, scoop = 0.3f, fall = 0.0f, drift = 0.3f, prep = 0.2f;
    // Diction
    float consLength = 1.0f, consStrength = 0.6f;
    int attack = 1;                // 0 soft, 1 normal, 2 hard (glottal)
    float releaseMs = 140.0f;
    int style = 0;
    float autoBreath = 0.4f;
    // Harmony + double
    int key = 0, scale = 0;
    std::array<int, 3> harmony { 0, 0, 0 };
    std::array<float, 3> harmonyLevel { 0.6f, 0.6f, 0.6f }, harmonyPan { -0.4f, 0.4f, 0.0f };
    float doubleLevel = 0.0f, doubleDetune = 9.0f, doubleDelayMs = 18.0f, width = 0.5f;
    // FX
    float lowCut = 90.0f, presence = 2.0f, air = 2.0f, comp = 0.4f, drive = 0.0f;
    float delayMix = 0.0f, delayBeats = 0.75f, delayFeedback = 0.35f;
    int reverb = 1;
    float reverbMix = 0.14f, reverbSize = 0.6f;
    float volume = 1.0f;
    float bendRange = 2.0f;
    double bpm = 120.0;
};

/** Curve lanes from the piano roll (smoothed in the engine). */
struct Expr
{
    float pitch = 0.0f;       // cents
    float dynamics = 1.0f;    // 0..2
    float breath = 0.0f;      // 0..1 extra
    float bright = 0.0f;      // -1..1
    float gender = 0.0f;      // -1..1 offset
    float tension = 0.0f;     // -1..1 offset
    float growl = 0.0f;       // 0..1
    float vibrato = 1.0f;     // 0..2 multiplier
    float cat = 0.0f;         // 0..1 extra
    float mouth = 0.0f;       // -1..1 jaw offset
};

/** One sung note. */
struct NoteEvent
{
    float pitch = 60.0f;           // MIDI note (fractional allowed)
    float velocity = 0.8f;
    Syllable syl {};
    bool legato = false;           // flows from the previous note without a new attack
    bool skipOnset = false;        // jump straight to the vowel (playback started inside the note)
    float pitchDelay = 0.0f;       // seconds before the pitch starts moving (consonants come first)
    float lengthSec = -1.0f;       // expected length (score), -1 = unknown (live)
    // per-note overrides (negative = use the global setting)
    float portaMs = -1.0f, vibDepth = -1.0f, vibRate = -1.0f, vibDelay = -1.0f;
    float consScale = 1.0f, consStrength = -1.0f;
};

class Engine
{
public:
    Engine();
    void prepare (double sampleRate, int maxBlock);
    void setSettings (const Settings&);
    const Settings& getSettings() const noexcept { return settings; }

    void noteStart (int group, const NoteEvent&);
    void noteRelease (int group, bool phraseEnd);
    void breathIn (int group, float seconds);
    void allOff (int group);
    void setExpression (int group, const Expr& e) { expr[(size_t) group] = e; }
    void pitchBend (float v) { bendValue = v; }          // -1..1 (live group)
    void modWheel (float v) { modWheelValue = v; }
    void pressure (float v) { pressureValue = v; }

    void render (float* left, float* right, int numSamples);
    bool isSounding (int group) const noexcept;

    // UI read-outs
    std::array<std::atomic<float>, numGroups> uiPitch, uiLevel;
    std::atomic<float> uiMouthOpen { 0.0f }, uiMouthWidth { 0.6f }, uiOutLevel { 0.0f };
    cat::SafetyLimiter limiter;
    std::atomic<int> uiGroup { 0 };
    std::array<std::atomic<float>, numCtrl> uiArea;

private:
    struct Voice
    {
        Glottis glottis;
        Tract tract;
        Noise rnd;
        int role = 0;                 // 0 lead, 1 double, 2..4 harmony
        float targetCm = 16.5f, currentSections = 0.0f;

        bool active = false, gate = false, releasing = false, phraseEnd = false;
        NoteEvent ev {};
        float semis = 60.0f, from = 60.0f, glideT = 1.0f, glideTime = 0.1f, glideDelay = 0.0f, interval = 0.0f;
        float env = 0.0f, noteTime = 0.0f, sinceArrive = 10.0f, relTime = 0.0f;
        float vibPhase = 0.0f, driftValue = 0.0f, driftTarget = 0.0f, driftTimer = 0.0f, scoopAmt = 0.0f;
        float idle = 0.0f;
        int pendingSamples = -1;      // double / harmony timing offset
        NoteEvent pending {};
        bool pendingRelease = false, pendingPhraseEnd = false;
        int releaseDelay = -1;

        // articulation
        std::array<Phone, 12> seq {};
        int seqLen = 0;
        float seqT = 0.0f;
        bool inCoda = false;
        Syllable syl {};
        bool hasSyl = false;
        float consVoice = 1.0f, consVoiceTarget = 1.0f, aspExtra = 0.0f;
        float fric = 0.0f, fricTarget = 0.0f, fricColour = 0.5f, fHp = 0.0f, fLast = 0.0f;
        int fricIndex = -1;
        float breathT = -1.0f, breathLen = 0.0f;
        float trillPhase = 0.0f, purrPhase = 0.0f, amMod = 1.0f;

        // per control chunk
        float f0 = 220.0f, voicing = 0.0f, aspiration = 0.0f, gain = 1.0f, panL = 0.7f, panR = 0.7f;
        float srcLp = 0.0f, srcCoef = 0.5f;
        std::array<float, numCtrl> laS {};   // articulators (log area), moved with realistic speed
        float velumS = 0.0f;
        bool articInit = false;
        float dcX = 0.0f, dcY = 0.0f, tiltLp = 0.0f, tilt = 0.0f, level = 0.0f, mouthOpen = 0.0f, mouthWidth = 0.6f;
        float detuneCents = 0.0f, timingJitter = 0.0f;
    };

    struct Group
    {
        std::array<Voice, voicesPerGroup> voices;
        float lastLeadPitch = -1.0f;
        bool sounding = false;
    };

    struct Artic
    {
        float x = 0.5f, y = 0.5f;
        int place = -1;
        float area = 1.0f, velum = 0.0f, voice = 1.0f, fric = 0.0f, colour = 0.5f, asp = 0.0f, mouthClose = 0.0f, lips = 0.0f;
        int fricPlace = -1;
        bool rhotic = false;
    };

    void startVoice (Voice&, const NoteEvent&, int group);
    void releaseVoice (Voice&, bool phraseEnd);
    void beginSequence (Voice&, const NoteEvent&, bool legato);
    Artic articulate (Voice&, float dt);
    void controlUpdate (Voice&, int group, int index, float dt);
    float tractCmFor (const Voice&, int group) const;
    float lipComp() const;
    float harmonyPitch (float lead, int harmonyIndex) const;
    void renderFx (float* left, float* right, int n);

    Settings settings;
    std::array<Group, numGroups> groups;
    std::array<Expr, numGroups> expr {}, exprSmooth {};
    float bendValue = 0.0f, modWheelValue = 0.0f, pressureValue = 0.0f;

    double sampleRate = 48000.0;
    float stepRate = 96000.0f;
    int maxBlockSize = 512;

    // FX state
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        std::array<float, 2> z1 {}, z2 {};
        float process (float x, int ch) noexcept
        {
            const float y = b0 * x + z1[(size_t) ch];
            z1[(size_t) ch] = b1 * x - a1 * y + z2[(size_t) ch];
            z2[(size_t) ch] = b2 * x - a2 * y;
            return y;
        }
    };
    Biquad lowCut, presence, air;
    float lastLowCut = -1, lastPresence = -100, lastAir = -100;
    float compEnv = 0.0f, compGain = 1.0f;
    std::vector<float> delayL, delayR;
    int delayPos = 0;
    float delaySmooth = 0.0f;
    juce::Reverb reverb;
    int lastReverb = -1;
    float lastReverbSize = -1.0f;
    std::vector<float> wetL, wetR, voiceBuf;
};

} // namespace talkcat

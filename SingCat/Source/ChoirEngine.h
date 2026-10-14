#pragma once
#include "SafetyLimiter.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "VoiceModel.h"
#include "Lyrics.h"

/**
    Sing-Cat engine: a choir of up to 64 physically modelled singers - people and cats.

    Every singer is built from a body type (tract length, register, tension, age), gets its own
    small random traits (tract size, tuning, vibrato, drift, timing) and sings through its own
    glottis + vocal tract. Singers stand on a stage (pan / depth) in a modelled room.
*/
namespace singcat
{
constexpr int maxSingers = 64;
constexpr int maxPathPoints = 48;

enum class Mode { Solo, Choir, Tutti };
enum class Move { Fixed, Wave, Envelope, Steps, Path, Random };
enum class Onset { Auto, Vowel, Hum, N, Meow };

enum class SingerType { Bass, Tenor, Alto, Soprano, Child, ElderMan, ElderWoman, Kitten, Cat, Tomcat };

struct TypeInfo
{
    const char* name;
    float tractCm;
    int centreNote;          // middle of the comfortable register
    float tense, breath, jitter, shimmer;
    float vibRate, vibDepth; // multipliers on the style vibrato
    float nasal;             // velum opening in cm^2
    bool isCat;
    bool female, child, elder;
};

const TypeInfo& typeInfo (SingerType);
juce::StringArray castNames();
SingerType castMember (int cast, int index);

struct StyleInfo
{
    const char* name;
    float tense, breath;
    float vibDepth, vibRate, vibDelay;    // cents, Hz, seconds
    float scoop, scoopTime;               // semitones (negative = from below), seconds
    float rasp, nasal, larynx;            // larynx: epilarynx narrowing -> "singer's formant"
    float drift;
};

const StyleInfo& styleInfo (int style);
juce::StringArray styleNames();
juce::StringArray modeNames();
juce::StringArray moveNames();
juce::StringArray onsetNames();
juce::StringArray roomNames();

/** Everything the engine reads from the parameters, once per block. */
struct Settings
{
    Mode mode = Mode::Choir;
    int singers = 8, unison = 1, cast = 0, style = 0;
    Onset onset = Onset::Auto;
    float ax = 0.55f, ay = 0.95f, bx = 0.95f, by = 0.06f;
    Move move = Move::Fixed;
    float moveRate = 0.5f, spread = 0.3f;
    float attackMs = 80.0f, releaseMs = 400.0f;
    float vibrato = 1.0f, breath = 0.2f, rasp = 0.0f, glideMs = 120.0f, drift = 0.3f, human = 0.3f, purr = 0.0f;
    float neck = 0.0f, jaw = 0.0f, lips = 0.0f, nose = 0.0f, chest = 0.0f;   // -1..1
    int room = 1;
    float roomMix = 0.3f, micWidth = 0.7f, micDist = 0.4f;
    bool mpe = false;
    float bendRange = 2.0f;
    bool highQuality = false;
    float volume = 1.0f;
    uint32_t seed = 1;
    bool words = false;          // sing the lyrics instead of the vowel chart
    float diction = 0.5f;        // 0 soft, lazy consonants .. 1 crisp, strong consonants
};

/** Lock-free view of one singer for the stage display. */
struct SingerView
{
    std::atomic<float> level { 0.0f }, open { 0.0f }, width { 0.6f }, facing { 0.0f };
    std::atomic<int> type { 0 }, note { -1 };
    std::atomic<bool> singing { false };
};

class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void setSettings (const Settings&);
    const Settings& getSettings() const noexcept { return settings; }

    // MIDI
    void noteOn (int channel, int note, float velocity);
    void noteOff (int channel, int note);
    void pitchBend (int channel, float value);      // -1..1
    void pressure (int channel, float value);       // 0..1
    void timbre (int channel, float value);         // 0..1 (CC74)
    void modWheel (float value);
    void sustain (bool down);
    void allNotesOff();

    void render (float* left, float* right, int numSamples);

    /** User path on the vowel chart (written by the UI, read lock-free with a try-lock). */
    void setPath (const std::vector<juce::Point<float>>& points);
    std::vector<juce::Point<float>> getPath() const;

    /** Stage positions: x -1 (left) .. 1 (right), y 0 (front) .. 1 (back). */
    std::array<std::atomic<float>, maxSingers> stageX, stageY;

    /** Microphone pair position on the stage (x -1..1, y -0.5 = in front of the stage .. 1 = back). */
    std::atomic<float> micX { 0.0f }, micY { -0.2f };
    void arrangeStage (int numSingers, int cast);
    void shuffleStage (uint32_t seed, int numSingers);

    std::array<SingerView, maxSingers> views;
    std::atomic<float> uiVowelX { 0.5f }, uiVowelY { 0.5f };   // where the choir is singing (chart)
    std::atomic<float> uiLevel { 0.0f };
    cat::SafetyLimiter limiter;
    int numActiveSingers() const noexcept;

    /** Lyrics: parsed on the message thread, handed to the audio thread with a try-lock. */
    void setLyrics (const juce::String& text);
    std::vector<Syllable> getSyllables() const;
    void restartLyrics() noexcept { restartPending = true; }
    std::atomic<int> uiSyllable { -1 };          // syllable being sung (index into getSyllables())

private:
    struct Singer
    {
        Glottis glottis;
        Tract tract;
        Noise rnd;
        SingerType type = SingerType::Tenor;

        // Individual traits
        float tractScale = 1.0f, detune = 0.0f, vibRateMul = 1.0f, vibDepthMul = 1.0f;
        float onsetDelay = 0.0f, phase = 0.0f, purrRate = 25.0f, wobble = 0.0f;

        // Note state
        bool active = false, gate = false;
        int channel = 1, note = 60, shift = 0;
        float velocity = 0.8f;
        float semis = 60.0f, glideFrom = 60.0f, glideTime = 0.0f, glideT = 1.0f;
        float env = 0.0f, envTarget = 0.0f;
        int delaySamples = 0;
        double noteTime = 0.0, idleTime = 0.0;
        float vibPhase = 0.0f, driftValue = 0.0f, driftTarget = 0.0f, driftTimer = 0.0f;
        float randX = 0.5f, randY = 0.5f;
        int stepIndex = -1, stepAnchor = 0;
        float purrPhase = 0.0f;
        float inhale = 0.0f, inhaleDelay = 0.0f;
        uint64_t age = 0;

        // Per-chunk values
        float f0 = 110.0f, voicing = 0.0f, aspiration = 0.0f, purrDepth = 0.0f;
        float gainL = 0.7f, gainR = 0.7f, send = 0.2f, lpCoef = 1.0f;
        float lpState = 0.0f, dcX = 0.0f, dcY = 0.0f, mouthOpen = 0.0f, mouthWidth = 0.6f;
        // Spaced microphone pair: per-channel arrival delay (samples, smoothed)
        std::array<float, 128> micBuf {};
        int micPos = 0;
        float delayL = 0.0f, delayR = 0.0f, targetDelayL = 0.0f, targetDelayR = 0.0f;
        float facing = 0.0f;     // -1..1: which way the singer turns towards the mics (for the faces)
        float level = 0.0f;

        // Words
        Syllable syl {};
        bool hasSyl = false, inCoda = false;
        std::array<Phone, 10> seq {};
        int seqLen = 0;
        float seqT = 0.0f;
        float consVoice = 1.0f, consVoiceTarget = 1.0f, aspExtra = 0.0f;
        float fricTarget = 0.0f, fric = 0.0f, fricColour = 0.5f, fHp = 0.0f, fLast = 0.0f;
        int fricIndex = -1;
    };

    struct Held { int channel, note; float velocity; bool sustained; uint64_t order; };
    struct Channel { float bend = 0.0f, pressure = 0.0f, timbre = 0.0f; };

    void startSinger (Singer&, int index, int channel, int note, int shift, float velocity, bool legato, float glideMs);
    void releaseSinger (Singer&);
    void prepareTract (Singer&);
    void retuneTutti();
    void syncWithHeld();
    int pickSinger (int note, std::vector<int>& taken);
    void controlUpdate (Singer&, int index, float dt);
    juce::Point<float> movementPoint (const Singer&, int index) const;
    juce::Point<float> pathPoint (float fraction) const;
    void makeTraits();
    void beginSyllable (Singer&, bool legato);
    void pullLyrics();

    /** What the current consonant does to the tract and the source (words mode). */
    struct Artic
    {
        juce::Point<float> p;            // vowel-chart point to use
        int place = -1;                  // area control point to constrict (-1 none)
        float area = 1.0f;               // constriction area (cm^2)
        float velum = 0.0f, voice = 1.0f, fric = 0.0f, colour = 0.5f, asp = 0.0f, mouthClose = 0.0f;
        int fricPlace = -1;
        bool round = false;
    };
    Artic articulate (Singer&, juce::Point<float> vowel);
    float dictionTime() const noexcept { return 1.25f - 0.5f * settings.diction; }
    int registerOf (int index) const;

    Settings settings;
    std::array<Singer, maxSingers> singers;
    std::vector<Held> held;
    std::vector<int> scratch;
    std::array<Channel, 17> channels {};
    Channel global;
    float modWheelValue = 0.0f;
    bool sustainDown = false;
    uint64_t counter = 0;
    uint32_t traitsSeed = 0;
    int traitsCast = -1;

    double sampleRate = 44100.0;
    float stepRate = 44100.0f;
    int stepsPerSample = 1;          // 2 = High quality (tract oversampled)
    double globalTime = 0.0;
    int controlCounter = 0;
    float norm = 1.0f;

    juce::Reverb reverb;
    int lastRoom = -1;
    std::vector<float> preDelay;
    int preDelayPos = 0, preDelayLength = 1;
    std::vector<float> sendBuffer, wetL, wetR;

    mutable juce::SpinLock lyricsLock;
    std::vector<Syllable> lyrics, lyricsAudio;
    bool lyricsChanged = false;
    int lyricPos = -1;
    double lastLyricStep = -10.0;
    std::atomic<bool> restartPending { false };
    Syllable currentSyl {};
    bool currentHold = false, newSyllable = false;

    mutable juce::SpinLock pathLock;
    std::vector<juce::Point<float>> path, pathAudio;
};

} // namespace singcat

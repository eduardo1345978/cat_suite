#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

/**
    Glide-Cat synth engine: a polyphonic synth whose voices behave like MPE strings.

    When a new chord arrives, the voices that are already sounding (or still remembered from
    the last chord) are matched to the new notes by voice leading and GLIDE there along the
    shortest path, so chords morph instead of restarting. Extra notes branch off a nearby
    voice or start fresh; spare voices merge into a neighbour or fade.

    Per voice: 2 layers x (detuned saw stack, PWM pulse, sine, noise) -> 12 dB high-pass ->
    resonant 12 dB low-pass with an initial/attack-level envelope -> amp envelope; ring mod
    between the layers; automatic swell, brightening, drift and delayed vibrato; MPE on top.
*/
namespace glidecat
{
constexpr int maxVoices = 16;
constexpr int historyLength = 256;

enum class Curve { Linear, Exponential, SCurve, Elastic, Stairs };
enum class Path { Direct, ArcUp, ArcDown, Zigzag, Swirl };
enum class StartFrom { Below, Above, Random, Centre, Outside, Alternate };
enum class Stagger { LowHigh, HighLow, OutsideIn, Random, ShortestFirst };
enum class Pilot { Off, OctaveLeaps, ScaleWalk, GlideArp, Bounce, Weave, Breathe, Wander, Mirror, Sequence, RandomLoop };

juce::StringArray curveNames();
juce::StringArray pathNames();
juce::StringArray startNames();
juce::StringArray staggerNames();
juce::StringArray pilotNames();
juce::StringArray scaleNames();
juce::StringArray feetNames();
juce::StringArray modTypeNames();
juce::StringArray modModeNames();
juce::StringArray modSourceNames();
juce::StringArray modDestNames();
juce::StringArray syncNames();
double syncToPpq (int index);   // 0 = free (Hz)

struct LayerSettings
{
    bool on = true;
    int feet = 1;                     // 0 = 16', 1 = 8', 2 = 4', 3 = 2'
    float saw = 0.8f, pulse = 0.3f, pw = 0.42f, pwmRate = 0.5f, pwm = 0.25f, sine = 0.1f, noise = 0.0f;
    int unison = 3;
    float detune = 10.0f, fine = 0.0f;           // cents
    float hpf = 70.0f, hpRes = 0.7f, lpf = 450.0f, lpRes = 1.6f, key = 0.45f;
    float egDepth = 2.6f, egIL = -0.4f, egAL = 0.8f, egAtk = 1.2f, egDec = 2.6f, egRel = 1.4f;   // oct, -1..1, seconds
    float attack = 0.6f, decay = 2.0f, sustain = 0.9f, release = 1.8f;                         // seconds, 0..1
    float brill = 0.1f, level = 0.8f;
};

struct ModSlot { int source = 0, dest = 0; float amount = 0.0f; };

struct Settings
{
    std::array<LayerSettings, 2> layers;
    float ring = 0.0f;
    int voices = 8;
    float volume = 0.8f, panSpread = 0.4f;
    float bendRange = 2.0f;

    // Glide
    bool rateMode = false;            // false = time, true = rate (ms per semitone)
    float glideMs = 600.0f, rateMs = 60.0f;
    Curve curve = Curve::SCurve;
    float glideSpread = 0.2f;
    bool smartLeading = true;         // non-crossing (smart) or classic (closest first)
    bool branch = true;               // extra notes branch off a voice (else fresh)
    bool merge = true;                // spare voices glide into a neighbour before fading
    float staggerMs = 0.0f;
    Stagger staggerOrder = Stagger::LowHigh;
    Path path = Path::Direct;
    float pathDepth = 3.0f;           // semitones
    StartFrom startFrom = StartFrom::Below;
    float startDepth = 0.0f;          // semitones (fresh voices glide in from there)
    float releaseBend = 0.0f;         // semitones over the release
    bool releasedLines = true;        // released voices are remembered and continue
    float lineMemory = 3.0f;          // seconds

    // Autopilot
    Pilot pilot = Pilot::Off;
    float pilotRateHz = 1.0f;
    int pilotSync = 0;
    float pilotRandom = 0.0f, pilotDensity = 0.7f, pilotSpread = 0.2f, pilotSmooth = 0.5f;
    float pilotRange = 12.0f;
    int pilotScale = 0;
    bool pilotReturn = true;
    std::array<int, 8> pilotSteps { 0, 7, 12, 7, 0, -5, 3, 5 };

    // Expression
    float swell = 0.3f, swellTime = 2.0f, brighten = 0.3f, drift = 0.2f, wander = 0.2f;
    float vibDepth = 0.15f, vibRate = 5.2f, vibDelay = 0.8f;   // semitones, Hz, s

    // Modulators + matrix
    std::array<int, 4> modType { 0, 0, 0, 0 }, modMode { 0, 0, 0, 0 };
    std::array<float, 4> modRate { 0.5f, 1.0f, 2.0f, 0.3f };
    std::array<ModSlot, 8> matrix {};
};

/** Lock-free voice view for the Glide View (written by the audio thread). */
struct VoiceView
{
    std::atomic<bool> active { false };
    std::atomic<float> pitch { 60.0f }, target { 60.0f }, level { 0.0f }, width { 0.0f }, pressure { 0.0f };
    std::array<std::atomic<float>, historyLength> history {};      // pitch, NaN-free; < 0 = silent
    std::atomic<int> write { 0 };
};

class Synth
{
public:
    Synth();
    void prepare (double sampleRate, int maxBlock);
    void setSettings (const Settings& s) { settings = s; }
    const Settings& getSettings() const noexcept { return settings; }

    /** Notes that start together (one chord) are handed over as one group. */
    void notesOn (const std::vector<std::pair<int, int>>& channelAndNote, const std::vector<float>& velocities);
    void noteOff (int channel, int note);
    void pitchBend (int channel, float v);      // -1..1
    void pressure (int channel, float v);
    void timbre (int channel, float v);
    void modWheel (float v) { modWheelValue = v; }
    void sustain (bool down);
    void allNotesOff();
    void setHostPosition (double ppq, double bpm, bool running) { hostPpq = ppq; hostBpm = bpm; hostRunning = running; }

    void render (float* left, float* right, int numSamples);

    std::array<VoiceView, maxVoices> views;
    std::atomic<int> uiVoicesInUse { 0 };
    int numSounding() const noexcept;

private:
    struct Layer
    {
        std::array<double, 3> sawPhase {};
        double pulsePhase = 0.0, sinePhase = 0.0, pwmPhase = 0.0;
        float hp1 = 0.0f, hp2 = 0.0f, lp1 = 0.0f, lp2 = 0.0f;
        float amp = 0.0f, filt = 0.0f;
        int ampStage = 0, filtStage = 0;          // 0 off, 1 attack, 2 decay, 3 sustain, 4 release
        float out = 0.0f;
    };

    struct Voice
    {
        bool active = false, gate = false, merging = false;
        int channel = 1, note = -1;
        uint64_t age = 0;
        float velocity = 0.8f;
        // pitch / glide
        float pitch = 60.0f, from = 60.0f, to = 60.0f, glideT = 1.0f, glideDur = 0.0f, delay = 0.0f, pathSign = 1.0f;
        float silentFor = 0.0f, releaseTime = 0.0f;
        // expression
        float noteTime = 0.0f, vibPhase = 0.0f, vibRateMul = 1.0f, driftValue = 0.0f, driftTarget = 0.0f, driftTimer = 0.0f;
        float wanderValue = 0.0f, wanderTarget = 0.0f, pan = 0.0f;
        // autopilot
        float pilotOffset = 0.0f;
        int pilotIndex = 0;
        std::array<int, 8> loop {};
        // modulators
        std::array<float, 4> modPhase {}, modValue {}, modTarget {};
        std::array<Layer, 2> layers;
        juce::Random rnd;
        int historyCounter = 0;
    };

    struct ChannelState { float bend = 0.0f, pressure = 0.0f, timbre = 0.0f; };

    Voice* freeVoice();
    void startGlide (Voice&, float target, float delaySeconds);
    void gateOn (Voice&, int channel, int note, float velocity, bool retrigger);
    void releaseVoice (Voice&);
    float glideDuration (Voice&, float distance);
    float shapeCurve (float u) const;
    float pathOffset (const Voice&, float u) const;
    void pilotStep (double stepIndex);
    float quantiseToScale (float semis, int root) const;
    float modSource (const Voice&, int source, int index) const;
    void renderVoice (Voice&, int index, float* left, float* right, int numSamples);

    Settings settings;
    std::array<Voice, maxVoices> voices;
    std::array<ChannelState, 17> channels {};
    std::vector<std::pair<int, int>> held;    // (channel, note) keys down
    std::vector<std::pair<int, int>> sustained;
    bool sustainDown = false;
    float modWheelValue = 0.0f;
    uint64_t counter = 0;
    double sampleRate = 44100.0;
    double hostPpq = 0.0, hostBpm = 120.0;
    bool hostRunning = false;
    double pilotClock = 0.0;
    int64_t lastPilotStep = -1;
    juce::Random rng;
    std::array<float, 4> monoModPhase {};
};

} // namespace glidecat

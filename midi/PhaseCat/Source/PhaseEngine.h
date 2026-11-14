#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

/**
    Phase-Cat's clock: four rhythm lanes, each its own little world.

      - Polymeter: every lane has its own length (1 - 32 steps): 3 against 4 against 5 against 7 comes
        round together only after a long time.
      - Phasing (Steve Reich): a lane can run a little faster or slower than the others (drift, in %):
        two lanes with the same pattern slide apart step by step and meet again.
      - Risset rhythm: the lane is played in three tempo layers an octave apart whose loudness follows a
        bell curve; the tempo of all of them glides up (or down) and loops - a beat that seems to speed
        up forever but never gets faster.
      - Metric modulation: one click and, at the next bar, the lanes' beat is multiplied by 3/2, 4/3 ...
        (triplets become the new eighths) - Cubase's tempo stays the same.

    Real-time safe: fixed tables only.
*/
namespace phasecat
{
constexpr int numLanes = 4;
constexpr int maxSteps = 32;
constexpr int numLayers = 3;              // Risset: three tempo octaves

juce::StringArray rateNames();
double rateBeats (int index);             // a step's length in quarter notes
constexpr int numRates = 8;

struct Lane
{
    bool on = true;
    int length = 16;
    int note = 36;
    int rate = 3;                         // 1/16
    float drift = 0.0f;                   // % faster (+) / slower (-)
    bool risset = false;
    std::array<float, maxSteps> steps {}; // velocity 0..1 (0 = rest)
};

struct Settings
{
    std::array<Lane, numLanes> lanes;
    int channel = 10;
    float gate = 0.5f;                    // of a step
    float rissetBars = 4.0f;              // one turn of the endless speed-up
    bool rissetDown = false;              // ... or slow-down
};

/** Metric modulation choices (the factor the beat is multiplied by). */
juce::StringArray modulationNames();
double modulationFactor (int index);
constexpr int numModulations = 6;

class Engine
{
public:
    Engine() { reset(); }
    void reset() noexcept;

    /** ppq = where the block starts (Cubase), playing = the transport runs. */
    void process (const Settings&, double ppq, double bpm, bool playing, double barBeats,
                  int numSamples, double sampleRate, juce::MidiBuffer& out);

    // ---- from the screen (any thread)
    void requestModulation (int index) noexcept { pendingMod.store (index); }
    void requestResetModulation() noexcept { pendingMod.store (-2); }
    void requestRealign() noexcept { pendingRealign.store (true); }

    // ---- for the screen (any thread)
    std::array<std::atomic<float>, numLanes> position {};        // the step now (fractional), per lane
    std::array<std::atomic<float>, numLanes> flash {};           // a hit (decays on the screen)
    std::array<std::array<std::atomic<float>, numLayers>, numLanes> layerLevel {};  // Risset loudness per layer
    std::atomic<double> ratio { 1.0 };                           // metric modulation now
    std::atomic<float> rissetPhase { 0.0f };
    std::atomic<int> hits { 0 };

private:
    struct Pending { int note = -1; int channel = 1; int samplesLeft = 0; };
    void noteOn (juce::MidiBuffer& out, int pos, int channel, int note, float velocity, int gateSamples) noexcept;
    void flushOffs (juce::MidiBuffer& out, int numSamples) noexcept;
    void realign (const Settings&, double ppq) noexcept;

    std::array<std::array<double, numLayers>, numLanes> phase {};  // in steps
    double rissetX = 0.0;                                         // 0..1 through one turn
    double lastEndPpq = -1.0;
    bool wasPlaying = false;
    double curRatio = 1.0;
    int pendingAtBar = -1;                                        // a modulation waiting for the bar line
    std::atomic<int> pendingMod { -1 };
    std::atomic<bool> pendingRealign { false };
    bool realignAtBar = false;
    std::array<Pending, 64> offs {};
};
} // namespace phasecat

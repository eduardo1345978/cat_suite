#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

/**
    Gate-Cat's engine: a step sequencer that turns the volume (and a low-pass filter) up and down.

    Each of the 16 steps has a LEVEL (0 = silent, 1 = full). The gate follows the steps in time with the DAW:
    a step opens the sound for LENGTH of its time, with an ATTACK (how fast it opens) and a RELEASE (how fast it
    closes). SWING pushes every second step later. DEPTH says how deep the holes are (0 = no effect at all),
    FILTER lets the steps also open a low-pass (a quiet step is also a darker one), MIX blends with the dry sound.
*/
namespace gate
{
constexpr int maxSteps = 16;

juce::StringArray rateNames();          // "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T"
double stepsPerBeat (int rate) noexcept;

struct Settings
{
    std::array<float, maxSteps> level {};
    int steps = 16;
    int rate = 2;
    float swing = 0.0f;       // 0 .. 1 (1 = the off-steps start 2/3 of the way)
    float length = 0.6f;      // 0.05 .. 1: how much of a step stays open
    float attack = 0.1f;      // 0 .. 1 -> 0.5 .. 60 ms
    float release = 0.2f;     // 0 .. 1 -> 2 .. 250 ms
    float depth = 1.0f;
    float filter = 0.0f;
    float mix = 1.0f;
    float outDb = 0.0f;
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;     // with the DAW: follow its position; else an own clock
};

class Engine
{
public:
    void prepare (double sampleRate, int blockSize);
    void reset();
    void process (juce::AudioBuffer<float>&, const Settings&);

    /** where the gate is now (for the screen) */
    std::atomic<int> stepNow { 0 };
    std::atomic<float> gainNow { 1.0f };

    /** the step index and how far into it (0..1) at a position in beats, with swing */
    static std::pair<int, double> stepAt (double beats, const Settings&) noexcept;

private:
    double sr = 48000.0, ownBeats = 0.0;
    bool wasPlaying = false;
    float env = 1.0f;
    float ic1[2] {}, ic2[2] {};
    juce::SmoothedValue<float> mixS, outS;
};
} // namespace gate

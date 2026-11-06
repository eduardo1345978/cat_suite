#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

/**
    Molten: the sound melts.

        in -> BODY (low shelf) -> WOBBLE (tape wow / flutter, and the pitch sags when it gets loud)
           -> HEAT (2x oversampled valve-like saturation: asymmetric, warmer and softer on top the hotter it gets)
           -> DRIP (echoes that fall: every repeat is lower and darker, a pitch shifter inside the feedback loop)
           -> bass kept in mono -> auto gain (turning HEAT doesn't make it louder) -> MIX -> out

    The latency (oversampling filters + the wobble's base delay) is reported to the host; the dry signal for MIX
    is delayed by the same amount, so MIX never comb-filters.
*/
namespace molten
{
struct Settings
{
    float heat = 0.5f;          // 0..1
    float drip = 0.35f;         // 0..1
    float wobble = 0.3f;        // 0..1
    float bodyDb = 2.0f;        // -6..+9
    float mix = 1.0f;           // 0..1
    double dripBeats = 0.75;    // echo time in beats (1/8 dotted)
    double bpm = 120.0;
    bool monoBass = true;
    float outDb = 0.0f;
};

juce::StringArray dripTimeNames();
double dripTimeBeats (int index);

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    int latencySamples() const noexcept { return latency; }
    void process (juce::AudioBuffer<float>&, const Settings&);

    // for the screen
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f }, dripLevel { 0.0f }, sag { 0.0f }, autoGainDb { 0.0f };
    std::atomic<int> drips { 0 };            // counts the falling echoes (a new drop each time)

private:
    struct Shelf   // RBJ low shelf
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1[2] {}, z2[2] {};
        void set (double sr, double f, double gainDb);
        inline float run (float x, int ch) noexcept
        {
            const float y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return y;
        }
    };

    double sr = 48000.0;
    int latency = 0, osLatency = 0, baseDelay = 0;
    Settings s;
    Shelf body;
    float lastBodyDb = -100.0f;

    // wobble
    std::vector<float> wobL, wobR;
    int wobMask = 0, wobPos = 0;
    double wowPh = 0.0, flutterPh = 0.0, driftPh = 0.0;
    float drift = 0.0f, driftTarget = 0.0f, env = 0.0f, sagSmooth = 0.0f;

    // heat
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    float heatSmooth = 0.5f, lpL = 0.0f, lpR = 0.0f;

    // drip
    std::vector<float> dripBufL, dripBufR;
    int dripMask = 0, dripPos = 0;
    double shiftPh = 0.0;
    float dripLpL = 0.0f, dripLpR = 0.0f, dripEnv = 0.0f;
    bool dripWasLoud = false;

    // dry path (delayed by the latency for MIX)
    std::vector<float> dryL, dryR;
    int dryMask = 0, dryPos = 0;

    // bass in mono
    // the side signal's high-pass (2 x 2nd-order Butterworth at 120 Hz = a 24 dB/oct Linkwitz-Riley cut)
    struct SideHp { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; float z1[2] {}, z2[2] {}; } sideHp;
    float sideLp1 = 0.0f, sideLp2 = 0.0f;

    // auto gain
    double inPow = 1.0e-6, outPow = 1.0e-6;
    float gain = 1.0f;

    juce::AudioBuffer<float> wet;
    juce::Random random { 0x5ea7 };
};
} // namespace molten

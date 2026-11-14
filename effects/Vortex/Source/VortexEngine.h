#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

/**
    Vortex: the sound spins up (or down) forever - a Shepard / Risset spiral.

    POSITION moves through octaves (SPIN octaves a second, or one octave every N bars with Cubase). Everything
    repeats every octave, so it never arrives anywhere:
      PITCH   4 pitch-shifted copies of the sound, an octave apart, all gliding up; each fades in at the bottom
              and out at the top (a window over 4 octaves), so you only ever hear "higher, higher, higher".
      FILTER  10 band-pass filters an octave apart sweep up through the sound the same way (a barber-pole filter).
      BOTH    pitch, then filter.
    FOCUS narrows the window (fewer copies, clearer), FEEDBACK sends the result back in (cascades), WIDTH spreads
    it, TONE tilts it, GEN adds a Shepard tone of its own (spins even with nothing coming in), MIX, OUTPUT.
*/
namespace vortex
{
enum class Mode { pitch = 0, filter = 1, both = 2 };

juce::StringArray modeNames();
juce::StringArray syncNames();          // "Free", "1 bar", ... "16 bars" (per octave)
double syncBars (int index) noexcept;  // 0 = free
/** The free speed: signed octaves a second (0 in the middle of the knob). */
double spinRate (float spin) noexcept;

constexpr int pitchLayers = 4;          // an octave apart
constexpr double pitchSpan = 4.0;       // octaves the window covers
constexpr double pitchLow = -2.5;       // lowest copy: 2.5 octaves down .. highest 1.5 up
constexpr int filterBands = 10;
constexpr double filterLowHz = 20.0;
constexpr int genPartials = 9;
constexpr double genLowHz = 27.5;

/** 0 at both ends of the window, 1 in the middle; FOCUS makes it narrower. t = 0..1 across the span. */
float window (double t, float power) noexcept;
float pitchPower (float focus) noexcept;
float filterPower (float focus) noexcept;
/** Where pitch copy i sits (octaves above / below the sound) at a position. */
double layerOctave (int i, double position) noexcept;
/** Where filter band j sits (Hz) at a position. */
double bandHz (int j, double position) noexcept;

struct Settings
{
    float spin = 0.35f;                 // -1 .. 1
    int sync = 0;                       // syncNames()
    Mode mode = Mode::pitch;
    float focus = 0.5f, feedback = 0.0f, width = 0.5f, tone = 0.0f, gen = 0.0f, mix = 1.0f, outDb = 0.0f;
    double bpm = 120.0, ppq = 0.0, barBeats = 4.0;
    bool playing = false;
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>&, const Settings&);

    double getPosition() const noexcept { return pos; }
    void setPosition (double p) noexcept { pos = p; }

    // for the screen
    std::atomic<float> screenPos { 0.0f }, inLevel { 0.0f }, outLevel { 0.0f }, rateNow { 0.0f };
    std::atomic<float> autoGainDb { 0.0f };

private:
    float readRing (int ch, double delay) const noexcept;
    void updateFilters (int ch, double position, float k) noexcept;

    double sr = 48000.0;
    double pos = 0.0, rate = 0.0;
    bool first = true;

    // pitch: one ring of the input per channel, two taps per copy
    std::array<std::vector<float>, 2> ring;
    int mask = 0, writePos = 0;
    double winSamples = 2400.0, minDelay = 8.0;
    std::array<double, pitchLayers> phase {};

    // filters (TPT state variable)
    struct Svf { float ic1 = 0.0f, ic2 = 0.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, w = 0.0f; };
    std::array<std::array<Svf, filterBands>, 2> svf {};
    int filterCountdown = 0;

    // feedback (a short echo that goes round again)
    std::array<std::vector<float>, 2> fbRing;
    int fbMask = 0, fbPos = 0;
    std::array<float, 2> fbHp {};

    // generator
    std::array<double, genPartials> genPhase {};

    // tone, auto gain
    std::array<float, 2> lp {}, hpState {};
    double inPow = 0.0, wetPow = 0.0;
    float wetGain = 1.0f;

    juce::SmoothedValue<float> mixS, outS, genS, fbS, widthS, focusS;
};
} // namespace vortex

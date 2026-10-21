#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

/**
    Echo-Cat's sound: a tape echo (tempo-synced or free), ping-pong between the speakers, a darker /
    thinner / wobblier / warmer sound each time round, ducking while you sing or play, freeze, and a
    shimmer space (a reverb whose tail is fed back an octave higher).

        in -> [echo line L/R: time, feedback, ping-pong, tone, low cut, wobble, drive]
           -> [duck] -> mix
        in + echoes -> [space: reverb + shimmer (octave up, fed back)] -> mix
*/
namespace echocat
{
/** Note lengths for the synced echo time (in beats). */
juce::StringArray divisionNames();
double divisionBeats (int index);
constexpr int numDivisions = 11;

struct Settings
{
    bool sync = true;
    int division = 5;              // 1/8 dotted
    float timeMs = 350.0f;         // when not synced
    double bpm = 120.0;
    float feedback = 0.4f;         // 0 .. 1 (shown as %, kept below runaway)
    float mix = 0.35f, dry = 1.0f;
    float pingpong = 0.0f, tone = 0.7f, lowcut = 0.15f, wobble = 0.0f, drive = 0.0f;
    float space = 0.0f, shimmer = 0.0f, duck = 0.0f;
    bool freeze = false;
    float outputDb = 0.0f;

    double delaySeconds() const;
};

/** A little granular pitch shifter: an octave up (two overlapping grains read at double speed). */
class OctaveUp
{
public:
    void prepare (double sampleRate);
    void reset();
    float process (float x) noexcept;
private:
    std::vector<float> buf;
    int w = 0, grain = 2048;
    float phase = 0.0f;
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const Settings&);

    // for the screen (any thread)
    std::atomic<float> inLevel { 0.0f }, echoLevel { 0.0f }, spaceLevel { 0.0f }, duckAmount { 0.0f };
    std::atomic<double> shownDelay { 0.3 };

private:
    double sr = 48000.0;
    std::vector<float> lineL, lineR;
    int w = 0;
    float delaySmooth = -1.0f;
    float lpL = 0, lpR = 0, hpL = 0, hpR = 0;
    double wobPhase1 = 0.0, wobPhase2 = 0.0;
    float env = 0.0f, freezeMix = 0.0f;
    juce::Reverb reverb;
    std::array<OctaveUp, 2> shifters;
    std::vector<float> revL, revR, shimL, shimR;
    float shimLp = 0.0f, shimLpR = 0.0f;
    float outGain = 1.0f;
};
} // namespace echocat

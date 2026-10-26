#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Project.h"
#include "Dsp.h"
#include "SafetyLimiter.h"
#include <vector>

/**
    After the tracks: two send effects (A = reverb, B = tempo delay) and the master bus
    (the DJ filter on the slider, glue compressor, tape, width, volume, safety limiter).
*/
namespace catbox
{
class SendReverb
{
public:
    void prepare (double sampleRate);
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Master&);
private:
    juce::Reverb reverb;
    dsp::Svf lowCutL, lowCutR;
    std::vector<float> l, r;
    double sr = 48000.0;
};

class SendDelay
{
public:
    void prepare (double sampleRate);
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Master&);
private:
    std::vector<float> bufL, bufR;
    int w = 0;
    float timeSmooth = 0.0f, lpL = 0.0f, lpR = 0.0f, hpL = 0.0f, hpR = 0.0f;
    double sr = 48000.0;
};

class MasterBus
{
public:
    void prepare (double sampleRate);
    /** In place. */
    void process (float* l, float* r, int n, const Master&);
    float gainReduction() const noexcept { return 1.0f - glueGain; }
private:
    dsp::Svf lpL, lpR, hpL, hpR;
    float cut = 0.5f, glueEnv = 0.0f, glueGain = 1.0f, vol = 0.8f;
    cat::SafetyLimiter limiter;
    double sr = 48000.0;
};
} // namespace catbox

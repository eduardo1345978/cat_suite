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
/** Schroeder all-pass (diffusion). */
struct Diffuser
{
    std::vector<float> buf;
    int i = 0;
    void prepare (int size) { buf.assign ((size_t) juce::jmax (1, size), 0.0f); i = 0; }
    inline float process (float x, float g) noexcept
    {
        const float d = buf[(size_t) i];
        const float v = x + d * g;
        buf[(size_t) i] = v;
        if (++i >= (int) buf.size()) i = 0;
        return d - v * g;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
};

/** An octave up (two crossfading heads), for the shimmer. */
struct OctaveUp
{
    std::vector<float> buf;
    int w = 0;
    float phase = 0.0f;
    void prepare (int size) { buf.assign ((size_t) size, 0.0f); w = 0; phase = 0.0f; }
    float process (float x) noexcept;
};

/** Send A: a reverb with types (hall, room, plate, shimmer, cloud, spring, dark), pre-delay and ducking. */
class SendReverb
{
public:
    void prepare (double sampleRate);
    /** duck: how loud the dry mix is now (0..1), it pushes the tail down while you play. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Master&, float duck);
private:
    juce::Reverb reverb;
    juce::Reverb::Parameters last;
    dsp::Svf lowCutL, lowCutR, toneL, toneR;
    std::vector<float> l, r, preL, preR;
    int preW = 0;
    std::array<Diffuser, 8> diff;
    std::array<float, 24> dispL {}, dispR {};
    OctaveUp shimL, shimR;
    float shimFbL = 0.0f, shimFbR = 0.0f, shimLpL = 0.0f, shimLpR = 0.0f, shimDcL = 0.0f, shimDcR = 0.0f, modPhase = 0.0f, gain = 0.0f;
    double sr = 48000.0;
};

/** Send B: a tempo delay with types (clean, tape echo, dub, ping-pong, multi-tap, diffused, reverse). */
class SendDelay
{
public:
    void prepare (double sampleRate);
    /** Adds the repeats to out, and delayToRev of them to rev (the reverb's input). */
    void process (const float* inL, const float* inR, float* outL, float* outR, float* revL, float* revR, int n, const Master&, float duck);
private:
    float tap (const std::vector<float>& buf, float back) const noexcept;
    std::vector<float> bufL, bufR;
    int w = 0;
    float timeSmooth = 0.0f, lpL = 0.0f, lpR = 0.0f, hpL = 0.0f, hpR = 0.0f, wowPhase = 0.0f, flutPhase = 0.0f, revPhase = 0.0f, gain = 0.0f;
    std::array<Diffuser, 6> diff;
    dsp::Rng rng;
    double sr = 48000.0;
};

/** Send C: any of the insert effects as a send (chorus, phaser, flanger, delay ...). */
class SendFx
{
public:
    void prepare (double sampleRate) { fx.prepare (sampleRate); tl.assign (4096, 0.0f); tr.assign (4096, 0.0f); }
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Master&, double ppq, double ppqPerSample, float duck);
private:
    InsertFx fx;
    std::vector<float> tl, tr;
    float gain = 0.0f;
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

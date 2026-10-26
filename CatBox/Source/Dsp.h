#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdint>
#include <algorithm>

/**
    Small building blocks shared by the Cat Box engines: band-limited oscillators (PolyBLEP),
    zero-delay-feedback filters, envelopes, LFOs and saturation. Everything here is real-time
    safe (no allocation, no locks).
*/
namespace catbox::dsp
{
constexpr float pi = 3.14159265358979f, twoPi = 6.28318530717959f;

inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
inline float softClip (float x) noexcept { return std::tanh (x); }
/** Cheaper tanh-like curve (exact enough for drive stages). */
inline float fastTanh (float x) noexcept
{
    x = juce::jlimit (-3.0f, 3.0f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
inline float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }

struct Rng
{
    uint32_t s = 0x9e3779b9u;
    inline float next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
    inline float uni() noexcept { return 0.5f + 0.5f * next(); }
};

/** PolyBLEP residual for a discontinuity at phase 0 (t in cycles, dt = increment per sample). */
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

enum Wave { WaveSaw = 0, WaveSquare, WaveTriangle, WaveSine, WaveNoise, NumWaves };

/** One band-limited oscillator. Triangle is an integrated square (leaky). */
struct Osc
{
    float phase = 0.0f, tri = 0.0f;
    inline float process (int wave, float dt, float pw, Rng& rng) noexcept
    {
        float y = 0.0f;
        switch (wave)
        {
            case WaveSaw:    y = 2.0f * phase - 1.0f - polyBlep (phase, dt); break;
            case WaveSquare:
            case WaveTriangle:
            {
                float sq = phase < pw ? 1.0f : -1.0f;
                sq += polyBlep (phase, dt);
                float t2 = phase - pw; if (t2 < 0.0f) t2 += 1.0f;
                sq -= polyBlep (t2, dt);
                if (wave == WaveSquare) { y = sq; break; }
                tri = tri * 0.9995f + sq * dt * 4.0f;
                y = tri;
                break;
            }
            case WaveSine:   y = std::sin (twoPi * phase); break;
            default:         y = rng.next(); break;
        }
        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        return y;
    }
    /** Hard sync: restart (called when the master wraps). */
    void reset (float p = 0.0f) noexcept { phase = p; }
};

/** Zero-delay-feedback state variable filter (Simper / Cytomic). Low, band, high and notch outputs. */
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f, g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    void set (float cutoffHz, float q, float sampleRate) noexcept
    {
        cutoffHz = juce::jlimit (16.0f, sampleRate * 0.45f, cutoffHz);
        g = std::tan (pi * cutoffHz / sampleRate);
        k = 1.0f / juce::jmax (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    /** Like set(), but the cutoff moves there over n samples (no steps when an envelope sweeps it). */
    float gInc = 0.0f;
    int gLeft = 0;
    void rampTo (float cutoffHz, float q, float sampleRate, int n) noexcept
    {
        cutoffHz = juce::jlimit (16.0f, sampleRate * 0.45f, cutoffHz);
        const float target = std::tan (pi * cutoffHz / sampleRate);
        k = 1.0f / juce::jmax (0.05f, q);
        if (g <= 0.0f || n <= 1) { g = target; gLeft = 0; }
        else { gInc = (target - g) / (float) n; gLeft = n; }
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    struct Out { float low, band, high; };
    inline Out process (float v0) noexcept
    {
        if (gLeft > 0) { g += gInc; --gLeft; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2; }
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        if (! std::isfinite (ic1) || ! std::isfinite (ic2)) ic1 = ic2 = 0.0f;
        return { v2, v1, v0 - k * v1 - v2 };
    }
    void reset() noexcept { ic1 = ic2 = 0.0f; }
};

/** A 4-pole transistor-ladder-style low-pass (ZDF stages, tanh in the feedback), self-oscillating near res = 1. */
struct Ladder
{
    float s[4] {}, g = 0.0f, res = 0.0f, drive = 1.0f, last = 0.0f;
    void set (float cutoffHz, float resonance, float sampleRate) noexcept
    {
        cutoffHz = juce::jlimit (16.0f, sampleRate * 0.42f, cutoffHz);
        const float wc = std::tan (pi * cutoffHz / sampleRate);
        g = wc / (1.0f + wc);
        res = juce::jlimit (0.0f, 1.05f, resonance) * 4.0f;
    }
    float gInc = 0.0f;
    int gLeft = 0;
    /** Like set(), but the cutoff moves there over n samples. */
    void rampTo (float cutoffHz, float resonance, float sampleRate, int n) noexcept
    {
        cutoffHz = juce::jlimit (16.0f, sampleRate * 0.42f, cutoffHz);
        const float wc = std::tan (pi * cutoffHz / sampleRate);
        const float target = wc / (1.0f + wc);
        res = juce::jlimit (0.0f, 1.05f, resonance) * 4.0f;
        if (g <= 0.0f || n <= 1) { g = target; gLeft = 0; }
        else { gInc = (target - g) / (float) n; gLeft = n; }
    }
    inline float process (float x) noexcept
    {
        if (gLeft > 0) { g += gInc; --gLeft; }
        // the last output feeds back (one sample late, smoothed by the tanh): resonance up to self-oscillation
        const float g4 = g * g * g * g;
        const float sigma = last;
        float u = fastTanh ((x * drive - res * sigma) / (1.0f + res * g4));
        for (int i = 0; i < 4; ++i)
        {
            const float v = (u - s[i]) * g;
            const float y = v + s[i];
            s[i] = y + v;
            u = y;
        }
        if (! std::isfinite (u)) { for (auto& z : s) z = 0.0f; u = 0.0f; }
        last = u;
        return u;
    }
    void reset() noexcept { for (auto& z : s) z = 0.0f; last = 0.0f; }
};

/** ADSR with exponential curves (times in seconds). */
struct Adsr
{
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    float value = 0.0f;
    float attackRate = 0.0f, decayCoef = 0.0f, releaseCoef = 0.0f, sustain = 1.0f;
    void set (float a, float d, float s, float r, float sampleRate) noexcept
    {
        attackRate = 1.0f / juce::jmax (1.0f, a * sampleRate);
        decayCoef = std::exp (-4.6f / juce::jmax (1.0f, d * sampleRate));
        releaseCoef = std::exp (-4.6f / juce::jmax (1.0f, r * sampleRate));
        sustain = juce::jlimit (0.0f, 1.0f, s);
    }
    void gateOn (bool retrigger = true) noexcept { if (retrigger || stage == Idle || stage == Release) stage = Attack; }
    void gateOff() noexcept { if (stage != Idle) stage = Release; }
    void kill() noexcept { stage = Idle; value = 0.0f; }
    bool active() const noexcept { return stage != Idle; }
    inline float process() noexcept
    {
        switch (stage)
        {
            case Attack:  value += attackRate * (1.05f - value) * 1.3f + attackRate * 0.2f; if (value >= 1.0f) { value = 1.0f; stage = Decay; } break;
            case Decay:   value = sustain + (value - sustain) * decayCoef; if (value - sustain < 1.0e-4f) { value = sustain; stage = Sustain; } break;
            case Sustain: value = sustain; if (sustain <= 0.0001f) stage = Idle; break;
            case Release: value *= releaseCoef; if (value < 1.0e-4f) { value = 0.0f; stage = Idle; } break;
            case Idle:    value = 0.0f; break;
        }
        return value;
    }
};

enum LfoShape { LfoSine = 0, LfoTriangle, LfoSaw, LfoSquare, LfoRandom, NumLfoShapes };

struct Lfo
{
    float phase = 0.0f, held = 0.0f;
    Rng rng;
    /** Advances by dt cycles; returns -1..1. */
    inline float process (int shape, float dt) noexcept
    {
        float y = 0.0f;
        switch (shape)
        {
            case LfoSine:     y = std::sin (twoPi * phase); break;
            case LfoTriangle: y = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
            case LfoSaw:      y = 1.0f - 2.0f * phase; break;
            case LfoSquare:   y = phase < 0.5f ? 1.0f : -1.0f; break;
            default:          y = held; break;
        }
        phase += dt;
        if (phase >= 1.0f) { phase -= 1.0f; held = rng.next(); }
        return y;
    }
};

/** One-pole smoother for parameters that jump (cutoff, volume ...). */
struct Smooth
{
    float y = 0.0f, a = 0.999f;
    void setTime (float seconds, float sampleRate) noexcept { a = std::exp (-1.0f / juce::jmax (1.0f, seconds * sampleRate)); }
    inline float process (float target) noexcept { y = target + (y - target) * a; return y; }
};

/** Stereo chorus (two modulated delay taps). */
struct Chorus
{
    static constexpr int size = 4096;
    float bufL[size] {}, bufR[size] {};
    int w = 0;
    float ph = 0.0f;
    void reset() noexcept { std::fill (bufL, bufL + size, 0.0f); std::fill (bufR, bufR + size, 0.0f); w = 0; }
    inline void process (float& l, float& r, float mix, float rateHz, float depth, float sampleRate) noexcept
    {
        bufL[w] = l; bufR[w] = r;
        ph += rateHz / sampleRate; if (ph >= 1.0f) ph -= 1.0f;
        const float base = 0.007f * sampleRate, dep = 0.004f * sampleRate * depth;
        auto tap = [this] (const float* b, float d)
        {
            float pos = (float) w - d; while (pos < 0.0f) pos += (float) size;
            const int i0 = (int) pos; const float f = pos - (float) i0;
            return b[i0 % size] * (1.0f - f) + b[(i0 + 1) % size] * f;
        };
        const float dl = base + dep * std::sin (twoPi * ph), dr = base + dep * std::sin (twoPi * (ph + 0.25f));
        const float wl = tap (bufL, dl), wr = tap (bufR, dr);
        w = (w + 1) % size;
        l = l * (1.0f - 0.5f * mix) + wl * mix;
        r = r * (1.0f - 0.5f * mix) + wr * mix;
    }
};
} // namespace catbox::dsp

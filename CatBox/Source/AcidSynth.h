#pragma once

#include <cmath>
#include <algorithm>

namespace catbox
{
/**
    Cat Box's own bass voice, in the spirit of the TB-303: one oscillator (saw or square), a
    resonant 4-pole low-pass that sweeps down after every new note, accents that hit harder,
    brighter and shorter, and slides that glide into the next note without restarting it.
*/
class AcidSynth
{
public:
    struct Params
    {
        float cutoff = 0.35f;    // 0..1 (about 40 Hz .. 8 kHz)
        float resonance = 0.6f;  // 0..1 (near 1 it squeals)
        float envMod = 0.55f;    // how far the filter opens on a note
        float decay = 0.4f;      // filter sweep length
        float accent = 0.6f;     // how much harder accented notes hit
        float slide = 0.35f;     // glide time
        int wave = 0;            // 0 saw, 1 square
    };

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate > 0.0 ? (float) sampleRate : 48000.0f;
        allOff();
        amp = 0.0f;
        y1 = y2 = y3 = y4 = ox = oy1 = oy2 = oy3 = 0.0f;
    }

    void setParams (const Params& p) noexcept { params = p; }

    void noteOn (int note, bool accent, bool slide) noexcept
    {
        const float f = 440.0f * std::pow (2.0f, (float) (note - 69) / 12.0f);
        const bool legato = slide && gate;
        current = note;
        target = f;
        gate = true;
        if (! legato)
        {
            freq = f;
            fenv = 1.0f;
            accentOn = accent;
            aenv = accent ? 1.0f : 0.0f;
        }
    }

    void noteOff (int note) noexcept
    {
        if (note == current) gate = false;
    }

    void allOff() noexcept { gate = false; current = -1; }

    /** Adds the voice to l / r. */
    void render (float* l, float* r, int n) noexcept
    {
        const float dt = 1.0f / sr;
        const float glideTime = 0.015f + params.slide * 0.18f;
        const float glide = 1.0f - std::exp (-dt / glideTime);
        const float decayTime = accentOn ? 0.18f : 0.06f + params.decay * params.decay * 1.4f;
        const float fdec = std::exp (-dt / decayTime);
        const float adec = std::exp (-dt / 0.2f);
        const float att = 1.0f - std::exp (-dt / 0.002f);
        const float rel = std::exp (-dt / 0.012f);
        const float res = std::min (0.97f, params.resonance * 0.97f);
        const float baseOct = params.cutoff * 7.6f;                    // 40 Hz * 2^0..7.6
        const float accentAmt = accentOn ? params.accent : 0.0f;

        for (int i = 0; i < n; ++i)
        {
            freq += (target - freq) * glide;
            const float inc = std::min (0.45f, freq * dt);
            ph += inc;
            if (ph >= 1.0f) ph -= 1.0f;

            float osc = blepSaw (ph, inc);
            if (params.wave == 1)
            {
                float ph2 = ph + 0.5f;
                if (ph2 >= 1.0f) ph2 -= 1.0f;
                osc = 0.5f * (osc - blepSaw (ph2, inc));
            }

            // envelopes
            fenv *= fdec;
            aenv *= adec;
            amp = gate ? amp + (1.0f - amp) * att : amp * rel;

            // filter cutoff: base + envelope sweep + accent kick
            const float oct = baseOct + params.envMod * 4.2f * fenv + accentAmt * 2.4f * aenv;
            const float fc = std::min (0.42f * sr, 40.0f * std::exp2 (oct));
            const float y = ladder (osc * 0.55f, fc / (0.5f * sr), res);

            float out = y * amp * (1.0f + accentAmt * 0.9f * aenv);
            out = std::tanh (out * 1.6f) * 0.55f;
            l[i] += out;
            r[i] += out;
        }
    }

private:
    static float blepSaw (float ph, float inc) noexcept
    {
        float v = 2.0f * ph - 1.0f;
        if (ph < inc)             { const float t = ph / inc;          v -= t + t - t * t - 1.0f; }
        else if (ph > 1.0f - inc) { const float t = (ph - 1.0f) / inc; v -= t * t + t + t + 1.0f; }
        return v;
    }

    /** Moog-style 4-pole low-pass (musicdsp "Moog VCF, variation 2"), soft-clipped. f = fc / nyquist. */
    float ladder (float in, float f, float resonance) noexcept
    {
        f = std::clamp (f, 0.0005f, 0.99f);
        const float p = f * (1.8f - 0.8f * f);
        const float k = p + p - 1.0f;
        const float t = (1.0f - p) * 1.386249f;
        const float t2 = 12.0f + t * t;
        const float rr = resonance * (t2 + 6.0f * t) / (t2 - 6.0f * t);
        const float x = in - rr * y4;
        y1 = x * p + ox * p - k * y1;
        y2 = y1 * p + oy1 * p - k * y2;
        y3 = y2 * p + oy2 * p - k * y3;
        y4 = y3 * p + oy3 * p - k * y4;
        y4 = std::clamp (y4 - (y4 * y4 * y4) / 6.0f, -1.5f, 1.5f);
        ox = x; oy1 = y1; oy2 = y2; oy3 = y3;
        if (! std::isfinite (y4)) y1 = y2 = y3 = y4 = ox = oy1 = oy2 = oy3 = 0.0f;
        return y4;
    }

    Params params;
    float sr = 48000.0f;
    float freq = 55.0f, target = 55.0f, ph = 0.0f;
    float fenv = 0.0f, aenv = 0.0f, amp = 0.0f;
    bool gate = false, accentOn = false;
    int current = -1;
    float y1 = 0, y2 = 0, y3 = 0, y4 = 0, ox = 0, oy1 = 0, oy2 = 0, oy3 = 0;
};
} // namespace catbox

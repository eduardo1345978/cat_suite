#include "MixFx.h"
#include <cmath>

namespace catbox
{
//==============================================================================
void SendReverb::prepare (double sampleRate)
{
    sr = sampleRate;
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    l.assign (4096, 0.0f);
    r.assign (4096, 0.0f);
    lowCutL.reset(); lowCutR.reset();
}

void SendReverb::process (const float* inL, const float* inR, float* outL, float* outR, int n, const Master& m)
{
    juce::Reverb::Parameters p;
    p.roomSize = 0.3f + 0.68f * m.revSize;
    p.damping = m.revDamp;
    p.width = m.revWidth;
    p.wetLevel = 1.0f;
    p.dryLevel = 0.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);
    const float lowCut = 40.0f * std::exp2 (m.revLowCut * 4.0f);   // 40 .. 640 Hz: a muddy-free tail
    lowCutL.set (lowCut, 0.7f, (float) sr);
    lowCutR.set (lowCut, 0.7f, (float) sr);
    for (int s0 = 0; s0 < n; s0 += 4096)
    {
        const int k = juce::jmin (4096, n - s0);
        for (int i = 0; i < k; ++i)
        {
            l[(size_t) i] = lowCutL.process (inL[s0 + i]).high;
            r[(size_t) i] = lowCutR.process (inR[s0 + i]).high;
        }
        reverb.processStereo (l.data(), r.data(), k);
        for (int i = 0; i < k; ++i) { outL[s0 + i] += l[(size_t) i] * 0.6f; outR[s0 + i] += r[(size_t) i] * 0.6f; }
    }
}

//==============================================================================
void SendDelay::prepare (double sampleRate)
{
    sr = sampleRate;
    const size_t size = (size_t) (sampleRate * 2.5) + 4;
    bufL.assign (size, 0.0f);
    bufR.assign (size, 0.0f);
    w = 0;
    timeSmooth = 0.0f;
}

void SendDelay::process (const float* inL, const float* inR, float* outL, float* outR, int n, const Master& m)
{
    const int size = (int) bufL.size();
    const float target = (float) juce::jlimit (1.0, (double) size - 4, delayBeats (m.delayTime) * 60.0 / m.tempo * sr);
    if (timeSmooth <= 0.0f) timeSmooth = target;
    const float fb = juce::jlimit (0.0f, 0.95f, m.delayFb);
    const float lpK = 0.08f + 0.9f * m.delayTone, hpK = 0.02f;
    const float pp = m.delayPingPong;
    for (int i = 0; i < n; ++i)
    {
        timeSmooth += (target - timeSmooth) * 0.0005f;
        float pos = (float) w - timeSmooth;
        while (pos < 0.0f) pos += (float) size;
        const int i0 = (int) pos;
        const float f = pos - (float) i0;
        const int i1 = (i0 + 1) % size;
        float dl = bufL[(size_t) i0] * (1.0f - f) + bufL[(size_t) i1] * f;
        float dr = bufR[(size_t) i0] * (1.0f - f) + bufR[(size_t) i1] * f;
        // tone in the loop: darker repeats, no build-up of lows
        lpL += (dl - lpL) * lpK; lpR += (dr - lpR) * lpK;
        hpL += (lpL - hpL) * hpK; hpR += (lpR - hpR) * hpK;
        dl = lpL - hpL; dr = lpR - hpR;
        // ping-pong: the input enters on the left, the repeats cross over
        const float mono = 0.5f * (inL[i] + inR[i]);
        const float inl = inL[i] * (1.0f - pp) + mono * pp, inr = inR[i] * (1.0f - pp);
        const float fl = dsp::fastTanh (inl + (dl * (1.0f - pp) + dr * pp) * fb);
        const float fr = dsp::fastTanh (inr + (dr * (1.0f - pp) + dl * pp) * fb);
        bufL[(size_t) w] = fl;
        bufR[(size_t) w] = fr;
        w = (w + 1) % size;
        outL[i] += dl * 0.8f;
        outR[i] += dr * 0.8f;
    }
}

//==============================================================================
void MasterBus::prepare (double sampleRate)
{
    sr = sampleRate;
    limiter.prepare (sampleRate);
    lpL.reset(); lpR.reset(); hpL.reset(); hpR.reset();
    glueEnv = 0.0f; glueGain = 1.0f;
}

void MasterBus::process (float* l, float* r, int n, const Master& m)
{
    const float att = 1.0f - std::exp (-1.0f / (0.01f * (float) sr)), rel = 1.0f - std::exp (-1.0f / (0.2f * (float) sr));
    const float thr = 0.7f - 0.45f * m.glue;
    const float ratio = 1.0f + 5.0f * m.glue;
    const float makeup = 1.0f + 0.6f * m.glue;
    const float tape = m.tape;
    const float width = m.width * 2.0f;
    for (int s0 = 0; s0 < n; s0 += 16)
    {
        const int k = juce::jmin (16, n - s0);
        // the DJ filter (smoothed): below the middle a low-pass closes, above it a high-pass opens
        cut += (m.djFilter - cut) * 0.02f;
        const float d = cut - 0.5f;
        const bool lp = d < -0.01f, hp = d > 0.01f;
        if (lp) { const float hz = 20000.0f * std::exp2 (d * 2.0f * 9.5f); lpL.set (hz, 0.9f, (float) sr); lpR.set (hz, 0.9f, (float) sr); }
        if (hp) { const float hz = 20.0f * std::exp2 (d * 2.0f * 9.0f); hpL.set (hz, 0.9f, (float) sr); hpR.set (hz, 0.9f, (float) sr); }
        for (int j = 0; j < k; ++j)
        {
            const int i = s0 + j;
            float a = l[i], b = r[i];
            if (lp) { a = lpL.process (a).low; b = lpR.process (b).low; }
            else { lpL.process (a); lpR.process (b); }
            if (hp) { a = hpL.process (a).high; b = hpR.process (b).high; }
            else { hpL.process (a); hpR.process (b); }
            // glue: a gentle bus compressor
            if (m.glue > 0.001f)
            {
                const float lev = juce::jmax (std::abs (a), std::abs (b));
                glueEnv += (lev - glueEnv) * (lev > glueEnv ? att : rel);
                const float g = glueEnv > thr ? std::pow (glueEnv / thr, 1.0f / ratio - 1.0f) : 1.0f;
                glueGain = g;
                a *= g * makeup; b *= g * makeup;
            }
            // tape: soft saturation
            if (tape > 0.001f)
            {
                const float dr = 1.0f + tape * 3.0f;
                a = dsp::fastTanh (a * dr) / dr * (1.0f + tape);
                b = dsp::fastTanh (b * dr) / dr * (1.0f + tape);
            }
            // width
            const float mid = 0.5f * (a + b), side = 0.5f * (a - b) * width;
            vol += (m.volume - vol) * 0.001f;
            a = (mid + side) * vol;
            b = (mid - side) * vol;
            limiter.processSample (a, b);
            l[i] = a; r[i] = b;
        }
    }
}
} // namespace catbox

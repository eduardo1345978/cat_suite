#include "MixFx.h"
#include <cmath>

namespace catbox
{
//==============================================================================
float OctaveUp::process (float x) noexcept
{
    const int size = (int) buf.size();
    buf[(size_t) w] = x;
    // two heads read at double speed: the delay shrinks by one sample per sample, crossfaded
    const float window = (float) (size - 4);
    phase += 1.0f / window;
    if (phase >= 1.0f) phase -= 1.0f;
    float y = 0.0f;
    for (int h = 0; h < 2; ++h)
    {
        float ph = phase + 0.5f * (float) h;
        if (ph >= 1.0f) ph -= 1.0f;
        const float d = (1.0f - ph) * window + 2.0f;
        float pos = (float) w - d;
        while (pos < 0.0f) pos += (float) size;
        const int i0 = (int) pos;
        const float f = pos - (float) i0;
        const float s = buf[(size_t) i0] + (buf[(size_t) ((i0 + 1) % size)] - buf[(size_t) i0]) * f;
        y += s * std::sin (dsp::pi * ph);
    }
    w = (w + 1) % size;
    return y * 0.7f;
}

void SendReverb::prepare (double sampleRate)
{
    sr = sampleRate;
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    last = {};
    l.assign (4096, 0.0f);
    r.assign (4096, 0.0f);
    preL.assign ((size_t) (sr * 0.26), 0.0f);
    preR.assign ((size_t) (sr * 0.26), 0.0f);
    preW = 0;
    lowCutL.reset(); lowCutR.reset(); toneL.reset(); toneR.reset();
    const int sizes[8] = { 142, 107, 379, 277, 151, 113, 383, 263 };
    for (int i = 0; i < 8; ++i) diff[(size_t) i].prepare ((int) (sizes[i] * sr / 44100.0));
    dispL.fill (0.0f); dispR.fill (0.0f);
    shimL.prepare ((int) (sr * 0.08)); shimR.prepare ((int) (sr * 0.08));
    shimFbL = shimFbR = 0.0f;
}

void SendReverb::process (const float* inL, const float* inR, float* outL, float* outR, int n, const Master& m, float duck)
{
    const int type = m.revType;
    juce::Reverb::Parameters p;
    float size = m.revSize, damp = m.revDamp;
    switch (type)
    {
        case 1:  p.roomSize = 0.1f + 0.45f * size; p.damping = 0.3f + 0.5f * damp; break;    // room
        case 2:  p.roomSize = 0.4f + 0.5f * size; p.damping = 0.4f * damp; break;             // plate: bright
        case 3:  p.roomSize = 0.6f + 0.39f * size; p.damping = 0.3f * damp + 0.1f; break;    // shimmer
        case 4:  p.roomSize = 0.85f + 0.145f * size; p.damping = 0.4f + 0.4f * damp; break;  // cloud
        case 5:  p.roomSize = 0.3f + 0.4f * size; p.damping = 0.2f + 0.4f * damp; break;     // spring
        case 6:  p.roomSize = 0.5f + 0.48f * size; p.damping = 0.6f + 0.4f * damp; break;    // dark
        default: p.roomSize = 0.3f + 0.68f * size; p.damping = damp; break;                  // hall
    }
    p.width = m.revWidth;
    p.wetLevel = 1.0f;
    p.dryLevel = 0.0f;
    p.freezeMode = 0.0f;
    if (p.roomSize != last.roomSize || p.damping != last.damping || p.width != last.width) { reverb.setParameters (p); last = p; }
    const float lowCut = 40.0f * std::exp2 (m.revLowCut * 4.0f);   // 40 .. 640 Hz: a muddy-free tail
    lowCutL.set (lowCut, 0.7f, (float) sr);
    lowCutR.set (lowCut, 0.7f, (float) sr);
    const float toneHz = type == 6 ? 2200.0f : type == 5 ? 3800.0f : type == 2 ? 14000.0f : 9000.0f;
    toneL.set (toneHz, 0.7f, (float) sr); toneR.set (toneHz, 0.7f, (float) sr);
    const int preSize = (int) preL.size();
    const int pre = juce::jlimit (0, preSize - 2, (int) (m.revPre * 0.25f * (float) sr));
    const float target = m.revLevel * (1.0f - m.revDuck * juce::jlimit (0.0f, 1.0f, duck * 1.5f));
    for (int s0 = 0; s0 < n; s0 += 4096)
    {
        const int k = juce::jmin (4096, n - s0);
        for (int i = 0; i < k; ++i)
        {
            float a = lowCutL.process (inL[s0 + i]).high, b = lowCutR.process (inR[s0 + i]).high;
            // pre-delay
            preL[(size_t) preW] = a; preR[(size_t) preW] = b;
            const int rd = (preW - pre + preSize) % preSize;
            a = preL[(size_t) rd]; b = preR[(size_t) rd];
            preW = (preW + 1) % preSize;
            if (type == 2 || type == 4)   // plate / cloud: dense from the start
            {
                for (int d = 0; d < 4; ++d) { a = diff[(size_t) d].process (a, 0.62f); b = diff[(size_t) (d + 4)].process (b, 0.62f); }
            }
            if (type == 4)   // cloud: a slowly moving smear
            {
                modPhase += 0.13f / (float) sr; if (modPhase >= 1.0f) modPhase -= 1.0f;
                const float g = 0.55f + 0.2f * std::sin (dsp::twoPi * modPhase);
                a = diff[2].process (a, g); b = diff[6].process (b, g);
            }
            if (type == 5)   // spring: a chirp of all-passes
            {
                for (int d = 0; d < 24; ++d)
                {
                    const float ya = -0.72f * a + dispL[(size_t) d]; dispL[(size_t) d] = a + 0.72f * ya; a = ya;
                    const float yb = -0.72f * b + dispR[(size_t) d]; dispR[(size_t) d] = b + 0.72f * yb; b = yb;
                }
            }
            if (type == 3) { a += shimFbL * 0.42f; b += shimFbR * 0.42f; }
            l[(size_t) i] = a; r[(size_t) i] = b;
        }
        reverb.processStereo (l.data(), r.data(), k);
        const float dg = (target - gain) / (float) k;
        for (int i = 0; i < k; ++i)
        {
            float a = l[(size_t) i], b = r[(size_t) i];
            if (type == 3)   // shimmer: the tail an octave up feeds back in
            {
                shimFbL = dsp::fastTanh (shimL.process (a));
                shimFbR = dsp::fastTanh (shimR.process (b));
            }
            if (type == 5 || type == 6 || type == 2) { a = toneL.process (a).low; b = toneR.process (b).low; }
            gain += dg;
            outL[s0 + i] += a * 0.6f * gain;
            outR[s0 + i] += b * 0.6f * gain;
        }
    }
}

//==============================================================================
void SendDelay::prepare (double sampleRate)
{
    sr = sampleRate;
    const size_t size = (size_t) (sampleRate * 4.5) + 4;
    bufL.assign (size, 0.0f);
    bufR.assign (size, 0.0f);
    w = 0;
    timeSmooth = 0.0f;
    const int sizes[6] = { 113, 337, 241, 127, 353, 229 };
    for (int i = 0; i < 6; ++i) diff[(size_t) i].prepare ((int) (sizes[i] * sr / 44100.0));
}

float SendDelay::tap (const std::vector<float>& buf, float back) const noexcept
{
    const int size = (int) buf.size();
    back = juce::jlimit (1.0f, (float) size - 3.0f, back);
    float pos = (float) w - back;
    while (pos < 0.0f) pos += (float) size;
    const int i0 = (int) pos;
    const float f = pos - (float) i0;
    return buf[(size_t) i0] + (buf[(size_t) ((i0 + 1) % size)] - buf[(size_t) i0]) * f;
}

void SendDelay::process (const float* inL, const float* inR, float* outL, float* outR, float* revL, float* revR, int n, const Master& m, float duck)
{
    const int size = (int) bufL.size();
    const int type = m.delayType;
    const float target = (float) juce::jlimit (1.0, (double) size / 2.2, delayBeats (m.delayTime) * 60.0 / m.tempo * sr);
    if (timeSmooth <= 0.0f) timeSmooth = target;
    float fb = juce::jlimit (0.0f, 0.95f, m.delayFb);
    float lpK = 0.08f + 0.9f * m.delayTone, hpK = 0.02f;
    float pp = m.delayPingPong;
    const float wow = m.delayWow * (type == 1 ? 1.0f : type == 2 ? 0.6f : 0.25f);
    float drive = 1.0f;
    switch (type)
    {
        case 1: lpK *= 0.7f; drive = 1.6f; break;                            // tape echo: darker, saturated
        case 2: lpK *= 0.45f; hpK = 0.06f; fb = juce::jmin (1.02f, fb * 1.1f); drive = 1.3f; break;   // dub: can run away (the tanh holds it)
        case 3: pp = 1.0f; break;                                            // ping-pong
        default: break;
    }
    const float level = m.delayLevel * (1.0f - m.delayDuck * juce::jlimit (0.0f, 1.0f, duck * 1.5f));
    const float dg = (level - gain) / (float) juce::jmax (1, n);
    const float toRev = m.delayToRev;
    for (int i = 0; i < n; ++i)
    {
        timeSmooth += (target - timeSmooth) * 0.0005f;
        wowPhase += 0.45f / (float) sr; if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutPhase += 6.5f / (float) sr; if (flutPhase >= 1.0f) flutPhase -= 1.0f;
        const float t = timeSmooth * (1.0f + wow * (0.012f * std::sin (dsp::twoPi * wowPhase) + 0.002f * std::sin (dsp::twoPi * flutPhase)));
        float dl, dr;
        if (type == 6)   // reverse: two heads play the last stretch backwards, crossfaded
        {
            revPhase += 1.0f / juce::jmax (64.0f, t);
            if (revPhase >= 1.0f) revPhase -= 1.0f;
            dl = dr = 0.0f;
            for (int h = 0; h < 2; ++h)
            {
                float ph = revPhase + 0.5f * (float) h;
                if (ph >= 1.0f) ph -= 1.0f;
                const float back = 2.0f * ph * t + 1.0f;
                const float win = std::sin (dsp::pi * ph);
                dl += tap (bufL, back) * win;
                dr += tap (bufR, back) * win;
            }
            dl *= 0.75f; dr *= 0.75f;
        }
        else
        {
            dl = tap (bufL, t);
            dr = tap (bufR, t);
        }
        // tone in the loop: darker repeats, no build-up of lows
        lpL += (dl - lpL) * lpK; lpR += (dr - lpR) * lpK;
        hpL += (lpL - hpL) * hpK; hpR += (lpR - hpR) * hpK;
        dl = lpL - hpL; dr = lpR - hpR;
        float loopL = dl, loopR = dr;
        if (type == 5)   // diffused: every repeat smears more
        {
            for (int d = 0; d < 3; ++d) { loopL = diff[(size_t) d].process (loopL, 0.6f); loopR = diff[(size_t) (d + 3)].process (loopR, 0.6f); }
        }
        // ping-pong: the input enters on the left, the repeats cross over
        const float mono = 0.5f * (inL[i] + inR[i]);
        const float inl = inL[i] * (1.0f - pp) + mono * pp, inr = inR[i] * (1.0f - pp);
        const float fl = dsp::fastTanh ((inl + (loopL * (1.0f - pp) + loopR * pp) * fb) * drive) / drive;
        const float fr = dsp::fastTanh ((inr + (loopR * (1.0f - pp) + loopL * pp) * fb) * drive) / drive;
        bufL[(size_t) w] = fl + rng.next() * 1.0e-9f;
        bufR[(size_t) w] = fr;
        w = (w + 1) % size;
        float ol = type == 5 ? loopL : dl, orr = type == 5 ? loopR : dr;
        if (type == 4)   // multi-tap: 3/4 and 1/2 of the time too
        {
            ol += 0.55f * tap (bufL, t * 0.75f) + 0.35f * tap (bufR, t * 0.5f);
            orr += 0.55f * tap (bufR, t * 0.75f) + 0.35f * tap (bufL, t * 0.5f);
        }
        gain += dg;
        outL[i] += ol * 0.8f * gain;
        outR[i] += orr * 0.8f * gain;
        if (toRev > 0.0f && revL != nullptr) { revL[i] += ol * toRev * gain; revR[i] += orr * toRev * gain; }
    }
}

//==============================================================================
void SendFx::process (const float* inL, const float* inR, float* outL, float* outR, int n, const Master& m, double ppq, double ppqPerSample, float duck)
{
    const float target = m.fxLevel * (1.0f - m.fxDuck * juce::jlimit (0.0f, 1.0f, duck * 1.5f));
    for (int s0 = 0; s0 < n; s0 += 4096)
    {
        const int k = juce::jmin (4096, n - s0);
        std::copy (inL + s0, inL + s0 + k, tl.begin());
        std::copy (inR + s0, inR + s0 + k, tr.begin());
        if (m.fxSend.type != InsOff) fx.process (m.fxSend.type, m.fxSend.p.data(), tl.data(), tr.data(), k, ppq + s0 * ppqPerSample, ppqPerSample, m.tempo);
        else { std::fill (tl.begin(), tl.begin() + k, 0.0f); std::fill (tr.begin(), tr.begin() + k, 0.0f); }
        const float dg = (target - gain) / (float) k;
        for (int i = 0; i < k; ++i) { gain += dg; outL[s0 + i] += tl[(size_t) i] * gain; outR[s0 + i] += tr[(size_t) i] * gain; }
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

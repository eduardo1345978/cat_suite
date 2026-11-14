#include "MoltenEngine.h"

namespace molten
{
juce::StringArray dripTimeNames() { return { "1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2" }; }
double dripTimeBeats (int i)
{
    static const double b[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    return b[juce::jlimit (0, 5, i)];
}

namespace
{
constexpr float pi = juce::MathConstants<float>::pi;
inline int pow2Above (int n) { int s = 1; while (s < n) s <<= 1; return s; }
inline float readRing (const std::vector<float>& b, int mask, double pos) noexcept
{
    const auto i = (int) std::floor (pos);
    const float f = (float) (pos - i);
    const float a = b[(size_t) (i & mask)], c = b[(size_t) ((i + 1) & mask)];
    return a + (c - a) * f;
}
inline float onePoleCoef (double sr, double hz) { return (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * hz / sr)); }
}

void Engine::Shelf::set (double sampleRate, double f, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * f / sampleRate;
    const double cs = std::cos (w), sn = std::sin (w), alpha = sn / 2.0 * std::sqrt (2.0);   // shelf slope 1
    const double sq = 2.0 * std::sqrt (A) * alpha;
    const double a0 = (A + 1) + (A - 1) * cs + sq;
    b0 = (float) (A * ((A + 1) - (A - 1) * cs + sq) / a0);
    b1 = (float) (2 * A * ((A - 1) - (A + 1) * cs) / a0);
    b2 = (float) (A * ((A + 1) - (A - 1) * cs - sq) / a0);
    a1 = (float) (-2 * ((A - 1) + (A + 1) * cs) / a0);
    a2 = (float) (((A + 1) + (A - 1) * cs - sq) / a0);
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = juce::jmax (16, maxBlock);
    os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    os->initProcessing ((size_t) maxBlock);
    osLatency = (int) std::lround (os->getLatencyInSamples());
    baseDelay = (int) std::lround (0.012 * sr);
    latency = osLatency + baseDelay;

    wobMask = pow2Above ((int) (0.05 * sr)) - 1;
    wobL.assign ((size_t) wobMask + 1, 0.0f); wobR.assign ((size_t) wobMask + 1, 0.0f);
    dripMask = pow2Above ((int) (6.0 * sr)) - 1;          // up to 2 beats at 30 BPM + the shifter's window
    dripBufL.assign ((size_t) dripMask + 1, 0.0f); dripBufR.assign ((size_t) dripMask + 1, 0.0f);
    dryMask = pow2Above (latency + maxBlock + 8) - 1;
    dryL.assign ((size_t) dryMask + 1, 0.0f); dryR.assign ((size_t) dryMask + 1, 0.0f);
    wet.setSize (2, maxBlock, false, false, true);
    lastBodyDb = -100.0f;
    reset();
}

void Engine::reset()
{
    std::fill (wobL.begin(), wobL.end(), 0.0f); std::fill (wobR.begin(), wobR.end(), 0.0f);
    std::fill (dripBufL.begin(), dripBufL.end(), 0.0f); std::fill (dripBufR.begin(), dripBufR.end(), 0.0f);
    std::fill (dryL.begin(), dryL.end(), 0.0f); std::fill (dryR.begin(), dryR.end(), 0.0f);
    if (os) os->reset();
    body.z1[0] = body.z1[1] = body.z2[0] = body.z2[1] = 0.0f;
    env = sagSmooth = drift = driftTarget = 0.0f;
    lpL = lpR = dripLpL = dripLpR = dripEnv = sideLp1 = sideLp2 = 0.0f;
    {
        // RBJ high-pass, Q = 0.7071, 120 Hz
        const double w = juce::MathConstants<double>::twoPi * 120.0 / sr, cs = std::cos (w), al = std::sin (w) / (2.0 * 0.70710678);
        const double a0 = 1.0 + al;
        sideHp = {};
        sideHp.b0 = (float) ((1.0 + cs) * 0.5 / a0); sideHp.b1 = (float) (-(1.0 + cs) / a0); sideHp.b2 = sideHp.b0;
        sideHp.a1 = (float) (-2.0 * cs / a0); sideHp.a2 = (float) ((1.0 - al) / a0);
    }
    inPow = outPow = 1.0e-6;
    gain = 1.0f;
    shiftPh = 0.0;
    wobPos = dripPos = dryPos = 0;      // the buffers may have got smaller (a new sample rate or block size)
}

void Engine::process (juce::AudioBuffer<float>& buffer, const Settings& settings)
{
    s = settings;
    const int total = buffer.getNumSamples();
    const int maxChunk = wet.getNumSamples();
    for (int start = 0; start < total; start += maxChunk)
    {
        const int n = juce::jmin (maxChunk, total - start);
        float* L = buffer.getWritePointer (0, start);
        float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : nullptr;
        float* wl = wet.getWritePointer (0);
        float* wr = wet.getWritePointer (1);

        if (std::abs (s.bodyDb - lastBodyDb) > 0.01f) { lastBodyDb = s.bodyDb; body.set (sr, 110.0, s.bodyDb); }

        // ---- in: meter, auto-gain reference, the dry copy, BODY, WOBBLE
        const float envA = onePoleCoef (sr, 1.0 / 0.005 / juce::MathConstants<double>::twoPi), envR = onePoleCoef (sr, 1.0 / 0.3 / juce::MathConstants<double>::twoPi);
        const float sagC = onePoleCoef (sr, 2.0);
        const double powC = 1.0 - std::exp (-1.0 / (0.4 * sr));
        const float h = s.heat;
        const double wobDepth = s.wobble * (0.3 + 0.7 * h) * 0.004 * sr;
        const double sagDepth = s.wobble * (0.25 + 0.75 * h) * 0.0035 * sr;
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float xl = L[i], xr = R != nullptr ? R[i] : xl;
            peak = juce::jmax (peak, std::abs (xl), std::abs (xr));
            inPow += ((double) (xl * xl + xr * xr) * 0.5 - inPow) * powC;
            dryL[(size_t) dryPos] = xl; dryR[(size_t) dryPos] = xr;
            dryPos = (dryPos + 1) & dryMask;

            const float bl = body.run (xl, 0), br = body.run (xr, 1);
            // the loudness makes the pitch sag
            const float a = std::abs (bl) + std::abs (br);
            env += (a - env) * (a > env ? envA : envR);
            sagSmooth += (juce::jmin (1.0f, env) - sagSmooth) * sagC;

            // tape wow + flutter + a slow drift
            wowPh += 0.62 / sr; flutterPh += 6.8 / sr; driftPh += 1.0 / sr;
            if (wowPh >= 1.0) wowPh -= 1.0;
            if (flutterPh >= 1.0) flutterPh -= 1.0;
            if (driftPh >= 0.7) { driftPh = 0.0; driftTarget = random.nextFloat() * 2.0f - 1.0f; }
            drift += (driftTarget - drift) * 0.00004f;
            const double mod = 0.7 * std::sin (juce::MathConstants<double>::twoPi * wowPh) + 0.18 * std::sin (juce::MathConstants<double>::twoPi * flutterPh) + 0.6 * drift;
            const double d = juce::jmax (2.0, baseDelay + wobDepth * mod + sagDepth * (sagSmooth - 0.3));
            wobL[(size_t) wobPos] = bl; wobR[(size_t) wobPos] = br;
            wl[i] = readRing (wobL, wobMask, wobPos - d);
            wr[i] = readRing (wobR, wobMask, wobPos - d);
            wobPos = (wobPos + 1) & wobMask;
        }
        inLevel = juce::jmax (peak, inLevel.load() * 0.85f);
        sag = sagSmooth;

        // ---- HEAT: valve-like saturation, 2x oversampled
        {
            juce::dsp::AudioBlock<float> block (wet.getArrayOfWritePointers(), 2, (size_t) n);
            auto up = os->processSamplesUp (block);
            // HEAT glides over the block (no zipper noise when the knob moves)
            const float h0 = heatSmooth, h1 = heatSmooth + (h - heatSmooth) * juce::jmin (1.0f, (float) n / (float) (0.05 * sr));
            heatSmooth = h1;
            auto curve = [] (float hh, float& k, float& b, float& tb, float& amt)
            {
                k = 1.0f + 18.0f * std::pow (hh, 1.6f);
                b = 0.12f * hh;
                tb = std::tanh (b);
                amt = juce::jmin (1.0f, hh * 6.0f);
            };
            float k0, b0, tb0, a0, k1, b1, tb1, a1;
            curve (h0, k0, b0, tb0, a0);
            curve (h1, k1, b1, tb1, a1);
            const auto len = up.getNumSamples();
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                float* x = up.getChannelPointer (ch);
                for (size_t i = 0; i < len; ++i)
                {
                    const float t = (float) i / (float) len;
                    const float k = k0 + (k1 - k0) * t, bb = b0 + (b1 - b0) * t, tb = tb0 + (tb1 - tb0) * t, amt = a0 + (a1 - a0) * t;
                    const float shaped = (std::tanh (k * x[i] + bb) - tb) / (k * (1.0f - tb * tb));
                    x[i] += (shaped - x[i]) * amt;
                }
            }
            os->processSamplesDown (block);
        }
        // a softer top the hotter it gets (no fizz)
        {
            const float c = onePoleCoef (sr, 20000.0 * std::pow (0.35, (double) h));
            for (int i = 0; i < n; ++i) { lpL += (wl[i] - lpL) * c; lpR += (wr[i] - lpR) * c; wl[i] = lpL; wr[i] = lpR; }
        }

        // ---- DRIP: echoes that fall (each repeat lower and darker)
        if (s.drip > 0.001f)
        {
            const double D = juce::jlimit (0.03, 4.0, s.dripBeats * 60.0 / juce::jmax (30.0, s.bpm)) * sr;
            const double W = 0.045 * sr;                           // the shifter's window
            const double semis = 1.0 + 3.0 * h;                    // hotter: they fall faster
            const double ratio = std::pow (2.0, -semis / 12.0);
            const double rate = (1.0 - ratio) / W;
            const float fb = 0.25f + 0.6f * s.drip;
            const float send = 0.6f * s.drip;
            const float level = 0.45f + 0.75f * s.drip;
            const float dark = onePoleCoef (sr, 4500.0 - 3000.0 * h);
            float dpeak = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                shiftPh += rate; if (shiftPh >= 1.0) shiftPh -= 1.0;
                float el = 0.0f, er = 0.0f;
                for (int head = 0; head < 2; ++head)
                {
                    double f = shiftPh + 0.5 * head; if (f >= 1.0) f -= 1.0;
                    const float g = std::sin (pi * (float) f); const float g2 = g * g;   // Hann windows add up to 1
                    el += g2 * readRing (dripBufL, dripMask, dripPos - (D + f * W));
                    er += g2 * readRing (dripBufR, dripMask, dripPos - (D * 1.03 + f * W));
                }
                dripLpL += (el - dripLpL) * dark; dripLpR += (er - dripLpR) * dark;
                // ping-pong a little: each side feeds the other
                dripBufL[(size_t) dripPos] = wl[i] * send + std::tanh (dripLpR * fb);
                dripBufR[(size_t) dripPos] = wr[i] * send + std::tanh (dripLpL * fb);
                dripPos = (dripPos + 1) & dripMask;
                wl[i] += dripLpL * level;
                wr[i] += dripLpR * level;
                dpeak = juce::jmax (dpeak, std::abs (dripLpL), std::abs (dripLpR));
            }
            dripEnv = juce::jmax (dpeak, dripEnv * 0.8f);
            const bool loud = dripEnv > 0.04f;
            if (loud && ! dripWasLoud) ++drips;
            if (dripEnv < 0.015f) dripWasLoud = false; else if (loud) dripWasLoud = true;
            dripLevel = dripEnv;
        }
        else
        {
            dripLevel = 0.0f;
            // let the buffer empty out so turning DRIP back up doesn't replay old sound
            for (int i = 0; i < n; ++i) { dripBufL[(size_t) dripPos] = 0.0f; dripBufR[(size_t) dripPos] = 0.0f; dripPos = (dripPos + 1) & dripMask; }
        }

        // ---- the bass stays in the middle (mono under ~120 Hz)
        if (s.monoBass)
        {
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (wl[i] + wr[i]);
                float side = 0.5f * (wl[i] - wr[i]);
                for (int st = 0; st < 2; ++st)
                {
                    auto& f = sideHp;
                    const float y = f.b0 * side + f.z1[st];
                    f.z1[st] = f.b1 * side - f.a1 * y + f.z2[st];
                    f.z2[st] = f.b2 * side - f.a2 * y;
                    side = y;
                }
                wl[i] = m + side; wr[i] = m - side;
            }
        }

        // ---- auto gain: as loud as what came in (slow, so it never pumps)
        const float outG = juce::Decibels::decibelsToGain (s.outDb);
        const float mix = s.mix;
        float opeak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            outPow += ((double) (wl[i] * wl[i] + wr[i] * wr[i]) * 0.5 - outPow) * powC;
            if (inPow > 1.0e-7 && outPow > 1.0e-9)
            {
                const float target = (float) juce::jlimit (0.1, 4.0, std::sqrt (inPow / outPow));
                gain += (target - gain) * 0.00002f;
            }
            const int dp = (dryPos - n + i - latency) & dryMask;
            const float dl = dryL[(size_t) dp], dr = dryR[(size_t) dp];
            const float ol = (dl + (wl[i] * gain - dl) * mix) * outG;
            const float orr = (dr + (wr[i] * gain - dr) * mix) * outG;
            L[i] = ol;
            if (R != nullptr) R[i] = orr;
            opeak = juce::jmax (opeak, std::abs (ol), std::abs (orr));
        }
        outLevel = juce::jmax (opeak, outLevel.load() * 0.85f);
        autoGainDb = juce::Decibels::gainToDecibels (gain);
    }
}
} // namespace molten

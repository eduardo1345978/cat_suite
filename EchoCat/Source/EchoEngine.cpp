#include "EchoEngine.h"

namespace echocat
{
namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    constexpr double maxDelaySeconds = 4.0;

    inline float onePole (double fc, double sr) { return (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * fc / sr)); }

    /** 4-point Hermite read from a ring buffer, d samples behind the write position. */
    inline float readAt (const std::vector<float>& b, int w, float d) noexcept
    {
        const int size = (int) b.size();
        float rp = (float) w - d;
        while (rp < 0.0f) rp += (float) size;
        const int i1 = ((int) rp) % size;
        const float t = rp - std::floor (rp);
        const int i0 = (i1 - 1 + size) % size, i2 = (i1 + 1) % size, i3 = (i1 + 2) % size;
        const float y0 = b[(size_t) i0], y1 = b[(size_t) i1], y2 = b[(size_t) i2], y3 = b[(size_t) i3];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
}

juce::StringArray divisionNames()
{
    return { "1/32", "1/16", "1/16 dotted", "1/8 triplet", "1/8", "1/8 dotted", "1/4 triplet", "1/4", "1/4 dotted", "1/2", "1 bar" };
}

double divisionBeats (int index)
{
    static const double beats[numDivisions] = { 0.125, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0 };
    return beats[juce::jlimit (0, numDivisions - 1, index)];
}

double Settings::delaySeconds() const
{
    const double s = sync ? divisionBeats (division) * 60.0 / juce::jlimit (30.0, 300.0, bpm) : timeMs / 1000.0;
    return juce::jlimit (0.005, maxDelaySeconds - 0.05, s);
}

//==============================================================================
void OctaveUp::prepare (double sampleRate)
{
    grain = juce::jlimit (512, 8192, (int) (sampleRate * 0.045));
    buf.assign ((size_t) grain * 2 + 8, 0.0f);
    reset();
}

void OctaveUp::reset()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    w = 0;
    phase = 0.0f;
}

float OctaveUp::process (float x) noexcept
{
    const int size = (int) buf.size();
    buf[(size_t) w] = x;
    // two read heads half a grain apart; each sweeps its delay from one grain down to nothing (= double speed)
    float out = 0.0f;
    for (int k = 0; k < 2; ++k)
    {
        float ph = phase + 0.5f * (float) k;
        if (ph >= 1.0f) ph -= 1.0f;
        const float d = (1.0f - ph) * (float) grain + 2.0f;
        float rp = (float) w - d;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = ((int) rp) % size;
        const float fr = rp - std::floor (rp);
        const float v = buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) ((i0 + 1) % size)] * fr;
        const float win = std::sin (juce::MathConstants<float>::pi * ph);
        out += v * win * win;
    }
    phase += 1.0f / (float) grain;
    if (phase >= 1.0f) phase -= 1.0f;
    w = (w + 1) % size;
    return out;
}

//==============================================================================
void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    const auto len = (size_t) (sr * (maxDelaySeconds + 0.2)) + 16;
    lineL.assign (len, 0.0f);
    lineR.assign (len, 0.0f);
    const auto mb = (size_t) juce::jmax (32, maxBlock);
    revL.assign (mb, 0.0f); revR.assign (mb, 0.0f);
    shimL.assign (mb, 0.0f); shimR.assign (mb, 0.0f);
    reverb.setSampleRate (sr);
    for (auto& s : shifters) s.prepare (sr);
    reset();
}

void Engine::reset()
{
    std::fill (lineL.begin(), lineL.end(), 0.0f);
    std::fill (lineR.begin(), lineR.end(), 0.0f);
    std::fill (shimL.begin(), shimL.end(), 0.0f);
    std::fill (shimR.begin(), shimR.end(), 0.0f);
    w = 0;
    delaySmooth = -1.0f;
    lpL = lpR = hpL = hpR = 0.0f;
    wobPhase1 = wobPhase2 = 0.0;
    env = freezeMix = 0.0f;
    shimLp = shimLpR = 0.0f;
    reverb.reset();
    for (auto& s : shifters) s.reset();
}

void Engine::process (float* l, float* r, int n, const Settings& s)
{
    if (lineL.empty() || n <= 0) return;
    // the work buffers were sized in prepare; split a bigger block (some hosts send more than promised)
    if (n > (int) revL.size())
    {
        for (int done = 0; done < n; done += (int) revL.size())
            process (l + done, r + done, juce::jmin ((int) revL.size(), n - done), s);
        return;
    }
    const int size = (int) lineL.size();
    const float target = juce::jlimit (16.0f, (float) size - 16.0f, (float) (s.delaySeconds() * sr));
    if (delaySmooth < 0.0f) delaySmooth = target;
    const float glide = 1.0f - std::exp (-1.0f / (0.12f * (float) sr));         // a tape-like glide to a new time
    const float fb = juce::jlimit (0.0f, 0.97f, s.feedback * 0.97f);
    const float freezeStep = 1.0f / (0.05f * (float) sr);
    const float aLp = onePole (1500.0 * std::pow (12.0, (double) juce::jlimit (0.0f, 1.0f, s.tone)), sr);
    const float aHp = onePole (20.0 * std::pow (40.0, (double) juce::jlimit (0.0f, 1.0f, s.lowcut)), sr);
    const float wobDepth = juce::jlimit (0.0f, 1.0f, s.wobble) * 0.0045f * (float) sr;
    const double wobInc1 = 0.55 / sr, wobInc2 = 5.7 / sr;
    const float k = 1.0f + 5.0f * s.drive, driveNorm = 1.0f / std::tanh (k * 0.5f) * 0.5f;
    const float att = 1.0f - std::exp (-1.0f / (0.005f * (float) sr)), rel = 1.0f - std::exp (-1.0f / (0.25f * (float) sr));
    const float pp = juce::jlimit (0.0f, 1.0f, s.pingpong);
    const float duck = juce::jlimit (0.0f, 1.0f, s.duck);
    const float space = juce::jlimit (0.0f, 1.0f, s.space);
    const float targetGain = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 6.0f, s.outputDb));
    float inPeak = 0.0f, echoPeak = 0.0f, duckShown = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float inL = l[i], inR = r[i];
        const float mono = 0.5f * (inL + inR);
        const float lev = juce::jmax (std::abs (inL), std::abs (inR));
        env += (lev > env ? att : rel) * (lev - env);
        inPeak = juce::jmax (inPeak, lev);
        freezeMix = juce::jlimit (0.0f, 1.0f, freezeMix + (s.freeze ? freezeStep : -freezeStep));

        delaySmooth += (target - delaySmooth) * glide;
        wobPhase1 += wobInc1; if (wobPhase1 >= 1.0) wobPhase1 -= 1.0;
        wobPhase2 += wobInc2; if (wobPhase2 >= 1.0) wobPhase2 -= 1.0;
        const float mod = wobDepth * (0.8f * (1.0f + std::sin (twoPi * (float) wobPhase1)) + 0.12f * std::sin (twoPi * (float) wobPhase2));
        const float d = juce::jlimit (4.0f, (float) size - 8.0f, delaySmooth + mod);
        const float dL = readAt (lineL, w, d), dR = readAt (lineR, w, d);

        // each time round: darker, thinner, warmer (not while frozen: then the loop holds as it is)
        float fL = dL, fR = dR;
        lpL += aLp * (fL - lpL); lpR += aLp * (fR - lpR);
        hpL += aHp * (lpL - hpL); hpR += aHp * (lpR - hpR);
        float cL = lpL - hpL, cR = lpR - hpR;
        if (s.drive > 0.002f) { cL = std::tanh (k * cL) * driveNorm; cR = std::tanh (k * cR) * driveNorm; }
        fL = cL + (dL - cL) * freezeMix;
        fR = cR + (dR - cR) * freezeMix;

        const float loopFb = fb + (1.0f - fb) * freezeMix;
        const float inGain = 1.0f - freezeMix;
        const float crossL = fL + (fR - fL) * pp, crossR = fR + (fL - fR) * pp;
        float wL = (inL + (mono - inL) * pp) * inGain + crossL * loopFb;
        float wR = inR * (1.0f - pp) * inGain + crossR * loopFb;
        if (! std::isfinite (wL) || ! std::isfinite (wR)) { wL = wR = 0.0f; lpL = lpR = hpL = hpR = 0.0f; }
        // never let the loop run away (a soft ceiling far above normal levels)
        wL = 2.0f * std::tanh (0.5f * wL);
        wR = 2.0f * std::tanh (0.5f * wR);
        lineL[(size_t) w] = wL;
        lineR[(size_t) w] = wR;
        w = (w + 1) % size;

        // ducking: the echoes step back while the input is loud
        const float duckGain = 1.0f - duck * 0.85f * juce::jmin (1.0f, env * 4.0f);
        duckShown = juce::jmax (duckShown, 1.0f - duckGain);
        const float eL = fL * duckGain, eR = fR * duckGain;
        echoPeak = juce::jmax (echoPeak, std::abs (eL), std::abs (eR));

        l[i] = inL * s.dry + eL * s.mix;
        r[i] = inR * s.dry + eR * s.mix;
        // the space: the input and the echoes into the reverb (plus the shimmer from the last block)
        revL[(size_t) i] = space * (0.35f * inL * inGain + 0.6f * eL) + shimL[(size_t) i];
        revR[(size_t) i] = space * (0.35f * inR * inGain + 0.6f * eR) + shimR[(size_t) i];
    }

    float spacePeak = 0.0f;
    if (space > 0.001f || s.shimmer > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.78f + 0.2f * space;
        rp.damping = 0.35f + 0.25f * (1.0f - s.tone);
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.processStereo (revL.data(), revR.data(), n);
        const float shimGain = juce::jlimit (0.0f, 1.0f, s.shimmer) * 0.5f;
        const float aShim = onePole (6000.0, sr);
        for (int i = 0; i < n; ++i)
        {
            const float wl = revL[(size_t) i] * 0.9f, wr = revR[(size_t) i] * 0.9f;
            l[i] += wl;
            r[i] += wr;
            spacePeak = juce::jmax (spacePeak, std::abs (wl), std::abs (wr));
            // shimmer: the tail an octave up, fed back into the space next block
            shimLp += aShim * (shifters[0].process (revL[(size_t) i]) - shimLp);
            shimLpR += aShim * (shifters[1].process (revR[(size_t) i]) - shimLpR);
            shimL[(size_t) i] = std::isfinite (shimLp) ? shimLp * shimGain : 0.0f;
            shimR[(size_t) i] = std::isfinite (shimLpR) ? shimLpR * shimGain : 0.0f;
        }
        for (int i = n; i < (int) shimL.size(); ++i) shimL[(size_t) i] = shimR[(size_t) i] = 0.0f;
        if (! std::isfinite (shimLp) || ! std::isfinite (shimLpR)) { shimLp = shimLpR = 0.0f; reverb.reset(); }
    }
    else
    {
        std::fill (shimL.begin(), shimL.end(), 0.0f);
        std::fill (shimR.begin(), shimR.end(), 0.0f);
    }

    // the output level (smoothed)
    for (int i = 0; i < n; ++i)
    {
        outGain += (targetGain - outGain) * 0.002f;
        l[i] *= outGain;
        r[i] *= outGain;
    }

    inLevel = juce::jmax (inPeak, inLevel.load() * 0.9f);
    echoLevel = juce::jmax (echoPeak, echoLevel.load() * 0.9f);
    spaceLevel = juce::jmax (spacePeak, spaceLevel.load() * 0.9f);
    duckAmount = duckShown;
    shownDelay = delaySmooth / sr;
}
} // namespace echocat

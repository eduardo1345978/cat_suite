#include "EchoEngine.h"

namespace echocat
{
namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    constexpr double maxDelaySeconds = 4.0;
    constexpr double lineSeconds = 8.4;           // reverse reads up to 2 echo times back

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

juce::StringArray modeNames() { return { "Normal", "Reverse", "Steps", "Resonator" }; }
juce::StringArray chordNames() { return { "One note", "Octaves", "Fifths", "Major", "Minor", "Sus 4", "Major 7", "Minor 7", "Cluster", "Chord-Cat's chord" }; }

int chordShape (int index, std::array<int, maxStrings>& out)
{
    static const std::vector<std::vector<int>> shapes = { { 0, 12 }, { 0, 12, 24, -12 }, { 0, 7, 12, 19 }, { 0, 4, 7, 12, 16, 19 },
                                                          { 0, 3, 7, 12, 15, 19 }, { 0, 5, 7, 12, 17, 19 }, { 0, 4, 7, 11, 12, 16 },
                                                          { 0, 3, 7, 10, 12, 15 }, { 0, 1, 2, 3, 5, 7 } };
    const auto& v = shapes[(size_t) juce::jlimit (0, (int) shapes.size() - 1, index)];
    int n = 0;
    for (auto x : v) if (n < maxStrings) out[(size_t) n++] = x;
    return n;
}

double Settings::delaySeconds() const
{
    const double s = sync ? divisionBeats (division) * 60.0 / juce::jlimit (30.0, 300.0, bpm) : timeMs / 1000.0;
    return juce::jlimit (0.005, maxDelaySeconds - 0.05, s);
}

//==============================================================================
void PitchShifter::prepare (double sampleRate)
{
    grain = juce::jlimit (512, 8192, (int) (sampleRate * 0.045));
    buf.assign ((size_t) grain * 2 + 8, 0.0f);
    reset();
}

void PitchShifter::reset()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    w = 0;
    phase = 0.0f;
}

float PitchShifter::process (float x) noexcept
{
    const int size = (int) buf.size();
    buf[(size_t) w] = x;
    // two read heads half a grain apart; each sweeps its delay from one grain down to nothing (faster:
    // higher) or from nothing up to one grain (slower: lower)
    const bool up = ratio >= 1.0f;
    float out = 0.0f;
    for (int k = 0; k < 2; ++k)
    {
        float ph = phase + 0.5f * (float) k;
        if (ph >= 1.0f) ph -= 1.0f;
        const float d = (up ? 1.0f - ph : ph) * (float) grain + 2.0f;
        float rp = (float) w - d;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = ((int) rp) % size;
        const float fr = rp - std::floor (rp);
        const float v = buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) ((i0 + 1) % size)] * fr;
        const float win = std::sin (juce::MathConstants<float>::pi * ph);
        out += v * win * win;
    }
    phase += std::abs (ratio - 1.0f) / (float) grain;
    if (phase >= 1.0f) phase -= 1.0f;
    w = (w + 1) % size;
    return out;
}

//==============================================================================
void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    const auto len = (size_t) (sr * lineSeconds) + 16;
    lineL.assign (len, 0.0f);
    lineR.assign (len, 0.0f);
    const auto mb = (size_t) juce::jmax (32, maxBlock);
    revL.assign (mb, 0.0f); revR.assign (mb, 0.0f);
    shimL.assign (mb, 0.0f); shimR.assign (mb, 0.0f);
    reverb.setSampleRate (sr);
    for (auto& s : shifters) { s.prepare (sr); s.setRatio (2.0f); }
    for (auto& s : loopShifters) s.prepare (sr);
    for (auto& st : strings) st.buf.assign ((size_t) (sr / 18.0) + 16, 0.0f);
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
    for (auto& s : loopShifters) s.reset();
    for (auto& st : strings) { std::fill (st.buf.begin(), st.buf.end(), 0.0f); st.w = 0; st.lp = 0.0f; st.delay = -1.0f; }
    revPhase = 0.0;
    fbLpL = fbLpR = fbHpL = fbHpR = 0.0f;
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
    // drive: tape saturation that never makes the loop louder (unity gain for quiet sounds, loud ones squashed),
    // else each time round the echoes would grow and run into the limiter
    const float k = 1.0f + 5.0f * s.drive, driveNorm = 1.0f / k;
    const float att = 1.0f - std::exp (-1.0f / (0.005f * (float) sr)), rel = 1.0f - std::exp (-1.0f / (0.25f * (float) sr));
    const float pp = juce::jlimit (0.0f, 1.0f, s.pingpong);
    const float duck = juce::jlimit (0.0f, 1.0f, s.duck);
    const float space = juce::jlimit (0.0f, 1.0f, s.space);
    const float targetGain = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 6.0f, s.outputDb));
    const int mode = juce::jlimit (0, 3, s.mode);
    const float semis = juce::jlimit (-12.0f, 12.0f, s.pitch);
    const bool pitched = std::abs (semis) > 0.01f;
    if (pitched) for (auto& ls : loopShifters) ls.setRatio (std::pow (2.0f, semis / 12.0f));
    // resonator: the strings' pitches (set by the processor: a chord, tuned by Micro-Cat over Cat Link)
    const int numRes = mode == ModeResonator ? juce::jlimit (1, maxStrings, s.numStrings) : 0;
    const float t60 = 0.15f + 9.0f * std::pow (juce::jlimit (0.0f, 1.0f, s.feedback), 2.0f);
    const float resDamp = onePole (800.0 * std::pow (20.0, (double) juce::jlimit (0.0f, 1.0f, s.tone)), sr);
    std::array<float, maxStrings> resG {};
    for (int j = 0; j < numRes; ++j)
    {
        auto& st = strings[(size_t) j];
        const double hz = 440.0 * std::pow (2.0, ((double) s.stringPitch[(size_t) j] - 69.0) / 12.0);
        // (minus the damping filter's own delay, so the string stays in tune)
        const double lpDelay = (1.0 - resDamp) / juce::jmax (0.01f, resDamp);
        st.targetDelay = (float) juce::jlimit (4.0, (double) st.buf.size() - 8.0, sr / juce::jlimit (20.0, 4000.0, hz) - lpDelay);
        if (st.delay <= 0.0f) st.delay = st.targetDelay;
        resG[(size_t) j] = std::pow (10.0f, -3.0f * st.targetDelay / (t60 * (float) sr));
    }
    std::array<float, numSteps> stepLevel {};
    for (int k = 0; k < numSteps; ++k) stepLevel[(size_t) k] = juce::jlimit (0.0f, 1.0f, s.steps[(size_t) k]);
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
        float fL = 0.0f, fR = 0.0f;
        const float inGain = 1.0f - freezeMix;
        if (mode == ModeResonator)
        {
            // tuned strings: short combs ringing at the chord's notes, the input plucks them
            const float excite = mono * inGain;
            float sL = 0.0f, sR = 0.0f;
            for (int j = 0; j < numRes; ++j)
            {
                auto& st = strings[(size_t) j];
                st.delay += (st.targetDelay - st.delay) * 0.002f;      // retune smoothly (a chord change slides)
                const float x = readAt (st.buf, st.w, st.delay);
                st.lp += resDamp * (x - st.lp);
                const float y = st.lp;
                float wv = excite * 0.6f + y * (resG[(size_t) j] + (0.9995f - resG[(size_t) j]) * freezeMix);
                wv = 1.5f * std::tanh (wv * (1.0f / 1.5f));
                if (! std::isfinite (wv)) { wv = 0.0f; st.lp = 0.0f; }
                st.buf[(size_t) st.w] = wv;
                st.w = (st.w + 1) % (int) st.buf.size();
                const float side = (j % 2 == 0 ? -1.0f : 1.0f) * pp * 0.8f;
                sL += y * (1.0f - side);
                sR += y * (1.0f + side);
            }
            const float norm = 0.9f / std::sqrt ((float) juce::jmax (1, numRes));
            fL = sL * norm;
            fR = sR * norm;
        }
        else
        {
        // the taps: dL/dR = what the echoes play, gL/gR = what goes round the loop again
        float dL, dR, gL, gR;
        if (mode == ModeReverse)
        {
            // two heads half a chunk apart, each reading the last echo-time backwards (its delay grows
            // from 0 to 2 echo-times while it plays), faded in and out so the turns don't click
            const float D = juce::jlimit (16.0f, (float) size * 0.5f - 16.0f, delaySmooth + mod);
            revPhase += 1.0 / (double) D;
            if (revPhase >= 1.0) revPhase -= 1.0;
            dL = dR = 0.0f;
            for (int h = 0; h < 2; ++h)
            {
                float ph = (float) revPhase + 0.5f * (float) h;
                if (ph >= 1.0f) ph -= 1.0f;
                const float win = std::sin (juce::MathConstants<float>::pi * ph);
                const float rd = 2.0f * ph * D + 4.0f;
                dL += readAt (lineL, w, rd) * win * win;
                dR += readAt (lineR, w, rd) * win * win;
            }
            gL = dL; gR = dR;
        }
        else if (mode == ModeSteps)
        {
            // 8 echoes in a row, each at its own level; the 8th goes round again (the pattern repeats)
            const float D = juce::jmax (16.0f, delaySmooth + mod);
            dL = dR = gL = gR = 0.0f;
            for (int t = 1; t <= numSteps; ++t)
            {
                const float td = D * (float) t;
                if (td > (float) size - 8.0f) break;
                const float tl = readAt (lineL, w, td), tr = readAt (lineR, w, td);
                const float lv = stepLevel[(size_t) t - 1];
                const float side = (t % 2 == 0) ? pp : 0.0f;          // ping-pong: every 2nd echo on the other side
                dL += lv * (tl + (tr - tl) * side);
                dR += lv * (tr + (tl - tr) * side);
                gL = tl; gR = tr;                                      // the last echo that fits
            }
        }
        else
        {
            const float d = juce::jlimit (4.0f, (float) size - 8.0f, delaySmooth + mod);
            dL = gL = readAt (lineL, w, d);
            dR = gR = readAt (lineR, w, d);
        }

        // each time round: darker, thinner, warmer (not while frozen: then the loop holds as it is)
        fL = dL; fR = dR;
        lpL += aLp * (fL - lpL); lpR += aLp * (fR - lpR);
        hpL += aHp * (lpL - hpL); hpR += aHp * (lpR - hpR);
        float cL = lpL - hpL, cR = lpR - hpR;
        if (s.drive > 0.002f) { cL = std::tanh (k * cL) * driveNorm; cR = std::tanh (k * cR) * driveNorm; }
        fL = cL + (dL - cL) * freezeMix;
        fR = cR + (dR - cR) * freezeMix;
        float bL = fL, bR = fR;                                       // the loop's signal
        if (mode == ModeSteps)
        {
            fbLpL += aLp * (gL - fbLpL); fbLpR += aLp * (gR - fbLpR);
            fbHpL += aHp * (fbLpL - fbHpL); fbHpR += aHp * (fbLpR - fbHpR);
            float qL = fbLpL - fbHpL, qR = fbLpR - fbHpR;
            if (s.drive > 0.002f) { qL = std::tanh (k * qL) * driveNorm; qR = std::tanh (k * qR) * driveNorm; }
            bL = qL + (gL - qL) * freezeMix;
            bR = qR + (gR - qR) * freezeMix;
        }

        const float loopFb = fb + (1.0f - fb) * freezeMix;
        const float ppLoop = mode == ModeSteps ? 0.0f : pp;            // (Steps: the ping-pong is in the taps)
        const float crossL = bL + (bR - bL) * ppLoop, crossR = bR + (bL - bR) * ppLoop;
        float wL = (inL + (mono - inL) * ppLoop) * inGain + crossL * loopFb;
        float wR = inR * (1.0f - ppLoop) * inGain + crossR * loopFb;
        if (pitched)
        {
            // every echo higher / lower: what goes into the line is shifted, so each time round once more
            // (not while frozen: a frozen loop stays as it is)
            const float sL = loopShifters[0].process (wL), sR = loopShifters[1].process (wR);
            wL = sL + (wL - sL) * freezeMix;
            wR = sR + (wR - sR) * freezeMix;
        }
        if (! std::isfinite (wL) || ! std::isfinite (wR))
        {
            wL = wR = 0.0f; lpL = lpR = hpL = hpR = 0.0f; fbLpL = fbLpR = fbHpL = fbHpR = 0.0f;
            for (auto& ls : loopShifters) ls.reset();
        }
        // never let the loop run away (a soft ceiling far above normal levels)
        wL = 2.0f * std::tanh (0.5f * wL);
        wR = 2.0f * std::tanh (0.5f * wR);
        lineL[(size_t) w] = wL;
        lineR[(size_t) w] = wR;
        w = (w + 1) % size;
        }

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
        // (the octave-up tail goes round the room again: its loop gain must stay well below 1, and a soft
        // ceiling keeps it from ever running away into a wall of noise)
        const float shimGain = juce::jlimit (0.0f, 1.0f, s.shimmer) * 0.22f;
        const float aShim = onePole (6000.0, sr);
        for (int i = 0; i < n; ++i)
        {
            const float wl = revL[(size_t) i] * 0.75f, wr = revR[(size_t) i] * 0.75f;
            l[i] += wl;
            r[i] += wr;
            spacePeak = juce::jmax (spacePeak, std::abs (wl), std::abs (wr));
            // shimmer: the tail an octave up, fed back into the space next block
            shimLp += aShim * (shifters[0].process (revL[(size_t) i]) - shimLp);
            shimLpR += aShim * (shifters[1].process (revR[(size_t) i]) - shimLpR);
            shimL[(size_t) i] = std::isfinite (shimLp) ? 0.25f * std::tanh (4.0f * shimLp * shimGain) : 0.0f;
            shimR[(size_t) i] = std::isfinite (shimLpR) ? 0.25f * std::tanh (4.0f * shimLpR * shimGain) : 0.0f;
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

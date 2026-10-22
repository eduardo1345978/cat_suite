#include "MasterFx.h"
#include <cmath>

namespace catbox
{
const char* perfName (int pad)
{
    static const char* names[NumPerf] = { "Repeat 1/4", "Repeat 1/8", "Repeat 1/16", "Repeat 1/32",
                                          "Sweep down", "Sweep up", "Tape stop", "Reverse",
                                          "Kill drums", "Kill arp", "Kill bass", "Crush",
                                          "Wash", "Riser", "Repeat 1/2", "Echo throw" };
    return pad >= 0 && pad < NumPerf ? names[pad] : "";
}

namespace
{
constexpr float pi = 3.14159265358979f;
inline bool held (uint32_t bits, int p) noexcept { return ((bits >> p) & 1u) != 0; }
}

void MasterFx::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    reverb.setSampleRate (sr);
    reverb.reset();
    echo.prepare (sr);
    const int block = juce::jmax (maxBlock, 2048);
    revL.assign ((size_t) block, 0.0f);
    revR.assign ((size_t) block, 0.0f);
    capSize = (int) (sr * 6.0);   // enough for one bar at 40 BPM... and a 1/2 repeat at the slowest tempo
    capL.assign ((size_t) capSize, 0.0f);
    capR.assign ((size_t) capSize, 0.0f);
    capWrite = 0;
    lastHeld = 0;
    repeatPad = -1;
    perfMix = 0.0f;
}

float MasterFx::readCapture (const std::vector<float>& b, double pos) const noexcept
{
    double p = std::fmod (pos, (double) capSize);
    if (p < 0.0) p += capSize;
    const int i0 = (int) p;
    const float fr = (float) (p - i0);
    return b[(size_t) i0] * (1.0f - fr) + b[(size_t) ((i0 + 1) % capSize)] * fr;
}

void MasterFx::process (float* const* inL, float* const* inR, int n, float* outL, float* outR,
                        const MasterSettings& s, uint32_t perfHeld, double songPpq, double ppqPerSample,
                        const int* kickOffsets, int numKicks)
{
    if ((int) revL.size() < n) return;   // never happens: prepare() sized it
    const float srf = (float) sr;
    const double beatSamples = 1.0 / juce::jmax (1.0e-9, ppqPerSample);
    const float smooth = 1.0f - std::exp (-1.0f / (0.01f * srf));    // 10 ms
    const float slow = 1.0f - std::exp (-1.0f / (0.25f * srf));     // 250 ms
    const float pumpDecay = std::exp (-1.0f / (0.22f * (float) beatSamples));

    const uint32_t pressed = perfHeld & ~lastHeld;
    lastHeld = perfHeld;

    // ---- performance pads that start something at the moment they are pressed
    static const int repeatPads[] = { PerfRepeat32, PerfRepeat16, PerfRepeat8, PerfRepeat4, PerfRepeat2 };
    static const double repeatPpq[] = { 0.125, 0.25, 0.5, 1.0, 2.0 };
    int wantRepeat = -1;
    double wantPpq = 1.0;
    for (int i = 0; i < 5; ++i)
        if (held (perfHeld, repeatPads[i])) { wantRepeat = repeatPads[i]; wantPpq = repeatPpq[i]; break; }   // the shortest held wins
    if (wantRepeat != repeatPad)
    {
        repeatPad = wantRepeat;
        if (repeatPad >= 0)
        {
            repeatLen = juce::jlimit (16.0, (double) capSize - 16.0, wantPpq * beatSamples);
            const double sinceGrid = std::fmod (juce::jmax (0.0, songPpq), wantPpq) * beatSamples;
            repeatPhase = juce::jmin (sinceGrid, repeatLen - 1.0);
            repeatStart = capWrite - repeatPhase;
        }
    }
    if (held (pressed, PerfReverse))
    {
        reverseStart = capWrite;
        reversePhase = 0.0;
        reverseLen = juce::jlimit (64.0, (double) capSize - 16.0, beatSamples);
    }
    if (held (pressed, PerfTapeStop)) { tapePos = capWrite; tapeRate = 1.0; }

    // ---- reverb: room size and wash
    const float washTarget = held (perfHeld, PerfWash) ? 1.0f : 0.0f;
    juce::Reverb::Parameters rp;
    rp.roomSize = juce::jlimit (0.0f, 1.0f, s.size + (0.98f - s.size) * washAmt);
    rp.damping = 0.45f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    reverb.setParameters (rp);

    const std::array<bool, 3> killHeld { held (perfHeld, PerfKillDrums), held (perfHeld, PerfKillArp), held (perfHeld, PerfKillBass) };
    int k = 0;   // next kick offset

    for (int i = 0; i < n; ++i)
    {
        while (k < numKicks && kickOffsets[k] <= i) { pumpEnv = 1.0f; ++k; }
        pumpEnv *= pumpDecay;
        washAmt += (washTarget - washAmt) * slow;
        const float duck = 1.0f - 0.85f * s.pump * pumpEnv;

        float l = 0.0f, r = 0.0f, sl = 0.0f, sr2 = 0.0f;
        for (int t = 0; t < 3; ++t)
        {
            kill[(size_t) t] += ((killHeld[(size_t) t] ? 0.0f : 1.0f) - kill[(size_t) t]) * smooth;
            const bool pumped = s.pumpTarget == 3 || (t == 1 && (s.pumpTarget == 0 || s.pumpTarget == 2))
                                                  || (t == 2 && (s.pumpTarget == 0 || s.pumpTarget == 1));
            const float g = kill[(size_t) t] * (pumped ? duck : 1.0f);
            const float xl = inL[t][i] * g, xr = inR[t][i] * g;
            l += xl; r += xr;
            const float send = s.sends[(size_t) t] + (1.0f - s.sends[(size_t) t]) * washAmt;
            sl += xl * send; sr2 += xr * send;
        }
        outL[i] = l;
        outR[i] = r;
        revL[(size_t) i] = sl;
        revR[(size_t) i] = sr2;
    }

    reverb.processStereo (revL.data(), revR.data(), n);
    const float revGain = s.reverb * 0.9f + washAmt * 0.6f;
    for (int i = 0; i < n; ++i)   // the reverb tail is not pumped
    {
        outL[i] += revL[(size_t) i] * revGain;
        outR[i] += revR[(size_t) i] * revGain;
    }

    // ---- master echo (and the echo throw)
    hub::TrackFxSettings es;
    es.delay = held (perfHeld, PerfDelayThrow) ? 0.95f : s.delay;
    es.bpm = s.bpm;
    echo.process (outL, outR, n, es);

    // ---- glue, tape, vinyl, crush, DJ filter + sweeps, width
    const float glueThr = juce::Decibels::decibelsToGain (-4.0f - 20.0f * s.glue);
    const float glueAtt = 1.0f - std::exp (-1.0f / (0.005f * srf)), glueRel = 1.0f - std::exp (-1.0f / (0.15f * srf));
    const float makeup = 1.0f + 0.9f * s.glue;
    const float tapeK = 1.0f + 3.0f * s.tape;
    const float tapeA = 1.0f - std::exp (-2.0f * pi * (16000.0f - 9000.0f * s.tape) / srf);
    const float vinylA = 1.0f - std::exp (-2.0f * pi * (18000.0f - 12000.0f * s.vinyl) / srf);
    const float crushAmt = held (perfHeld, PerfCrush) ? juce::jmax (s.crush, 0.85f) : s.crush;
    const float levels = std::exp2 (16.0f - 12.0f * crushAmt);
    const int hold = 1 + (int) (crushAmt * crushAmt * 14.0f);

    // sweeps grow while held (2 beats), snap back fast when let go
    const float sweepStep = (float) (n / (2.0 * beatSamples));
    lowSweep = held (perfHeld, PerfLowSweep) ? juce::jmin (1.0f, lowSweep + sweepStep) : juce::jmax (0.0f, lowSweep - sweepStep * 6.0f);
    highSweep = held (perfHeld, PerfHighSweep) ? juce::jmin (1.0f, highSweep + sweepStep) : juce::jmax (0.0f, highSweep - sweepStep * 6.0f);
    const float dj = s.djFilter;
    const float djLp = dj < 0.48f ? 20000.0f * std::pow (150.0f / 20000.0f, (0.48f - dj) / 0.48f) : 20000.0f;
    const float djHp = dj > 0.52f ? 20.0f * std::pow (3000.0f / 20.0f, (dj - 0.52f) / 0.48f) : 20.0f;
    const float lpTarget = juce::jmin (djLp, 20000.0f * std::pow (200.0f / 20000.0f, lowSweep));
    const float hpTarget = juce::jmax (djHp, 20.0f * std::pow (4000.0f / 20.0f, highSweep));
    const float filterK = 1.0f / 0.9f;   // a little resonance

    for (int i = 0; i < n; ++i)
    {
        float l = outL[i], r = outR[i];

        if (s.glue > 0.002f)
        {
            const float det = juce::jmax (std::abs (l), std::abs (r));
            glueEnv += (det - glueEnv) * (det > glueEnv ? glueAtt : glueRel);
            const float target = glueEnv > glueThr ? std::pow (glueThr / glueEnv, 0.66f) : 1.0f;
            glueGain += (target - glueGain) * glueAtt;
            l *= glueGain * makeup;
            r *= glueGain * makeup;
        }
        if (s.tape > 0.002f)
        {
            l = std::tanh (l * tapeK) / tapeK * (1.0f + 0.6f * s.tape);
            r = std::tanh (r * tapeK) / tapeK * (1.0f + 0.6f * s.tape);
            tapeLpL += tapeA * (l - tapeLpL);
            tapeLpR += tapeA * (r - tapeLpR);
            l = tapeLpL; r = tapeLpR;
        }
        if (s.vinyl > 0.002f)
        {
            if (noise.next() > 1.0f - 0.0009f * s.vinyl) crackle = noise.next() * 0.35f * s.vinyl;
            crackle *= 0.86f;
            const float hiss = noise.next() * 0.006f * s.vinyl;
            vinylLpL += vinylA * (l - vinylLpL);
            vinylLpR += vinylA * (r - vinylLpR);
            l = vinylLpL + crackle + hiss;
            r = vinylLpR + crackle * 0.8f + hiss;
        }
        if (crushAmt > 0.002f)
        {
            if (--holdCount <= 0)
            {
                holdCount = hold;
                holdL = std::round (l * levels) / levels;
                holdR = std::round (r * levels) / levels;
            }
            l = holdL; r = holdR;
        }

        // filter cutoffs glide (coefficients every 16 samples)
        if ((i & 15) == 0)
        {
            lpCut += (lpTarget - lpCut) * 0.08f;
            hpCut += (hpTarget - hpCut) * 0.08f;
        }
        const bool lpOn = lpCut < 19000.0f, hpOn = hpCut > 25.0f;
        if (lpOn)
        {
            const float g = std::tan (pi * juce::jmin (lpCut, 0.45f * srf) / srf);
            l = lpL.process (l, g, filterK, 0);
            r = lpR.process (r, g, filterK, 0);
        }
        if (hpOn)
        {
            const float g = std::tan (pi * juce::jmin (hpCut, 0.45f * srf) / srf);
            l = hpL.process (l, g, filterK, 1);
            r = hpR.process (r, g, filterK, 1);
        }

        // width (0.5 = as it is)
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * (2.0f * s.width);
        l = mid + side;
        r = mid - side;

        // ---- performance: capture the sound, then repeat / reverse / tape stop it
        capL[(size_t) capWrite] = l;
        capR[(size_t) capWrite] = r;

        // which capture player sounds: the held one, or the last one while it fades out
        const int mode = repeatPad >= 0 ? 1 : held (perfHeld, PerfReverse) ? 2 : held (perfHeld, PerfTapeStop) ? 3 : 0;
        if (mode != 0) lastMode = mode;
        const int play = mode != 0 ? mode : (perfMix > 0.001f ? lastMode : 0);
        float pl = 0.0f, pr = 0.0f;
        if (play == 1)
        {
            const double ph = std::fmod (repeatPhase, repeatLen);
            const float edge = (float) juce::jmin (1.0, juce::jmin (ph, repeatLen - ph) / 96.0);   // tiny fades at the loop seam
            pl = readCapture (capL, repeatStart + ph) * edge;
            pr = readCapture (capR, repeatStart + ph) * edge;
            repeatPhase += 1.0;
        }
        else if (play == 2)
        {
            const double ph = std::fmod (reversePhase, reverseLen);
            const float edge = (float) juce::jmin (1.0, juce::jmin (ph, reverseLen - ph) / 96.0);
            pl = readCapture (capL, reverseStart - ph) * edge;
            pr = readCapture (capR, reverseStart - ph) * edge;
            reversePhase += 1.0;
        }
        else if (play == 3)
        {
            if (mode == 3) tapeRate = juce::jmax (0.0, tapeRate - 1.0 / (1.2 * beatSamples));
            tapePos += tapeRate;
            const float g = (float) juce::jmin (1.0, tapeRate * 4.0);
            pl = readCapture (capL, tapePos) * g;
            pr = readCapture (capR, tapePos) * g;
        }
        capWrite = (capWrite + 1) % capSize;

        perfMix += ((mode != 0 ? 1.0f : 0.0f) - perfMix) * juce::jmin (1.0f, smooth * 4.0f);
        l = l * (1.0f - perfMix) + pl * perfMix;
        r = r * (1.0f - perfMix) + pr * perfMix;

        // riser: filtered noise climbing for 4 beats
        if (held (perfHeld, PerfRiser)) riserT = juce::jmin (1.0f, riserT + (float) (1.0 / (4.0 * beatSamples)));
        else riserT = juce::jmax (0.0f, riserT - (float) (1.0 / (0.25 * beatSamples)));
        if (riserT > 0.0f)
        {
            if ((i & 15) == 0) riserLevel = riserT * riserT * 0.3f;
            const float g = std::tan (pi * juce::jmin (300.0f * std::exp2 (riserT * 5.3f), 0.45f * srf) / srf);
            const float z = riserF.process (noise.next(), g, 1.0f / 3.0f, 2) * riserLevel;
            l += z; r += z;
        }

        outL[i] = l;
        outR[i] = r;
    }
}

} // namespace catbox

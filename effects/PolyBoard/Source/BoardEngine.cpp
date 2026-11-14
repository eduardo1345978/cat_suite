#include "BoardEngine.h"

namespace board
{
namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;
float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
float onePole (float hz, double sr) noexcept { return 1.0f - std::exp (-twoPi * hz / (float) sr); }

// the delay's note values, in beats
struct Note { double beats; const char* name; };
const Note notes[] = { { 0.25, "1/16" }, { 0.375, "1/16." }, { 0.5, "1/8" }, { 0.75, "1/8." }, { 1.0, "1/4" }, { 1.5, "1/4." }, { 2.0, "1/2" } };
// the filter LFO: beats per cycle
struct Speed { double beats; const char* name; };
const Speed speeds[] = { { 16.0, "4 bars" }, { 8.0, "2 bars" }, { 4.0, "1 bar" }, { 2.0, "1/2" }, { 1.0, "1/4" }, { 0.5, "1/8" }, { 0.25, "1/16" } };
}

const char* pedalId (int p)   { static const char* n[] = { "drive", "comp", "filter", "phaser", "chorus", "delay", "reverb" }; return n[juce::jlimit (0, 6, p)]; }
const char* pedalName (int p) { static const char* n[] = { "Drive", "Compressor", "Filter", "Phaser", "Chorus", "Delay", "Reverb" }; return n[juce::jlimit (0, 6, p)]; }
const char* pedalKind (int p) { static const char* n[] = { "overdrive", "sustain", "auto-wah", "4-stage", "ensemble", "tape", "hall" }; return n[juce::jlimit (0, 6, p)]; }

double delaySeconds (float v, double bpm)
{
    const int i = juce::jlimit (0, 6, juce::roundToInt (v * 6.0f));
    return notes[i].beats * 60.0 / juce::jlimit (30.0, 300.0, bpm);
}
juce::String delayName (float v) { return notes[juce::jlimit (0, 6, juce::roundToInt (v * 6.0f))].name; }
double filterBeats (float v) { return speeds[juce::jlimit (0, 6, juce::roundToInt (v * 6.0f))].beats; }
juce::String filterSpeedName (float v) { return speeds[juce::jlimit (0, 6, juce::roundToInt (v * 6.0f))].name; }

float Engine::Svf::run (float x, float g, float k, float& band) noexcept
{
    // the "TPT" state-variable filter: stable even when the cutoff moves fast
    const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const float v3 = x - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    band = v1;
    return v2;   // low-pass
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    chL.assign ((size_t) (sr * 0.05) + 4, 0.0f);
    chR.assign (chL.size(), 0.0f);
    dlL.assign ((size_t) (sr * 2.5) + 4, 0.0f);
    dlR.assign (dlL.size(), 0.0f);
    wetL.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
    wetR.assign (wetL.size(), 0.0f);
    room.setSampleRate (sr);
    driveGain.reset (sr, 0.03); driveTone.reset (sr, 0.03); dlTime.reset (sr, 0.25);
    reset();
}

void Engine::reset()
{
    toneL = toneR = 0.0f; env = 0.0f; gr = 1.0f;
    fL = fR = Svf(); lfoPhase = 0.0; filterCut = 0.0f;
    apL.fill (0.0f); apR.fill (0.0f); phFbL = phFbR = 0.0f; phPhase = 0.0;
    std::fill (chL.begin(), chL.end(), 0.0f); std::fill (chR.begin(), chR.end(), 0.0f); chPos = 0; chPhase = 0.0;
    std::fill (dlL.begin(), dlL.end(), 0.0f); std::fill (dlR.begin(), dlR.end(), 0.0f); dlPos = 0; dlLpL = dlLpR = 0.0f;
    room.reset();
    for (auto& w : meters.work) w = 0.0f;
}

void Engine::process (float* l, float* r, int n, const Settings& s)
{
    const float in = dbToGain (s.input);
    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i) { l[i] *= in; r[i] *= in; peakIn = juce::jmax (peakIn, std::abs (l[i]), std::abs (r[i])); }
    meters.inLevel = peakIn;

    for (int slot = 0; slot < numPedals; ++slot)
    {
        const int p = s.order[(size_t) slot];
        if (p < 0 || p >= numPedals) continue;
        if (! s.on[(size_t) p]) { meters.work[(size_t) p] = meters.work[(size_t) p] * 0.8f; continue; }
        const float a = s.a[(size_t) p], b = s.b[(size_t) p];
        switch (p)
        {
            case Drive:  drive (l, r, n, a, b); break;
            case Comp:   comp (l, r, n, a, b); break;
            case Filter: filter (l, r, n, a, b, s.bpm); break;
            case Phaser: phaser (l, r, n, a, b); break;
            case Chorus: chorus (l, r, n, a, b); break;
            case Delay:  delay (l, r, n, a, b, s.bpm); break;
            case Reverb: reverb (l, r, n, a, b); break;
            default: break;
        }
    }

    const float out = dbToGain (s.output);
    float peakOut = 0.0f;
    for (int i = 0; i < n; ++i) { l[i] *= out; r[i] *= out; peakOut = juce::jmax (peakOut, std::abs (l[i]), std::abs (r[i])); }
    meters.outLevel = peakOut;
}

// DRIVE: gain 0..1 = +0 .. +36 dB into tanh; tone = a low-pass 1.5 .. 14 kHz after it; the level is evened out
void Engine::drive (float* l, float* r, int n, float gain, float tone)
{
    driveGain.setTargetValue (dbToGain (36.0f * gain));
    driveTone.setTargetValue (1500.0f * std::pow (14000.0f / 1500.0f, tone));
    float work = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = driveGain.getNextValue();
        const float c = onePole (driveTone.getNextValue(), sr);
        const float makeup = 1.0f / std::sqrt (g);            // louder in = squarer, not much louder out
        // a little offset makes it asymmetric = even overtones, like a valve
        float x = std::tanh (l[i] * g + 0.08f) - std::tanh (0.08f);
        float y = std::tanh (r[i] * g + 0.08f) - std::tanh (0.08f);
        toneL += (x - toneL) * c; toneR += (y - toneR) * c;
        work = juce::jmax (work, std::abs (l[i] * g));
        l[i] = toneL * makeup * 1.2f;
        r[i] = toneR * makeup * 1.2f;
    }
    meters.work[Drive] = juce::jlimit (0.0f, 1.0f, (work - 0.5f) / 2.5f);
}

// COMPRESSOR: squash 0..1 = threshold -6 .. -40 dB at 4:1; level = make-up gain 0 .. +18 dB
void Engine::comp (float* l, float* r, int n, float squash, float level)
{
    const float thresh = dbToGain (-6.0f - 34.0f * squash);
    const float att = 1.0f - std::exp (-1.0f / (0.008f * (float) sr)), rel = 1.0f - std::exp (-1.0f / (0.15f * (float) sr));
    const float makeup = dbToGain (18.0f * level) * dbToGain (squash * 6.0f);
    float most = 1.0f;
    for (int i = 0; i < n; ++i)
    {
        const float x = juce::jmax (std::abs (l[i]), std::abs (r[i]));
        env += (x - env) * (x > env ? att : rel);
        float target = 1.0f;
        if (env > thresh) target = std::pow (env / thresh, 1.0f / 4.0f - 1.0f);   // 4:1 above the threshold
        gr += (target - gr) * 0.05f;
        most = juce::jmin (most, gr);
        l[i] *= gr * makeup;
        r[i] *= gr * makeup;
    }
    meters.gainReduction = most;
    meters.work[Comp] = juce::jlimit (0.0f, 1.0f, 1.0f - most);
}

// FILTER: sweep = how far the cutoff moves (200 Hz up to 200 Hz .. 6 kHz), speed = the LFO in time with the song
void Engine::filter (float* l, float* r, int n, float sweep, float speed, double bpm)
{
    const double cyclesPerSecond = (bpm / 60.0) / filterBeats (speed);
    const float k = 0.35f;     // resonance: low k = a sharp peak (the "wah")
    for (int i = 0; i < n; ++i)
    {
        lfoPhase += cyclesPerSecond / sr;
        if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        const float lfo = 0.5f - 0.5f * std::cos (twoPi * (float) lfoPhase);       // 0..1, smooth
        const float hz = 250.0f * std::pow (1.0f + 24.0f * sweep, lfo);
        const float g = std::tan (juce::MathConstants<float>::pi * juce::jmin (hz, (float) sr * 0.45f) / (float) sr);
        float bL, bR;
        const float lpL = fL.run (l[i], g, k, bL), lpR = fR.run (r[i], g, k, bR);
        l[i] = lpL * 0.7f + bL * 0.6f;
        r[i] = lpR * 0.7f + bR * 0.6f;
        filterCut = lfo;
    }
    meters.lfo = filterCut;
    meters.work[Filter] = filterCut;
}

// PHASER: 4 all-pass stages whose corner moves (rate 0.05 .. 4 Hz); depth = how far + how much feedback
void Engine::phaser (float* l, float* r, int n, float rate, float depth)
{
    const double hz = 0.05 * std::pow (80.0, (double) rate);
    float lastLfo = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        phPhase += hz / sr;
        if (phPhase >= 1.0) phPhase -= 1.0;
        const float lfo = 0.5f + 0.5f * std::sin (twoPi * (float) phPhase);
        const float lfoR = 0.5f + 0.5f * std::sin (twoPi * (float) phPhase + 1.2f);        // the right side a bit later = stereo
        auto stages = [&] (float x, std::array<float, 4>& st, float& fb, float m)
        {
            const float corner = 300.0f * std::pow (12.0f, m * depth + (1.0f - depth) * 0.3f);
            const float t = std::tan (juce::MathConstants<float>::pi * corner / (float) sr);
            const float c = (t - 1.0f) / (t + 1.0f);
            float y = x + fb * 0.6f * depth;
            for (auto& z : st) { const float o = c * y + z; z = y - c * o; y = o; }        // first-order all-pass
            fb = y;
            return 0.5f * (x + y);           // dry + shifted: the notches
        };
        l[i] = stages (l[i], apL, phFbL, lfo);
        r[i] = stages (r[i], apR, phFbR, lfoR);
        lastLfo = lfo;
    }
    meters.work[Phaser] = lastLfo;
}

// CHORUS: two wobbling delays (7..25 ms), opposite on left and right; mix = how much of them
void Engine::chorus (float* l, float* r, int n, float rate, float mix)
{
    const double hz = 0.1 + 2.4 * (double) rate;
    const int len = (int) chL.size();
    float lfoNow = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        chL[(size_t) chPos] = l[i];
        chR[(size_t) chPos] = r[i];
        chPhase += hz / sr;
        if (chPhase >= 1.0) chPhase -= 1.0;
        const float s1 = std::sin (twoPi * (float) chPhase), s2 = std::sin (twoPi * (float) chPhase + 2.1f);
        auto tap = [&] (const std::vector<float>& buf, float ms)
        {
            const float d = ms * 0.001f * (float) sr;
            float pos = (float) chPos - d;
            while (pos < 0.0f) pos += (float) len;
            const int i0 = (int) pos;
            const float f = pos - (float) i0;
            return buf[(size_t) i0] * (1.0f - f) + buf[(size_t) ((i0 + 1) % len)] * f;
        };
        const float wl = tap (chL, 14.0f + 7.0f * s1), wr = tap (chR, 14.0f + 7.0f * s2);
        l[i] = l[i] * (1.0f - 0.5f * mix) + wl * mix * 0.8f;
        r[i] = r[i] * (1.0f - 0.5f * mix) + wr * mix * 0.8f;
        chPos = (chPos + 1) % len;
        lfoNow = 0.5f + 0.5f * s1;
    }
    meters.work[Chorus] = lfoNow * mix;
}

// DELAY: time = a note value in time with the song, repeats = feedback (0 .. 0.9); every repeat darker, a little wow
void Engine::delay (float* l, float* r, int n, float time, float repeats, double bpm)
{
    const int len = (int) dlL.size();
    dlTime.setTargetValue ((float) (delaySeconds (time, bpm) * sr));
    const float fb = 0.9f * repeats;
    const float c = onePole (3500.0f, sr);
    float level = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        wow += 0.6 / sr;
        if (wow >= 1.0) wow -= 1.0;
        const float d = juce::jlimit (16.0f, (float) len - 4.0f, dlTime.getNextValue() * (1.0f + 0.002f * std::sin (twoPi * (float) wow)));
        float pos = (float) dlPos - d;
        while (pos < 0.0f) pos += (float) len;
        const int i0 = (int) pos;
        const float f = pos - (float) i0;
        const float el = dlL[(size_t) i0] * (1.0f - f) + dlL[(size_t) ((i0 + 1) % len)] * f;
        const float er = dlR[(size_t) i0] * (1.0f - f) + dlR[(size_t) ((i0 + 1) % len)] * f;
        dlLpL += (el - dlLpL) * c;
        dlLpR += (er - dlLpR) * c;
        // ping-pong: the left echo comes back on the right and the other way round
        dlL[(size_t) dlPos] = std::tanh (l[i] + dlLpR * fb);
        dlR[(size_t) dlPos] = std::tanh (r[i] + dlLpL * fb);
        dlPos = (dlPos + 1) % len;
        l[i] += dlLpL * 0.55f;
        r[i] += dlLpR * 0.55f;
        level = juce::jmax (level, std::abs (dlLpL));
    }
    meters.work[Delay] = juce::jlimit (0.0f, 1.0f, level * 2.0f);
}

// REVERB: size = how big the room (and how long it rings), mix = how much of it
void Engine::reverb (float* l, float* r, int n, float size, float mix)
{
    juce::Reverb::Parameters p;
    p.roomSize = 0.4f + 0.58f * size;
    p.damping = 0.55f - 0.3f * size;
    p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.width = 1.0f;
    room.setParameters (p);
    float level = 0.0f;
    for (int start = 0; start < n; start += (int) wetL.size())
    {
        const int m = juce::jmin ((int) wetL.size(), n - start);
        std::copy (l + start, l + start + m, wetL.data());
        std::copy (r + start, r + start + m, wetR.data());
        room.processStereo (wetL.data(), wetR.data(), m);
        for (int i = 0; i < m; ++i)
        {
            l[start + i] = l[start + i] * (1.0f - 0.5f * mix) + wetL[(size_t) i] * mix * 0.7f;
            r[start + i] = r[start + i] * (1.0f - 0.5f * mix) + wetR[(size_t) i] * mix * 0.7f;
            level = juce::jmax (level, std::abs (wetL[(size_t) i]));
        }
    }
    meters.work[Reverb] = juce::jlimit (0.0f, 1.0f, level * 2.0f * mix);
}
} // namespace board

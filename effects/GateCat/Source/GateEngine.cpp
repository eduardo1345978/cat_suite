#include "GateEngine.h"

namespace gate
{
juce::StringArray rateNames() { return { "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T" }; }

double stepsPerBeat (int rate) noexcept
{
    // a beat is a quarter note: 1/16 = 4 steps per beat, 1/8 triplets = 3 per beat
    const double s[] = { 1.0, 2.0, 4.0, 8.0, 3.0, 6.0 };
    return s[juce::jlimit (0, 5, rate)];
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    mixS.reset (sampleRate, 0.05);
    outS.reset (sampleRate, 0.05);
    reset();
}

void Engine::reset()
{
    ownBeats = 0.0; wasPlaying = false; env = 1.0f;
    for (auto& v : ic1) v = 0.0f;
    for (auto& v : ic2) v = 0.0f;
}

std::pair<int, double> Engine::stepAt (double beats, const Settings& s) noexcept
{
    const double pos = juce::jmax (0.0, beats) * stepsPerBeat (s.rate);
    const int steps = juce::jlimit (1, maxSteps, s.steps);
    // swing works on pairs of steps: the second one starts later (at 0.5 .. 0.667 of the pair)
    const double pair = pos * 0.5;
    const double inPair = pair - std::floor (pair);
    const double split = 0.5 + (double) juce::jlimit (0.0f, 1.0f, s.swing) / 6.0;
    int k = (int) std::floor (pair) * 2;
    double frac;
    if (inPair < split) frac = inPair / split;
    else { ++k; frac = (inPair - split) / (1.0 - split); }
    return { k % steps, frac };
}

void Engine::process (juce::AudioBuffer<float>& buffer, const Settings& s)
{
    const int n = buffer.getNumSamples(), chans = juce::jmin (2, buffer.getNumChannels());
    if (n == 0 || chans == 0) return;

    const double beatsPerSample = juce::jlimit (20.0, 400.0, s.bpm) / 60.0 / sr;
    if (s.playing && ! wasPlaying) env = 0.0f;              // the DAW starts: the first step opens cleanly
    if (! s.playing && wasPlaying) ownBeats = 0.0;
    wasPlaying = s.playing;
    double beats = s.playing ? s.ppq : ownBeats;

    const float atk = 0.0005f + std::pow (juce::jlimit (0.0f, 1.0f, s.attack), 2.0f) * 0.06f;
    const float rel = 0.002f + std::pow (juce::jlimit (0.0f, 1.0f, s.release), 2.0f) * 0.25f;
    const float up = 1.0f - std::exp (-1.0f / (atk * (float) sr)), down = 1.0f - std::exp (-1.0f / (rel * (float) sr));
    const float length = juce::jlimit (0.05f, 1.0f, s.length);
    mixS.setTargetValue (s.mix);
    outS.setTargetValue (juce::Decibels::decibelsToGain (s.outDb));
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (chans - 1);
    int lastStep = 0;

    for (int i = 0; i < n; ++i)
    {
        const auto [k, frac] = stepAt (beats, s);
        lastStep = k;
        const float target = frac < length ? juce::jlimit (0.0f, 1.0f, s.level[(size_t) k]) : 0.0f;
        env += (target - env) * (target > env ? up : down);
        beats += beatsPerSample;

        const float g = 1.0f - s.depth + s.depth * env;    // DEPTH: how deep the holes are
        const float m = mixS.getNextValue(), o = outS.getNextValue();
        float x[2] = { l[i], r[i] };
        float y[2] = { x[0] * g, x[1] * g };
        if (s.filter > 0.001f)
        {
            // the low-pass opens with the gate: 20 kHz when open, down to 150 Hz when shut (by FILTER)
            const float open = 1.0f - s.filter * (1.0f - env);
            const double hz = 150.0 * std::pow (20000.0 / 150.0, (double) open);
            const float gg = (float) std::tan (juce::MathConstants<double>::pi * juce::jmin (hz, sr * 0.45) / sr);
            const float a1 = 1.0f / (1.0f + gg * (gg + 1.2f)), a2 = gg * a1, a3 = gg * a2;
            for (int c = 0; c < 2; ++c)
            {
                const float v3 = y[c] - ic2[c];
                const float v1 = a1 * ic1[c] + a2 * v3, v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
                ic1[c] = 2.0f * v1 - ic1[c]; ic2[c] = 2.0f * v2 - ic2[c];
                y[c] = v2;
            }
        }
        l[i] = (x[0] + (y[0] - x[0]) * m) * o;
        if (chans > 1) r[i] = (x[1] + (y[1] - x[1]) * m) * o;
    }
    if (! s.playing) ownBeats = beats;
    stepNow = lastStep;
    gainNow = env;
}
} // namespace gate

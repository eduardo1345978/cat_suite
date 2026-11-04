#include "PhaseEngine.h"

namespace phasecat
{
juce::StringArray rateNames() { return { "1/4", "1/8", "1/8 triplet", "1/16", "1/16 triplet", "1/32", "1/4 triplet", "1/2" }; }

double rateBeats (int index)
{
    static const double b[numRates] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 2.0 / 3.0, 2.0 };
    return b[juce::jlimit (0, numRates - 1, index)];
}

juce::StringArray modulationNames() { return { "x 3/2  (triplets become the beat)", "x 2/3  (back down)", "x 4/3", "x 3/4", "x 5/4", "x 4/5" }; }

double modulationFactor (int index)
{
    static const double f[numModulations] = { 1.5, 2.0 / 3.0, 4.0 / 3.0, 0.75, 1.25, 0.8 };
    return f[juce::jlimit (0, numModulations - 1, index)];
}

void Engine::reset() noexcept
{
    for (auto& l : phase) l.fill (0.0);
    rissetX = 0.0;
    lastEndPpq = -1.0;
    wasPlaying = false;
    curRatio = 1.0;
    ratio = 1.0;
    pendingAtBar = -1;
    realignAtBar = false;
    for (auto& o : offs) o = Pending();
    for (auto& p : position) p = 0.0f;
    for (auto& f : flash) f = 0.0f;
}

void Engine::realign (const Settings& s, double ppq) noexcept
{
    // every lane back in step with Cubase's beat (drift starts again from nothing)
    for (int l = 0; l < numLanes; ++l)
    {
        const auto& lane = s.lanes[(size_t) l];
        const double base = ppq * curRatio / rateBeats (lane.rate);
        for (int k = 0; k < numLayers; ++k)
            phase[(size_t) l][(size_t) k] = lane.risset ? base * std::pow (2.0, rissetX + k - 1) : base;
    }
}

void Engine::noteOn (juce::MidiBuffer& out, int pos, int channel, int note, float velocity, int gateSamples) noexcept
{
    // the same note still sounding: end it first
    for (auto& o : offs)
        if (o.note == note && o.channel == channel)
        {
            out.addEvent (juce::MidiMessage::noteOff (channel, note), pos);
            o.note = -1;
        }
    out.addEvent (juce::MidiMessage::noteOn (channel, note, juce::jlimit (0.02f, 1.0f, velocity)), pos);
    for (auto& o : offs)
        if (o.note < 0) { o.note = note; o.channel = channel; o.samplesLeft = pos + juce::jmax (8, gateSamples); return; }
    out.addEvent (juce::MidiMessage::noteOff (channel, note), juce::jmin (pos + 8, pos + gateSamples));   // (table full)
}

void Engine::flushOffs (juce::MidiBuffer& out, int numSamples) noexcept
{
    for (auto& o : offs)
    {
        if (o.note < 0) continue;
        if (o.samplesLeft < numSamples)
        {
            out.addEvent (juce::MidiMessage::noteOff (o.channel, o.note), juce::jmax (0, o.samplesLeft));
            o.note = -1;
        }
        else o.samplesLeft -= numSamples;
    }
}

void Engine::process (const Settings& s, double ppq, double bpm, bool playing, double barBeats,
                      int numSamples, double sampleRate, juce::MidiBuffer& out)
{
    if (numSamples <= 0) return;
    const int ch = juce::jlimit (1, 16, s.channel);

    if (const int m = pendingMod.exchange (-1); m != -1) pendingAtBar = m;
    if (pendingRealign.exchange (false)) realignAtBar = true;

    if (! playing)
    {
        if (wasPlaying)
            for (auto& o : offs)
                if (o.note >= 0) { out.addEvent (juce::MidiMessage::noteOff (o.channel, o.note), 0); o.note = -1; }
        wasPlaying = false;
        lastEndPpq = -1.0;
        // a modulation asked for while stopped: right away
        if (pendingAtBar != -1) { curRatio = pendingAtBar == -2 ? 1.0 : juce::jlimit (0.125, 8.0, curRatio * modulationFactor (pendingAtBar)); pendingAtBar = -1; ratio = curRatio; }
        return;
    }

    bpm = juce::jlimit (20.0, 400.0, bpm);
    barBeats = juce::jmax (0.5, barBeats);
    const double beatsPerSample = bpm / 60.0 / juce::jmax (1000.0, sampleRate);
    if (! wasPlaying || lastEndPpq < 0.0 || std::abs (ppq - lastEndPpq) > 0.02)
        realign (s, ppq);                                   // started, or Cubase jumped
    wasPlaying = true;

    auto segment = [&] (int start, int count, double segPpq)
    {
        if (count <= 0) return;
        const double beats = count * beatsPerSample;
        const double x = rissetX;
        for (int l = 0; l < numLanes; ++l)
        {
            const auto& lane = s.lanes[(size_t) l];
            const int len = juce::jlimit (1, maxSteps, lane.length);
            const double stepBeats = rateBeats (lane.rate);
            const double speed = curRatio * (1.0 + juce::jlimit (-20.0f, 20.0f, lane.drift) / 100.0) / stepBeats;   // steps per beat
            const int layers = lane.risset ? numLayers : 1;
            for (int k = 0; k < layers; ++k)
            {
                const double mult = lane.risset ? std::pow (2.0, x + k - 1) : 1.0;
                const double w = lane.risset ? std::pow (std::sin (juce::MathConstants<double>::pi * (x + k) / numLayers), 2.0) : 1.0;
                layerLevel[(size_t) l][(size_t) k] = (float) w;
                double& ph = phase[(size_t) l][(size_t) k];
                const double adv = beats * speed * mult;
                if (lane.on && adv > 0.0)
                {
                    const double stepSamples = 1.0 / (speed * mult * beatsPerSample);
                    const int gate = (int) juce::jlimit (16.0, sampleRate * 2.0, stepSamples * juce::jlimit (0.05f, 1.0f, s.gate));
                    for (double b = std::ceil (ph - 1e-9); b < ph + adv - 1e-9; b += 1.0)
                    {
                        const int idx = (int) (((long long) b % len + len) % len);
                        const float vel = lane.steps[(size_t) idx] * (float) w;
                        if (vel > 0.03f)
                        {
                            const int pos = start + juce::jlimit (0, count - 1, (int) ((b - ph) / adv * count));
                            noteOn (out, pos, ch, juce::jlimit (0, 127, lane.note), vel, gate);
                            flash[(size_t) l] = juce::jmax (flash[(size_t) l].load(), vel);
                            ++hits;
                        }
                    }
                }
                ph += adv;
            }
            const double shown = phase[(size_t) l][(size_t) (lane.risset ? 1 : 0)];
            position[(size_t) l] = (float) std::fmod (std::fmod (shown, (double) len) + len, (double) len);
        }
        // the Risset turn goes on (layers swap places at the end of each turn, so nothing jumps)
        const double dx = beats / (juce::jmax (0.25f, s.rissetBars) * barBeats) * (s.rissetDown ? -1.0 : 1.0);
        rissetX += dx;
        while (rissetX >= 1.0)
        {
            rissetX -= 1.0;
            for (auto& p : phase) { p[2] = p[1]; p[1] = p[0]; p[0] = p[1] * 0.5; }
        }
        while (rissetX < 0.0)
        {
            rissetX += 1.0;
            for (auto& p : phase) { p[0] = p[1]; p[1] = p[2]; p[2] = p[1] * 2.0; }
        }
        rissetPhase = (float) rissetX;
        juce::ignoreUnused (segPpq);
    };

    // a metric modulation / realign waits for the next bar line: split the block there
    const double endPpq = ppq + numSamples * beatsPerSample;
    int done = 0;
    if (pendingAtBar != -1 || realignAtBar)
    {
        const double bar = std::ceil (ppq / barBeats - 1e-9) * barBeats;
        if (bar < endPpq)
        {
            const int at = juce::jlimit (0, numSamples, (int) std::lround ((bar - ppq) / beatsPerSample));
            segment (0, at, ppq);
            if (pendingAtBar != -1)
            {
                // (the pattern goes on from where it is, only its beat changes speed)
                curRatio = pendingAtBar == -2 ? 1.0 : juce::jlimit (0.125, 8.0, curRatio * modulationFactor (pendingAtBar));
                pendingAtBar = -1;
                ratio = curRatio;
            }
            if (realignAtBar) { realign (s, bar); realignAtBar = false; }
            done = at;
        }
    }
    segment (done, numSamples - done, ppq + done * beatsPerSample);
    flushOffs (out, numSamples);
    lastEndPpq = endPpq;
}
} // namespace phasecat

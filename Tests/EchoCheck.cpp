// Echo-Cat's sound check: echoes arrive on time (synced and free), decay, ping-pong, freeze holds,
// ducking works, and no preset ever blows up (noise and clicks in, finite and bounded out).
#include "PluginProcessor.h"
#include <cstdio>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

struct Rig
{
    EchoCatProcessor p;
    double sr = 48000.0;
    int block = 480;
    Rig() { p.setPlayConfigDetails (2, 2, sr, block); p.prepareToPlay (sr, block); }
    void set (const String& id, float v) { if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
    /** Runs the input through, block by block; returns the output. */
    AudioBuffer<float> run (const AudioBuffer<float>& in)
    {
        AudioBuffer<float> out (in);
        MidiBuffer m;
        for (int pos = 0; pos < out.getNumSamples(); pos += block)
        {
            const int n = jmin (block, out.getNumSamples() - pos);
            AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
            p.processBlock (view, m);
        }
        return out;
    }
};

static int peakIndex (const AudioBuffer<float>& b, int ch, int from, int to)
{
    int best = from;
    for (int i = from; i < jmin (to, b.getNumSamples()); ++i) if (std::abs (b.getSample (ch, i)) > std::abs (b.getSample (ch, best))) best = i;
    return best;
}

int main()
{
    ScopedJuceInitialiser_GUI init;
    // a click, synced 1/8 dotted at 120 BPM (no host: 120) = 375 ms
    {
        Rig r;
        r.p.setCurrentProgram (0);
        r.set ("pingpong", 0.0f); r.set ("space", 0.0f); r.set ("wobble", 0.0f); r.set ("drive", 0.0f); r.set ("feedback", 0.5f);
        r.set ("tone", 1.0f); r.set ("lowcut", 0.0f); r.set ("mix", 1.0f); r.set ("dry", 1.0f);
        AudioBuffer<float> in (2, (int) (r.sr * 2.0)); in.clear();
        in.setSample (0, 100, 1.0f); in.setSample (1, 100, 1.0f);
        auto out = r.run (in);
        const int expect = 100 + (int) std::round (0.375 * r.sr);
        const int got = peakIndex (out, 0, 1000, expect + 2000);
        check (std::abs (got - expect) < 4, "synced echo arrives after 375 ms (sample " + String (got) + ", expected " + String (expect) + ")");
        const float e1 = std::abs (out.getSample (0, got));
        const int got2 = peakIndex (out, 0, got + 2000, got + (int) (0.375 * r.sr) + 2000);
        const float e2 = std::abs (out.getSample (0, got2));
        check (std::abs (got2 - got - (int) std::round (0.375 * r.sr)) < 4 && e2 < e1 && e2 > e1 * 0.2f,
               "the second echo comes one echo-time later and quieter (" + String (e1, 3) + " -> " + String (e2, 3) + ")");
        check (std::abs (out.getSample (0, 100) - 1.0f) < 0.05f, "the dry sound passes straight through (no delay)");
    }
    // free time in ms
    {
        Rig r;
        r.p.setCurrentProgram (1);   // Slapback: 95 ms
        r.set ("drive", 0.0f);
        AudioBuffer<float> in (2, (int) (r.sr)); in.clear();
        in.setSample (0, 50, 1.0f); in.setSample (1, 50, 1.0f);
        auto out = r.run (in);
        const int expect = 50 + (int) std::round (0.095 * r.sr);
        const int got = peakIndex (out, 0, 500, expect + 1500);
        check (std::abs (got - expect) < 4, "free echo time: 95 ms (sample " + String (got) + ")");
    }
    // ping-pong: the first echo on the left, the second on the right
    {
        Rig r;
        r.p.setCurrentProgram (3);   // Ping-Pong Party
        r.set ("space", 0.0f); r.set ("feedback", 0.6f);
        AudioBuffer<float> in (2, (int) (r.sr * 2.0)); in.clear();
        in.setSample (0, 100, 1.0f); in.setSample (1, 100, 1.0f);
        auto out = r.run (in);
        const int d = (int) std::round (0.25 * r.sr);   // 1/8 at 120
        auto energy = [&] (int ch, int at) { float e = 0; for (int i = at - 200; i < at + 600; ++i) e += std::abs (out.getSample (ch, i)); return e; };
        const float l1 = energy (0, 100 + d), r1 = energy (1, 100 + d), l2 = energy (0, 100 + 2 * d), r2 = energy (1, 100 + 2 * d);
        check (l1 > r1 * 4.0f && r2 > l2 * 4.0f, "ping-pong: echo 1 left (" + String (l1, 2) + " / " + String (r1, 2) + "), echo 2 right (" + String (l2, 2) + " / " + String (r2, 2) + ")");
    }
    // freeze: the loop holds with nothing new going in
    {
        Rig r;
        r.p.setCurrentProgram (0);
        r.set ("space", 0.0f); r.set ("feedback", 0.3f);
        AudioBuffer<float> in (2, (int) (r.sr * 1.0));
        Random rng (3);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < in.getNumSamples(); ++i) in.setSample (ch, i, (rng.nextFloat() - 0.5f) * 0.5f);
        r.run (in);
        r.set ("freeze", 1.0f);
        AudioBuffer<float> silence (2, (int) (r.sr * 6.0)); silence.clear();
        auto out = r.run (silence);
        const float early = out.getRMSLevel (0, (int) r.sr, (int) r.sr), late = out.getRMSLevel (0, (int) (r.sr * 5.0), (int) r.sr);
        check (late > 0.02f && late > early * 0.6f, "freeze: the echoes still sound after 5 s of silence (rms " + String (early, 3) + " -> " + String (late, 3) + ")");
        r.set ("freeze", 0.0f);
        AudioBuffer<float> more (2, (int) (r.sr * 8.0)); more.clear();
        auto after = r.run (more);
        check (after.getRMSLevel (0, (int) (r.sr * 7.0), (int) r.sr) < late * 0.1f, "unfreezing: the echoes fade away again");
    }
    // ducking: echoes are quieter while the input plays
    {
        auto echoUnder = [] (float duck)
        {
            Rig r;
            r.p.setCurrentProgram (0);
            r.set ("space", 0.0f); r.set ("dry", 0.0f); r.set ("duck", duck); r.set ("feedback", 0.6f);
            AudioBuffer<float> in (2, (int) (r.sr * 3.0));
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < in.getNumSamples(); ++i) in.setSample (ch, i, 0.4f * std::sin ((float) i * 0.05f));
            auto out = r.run (in);
            return out.getRMSLevel (0, (int) r.sr, (int) (r.sr * 2.0));
        };
        const float plain = echoUnder (0.0f), ducked = echoUnder (1.0f);
        check (ducked < plain * 0.4f, "ducking: echoes under a playing input drop (rms " + String (plain, 3) + " -> " + String (ducked, 3) + ")");
    }
    // every preset: noise, clicks and silence -> finite, bounded, and the shimmer space rings on
    for (int pr = 0; pr < (int) EchoCatProcessor::presets().size(); ++pr)
    {
        Rig r;
        r.p.setCurrentProgram (pr);
        AudioBuffer<float> in (2, (int) (r.sr * 4.0));
        Random rng (pr + 1);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < in.getNumSamples(); ++i)
                in.setSample (ch, i, i < (int) r.sr * 2 ? (rng.nextFloat() - 0.5f) * 1.6f + (i % 9000 == 0 ? 1.0f : 0.0f) : 0.0f);
        auto out = r.run (in);
        bool finite = true;
        float peak = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < out.getNumSamples(); ++i) { const float v = out.getSample (ch, i); finite = finite && std::isfinite (v); peak = jmax (peak, std::abs (v)); }
        const float tail = out.getRMSLevel (0, (int) (r.sr * 2.3), (int) (r.sr * 0.5));
        bool wantTail = false;   // a long feedback or a space should still ring 300 ms after the input stops
        for (auto& [id, v] : EchoCatProcessor::presets()[(size_t) pr].values)
            wantTail = wantTail || (id == "feedback" && v >= 0.2f) || (id == "space" && v > 0.0f);
        check (finite && peak <= 1.0f && (! wantTail || tail > 1.0e-4f),
               String ("preset ") + EchoCatProcessor::presets()[(size_t) pr].name + ": peak " + String (peak, 2) + ", tail " + String (tail, 4));
    }
    // state: a saved project keeps the settings
    {
        Rig a, b;
        a.p.setCurrentProgram (11);
        a.set ("output", -6.0f);
        MemoryBlock mb;
        a.p.getStateInformation (mb);
        b.p.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (b.p.apvts.getRawParameterValue ("shimmer")->load() - 0.7f) < 1e-3f && std::abs (b.p.apvts.getRawParameterValue ("output")->load() + 6.0f) < 1e-3f
               && b.p.getCurrentProgram() == 11, "a saved project keeps Echo-Cat's settings");
    }
    std::printf (failures == 0 ? "ECHO CHECK PASSED\n" : "ECHO CHECK FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}

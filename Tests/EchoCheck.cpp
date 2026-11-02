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
    // a plain rig: free time, one clean echo path, only the echoes out
    auto plainRig = [] (Rig& r, float ms, float fb)
    {
        r.p.setCurrentProgram (0);
        r.set ("sync", 0.0f); r.set ("timeMs", ms); r.set ("feedback", fb);
        r.set ("pingpong", 0.0f); r.set ("space", 0.0f); r.set ("wobble", 0.0f); r.set ("drive", 0.0f); r.set ("duck", 0.0f);
        r.set ("tone", 1.0f); r.set ("lowcut", 0.0f); r.set ("mix", 1.0f); r.set ("dry", 0.0f); r.set ("pitch", 0.0f);
    };
    auto envelope = [] (const AudioBuffer<float>& b, int from, int len, int win)       // RMS in windows
    {
        std::vector<float> e;
        for (int s0 = from; s0 + win <= jmin (from + len, b.getNumSamples()); s0 += win)
            e.push_back (b.getRMSLevel (0, s0, win));
        return e;
    };
    auto freqIn = [] (const AudioBuffer<float>& b, int from, int len)            // by zero crossings
    {
        int crossings = 0, first = -1, last = -1;
        for (int i = from + 1; i < jmin (from + len, b.getNumSamples()); ++i)
            if (b.getSample (0, i - 1) < 0.0f && b.getSample (0, i) >= 0.0f) { ++crossings; if (first < 0) first = i; last = i; }
        return crossings > 1 ? (double) (crossings - 1) * 48000.0 / (double) (last - first) : 0.0;
    };

    // reverse: a sound that starts hard and fades comes back fading IN (backwards)
    {
        Rig fwd, rev;
        plainRig (fwd, 500.0f, 0.0f);
        plainRig (rev, 500.0f, 0.0f);
        rev.set ("mode", 1.0f);
        AudioBuffer<float> in (2, (int) (fwd.sr * 1.6)); in.clear();
        const int len = (int) (0.3 * fwd.sr);
        for (int i = 0; i < len; ++i)
        {
            const float v = 0.5f * (1.0f - (float) i / (float) len) * std::sin (MathConstants<float>::twoPi * 600.0f * (float) i / 48000.0f);
            in.setSample (0, i, v); in.setSample (1, i, v);
        }
        auto riseTime = [&] (const AudioBuffer<float>& out)             // seconds from the echo's start to its loudest point
        {
            const auto e = envelope (out, 0, out.getNumSamples(), 480);
            float peak = 0.0f; int peakAt = 0;
            for (int i = 0; i < (int) e.size(); ++i) if (e[(size_t) i] > peak) { peak = e[(size_t) i]; peakAt = i; }
            int start = peakAt;
            while (start > 0 && e[(size_t) start - 1] > 0.08f * peak) --start;
            return std::make_pair ((peakAt - start) * 0.01, peak);
        };
        const auto [fr, fp] = riseTime (fwd.run (in));
        const auto [rr, rp] = riseTime (rev.run (in));
        check (fr < 0.05 && rr > 0.15 && rp > 0.1f, "reverse: the echo plays backwards (fades in over " + String (rr, 2) + " s, forward: " + String (fr, 2) + " s)");
    }

    // pitch: each echo an octave higher / lower than the last
    for (int semis : { 12, -12, 7 })
    {
        Rig r;
        plainRig (r, 400.0f, 0.6f);
        r.set ("pitch", (float) semis);
        AudioBuffer<float> in (2, (int) (r.sr * 1.2)); in.clear();
        for (int i = 0; i < (int) (0.2 * r.sr); ++i)
        {
            const float v = 0.4f * std::sin (MathConstants<float>::twoPi * 440.0f * (float) i / 48000.0f);
            in.setSample (0, i, v); in.setSample (1, i, v);
        }
        const auto out = r.run (in);
        const double ratio = std::pow (2.0, semis / 12.0);
        const double f1 = freqIn (out, (int) (0.46 * r.sr), (int) (0.12 * r.sr)), f2 = freqIn (out, (int) (0.86 * r.sr), (int) (0.12 * r.sr));
        check (std::abs (f1 / (440.0 * ratio) - 1.0) < 0.04 && std::abs (f2 / (440.0 * ratio * ratio) - 1.0) < 0.04,
               "pitch " + String (semis) + ": 1st echo " + String (f1, 0) + " Hz, 2nd " + String (f2, 0) + " Hz (440 Hz in)");
    }

    // steps: a rhythm of echoes - levels per step, rests stay silent, the pattern repeats with feedback
    {
        Rig r;
        plainRig (r, 100.0f, 0.5f);
        r.set ("mode", 2.0f);
        const float pattern[8] = { 1.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.25f };
        for (int i = 0; i < 8; ++i) r.set ("step" + String (i + 1), pattern[i]);
        AudioBuffer<float> in (2, (int) (r.sr * 2.0)); in.clear();
        for (int i = 100; i < 160; ++i) { in.setSample (0, i, 0.5f); in.setSample (1, i, 0.5f); }   // a short pulse
        const auto out = r.run (in);
        auto levelAt = [&] (double sec) { return out.getMagnitude (0, 100 + (int) (sec * r.sr) - 200, 600); };
        const float s1 = levelAt (0.1), s2 = levelAt (0.2), s3 = levelAt (0.3), s4 = levelAt (0.4), s8 = levelAt (0.8), again = levelAt (0.9);
        check (s1 > 0.2f && s2 < 0.02f * s1 && s4 < 0.02f * s1 && std::abs (s3 / s1 - 0.5f) < 0.1f && std::abs (s8 / s1 - 0.25f) < 0.08f,
               "steps: echo 1 full, 2 silent, 3 half, 8 a quarter (" + String (s3 / s1, 2) + ", " + String (s8 / s1, 2) + ")");
        check (again > 0.25f * s1 && again < 0.75f * s1, "steps: the pattern comes round again, quieter (" + String (again / s1, 2) + ")");
    }

    // the new presets never blow up either (noise in)
    {
        Rig r;
        Random rng (5);
        bool ok = true;
        for (int prog = 16; prog < r.p.getNumPrograms(); ++prog)
        {
            r.p.setCurrentProgram (prog);
            AudioBuffer<float> in (2, (int) (r.sr * 3.0));
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < in.getNumSamples(); ++i) in.setSample (ch, i, i < 48000 ? (rng.nextFloat() * 2.0f - 1.0f) * 0.5f : 0.0f);
            const auto out = r.run (in);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < out.getNumSamples(); ++i)
                    if (! std::isfinite (out.getSample (ch, i)) || std::abs (out.getSample (ch, i)) > 1.5f) ok = false;
        }
        check (ok && r.p.getNumPrograms() >= 24, "reverse / pitch / steps presets: finite and bounded (" + String (r.p.getNumPrograms()) + " presets)");
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

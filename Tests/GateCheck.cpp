// Gate-Cat's check: the steps open and close the sound in time with the DAW (1/16 at 120 BPM = 125 ms),
// an off step is silent, DEPTH 0 leaves the sound alone, SWING moves the second step later, FILTER darkens the
// closed steps, it runs on its own clock without the DAW, every preset is safe, the state comes back.
#include "PluginProcessor.h"
#include <cstdio>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

static constexpr double sr = 48000.0;
static constexpr int block = 256;
using Sig = std::vector<float>;

struct Head : AudioPlayHead
{
    bool playing = true;
    double ppq = 0.0;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (120.0); p.setIsPlaying (playing); p.setPpqPosition (ppq); return p;
    }
};

static std::unique_ptr<GateCatProcessor> make()
{
    auto p = std::make_unique<GateCatProcessor>();
    p->setPlayConfigDetails (2, 2, sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

static void set (GateCatProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void pattern (GateCatProcessor& p, const char* pat)
{
    for (int k = 0; k < 16; ++k) set (p, GateCatProcessor::stepId (k), pat[k] == '9' ? 1.0f : 0.0f);
}

static float sine (int i, float hz = 220.0f) { return 0.5f * (float) std::sin (MathConstants<double>::twoPi * hz * i / sr); }
static float noise (int i) { uint32_t s = (uint32_t) i * 2654435761u; s ^= s >> 13; s *= 1274126177u; return ((float) (s >> 8) / 8388608.0f - 1.0f) * 0.4f; }

static std::pair<Sig, Sig> run (GateCatProcessor& p, double seconds, std::function<float (int)> in, bool playing = true)
{
    Sig l, r;
    AudioBuffer<float> buf (2, block);
    Head head;
    head.playing = playing;
    p.setPlayHead (&head);
    const int total = (int) (seconds * sr);
    for (int pos = 0; pos < total; pos += block)
    {
        head.ppq = pos / sr * 2.0;                      // 120 BPM: 2 beats a second
        for (int i = 0; i < block; ++i) { const float x = in (pos + i); buf.setSample (0, i, x); buf.setSample (1, i, x); }
        MidiBuffer midi;
        p.processBlock (buf, midi);
        for (int i = 0; i < block; ++i) { l.push_back (buf.getSample (0, i)); r.push_back (buf.getSample (1, i)); }
    }
    return { l, r };
}

static float rms (const Sig& s, double a, double b)
{
    double sum = 0.0; int n = 0;
    for (int i = (int) (a * sr); i < jmin ((int) s.size(), (int) (b * sr)); ++i) { sum += s[(size_t) i] * s[(size_t) i]; ++n; }
    return n > 0 ? (float) std::sqrt (sum / n) : 0.0f;
}
static bool finite (const Sig& s) { for (auto v : s) if (! std::isfinite (v)) return false; return true; }
static float peak (const Sig& s) { float m = 0.0f; for (auto v : s) m = jmax (m, std::abs (v)); return m; }
/** how bright: the average size of the change from one sample to the next, against the level */
static float bright (const Sig& s, double a, double b)
{
    double d = 0.0, e = 0.0;
    for (int i = (int) (a * sr) + 1; i < jmin ((int) s.size(), (int) (b * sr)); ++i) { d += std::abs (s[(size_t) i] - s[(size_t) i - 1]); e += std::abs (s[(size_t) i]); }
    return e > 0.0 ? (float) (d / e) : 0.0f;
}

int main()
{
    ScopedJuceInitialiser_GUI gui;
    constexpr double step = 0.125;      // 1/16 at 120 BPM

    {   // on / off steps, in time
        auto p = make();
        pattern (*p, "9.9.9.9.9.9.9.9.");
        set (*p, "length", 1.0f);
        auto [l, r] = run (*p, 2.0, [] (int i) { return sine (i); });
        const float on = rms (l, 4 * step + 0.02, 5 * step - 0.01), offv = rms (l, 5 * step + 0.03, 6 * step - 0.01);
        check (on > 0.3f && offv < 0.01f, "an on step plays (" + String (on, 3) + "), an off step is silent (" + String (offv, 4) + ")");
        // where does it open? the first sample of step 6 (index 6) loud again
        const float rise = rms (l, 6 * step + 0.002, 6 * step + 0.01);
        check (rise > 0.2f, "the gate opens on the 1/16 grid of the DAW (" + String (rise, 3) + ")");
        check (finite (l) && peak (l) <= 1.0f, "clean and safe");
    }

    {   // levels in between
        auto p = make();
        pattern (*p, "9999999999999999");
        set (*p, "length", 1.0f);
        for (int k = 0; k < 16; ++k) set (*p, GateCatProcessor::stepId (k), k % 2 == 0 ? 1.0f : 0.5f);
        auto [l, r] = run (*p, 1.0, [] (int i) { return sine (i); });
        const float a = rms (l, 4 * step + 0.03, 5 * step - 0.01), b = rms (l, 5 * step + 0.03, 6 * step - 0.01);
        check (std::abs (b / a - 0.5f) < 0.05f, "a step at 50 % is half as loud (" + String (b / a, 2) + ")");
    }

    {   // DEPTH 0 = nothing happens
        auto p = make();
        pattern (*p, "9.9.9.9.9.9.9.9.");
        set (*p, "depth", 0.0f);
        auto [l, r] = run (*p, 1.0, [] (int i) { return sine (i); });
        const float a = rms (l, 0.1, 0.9);
        check (std::abs (a - 0.3536f) < 0.01f, "DEPTH 0: the sound passes untouched (" + String (a, 3) + ")");
    }

    {   // SWING: the second step of a pair starts later
        auto opens = [] (float swing)
        {
            auto p = make();
            pattern (*p, ".9.9.9.9.9.9.9.9");
            set (*p, "length", 1.0f); set (*p, "attack", 0.0f); set (*p, "release", 0.0f); set (*p, "swing", swing);
            auto [l, r] = run (*p, 1.0, [] (int) { return 0.5f; });
            for (int i = (int) (4 * step * sr); i < (int) (6 * step * sr); ++i) if (l[(size_t) i] > 0.25f) return (double) i / sr - 4 * step;
            return -1.0;
        };
        const double straight = opens (0.0f), swung = opens (1.0f);
        check (std::abs (straight - step) < 0.003 && swung > step + 0.03, "SWING moves the off-step later ("
               + String (straight * 1000.0, 0) + " ms -> " + String (swung * 1000.0, 0) + " ms)");
    }

    {   // FILTER: a half-open step is darker
        auto p = make();
        pattern (*p, "9999999999999999");
        set (*p, "length", 1.0f); set (*p, "depth", 0.0f);
        for (int k = 0; k < 16; ++k) set (*p, GateCatProcessor::stepId (k), k % 2 == 0 ? 1.0f : 0.2f);
        set (*p, "filter", 1.0f);
        auto [l, r] = run (*p, 1.0, noise);
        const float open = bright (l, 4 * step + 0.03, 5 * step - 0.01), shut = bright (l, 5 * step + 0.03, 6 * step - 0.01);
        check (shut < open * 0.6f, "FILTER: the quiet steps are darker (" + String (open, 3) + " -> " + String (shut, 3) + ")");
    }

    {   // no DAW playing: its own clock
        auto p = make();
        pattern (*p, "9.9.9.9.9.9.9.9.");
        auto [l, r] = run (*p, 1.0, [] (int i) { return sine (i); }, false);
        float lo = 1.0f, hi = 0.0f;
        for (double t = 0.05; t < 0.9; t += 0.0625) { const float v = rms (l, t, t + 0.03); lo = jmin (lo, v); hi = jmax (hi, v); }
        check (hi > 0.2f && lo < 0.02f, "without the DAW it chops on its own clock");
    }

    {   // every preset
        auto p = make();
        bool ok = true;
        for (int i = 0; i < p->getNumPrograms(); ++i)
        {
            p->setCurrentProgram (i);
            auto [l, r] = run (*p, 1.0, noise);
            if (! finite (l) || peak (l) > 1.0f || rms (l, 0.0, 1.0) < 0.01f) { ok = false; check (false, String ("preset ") + p->getProgramName (i)); }
        }
        check (ok && p->getNumPrograms() == 16, "all " + String (p->getNumPrograms()) + " presets play safely");
    }

    {   // the state
        auto a = make();
        a->setCurrentProgram (5);
        set (*a, GateCatProcessor::stepId (3), 0.77f);
        MemoryBlock m;
        a->getStateInformation (m);
        auto b = make();
        b->setStateInformation (m.getData(), (int) m.getSize());
        check (std::abs (b->apvts.getRawParameterValue (GateCatProcessor::stepId (3))->load() - 0.77f) < 0.01f && b->getCurrentProgram() == 5,
               "the state comes back (steps and preset)");
    }

    {   // fast
        auto p = make();
        set (*p, "filter", 1.0f);
        const auto t0 = Time::getMillisecondCounterHiRes();
        run (*p, 10.0, noise);
        const double ms = Time::getMillisecondCounterHiRes() - t0;
        check (ms < 10000.0 * 0.05, "10 s of sound in " + String (ms, 0) + " ms");
    }

    std::printf (failures == 0 ? "ALL GATE CHECKS PASSED\n" : "%d GATE CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}

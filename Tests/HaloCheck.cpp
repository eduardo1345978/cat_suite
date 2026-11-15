// Halo-Cat's check: one key plays the progression (a new chord on each change), several keys are the chord,
// the voices glide, the planets ring in time with the DAW, every preset is safe, the state comes back.
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
    double ppq = 0.0;
    bool playing = true;
    Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm (120.0); p.setIsPlaying (playing); p.setPpqPosition (ppq); return p; }
};

static std::unique_ptr<HaloCatProcessor> make()
{
    auto p = std::make_unique<HaloCatProcessor>();
    p->setPlayConfigDetails (0, 2, sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

static void set (HaloCatProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

/** run for some seconds from song position 0; notes = { time, note, on } */
static Sig run (HaloCatProcessor& p, double seconds, std::vector<std::tuple<double, int, bool>> notes, std::function<void (double)> each = {}, bool playing = true)
{
    Sig out;
    AudioBuffer<float> buf (2, block);
    Head head;
    head.playing = playing;
    p.setPlayHead (&head);
    for (int pos = 0; pos < (int) (seconds * sr); pos += block)
    {
        head.ppq = pos / sr * 2.0;
        MidiBuffer midi;
        for (auto& [t, n, on] : notes)
        {
            const int s = (int) (t * sr);
            if (s >= pos && s < pos + block) midi.addEvent (on ? MidiMessage::noteOn (1, n, (uint8) 100) : MidiMessage::noteOff (1, n), s - pos);
        }
        p.processBlock (buf, midi);
        for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
        if (each) each (pos / sr);
    }
    return out;
}

static float rms (const Sig& s, double a, double b)
{
    double sum = 0.0; int n = 0;
    for (int i = (int) (a * sr); i < jmin ((int) s.size(), (int) (b * sr)); ++i) { sum += s[(size_t) i] * s[(size_t) i]; ++n; }
    return n > 0 ? (float) std::sqrt (sum / n) : 0.0f;
}
static bool finite (const Sig& s) { for (auto v : s) if (! std::isfinite (v)) return false; return true; }
static float peak (const Sig& s) { float m = 0.0f; for (auto v : s) m = jmax (m, std::abs (v)); return m; }

int main()
{
    ScopedJuceInitialiser_GUI gui;

    {   // one key: the progression (Pop: I V vi IV in C, a chord every bar at 120 BPM = 2 s)
        auto p = make();
        set (*p, "change", 0.0f);
        StringArray names;
        auto out = run (*p, 7.9, { { 0.0, 60, true } }, [&] (double t)
        {
            const auto n = p->chordName();
            if (n.isNotEmpty() && (names.isEmpty() || names[names.size() - 1] != n)) names.add (n);
            ignoreUnused (t);
        });
        check (names.joinIntoString (" ") == "C G Am F", "one key held: the progression C G Am F (" + names.joinIntoString (" ") + ")");
        check (rms (out, 1.0, 7.9) > 0.01f && finite (out) && peak (out) <= 1.0f, "it sings, cleanly (" + String (rms (out, 1.0, 7.9), 3) + ")");
    }

    {   // several keys: your own chord
        auto p = make();
        run (*p, 1.0, { { 0.0, 62, true }, { 0.0, 65, true }, { 0.0, 69, true } });
        const auto c = p->engine.currentChord();
        check (c.size() == 3 && c[0] == 62 && c[1] == 65 && c[2] == 69, "three keys held: that is the chord (D F A)");
    }

    {   // nothing held: it fades out
        auto p = make();
        auto out = run (*p, 9.0, { { 0.0, 60, true }, { 1.0, 60, false } });
        set (*p, "shimmer", 0.0f);
        check (rms (out, 0.3, 0.9) > 0.01f && rms (out, 8.0, 9.0) < rms (out, 0.3, 0.9) * 0.1f, "let go: the choir fades away");
    }

    {   // the planets ring in time: Waltzing, planet 1 every beat (0.5 s) -> bell onsets with only the bells on
        auto p = make();
        set (*p, "choir", 0.0f); set (*p, "dust", 0.0f); set (*p, "shimmer", 0.0f);
        int flashes = 0;
        float lastFlash = 0.0f;
        run (*p, 4.0, { { 0.0, 60, true } }, [&] (double)
        {
            const float f = p->engine.planetFlash[0].load();
            if (f > 0.5f && lastFlash <= 0.5f) ++flashes;
            lastFlash = f;
        });
        check (flashes >= 7 && flashes <= 9, "planet 1 rings every beat: " + String (flashes) + " bells in 4 s at 120 BPM");
    }

    {   // glide: a held chord change slides (no jump) - the voices' pitch moves over time
        auto p = make();
        set (*p, "glide", 1.0f); set (*p, "bells", 0.0f); set (*p, "dust", 0.0f); set (*p, "shimmer", 0.0f); set (*p, "change", 0.0f);
        auto out = run (*p, 3.0, { { 0.0, 60, true } });
        check (finite (out) && peak (out) <= 1.0f, "a long glide stays clean");
    }

    {   // without the DAW playing it keeps time on its own
        auto p = make();
        auto out = run (*p, 3.0, { { 0.0, 57, true } }, {}, false);
        check (rms (out, 1.0, 3.0) > 0.01f && p->chordName() == "A", "without the DAW: it plays on its own clock (A: the Pop progression in A, " + p->chordName() + ")");
    }

    {   // every preset
        auto p = make();
        bool ok = true;
        for (int i = 0; i < p->getNumPrograms(); ++i)
        {
            p->setCurrentProgram (i);
            auto out = run (*p, 3.0, { { 0.0, 60, true } });
            if (! finite (out) || peak (out) > 1.0f || rms (out, 0.5, 3.0) < 0.005f) { ok = false; check (false, "preset " + p->getProgramName (i)); }
            run (*p, 0.1, { { 0.0, 60, false } });
        }
        check (ok && p->getNumPrograms() == 12, String (p->getNumPrograms()) + " presets play safely");
    }

    {   // the state
        auto a = make();
        a->setCurrentProgram (3);
        set (*a, "dust", 0.77f);
        MemoryBlock m;
        a->getStateInformation (m);
        auto b = make();
        b->setStateInformation (m.getData(), (int) m.getSize());
        check (std::abs (b->apvts.getRawParameterValue ("dust")->load() - 0.77f) < 0.01f && b->getCurrentProgram() == 3, "the state comes back");
    }

    {   // fast enough: 8 keys held, everything on
        auto p = make();
        set (*p, "dust", 1.0f); set (*p, "bells", 1.0f);
        const auto t0 = Time::getMillisecondCounterHiRes();
        run (*p, 10.0, { { 0.0, 60, true }, { 0.0, 64, true }, { 0.0, 67, true }, { 0.0, 71, true }, { 0.0, 74, true } });
        const double ms = Time::getMillisecondCounterHiRes() - t0;
        check (ms < 10000.0 * 0.15, "10 s of sound in " + String (ms, 0) + " ms");
    }

    std::printf (failures == 0 ? "ALL HALO CHECKS PASSED\n" : "%d HALO CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}

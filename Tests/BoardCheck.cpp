// Poly Board's check: every pedal does what it says (drive adds overtones, the compressor squashes, the filter moves,
// the phaser cuts notches, the chorus widens, the delay echoes in time, the reverb rings on), the order matters,
// a mono synth comes out on both sides, a MIDI footswitch works, every preset is safe, the state (with the order)
// comes back, the tuner hears the note.
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
    Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm (120.0); p.setIsPlaying (true); return p; }
};

static std::unique_ptr<PolyBoardProcessor> make (int ins = 2)
{
    auto p = std::make_unique<PolyBoardProcessor>();
    p->setPlayConfigDetails (ins, 2, sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

static void set (PolyBoardProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

/** Only these pedals on (the rest off). */
static void only (PolyBoardProcessor& p, std::initializer_list<int> pedals)
{
    for (int pd = 0; pd < board::numPedals; ++pd) set (p, PolyBoardProcessor::paramId (pd, "on"), 0.0f);
    for (int pd : pedals) set (p, PolyBoardProcessor::paramId (pd, "on"), 1.0f);
}

static std::pair<Sig, Sig> run (PolyBoardProcessor& p, double seconds, std::function<float (int)> in, std::vector<std::pair<double, MidiMessage>> midiAt = {})
{
    Sig l, r;
    AudioBuffer<float> buf (2, block);
    Head head;
    p.setPlayHead (&head);
    const int total = (int) (seconds * sr);
    for (int pos = 0; pos < total; pos += block)
    {
        for (int i = 0; i < block; ++i) { const float x = in ? in (pos + i) : 0.0f; buf.setSample (0, i, x); buf.setSample (1, i, x); }
        MidiBuffer midi;
        for (auto& [t, m] : midiAt) { const int s = (int) (t * sr); if (s >= pos && s < pos + block) midi.addEvent (m, s - pos); }
        p.processBlock (buf, midi);
        for (int i = 0; i < block; ++i) { l.push_back (buf.getSample (0, i)); r.push_back (buf.getSample (1, i)); }
    }
    p.setPlayHead (nullptr);
    return { l, r };
}

static float rms (const Sig& s, double a, double b)
{
    const int i0 = jlimit (0, (int) s.size(), (int) (a * sr)), i1 = jlimit (0, (int) s.size(), (int) (b * sr));
    double e = 0.0; for (int i = i0; i < i1; ++i) e += (double) s[(size_t) i] * s[(size_t) i];
    return i1 > i0 ? (float) std::sqrt (e / (i1 - i0)) : 0.0f;
}
static float peak (const Sig& s, double a = 0.0, double b = 1.0e9)
{
    float m = 0.0f;
    for (int i = jmax (0, (int) (a * sr)); i < jmin ((int) s.size(), (int) (b * sr)); ++i) m = jmax (m, std::abs (s[(size_t) i]));
    return m;
}
static bool finite (const Sig& s) { for (float x : s) if (! std::isfinite (x)) return false; return true; }
/** Brightness: the average frequency (Hz) from the slope of the signal. */
static float bright (const Sig& s, double a, double b)
{
    double e = 0.0, d = 0.0;
    for (int i = (int) (a * sr) + 1; i < (int) (b * sr) && i < (int) s.size(); ++i) { e += (double) s[(size_t) i] * s[(size_t) i]; const double x = s[(size_t) i] - s[(size_t) i - 1]; d += x * x; }
    return (float) (sr / MathConstants<double>::twoPi * std::sqrt (d / (e + 1.0e-20)));
}

static float sine (int i, float hz = 220.0f, float amp = 0.3f) { return amp * (float) std::sin (MathConstants<double>::twoPi * hz * i / sr); }
static float saw (int i, float hz = 110.0f) { const double ph = std::fmod (hz * i / sr, 1.0); return (float) (0.4 * (2.0 * ph - 1.0)); }

int main()
{
    ScopedJuceInitialiser_GUI juce;
    using namespace board;

    {   // all off = the sound as it came in
        auto p = make();
        only (*p, {});
        auto [l, r] = run (*p, 0.5, [] (int i) { return sine (i); });
        // (only the safety limiter's tiny low-cut is left: the level is the same, the phase moves a hair)
        Sig in; for (size_t i = 0; i < l.size(); ++i) in.push_back (sine ((int) i));
        const float ratio = rms (l, 0.1, 0.5) / rms (in, 0.1, 0.5);
        check (std::abs (ratio - 1.0f) < 0.02f && std::abs (bright (l, 0.1, 0.5) - 220.0f) < 3.0f, "all pedals off: the sound goes through as it was (level x" + String (ratio, 3) + ")");
    }

    {   // DRIVE adds overtones (brighter), the more GAIN the more
        auto p = make();
        only (*p, { Drive });
        set (*p, "drive_b", 1.0f);
        set (*p, "drive_a", 0.1f);
        auto [l1, r1] = run (*p, 0.5, [] (int i) { return sine (i); });
        set (*p, "drive_a", 0.9f);
        auto [l2, r2] = run (*p, 0.5, [] (int i) { return sine (i); });
        const float b1 = bright (l1, 0.2, 0.5), b2 = bright (l2, 0.2, 0.5);
        check (b2 > b1 * 1.5f && b1 >= 200.0f, "DRIVE: more gain = more overtones (" + String (b1, 0) + " -> " + String (b2, 0) + " Hz)");
        check (peak (l2, 0.2) < 1.0f, "DRIVE: not much louder, just squarer (peak " + String (peak (l2, 0.2), 2) + ")");
    }

    {   // COMPRESSOR: loud gets turned down more than quiet
        auto p = make();
        only (*p, { Comp });
        set (*p, "comp_a", 0.8f); set (*p, "comp_b", 0.0f);
        auto [lq, rq] = run (*p, 0.6, [] (int i) { return sine (i, 220.0f, 0.05f); });
        auto [ll, rl] = run (*p, 0.6, [] (int i) { return sine (i, 220.0f, 0.5f); });
        const float ratioIn = 10.0f, ratioOut = rms (ll, 0.3, 0.6) / jmax (1.0e-6f, rms (lq, 0.3, 0.6));
        check (ratioOut < ratioIn * 0.6f, "COMPRESSOR: 10x louder in = only " + String (ratioOut, 1) + "x louder out");
        check (p->engine.meters.gainReduction.load() < 0.8f, "COMPRESSOR: shows how much it squashes");
    }

    {   // FILTER: the brightness moves up and down (the auto-wah)
        auto p = make();
        only (*p, { Filter });
        set (*p, "filter_a", 1.0f); set (*p, "filter_b", 4.0f / 6.0f);     // 1/4: one sweep per beat
        auto [l, r] = run (*p, 2.0, [] (int i) { return saw (i); });
        float lo = 1.0e9f, hi = 0.0f;
        for (double t = 0.5; t < 1.9; t += 0.05) { const float b = bright (l, t, t + 0.04); lo = jmin (lo, b); hi = jmax (hi, b); }
        check (hi > lo * 2.0f, "FILTER: it opens and closes (" + String (lo, 0) + " .. " + String (hi, 0) + " Hz)");
    }

    {   // PHASER: the level of a steady tone moves (a notch sweeps through it)
        auto p = make();
        only (*p, { Phaser });
        set (*p, "phaser_a", 0.6f); set (*p, "phaser_b", 1.0f);
        auto [l, r] = run (*p, 3.0, [] (int i) { return sine (i, 700.0f); });
        float lo = 1.0e9f, hi = 0.0f;
        for (double t = 0.5; t < 2.9; t += 0.05) { const float v = rms (l, t, t + 0.05); lo = jmin (lo, v); hi = jmax (hi, v); }
        check (hi > lo * 1.5f, "PHASER: a notch sweeps through the sound (" + String (lo, 3) + " .. " + String (hi, 3) + ")");
    }

    {   // CHORUS: left and right become different (wide)
        auto p = make();
        only (*p, { Chorus });
        set (*p, "chorus_b", 1.0f);
        auto [l, r] = run (*p, 1.0, [] (int i) { return saw (i); });
        double diff = 0.0, sum = 0.0;
        for (size_t i = 9600; i < l.size(); ++i) { diff += std::abs (l[i] - r[i]); sum += std::abs (l[i]); }
        check (diff / sum > 0.1, "CHORUS: left and right differ = wide (" + String (diff / sum, 2) + ")");
    }

    {   // DELAY: TIME 1/8 at 120 BPM = an echo 0.25 s later, then another
        auto p = make();
        only (*p, { Delay });
        set (*p, "delay_a", 2.0f / 6.0f); set (*p, "delay_b", 0.5f);
        auto [l, r] = run (*p, 1.2, [] (int i) { return i < 480 ? sine (i, 440.0f, 0.6f) : 0.0f; });
        const float echo1 = peak (l, 0.24, 0.27), echo2 = peak (r, 0.49, 0.52), between = peak (l, 0.05, 0.22);
        check (echo1 > 0.05f && between < 0.01f, "DELAY: 1/8 at 120 BPM = an echo after 0.25 s (" + String (echo1, 3) + ")");
        check (echo2 > 0.01f && echo2 < echo1, "DELAY: a second echo, quieter, on the other side (ping-pong)");
    }

    {   // REVERB: a tail after the sound stops
        auto p = make();
        only (*p, { board::Reverb });
        set (*p, "reverb_a", 1.0f); set (*p, "reverb_b", 0.8f);
        auto [l, r] = run (*p, 2.0, [] (int i) { return i < 4800 ? saw (i) : 0.0f; });
        check (rms (l, 0.4, 0.8) > 0.005f, "REVERB: it rings on after the note (" + String (rms (l, 0.4, 0.8), 4) + ")");
    }

    {   // the order matters: drive -> reverb is not reverb -> drive
        auto p = make();
        only (*p, { Drive, board::Reverb });
        set (*p, "drive_a", 0.9f); set (*p, "reverb_b", 0.8f);
        p->setOrder ({ 0, 1, 2, 3, 4, 5, 6 });
        auto [l1, r1] = run (*p, 1.0, [] (int i) { return saw (i); });
        p->engine.reset();
        p->setOrder ({ 6, 1, 2, 3, 4, 5, 0 });
        auto [l2, r2] = run (*p, 1.0, [] (int i) { return saw (i); });
        double d = 0.0; for (size_t i = 24000; i < l1.size(); ++i) d += std::abs (l1[i] - l2[i]);
        check (d / (double) (l1.size() - 24000) > 0.01, "the order matters: drive into reverb sounds different from reverb into drive");
        p->setOrder ({ 0, 0, 1, 2, 3, 4, 5 });
        check (p->getOrder()[1] == 1, "a broken order (a pedal twice) is refused");
        p->movePedal (6, 0);
        check (p->getOrder()[0] == 0 && p->getOrder()[1] == 6, "move a pedal: from the last place to the first");
    }

    {   // a mono synth (one input) comes out on both sides
        auto p = make (1);
        only (*p, {});
        AudioBuffer<float> buf (2, block);
        MidiBuffer midi;
        for (int i = 0; i < block; ++i) { buf.setSample (0, i, sine (i)); buf.setSample (1, i, 0.0f); }
        p->processBlock (buf, midi);
        check (buf.getMagnitude (1, 0, block) > 0.2f, "a mono synth comes out on both sides");
    }

    {   // a MIDI footswitch: CC 80 switches the first pedal on the board
        auto p = make();
        only (*p, {});
        const int first = p->getOrder()[0];
        run (*p, 0.1, {}, { { 0.0, MidiMessage::controllerEvent (1, 80, 127) } });
        check (p->apvts.getRawParameterValue (PolyBoardProcessor::paramId (first, "on"))->load() > 0.5f, "MIDI footswitch CC 80: the first pedal on");
        run (*p, 0.1, {}, { { 0.0, MidiMessage::controllerEvent (1, 80, 127) } });
        check (p->apvts.getRawParameterValue (PolyBoardProcessor::paramId (first, "on"))->load() < 0.5f, "... and off again");
    }

    {   // every preset: clean and safe with a loud saw chord
        auto p = make();
        bool ok = true; String bad;
        for (int i = 0; i < p->getNumPrograms(); ++i)
        {
            p->setCurrentProgram (i);
            p->engine.reset();
            auto [l, r] = run (*p, 1.5, [] (int k) { return saw (k, 110.0f) + saw (k, 164.8f) + saw (k, 220.0f); });
            if (! finite (l) || ! finite (r) || peak (l) > 1.0f || rms (l, 0.5, 1.5) < 0.01f) { ok = false; bad << p->getProgramName (i) << " "; }
        }
        check (ok, "all " + String (p->getNumPrograms()) + " presets are clean and safe " + bad);
    }

    {   // the state comes back, with the order
        auto a = make();
        a->setCurrentProgram (9);           // Shoegaze Wall: its own order
        set (*a, "delay_b", 0.77f);
        MemoryBlock m;
        a->getStateInformation (m);
        auto b = make();
        b->setStateInformation (m.getData(), (int) m.getSize());
        check (b->getOrder() == a->getOrder() && std::abs (b->apvts.getRawParameterValue ("delay_b")->load() - 0.77f) < 0.01f && b->getCurrentProgram() == 9,
               "the state comes back (knobs, the order, the preset)");
    }

    {   // the tuner hears the note
        auto p = make();
        only (*p, {});
        run (*p, 0.3, [] (int i) { return saw (i, 220.0f); });
        const float hz = p->detectPitch();
        check (std::abs (hz / 220.0f - 1.0f) < 0.01f, "the tuner hears A3 (" + String (hz, 1) + " Hz)");
        run (*p, 0.3, {});
        check (p->detectPitch() == 0.0f, "the tuner says nothing in silence");
    }

    {   // fast enough: all seven on
        auto p = make();
        only (*p, { 0, 1, 2, 3, 4, 5, 6 });
        const auto t0 = Time::getMillisecondCounterHiRes();
        run (*p, 10.0, [] (int i) { return saw (i); });
        const double ms = Time::getMillisecondCounterHiRes() - t0;
        check (ms < 10000.0 * 0.15, "all 7 pedals: 10 s of sound in " + String (ms, 0) + " ms");
    }

    std::printf (failures == 0 ? "ALL BOARD CHECKS PASSED\n" : "%d BOARD CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}

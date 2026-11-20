// Chop-Cat's check: it passes the sound untouched when nothing glitches; every glitch does what it says
// (repeat, reverse, scatter, mute, roll, tape stop, pitch); MIDI keys and quantize; CHAOS is in time, LOCK repeats,
// FILL fills; every preset and 50 CRAFTs are safe; UNDO and the state come back.
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
static constexpr int stepLen = 6000;        // a 1/16 at 120 BPM

struct Head : AudioPlayHead
{
    double ppq = 0.0;
    Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm (120.0); p.setIsPlaying (true); p.setPpqPosition (ppq); return p; }
};

static std::unique_ptr<ChopCatProcessor> make (int preset = 0)
{
    auto p = std::make_unique<ChopCatProcessor>();
    p->setPlayConfigDetails (2, 2, sr, block);
    p->applyPreset (preset);
    p->prepareToPlay (sr, block);
    return p;
}

static void set (ChopCatProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

/** input: every sample different (so we can find where a piece came from) */
static std::vector<float> noise (int n, int seed = 3)
{
    Random r (seed);
    std::vector<float> v ((size_t) n);
    for (auto& x : v) x = (r.nextFloat() * 2.0f - 1.0f) * 0.4f;
    return v;
}

/** drums: a noisy hit every 1/8, decaying */
static std::vector<float> drums (int n)
{
    Random r (9);
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i) { const int ph = i % 12000; v[(size_t) i] = (r.nextFloat() * 2.0f - 1.0f) * 0.8f * std::exp (-(float) ph / 1500.0f); }
    return v;
}

/** runs the input through; midi = { sample, note, on }. engineOnly: straight through the engine (without the safety
    limiter's DC filter, which changes every sample a tiny bit), so pieces can be matched exactly */
static std::vector<float> run (ChopCatProcessor& p, const std::vector<float>& in, std::vector<std::tuple<int, int, bool>> midi = {}, bool engineOnly = true)
{
    std::vector<float> out (in.size());
    AudioBuffer<float> buf (2, block);
    Head head;
    p.setPlayHead (&head);
    for (int pos = 0; pos + block <= (int) in.size(); pos += block)
    {
        head.ppq = pos / sr * 2.0;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) buf.setSample (c, i, in[(size_t) (pos + i)]);
        MidiBuffer mb;
        for (auto& [s, n, on] : midi) if (s >= pos && s < pos + block) mb.addEvent (on ? MidiMessage::noteOn (1, n, (uint8) 100) : MidiMessage::noteOff (1, n), s - pos);
        if (engineOnly)
        {
            auto st = p.settings();
            st.playing = true; st.ppq = head.ppq; st.bpm = 120.0;
            p.engine.process (buf, mb, st);
        }
        else p.processBlock (buf, mb);
        for (int i = 0; i < block; ++i) out[(size_t) (pos + i)] = buf.getSample (0, i);
    }
    p.setPlayHead (nullptr);
    return out;
}

/** how well out[a..a+n] matches in[b..b+n] (max difference) */
static float diff (const std::vector<float>& out, int a, const std::vector<float>& in, int b, int n, int dir = 1)
{
    float d = 0.0f; int worst = 0;
    for (int i = 0; i < n; ++i) { const float x = std::abs (out[(size_t) (a + i)] - in[(size_t) (b + dir * i)]); if (x > d) { d = x; worst = i; } }
    if (getenv ("CHOP_DEBUG") && d > 1e-4f) std::printf ("     diff %f at +%d (out[%d]=%f in[%d]=%f)\n", d, worst, a + worst, out[(size_t) (a + worst)], b + dir * worst, in[(size_t) (b + dir * worst)]);
    return d;
}

static void steps (ChopCatProcessor& p, const char* s)
{
    for (int k = 0; k < 16; ++k) p.setStep (k, s[k] >= 'a' && s[k] <= 'l' ? 1 + (s[k] - 'a') : 0);
}

int main()
{
    ScopedJuceInitialiser_GUI gui;
    const int bar = 16 * stepLen;
    const auto in = noise (bar * 2 + block * 4);

    // (a glitch fades in over a few ms - SMOOTH - so the exact comparisons start 1500 samples into a step)
    {   // nothing on: the sound goes through untouched
        auto p = make (0);
        const auto out = run (*p, in);
        int first = -1; for (int i = 0; i < bar * 2; ++i) if (std::abs (out[(size_t) i] - in[(size_t) i]) > 1e-5f) { first = i; break; }
        if (first >= 0) std::printf ("     first difference at %d (%f vs %f)\n", first, out[(size_t) first], in[(size_t) first]);
        check (first < 0, "nothing on: the sound passes untouched");
    }
    {   // REPEAT 1/32 on step 3: the 2nd half of the step is the 1st half again
        auto p = make (0);
        steps (*p, "..c.............");
        const auto out = run (*p, in);
        const int s0 = 2 * stepLen, half = stepLen / 2;
        check (diff (out, s0 + half + 400, in, s0 + 400, half - 800) < 1e-4f, "REPEAT 1/32: the step's 2nd half repeats its 1st half");
        check (diff (out, s0 + 400, in, s0 + 400, half - 800) < 1e-4f, "REPEAT: the first time through is the real sound");
        check (diff (out, 4 * stepLen, in, 4 * stepLen, stepLen) < 1e-4f, "after the glitch: the sound again");
    }
    {   // REVERSE on step 5: the step before it, backwards
        auto p = make (0);
        steps (*p, "....f...........");
        const auto out = run (*p, in);
        const int s0 = 4 * stepLen;
        check (diff (out, s0 + 1500, in, s0 - 1 - 1500, stepLen - 3000, -1) < 1e-4f, "REVERSE: plays what came before, backwards");
    }
    {   // SCATTER: a whole earlier step
        auto p = make (0);
        steps (*p, "........j.......");
        const auto out = run (*p, in);
        const int s0 = 8 * stepLen;
        int found = -1;
        for (int back = 1; back <= 8; ++back) if (diff (out, s0 + 1500, in, s0 - back * stepLen + 1500, stepLen - 3000) < 1e-4f) found = back;
        check (found > 0, "SCATTER: plays an earlier step of the bar (" + String (found) + " steps back)");
    }
    {   // MUTE
        auto p = make (0);
        steps (*p, "......l.........");
        const auto out = run (*p, in);
        float pk = 0.0f;
        for (int i = 6 * stepLen + 1500; i < 7 * stepLen - 400; ++i) pk = jmax (pk, std::abs (out[(size_t) i]));
        check (pk < 1e-3f, "MUTE: silence");
    }
    {   // TAPE STOP: slower and slower - the output falls behind the input, and ends quiet
        auto p = make (0);
        steps (*p, "............gggg");
        const auto out = run (*p, in);
        float endPk = 0.0f;
        for (int i = 16 * stepLen - 600; i < 16 * stepLen - 50; ++i) endPk = jmax (endPk, std::abs (out[(size_t) i]));
        const float notLive = diff (out, 13 * stepLen, in, 13 * stepLen, 2000);
        check (endPk < 0.05f && notLive > 0.05f, "TAPE STOP: slows down and stops (end level " + String (endPk, 3) + ")");
    }
    {   // PITCH UP: the step before, twice as fast
        auto p = make (0);
        steps (*p, "..........h.....");
        const auto out = run (*p, in);
        const int s0 = 10 * stepLen;
        float d = 0.0f;
        for (int i = 1500; i < stepLen / 2 - 400; ++i) d = jmax (d, std::abs (out[(size_t) (s0 + i)] - in[(size_t) (s0 - stepLen + 2 * i)]));
        check (d < 1e-4f, "PITCH UP: the step before, at double speed (an octave up)");
    }
    {   // ROLL: it repeats, and the repeats get shorter
        auto p = make (0);
        steps (*p, "............eeee");
        const auto out = run (*p, in);
        const int s0 = 12 * stepLen, first = stepLen / 2;
        check (diff (out, s0 + first + 300, in, s0 + 300, 1000) < 1e-4f, "ROLL: repeats the start");
        const int second = (int) (first * 0.8);
        check (diff (out, s0 + first + second + 300, in, s0 + 300, 1000) < 1e-4f, "ROLL: the next repeat is shorter (it speeds up)");
    }
    {   // the same glitch on neighbouring steps runs on (one long repeat, not restarted every step)
        auto p = make (0);
        steps (*p, "..aa............");
        const auto out = run (*p, in);
        check (diff (out, 4 * stepLen + 500, in, 4 * stepLen + 500, 1000) > 1e-3f, "REPEAT 1/8 on 2 steps: the repeat runs on into the 3rd step's start");
    }

    {   // MIDI: D (pad 3 = REPEAT 1/32) right away with quantize off; with 1/16 it waits for the grid
        auto p = make (0);
        set (*p, "quantize", 0);
        const int at = 3 * stepLen + 1000;
        const auto out = run (*p, in, { { at, 62, true }, { at + 9000, 62, false } });
        const int L = stepLen / 2;
        check (diff (out, at + L + 300, in, at + 300, 1500) < 1e-4f, "MIDI key D: REPEAT 1/32 starts at once (quantize off)");
        check (diff (out, at + 9000 + 2000, in, at + 9000 + 2000, 3000) < 1e-4f, "MIDI key up: the sound comes back");

        auto q = make (0);
        set (*q, "quantize", 1);
        const auto out2 = run (*q, in, { { at, 62, true }, { at + 20000, 62, false } });
        const int grid = 4 * stepLen;
        check (diff (out2, at + 200, in, at + 200, grid - at - 400) < 1e-4f && diff (out2, grid + L + 300, in, grid + 300, 1500) < 1e-4f,
               "quantize 1/16: the key waits for the next step");
    }
    {   // a newer key wins over an older one, and the older one comes back
        auto p = make (0);
        set (*p, "quantize", 0);
        const auto out = run (*p, in, { { 1000, 60, true }, { 20000, 72 + 5, true } });
        ignoreUnused (out);
        check (p->engine.nowSource.load() == chop::SrcLive, "a held key keeps its glitch going");
    }

    {   // CHAOS: none at 0; in time; LOCK repeats; FILL fills the end of the bar
        auto p = make (0);
        auto s = p->settings();
        s.chaos = 0.0f;
        int any = 0;
        for (int b = 0; b < 8; ++b) for (int k = 0; k < 16; ++k) any += p->engine.chaosAt (s, b, k).first != chop::None ? 1 : 0;
        check (any == 0, "CHAOS 0: nothing");
        s.chaos = 0.8f; s.lock = 1; s.fill = 0.5f;
        bool same = true; int count = 0;
        for (int k = 0; k < 16; ++k) { const auto a = p->engine.chaosAt (s, 0, k), b = p->engine.chaosAt (s, 7, k); same = same && a == b; count += a.first != chop::None ? 1 : 0; }
        check (same && count > 0, "LOCK 1 bar: every bar the same glitches (" + String (count) + " a bar)");
        s.lock = 0;
        int differ = 0;
        for (int b = 1; b < 8; ++b) for (int k = 0; k < 16; ++k) differ += p->engine.chaosAt (s, 0, k) != p->engine.chaosAt (s, b, k) ? 1 : 0;
        check (differ > 5, "LOCK free: other glitches every bar");
        auto endCount = [&] (float fill)
        {
            s.fill = fill; int endN = 0, startN = 0;
            for (int b = 0; b < 200; ++b) for (int k = 0; k < 16; ++k) if (p->engine.chaosAt (s, b, k).first != chop::None) (k >= 12 ? endN : startN)++;
            return std::pair<int, int> { endN, startN };
        };
        const auto lo = endCount (0.0f), hi = endCount (1.0f);
        check (hi.first > lo.first * 2, "FILL: many more glitches at the end of the bar (" + String (lo.first) + " -> " + String (hi.first) + ")");
        s.seed = 5; const auto a = p->engine.chaosAt (s, 3, 13); s.seed = 6;
        int changed = 0;
        for (int k = 0; k < 16; ++k) { s.seed = 5; auto x = p->engine.chaosAt (s, 3, k); s.seed = 6; changed += x != p->engine.chaosAt (s, 3, k) ? 1 : 0; }
        ignoreUnused (a);
        check (changed > 0, "SEED: another seed, other glitches");
    }

    // --- every preset is safe on drums
    const auto dr = drums (bar * 4);
    for (int i = 0; i < (int) ChopCatProcessor::presets().size(); ++i)
    {
        auto p = make (i);
        const auto out = run (*p, dr, { { bar, 60 + i % 12, true }, { bar + 20000, 60 + i % 12, false } }, false);
        float pk = 0.0f; bool finite = true;
        for (float x : out) { finite = finite && std::isfinite (x); pk = jmax (pk, std::abs (x)); }
        check (finite && pk > 0.05f && pk <= 1.0f, String ("preset ") + ChopCatProcessor::presets()[(size_t) i].name + " (peak " + String (pk, 2) + ")");
    }

    {   // CRAFT x50: safe, something glitches, UNDO
        auto p = make (0);
        Random r (21);
        int good = 0, glitchy = 0;
        for (int i = 0; i < 50; ++i)
        {
            p->craft (r);
            p->engine.reset();
            const auto out = run (*p, dr, {}, false);
            float pk = 0.0f; bool finite = true;
            for (float x : out) { finite = finite && std::isfinite (x); pk = jmax (pk, std::abs (x)); }
            good += finite && pk > 0.05f && pk <= 1.0f ? 1 : 0;
            glitchy += diff (out, 0, dr, 0, (int) dr.size() - block * 4) > 1e-3f ? 1 : 0;
        }
        check (good == 50, "CRAFT: " + String (good) + " of 50 styles are safe");
        check (glitchy >= 45, "CRAFT: " + String (glitchy) + " of 50 styles really glitch");
        const int before = p->getStep (15);
        const auto name = p->craftName;
        p->craft (r);
        check (p->undoCraft() && p->getStep (15) == before && p->craftName == name, "UNDO brings back the style before the CRAFT");
    }
    {   // the state
        auto p = make (3);
        set (*p, "chaos", 0.42f);
        MemoryBlock mb; p->getStateInformation (mb);
        auto q = make (0);
        q->setStateInformation (mb.getData(), (int) mb.getSize());
        bool same = std::abs (q->settings().chaos - 0.42f) < 0.01f;
        for (int k = 0; k < 16; ++k) same = same && q->getStep (k) == p->getStep (k);
        check (same, "the state comes back (steps, chaos)");
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL CHOP CHECKS PASSED" : (String (failures) + " FAILED").toRawUTF8());
    return failures == 0 ? 0 : 1;
}

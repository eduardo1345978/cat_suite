// Hero-Cat's check: Chord Lock (scale chords, snapping, voice leading), strum, every sound plays, the arp (up, waterfall,
// pattern rests, latch, in time with the DAW), MIDI out, every preset is safe, the state (with the pattern) comes back.
#include "PluginProcessor.h"
#include <cstdio>
#include <set>
#include <algorithm>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

static constexpr double sr = 48000.0;
static constexpr int block = 256;

struct Head : AudioPlayHead
{
    double ppq = 0.0;
    bool playing = true;
    Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm (120.0); p.setIsPlaying (playing); p.setPpqPosition (ppq); return p; }
};

static std::unique_ptr<HeroCatProcessor> make (int preset = 0)
{
    auto p = std::make_unique<HeroCatProcessor>();
    p->setPlayConfigDetails (0, 2, sr, block);
    p->applyPreset (preset);
    p->prepareToPlay (sr, block);
    return p;
}

static void set (HeroCatProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

struct Run { float peak = 0.0f; bool finite = true; std::vector<std::pair<double, int>> noteOns; int noteOffs = 0; };

/** notes = { time, note, on } */
static Run run (HeroCatProcessor& p, double seconds, std::vector<std::tuple<double, int, bool>> notes, bool playing = true)
{
    Run res;
    AudioBuffer<float> buf (2, block);
    Head head; head.playing = playing;
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
        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            if (msg.isNoteOn()) res.noteOns.push_back ({ (pos + m.samplePosition) / sr, msg.getNoteNumber() });
            else if (msg.isNoteOff()) ++res.noteOffs;
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i)
            {
                const float x = buf.getSample (c, i);
                if (! std::isfinite (x)) res.finite = false;
                res.peak = jmax (res.peak, std::abs (x));
            }
    }
    p.setPlayHead (nullptr);
    return res;
}

static String list (const std::vector<int>& v) { String s; for (int x : v) s << x << " "; return s.trim(); }

int main()
{
    ScopedJuceInitialiser_GUI gui;

    // HERO_WAV=<file> [HERO_PRESET=n] [HERO_SEED=n]: write 2 rounds of a QUEST song to a WAV (to listen to) and stop
    if (auto wav = SystemStats::getEnvironmentVariable ("HERO_WAV", {}); wav.isNotEmpty())
    {
        auto p = make (SystemStats::getEnvironmentVariable ("HERO_PRESET", "0").getIntValue());
        p->questSeed = SystemStats::getEnvironmentVariable ("HERO_SEED", "1").getIntValue();
        set (*p, "quest", 1.0f);
        File f (wav); f.deleteFile();
        WavAudioFormat fmt;
        std::unique_ptr<AudioFormatWriter> w (fmt.createWriterFor (f.createOutputStream().release(), sr, 2, 16, {}, 0));
        AudioBuffer<float> buf (2, block);
        Head head;
        p->setPlayHead (&head);
        for (int pos = 0; pos < (int) (34.0 * sr); pos += block)
        {
            head.ppq = pos / sr * 2.0;
            MidiBuffer midi;
            p->processBlock (buf, midi);
            w->writeFromAudioSampleBuffer (buf, 0, block);
        }
        p->setPlayHead (nullptr);
        std::printf ("wrote %s\n", wav.toRawUTF8());
        return 0;
    }

    // --- Chord Lock
    {
        auto p = make();
        auto s = p->settings();
        s.chordMode = 1; s.key = 0; s.scale = 0; s.voiceLead = false;
        check (p->engine.chordFor (62, s, {}) == std::vector<int> { 62, 65, 69 }, "C major triads: D becomes Dm (62 65 69)");
        check (p->engine.chordFor (61, s, {}) == std::vector<int> { 60, 64, 67 }, "C# is not in C major: it snaps to C (60 64 67)");
        s.chordMode = 2;
        check (p->engine.chordFor (67, s, {}) == std::vector<int> { 67, 71, 74, 77 }, "7ths: G becomes G7 (" + list (p->engine.chordFor (67, s, {})) + ")");
        s.chordMode = 3;
        check (p->engine.chordFor (60, s, {}) == std::vector<int> { 60, 64, 67, 74 }, "anime add9: C becomes Cadd9 (" + list (p->engine.chordFor (60, s, {})) + ")");
        s.chordMode = 1; s.scale = 1; s.key = 9;
        check (p->engine.chordFor (69, s, {}) == std::vector<int> { 69, 72, 76 }, "A minor: A becomes Am");
        s.chordMode = 7;
        check (p->engine.chordFor (40, s, {}) == std::vector<int> { 40, 47, 52 }, "Power chord: E 5 + octave");
        // voice leading: C then F - the upper notes stay close
        s.chordMode = 1; s.scale = 0; s.key = 0; s.voiceLead = true;
        const auto c = p->engine.chordFor (60, s, {});
        const auto f = p->engine.chordFor (65, s, c);
        int far = 0;
        for (size_t i = 1; i < f.size(); ++i) { int d = 99; for (int x : c) d = jmin (d, std::abs (f[i] - x)); far = jmax (far, d); }
        check (f[0] == 65 && far <= 5, "voice leading: F after C keeps the bass and every upper note near the last chord (" + list (f) + ")");
    }

    // --- every sound plays a held key
    for (int src = 0; src < hero::numSources; ++src)
    {
        auto p = make();
        set (*p, "srcA", (float) src);
        const auto r = run (*p, 1.0, { { 0.0, 60, true }, { 0.7, 60, false } });
        check (r.finite && r.peak > 0.02f && r.peak <= 1.0f, hero::sourceNames()[src] + " plays (peak " + String (r.peak, 2) + ")");
    }
    {   // and it stops after the release
        auto p = make();
        set (*p, "reverb", 0.0f); set (*p, "delay", 0.0f);
        run (*p, 1.0, { { 0.0, 60, true }, { 0.2, 60, false } });
        const auto r = run (*p, 0.5, {});
        check (r.peak < 0.01f && p->engine.voicesSounding() == 0, "the note ends after its release");
    }

    // --- strum: the chord's notes one after another, also as MIDI out
    {
        auto p = make();
        set (*p, "chordMode", 1); set (*p, "strum", 1.0f); set (*p, "reverb", 0.0f); set (*p, "delay", 0.0f);
        const auto r = run (*p, 0.6, { { 0.1, 60, true } });
        bool spaced = r.noteOns.size() == 3;
        for (size_t i = 1; spaced && i < r.noteOns.size(); ++i) spaced = std::abs ((r.noteOns[i].first - r.noteOns[i - 1].first) - 0.12) < 0.01;
        check (spaced, "strum: 3 MIDI notes 120 ms apart (" + String ((int) r.noteOns.size()) + " notes)");
        set (*p, "midiOut", 0.0f);
        const auto r2 = run (*p, 0.4, { { 0.0, 64, true } });
        check (r2.noteOns.empty() && r2.peak > 0.01f, "MIDI OUT off: it plays but sends nothing");
    }

    // --- the arp
    {
        auto p = make();
        set (*p, "chordMode", 1); set (*p, "arpMode", hero::ArpUp); set (*p, "rate", 2);    // 1/16 at 120 = 8 notes a second
        const auto r = run (*p, 1.0, { { 0.0, 60, true } });
        bool up = r.noteOns.size() >= 4 && r.noteOns[0].second == 60 && r.noteOns[1].second == 64 && r.noteOns[2].second == 67 && r.noteOns[3].second == 60;
        check (std::abs ((int) r.noteOns.size() - 8) <= 1, "arp 1/16 at 120 BPM: 8 notes a second (" + String ((int) r.noteOns.size()) + ")");
        check (up, "arp Up: C E G C ...");
        bool onGrid = true;
        for (auto& [t, n] : r.noteOns) onGrid = onGrid && std::abs (t * 8.0 - std::round (t * 8.0)) < 0.002;
        check (onGrid, "arp notes land on the DAW's 1/16 grid");

        set (*p, "arpMode", hero::ArpWaterfall);
        const auto w = run (*p, 0.6, { { 0.0, 60, true } });
        check (w.noteOns.size() >= 3 && w.noteOns[0].second == 67 && w.noteOns[1].second == 64 && w.noteOns[2].second == 60, "waterfall falls from the top: G E C");

        set (*p, "arpMode", hero::ArpPattern);
        for (auto& x : p->engine.pattern) x = 0.0f;
        p->engine.pattern[0] = 1.0f; p->engine.pattern[8] = 1.0f;
        const auto pt = run (*p, 2.0, { { 0.0, 60, true } });
        check (pt.noteOns.size() == 2, "pattern: only the 2 lit steps play (2 s = 1 bar: " + String ((int) pt.noteOns.size()) + " notes)");

        set (*p, "arpMode", hero::ArpUp); set (*p, "latch", 1.0f);
        const auto l = run (*p, 1.0, { { 0.0, 60, true }, { 0.1, 60, false } });
        check (l.noteOns.size() >= 7, "latch: the arp keeps going after the key is up (" + String ((int) l.noteOns.size()) + " notes)");

        set (*p, "latch", 0.0f); set (*p, "chaos", 1.0f);
        const auto ch = run (*p, 4.0, { { 0.0, 60, true } });
        check (ch.finite && ch.noteOns.size() > 10, "chaos: still plays, and differently (" + String ((int) ch.noteOns.size()) + " notes)");

        set (*p, "chaos", 0.0f);
        const auto own = run (*p, 1.0, { { 0.0, 62, true } }, false);
        check (own.noteOns.size() >= 7, "the arp runs on its own clock when the DAW is stopped");
    }

    // --- every preset is safe (one chord, held)
    for (int i = 0; i < (int) HeroCatProcessor::presets().size(); ++i)
    {
        auto p = make (i);
        const auto r = run (*p, 2.5, { { 0.0, 60, true }, { 0.0, 64, true }, { 0.0, 67, true }, { 2.0, 60, false }, { 2.0, 64, false }, { 2.0, 67, false } });
        check (r.finite && r.peak > 0.02f && r.peak <= 1.0f, String ("preset ") + HeroCatProcessor::presets()[(size_t) i].name + " (peak " + String (r.peak, 2) + ")");
    }

    // --- textures sound without notes
    for (int t = 1; t < hero::textureNames().size(); ++t)
    {
        auto p = make();
        set (*p, "texture", (float) t); set (*p, "texLevel", 1.0f);
        if (t == 4) run (*p, 0.3, { { 0.0, 60, true }, { 0.1, 60, false } });
        const auto r = run (*p, 20.0, {});
        check (r.finite && r.peak > 0.002f && r.peak < 1.0f, "texture " + hero::textureNames()[t] + " (peak " + String (r.peak, 3) + ")");
    }

    // --- CRAFT: many new sounds, every one plays safely; UNDO goes back
    {
        auto p = make();
        Random r (77);
        int good = 0, n = 60;
        std::set<String> names;
        for (int i = 0; i < n; ++i)
        {
            names.insert (p->craft (r));
            p->engine.reset();
            const auto res = run (*p, 1.2, { { 0.0, 60, true }, { 0.0, 64, true }, { 1.0, 60, false }, { 1.0, 64, false } });
            if (res.finite && res.peak > 0.01f && res.peak <= 1.0f) ++good;
            else std::printf ("     craft %d: %s peak %.3f\n", i, p->craftName.toRawUTF8(), res.peak);
        }
        check (good == n, "CRAFT: " + String (good) + " of " + String (n) + " new sounds play safely");
        check ((int) names.size() > n / 2, "CRAFT names differ (" + String ((int) names.size()) + " names)");
        const auto before = p->settings().srcA;
        const auto name = p->craftName;
        p->craft (r);
        check (p->undoCraft() && p->settings().srcA == before && p->craftName == name && ! p->canUndo(), "UNDO brings back the sound before the CRAFT");
    }

    // --- QUEST: a whole theme by itself, in the scale, the same seed = the same song, another seed = another song
    {
        int inScale = 0, total = 0;
        for (int seed = 1; seed <= 40; ++seed)
            for (int scale : { 0, 1, 6 })
            {
                const auto song = hero::Engine::makeSong (seed, 2, scale);
                for (auto& n : song)
                {
                    static const std::vector<std::vector<int>> sc { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, {}, {}, {}, {}, { 0, 1, 5, 7, 8 } };
                    const int pc = ((n.note - 2) % 12 + 12) % 12;
                    ++total; inScale += std::find (sc[(size_t) scale].begin(), sc[(size_t) scale].end(), pc) != sc[(size_t) scale].end() ? 1 : 0;
                }
            }
        check (total > 0 && inScale == total, "QUEST songs: every note is in the scale (" + String (inScale) + " / " + String (total) + ")");
        auto a = hero::Engine::makeSong (5, 0, 0), b = hero::Engine::makeSong (5, 0, 0), c = hero::Engine::makeSong (6, 0, 0);
        auto same = [] (const std::vector<hero::Engine::SongNote>& x, const std::vector<hero::Engine::SongNote>& y)
        { if (x.size() != y.size()) return false; for (size_t i = 0; i < x.size(); ++i) if (x[i].note != y[i].note || x[i].step != y[i].step) return false; return true; };
        check (same (a, b) && ! same (a, c), "QUEST: the same seed makes the same song, another seed another one");
        int melody = 0, lastStep = -1; bool endsHome = false;
        for (auto& n : a) if (! n.chord) { ++melody; if (n.step > lastStep) { lastStep = n.step; endsHome = n.note % 12 == 0; } }
        check (melody >= 24 && endsHome, "QUEST: a real melody (" + String (melody) + " notes) that ends on the key note");

        auto p = make();
        set (*p, "quest", 1.0f); set (*p, "reverb", 0.0f);
        const auto r = run (*p, 16.0, {});                              // 8 bars at 120 BPM
        check (r.finite && r.peak > 0.02f && r.peak <= 1.0f && r.noteOns.size() >= 40, "QUEST plays by itself: " + String ((int) r.noteOns.size()) + " notes in 8 bars");
        set (*p, "quest", 0.0f);
        run (*p, 1.0, {});
        const auto quiet = run (*p, 1.0, {});
        check (quiet.noteOns.empty(), "QUEST off: it stops");
    }

    // --- the state comes back, with the pattern
    {
        auto p = make();
        set (*p, "srcA", 6); set (*p, "chaos", 0.4f);
        p->engine.pattern[3] = 0.25f;
        MemoryBlock mb; p->getStateInformation (mb);
        auto q = make (5);
        q->setStateInformation (mb.getData(), (int) mb.getSize());
        check (q->settings().srcA == 6 && std::abs (q->settings().chaos - 0.4f) < 0.01f && std::abs (q->engine.pattern[3].load() - 0.25f) < 0.01f,
               "the state (and the drawn pattern) comes back");
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL HERO CHECKS PASSED" : (String (failures) + " FAILED").toRawUTF8());
    return failures == 0 ? 0 : 1;
}

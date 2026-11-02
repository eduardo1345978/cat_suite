// Micro-Cat's check: the tunings are right (cents and keys), the MIDI out is correct MPE (set-up,
// one channel per note, the bend before the note, note-offs on the same channel), Scala files and the
// saved project keep everything.
#include "PluginProcessor.h"
#include <cstdio>

using namespace juce;
using namespace micro;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

static const Tuning* find (const String& name)
{
    for (auto& t : library()) if (t.name == name) return &t;
    return nullptr;
}

static double centsOf (const KeyPitch& k, double rootHz) { return 1200.0 * std::log2 (k.hz / rootHz); }

int main()
{
    ScopedJuceInitialiser_GUI init;
    const double C4 = MicroCatProcessor::concertHz (60);

    // the library
    {
        const auto& lib = library();
        bool allValid = true, unique = true;
        std::array<int, NumCategories> perCat {};
        for (size_t i = 0; i < lib.size(); ++i)
        {
            String why;
            if (! lib[i].isValid (&why)) { allValid = false; std::printf ("   invalid: %s (%s)\n", lib[i].name.toRawUTF8(), why.toRawUTF8()); }
            for (size_t j = i + 1; j < lib.size(); ++j) unique = unique && lib[i].name != lib[j].name;
            ++perCat[(size_t) lib[i].categoryId];
        }
        check (lib.size() >= 35 && allValid && unique, "library: " + String ((int) lib.size()) + " tunings, all valid, names unique");
        check (perCat[CatEqual] >= 10 && perCat[CatJust] >= 8 && perCat[CatWorld] >= 8,
               "library: equal " + String (perCat[CatEqual]) + ", just & historical " + String (perCat[CatJust]) + ", world " + String (perCat[CatWorld]));
    }

    // pitches
    {
        auto d12 = TuningData::from (*find ("12-EDO (normal)"));
        bool same = true;
        for (int k = 0; k < 128; ++k) { auto kp = pitchFor (d12, k, 60, C4); same = same && kp.valid && kp.note == k && std::abs (kp.bend) < 1e-9; }
        check (same, "12-EDO plays every key as it is (no bend)");

        auto d24 = TuningData::from (*find ("24-EDO"));
        auto q = pitchFor (d24, 61, 60, C4);
        check (std::abs (centsOf (q, C4) - 50.0) < 1e-6 && std::abs ((q.note + q.bend) - 60.5) < 1e-9, "24-EDO: the next key is a quarter tone up");
        auto d19 = TuningData::from (*find ("19-EDO"));
        check (std::abs (centsOf (pitchFor (d19, 79, 60, C4), C4) - 1200.0) < 1e-6, "19-EDO: 19 keys up is an octave");
        auto d53 = TuningData::from (*find ("53-EDO"));
        check (std::abs (centsOf (pitchFor (d53, 60 - 53, 60, C4), C4) + 1200.0) < 1e-6, "53-EDO: 53 keys down is an octave down");

        auto ji = TuningData::from (*find ("Just intonation (5-limit)"));
        auto e = pitchFor (ji, 64, 60, C4), g = pitchFor (ji, 67, 60, C4), e5 = pitchFor (ji, 76, 60, C4);
        check (ji.mapping == MapTwelve && e.note == 64 && std::abs (e.bend * 100.0 + 13.686) < 0.01 && std::abs (g.bend * 100.0 - 1.955) < 0.01
               && std::abs (centsOf (e5, C4) - 1586.314) < 0.01,
               "just intonation: E is 13.7 cents low, G 2 cents high, an octave up the same");
        auto bp = TuningData::from (*find ("Bohlen-Pierce"));
        check (std::abs (centsOf (pitchFor (bp, 73, 60, C4), C4) - 1901.955) < 0.01, "Bohlen-Pierce: 13 keys up is a tritave (3:1), not an octave");
        auto rast = TuningData::from (*find ("Maqam Rast (on C)"));
        check (std::abs (pitchFor (rast, 64, 60, C4).bend * 100.0 + 50.0) < 1e-6 && std::abs (pitchFor (rast, 71, 60, C4).bend * 100.0 + 50.0) < 1e-6
               && std::abs (pitchFor (rast, 62, 60, C4).bend) < 1e-9, "maqam Rast: E and B a quarter tone flat, D as it is");
        auto five = TuningData::from (*find ("5-EDO"));
        check (! pitchFor (five, 127, 60, C4).valid && pitchFor (five, 70, 60, C4).valid, "5-EDO: keys far above MIDI's range stay silent instead of wrapping");
        check (std::abs (pitchFor (d12, 69, 69, 432.0).hz - 432.0) < 1e-9, "the root's frequency is free (A = 432 Hz)");
    }

    // the plug-in's MIDI out
    {
        MicroCatProcessor p;
        p.setPlayConfigDetails (0, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        p.applyPreset ((int) (find ("Just intonation (5-limit)") - library().data()));
        AudioBuffer<float> audio (2, 512);
        MidiBuffer in;
        in.addEvent (MidiMessage::noteOn (1, 60, (uint8) 100), 10);
        in.addEvent (MidiMessage::noteOn (1, 64, (uint8) 100), 20);
        in.addEvent (MidiMessage::noteOn (1, 67, (uint8) 100), 30);
        in.addEvent (MidiMessage::controllerEvent (1, 64, 127), 40);
        in.addEvent (MidiMessage::aftertouchChange (1, 64, 90), 50);
        p.processBlock (audio, in);
        bool config = false;
        std::map<int, int> noteChannel, bendBefore;
        std::map<int, int> lastBend;
        bool sustainOnMaster = false, pressureOnNote = false, bendFirst = true;
        for (const auto meta : in)
        {
            const auto m = meta.getMessage();
            if (m.isController() && m.getChannel() == 1 && m.getControllerNumber() == 100 && m.getControllerValue() == 6) config = true;
            if (m.isPitchWheel()) lastBend[m.getChannel()] = m.getPitchWheelValue();
            if (m.isNoteOn())
            {
                noteChannel[m.getNoteNumber()] = m.getChannel();
                if (lastBend.count (m.getChannel()) == 0) bendFirst = false;
                else bendBefore[m.getNoteNumber()] = lastBend[m.getChannel()];
            }
            if (m.isController() && m.getControllerNumber() == 64 && m.getChannel() == 1) sustainOnMaster = true;
            if (m.isChannelPressure() && noteChannel.count (64) && m.getChannel() == noteChannel[64] && m.getChannelPressureValue() == 90) pressureOnNote = true;
        }
        std::set<int> chans;
        for (auto& [n, c] : noteChannel) chans.insert (c);
        check (config, "MIDI out: the MPE set-up (Configuration Message on channel 1) goes out first");
        check (noteChannel.size() == 3 && chans.size() == 3 && *chans.begin() >= 2 && bendFirst, "MIDI out: every note on its own channel (2-16), its bend sent before it");
        const int expectE = Engine::bendValue (-0.13686, 48.0);
        check (bendBefore.count (64) && std::abs (bendBefore[64] - expectE) <= 1 && std::abs (bendBefore[60] - 8192) <= 1,
               "MIDI out: E's bend is -13.7 cents at a 48 semitone range (" + String (bendBefore.count (64) ? bendBefore[64] : -1) + ")");
        check (sustainOnMaster && pressureOnNote, "MIDI out: sustain on the master channel, aftertouch on the note's own channel");

        MidiBuffer off;
        off.addEvent (MidiMessage::noteOff (1, 64), 5);
        p.processBlock (audio, off);
        bool offOk = false;
        for (const auto meta : off) { const auto m = meta.getMessage(); if (m.isNoteOff() && m.getNoteNumber() == 64 && m.getChannel() == noteChannel[64]) offOk = true; }
        check (offOk, "MIDI out: the note-off goes to the note's channel");

        // fewer voices: the oldest note gives its channel up
        if (auto* v = p.apvts.getParameter ("voices")) v->setValueNotifyingHost (v->convertTo0to1 (2.0f));
        MidiBuffer two;
        two.addEvent (MidiMessage::noteOn (1, 60, (uint8) 90), 0);
        two.addEvent (MidiMessage::noteOn (1, 62, (uint8) 90), 1);
        two.addEvent (MidiMessage::noteOn (1, 64, (uint8) 90), 2);
        p.processBlock (audio, two);
        int ons = 0, offs = 0;
        std::set<int> used;
        for (const auto meta : two) { const auto m = meta.getMessage(); if (m.isNoteOn()) { ++ons; used.insert (m.getChannel()); } if (m.isNoteOff() && m.getNoteNumber() == 60) ++offs; }
        check (ons == 3 && offs == 1 && used.size() == 2, "voices: with 2 voices a 3rd note takes the oldest note's channel (that note ends)");

        // bypass: as it came in
        if (auto* b = p.apvts.getParameter ("bypass")) b->setValueNotifyingHost (1.0f);
        MidiBuffer by;
        by.addEvent (MidiMessage::noteOn (1, 64, (uint8) 80), 0);
        p.processBlock (audio, by);
        bool plain = false, bent = false;
        for (const auto meta : by) { const auto m = meta.getMessage(); if (m.isNoteOn() && m.getChannel() == 1 && m.getNoteNumber() == 64) plain = true; if (m.isPitchWheel() && m.getPitchWheelValue() != 8192) bent = true; }
        check (plain && ! bent, "bypass: notes go out as they came in");
    }

    // per-step mapping through the plug-in: 31-EDO, the next key is 38.7 cents up
    {
        MicroCatProcessor p;
        p.setPlayConfigDetails (0, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        p.applyPreset ((int) (find ("31-EDO") - library().data()));
        AudioBuffer<float> audio (2, 512);
        MidiBuffer in;
        in.addEvent (MidiMessage::noteOn (1, 61, (uint8) 100), 0);
        p.processBlock (audio, in);
        int note = -1, bend = -1;
        for (const auto meta : in) { const auto m = meta.getMessage(); if (m.isPitchWheel()) bend = m.getPitchWheelValue(); if (m.isNoteOn()) note = m.getNoteNumber(); }
        check (note == 60 && std::abs (bend - Engine::bendValue (1200.0 / 31.0 / 100.0, 48.0)) <= 1, "31-EDO: key C#4 plays C4 + 38.7 cents");
    }

    // Scala files
    {
        Tuning t;
        String err;
        const bool ok = parseScl ("! test\nA test scale\n 4\n!\n 9/8\n 386.3137\n 3/2\n 2\n", t, &err);
        check (ok && t.cents.size() == 4 && std::abs (t.cents[0] - 203.91) < 0.01 && std::abs (t.cents[3] - 1200.0) < 1e-9, "Scala: reads ratios and cents (" + err + ")");
        Tuning back;
        const auto& src = *find ("Werckmeister III");
        const bool rt = parseScl (toScl (src), back);
        bool same = rt && back.cents.size() == src.cents.size();
        for (size_t i = 0; same && i < src.cents.size(); ++i) same = std::abs (back.cents[i] - src.cents[i]) < 1e-4;
        check (same && back.mapping == MapTwelve, "Scala: a tuning saved and read again is the same (12 notes = 12-key layout)");
        check (! parseScl ("bad\n 3\n 100.0\n 50.0\n 1200.0\n", back), "Scala: notes going down are refused");
    }

    // a saved project keeps an edited tuning and the root
    {
        MicroCatProcessor a, b;
        Tuning t = *find ("19-EDO");
        t.name = "My 19"; t.cents[4] = 333.0; t.categoryId = CatMine;
        a.setTuning (t, true);
        a.setRoot (57, 220.0);
        MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        const auto bt = b.getTuning();
        check (bt.name == "My 19" && bt.cents.size() == 19 && std::abs (bt.cents[4] - 333.0) < 1e-6 && b.currentSettings().rootKey == 57
               && std::abs (b.currentSettings().rootHz - 220.0) < 0.01, "a saved project keeps an edited tuning and the root (A3 = 220 Hz)");
    }

    std::printf (failures == 0 ? "MICRO CHECK PASSED\n" : "MICRO CHECK FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}

// Micro-Cat's check: the tunings are right (cents and keys), the MIDI out is correct MPE (set-up,
// one channel per note, the bend before the note, note-offs on the same channel), Scala files and the
// saved project keep everything.
#include "PluginProcessor.h"
#include <cstdio>
#include <thread>
#include "LinkClient.h"
#include "../ThirdParty/MTS-ESP/Client/libMTSClient.h"

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

/** A follower (Glide / Sing / Talk-Cat stand-in) that listens on Cat Link. */
struct Follower : AudioProcessor
{
    Follower() : apvts (*this, nullptr, "PARAMS", {}) {}
    const String getName() const override { return "Glide-Cat"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}
    void getStateInformation (MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    AudioProcessorValueTreeState apvts;
};

static void onMessageThread (std::function<void()> f)
{
    WaitableEvent done;
    MessageManager::callAsync ([&] { f(); done.signal(); });
    done.wait();
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

    // Ableton .ascl: a Scala file with @ABL lines
    {
        const String ascl = "! 22-EDO.ascl\n!\n22 equal steps\n 22\n!\n"
                            + [] { String s; for (int i = 1; i <= 22; ++i) s << " " << String (1200.0 * i / 22.0, 5) << "\n"; return s; }()
                            + "! @ABL NOTE_NAMES " + [] { String s; for (int i = 0; i < 22; ++i) s << "\"n " << i << "\" "; return s; }() + "\n"
                            + "! @ABL REFERENCE_PITCH 4 3 300.0\n!! @ABL REFERENCE_PITCH 4 0 999.0\n! @ABL SOURCE Somebody\n";
        Tuning t; String err;
        const bool ok = parseScl (ascl, t, &err);
        const double first = 300.0 / std::pow (2.0, 3.0 * 1200.0 / 22.0 / 1200.0);
        check (ok && t.cents.size() == 22 && std::abs (t.referenceHz - first) < 1e-6 && t.noteNames.size() == 22 && t.noteNames[1] == "n 1",
               ".ascl: reads the notes, the names and the reference pitch (degree 3 = 300 Hz; a !! line is off) " + err);
        int key = 0; double hz = 0.0;
        const bool root = rootFromReference (t, key, hz);
        check (root && std::abs (hz - first) < 1e-6 && key == 60, ".ascl: the root lands near middle C (" + String (hz, 2) + " Hz on key " + String (key) + ")");
        Tuning plain;
        int k2 = 0; double h2 = 0.0;
        Tuning ji12;
        String a12 = "ji\n 12\n"; for (auto r : { "16/15", "9/8", "6/5", "5/4", "4/3", "45/32", "3/2", "8/5", "5/3", "9/5", "15/8", "2/1" }) a12 << " " << r << "\n";
        a12 << "! @ABL REFERENCE_PITCH 3 9 432.0\n";       // A = 432 Hz: C is a just major sixth below
        int k3 = 0; double h3 = 0.0;
        const bool r12 = parseScl (a12, ji12) && rootFromReference (ji12, k3, h3);
        check (r12 && k3 == 60 && std::abs (h3 - 432.0 * 3.0 / 5.0) < 1e-6,
               ".ascl, 12 notes: A4 = 432 Hz puts C4 at " + String (h3, 2) + " Hz on key " + String (k3));
        check (parseScl ("x\n 2\n 600.0\n 2/1\n", plain) && ! rootFromReference (plain, k2, h2), ".scl without @ABL lines: no reference pitch, the root stays");
    }

    // the pitch every key plays (what Glide / Sing / Talk-Cat get over Cat Link)
    {
        MicroCatProcessor m;
        m.setTuning (*find ("24-EDO"));
        m.setRoot (60, C4);
        const auto kp = m.keyPitches();
        check (std::abs (kp[60] - 60.0f) < 1e-3f && std::abs (kp[61] - 60.5f) < 1e-3f && std::abs (kp[84] - 72.0f) < 1e-3f,
               "Cat Link pitches: 24-EDO, key C#4 plays 60.5 (a quarter tone), 24 keys up an octave");
        m.setTuning (*find ("Just intonation (5-limit)"));
        const auto ji = m.keyPitches();
        check (std::abs ((ji[64] - 64.0f) * 100.0f + 13.686f) < 0.02f, "Cat Link pitches: just E4 is 13.7 cents low");
    }

    // a real Micro-Cat sends its tuning, a follower gets it (needs the message loop: the Cat Link timers)
    // (one worker thread for all of these: JUCE's dispatch loop can't be started again once stopped)
    {
        std::thread worker ([C4]
        {
        {
            std::unique_ptr<MicroCatProcessor> m;
            std::unique_ptr<Follower> f;
            std::unique_ptr<cat::LinkClient> fl;
            onMessageThread ([&]
            {
                m = std::make_unique<MicroCatProcessor>();
                m->setTuning (*find ("24-EDO"));
                f = std::make_unique<Follower>();
                cat::LinkClient::Caps caps; caps.receiveTuning = true;
                fl = std::make_unique<cat::LinkClient> (*f, f->apvts, "Glide-Cat", caps);
            });
            Thread::sleep (600);
            const float t61 = fl->tuned (61.0f), t60_25 = fl->tuned (60.25f);
            check (fl->followingTuning() && std::abs (t61 - 60.5f) < 1e-3f && std::abs (t60_25 - 60.125f) < 1e-3f,
                   "Cat Link: the follower plays in Micro-Cat's 24-EDO (key 61 -> " + String (t61, 3) + ", a bend between keys too)");
            onMessageThread ([&] { if (auto* prm = m->apvts.getParameter ("bypass")) prm->setValueNotifyingHost (1.0f); });
            Thread::sleep (600);
            check (! fl->followingTuning() && std::abs (fl->tuned (61.0f) - 61.0f) < 1e-6f, "Cat Link: Micro-Cat bypassed, the follower is back in normal tuning");
            onMessageThread ([&] { if (auto* prm = m->apvts.getParameter ("bypass")) prm->setValueNotifyingHost (0.0f); });
            Thread::sleep (600);
            const bool again = fl->followingTuning();
            onMessageThread ([&] { fl->setOption (cat::LinkClient::Tuning, false); });
            Thread::sleep (300);
            const bool off = ! fl->followingTuning() && std::abs (fl->tuned (61.0f) - 61.0f) < 1e-6f;
            onMessageThread ([&] { fl->setOption (cat::LinkClient::Tuning, true); });
            Thread::sleep (300);
            onMessageThread ([&] { m.reset(); });
            Thread::sleep (600);
            check (again && off && ! fl->followingTuning(), "Cat Link: the follower can switch it off; closing Micro-Cat ends it");
            onMessageThread ([&] { fl.reset(); f.reset(); });
        }

        // MTS-ESP master: a synth (an MTS-ESP client) gets the tuning; only one master; without the library it says so
        {
            const bool lib = micro::MtsMaster::libraryInstalled();
            std::printf ("      (MTS-ESP library %s)\n", lib ? "installed" : "not installed");
            std::unique_ptr<MicroCatProcessor> a, b;
            auto setMts = [] (MicroCatProcessor& m, bool on) { if (auto* prm = m.apvts.getParameter ("mtsEsp")) prm->setValueNotifyingHost (on ? 1.0f : 0.0f); };
            onMessageThread ([&] { a = std::make_unique<MicroCatProcessor>(); a->setTuning (*find ("24-EDO")); setMts (*a, true); });
            Thread::sleep (500);
            auto playOne = [] (MicroCatProcessor& m)        // E4 in: what goes out (the note, and a bend before it?)
            {
                m.prepareToPlay (48000.0, 256);
                AudioBuffer<float> buf (2, 256);
                MidiBuffer mb; mb.addEvent (MidiMessage::noteOn (1, 64, (uint8) 100), 10);
                m.processBlock (buf, mb);
                int bends = 0, note = -1;
                for (auto e : mb) { const auto msg = e.getMessage(); if (msg.isPitchWheel()) ++bends; if (msg.isNoteOn()) note = msg.getNoteNumber(); }
                return std::make_pair (note, bends);
            };
            if (! lib)
            {
                const auto [note, bends] = playOne (*a);
                check (a->mts.state() == micro::MtsMaster::State::NoLibrary && a->mts.statusText().contains ("not installed") && bends > 0,
                       "MTS-ESP without the library: says it's not installed, the MPE out keeps working");
            }
            else
            {
                MTSClient* client = MTS_RegisterClient();
                const double f61 = MTS_NoteToFrequency (client, 61, -1), f84 = MTS_NoteToFrequency (client, 84, -1);
                check (a->mts.state() == micro::MtsMaster::State::Connected && MTS_HasMaster (client)
                       && std::abs (f61 - C4 * std::pow (2.0, 1.0 / 24.0)) < 1e-3 && std::abs (f84 - C4 * 2.0) < 1e-3,
                       "MTS-ESP: a synth gets 24-EDO (key 61 = " + String (f61, 3) + " Hz, 24 keys up = an octave)");
                check (String (MTS_GetScaleName (client)) == "24-EDO" && a->mts.numClients() >= 1, "MTS-ESP: the scale's name and the synth count");
                const auto [note, bends] = playOne (*a);
                check (note == 64 && bends == 0, "MTS-ESP on: the MIDI out plays the notes as they are (the synths tune themselves)");
                onMessageThread ([&] { if (auto* prm = a->apvts.getParameter ("bypass")) prm->setValueNotifyingHost (1.0f); });
                Thread::sleep (400);
                check (std::abs (MTS_NoteToFrequency (client, 61, -1) - MicroCatProcessor::concertHz (61)) < 1e-3, "MTS-ESP: bypass sends normal tuning");
                onMessageThread ([&] { if (auto* prm = a->apvts.getParameter ("bypass")) prm->setValueNotifyingHost (0.0f); });

                // a second Micro-Cat can't be the master too; it takes over when the first one closes
                onMessageThread ([&] { b = std::make_unique<MicroCatProcessor>(); b->setTuning (*find ("19-EDO")); setMts (*b, true); });
                Thread::sleep (500);
                const bool secondWaits = b->mts.state() == micro::MtsMaster::State::OtherMaster && std::abs (MTS_NoteToFrequency (client, 61, -1) - f61) < 1e-3;
                onMessageThread ([&] { a.reset(); });
                Thread::sleep (1600);
                const double f19 = MTS_NoteToFrequency (client, 61, -1);
                check (secondWaits && b->mts.state() == micro::MtsMaster::State::Connected && std::abs (f19 - C4 * std::pow (2.0, 1.0 / 19.0)) < 1e-3,
                       "MTS-ESP: one master at a time; the second Micro-Cat takes over when the first closes (19-EDO now)");
                onMessageThread ([&] { setMts (*b, false); });
                Thread::sleep (400);
                check (! MTS_HasMaster (client) && std::abs (MTS_NoteToFrequency (client, 61, -1) - MicroCatProcessor::concertHz (61)) < 1e-3,
                       "MTS-ESP switched off: the synths are back in normal tuning");
                MTS_DeregisterClient (client);
            }
            onMessageThread ([&] { a.reset(); b.reset(); });
        }
            MessageManager::getInstance()->stopDispatchLoop();
        });
        MessageManager::getInstance()->runDispatchLoop();
        worker.join();
    }

    std::printf (failures == 0 ? "MICRO CHECK PASSED\n" : "MICRO CHECK FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}

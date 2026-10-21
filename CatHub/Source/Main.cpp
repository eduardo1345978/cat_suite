// Cat Hub: one window that plays Chord-Cat, Bass-Cat, Drum-Cat and Life-Cat together.
//
//   Cat-Hub                                     normal app
//   Cat-Hub --selftest <catsFolder> <out.wav> [seconds]
//                                               renders a jam offline, writes the WAV and
//                                               <out.wav>.txt, exit code 0 = every Cat was heard
//   Cat-Hub --snapshot <catsFolder> <out.png>   pictures of the window (tutorial, simple, advanced,
//                                               settings, every theme)
//   Cat-Hub --uistress <catsFolder> <seconds> [seed]
//                                               the real window, random clicks / keys / tabs /
//                                               play / record / save + load while audio runs
#include "HubWindow.h"
#include <set>
#include <thread>

namespace hub
{
static int runSelfTest (const juce::StringArray& args)
{
    const int at = args.indexOf ("--selftest");
    const juce::File catsDir (args[at + 1]);
    const juce::File out (args[at + 2]);
    const double seconds = args.size() > at + 3 ? juce::jmax (12.0, args[at + 3].getDoubleValue()) : 16.0;
    juce::StringArray log;
    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what) { log.add ((ok ? "ok    " : "FAIL  ") + what); if (! ok) ++failures; };

    HubEngine engine;
    const double sr = 48000.0;
    const int block = 480;   // not a power of two on purpose
    engine.prepare (sr, 512);
    const auto report = engine.loadCats ({ catsDir });
    if (report.isNotEmpty()) log.add (report);
    for (int id = 0; id < PattCat; ++id) check (engine.hasCat (id), juce::String (catName (id)) + " loaded");

    // Extra parts with every other Cat: MIDI effects and sound Cats
    struct Extra { int src, fx, sound; };
    const Extra extras[] = { { SrcChords, FxPatt, SndGlide }, { SrcLead, FxOrbit, SndSing }, { SrcLead, FxBend, SndTalk }, { SrcChords, FxNone, SndEPiano } };
    for (const auto& x : extras)
    {
        const bool fxOk = catForMidiFx (x.fx) < 0 || engine.catAvailable (catForMidiFx (x.fx));
        const bool sndOk = catForSound (x.sound) < 0 || engine.catAvailable (catForSound (x.sound));
        if (! fxOk || ! sndOk) { log.add ("(skipped a part: " + juce::String (midiFxName (x.fx)) + " / " + soundName (x.sound) + " not found)"); continue; }
        const bool added = engine.addTrack (x.src, x.fx, x.sound);
        auto& t = engine.track (engine.getNumTracks() - 1);
        check (added && t.midiFx.load() == x.fx && t.sound.load() == x.sound,
               "added part: " + juce::String (sourceName (x.src)) + " > " + midiFxName (x.fx) + " > " + soundName (x.sound)
               + (engine.lastLoadError.isNotEmpty() ? "  (" + engine.lastLoadError + ")" : juce::String()));
        engine.lastLoadError = {};
        t.filter = 0.3f; t.chorus = 0.4f; t.delay = 0.3f; t.drive = 0.2f; t.crush = 0.1f; t.volume = 0.6f;
    }

    engine.bpm = 100.0;
    engine.barsPerChord = 1;
    engine.setPlaying (true);
    const double life0 = engine.getLifeTime();

    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> audio (2, total);
    std::set<int> padsSeen;
    bool finite = true;
    float peak = 0.0f;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        engine.render (audio.getWritePointer (0, pos), audio.getWritePointer (1, pos), n);
        padsSeen.insert (engine.getCurrentPad());
    }
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < total; ++i)
        {
            const float v = audio.getSample (ch, i);
            if (! std::isfinite (v)) finite = false;
            peak = juce::jmax (peak, std::abs (v));
        }

    for (int i = 0; i < engine.getNumTracks(); ++i)
    {
        auto& t = engine.track (i);
        const double rms = std::sqrt (t.energy.load() / (2.0 * total));
        check (rms > 1.0e-3, "part " + juce::String (i + 1) + " (" + sourceName (t.source.load()) + " > " + soundName (t.sound.load())
                             + ") is heard (rms " + juce::String (rms, 4) + ")");
    }
    const double life1 = engine.getLifeTime();
    check (life1 > life0 + seconds * 0.5, "Life-Cat's world is moving (" + juce::String (life0, 1) + " s -> " + juce::String (life1, 1) + " s)");
    check (finite, "no NaN / inf");
    check (peak <= 1.0f, "never clips (peak " + juce::String (peak, 3) + ")");
    padsSeen.erase (-1);
    check (padsSeen.size() >= 4, "the song stepped through " + juce::String ((int) padsSeen.size()) + " pads");

    // every style loads and plays
    for (int st = 0; st < HubEngine::styleNames().size(); ++st)
    {
        engine.applyStyle (st);
        for (int i = 0; i < engine.getNumTracks(); ++i) engine.track (i).energy = 0.0;
        juce::AudioBuffer<float> b (2, (int) (4.0 * sr));
        for (int pos = 0; pos < b.getNumSamples(); pos += block)
            engine.render (b.getWritePointer (0, pos), b.getWritePointer (1, pos), juce::jmin (block, b.getNumSamples() - pos));
        int heard = 0;
        for (int i = 0; i < engine.getNumTracks(); ++i) heard += engine.track (i).energy.load() > 1.0e-3 ? 1 : 0;
        const float pk = juce::jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples()));
        check (heard >= 3 && pk <= 1.0f && pk > 0.01f, "style " + HubEngine::styleNames()[st] + ": " + juce::String (heard) + " of "
                                                        + juce::String (engine.getNumTracks()) + " parts heard, peak " + juce::String (pk, 2));
    }
    juce::Random dice (3);
    for (int k = 0; k < 4; ++k) { engine.roll (dice); juce::AudioBuffer<float> b (2, 48000); engine.render (b.getWritePointer (0), b.getWritePointer (1), 48000); }
    check (engine.isSongMode() && engine.getSong() != nullptr, "dice rolled 4 times: it writes songs (last: \"" + (engine.getSong() != nullptr ? engine.getSong()->title : juce::String()) + "\")");

    // ---- composed songs: every genre plays, the words reach Talk-Cat, endless keeps going, export, save
    {
        auto renderFor = [&] (double secs)
        {
            juce::AudioBuffer<float> b (2, block);
            float pk = 0.0f;
            bool ok = true;
            for (int pos = 0; pos < (int) (secs * sr); pos += block)
            {
                engine.render (b.getWritePointer (0), b.getWritePointer (1), block);
                pk = juce::jmax (pk, b.getMagnitude (0, 0, block), b.getMagnitude (1, 0, block));
                for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < block; ++i) ok = ok && std::isfinite (b.getSample (ch, i));
                if (pos % (block * 50) == 0) engine.tickSong();
            }
            return std::pair<float, bool> { pk, ok };
        };
        for (int g = 0; g < cat::muse::NumGenres; ++g)
        {
            cat::muse::Settings st;
            st.seed = 100 + (juce::uint32) g; st.genre = g; st.mood = g % cat::muse::NumMoods;
            engine.composeSong (st, true);
            engine.setPlaying (false);
            renderFor (0.1);
            engine.setPlaying (true);
            for (int i = 0; i < engine.getNumTracks(); ++i) engine.track (i).energy = 0.0;
            auto [pk, ok] = renderFor (10.0);
            check (engine.track (0).energy.load() > 1.0e-3 && pk > 0.01f && pk <= 1.0f && ok,
                   "song " + cat::muse::genreNames()[g] + " \"" + engine.getSong()->title + "\" plays (peak " + juce::String (pk, 2) + ")");
        }

        // a pop song sung by Talk-Cat, exported whole: every part is heard
        cat::muse::Settings st;
        st.seed = 777; st.genre = cat::muse::Pop; st.mood = cat::muse::Happy;
        engine.composeSong (st, true);
        int melodyTrack = -1;
        for (int i = 0; i < engine.getNumTracks(); ++i) if (engine.track (i).source.load() == SrcLead) melodyTrack = i;
        check (engine.getNumTracks() == 5 && melodyTrack >= 0, "a song has 5 parts (chords, bass, drums, melody, counter-melody)");
        const bool talk = engine.catAvailable (TalkCat) && melodyTrack >= 0 && engine.setSound (melodyTrack, SndTalk);
        engine.tickSong();
        if (talk)
        {
            juce::String sung;
            juce::MemoryBlock mb;
            engine.track (melodyTrack).soundCat->getStateInformation (mb);
            auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
            if (xml != nullptr && xml->hasTagName ("VST3PluginState"))
                if (auto* comp = xml->getChildByName ("IComponent"))
                {
                    juce::MemoryBlock inner;
                    inner.fromBase64Encoding (comp->getAllSubText());
                    xml = juce::AudioProcessor::getXmlFromBinary (inner.getData(), (int) inner.getSize());
                }
            if (xml != nullptr) sung = xml->getStringAttribute ("liveLyrics");
            check (sung.isNotEmpty() && sung == engine.getSong()->lyricsText, "Talk-Cat got the song's words (" + juce::String (sung.length()) + " letters)");
        }
        else log.add ("(skipped: Talk-Cat not found)");

        const auto wavFile = out.getSiblingFile ("selftest_song.wav");
        for (int i = 0; i < engine.getNumTracks(); ++i) engine.track (i).energy = 0.0;
        const bool exported = engine.exportSong (wavFile);
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (wavFile));
        const double secs = rd != nullptr ? (double) rd->lengthInSamples / rd->sampleRate : 0.0;
        const double want = engine.getSong()->lengthBeats * 60.0 / engine.bpm.load() + 3.0;
        check (exported && std::abs (secs - want) < 0.5, "exported the song as WAV (" + juce::String (secs, 1) + " s, expected " + juce::String (want, 1) + " s)");
        for (int i = 0; i < engine.getNumTracks(); ++i)
        {
            auto& t = engine.track (i);
            check (t.energy.load() > 1.0e-3, "export: part " + juce::String (i + 1) + " (" + sourceName (t.source.load()) + " > " + soundName (t.sound.load()) + ") is heard");
        }
        const auto stem = out.getSiblingFile ("selftest_stem.wav");
        check (engine.exportSong (stem, 2) && stem.getSize() > 10000, "exported a stem");
        const auto midFile = out.getSiblingFile ("selftest_song.mid");
        juce::MidiFile mf;
        bool midOk = engine.exportSongMidi (midFile);
        if (midOk) { juce::FileInputStream in (midFile); midOk = in.openedOk() && mf.readFrom (in) && mf.getNumTracks() == 6; }
        check (midOk, "exported the song as MIDI (" + juce::String (mf.getNumTracks()) + " tracks)");

        // save + restore keeps the song
        const auto title = engine.getSong()->title;
        const auto saved = engine.saveState();
        engine.applyStyle (1);
        check (! engine.isSongMode(), "a style goes back to jamming");
        engine.restoreState (saved);
        check (engine.isSongMode() && engine.getSong() != nullptr && engine.getSong()->title == title, "the saved song comes back (\"" + title + "\")");

        // the arranger: remove a section, play the chorus twice, turn one into a bridge, add an outro
        {
            auto form = engine.getSong()->sections;
            const auto before = form.size();
            int chorus = -1;
            for (int i = 0; i < (int) form.size(); ++i) if (form[(size_t) i].type == cat::muse::Chorus) { chorus = i; break; }
            if (chorus >= 0) form.insert (form.begin() + chorus, form[(size_t) chorus]);
            form.erase (form.begin());
            form[0] = cat::muse::makeSection (*engine.getSong(), cat::muse::Bridge, 8, 5);
            form.push_back (cat::muse::makeSection (*engine.getSong(), cat::muse::Outro, 4, 6));
            double beats = 0.0;
            for (auto& sec : form) beats += sec.bars * 4.0;
            const bool arranged = engine.arrangeSong (form);
            auto sg = engine.getSong();
            bool notesInside = true;
            for (const auto& part : sg->parts) for (const auto& n : part) notesInside = notesInside && n.start >= 0.0 && n.start < sg->lengthBeats + 1e-6;
            check (arranged && sg->sections.size() == form.size() && std::abs (sg->lengthBeats - beats) < 1e-6 && sg->sections[0].type == cat::muse::Bridge
                       && ! sg->chords.empty() && ! sg->parts[cat::muse::Melody].empty() && notesInside,
                   "arranger: " + juce::String ((int) before) + " -> " + juce::String ((int) sg->sections.size()) + " sections, "
                       + juce::String (sg->lengthBeats / 4.0, 0) + " bars, every part re-written");
            const bool wasPlaying = engine.isPlaying();
            engine.setPlaying (true);
            auto [apk, aok] = renderFor (6.0);
            check (aok && apk > 0.01f && apk <= 1.0f, "the arranged song plays (peak " + juce::String (apk, 2) + ", was playing "
                                                       + juce::String ((int) wasPlaying) + ", song mode " + juce::String ((int) engine.isSongMode()) + ")");
            const auto savedArr = engine.saveState();
            engine.applyStyle (1);
            engine.restoreState (savedArr);
            check (engine.isSongMode() && engine.getSong()->sections.size() == form.size() && std::abs (engine.getSong()->lengthBeats - beats) < 1e-6,
                   "a saved jam keeps the arrangement");
            st.endless = true;
            engine.composeSong (st, false);
            check (! engine.arrangeSong (form), "endless songs are not arranged by hand");
            st.endless = false;
            engine.composeSong (st, false);
        }

        // the master effects: squeezed, warm and mono still never clips and stays mono
        {
            engine.masterGlue = 1.0f; engine.masterWarmth = 1.0f; engine.masterWidth = 0.0f; engine.masterTone = 1.0f; engine.masterRoom = 1.0f;
            juce::AudioBuffer<float> b (2, block);
            float pk = 0.0f, side = 0.0f, red = 0.0f;
            bool ok = true;
            for (int pos = 0; pos < (int) (6.0 * sr); pos += block)
            {
                engine.render (b.getWritePointer (0), b.getWritePointer (1), block);
                for (int i = 0; i < block; ++i)
                {
                    ok = ok && std::isfinite (b.getSample (0, i)) && std::isfinite (b.getSample (1, i));
                    pk = juce::jmax (pk, std::abs (b.getSample (0, i)), std::abs (b.getSample (1, i)));
                    if (pos > (int) (2.0 * sr)) side = juce::jmax (side, std::abs (b.getSample (0, i) - b.getSample (1, i)));   // after the limiter's DC filter settles
                }
                red = juce::jmax (red, engine.getMasterReduction());
            }
            check (ok && pk <= 1.0f && pk > 0.01f && side < 2.0e-3f && red > 1.0f,
                   "master FX (glue, warmth, tone, mono): peak " + juce::String (pk, 2) + ", side " + juce::String (side, 5) + ", glue squeezes " + juce::String (red, 1) + " dB");
            const auto savedFx = engine.saveState();
            engine.masterGlue = 0.0f; engine.masterWarmth = 0.0f; engine.masterWidth = 1.0f; engine.masterTone = 0.0f; engine.masterRoom = 0.5f;
            engine.restoreState (savedFx);
            check (engine.masterGlue.load() > 0.99f && engine.masterWidth.load() < 0.01f, "a saved jam keeps the master effects");
            engine.masterGlue = 0.0f; engine.masterWarmth = 0.0f; engine.masterWidth = 1.0f; engine.masterTone = 0.0f; engine.masterRoom = 0.5f;
        }

        // endless: the next part gets written while it plays
        st.endless = true;
        st.genre = cat::muse::House;
        engine.composeSong (st, true);
        engine.setPlaying (false);
        renderFor (0.1);
        engine.setPlaying (true);
        double played = 0.0;
        while (played < 240.0 && engine.getSong()->chunk < 1) { renderFor (2.0); played += 2.0; }
        check (engine.getSong()->chunk >= 1, "endless: moved on to part " + juce::String (engine.getSong()->chunk + 1) + " after " + juce::String (played, 0) + " s");
        auto [pk, ok] = renderFor (4.0);
        check (ok && pk <= 1.0f && pk > 0.01f, "endless keeps playing (peak " + juce::String (pk, 2) + ")");
    }

    // a MIDI keyboard plays the Lead part
    {
        engine.applyStyle (0);
        engine.setPlaying (true);
        juce::AudioBuffer<float> b (2, block);
        engine.render (b.getWritePointer (0), b.getWritePointer (1), block);
        const int kt = engine.getKeyboardTrackNow();
        bool heard = false;
        if (kt >= 0)
        {
            engine.track (kt).takeNotes();
            engine.playKeyboardNote (juce::MidiMessage::noteOn (3, 97, (juce::uint8) 110));
            for (int k = 0; k < 20 && ! heard; ++k)
            {
                engine.render (b.getWritePointer (0), b.getWritePointer (1), block);
                for (int nn : engine.track (kt).takeNotes()) heard = heard || nn == 97;
            }
            engine.playKeyboardNote (juce::MidiMessage::noteOff (3, 97));
            for (int k = 0; k < 4; ++k) engine.render (b.getWritePointer (0), b.getWritePointer (1), block);
        }
        check (kt >= 0 && heard, "a MIDI keyboard plays part " + juce::String (kt + 1) + (kt >= 0 ? " (" + juce::String (sourceName (engine.track (kt).source.load())) + ")" : juce::String()));
    }

    // the loop recorder: record a note from the keyboard, then the loop plays it again by itself
    {
        engine.loopBars = 1;
        engine.bpm = 120.0;
        juce::AudioBuffer<float> b (2, block);
        auto renderBlocks = [&] (int k) { for (int i = 0; i < k; ++i) engine.render (b.getWritePointer (0), b.getWritePointer (1), block); };
        engine.setLoopRecording (true);
        renderBlocks (2);
        engine.playKeyboardNote (juce::MidiMessage::noteOn (1, 101, (juce::uint8) 100));
        renderBlocks (10);
        engine.playKeyboardNote (juce::MidiMessage::noteOff (1, 101));
        renderBlocks (2);
        engine.setLoopRecording (false);
        renderBlocks (2);
        const int kt = engine.getKeyboardTrackNow();
        const int recorded = engine.getLoopNotes();
        bool replayed = false;
        if (kt >= 0)
        {
            engine.track (kt).takeNotes();
            for (int i = 0; i < 260 && ! replayed; ++i)   // one bar at 120 bpm = 2 s = 200 blocks of 480
            {
                renderBlocks (1);
                for (int nn : engine.track (kt).takeNotes()) replayed = replayed || nn == 101;
            }
        }
        check (recorded == 1 && replayed, "loop recorder: a recorded note plays again in the loop (" + juce::String (recorded) + " notes)");
        engine.undoLoopTake();
        renderBlocks (2);
        check (engine.getLoopNotes() == 0, "loop recorder: undo removes the last take");

        // the piano roll: notes in, the same notes out, they play, a saved jam keeps them, and the roll can draw
        {
            std::vector<HubEngine::LoopNote> drawn = { { 0.0, 0.5, 64, 0.8f, 7 }, { 1.0, 1.0, 67, 0.6f, 7 }, { 3.75, 0.5, 71, 0.9f, 7 } };
            engine.setLoopNoteList (drawn);
            renderBlocks (2);
            const auto back = engine.getLoopNoteList();
            bool same = back.size() == drawn.size();
            for (size_t i = 0; same && i < back.size(); ++i)
                same = std::abs (back[i].start - drawn[i].start) < 1e-6 && std::abs (back[i].length - drawn[i].length) < 1e-6 && back[i].pitch == drawn[i].pitch;
            check (same && engine.getLoopNotes() == 3, "piano roll: the loop gives back the notes drawn (incl. one round the loop's end)");
            bool heard64 = false;
            if (kt >= 0)
            {
                engine.track (kt).takeNotes();
                for (int i = 0; i < 220 && ! heard64; ++i) { renderBlocks (1); for (int nn : engine.track (kt).takeNotes()) heard64 = heard64 || nn == 64; }
            }
            check (heard64, "piano roll: drawn notes play in the loop");
            const auto saved = engine.saveState();
            engine.setLoopNoteList ({});
            engine.restoreState (saved);
            check (engine.getLoopNoteList().size() == 3 && engine.loopBars.load() == 1, "a saved jam keeps the loop");
            {
                LoopEditor roll (engine);
                roll.setSize (900, 420);
                roll.addNoteAt (2.3, 60);
                check (engine.getLoopNoteList().size() == 4 && std::abs (roll.getNotes().back().start - 2.25) < 1e-6,
                       "piano roll: a click draws a note on the 1/16 grid");
                roll.quantize();
                bool onGrid = true;
                for (auto& nt : engine.getLoopNoteList()) onGrid = onGrid && std::abs (nt.start * 4.0 - std::round (nt.start * 4.0)) < 1e-6;
                check (onGrid, "piano roll: quantize");
                roll.clearButton.onClick();
                renderBlocks (2);
                check (engine.getLoopNotes() == 0, "piano roll: clear");
            }
        }
        engine.loopBars = 4;
    }

    // the new effects: phaser and tremolo on every part, pump and vinyl on the master - loud, finite, saved
    {
        juce::AudioBuffer<float> b (2, block);
        auto energyOf = [&] (int blocks)
        {
            double e = 0.0;
            float pk = 0.0f;
            bool fin = true;
            for (int i = 0; i < blocks; ++i)
            {
                engine.render (b.getWritePointer (0), b.getWritePointer (1), block);
                for (int ch = 0; ch < 2; ++ch)
                    for (int k = 0; k < block; ++k)
                    {
                        const float v = b.getSample (ch, k);
                        fin = fin && std::isfinite (v);
                        pk = juce::jmax (pk, std::abs (v));
                        e += (double) v * v;
                    }
            }
            return std::make_tuple (e, pk, fin);
        };
        for (int i = 0; i < engine.getNumTracks(); ++i) { engine.track (i).phaser = 1.0f; engine.track (i).tremolo = 1.0f; }
        engine.masterPump = 1.0f;
        engine.masterVinyl = 1.0f;
        auto [e1, pk1, fin1] = energyOf (300);
        check (fin1 && pk1 <= 1.0f && e1 > 1.0, "phaser + tremolo + pump + vinyl all on: peak " + juce::String (pk1, 2) + ", still playing");
        const auto saved = engine.saveState();
        for (int i = 0; i < engine.getNumTracks(); ++i) { engine.track (i).phaser = 0.0f; engine.track (i).tremolo = 0.0f; }
        engine.masterPump = 0.0f; engine.masterVinyl = 0.0f;
        engine.restoreState (saved);
        check (engine.masterPump.load() > 0.99f && engine.masterVinyl.load() > 0.99f && engine.getNumTracks() > 0
               && engine.track (0).phaser.load() > 0.99f && engine.track (0).tremolo.load() > 0.99f, "a saved jam keeps phaser, tremolo, pump and vinyl");
        for (int i = 0; i < engine.getNumTracks(); ++i) { engine.track (i).phaser = 0.0f; engine.track (i).tremolo = 0.0f; }
        engine.masterPump = 0.0f; engine.masterVinyl = 0.0f;
    }

    // stop: everything should fade out (no hanging notes in the built-in synths)
    engine.applyStyle (0);
    engine.setPlaying (true);
    { juce::AudioBuffer<float> b (2, 96000); engine.render (b.getWritePointer (0), b.getWritePointer (1), 96000); }
    engine.setPlaying (false);
    for (int i = 0; i < engine.getNumTracks(); ++i) if (engine.track (i).source.load() == SrcLife) engine.track (i).mute = true;   // Life lives on
    juce::AudioBuffer<float> tail (2, (int) (5.0 * sr));
    for (int pos = 0; pos < tail.getNumSamples(); pos += block)
        engine.render (tail.getWritePointer (0, pos), tail.getWritePointer (1, pos), juce::jmin (block, tail.getNumSamples() - pos));
    const float lastSecond = juce::jmax (tail.getMagnitude (0, tail.getNumSamples() - (int) sr, (int) sr), tail.getMagnitude (1, tail.getNumSamples() - (int) sr, (int) sr));
    check (lastSecond < 0.01f, "silent after Stop (" + juce::String (lastSecond, 4) + ")");

    // save + restore round trip
    const auto state = engine.saveState();
    const int parts = engine.getNumTracks();
    engine.restoreState (state);
    check (engine.getNumTracks() == parts && state.getNumChildren() >= parts + 4, "state saves every part and Cat");

    if (out != juce::File())
    {
        out.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (out.createOutputStream().release());
        if (os != nullptr)
            if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (16)))
                w->writeFromAudioSampleBuffer (audio, 0, total);
        log.add ("wrote " + out.getFullPathName());
    }
    log.add (failures == 0 ? "CAT HUB SELF TEST PASSED" : "CAT HUB SELF TEST FAILED (" + juce::String (failures) + ")");
    const auto text = log.joinIntoString ("\n");
    std::cout << text << std::endl;
    if (out != juce::File()) out.withFileExtension ("txt").replaceWithText (text);
    engine.releaseCats();
    return failures == 0 ? 0 : 1;
}

//==============================================================================
/** Pokes the real Hub window at random while another thread renders audio. */
class UiStress : private juce::Timer
{
public:
    UiStress (const juce::File& catsDir, double secondsToRun, int seed)
        : seconds (secondsToRun), rng (seed)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Cat Hub stress";
        o.filenameSuffix = "settings";
        o.folderName = "Cat Suite";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
        engine.prepare (48000.0, 512);
        const auto report = engine.loadCats ({ catsDir });
        std::cout << "cats: " << (report.isEmpty() ? juce::String ("all loaded") : report) << std::endl;
        audio = std::thread ([this]
        {
            juce::Random r (7);
            std::vector<float> l (1024), rr (1024);
            while (! stop)
            {
                const int n = 1 + r.nextInt (1024);
                engine.render (l.data(), rr.data(), n);
                for (int i = 0; i < n; ++i)
                    if (! std::isfinite (l[(size_t) i]) || std::abs (l[(size_t) i]) > 1.0f) { std::cout << "!! bad sample " << l[(size_t) i] << std::endl; std::abort(); }
                std::this_thread::sleep_for (std::chrono::milliseconds (r.nextInt (3)));
            }
        });
        openWindow();
        start = juce::Time::getMillisecondCounterHiRes();
        startTimer (15);
    }

    ~UiStress() override
    {
        stopTimer();
        engine.stopRecording();
        main.reset();
        stop = true;
        audio.join();
        engine.releaseCats();
    }

    int failures = 0;

private:
    void openWindow()
    {
        main = std::make_unique<HubMain> (engine, deviceManager, *settings);
        main->setTopLeftPosition (0, 0);
        main->addToDesktop (juce::ComponentPeer::windowAppearsOnTaskbar);
        main->setVisible (true);
        if (rng.nextBool()) main->finishTutorialForTests();   // sometimes click through the tutorial too
    }

    static void collect (juce::Component& c, juce::Array<juce::Component*>& out)
    {
        for (auto* ch : c.getChildren())
            if (ch->isVisible()) { out.add (ch); collect (*ch, out); }
    }

    void timerCallback() override
    {
        if (juce::Time::getMillisecondCounterHiRes() - start > seconds * 1000.0)
        {
            stopTimer();
            std::cout << "UI STRESS OK after " << actions << " actions" << std::endl;
            juce::JUCEApplication::getInstance()->setApplicationReturnValue (failures == 0 ? 0 : 1);
            juce::JUCEApplication::quit();
            return;
        }
        ++actions;
        const int what = rng.nextInt (100);
        if (what < 3)  { engine.setPlaying (! engine.isPlaying()); return; }
        if (what < 5)  { main.reset(); openWindow(); return; }                                  // close / reopen the window
        if (what < 7)  { const auto t = engine.saveState(); main->restoreStateSafely (t); return; }  // save + load while playing
        if (what < 9)  { const int st = rng.nextInt (HubEngine::styleNames().size()); main->changeCats ([this, st] { engine.applyStyle (st); }); return; }
        if (what < 10) { main->changeCats ([this] { engine.roll (rng); }); return; }
        if (what < 8)  { if (engine.isRecording()) engine.stopRecording(); else engine.startRecording (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cathub-stress.wav")); return; }
        if (what < 12) { juce::PopupMenu::dismissAllActiveMenus(); return; }
        if (what < 18)
        {
            const int pad = rng.nextInt (9);
            main->keyPressed (pad == 8 ? juce::KeyPress (juce::KeyPress::spaceKey) : juce::KeyPress ('1' + pad, juce::ModifierKeys(), (juce::juce_wchar) ('1' + pad)));
            engine.setLivePad (rng.nextBool() ? -1 : pad);
            return;
        }

        if (what >= 94)
        {
            // the arranger: a random change to a random section (or start a song first)
            if (! engine.isSongMode() || engine.getSong() == nullptr || engine.getSong()->settings.endless)
            {
                cat::muse::Settings st;
                st.seed = (juce::uint32) rng.nextInt (100000); st.genre = rng.nextInt (cat::muse::NumGenres);
                main->changeCats ([this, st] { engine.composeSong (st, rng.nextBool()); });
                return;
            }
            static const int results[] = { 1, 2, 3, 4, 5, 6, 7, 8, 100, 103, 104, 206, 208, 300, 303, 400, 402 };
            const int n = (int) engine.getSong()->sections.size();
            main->getComposePage()->arrangeSection (rng.nextInt (n + 1) - (rng.nextInt (10) == 0 ? 1 : 0), results[rng.nextInt ((int) std::size (results))]);
            return;
        }
        juce::Array<juce::Component*> comps;
        collect (*main, comps);
        if (comps.isEmpty()) return;
        auto* c = comps[rng.nextInt (comps.size())];
        if (auto* b = dynamic_cast<juce::Button*> (c))
        {
            const auto t = b->getButtonText();
            if (t.startsWith ("Audio") || t.startsWith ("Find") || t.startsWith ("Rec") || t.startsWith ("Stop rec")) return;  // dialogs / files
            if (t == "Test" || t.startsWith ("Control Panel") || t.startsWith ("Reset")) return;    // sound card dialogs
            b->triggerClick();
            return;
        }
        if (auto* cb = dynamic_cast<juce::ComboBox*> (c))
        {
            if (cb->getNumItems() > 0) cb->setSelectedItemIndex (rng.nextInt (cb->getNumItems()), juce::sendNotificationSync);
            return;
        }
        if (auto* sl = dynamic_cast<juce::Slider*> (c))
        {
            sl->setValue (sl->getMinimum() + rng.nextDouble() * (sl->getMaximum() - sl->getMinimum()), juce::sendNotificationSync);
            return;
        }
        // a click / drag on whatever it is (pads, cards, tab bar, the Cats' windows)
        const auto r = c->getLocalBounds();
        if (r.isEmpty()) return;
        auto pt = [&] { return juce::Point<float> ((float) rng.nextInt (juce::jmax (1, r.getWidth())), (float) rng.nextInt (juce::jmax (1, r.getHeight()))); };
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        const auto now = juce::Time::getCurrentTime();
        const auto down = pt();
        auto ev = [&] (juce::Point<float> pos, juce::ModifierKeys m, bool dragged)
        { return juce::MouseEvent (src, pos, m, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, c, c, now, down, now, 1, dragged); };
        // like a real click: a component behind a modal window (e.g. the open LINK pop-up)
        // does not get it; the modal window is told instead (and usually closes)
        if (c->isCurrentlyBlockedByAnotherModalComponent())
        {
            if (auto* m = juce::Component::getCurrentlyModalComponent()) m->inputAttemptWhenModal();
            return;
        }
        juce::Component::SafePointer<juce::Component> safe (c);
        c->mouseDown (ev (down, juce::ModifierKeys::leftButtonModifier, false));
        for (int k = rng.nextInt (4); k > 0 && safe; --k) c->mouseDrag (ev (pt(), juce::ModifierKeys::leftButtonModifier, true));
        if (safe) c->mouseUp (ev (pt(), juce::ModifierKeys(), false));
    }

    HubEngine engine;
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<HubMain> main;
    std::thread audio;
    std::atomic<bool> stop { false };
    const double seconds;
    juce::Random rng;
    double start = 0.0;
    int actions = 0;
};

//==============================================================================
class HubApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Cat Hub"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        auto args = juce::StringArray::fromTokens (commandLine, true);
        for (auto& a : args) a = a.unquoted();
        if (args.contains ("--snapshot"))
        {
            // --snapshot <catsFolder> <out.png>: pictures of every tab (for checking the layout)
            const int at = args.indexOf ("--snapshot");
            juce::PropertiesFile::Options o;
            o.applicationName = "Cat Hub snapshot"; o.filenameSuffix = "settings"; o.folderName = "Cat Suite";
            juce::PropertiesFile props (o);
            HubEngine e;
            e.prepare (48000.0, 512);
            e.loadCats ({ juce::File (args[at + 1]) });
            e.setPlaying (true);
            juce::AudioDeviceManager dm;
            {
                HubMain m (e, dm, props);
                m.setSize (1280, 820);
                const juce::File out (args[at + 2]);
                std::vector<float> l (512), r (512);
                int shot = 0;
                auto snap = [&] (const juce::String& name)
                {
                    for (int i = 0; i < 20; ++i) e.render (l.data(), r.data(), 512);
                    for (int k = 0; k < 3; ++k) m.tickForSnapshot();
                    auto img = m.createComponentSnapshot (m.getLocalBounds());
                    juce::PNGImageFormat png;
                    auto f = out.getSiblingFile (out.getFileNameWithoutExtension() + "_" + juce::String (shot++) + "_" + name + ".png");
                    f.deleteFile();
                    if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
                };
                for (int i = 0; i < 200; ++i) e.render (l.data(), r.data(), 512);   // meters + pads moving
                m.startTutorial();
                snap ("tutorial");
                m.finishTutorialForTests();
                m.setAdvanced (false);
                snap ("simple");
                m.changeCats ([&e] { e.applyStyle (4); });
                m.setAdvanced (true);
                snap ("advanced");
                m.selectTab (2);
                snap ("settings");
                m.changeCats ([&e]
                {
                    cat::muse::Settings st;
                    st.seed = 4242; st.genre = cat::muse::Pop; st.mood = cat::muse::Happy;
                    e.composeSong (st, true);
                    e.setPlaying (true);
                });
                for (int i = 0; i < 1500; ++i) e.render (l.data(), r.data(), 512);   // into the song a bit
                m.selectTab (1);
                snap ("compose");
                m.selectTab (0);
                snap ("song_rack");
                {
                    // the piano roll and the master effects
                    e.loopBars = 2;
                    std::vector<HubEngine::LoopNote> mel;
                    const int line[] = { 60, 64, 67, 72, 71, 67, 64, 62, 60, 62, 64, 67, 69, 67, 65, 64 };
                    for (int k = 0; k < 16; ++k) mel.push_back ({ k * 0.5, k % 4 == 3 ? 1.0 : 0.5, line[k], 0.5f + 0.03f * k, 1 });
                    for (int k = 0; k < 4; ++k) mel.push_back ({ k * 2.0, 2.0, 48 + (k % 2) * 5, 0.6f, 2 });
                    e.setLoopNoteList (mel);
                    for (int i = 0; i < 40; ++i) e.render (l.data(), r.data(), 512);
                    juce::PNGImageFormat png;
                    auto save = [&] (juce::Component& c, const juce::String& name)
                    {
                        auto f = out.getSiblingFile (out.getFileNameWithoutExtension() + "_" + juce::String (shot++) + "_" + name + ".png");
                        f.deleteFile();
                        if (auto os = f.createOutputStream()) png.writeImageToStream (c.createComponentSnapshot (c.getLocalBounds()), *os);
                    };
                    LoopEditor roll (e);
                    roll.setSize (980, 480);
                    roll.setLookAndFeel (&m.getLookAndFeel());
                    save (roll, "pianoroll");
                    roll.setLookAndFeel (nullptr);
                    MasterFxPanel fx (e);
                    fx.setSize (540, 130);
                    fx.setLookAndFeel (&m.getLookAndFeel());
                    save (fx, "masterfx");
                    fx.setLookAndFeel (nullptr);
                }
                for (int th = 1; th < (int) themes().size(); ++th) { m.applyTheme (th); snap ("theme" + juce::String (th)); }
                m.applyTheme (0);
            }
            e.releaseCats();
            quit();
            return;
        }
        if (args.contains ("--uistress"))
        {
            const int at = args.indexOf ("--uistress");
            uiStress = std::make_unique<UiStress> (juce::File (args[at + 1]), juce::jmax (5.0, args[at + 2].getDoubleValue()),
                                                   args.size() > at + 3 ? args[at + 3].getIntValue() : 1);
            return;
        }
        if (args.contains ("--selftest"))
        {
            setApplicationReturnValue (runSelfTest (args));
            quit();
            return;
        }

        juce::PropertiesFile::Options o;
        o.applicationName = "Cat Hub";
        o.filenameSuffix = "settings";
        o.folderName = "Cat Suite";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);

        engine = std::make_unique<HubEngine>();
        engine->prepare (48000.0, 512);
        engine->loadCats (HubEngine::defaultSearchFolders (juce::File (settings->getValue ("catsFolder"))));
        if (auto xml = settings->getXmlValue ("hub")) engine->restoreState (juce::ValueTree::fromXml (*xml));

        const auto audioState = settings->getXmlValue ("audio");
        deviceManager.initialise (0, 2, audioState.get(), true);
        deviceManager.addAudioCallback (engine.get());
        // MIDI keyboards: the first time, turn every one on (Settings - Sound card lists them)
        if (! settings->getBoolValue ("midiSetUp", false))
        {
            for (const auto& d : juce::MidiInput::getAvailableDevices()) deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
            settings->setValue ("midiSetUp", true);
        }
        deviceManager.addMidiInputDeviceCallback ({}, engine.get());

        window = std::make_unique<MainWindow> (*engine, deviceManager, *settings);
    }

    void shutdown() override
    {
        uiStress.reset();
        if (engine == nullptr) return;
        if (auto xml = engine->saveState().createXml()) settings->setValue ("hub", xml.get());
        if (auto xml = deviceManager.createStateXml()) settings->setValue ("audio", xml.get());
        settings->saveIfNeeded();
        window.reset();                                   // Cat windows first
        deviceManager.removeMidiInputDeviceCallback ({}, engine.get());
        deviceManager.removeAudioCallback (engine.get());
        deviceManager.closeAudioDevice();
        engine.reset();                                   // then the Cats
    }

    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (HubEngine& e, juce::AudioDeviceManager& dm, juce::PropertiesFile& s)
            : DocumentWindow ("Cat Hub", theme().bg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new HubMain (e, dm, s), true);
            setResizable (true, false);
            setResizeLimits (1000, 680, 4000, 3000);
            // fit the screen, then centre it
            if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            {
                const auto area = d->userArea;
                centreWithSize (juce::jmin (getWidth(), area.getWidth() - 40), juce::jmin (getHeight(), area.getHeight() - 60));
            }
            else centreWithSize (getWidth(), getHeight());
            setVisible (true);
            if (auto* c = getContentComponent()) c->grabKeyboardFocus();
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<HubEngine> engine;
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<UiStress> uiStress;
};
} // namespace hub

START_JUCE_APPLICATION (hub::HubApplication)

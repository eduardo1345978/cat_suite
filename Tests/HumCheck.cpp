// Hum-Cat's check: a synthetic "sung" melody (harmonics, vibrato, scoops into the notes, breaths and noise)
// is heard as the right notes, in the right key, with sensible chords; the plug-in writes the MIDI out on
// the right channels in time, plays its own sound, and keeps the song in the saved project.
#include "PluginProcessor.h"
#include <cstdio>
#include <thread>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

/** A voice singing `pitches` (one per `noteSeconds`), with a short breath between notes. */
static std::vector<float> sing (const std::vector<int>& pitches, double sr, double noteSeconds, float vibratoCents = 25.0f, float noise = 0.01f)
{
    std::vector<float> out;
    Random rng (7);
    double phase = 0.0;
    out.resize ((size_t) (sr * 0.3), 0.0f);                         // a little silence first
    for (auto& s : out) s = noise * (rng.nextFloat() * 2.0f - 1.0f);
    for (int p : pitches)
    {
        const int n = (int) (sr * noteSeconds), gap = (int) (sr * 0.045);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double scoop = t < 0.05 ? -70.0 * (1.0 - t / 0.05) : 0.0;          // slide up into the note
            const double vib = t > 0.12 ? vibratoCents * std::sin (2.0 * MathConstants<double>::pi * 5.5 * t) : 0.0;
            const double f = 440.0 * std::pow (2.0, (p - 69 + (scoop + vib) / 100.0) / 12.0);
            phase += f / sr;
            const double env = jmin (1.0, t / 0.03) * jmin (1.0, (noteSeconds - t) / 0.05);
            double v = 0.0;
            for (int h = 1; h <= 8; ++h) v += std::sin (2.0 * MathConstants<double>::pi * phase * h) * (h == 1 ? 1.0 : 0.6 / h);
            out.push_back ((float) (0.25 * env * v) + noise * (rng.nextFloat() * 2.0f - 1.0f));
        }
        for (int i = 0; i < gap; ++i) out.push_back (noise * (rng.nextFloat() * 2.0f - 1.0f));
    }
    for (int i = 0; i < (int) (sr * 0.3); ++i) out.push_back (noise * (rng.nextFloat() * 2.0f - 1.0f));
    return out;
}

static std::vector<int> pitchesOf (const std::vector<humcat::Note>& notes)
{
    std::vector<int> p;
    for (auto& n : notes) p.push_back (n.pitch);
    return p;
}

static String list (const std::vector<int>& v)
{
    String s;
    for (int x : v) s << x << " ";
    return s.trim();
}

static void onMessageThread (std::function<void()> fn)
{
    WaitableEvent done;
    MessageManager::callAsync ([&] { fn(); done.signal(); });
    done.wait (10000);
}

int main()
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0;
    const std::vector<int> cMajor { 60, 64, 67, 72, 71, 67, 65, 62, 60, 64, 62, 60 };   // C E G C B G F D C E D C

    // --- the brain on its own ---
    {
        const auto audio = sing (cMajor, sr, 0.5);
        const auto track = humcat::trackPitch (audio.data(), (int) audio.size(), sr, 0.5f);
        const auto notes = humcat::segment (track);
        check (pitchesOf (notes) == cMajor, "heard the sung notes through vibrato and scoops: " + list (pitchesOf (notes)));
        bool timing = notes.size() == cMajor.size();
        for (size_t i = 0; timing && i < notes.size(); ++i)
            timing = std::abs (notes[i].start - (0.3 + i * 0.545)) < 0.07 && std::abs (notes[i].length - 0.5) < 0.12;
        check (timing, "note starts and lengths are where they were sung");

        int key = -1, scale = -1;
        humcat::detectKey (notes, key, scale);
        check (key == 0 && scale == 0, "key: C major (got " + humcat::keyName (key) + (scale == 1 ? " minor)" : " major)"));

        humcat::SongSettings s;
        s.bpm = 60.0 / 0.545;   // one note per beat
        const auto song = humcat::makeSong (notes, s);
        const auto& lead = song.parts[humcat::Lead];
        bool onGrid = lead.size() == cMajor.size() && lead.front().start == 0.0;
        for (size_t i = 0; onGrid && i < lead.size(); ++i)
            onGrid = std::abs (lead[i].start - (double) i) < 1e-6 && lead[i].pitch == cMajor[i];
        check (onGrid, "lead: the melody on the beat grid, first note on bar 1");
        check (std::fmod (song.lengthBeats, 4.0) == 0.0 && song.lengthBeats >= 12.0, "the song is whole bars (" + String (song.lengthBeats) + " beats)");
        check (! song.chords.empty() && song.chords.front().root == 0 && ! song.chords.front().minor, "first chord is C (" + (song.chords.empty() ? String() : song.chordName (song.chords.front())) + ")");
        check (song.chords.back().root == 0 || song.chords.back().root == 7, "last chord comes home (I or V): " + song.chordName (song.chords.back()));
        bool fits = true;   // every chord shares a note with the melody under it
        for (const auto& c : song.chords)
        {
            bool shares = false, any = false;
            for (const auto& n : lead)
                if (n.start >= c.start && n.start < c.start + c.length)
                {
                    any = true;
                    const int pc = ((n.pitch - c.root) % 12 + 12) % 12;
                    shares |= pc == 0 || pc == (c.minor ? 3 : 4) || pc == 7;
                }
            fits &= shares || ! any;
        }
        check (fits, "every chord fits the melody sung over it");
        bool bassOk = ! song.parts[humcat::Bass].empty();
        for (const auto& n : song.parts[humcat::Bass]) bassOk &= n.pitch >= 28 && n.pitch <= 60;
        check (bassOk, "a bass line, in the bass range");
        check (song.parts[humcat::Drums].size() > 16, "drums (" + String ((int) song.parts[humcat::Drums].size()) + " hits)");
        bool chordsInRange = ! song.parts[humcat::Chords].empty();
        for (const auto& n : song.parts[humcat::Chords]) chordsInRange &= n.pitch >= 45 && n.pitch <= 80;
        check (chordsInRange, "chord voicings in a sensible range");

        const auto file = humcat::toMidiFile (song, { true, true, true, true });
        check (file.getNumTracks() == 5, "MIDI file: tempo + four parts");
        const auto leadOnly = humcat::toMidiFile (song, { false, false, true, false });
        check (leadOnly.getNumTracks() == 2, "MIDI file with only the lead");

        // every style makes a song
        bool styles = true;
        for (int st = 0; st < humcat::NumStyles; ++st)
        {
            s.style = st;
            const auto sg = humcat::makeSong (notes, s);
            styles &= ! sg.parts[humcat::Drums].empty() && ! sg.parts[humcat::Bass].empty() && ! sg.parts[humcat::Chords].empty();
        }
        check (styles, "all six styles write chords, bass and drums");
    }

    // minor, a low male voice and less vibrato
    {
        const std::vector<int> aMinor { 45, 48, 52, 57, 55, 52, 50, 47, 45 };   // A C E A G E D B A, an octave low
        const auto audio = sing (aMinor, sr, 0.42, 15.0f);
        const auto notes = humcat::segment (humcat::trackPitch (audio.data(), (int) audio.size(), sr, 0.5f));
        check (pitchesOf (notes) == aMinor, "a low voice: no octave mistakes (" + list (pitchesOf (notes)) + ")");
        int key = -1, scale = -1;
        humcat::detectKey (notes, key, scale);
        check (key == 9 && scale == 1, "key: A minor (got " + humcat::keyName (key) + (scale == 1 ? " minor)" : " major)"));
    }

    // noise only: no notes
    {
        Random rng (3);
        std::vector<float> noise ((size_t) (sr * 3.0));
        for (auto& s : noise) s = 0.05f * (rng.nextFloat() * 2.0f - 1.0f);
        const auto notes = humcat::segment (humcat::trackPitch (noise.data(), (int) noise.size(), sr, 0.5f));
        check (notes.size() <= 1, "noise alone isn't heard as a tune (" + String ((int) notes.size()) + " notes)");
    }

    // --- the plug-in ---
    {
        auto wavFile = File::getSpecialLocation (File::tempDirectory).getChildFile ("humcheck.wav");
        {
            const auto audio = sing (cMajor, sr, 0.5);
            wavFile.deleteFile();
            WavAudioFormat wav;
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (wavFile), sr, 1, 24, {}, 0));
            const float* ch[] = { audio.data() };
            w->writeFromFloatArrays (ch, 1, (int) audio.size());
        }

        std::thread worker ([&]
        {
            std::unique_ptr<HumCatProcessor> p;
            onMessageThread ([&] { p = std::make_unique<HumCatProcessor>(); p->prepareToPlay (48000.0, 512); });
            String err;
            bool loaded = false;
            onMessageThread ([&] { loaded = p->loadFile (wavFile, err); });
            for (int i = 0; i < 200 && p->isAnalysing(); ++i) Thread::sleep (25);
            Thread::sleep (200);
            auto song = p->getSong();
            check (loaded && song != nullptr && pitchesOf (song->parts[humcat::Lead]) == cMajor, "plug-in: loaded a wav and heard the tune");
            check (song != nullptr && song->key == 0 && song->scale == 0, "plug-in: C major");

            // play it (preview, no host): MIDI out on 1 / 2 / 3 / 10 and its own sound
            p->previewPlaying = true;
            AudioBuffer<float> buf (2, 512);
            MidiBuffer midi;
            std::array<int, 17> ons {};
            int offs = 0, onsTotal = 0;
            std::vector<int> leadSeen;
            double peak = 0.0;
            for (int block = 0; block < (int) (48000.0 * 8.0 / 512); ++block)
            {
                buf.clear();
                p->processBlock (buf, midi);
                peak = jmax (peak, (double) buf.getMagnitude (0, 0, 512));
                for (const auto m : midi)
                {
                    const auto msg = m.getMessage();
                    if (msg.isNoteOn()) { ++ons[(size_t) msg.getChannel()]; ++onsTotal; if (msg.getChannel() == 3) leadSeen.push_back (msg.getNoteNumber()); }
                    if (msg.isNoteOff()) ++offs;
                }
            }
            p->previewPlaying = false;
            buf.clear();
            p->processBlock (buf, midi);
            for (const auto m : midi) if (m.getMessage().isNoteOff()) ++offs;
            check (ons[1] > 0 && ons[2] > 0 && ons[3] > 0 && ons[10] > 0, "MIDI out: chords ch 1, bass ch 2, lead ch 3, drums ch 10");
            const std::vector<int> firstLead (leadSeen.begin(), leadSeen.begin() + jmin (leadSeen.size(), cMajor.size()));
            check (firstLead == cMajor, "MIDI out: the lead plays the sung melody (" + list (firstLead) + ")");
            check (offs == onsTotal, "every note-on gets its note-off (" + String (onsTotal) + " / " + String (offs) + ")");
            check (peak > 0.05 && peak <= 1.0, "its own sound plays, under the limiter (peak " + String (peak, 3) + ")");

            // switch a part off: no more drum notes
            onMessageThread ([&] { p->apvts.getParameter ("outDrums")->setValueNotifyingHost (0.0f); });
            Thread::sleep (100);
            p->previewPlaying = true;
            int drums = 0;
            for (int block = 0; block < 400; ++block)
            {
                buf.clear(); p->processBlock (buf, midi);
                for (const auto m : midi) if (m.getMessage().isNoteOn() && m.getMessage().getChannel() == 10) ++drums;
            }
            p->previewPlaying = false;
            check (drums == 0, "drums off: no drum notes");

            // the project keeps the song
            MemoryBlock state;
            onMessageThread ([&] { p->apvts.getParameter ("style")->setValueNotifyingHost (p->apvts.getParameter ("style")->convertTo0to1 (3.0f)); });
            Thread::sleep (100);
            onMessageThread ([&] { p->getStateInformation (state); });
            std::unique_ptr<HumCatProcessor> q;
            onMessageThread ([&] { q = std::make_unique<HumCatProcessor>(); q->prepareToPlay (48000.0, 512); q->setStateInformation (state.getData(), (int) state.getSize()); });
            Thread::sleep (300);
            auto restored = q->getSong();
            check (restored != nullptr && pitchesOf (restored->parts[humcat::Lead]) == cMajor && q->apvts.getRawParameterValue ("style")->load() == 3.0f
                       && q->apvts.getRawParameterValue ("outDrums")->load() < 0.5f,
                   "saved project: the sung notes, the style and the parts come back");

            // recording from the audio input
            onMessageThread ([&] { q->clear(); q->startRecording(); });
            {
                const auto audio = sing ({ 62, 66, 69, 74, 69, 66, 62 }, 48000.0, 0.4);   // D major arpeggio
                AudioBuffer<float> in (2, 512);
                for (size_t pos = 0; pos < audio.size(); pos += 512)
                {
                    in.clear();
                    const int n = (int) jmin ((size_t) 512, audio.size() - pos);
                    in.copyFrom (0, 0, audio.data() + pos, n);
                    in.copyFrom (1, 0, audio.data() + pos, n);
                    q->processBlock (in, midi);
                }
            }
            onMessageThread ([&] { q->stopRecording(); });
            for (int i = 0; i < 200 && q->isAnalysing(); ++i) Thread::sleep (25);
            Thread::sleep (200);
            auto rec = q->getSong();
            check (rec != nullptr && pitchesOf (rec->parts[humcat::Lead]) == std::vector<int> { 62, 66, 69, 74, 69, 66, 62 } && rec->key == 2,
                   "recording from the track's input: heard it, D major");

            // a broken file doesn't crash it
            auto bad = File::getSpecialLocation (File::tempDirectory).getChildFile ("humcheck_bad.wav");
            bad.replaceWithText ("not audio");
            bool rejected = false;
            onMessageThread ([&] { String e; rejected = ! q->loadFile (bad, e) && e.isNotEmpty(); });
            check (rejected, "a file that isn't audio is refused with a message");
            bad.deleteFile();

            onMessageThread ([&] { p.reset(); q.reset(); });
            MessageManager::callAsync ([] { MessageManager::getInstance()->stopDispatchLoop(); });
        });
        MessageManager::getInstance()->runDispatchLoop();
        worker.join();
        wavFile.deleteFile();
    }

    std::printf (failures == 0 ? "HUM CHECK PASSED\n" : "HUM CHECK FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}

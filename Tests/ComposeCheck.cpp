// Composer check: writes songs for every genre and mood and checks the music theory rules,
// the parts, determinism, regenerate, endless mode, MIDI export and the real-time player.
// Usage: ComposeCheck [outDir] [seedsPerCombo] [renderDemos 0/1]
#include "../Shared/Source/Composer.h"
#include "../Shared/Source/SongRenderer.h"
#include <juce_events/juce_events.h>
#include <cstdio>
#include <map>

using namespace cat::muse;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

static bool inScale (const Song& s, int pitch)
{
    static const int steps[7][7] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 4, 5, 7, 9, 10 },
                                     { 0, 2, 4, 6, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 1, 3, 5, 7, 8, 10 } };
    const int d = ((pitch - s.key) % 12 + 12) % 12;
    for (int x : steps[s.scale]) if (x == d) return true;
    return false;
}

struct Stats { int melodyNotes = 0, strong = 0, strongChordTone = 0, steps = 0, leaps = 0, bigLeaps = 0; };

static void checkSong (const Song& s, const juce::String& what, Stats& st)
{
    CHECK (s.lengthBeats > 32, "%s: too short (%f)", what.toRawUTF8(), s.lengthBeats);
    CHECK (! s.sections.empty() && ! s.chords.empty(), "%s: no sections / chords", what.toRawUTF8());
    CHECK (s.title.isNotEmpty(), "%s: no title", what.toRawUTF8());
    double t = 0;
    for (auto& sec : s.sections) { CHECK (std::abs (sec.start - t) < 1e-6, "%s: section gap", what.toRawUTF8()); t = sec.end(); }
    for (auto& c : s.chords)
    {
        CHECK (c.voicing.size() >= 3, "%s: thin chord %s", what.toRawUTF8(), c.name.toRawUTF8());
        for (auto p : c.voicing) CHECK (p >= 40 && p <= 80, "%s: chord note out of range %d", what.toRawUTF8(), p);
    }
    // voice leading: chords move little
    for (size_t i = 1; i < s.chords.size(); ++i)
    {
        int move = 0;
        for (auto p : s.chords[i].voicing) { int d = 99; for (auto q : s.chords[i - 1].voicing) d = std::min (d, std::abs (p - q)); move += d; }
        CHECK (move <= 16, "%s: chords jump (%d semitones) %s -> %s", what.toRawUTF8(), move, s.chords[i - 1].name.toRawUTF8(), s.chords[i].name.toRawUTF8());
    }
    for (int p = 0; p < NumParts; ++p)
    {
        bool anyOn = false;
        for (auto& sec : s.sections) anyOn |= sec.on[(size_t) p];
        if (anyOn) CHECK (! s.parts[(size_t) p].empty(), "%s: part %s is on but empty", what.toRawUTF8(), partNames()[p].toRawUTF8());
        for (auto& n : s.parts[(size_t) p])
        {
            CHECK (n.start >= -0.01 && n.start < s.lengthBeats + 0.01, "%s: note outside the song", what.toRawUTF8());
            CHECK (n.length > 0 && n.length < 33, "%s: bad length %f", what.toRawUTF8(), n.length);
            CHECK (n.pitch >= 0 && n.pitch < 128 && n.velocity > 0 && n.velocity <= 1.0f, "%s: bad note", what.toRawUTF8());
        }
        for (size_t i = 1; i < s.parts[(size_t) p].size(); ++i)
            CHECK (s.parts[(size_t) p][i].start >= s.parts[(size_t) p][i - 1].start, "%s: unsorted", what.toRawUTF8());
    }
    // melody rules
    const auto& mel = s.parts[Melody];
    for (size_t i = 0; i < mel.size(); ++i)
    {
        const auto& n = mel[i];
        CHECK (inScale (s, n.pitch), "%s: melody note out of the scale", what.toRawUTF8());
        CHECK (n.pitch >= 45 && n.pitch <= 88, "%s: melody out of range %d", what.toRawUTF8(), n.pitch);
        if (i + 1 < mel.size()) CHECK (n.start + n.length <= mel[i + 1].start + 1e-6, "%s: melody overlaps", what.toRawUTF8());
        CHECK (juce::isPositiveAndBelow (n.syllable, s.syllables.size()), "%s: melody note without a syllable", what.toRawUTF8());
        ++st.melodyNotes;
        const int sec = s.sectionIndexAt (n.start + 1e-6);
        if (sec >= 0 && std::fmod (n.start - s.sections[(size_t) sec].start + 1e-6, 2.0) < 0.01)
        {
            ++st.strong;
            if (auto* c = s.chordAt (n.start + 1e-6); c != nullptr && c->isTone (n.pitch)) ++st.strongChordTone;
        }
        if (i > 0 && mel[i].start - mel[i - 1].start < 2.01)
        {
            const int d = std::abs (n.pitch - mel[i - 1].pitch);
            if (d <= 2) ++st.steps; else if (d > 7) ++st.bigLeaps; else ++st.leaps;
        }
    }
    CHECK (s.syllables.size() == (int) mel.size(), "%s: %d syllables for %d notes", what.toRawUTF8(), s.syllables.size(), (int) mel.size());
    // the lyrics text has exactly one syllable per note
    int textSyl = 0;
    for (auto& w : juce::StringArray::fromTokens (s.lyricsText, " \n", "")) if (w.isNotEmpty()) textSyl += 1 + w.retainCharacters ("-").length();
    CHECK (textSyl == (int) mel.size(), "%s: lyrics text has %d syllables for %d notes", what.toRawUTF8(), textSyl, (int) mel.size());
    // the chorus hook is the same every time
    std::vector<std::vector<int>> choruses;
    for (auto& sec : s.sections)
        if (sec.type == Chorus && sec.on[Melody])
        {
            std::vector<int> v;
            for (auto& n : mel) if (n.start >= sec.start - 0.1 && n.start < sec.end()) v.push_back (n.pitch);
            choruses.push_back (v);
        }
    for (size_t i = 1; i < choruses.size(); ++i)
        CHECK (choruses[i] == choruses[0], "%s: chorus %d differs from the first", what.toRawUTF8(), (int) i);
}

static bool sameSong (const Song& a, const Song& b)
{
    if (a.title != b.title || a.key != b.key || a.bpm != b.bpm || a.lyricsText != b.lyricsText) return false;
    for (int p = 0; p < NumParts; ++p)
    {
        if (a.parts[(size_t) p].size() != b.parts[(size_t) p].size()) return false;
        for (size_t i = 0; i < a.parts[(size_t) p].size(); ++i)
            if (a.parts[(size_t) p][i].pitch != b.parts[(size_t) p][i].pitch || std::abs (a.parts[(size_t) p][i].start - b.parts[(size_t) p][i].start) > 1e-9) return false;
    }
    return true;
}

static void checkPlayer (const Song& song, const juce::String& what)
{
    SongPlayer player;
    player.setSong (std::make_shared<Song> (song));
    const double sr = 48000.0, ppqPerSample = song.bpm / 60.0 / sr;
    std::array<juce::MidiBuffer, NumParts> midi;
    std::array<juce::MidiBuffer*, NumParts> ptrs;
    for (int p = 0; p < NumParts; ++p) ptrs[(size_t) p] = &midi[(size_t) p];
    std::array<int, NumParts> ons {}, offs {}, expected {};
    std::map<std::pair<int, int>, int> open;
    double ppq = 0;
    juce::Random rnd (7);
    // play once through the whole song with uneven blocks, then stop
    while (ppq < song.lengthBeats - 1e-6)
    {
        const int n = 64 + rnd.nextInt (900);
        const int len = (int) std::min<double> (n, std::floor ((song.lengthBeats - ppq) / ppqPerSample));
        if (len <= 0) break;
        for (auto& m : midi) m.clear();
        player.process (ppq, ppqPerSample, len, true, ptrs);
        for (int p = 0; p < NumParts; ++p)
            for (const auto meta : midi[(size_t) p])
            {
                const auto m = meta.getMessage();
                CHECK (meta.samplePosition >= 0 && meta.samplePosition < len, "%s: event outside the block", what.toRawUTF8());
                CHECK (m.getChannel() == partChannel (p), "%s: wrong channel", what.toRawUTF8());
                if (m.isNoteOn()) { ++ons[(size_t) p]; ++open[{ p, m.getNoteNumber() }]; }
                else if (m.isNoteOff()) { ++offs[(size_t) p]; --open[{ p, m.getNoteNumber() }]; }
            }
        ppq += len * ppqPerSample;
    }
    for (auto& m : midi) m.clear();
    player.process (ppq, ppqPerSample, 256, false, ptrs);
    for (int p = 0; p < NumParts; ++p)
        for (const auto meta : midi[(size_t) p])
            if (meta.getMessage().isNoteOff()) { ++offs[(size_t) p]; --open[{ p, meta.getMessage().getNoteNumber() }]; }
    for (int p = 0; p < NumParts; ++p)
    {
        expected[(size_t) p] = (int) song.parts[(size_t) p].size();
        CHECK (ons[(size_t) p] == expected[(size_t) p], "%s: player played %d of %d %s notes", what.toRawUTF8(), ons[(size_t) p], expected[(size_t) p], partNames()[p].toRawUTF8());
    }
    for (auto& [k, v] : open) CHECK (v <= 0, "%s: stuck note part %d pitch %d", what.toRawUTF8(), k.first, k.second);
}

static std::pair<float, double> measure (const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    float peak = 0; double sum = 0; juce::int64 n = 0;
    if (rd != nullptr)
    {
        juce::AudioBuffer<float> b (2, 65536);
        for (juce::int64 pos = 0; pos < rd->lengthInSamples; pos += 65536)
        {
            const int len = (int) std::min<juce::int64> (65536, rd->lengthInSamples - pos);
            rd->read (&b, 0, len, pos, true, true);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < len; ++i) { const float x = b.getSample (c, i); peak = std::max (peak, std::abs (x)); sum += x * x; }
            n += len * 2;
        }
    }
    return { peak, n > 0 ? std::sqrt (sum / (double) n) : 0.0 };
}

// How repetitive a song is: distinct bars per part, distinct 4-bar blocks of the whole mix,
// and the longest run of bars where nothing at all changes.
struct Variety { double partDistinct = 0, blockDistinct = 0, longestSame = 0; int n = 0; };
static juce::String barPrint (const Song& s, int part, int bar)
{
    juce::String f;
    const double a = bar * 4.0, b = a + 4.0;
    for (auto& n : s.parts[(size_t) part])
        if (n.start >= a - 1e-6 && n.start < b - 1e-6)
            f << juce::String ((n.start - a), 2) << ":" << n.pitch << ":" << juce::String (n.length, 1) << " ";
    return f;
}
static void measureVariety (const Song& s, Variety& v)
{
    const int bars = (int) std::ceil (s.lengthBeats / 4.0);
    double partSum = 0; int partCount = 0;
    for (int p = 0; p < NumParts; ++p)
    {
        juce::StringArray seen; int playing = 0;
        for (int b = 0; b < bars; ++b) { auto f = barPrint (s, p, b); if (f.isEmpty()) continue; ++playing; seen.addIfNotAlreadyThere (f); }
        if (playing > 0) { partSum += (double) seen.size() / playing; ++partCount; }
    }
    juce::StringArray mixBars;
    for (int b = 0; b < bars; ++b) { juce::String f; for (int p = 0; p < NumParts; ++p) f << p << "|" << barPrint (s, p, b); mixBars.add (f); }
    juce::StringArray blocks; int blockCount = 0;
    for (int b = 0; b + 4 <= bars; b += 4) { blocks.addIfNotAlreadyThere (mixBars[b] + mixBars[b+1] + mixBars[b+2] + mixBars[b+3]); ++blockCount; }
    int longest = 1, run = 1;
    for (int b = 1; b < bars; ++b) { run = mixBars[b] == mixBars[b - 1] ? run + 1 : 1; longest = juce::jmax (longest, run); }
    v.partDistinct += partCount > 0 ? partSum / partCount : 0;
    v.blockDistinct += blockCount > 0 ? (double) blocks.size() / blockCount : 0;
    v.longestSame += longest;
    ++v.n;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    const int seeds = argc > 2 ? juce::jmax (1, atoi (argv[2])) : 3;
    const bool demos = argc > 3 && atoi (argv[3]) != 0;
    const bool stems = argc > 4 && atoi (argv[4]) != 0;
    outDir.createDirectory();

    Stats st;
    std::array<Variety, NumGenres> variety;
    int songs = 0;
    juce::StringArray titles;
    for (int g = 0; g < NumGenres; ++g)
        for (int m = 0; m < NumMoods; ++m)
            for (int k = 0; k < seeds; ++k)
            {
                Settings set;
                set.seed = (juce::uint32) (1000 + g * 97 + m * 13 + k * 7919);
                set.genre = g; set.mood = m;
                const auto what = genreNames()[g] + "/" + moodNames()[m] + "/" + juce::String (set.seed);
                auto s = compose (set);
                checkSong (s, what, st);
                measureVariety (s, variety[(size_t) g]);
                ++songs;
                titles.addIfNotAlreadyThere (s.title);
                CHECK (sameSong (s, compose (set)), "%s: not deterministic", what.toRawUTF8());
                if (k == 0)
                {
                    checkPlayer (s, what);
                    for (int t = 0; t < NumTargets; ++t)
                    {
                        auto r = s;
                        regenerate (r, t, 4242);
                        checkSong (r, what + " regen " + targetNames()[t], st);
                        if (t == NewDrums) CHECK (r.parts[Melody].size() == s.parts[Melody].size() && r.lyricsText == s.lyricsText, "%s: drums regen changed the melody", what.toRawUTF8());
                        if (t == NewBass || t == NewCounter || t == NewDrums)
                            CHECK (r.parts[Chords].size() == s.parts[Chords].size(), "%s: regen changed the chords", what.toRawUTF8());
                        if (t != NewLyrics) { auto r2 = s; regenerate (r2, t, 4242); CHECK (sameSong (r, r2), "%s: regen not deterministic", what.toRawUTF8()); }
                        if (! r.settings.endless) CHECK (sameSong (r, compose (r.settings)), "%s: regen %d can't be rebuilt from its settings", what.toRawUTF8(), t);
                    }
                    // endless: pieces follow on, keep the key and the hook
                    Settings e = set; e.endless = true;
                    auto piece = compose (e);
                    checkSong (piece, what + " endless 0", st);
                    for (int c = 1; c <= 4; ++c)
                    {
                        auto nextPiece = composeNext (piece);
                        checkSong (nextPiece, what + " endless " + juce::String (c), st);
                        CHECK (nextPiece.key == piece.key && nextPiece.bpm == piece.bpm && nextPiece.chunk == c, "%s: endless lost the key", what.toRawUTF8());
                        CHECK (nextPiece.sections.front().type != Intro, "%s: endless intro again", what.toRawUTF8());
                        piece = nextPiece;
                    }
                    auto midi = toMidiFile (s);
                    CHECK (midi.getNumTracks() == NumParts + 1, "%s: midi tracks", what.toRawUTF8());
                    juce::MemoryOutputStream mo;
                    CHECK (midi.writeTo (mo) && mo.getDataSize() > 200, "%s: midi write", what.toRawUTF8());
                }
            }

    // the endless player moves from piece to piece by itself
    {
        Settings e; e.seed = 77; e.genre = House; e.endless = true;
        auto first = std::make_shared<Song> (compose (e));
        SongPlayer player;
        player.setSong (first);
        std::shared_ptr<const Song> last = first;
        const double sr = 44100.0, pps = first->bpm / 60.0 / sr;
        std::array<juce::MidiBuffer, NumParts> midi;
        std::array<juce::MidiBuffer*, NumParts> ptrs;
        for (int p = 0; p < NumParts; ++p) ptrs[(size_t) p] = &midi[(size_t) p];
        double ppq = 0;
        int notes = 0;
        while (ppq < first->lengthBeats * 3.5)
        {
            for (auto& m : midi) m.clear();
            player.process (ppq, pps, 512, true, ptrs);
            for (auto& m : midi) for (const auto meta : m) notes += meta.getMessage().isNoteOn() ? 1 : 0;
            ppq += 512 * pps;
            if (player.needNext()) { last = std::make_shared<Song> (composeNext (*last)); player.queueNext (last); }
        }
        CHECK (player.getCurrentChunk() >= 2, "endless player stayed on piece %d", player.getCurrentChunk());
        CHECK (notes > 500, "endless player played only %d notes", notes);
        std::printf ("endless player: reached piece %d, %d notes\n", player.getCurrentChunk(), notes);
    }

    if (demos)
    {
        for (int g = 0; g < NumGenres; ++g)
        {
            Settings set; set.seed = 2026 + (juce::uint32) g; set.genre = g; set.mood = (g * 5) % NumMoods;
            auto s = compose (set);
            auto f = outDir.getChildFile ("demo_" + genreNames()[g].removeCharacters ("-") + ".wav");
            CHECK (SongRenderer::renderToWav (s, defaultSounds (g), f), "render %s", f.getFullPathName().toRawUTF8());
            auto [peak, rms] = measure (f);
            if (stems)
            {
                juce::String line;
                for (int p = 0; p < NumParts; ++p)
                {
                    auto sf = outDir.getChildFile ("stem.wav");
                    SongRenderer::renderToWav (s, defaultSounds (g), sf, 44100.0, p);
                    auto [pp, pr] = measure (sf);
                    line << partNames()[p] << " " << juce::String (pp, 2) << "/" << juce::String (pr, 3) << "  ";
                    sf.deleteFile();
                }
                std::printf ("  stems %-10s %s\n", genreNames()[g].toRawUTF8(), line.toRawUTF8());
            }
            std::printf ("demo %-10s \"%s\" %s %s %d bpm, %.0f s, peak %.2f rms %.3f\n", genreNames()[g].toRawUTF8(), s.title.toRawUTF8(),
                         keyName (s.key).toRawUTF8(), scaleName (s.scale).toRawUTF8(), (int) s.bpm, s.lengthBeats * 60.0 / s.bpm, peak, rms);
            CHECK (peak > 0.05f && peak <= 1.0f && rms > 0.01, "demo %s level peak %.2f rms %.3f", genreNames()[g].toRawUTF8(), peak, rms);
            auto mf = outDir.getChildFile ("demo_" + genreNames()[g].removeCharacters ("-") + ".mid");
            mf.deleteFile();
            if (auto os = mf.createOutputStream()) toMidiFile (s).writeTo (*os);
            outDir.getChildFile ("demo_" + genreNames()[g].removeCharacters ("-") + ".txt")
                .replaceWithText (s.title + "\n" + keyName (s.key) + " " + scaleName (s.scale) + ", " + juce::String ((int) s.bpm) + " bpm\n\n" + s.lyricsText);
        }
    }

    // a sample song to read
    {
        Settings set; set.seed = 12345; set.genre = Pop; set.mood = Happy;
        auto s = compose (set);
        std::printf ("\n\"%s\" - %s %s, %d bpm\n", s.title.toRawUTF8(), keyName (s.key).toRawUTF8(), scaleName (s.scale).toRawUTF8(), (int) s.bpm);
        for (auto& sec : s.sections)
        {
            juce::String chords;
            for (auto& c : s.chords) if (c.start >= sec.start && c.start < sec.end() && chords.length() < 70) chords << c.name << " ";
            std::printf ("  %-10s %2d bars  energy %.2f  %s\n", sectionName (sec.type).toRawUTF8(), sec.bars, sec.energy, chords.toRawUTF8());
        }
        for (int i = 0; i < juce::jmin (12, s.lyricLines.size()); ++i) std::printf ("    %s\n", s.lyricLines[i].toRawUTF8());
    }

    for (int g = 0; g < NumGenres; ++g)
    {
        const auto& v = variety[(size_t) g];
        std::printf ("variety %-10s distinct bars per part %3.0f%%, distinct 4-bar blocks %3.0f%%, longest unchanged run %.1f bars\n",
                     genreNames()[g].toRawUTF8(), 100.0 * v.partDistinct / v.n, 100.0 * v.blockDistinct / v.n, v.longestSame / v.n);
    }
    std::printf ("\n%d songs, %d different titles. melody: %d notes, %.0f%% chord tones on strong beats, steps %.0f%%, leaps %.0f%%, leaps > 5th %.1f%%\n",
                 songs, titles.size(), st.melodyNotes, 100.0 * st.strongChordTone / juce::jmax (1, st.strong),
                 100.0 * st.steps / juce::jmax (1, st.steps + st.leaps + st.bigLeaps), 100.0 * st.leaps / juce::jmax (1, st.steps + st.leaps + st.bigLeaps),
                 100.0 * st.bigLeaps / juce::jmax (1, st.steps + st.leaps + st.bigLeaps));
    CHECK (st.strongChordTone > st.strong * 0.85, "too few chord tones on strong beats");
    CHECK (st.bigLeaps * 10 < st.steps, "too many big leaps");
    std::printf (failures == 0 ? "COMPOSE CHECK PASSED\n" : "COMPOSE CHECK FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}

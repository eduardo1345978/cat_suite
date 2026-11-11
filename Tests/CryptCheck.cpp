// Crypt's check: the synth plays in tune, chords / arpeggios / sweep / release work, every preset is safe; the
// sound-effect maker makes every kind (the same seed = the same sound), saves real .wav files with numbered names;
// Crypt FX crushes to few levels and its MIX 0 is the dry sound.   CryptCheck --render <dir>  writes demo .wav files.
#include "PluginProcessor.h"
#include "../CryptFX/Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <set>

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
struct Ev { double at; MidiMessage m; };

struct Head : AudioPlayHead
{
    double bpm = 120.0;
    Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm (bpm); p.setIsPlaying (true); return p; }
};

static void set (AudioProcessor& p, AudioProcessorValueTreeState& s, const char* id, float v)
{
    ignoreUnused (p);
    auto* prm = s.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static std::pair<Sig, Sig> run (AudioProcessor& p, double seconds, std::vector<Ev> evs, std::function<float (int)> input = {})
{
    Sig l, r;
    AudioBuffer<float> buf (2, block);
    Head head;
    p.setPlayHead (&head);
    const int total = (int) (seconds * sr);
    for (int pos = 0; pos < total; pos += block)
    {
        MidiBuffer midi;
        for (auto& e : evs) { const int s = (int) (e.at * sr); if (s >= pos && s < pos + block) midi.addEvent (e.m, s - pos); }
        buf.clear();
        if (input) for (int i = 0; i < block; ++i) { const float x = input (pos + i); buf.setSample (0, i, x); buf.setSample (1, i, x); }
        p.processBlock (buf, midi);
        for (int i = 0; i < block; ++i) { l.push_back (buf.getSample (0, i)); r.push_back (buf.getSample (1, i)); }
    }
    p.setPlayHead (nullptr);
    return { l, r };
}

static Ev on (double t, int n, float v = 0.8f) { return { t, MidiMessage::noteOn (1, n, v) }; }
static Ev off (double t, int n) { return { t, MidiMessage::noteOff (1, n) }; }
static float rms (const Sig& s, double a, double b)
{
    const int i0 = jlimit (0, (int) s.size(), (int) (a * sr)), i1 = jlimit (0, (int) s.size(), (int) (b * sr));
    double e = 0.0; for (int i = i0; i < i1; ++i) e += (double) s[(size_t) i] * s[(size_t) i];
    return i1 > i0 ? (float) std::sqrt (e / (i1 - i0)) : 0.0f;
}
static float peak (const Sig& s) { float m = 0.0f; for (float x : s) m = jmax (m, std::abs (x)); return m; }
static bool finite (const Sig& s) { for (float x : s) if (! std::isfinite (x)) return false; return true; }

/** The pitch from the zero crossings going up (fine for chip waves). */
static float pitch (const Sig& s, double a, double b)
{
    const int i0 = (int) (a * sr), i1 = jmin ((int) s.size(), (int) (b * sr));
    int first = -1, last = -1, k = 0;
    for (int i = i0 + 1; i < i1; ++i)
        if (s[(size_t) i - 1] < 0.0f && s[(size_t) i] >= 0.0f) { if (first < 0) first = i; last = i; ++k; }
    return k > 1 ? (float) ((k - 1) * sr / (last - first)) : 0.0f;
}

static std::unique_ptr<CryptProcessor> makeSynth()
{
    auto p = std::make_unique<CryptProcessor>();
    p->setPlayConfigDetails (0, 2, sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

static void clean (CryptProcessor& p)
{
    for (auto [id, v] : { std::pair { "crypt", 0.0f }, { "echo", 0.0f }, { "gloom", 0.0f }, { "drive", 0.0f }, { "vibrato", 0.0f }, { "detune", 0.0f }, { "sub", 0.0f } })
        set (p, p.apvts, id, v);
}

static void writeWav (const File& f, const Sig& l, const Sig& r)
{
    f.deleteFile();
    std::unique_ptr<OutputStream> out (f.createOutputStream());
    WavAudioFormat wav;
    if (auto w = wav.createWriterFor (out, AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24)))
    {
        AudioBuffer<float> b (2, (int) l.size());
        b.copyFrom (0, 0, l.data(), (int) l.size());
        b.copyFrom (1, 0, r.data(), (int) r.size());
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juce;

    {   // silence, then a square wave in tune
        auto p = makeSynth();
        clean (*p);
        auto [l0, r0] = run (*p, 0.3, {});
        check (peak (l0) < 1.0e-6f, "silent without notes");
        set (*p, p->apvts, "wave", 2.0f);
        auto [l, r] = run (*p, 1.0, { on (0.0, 57), off (0.7, 57) });
        const float hz = pitch (l, 0.2, 0.6);
        check (std::abs (hz / 220.0f - 1.0f) < 0.02f, "a square wave in tune (" + String (hz, 1) + " Hz, want 220)");
        check (rms (l, 0.2, 0.6) > 0.03f && finite (l), "loud enough and clean");
        check (rms (l, 0.95, 1.0) < 1.0e-4f, "it stops after the release");
        for (int w = 0; w < 9; ++w)
        {
            set (*p, p->apvts, "wave", (float) w);
            auto [lw, rw] = run (*p, 0.5, { on (0.0, 60), off (0.3, 60) });
            if (! finite (lw) || rms (lw, 0.05, 0.3) < 0.01f) check (false, "wave " + tomb::waveNames()[w] + " sounds");
        }
        check (true, "all 9 waves sound");
    }

    {   // the arpeggio: up a minor chord in 1/16 at 120 BPM = a new note every 125 ms
        auto p = makeSynth();
        clean (*p);
        set (*p, p->apvts, "wave", 2.0f);
        set (*p, p->apvts, "chord", 1.0f); set (*p, p->apvts, "arp", 1.0f); set (*p, p->apvts, "arpRate", 1.0f);
        auto [l, r] = run (*p, 1.0, { on (0.0, 57), off (0.9, 57) });
        const float a = pitch (l, 0.01, 0.12), b = pitch (l, 0.135, 0.245), c = pitch (l, 0.26, 0.37);
        check (std::abs (a / 220.0f - 1.0f) < 0.03f && std::abs (b / 261.6f - 1.0f) < 0.03f && std::abs (c / 329.6f - 1.0f) < 0.03f,
               "ARP up a minor chord, 1/16 at 120 BPM (" + String (a, 0) + ", " + String (b, 0) + ", " + String (c, 0) + " Hz)");
        set (*p, p->apvts, "arp", 0.0f);
        auto [l2, r2] = run (*p, 0.6, { on (0.0, 57), off (0.4, 57) });
        check (rms (l2, 0.1, 0.3) > rms (l, 0.1, 0.3) * 1.2f, "the chord all at once is fuller than one arpeggio note");
    }

    {   // the sweep falls into the note
        auto p = makeSynth();
        clean (*p);
        set (*p, p->apvts, "wave", 2.0f); set (*p, p->apvts, "sweep", 0.5f);
        auto [l, r] = run (*p, 0.8, { on (0.0, 57), off (0.6, 57) });
        const float early = pitch (l, 0.0, 0.03), late = pitch (l, 0.4, 0.55);
        check (early > late * 1.5f && std::abs (late / 220.0f - 1.0f) < 0.03f, "SWEEP falls into the note (" + String (early, 0) + " -> " + String (late, 0) + " Hz)");
    }

    {   // 12 notes on 8 voices, and every preset is safe
        auto p = makeSynth();
        std::vector<Ev> evs;
        for (int k = 0; k < 12; ++k) evs.push_back (on (0.01 * k, 40 + k * 3));
        auto [l, r] = run (*p, 1.0, evs);
        check (finite (l) && peak (l) <= 1.0f, "12 notes at once: clean and under 0 dB");
        bool ok = true; String bad;
        for (int i = 0; i < p->getNumPrograms(); ++i)
        {
            p->setCurrentProgram (i);
            auto [a, b] = run (*p, 1.2, { on (0.0, 48), on (0.0, 55), on (0.0, 60, 1.0f), off (0.8, 48), off (0.8, 55), off (0.8, 60) });
            if (! finite (a) || ! finite (b) || peak (a) > 1.0f || rms (a, 0.0, 0.8) < 0.003f) { ok = false; bad << p->getProgramName (i) << " "; }
            run (*p, 0.1, { { 0.0, MidiMessage::allNotesOff (1) } });
            run (*p, 4.0, {});
        }
        check (ok, "all " + String (p->getNumPrograms()) + " presets sound and are safe " + bad);
    }

    {   // the sound-effect maker
        bool allOk = true; String bad;
        for (int k = 0; k < (int) tomb::Kind::count; ++k)
            for (juce::uint32 seed : { 1u, 77u, 9001u })
            {
                const auto s = tomb::generate ((tomb::Kind) k, seed, 0.5f);
                const auto b = tomb::render (s, 44100.0);
                const float pk = b.getMagnitude (0, 0, b.getNumSamples());
                const float secs = (float) b.getNumSamples() / 44100.0f;
                bool fin = true;
                for (int i = 0; i < b.getNumSamples(); ++i) fin &= std::isfinite (b.getSample (0, i));
                if (! fin || std::abs (pk - 0.89f) > 0.01f || secs < 0.03f || secs > 7.0f) { allOk = false; bad << tomb::kindNames()[k] << " (" << String (secs, 2) << " s) "; }
            }
        check (allOk, "SFX: all 14 kinds x 3 seeds make a clean sound at -1 dB, 0.03 .. 7 s " + bad);

        const auto a = tomb::render (tomb::generate (tomb::Kind::Spell, 42, 0.5f), 44100.0);
        const auto b = tomb::render (tomb::generate (tomb::Kind::Spell, 42, 0.5f), 44100.0);
        bool same = a.getNumSamples() == b.getNumSamples();
        for (int i = 0; same && i < a.getNumSamples(); ++i) same = a.getSample (0, i) == b.getSample (0, i);
        check (same, "SFX: the same seed makes exactly the same sound");
        const auto c = tomb::render (tomb::generate (tomb::Kind::Spell, 43, 0.5f), 44100.0);
        check (c.getNumSamples() != a.getNumSamples() || c.getSample (0, c.getNumSamples() / 3) != a.getSample (0, a.getNumSamples() / 3), "SFX: another seed, another sound");
        const auto s = tomb::generate (tomb::Kind::Door, 5, 0.5f);
        const auto back = tomb::Sfx::fromVar (JSON::parse (JSON::toString (s.toVar())));
        const auto r1 = tomb::render (s, 44100.0), r2 = tomb::render (back, 44100.0);
        bool sameBack = r1.getNumSamples() == r2.getNumSamples();
        for (int i = 0; sameBack && i < r1.getNumSamples(); i += 7) sameBack = std::abs (r1.getSample (0, i) - r2.getSample (0, i)) < 1.0e-4f;
        check (sameBack, "SFX: saved in the session and read back, it is the same sound");
        const auto m = tomb::mutate (s, 99);
        check (m.kind == s.kind && m.freq != s.freq, "SFX: MUTATE keeps the kind, changes the sound");
        const auto lo = tomb::generate (tomb::Kind::Hit, 3, 0.0f), hi = tomb::generate (tomb::Kind::Hit, 3, 1.0f);
        check (hi.freq < lo.freq && hi.room > lo.room, "SFX: DARKNESS = lower and more room");

        // saving into the game folder: sword_01.wav, sword_02.wav, real 44.1 kHz 16-bit mono wav files
        auto p = makeSynth();
        const auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("crypt-check-" + String (Time::currentTimeMillis()));
        p->sfxFolder = dir;
        p->newSfx (tomb::Kind::Sword);
        const auto f1 = p->saveSfx();
        const auto f2 = p->saveSfx();
        check (f1.getFileName() == "sword_01.wav" && f2.getFileName() == "sword_02.wav", "SAVE TO GAME: " + f1.getFileName() + ", " + f2.getFileName());
        AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> rd (fm.createReaderFor (f1));
        check (rd != nullptr && rd->sampleRate == 44100.0 && rd->numChannels == 1 && rd->bitsPerSample == 16 && rd->lengthInSamples > 1000,
               "the file is a real wav (44.1 kHz, mono, 16 bit)");
        p->newSfx (tomb::Kind::LevelUp);
        check (p->saveSfx().getFileName() == "level_up_01.wav" && p->history.size() == 2, "each kind counts on its own (level_up_01), BACK remembers");

        // the preview plays through the plug-in's output
        p->playPreview (p->renderSfx (sr));
        auto [l, rr] = run (*p, 0.5, {});
        check (rms (l, 0.0, 0.3) > 0.01f, "PLAY: the preview comes out of the plug-in");

        // the state keeps the effect and the folder
        MemoryBlock mb;
        p->getStateInformation (mb);
        auto q = makeSynth();
        q->setStateInformation (mb.getData(), (int) mb.getSize());
        check (q->sfx.kind == tomb::Kind::LevelUp && q->sfxFolder == dir, "the session remembers the sound and the game folder");
        dir.deleteRecursively();
    }

    {   // Crypt FX
        auto fx = std::make_unique<CryptFxProcessor>();
        fx->setPlayConfigDetails (2, 2, sr, block);
        fx->prepareToPlay (sr, block);
        auto setF = [&fx] (const char* id, float v) { auto* prm = fx->apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        for (auto [id, v] : { std::pair { "crypt", 0.0f }, { "echo", 0.0f }, { "gloom", 0.0f }, { "drive", 0.0f }, { "rate", 1.0f }, { "bits", 3.0f }, { "mix", 1.0f } }) setF (id, v);
        auto sine = [] (int i) { return 0.7f * (float) std::sin (i * 0.01); };
        auto [l, r] = run (*fx, 1.0, {}, sine);
        std::set<int> levels;
        for (size_t i = (size_t) (0.3 * sr); i < l.size(); ++i) levels.insert (roundToInt (l[i] * 1000.0f));
        check (levels.size() <= 12, "BITS 3: the sine becomes a few steps (" + String ((int) levels.size()) + " levels)");
        setF ("mix", 0.0f);
        run (*fx, 0.2, {}, sine);
        auto [l2, r2] = run (*fx, 0.5, {}, sine);
        float err = 0.0f;
        for (size_t i = 2000; i < l2.size(); ++i) err = jmax (err, std::abs (l2[i] - sine ((int) (i + (size_t) (0.2 * sr) * 0) )));
        // the input index restarts each run, so compare against the same run's input
        float err2 = 0.0f;
        for (size_t i = 2000; i < l2.size(); ++i) err2 = jmax (err2, std::abs (l2[i] - sine ((int) i)));
        check (err2 < 1.0e-3f, "MIX 0: the sound as it came in");
        ignoreUnused (err);
        bool ok = true;
        for (int i = 0; i < fx->getNumPrograms(); ++i)
        {
            fx->setCurrentProgram (i);
            auto [a, b] = run (*fx, 0.8, {}, [] (int k) { return (k / 4800) % 2 == 0 ? 0.9f * (float) std::sin (k * 0.05) : 0.0f; });
            if (! finite (a) || peak (a) > 1.0f || rms (a, 0.0, 0.8) < 0.005f) ok = false;
        }
        check (ok, "all " + String (fx->getNumPrograms()) + " Crypt FX presets are safe and pass the sound");
    }

    if (argc > 2 && String (argv[1]) == "--render")
    {
        const File dir { String (argv[2]) };
        dir.createDirectory();
        for (int k = 0; k < (int) tomb::Kind::count; ++k)
            for (juce::uint32 seed : { 11u, 12u })
            {
                const auto s = tomb::generate ((tomb::Kind) k, seed, 0.5f);
                tomb::saveWav (tomb::render (s, 44100.0), 44100.0, dir.getChildFile ("sfx-" + tomb::kindFileName ((tomb::Kind) k) + "-" + String ((int) seed) + ".wav"));
            }
        const int tune[] = { 57, 60, 64, 63, 62, 60, 59, 60, 57, 52, 53, 57 };
        for (int pr : { 0, 2, 3, 4, 6, 7, 9, 17 })
        {
            auto p = makeSynth();
            p->setCurrentProgram (pr);
            std::vector<Ev> e;
            const bool pad = pr == 3 || pr == 4;
            if (pad) { for (int c = 0; c < 3; ++c) { e.push_back (on (c * 2.0, 45 + c * 2)); e.push_back (off (c * 2.0 + 1.8, 45 + c * 2)); } }
            else for (int i = 0; i < 12; ++i) { e.push_back (on (i * 0.4, tune[i] + (pr == 0 ? 12 : 0))); e.push_back (off (i * 0.4 + 0.34, tune[i] + (pr == 0 ? 12 : 0))); }
            auto [l, r] = run (*p, pad ? 7.5 : 6.5, e);
            writeWav (dir.getChildFile ("synth-" + p->getProgramName (pr).toLowerCase().replace (" ", "-") + ".wav"), l, r);
        }
        std::printf ("demo wavs in %s\n", dir.getFullPathName().toRawUTF8());
    }

    std::printf (failures == 0 ? "ALL CRYPT CHECKS PASSED\n" : "%d CRYPT CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}

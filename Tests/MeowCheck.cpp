// Meow-Cat's check: a note really meows (at the right pitch, rising and falling), SING holds and lets go, SIZE moves
// the mouth, a hard hit hisses, PURR purrs at ~20-30 Hz, CHATTER chatters in time, the choir is wide, bend / octave /
// sustain / all-notes-off work, 10 notes x 6 cats stay safe and fast, every preset is safe, the state comes back.
//   MeowCheck --render <dir>   also writes a few demo .wav files
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

static double sr = 48000.0;
constexpr int block = 256;

struct Head : AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0; bool playing = false;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setIsPlaying (playing); p.setPpqPosition (ppq);
        return p;
    }
};

static std::unique_ptr<MeowCatProcessor> make (double rate = 48000.0)
{
    sr = rate;
    auto p = std::make_unique<MeowCatProcessor>();
    p->setPlayConfigDetails (0, 2, rate, block);
    p->prepareToPlay (rate, block);
    return p;
}

static void set (MeowCatProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void plain (MeowCatProcessor& p)
{
    set (p, "chaos", 0.0f); set (p, "alley", 0.0f); set (p, "hiss", 0.0f); set (p, "purr", 0.0f); set (p, "choir", 1.0f);
}

using Sig = std::vector<float>;
struct Ev { double at; MidiMessage m; };

/** Renders `seconds`, sending the events at their times. */
static std::pair<Sig, Sig> run (MeowCatProcessor& p, double seconds, std::vector<Ev> evs, Head* head = nullptr)
{
    Sig l, r;
    AudioBuffer<float> buf (2, block);
    const int total = (int) (seconds * sr);
    if (head != nullptr) p.setPlayHead (head);
    for (int pos = 0; pos < total; pos += block)
    {
        MidiBuffer midi;
        for (auto& e : evs)
        {
            const int s = (int) (e.at * sr);
            if (s >= pos && s < pos + block) midi.addEvent (e.m, s - pos);
        }
        buf.clear();
        p.processBlock (buf, midi);
        if (head != nullptr) head->ppq += block / sr * head->bpm / 60.0;
        for (int i = 0; i < block; ++i) { l.push_back (buf.getSample (0, i)); r.push_back (buf.getSample (1, i)); }
    }
    p.setPlayHead (nullptr);
    return { l, r };
}

static Ev on (double t, int note, float vel = 0.7f) { return { t, MidiMessage::noteOn (1, note, vel) }; }
static Ev off (double t, int note) { return { t, MidiMessage::noteOff (1, note) }; }

static float rms (const Sig& s, double from, double to)
{
    const int a = jlimit (0, (int) s.size(), (int) (from * sr)), b = jlimit (0, (int) s.size(), (int) (to * sr));
    double e = 0.0;
    for (int i = a; i < b; ++i) e += (double) s[(size_t) i] * s[(size_t) i];
    return b > a ? (float) std::sqrt (e / (b - a)) : 0.0f;
}

static float peak (const Sig& s) { float m = 0.0f; for (float x : s) m = jmax (m, std::abs (x)); return m; }
static bool finite (const Sig& s) { for (float x : s) if (! std::isfinite (x)) return false; return true; }

/** The pitch (Hz) around `at` (autocorrelation, 60 .. 2000 Hz). */
static float pitchAt (const Sig& s, double at, double win = 0.06)
{
    const int a = (int) (at * sr), n = (int) (win * sr);
    if (a + n * 2 >= (int) s.size()) return 0.0f;
    const int minLag = (int) (sr / 2000.0), maxLag = (int) (sr / 60.0);
    double best = 0.0; int bestLag = 0;
    std::vector<double> c ((size_t) maxLag + 2, 0.0);
    double e0 = 0.0;
    for (int i = 0; i < n; ++i) e0 += (double) s[(size_t) (a + i)] * s[(size_t) (a + i)];
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double x = 0.0, e1 = 0.0;
        for (int i = 0; i < n; ++i) { x += (double) s[(size_t) (a + i)] * s[(size_t) (a + i + lag)]; e1 += (double) s[(size_t) (a + i + lag)] * s[(size_t) (a + i + lag)]; }
        c[(size_t) lag] = x / std::sqrt (e0 * e1 + 1.0e-20);
    }
    // the first strong peak (not a multiple of the period)
    for (int lag = minLag; lag <= maxLag; ++lag) best = jmax (best, c[(size_t) lag]);
    for (int lag = minLag + 1; lag < maxLag; ++lag)
        if (c[(size_t) lag] >= 0.85 * best && c[(size_t) lag] >= c[(size_t) lag - 1] && c[(size_t) lag] >= c[(size_t) lag + 1]) { bestLag = lag; break; }
    if (bestLag == 0) return 0.0f;
    // parabolic refinement
    const double y0 = c[(size_t) bestLag - 1], y1 = c[(size_t) bestLag], y2 = c[(size_t) bestLag + 1];
    const double d = 0.5 * (y0 - y2) / (y0 - 2.0 * y1 + y2 + 1.0e-12);
    return (float) (sr / (bestLag + d));
}

/** Zero crossings per sample (noise ~0.5, a voice much less). */
static float crossings (const Sig& s, double from, double to)
{
    const int a = (int) (from * sr), b = jmin ((int) s.size(), (int) (to * sr));
    int k = 0;
    for (int i = a + 1; i < b; ++i) if ((s[(size_t) i] >= 0.0f) != (s[(size_t) i - 1] >= 0.0f)) ++k;
    return (float) k / (float) jmax (1, b - a);
}

/** The brightness: the average frequency of the spectrum (Hz), by the difference signal's level. */
static float brightness (const Sig& s, double from, double to)
{
    const int a = (int) (from * sr), b = jmin ((int) s.size(), (int) (to * sr));
    double e = 0.0, d = 0.0;
    for (int i = a + 1; i < b; ++i) { e += (double) s[(size_t) i] * s[(size_t) i]; const double x = s[(size_t) i] - s[(size_t) i - 1]; d += x * x; }
    return (float) (sr / MathConstants<double>::twoPi * std::sqrt (d / (e + 1.0e-20)));
}

static void writeWav (const File& f, const Sig& l, const Sig& r)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<OutputStream> out (f.createOutputStream());
    if (out == nullptr) return;
    auto opts = AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24);
    if (auto w = wav.createWriterFor (out, opts))
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

    {   // silence when nothing plays
        auto p = make();
        auto [l, r] = run (*p, 0.5, {});
        check (peak (l) < 1.0e-9f && peak (r) < 1.0e-9f, "silent without notes");
    }

    {   // one meow: sounds, at a sensible level, and ends by itself
        auto p = make();
        plain (*p);
        const float secs = meow::meowSeconds (0.35f, 0.45f);
        auto [l, r] = run (*p, secs + 1.0, { on (0.0, 72) });     // the key is held all the time
        const float level = rms (l, 0.05, secs * 0.9);
        check (finite (l) && finite (r), "a meow is clean (no NaN)");
        check (level > 0.02f && level < 0.4f, "a meow is loud enough, not too loud (rms " + String (level, 3) + ")");
        check (rms (l, secs + 0.1, secs + 1.0) < 1.0e-4f, "MEOW: one meow per note, it ends by itself (" + String (secs, 2) + " s)");
        check (p->engine.meows.load() == 1, "counts the meow for the screen");

        // it rises and then falls (pitch at the peak vs at the end)
        const float early = pitchAt (l, secs * 0.1), top = pitchAt (l, secs * 0.32), late = pitchAt (l, secs * 0.78);
        check (top > early * 1.03f && late < top * 0.9f, "the meow's pitch rises, then falls (" + String (early, 0) + " > " + String (top, 0) + " > " + String (late, 0) + " Hz)");
    }

    {   // SING: holds at the note's pitch while the key is down, says "ow" and stops when it is let go
        for (int note : { 69, 57, 76 })
        {
            auto p = make();
            plain (*p);
            set (*p, "mode", 1.0f); set (*p, "mood", 0.2f);
            auto [l, r] = run (*p, 3.0, { on (0.0, note), off (2.0, note) });
            const float want = 440.0f * std::exp2 ((note - 69) / 12.0f);
            const float got = pitchAt (l, 1.2, 0.1);
            check (std::abs (got / want - 1.0f) < 0.03f, "SING holds note " + String (note) + " in tune (" + String (got, 1) + " Hz, want " + String (want, 1) + ")");
            check (rms (l, 1.5, 1.95) > 0.02f, "SING: still singing while the key is held");
            check (rms (l, 2.9, 3.0) < 1.0e-4f, "SING: stops after the key is let go");
        }
    }

    {   // SIZE: a kitten's mouth is brighter than a lion's
        float bright[2] {};
        for (int k = 0; k < 2; ++k)
        {
            auto p = make();
            plain (*p);
            set (*p, "mode", 1.0f); set (*p, "size", k == 0 ? 0.0f : 1.0f);
            auto [l, r] = run (*p, 1.2, { on (0.0, 64) });
            bright[k] = brightness (l, 0.6, 1.1);
        }
        check (bright[0] > bright[1] * 1.25f, "SIZE: a kitten sounds smaller than a lion (" + String (bright[0], 0) + " vs " + String (bright[1], 0) + " Hz)");
    }

    {   // the mouth opens: "m" at the start is darker than "a" in the middle
        auto p = make();
        plain (*p);
        set (*p, "mode", 1.0f); set (*p, "mood", 0.5f);
        auto [l, r] = run (*p, 1.5, { on (0.0, 64) });
        const float start = brightness (l, 0.005, 0.03), mid = brightness (l, 0.8, 1.2);
        check (mid > start * 1.15f, "the mouth opens: 'm' then 'a' (" + String (start, 0) + " -> " + String (mid, 0) + " Hz)");
    }

    {   // HISS: a hard hit hisses (noise), a soft one meows; HISS 0 never hisses
        auto p = make();
        plain (*p);
        set (*p, "hiss", 1.0f);
        auto [l1, r1] = run (*p, 0.6, { on (0.0, 64, 1.0f) });
        const float hard = crossings (l1, 0.05, 0.4);
        check (hard > 0.15f, "HISS: a hard hit hisses (" + String (hard, 3) + " crossings / sample)");
        check (p->engine.meows.load() == 1, "a hiss counts too");
        auto [l2, r2] = run (*p, 3.0, { on (0.0, 64, 0.3f) });
        const float soft = crossings (l2, 0.05, 0.4);
        check (soft < hard * 0.6f, "HISS: a soft note meows (" + String (soft, 3) + ")");
        set (*p, "hiss", 0.0f);
        auto [l3, r3] = run (*p, 0.6, { on (0.0, 64, 1.0f) });
        check (crossings (l3, 0.05, 0.4) < hard * 0.6f, "HISS 0: never hisses");
    }

    {   // PURR: pulses at ~20-30 Hz while held, stops after
        auto p = make();
        plain (*p);
        set (*p, "mode", 3.0f); set (*p, "size", 0.45f);
        auto [l, r] = run (*p, 4.0, { on (0.0, 60), off (3.0, 60) });
        // the envelope's period
        Sig env;
        float e = 0.0f;
        for (float x : l) { e += (std::abs (x) - e) * 0.01f; env.push_back (e); }
        const float rate = pitchAt (env, 0.5, 0.5);   // autocorrelation of the envelope
        Sig dummy;
        float hz = 0.0f;
        {
            // the envelope is slow, so look for its period with a longer lag range
            const int a = (int) (0.3 * sr), n = (int) (1.0 * sr);
            double best = -1.0; int bestLag = 0;
            for (int lag = (int) (sr / 45.0); lag <= (int) (sr / 12.0); ++lag)
            {
                double x = 0.0, m = 0.0;
                for (int i = 0; i < n; i += 4) m += env[(size_t) (a + i)];
                m /= (n / 4);
                for (int i = 0; i < n; i += 4) x += (env[(size_t) (a + i)] - m) * (env[(size_t) (a + i + lag)] - m);
                if (x > best) { best = x; bestLag = lag; }
            }
            hz = bestLag > 0 ? (float) (sr / bestLag) : 0.0f;
        }
        ignoreUnused (rate, dummy);
        check (hz > 17.0f && hz < 32.0f, "PURR: purrs at " + String (hz, 1) + " Hz (a real purr is ~20-30)");
        check (rms (l, 0.5, 2.9) > 0.005f, "PURR: you hear it");
        check (rms (l, 3.7, 4.0) < 1.0e-4f, "PURR: stops after the key is let go");
        check (p->engine.meows.load() == 0, "PURR mode: no meows");
    }

    {   // the mod wheel purrs in MEOW mode too
        auto p = make();
        plain (*p);
        auto [l, r] = run (*p, 4.0, { { 0.0, MidiMessage::controllerEvent (1, 1, 127) }, on (0.0, 60) });
        check (rms (l, 3.0, 3.9) > 0.003f, "the mod wheel purrs (after the meow, while held)");
    }

    {   // CHATTER: "ek-ek-ek" in 1/16 notes with the song
        auto p = make();
        plain (*p);
        set (*p, "mode", 2.0f);
        Head h; h.playing = true; h.bpm = 120.0;
        auto [l, r] = run (*p, 1.2, { on (0.0, 72), off (1.0, 72) }, &h);
        int onsets = 0; bool loud = false;
        float e = 0.0f;
        for (size_t i = 0; i < (size_t) (1.1 * sr); ++i)
        {
            e += (std::abs (l[i]) - e) * 0.02f;
            if (! loud && e > 0.02f) { loud = true; ++onsets; }
            if (loud && e < 0.005f) loud = false;
        }
        check (onsets >= 7 && onsets <= 9, "CHATTER: 8 'ek's in a second at 120 BPM (" + String (onsets) + ")");
        check (rms (l, 1.15, 1.2) < 1.0e-4f, "CHATTER: stops when the key is let go");
    }

    {   // the choir is wide; one plain cat is in the middle
        auto p = make();
        plain (*p);
        auto [l1, r1] = run (*p, 1.0, { on (0.0, 67) });
        double d1 = 0.0, s1 = 0.0; for (size_t i = 0; i < l1.size(); ++i) { d1 += std::abs (l1[i] - r1[i]); s1 += std::abs (l1[i]); }
        check (d1 / (s1 + 1.0e-9) < 1.0e-4, "one cat (CHAOS 0): in the middle (" + String (d1 / s1, 7) + ")");
        set (*p, "choir", 6.0f);
        auto [l2, r2] = run (*p, 1.5, { on (0.0, 67) });
        double d2 = 0.0, s2 = 0.0; for (size_t i = 0; i < l2.size(); ++i) { d2 += std::abs (l2[i] - r2[i]); s2 += std::abs (l2[i]) + std::abs (r2[i]); }
        check (d2 / (s2 + 1.0e-9) > 0.1, "CHOIR 6: the cats sit left and right (" + String (d2 / s2, 2) + ")");
        check (p->engine.activeCats() == 0, "all six cats finish");
    }

    {   // pitch bend, octave, sustain, all notes off
        auto p = make();
        plain (*p);
        set (*p, "mode", 1.0f); set (*p, "mood", 0.2f);
        auto [l, r] = run (*p, 1.5, { { 0.0, MidiMessage::pitchWheel (1, 16383) }, on (0.0, 69) });
        const float bent = pitchAt (l, 1.0, 0.1);
        check (std::abs (bent / 493.88f - 1.0f) < 0.03f, "pitch bend up = 2 semitones (" + String (bent, 1) + " Hz)");
        run (*p, 0.1, { { 0.0, MidiMessage::pitchWheel (1, 8192) }, { 0.0, MidiMessage::allNotesOff (1) } });
        run (*p, 2.0, {});
        set (*p, "octave", 1.0f);
        auto [l2, r2] = run (*p, 1.5, { on (0.0, 69) });
        const float up = pitchAt (l2, 1.0, 0.1);
        check (std::abs (up / 880.0f - 1.0f) < 0.03f, "OCTAVE +1 (" + String (up, 1) + " Hz)");
        auto [l3, r3] = run (*p, 1.0, { { 0.0, MidiMessage::allNotesOff (1) } });
        check (rms (l3, 0.8, 1.0) < 1.0e-4f, "all notes off: the singing stops");
        set (*p, "octave", 0.0f);
        auto [l4, r4] = run (*p, 3.0, { { 0.0, MidiMessage::controllerEvent (1, 64, 127) }, on (0.0, 69), off (0.5, 69) });
        check (rms (l4, 2.5, 2.9) > 0.02f, "the sustain pedal keeps it singing");
        auto [l5, r5] = run (*p, 1.5, { { 0.0, MidiMessage::controllerEvent (1, 64, 0) } });
        check (rms (l5, 1.3, 1.5) < 1.0e-4f, "pedal up: it stops");
    }

    {   // 12 notes on 10 voices with 6 cats each: safe, bounded, fast enough
        auto p = make();
        set (*p, "choir", 6.0f); set (*p, "mode", 1.0f); set (*p, "chaos", 1.0f); set (*p, "alley", 1.0f); set (*p, "purr", 1.0f);
        std::vector<Ev> evs;
        for (int k = 0; k < 12; ++k) evs.push_back (on (0.01 * k, 48 + k * 3, 0.5f + 0.04f * k));
        const auto t0 = Time::getMillisecondCounterHiRes();
        auto [l, r] = run (*p, 4.0, evs);
        const double ms = Time::getMillisecondCounterHiRes() - t0;
        check (finite (l) && finite (r) && peak (l) <= 1.0f && peak (r) <= 1.0f, "12 notes x 6 cats + purr + alley: clean and under 0 dB (peak " + String (peak (l), 2) + ")");
        check (ms < 4000.0 * 0.5, "fast enough: 4 s of 60 cats in " + String (ms, 0) + " ms");
        check (p->engine.activeNotes() <= meow::maxNotes, "never more than 10 notes");
    }

    {   // every preset, a chord with a hard note, is clean and safe
        auto p = make();
        bool ok = true; String bad;
        for (int i = 0; i < p->getNumPrograms(); ++i)
        {
            p->setCurrentProgram (i);
            auto [l, r] = run (*p, 1.6, { on (0.0, 60), on (0.0, 64, 1.0f), on (0.0, 67), off (1.0, 60), off (1.0, 64), off (1.0, 67) });
            if (! finite (l) || ! finite (r) || peak (l) > 1.0f || rms (l, 0.0, 1.0) < 0.002f) { ok = false; bad << p->getProgramName (i) << " "; }
            run (*p, 0.1, { { 0.0, MidiMessage::allNotesOff (1) } });
            run (*p, 4.0, {});
        }
        check (ok, "all " + String (p->getNumPrograms()) + " presets sound and are safe " + bad);
    }

    {   // the state comes back
        auto a = make();
        a->setCurrentProgram (7);
        set (*a, "output", -3.0f);
        MemoryBlock m;
        a->getStateInformation (m);
        auto b = make();
        b->setStateInformation (m.getData(), (int) m.getSize());
        bool same = true;
        for (auto* prm : a->getParameters())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
                if (std::abs (rp->getValue() - b->apvts.getParameter (rp->getParameterID())->getValue()) > 1.0e-5f) same = false;
        check (same && b->getCurrentProgram() == 7, "the state comes back (preset " + b->getProgramName (b->getCurrentProgram()) + ")");
    }

    for (double rate : { 44100.0, 96000.0 })
    {
        auto p = make (rate);
        plain (*p);
        set (*p, "mode", 1.0f); set (*p, "mood", 0.2f);
        auto [l, r] = run (*p, 1.5, { on (0.0, 69) });
        const float got = pitchAt (l, 1.0, 0.1);
        check (std::abs (got / 440.0f - 1.0f) < 0.03f, "at " + String (rate / 1000.0, 1) + " kHz still in tune (" + String (got, 1) + " Hz)");
        sr = 48000.0;
    }

    if (argc > 2 && String (argv[1]) == "--render")
    {
        const File dir { String (argv[2]) };
        dir.createDirectory();
        auto melody = [] (double step, double hold)
        {
            std::vector<Ev> e;
            const int tune[] = { 67, 64, 67, 72, 71, 69, 67, 64, 62, 64, 67, 60 };
            for (int i = 0; i < 12; ++i) { e.push_back (on (i * step, tune[i], 0.55f + 0.04f * (i % 3))); e.push_back (off (i * step + hold, tune[i])); }
            return e;
        };
        auto chords = []
        {
            std::vector<Ev> e;
            const int ch[4][3] = { { 60, 64, 67 }, { 57, 60, 64 }, { 53, 57, 60 }, { 55, 59, 62 } };
            for (int c = 0; c < 4; ++c) for (int k = 0; k < 3; ++k) { e.push_back (on (c * 1.6, ch[c][k], 0.6f)); e.push_back (off (c * 1.6 + 1.45, ch[c][k])); }
            return e;
        };
        struct Demo { const char* file; int preset; bool chordDemo; };
        for (auto d : { Demo { "1-house-cat-melody.wav", 0, false }, Demo { "2-cat-choir.wav", 3, true }, Demo { "3-opera-cat.wav", 4, false },
                        Demo { "4-alley-fight.wav", 7, false }, Demo { "5-lion-king.wav", 9, false }, Demo { "6-purring.wav", 12, true },
                        Demo { "7-birds-at-the-window.wav", 14, false }, Demo { "8-kitten-choir.wav", 5, true } })
        {
            auto p = make();
            p->setCurrentProgram (d.preset);
            Head h; h.playing = true;
            auto [l, r] = run (*p, d.chordDemo ? 8.0 : 7.5, d.chordDemo ? chords() : melody (0.55, 0.45), &h);
            writeWav (dir.getChildFile (d.file), l, r);
        }
        std::printf ("demo wavs in %s\n", dir.getFullPathName().toRawUTF8());
    }

    std::printf (failures == 0 ? "ALL MEOW CHECKS PASSED\n" : "%d MEOW CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}

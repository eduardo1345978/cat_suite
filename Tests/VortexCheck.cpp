// Vortex's check: the spiral really goes up (and down) in PITCH and FILTER, at the speed it says, locks to the
// song position with SYNC, GEN spins on its own, FEEDBACK stays bounded and dies away, no clicks, auto gain keeps
// the level, MIX 0 is the dry sound, every preset is safe, the state comes back, a sample-rate change is fine.
#include "PluginProcessor.h"
#include "SafetyLimiter.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <complex>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

constexpr double sr = 48000.0;
constexpr int block = 512;

struct Head : AudioPlayHead
{
    double ppq = 0.0; bool playing = false;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (120.0); p.setIsPlaying (playing); p.setPpqPosition (ppq);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

static std::unique_ptr<VortexProcessor> make (double rate = sr)
{
    auto p = std::make_unique<VortexProcessor>();
    p->setPlayConfigDetails (2, 2, rate, block);
    p->prepareToPlay (rate, block);
    return p;
}

static void set (VortexProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

using Sig = std::vector<float>;

static std::pair<Sig, Sig> run (VortexProcessor& p, const Sig& l, const Sig& r, Head* head = nullptr)
{
    Sig ol, orr;
    AudioBuffer<float> buf (2, block);
    MidiBuffer midi;
    if (head != nullptr) p.setPlayHead (head);
    for (size_t pos = 0; pos < l.size(); pos += block)
    {
        const int n = (int) jmin ((size_t) block, l.size() - pos);
        buf.setSize (2, n, false, false, true);
        buf.copyFrom (0, 0, l.data() + pos, n);
        buf.copyFrom (1, 0, r.data() + pos, n);
        p.processBlock (buf, midi);
        if (head != nullptr && head->playing) head->ppq += n / sr * 2.0;   // 120 bpm
        for (int i = 0; i < n; ++i) { ol.push_back (buf.getSample (0, i)); orr.push_back (buf.getSample (1, i)); }
    }
    p.setPlayHead (nullptr);
    return { ol, orr };
}

static Sig sine (double hz, double seconds, float amp)
{
    Sig x ((size_t) (seconds * sr));
    for (size_t i = 0; i < x.size(); ++i) x[i] = amp * (float) std::sin (MathConstants<double>::twoPi * hz * (double) i / sr);
    return x;
}

static Sig noise (double seconds, float amp, int seed = 1)
{
    Random r (seed);
    Sig x ((size_t) (seconds * sr));
    for (auto& v : x) v = amp * (r.nextFloat() * 2.0f - 1.0f);
    return x;
}

static double rms (const Sig& x, size_t from, size_t to)
{
    double s = 0.0;
    for (size_t i = from; i < to; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) jmax ((size_t) 1, to - from));
}

static float peak (const Sig& x, size_t from = 0)
{
    float m = 0.0f;
    for (size_t i = from; i < x.size(); ++i) m = jmax (m, std::abs (x[i]));
    return m;
}

static bool finite (const Sig& x) { for (auto v : x) if (! std::isfinite (v)) return false; return true; }

/** Where the sound sits inside the octave (0..1 above ref, the same for every octave): a power-weighted circular mean
    over a fine log-frequency scan of one window. */
static double chroma (const Sig& x, size_t start, double ref, double loHz = 40.0, double hiHz = 8000.0)
{
    constexpr int len = 4096;
    std::vector<float> win (len);
    for (int i = 0; i < len; ++i) win[(size_t) i] = x[start + (size_t) i] * (float) (0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * i / (len - 1)));
    std::complex<double> acc { 0.0, 0.0 };
    for (double o = std::log2 (loHz); o < std::log2 (hiHz); o += 1.0 / 240.0)
    {
        const double hz = std::exp2 (o), w = MathConstants<double>::twoPi * hz / sr;
        double re = 0.0, im = 0.0;
        for (int i = 0; i < len; ++i) { re += win[(size_t) i] * std::cos (w * i); im -= win[(size_t) i] * std::sin (w * i); }
        const double pw = re * re + im * im;
        const double c = std::log2 (hz / ref);
        acc += pw * std::polar (1.0, MathConstants<double>::twoPi * c);
    }
    double a = std::arg (acc) / MathConstants<double>::twoPi;
    return a < 0.0 ? a + 1.0 : a;
}

static double circDiff (double a, double b) { double d = a - b; d -= std::round (d); return d; }

/** VortexCheck --render <folder>: a few presets on a soft chord, as .wav files to listen to. */
static int render (const File& dir)
{
    dir.createDirectory();
    Sig in ((size_t) (24.0 * sr), 0.0f);
    for (double hz : { 130.81, 155.56, 196.0, 233.08 })          // C minor 7, soft saws with a slow swell
    {
        double ph = 0.0;
        for (size_t i = 0; i < in.size(); ++i)
        {
            ph += hz / sr; ph -= std::floor (ph);
            const float env = (float) jmin (1.0, (double) i / (0.5 * sr));
            in[i] += env * 0.06f * (float) (2.0 * ph - 1.0);
        }
    }
    float lp = 0.0f;
    for (auto& v : in) { lp += 0.12f * (v - lp); v = lp; }
    const Sig silence (in.size(), 0.0f);
    for (auto [index, file] : { std::pair { 0, "1 Endless Rise" }, { 1, "2 Endless Fall" }, { 6, "3 Barber Pole" }, { 10, "4 Cascade" },
                                { 12, "5 Spiral Choir" }, { 21, "6 Shepard Tone (no input)" }, { 18, "7 Black Hole" } })
    {
        auto p = make();
        p->setCurrentProgram (index);
        auto [l, r] = run (*p, index == 21 ? silence : in, index == 21 ? silence : in);
        AudioBuffer<float> out (2, (int) l.size());
        out.copyFrom (0, 0, l.data(), (int) l.size());
        out.copyFrom (1, 0, r.data(), (int) r.size());
        auto f = dir.getChildFile (String (file) + ".wav");
        f.deleteFile();
        WavAudioFormat wav;
        if (auto w = std::unique_ptr<AudioFormatWriter> (wav.createWriterFor (new FileOutputStream (f), sr, 2, 16, {}, 0)))
            w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
    }
    return 0;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juce;
    if (argc > 2 && String (argv[1]) == "--render") return render (File (argv[2]));

    // ---- PITCH rises at the speed it says ----
    for (float spinV : { 0.6f, -0.6f })
    {
        auto p = make();
        set (*p, "focus", 1.0f); set (*p, "spin", spinV); set (*p, "width", 0.0f);
        const auto in = sine (220.0, 3.0, 0.3f);
        auto [l, r] = run (*p, in, in);
        const double rate = vortex::spinRate (spinV);
        double worst = 0.0;
        for (double t : { 1.0, 1.5, 2.0 })
        {
            const double c1 = chroma (l, (size_t) (t * sr), 220.0), c2 = chroma (l, (size_t) ((t + 0.4) * sr), 220.0);
            worst = jmax (worst, std::abs (circDiff (c2 - c1, rate * 0.4)));
        }
        check (worst < 0.03, String ("PITCH ") + (spinV > 0 ? "rises" : "falls") + " " + String (std::abs (rate) * 0.4, 3)
                             + " octaves in 0.4 s (worst error " + String (worst, 3) + ")");
    }

    // ---- FILTER sweeps up through noise ----
    for (float spinV : { 0.7f, -0.7f })
    {
        auto p = make();
        set (*p, "mode", 1); set (*p, "focus", 1.0f); set (*p, "spin", spinV); set (*p, "width", 0.0f);
        const auto in = noise (3.0, 0.3f);
        auto [l, r] = run (*p, in, in);
        const double rate = vortex::spinRate (spinV);
        double sum = 0.0;                              // noise: average the steps of 8 windows
        double prev = chroma (l, (size_t) (1.0 * sr), 20.0, 60.0, 8000.0);
        for (int k = 1; k <= 8; ++k)
        {
            const double c = chroma (l, (size_t) ((1.0 + 0.2 * k) * sr), 20.0, 60.0, 8000.0);
            sum += circDiff (c, prev);
            prev = c;
        }
        const double worst = std::abs (sum / 8.0 / 0.2 * 0.3 - rate * 0.3);
        check (worst < 0.03, String ("FILTER sweeps ") + (spinV > 0 ? "up " : "down ") + String (std::abs (rate) * 0.3, 3)
                             + " octaves in 0.3 s (worst error " + String (worst, 3) + ")");
    }

    // ---- SYNC: one octave every bar, locked to the song position ----
    {
        auto p = make();
        set (*p, "sync", 1); set (*p, "spin", 0.5f);
        Head head; head.ppq = 13.0; head.playing = true;
        const auto in = noise (2.0, 0.1f);
        run (*p, in, in, &head);
        const double want = head.ppq / 4.0;
        const double got = p->engine.getPosition();
        check (std::abs (circDiff (got, want)) < 0.01, "SYNC 1 bar: the spiral sits where the song is (" + String (circDiff (got, want), 4) + " octaves off)");
        set (*p, "spin", -0.5f);
        run (*p, in, in, &head);
        check (std::abs (circDiff (p->engine.getPosition(), -head.ppq / 4.0)) < 0.01, "SYNC with SPIN to the left falls, still locked");
    }

    // ---- GEN: an endless tone of its own ----
    {
        auto p = make();
        set (*p, "gen", 0.8f); set (*p, "spin", 0.6f);
        const Sig silence ((size_t) (3.0 * sr), 0.0f);
        auto [l, r] = run (*p, silence, silence);
        const double rate = vortex::spinRate (0.6f);
        const double c1 = chroma (l, (size_t) (1.5 * sr), vortex::genLowHz), c2 = chroma (l, (size_t) (1.9 * sr), vortex::genLowHz);
        check (rms (l, (size_t) sr, l.size()) > 0.02 && std::abs (circDiff (c2 - c1, rate * 0.4)) < 0.03,
               "GEN plays a Shepard tone with nothing coming in, and it rises (rms " + String (rms (l, (size_t) sr, l.size()), 3) + ")");
    }

    // ---- silence stays silent ----
    {
        auto p = make();
        const Sig silence ((size_t) (2.0 * sr), 0.0f);
        auto [l, r] = run (*p, silence, silence);
        check (peak (l) < 1.0e-6f && peak (r) < 1.0e-6f, "silence in, silence out");
    }

    // ---- no clicks while the copies come and go ----
    for (int m : { 0, 2 })
    {
        auto p = make();
        set (*p, "mode", m); set (*p, "spin", 0.8f); set (*p, "focus", 0.3f);
        const auto in = sine (220.0, 4.0, 0.3f);
        auto [l, r] = run (*p, in, in);
        float jump = 0.0f;
        for (size_t i = (size_t) (0.5 * sr); i + 1 < l.size(); ++i) jump = jmax (jump, std::abs (l[i + 1] - l[i]), std::abs (r[i + 1] - r[i]));
        check (jump < 0.12f, String (m == 0 ? "PITCH" : "BOTH") + ": no clicks over 4 octaves of spinning (largest step " + String (jump, 3) + ")");
    }

    // ---- auto gain ----
    for (int m : { 0, 1, 2 })
    {
        auto p = make();
        set (*p, "mode", m);
        const auto in = noise (4.0, 0.2f, 3);
        auto [l, r] = run (*p, in, in);
        const double db = Decibels::gainToDecibels (rms (l, (size_t) (2 * sr), l.size()) / rms (in, (size_t) (2 * sr), in.size()));
        check (std::abs (db) < 4.0, String (vortex::modeNames()[m]) + ": as loud as what came in (" + String (db, 1) + " dB)");
    }

    // ---- MIX 0 = the dry sound ----
    {
        auto p = make();
        set (*p, "mix", 0.0f);
        const auto in = sine (330.0, 1.0, 0.3f);
        auto [l, r] = run (*p, in, in);
        Sig ref = in, refR = in;                       // the dry sound through the same safety limiter
        cat::SafetyLimiter lim;
        lim.prepare (sr);
        lim.process (ref.data(), refR.data(), (int) ref.size());
        float d = 0.0f;
        for (size_t i = (size_t) (0.2 * sr); i < l.size(); ++i) d = jmax (d, std::abs (l[i] - ref[i]));
        check (d < 1.0e-4f, "MIX 0 is the dry sound (" + String (d, 6) + ")");
    }

    // ---- FEEDBACK all the way up: bounded, and it dies away ----
    {
        auto p = make();
        set (*p, "feedback", 0.95f); set (*p, "mode", 2); set (*p, "focus", 0.2f);
        Sig in = noise (2.0, 0.6f, 5);
        in.resize ((size_t) (16.0 * sr), 0.0f);
        auto [l, r] = run (*p, in, in);
        const double tail = rms (l, (size_t) (15.0 * sr), l.size());
        check (finite (l) && peak (l) <= 1.0f, "FEEDBACK 95%: finite and never over 0 dB (peak " + String (peak (l), 2) + ")");
        check (tail < 1.0e-3, "FEEDBACK 95%: the cascade dies away after the sound stops (" + String (Decibels::gainToDecibels (tail), 0) + " dB)");
    }

    // ---- every preset ----
    {
        int bad = 0, quiet = 0;
        String names;
        for (int i = 0; i < (int) VortexProcessor::presets().size(); ++i)
        {
            auto p = make();
            p->setCurrentProgram (i);
            Sig in = noise (3.0, 0.15f, 10 + i);
            const auto s = sine (165.0, 3.0, 0.2f);
            for (size_t k = 0; k < in.size(); ++k) in[k] += s[k];
            Head head; head.playing = true;
            auto [l, r] = run (*p, in, in, &head);
            if (! finite (l) || ! finite (r) || peak (l) > 1.0f || peak (r) > 1.0f) { ++bad; names << VortexProcessor::presets()[(size_t) i].name << " "; }
            if (rms (l, (size_t) sr, l.size()) < 0.02) { ++quiet; names << "(quiet) " << VortexProcessor::presets()[(size_t) i].name << " "; }
        }
        check (bad == 0 && quiet == 0, String ((int) VortexProcessor::presets().size()) + " presets: all finite, under 0 dB and audible " + names);
    }

    // ---- the state comes back ----
    {
        auto a = make();
        a->setCurrentProgram (7);
        set (*a, "tone", -0.5f);
        MemoryBlock m;
        a->getStateInformation (m);
        auto b = make();
        b->setStateInformation (m.getData(), (int) m.getSize());
        bool same = true;
        for (auto* id : { "spin", "mode", "focus", "feedback", "width", "tone", "gen", "mix", "sync" })
            same &= std::abs (a->apvts.getRawParameterValue (id)->load() - b->apvts.getRawParameterValue (id)->load()) < 1.0e-5f;
        check (same && b->getCurrentProgram() == 7, "the state (and the preset) comes back");
    }

    // ---- sample-rate changes ----
    {
        auto p = make (44100.0);
        set (*p, "mode", 2); set (*p, "feedback", 0.5f);
        const auto in = noise (1.0, 0.3f);
        auto a = run (*p, in, in);
        p->prepareToPlay (96000.0, block);
        auto b = run (*p, in, in);
        p->prepareToPlay (22050.0, 128);
        auto c = run (*p, in, in);
        check (finite (a.first) && finite (b.first) && finite (c.first) && peak (c.first) <= 1.0f, "44.1 -> 96 -> 22 kHz: fine");
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL VORTEX CHECKS PASSED" : (String (failures) + " FAILED").toRawUTF8());
    return failures == 0 ? 0 : 1;
}

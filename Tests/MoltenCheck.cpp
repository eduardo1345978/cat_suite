// Molten's check: HEAT doesn't change the loudness (auto gain), it does add harmonics, the latency is what it says,
// the drips fall (every echo lower than the last), the bass stays mono, the presets are safe, the state comes back.
//   MoltenCheck --lava <folder>   also draws the lava simulation (frames as PNG) to look at.
#include "PluginProcessor.h"
#include "LavaSim.h"
#include <cstdio>

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
    Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm (120.0); p.setIsPlaying (true); return p; }
};

static std::unique_ptr<MoltenProcessor> make()
{
    auto p = std::make_unique<MoltenProcessor>();
    p->setPlayConfigDetails (2, 2, sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

static void set (MoltenProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

/** Runs stereo input through it, returns the output. */
static std::pair<std::vector<float>, std::vector<float>> run (MoltenProcessor& p, const std::vector<float>& l, const std::vector<float>& r)
{
    std::vector<float> ol, orr;
    AudioBuffer<float> buf (2, block);
    MidiBuffer midi;
    for (size_t pos = 0; pos < l.size(); pos += block)
    {
        const int n = (int) jmin ((size_t) block, l.size() - pos);
        buf.setSize (2, n, false, false, true);
        buf.copyFrom (0, 0, l.data() + pos, n);
        buf.copyFrom (1, 0, r.data() + pos, n);
        p.processBlock (buf, midi);
        for (int i = 0; i < n; ++i) { ol.push_back (buf.getSample (0, i)); orr.push_back (buf.getSample (1, i)); }
    }
    return { ol, orr };
}

static double rms (const std::vector<float>& x, size_t from, size_t to)
{
    double s = 0.0;
    for (size_t i = from; i < to; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) jmax ((size_t) 1, to - from));
}

static std::vector<float> musical (double seconds, float level)
{
    // a bass (55 Hz) + a saw-ish chord tone, like a busy synth track
    std::vector<float> x ((size_t) (seconds * sr));
    double p1 = 0, p2 = 0, p3 = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        p1 += 55.0 / sr; p2 += 220.0 / sr; p3 += 277.2 / sr;
        const double saw = 2.0 * (p2 - std::floor (p2)) - 1.0, saw2 = 2.0 * (p3 - std::floor (p3)) - 1.0;
        x[i] = level * (float) (0.6 * std::sin (MathConstants<double>::twoPi * p1) + 0.25 * saw + 0.2 * saw2);
    }
    return x;
}

static double pitchHz (const std::vector<float>& x, size_t from, size_t to)
{
    double best = 0; int lag0 = 0;
    for (int lag = 40; lag < 400; ++lag)
    {
        double s = 0, e = 0;
        for (size_t i = from; i + (size_t) lag < to; ++i) { s += x[i] * x[i + (size_t) lag]; e += x[i] * x[i]; }
        const double c = s / (e + 1e-12);
        if (c > best) { best = c; lag0 = lag; }
    }
    if (lag0 == 0) return 0;
    // the shortest lag that's nearly as good (a period 3 times as long fits too, but that's not the note)
    for (int lag = 41; lag < lag0; ++lag)
    {
        double s = 0, e = 0, sp = 0, ep = 0, sn = 0, en = 0;
        for (size_t i = from; i + (size_t) lag + 1 < to; ++i)
        {
            s += x[i] * x[i + (size_t) lag]; e += x[i] * x[i];
            sp += x[i] * x[i + (size_t) lag - 1]; sn += x[i] * x[i + (size_t) lag + 1];
        }
        juce::ignoreUnused (ep, en);
        if (s / (e + 1e-12) > 0.75 * best && s >= sp && s >= sn) { lag0 = lag; break; }
    }
    auto corr = [&] (int lag) { double s = 0, e = 0; for (size_t i = from; i + (size_t) lag < to; ++i) { s += x[i] * x[i + (size_t) lag]; e += x[i] * x[i]; } return s / (e + 1e-12); };
    const double a = corr (lag0 - 1), b = corr (lag0), c = corr (lag0 + 1);
    const double sh = 0.5 * (a - c) / (a - 2 * b + c);
    return sr / (lag0 + (std::isfinite (sh) ? sh : 0.0));
}

/** The strongest frequency between lo and hi (a plain DFT scan, Hann windowed) - for the drips, which are single notes. */
static double peakHz (const std::vector<float>& x, size_t from, size_t to, double lo, double hi)
{
    double best = 0, bestF = 0;
    const size_t n = to - from;
    for (double f = lo; f <= hi; f += 0.5)
    {
        double re = 0, im = 0;
        for (size_t i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * (double) i / (double) n);
            const double ph = MathConstants<double>::twoPi * f * (double) i / sr;
            re += w * x[from + i] * std::cos (ph); im += w * x[from + i] * std::sin (ph);
        }
        const double m = re * re + im * im;
        if (m > best) { best = m; bestF = f; }
    }
    return bestF;
}

static void lavaPictures (const File& dir)
{
    dir.createDirectory();
    for (float heat : { 0.15f, 0.55f, 1.0f })
    {
        LavaSim sim;
        sim.setSize (320, 220);
        Random rng (5);
        int drips = 0;
        Image sheet (Image::RGB, 640 * 3, 440, true);
        Graphics g (sheet);
        int shot = 0;
        for (int f = 0; f < 240; ++f)
        {
            LavaSim::Input in;
            in.heat = heat; in.drip = 0.6f; in.wobble = 0.3f;
            in.level = (f % 30) < 4 ? 0.8f : 0.2f;
            if (f % 45 == 20) ++drips;
            in.dripCount = drips;
            sim.step (in);
            if (f == 90 || f == 160 || f == 239)
            {
                sim.render();
                const Rectangle<float> area ((float) shot * 640.0f, 0.0f, 640.0f, 440.0f);
                ColourGradient bg (Colour (0xff2a0d06), area.getCentreX(), 176.0f, Colour (0xff070303), area.getCentreX() + 380.0f, 176.0f, true);
                g.setGradientFill (bg);
                g.fillRect (area);
                g.setImageResamplingQuality (Graphics::highResamplingQuality);
                g.setOpacity (0.85f);
                g.drawImage (sim.glow(), area.expanded (38.0f, 26.0f));
                g.setOpacity (1.0f);
                g.drawImage (sim.image(), area);
                ++shot;
            }
        }
        PNGImageFormat png;
        FileOutputStream os (dir.getChildFile ("lava-heat-" + String (roundToInt (heat * 100)) + ".png"));
        os.setPosition (0); os.truncate();
        png.writeImageToStream (sheet, os);
    }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    if (argc > 2 && String (argv[1]) == "--lava") { lavaPictures (File (argv[2])); return 0; }

    // HEAT: as loud as it came in, at every setting
    {
        const auto in = musical (5.0, 0.25f);
        // the reference: the input through the same safety stage every Cat ends with (its sub-sonic filter takes a
        // little off a 55 Hz bass), so this measures Molten itself
        std::vector<float> ref (in);
        {
            std::vector<float> refR (in);
            cat::SafetyLimiter lim;
            lim.prepare (sr);
            lim.process (ref.data(), refR.data(), (int) ref.size());
        }
        const double inR = rms (ref, (size_t) (3.0 * sr), ref.size());
        String levels;
        bool ok = true;
        double thd0 = 0, thd1 = 0;
        for (float heat : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            auto p = make();
            set (*p, "heat", heat); set (*p, "drip", 0.0f); set (*p, "wobble", 0.0f); set (*p, "body", 0.0f);
            const auto [l, r] = run (*p, in, in);
            const double db = 20.0 * std::log10 (rms (l, (size_t) (3.0 * sr), l.size()) / inR);
            levels << String (db, 1) << " ";
            ok &= std::abs (db) < 1.5;
        }
        // HEAT adds harmonics: a pure 220 Hz sine goes in, how much comes out that isn't 220 Hz?
        auto newHarmonics = [] (float heat)
        {
            auto p = make();
            set (*p, "heat", heat); set (*p, "drip", 0.0f); set (*p, "wobble", 0.0f); set (*p, "body", 0.0f);
            std::vector<float> x ((size_t) (3.0 * sr));
            for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * (float) std::sin (MathConstants<double>::twoPi * 220.0 * (double) i / sr);
            const auto [l, r] = run (*p, x, x);
            // least-squares fit of a 220 Hz sine + cosine over the last second; what's left over is new
            double ss = 0, sc = 0, cc = 0, ys = 0, yc = 0, yy = 0;
            const size_t a = (size_t) (2.0 * sr);
            for (size_t i = a; i < l.size(); ++i)
            {
                const double ph = MathConstants<double>::twoPi * 220.0 * (double) i / sr, sn = std::sin (ph), cs = std::cos (ph);
                ss += sn * sn; sc += sn * cs; cc += cs * cs; ys += l[i] * sn; yc += l[i] * cs; yy += (double) l[i] * l[i];
            }
            const double det = ss * cc - sc * sc;
            const double A = (ys * cc - yc * sc) / det, B = (yc * ss - ys * sc) / det;
            const double fit = A * ys + B * yc;
            return (yy - fit) / (yy + 1e-12);
        };
        thd0 = newHarmonics (0.0f); thd1 = newHarmonics (1.0f);
        check (ok, "auto gain: every HEAT is as loud as the input (dB: " + levels.trim() + ")");
        check (thd0 < 0.001 && thd1 > 0.02, "HEAT adds dirt (" + String (thd0, 5) + " clean, " + String (thd1, 3) + " hot)");
    }

    // the latency is real: at MIX 0 the input comes out exactly that late
    {
        auto p = make();
        set (*p, "mix", 0.0f);
        std::vector<float> x ((size_t) sr, 0.0f);
        x[1000] = 0.5f;
        const auto [l, r] = run (*p, x, x);
        int at = -1;
        for (size_t i = 0; i < l.size(); ++i) if (std::abs (l[i]) > 0.25f) { at = (int) i; break; }
        check (at - 1000 == p->getLatencySamples(), "the dry sound comes out exactly the reported latency late (" + String (p->getLatencySamples()) + " samples)");
        // and the wet one lines up with it (no comb filter at MIX 50%)
        auto q = make();
        set (*q, "heat", 0.0f); set (*q, "drip", 0.0f); set (*q, "wobble", 0.0f); set (*q, "body", 0.0f); set (*q, "mix", 1.0f);
        const auto [wl, wr] = run (*q, x, x);
        int peakAt = 0; float pk = 0.0f;
        for (size_t i = 0; i < wl.size(); ++i) if (std::abs (wl[i]) > pk) { pk = std::abs (wl[i]); peakAt = (int) i; }
        check (std::abs (peakAt - 1000 - q->getLatencySamples()) <= 3, "the melted sound lines up with the dry one (" + String (peakAt - 1000) + " vs " + String (q->getLatencySamples()) + ")");
    }

    // DRIP: each echo falls lower than the last
    {
        auto p = make();
        Head head;
        p->setPlayHead (&head);
        set (*p, "heat", 0.3f); set (*p, "drip", 0.8f); set (*p, "wobble", 0.0f); set (*p, "body", 0.0f); set (*p, "mix", 1.0f);
        set (*p, "dripTime", 1.0f);                       // 1/8 at 120 = 0.25 s
        std::vector<float> x ((size_t) (2.0 * sr), 0.0f);
        for (size_t i = 0; i < (size_t) (0.08 * sr); ++i)
            x[i] = 0.3f * (float) std::sin (MathConstants<double>::twoPi * 440.0 * (double) i / sr) * (float) std::sin (MathConstants<double>::pi * (double) i / (0.08 * sr));
        const auto [l, r] = run (*p, x, x);
        const int lat = p->getLatencySamples();
        auto window = [&] (double t) { const size_t a = (size_t) (t * sr) + (size_t) lat; return peakHz (l, a + 600, a + 3200, 200.0, 600.0); };
        const double f0 = window (0.0), f1 = window (0.25), f2 = window (0.5);
        check (f1 < f0 * 0.985 && f2 < f1 * 0.99 && f2 > 200.0,
               "the drips fall: " + String (f0, 0) + " Hz, then " + String (f1, 0) + " Hz, then " + String (f2, 0) + " Hz");
        check (p->engine.drips.load() >= 2, "the screen hears the drips (" + String (p->engine.drips.load()) + ")");
        p->setPlayHead (nullptr);
    }

    // WOBBLE moves the pitch; without it the pitch stands still
    {
        auto spread = [] (float wobble)
        {
            auto p = make();
            set (*p, "heat", 0.6f); set (*p, "drip", 0.0f); set (*p, "wobble", wobble); set (*p, "body", 0.0f);
            std::vector<float> x ((size_t) (4.0 * sr));
            for (size_t i = 0; i < x.size(); ++i) x[i] = 0.2f * (float) std::sin (MathConstants<double>::twoPi * 330.0 * (double) i / sr);
            const auto [l, r] = run (*p, x, x);
            double lo = 1e9, hi = 0;
            for (double t = 1.0; t < 3.8; t += 0.1)
            {
                const double f = pitchHz (l, (size_t) (t * sr), (size_t) (t * sr) + 2400);
                lo = jmin (lo, f); hi = jmax (hi, f);
            }
            return 1200.0 * std::log2 (hi / lo);
        };
        const double still = spread (0.0), moving = spread (1.0);
        check (still < 2.0 && moving > 8.0, "WOBBLE: the pitch moves " + String (moving, 1) + " cents (" + String (still, 1) + " without)");
    }

    // the bass stays in the middle
    {
        auto p = make();
        set (*p, "heat", 0.2f); set (*p, "drip", 0.0f); set (*p, "wobble", 0.0f);
        std::vector<float> l ((size_t) (2.0 * sr)), r (l.size());
        for (size_t i = 0; i < l.size(); ++i) { l[i] = 0.3f * (float) std::sin (MathConstants<double>::twoPi * 55.0 * (double) i / sr); r[i] = -l[i]; }
        const auto [ol, orr] = run (*p, l, r);
        std::vector<float> side (ol.size());
        for (size_t i = 0; i < side.size(); ++i) side[i] = 0.5f * (ol[i] - orr[i]);
        const double db = 20.0 * std::log10 (rms (side, (size_t) sr, side.size()) / rms (l, (size_t) sr, l.size()) + 1e-12);
        check (db < -12.0, "a 55 Hz bass that is all stereo comes out in the middle (side " + String (db, 1) + " dB)");
    }

    // every preset, loud: finite and under 0 dB
    {
        auto p = make();
        Random rng (2);
        std::vector<float> x ((size_t) (2.0 * sr));
        for (size_t i = 0; i < x.size(); ++i) x[i] = 0.9f * (float) std::sin (MathConstants<double>::twoPi * 45.0 * (double) i / sr) + 0.3f * (rng.nextFloat() * 2.0f - 1.0f);
        bool ok = true; float worst = 0.0f;
        for (int i = 0; i < p->getNumPrograms(); ++i)
        {
            p->setCurrentProgram (i);
            const auto [l, r] = run (*p, x, x);
            for (size_t k = 0; k < l.size(); ++k) { ok &= std::isfinite (l[k]) && std::isfinite (r[k]); worst = jmax (worst, std::abs (l[k]), std::abs (r[k])); }
        }
        check (ok && worst <= 1.0f, String (p->getNumPrograms()) + " presets, driven hard: finite and under 0 dB (peak " + String (worst, 3) + ")");
    }

    // the project keeps the knobs
    {
        auto p = make();
        set (*p, "heat", 0.83f); set (*p, "dripTime", 4.0f);
        MemoryBlock m;
        p->getStateInformation (m);
        auto q = make();
        q->setStateInformation (m.getData(), (int) m.getSize());
        check (std::abs (q->apvts.getRawParameterValue ("heat")->load() - 0.83f) < 0.001f && q->apvts.getRawParameterValue ("dripTime")->load() == 4.0f,
               "saved project: the knobs come back");
    }

    // the lava runs and stays in its box
    {
        LavaSim sim;
        sim.setSize (320, 220);
        int drips = 0;
        for (int f = 0; f < 600; ++f)
        {
            LavaSim::Input in;
            in.heat = f < 300 ? 1.0f : 0.1f; in.drip = 1.0f; in.level = (f % 20) < 3 ? 1.0f : 0.0f;
            if (f % 15 == 0) ++drips;
            in.dripCount = drips;
            sim.step (in);
        }
        sim.render();
        check (sim.image().isValid() && sim.numFree() < 120, "the lava simulation runs 20 s hot and cold without falling apart");
    }

    std::printf (failures == 0 ? "MOLTEN CHECK PASSED\n" : "MOLTEN CHECK FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}

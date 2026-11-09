// Swarm's check: silence stays silent, every corner of the nebula is as loud as what came in, grains don't click,
// pitched grains land on the notes they're allowed to, FREEZE holds, PULSE plays on the grid, the lows stay dry,
// LIFE moves (and stays put without it), the presets and the dice are safe, the state comes back.
#include "PluginProcessor.h"
#include <complex>
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

static std::unique_ptr<SwarmProcessor> make()
{
    auto p = std::make_unique<SwarmProcessor>();
    p->setPlayConfigDetails (2, 2, sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

static void set (SwarmProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static std::pair<std::vector<float>, std::vector<float>> run (SwarmProcessor& p, const std::vector<float>& x)
{
    std::vector<float> ol, orr;
    AudioBuffer<float> buf (2, block);
    MidiBuffer midi;
    for (size_t pos = 0; pos < x.size(); pos += block)
    {
        const int n = (int) jmin ((size_t) block, x.size() - pos);
        buf.setSize (2, n, false, false, true);
        buf.copyFrom (0, 0, x.data() + pos, n);
        buf.copyFrom (1, 0, x.data() + pos, n);
        p.processBlock (buf, midi);
        for (int i = 0; i < n; ++i) { ol.push_back (buf.getSample (0, i)); orr.push_back (buf.getSample (1, i)); }
    }
    return { ol, orr };
}

static double rms (const std::vector<float>& x, size_t from, size_t to)
{
    double s = 0.0;
    to = jmin (to, x.size());
    for (size_t i = from; i < to; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) jmax ((size_t) 1, to - from));
}

static float peak (const std::vector<float>& x) { float m = 0; for (auto v : x) m = jmax (m, std::abs (v)); return m; }
static bool finite (const std::vector<float>& x) { for (auto v : x) if (! std::isfinite (v)) return false; return true; }

static std::vector<float> sine (double seconds, double hz, float amp)
{
    std::vector<float> x ((size_t) (seconds * sr));
    for (size_t i = 0; i < x.size(); ++i) x[i] = amp * (float) std::sin (MathConstants<double>::twoPi * hz * (double) i / sr);
    return x;
}

/** Something like music: a soft saw chord. */
static std::vector<float> musical (double seconds, float amp)
{
    std::vector<float> x ((size_t) (seconds * sr));
    for (size_t i = 0; i < x.size(); ++i)
    {
        double v = 0.0;
        for (double f : { 220.0, 277.18, 329.63 })
            for (int k = 1; k <= 8; ++k) v += std::sin (MathConstants<double>::twoPi * f * k * (double) i / sr) / k;
        x[i] = amp * (float) (v / 6.0);
    }
    return x;
}

/** Power spectrum of x[from .. from + 2^bits) (Hann window). */
static std::vector<double> spectrum (const std::vector<float>& x, size_t from, int bits)
{
    const size_t n = (size_t) 1 << bits;
    std::vector<std::complex<double>> a (n);
    for (size_t i = 0; i < n; ++i)
        a[i] = (double) x[from + i] * (0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * (double) i / (double) n));
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const auto w = std::polar (1.0, -MathConstants<double>::twoPi / (double) len);
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> wn (1.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * wn;
                a[i + k] = u + v; a[i + k + len / 2] = u - v;
                wn *= w;
            }
        }
    }
    std::vector<double> p (n / 2);
    for (size_t i = 0; i < n / 2; ++i) p[i] = std::norm (a[i]);
    return p;
}

static double band (const std::vector<double>& p, double lo, double hi, int bits)
{
    const double binHz = sr / (double) ((size_t) 1 << bits);
    double s = 0.0;
    for (size_t i = (size_t) jmax (1.0, lo / binHz); i < p.size() && (double) i * binHz <= hi; ++i) s += p[i];
    return s;
}

static void calm (SwarmProcessor& p)
{
    set (p, "pitch", 0.0f); set (p, "dirt", 0.0f); set (p, "tone", 0.5f); set (p, "space", 0.0f); set (p, "life", 0.0f);
    set (p, "mix", 1.0f); set (p, "scatter", 0.2f);
}

int main()
{
    ScopedJuceInitialiser_GUI init;

    // silence in, silence out (no hiss, no denormal junk), wherever the point is
    {
        bool ok = true;
        for (auto [x, y] : { std::pair { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } })
        {
            auto p = make();
            set (*p, "x", x); set (*p, "y", y); set (*p, "life", 1.0f); set (*p, "dirt", 1.0f);
            const auto [l, r] = run (*p, std::vector<float> ((size_t) (2.0 * sr), 0.0f));
            ok &= peak (l) < 1.0e-6f && peak (r) < 1.0e-6f;
        }
        check (ok, "silence in, silence out (all four corners, life and dirt up)");
    }

    // every corner about as loud as what came in
    {
        const auto in = musical (6.0, 0.3f);
        const double inR = rms (in, (size_t) (3.0 * sr), in.size());
        String levels;
        bool ok = true;
        for (auto [x, y] : { std::pair { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.5f, 0.5f } })
        {
            auto p = make();
            set (*p, "x", x); set (*p, "y", y); set (*p, "mix", 1.0f);
            const auto [l, r] = run (*p, in);
            const double db = 20.0 * std::log10 ((rms (l, (size_t) (3.0 * sr), l.size()) + rms (r, (size_t) (3.0 * sr), r.size())) * 0.5 / inR);
            levels << String (db, 1) << " ";
            ok &= finite (l) && finite (r) && std::abs (db) < 3.0 && peak (l) <= 1.0f;
        }
        check (ok, "auto gain: DREAM, PULSE, DARK, WILD, middle within 3 dB of the input (" + levels.trim() + " dB)");
    }

    // grains are smooth: a pure tone doesn't turn into clicks
    {
        String res;
        bool ok = true;
        for (auto [x, y, limit] : { std::tuple { 0.0f, 0.0f, -40.0 }, { 0.0f, 1.0f, -40.0 }, { 1.0f, 0.0f, -30.0 } })
        {
            auto p = make();
            calm (*p);
            set (*p, "x", x); set (*p, "y", y);
            const auto in = sine (4.0, 330.0, 0.3f);
            const auto [l, r] = run (*p, in);
            const auto sp = spectrum (l, (size_t) (2.0 * sr), 16);
            const double hf = 10.0 * std::log10 (band (sp, 3000.0, 20000.0, 16) / (band (sp, 20.0, 20000.0, 16) + 1e-30));
            res << String (hf, 1) << " ";
            ok &= hf < limit;
        }
        check (ok, "no clicks: energy above 3 kHz from a 330 Hz tone (DREAM, DARK < -40 dB, PULSE chops < -30 dB): " + res.trim() + " dB");
    }

    // the shortest grains (GRAIN at 0, chopped) still don't click
    {
        String res;
        bool ok = true;
        for (auto [x, y, shape] : { std::tuple { 1.0f, 0.0f, 0.5f }, { 1.0f, 1.0f, 0.5f }, { 1.0f, 0.0f, 1.0f }, { 0.5f, 0.5f, 1.0f } })
        {
            auto p = make();
            calm (*p);
            set (*p, "x", x); set (*p, "y", y); set (*p, "grain", 0.15f); set (*p, "density", 1.0f); set (*p, "shape", shape);
            const auto [l, r] = run (*p, sine (4.0, 330.0, 0.3f));
            const auto sp = spectrum (l, (size_t) (2.0 * sr), 16);
            const double hf = 10.0 * std::log10 (band (sp, 3000.0, 20000.0, 16) / (band (sp, 20.0, 20000.0, 16) + 1e-30));
            res << String (hf, 1) << " ";
            ok &= hf < -32.0;
        }
        check (ok, "no clicks with 20 ms grains (PULSE, WILD, fully chopped): above 3 kHz " + res.trim() + " dB (< -32)");
    }

    // micro grains (2 ms, GRAIN at 0): a fizz, but every grain is a smooth bump - no jumps in the sound
    {
        String res;
        bool ok = true;
        for (auto [x, y, shape] : { std::tuple { 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.5f }, { 0.0f, 0.0f, 0.5f } })
        {
            auto p = make();
            calm (*p);
            set (*p, "x", x); set (*p, "y", y); set (*p, "grain", 0.0f); set (*p, "density", 1.0f); set (*p, "shape", shape);
            const auto [l, r] = run (*p, sine (3.0, 330.0, 0.3f));
            float jump = 0.0f, level = 0.0f;
            for (size_t i = (size_t) sr + 1; i < l.size(); ++i) { jump = jmax (jump, std::abs (l[i] - l[i - 1])); level = jmax (level, std::abs (l[i])); }
            res << String (jump / jmax (1.0e-6f, level), 3) << " ";
            ok &= jump < 0.25f * level && level > 0.01f;
        }
        check (ok, "micro grains (2 ms) are smooth: the biggest step between samples is " + res.trim() + " of the peak (a click would be ~1)");
    }

    // MORE: UP / DOWN all the way up = no grain goes down; WIDTH 0 = mono; DUCK steps back while the sound plays
    {
        auto p = make();
        calm (*p);
        set (*p, "x", 0.5f); set (*p, "y", 0.5f); set (*p, "pitch", 1.0f); set (*p, "grain", 0.7f); set (*p, "updown", 1.0f); set (*p, "width", 0.0f);
        const auto [l, r] = run (*p, sine (5.0, 440.0, 0.3f));
        const int bits = 17;
        const auto sp = spectrum (l, (size_t) (2.0 * sr), bits);
        const double total = band (sp, 40.0, 20000.0, bits), below = band (sp, 40.0, 400.0, bits);
        double diff = 0.0;
        for (size_t i = (size_t) (2.0 * sr); i < l.size(); ++i) diff = jmax (diff, (double) std::abs (l[i] - r[i]));
        check (below / total < 0.01 && diff < 1.0e-4, "UP only: " + String (100.0 * below / total, 2) + "% of the sound under the note; WIDTH 0: left = right");

        auto dk = [] (float duck)
        {
            auto q = make();
            set (*q, "x", 0.2f); set (*q, "y", 0.2f); set (*q, "mix", 1.0f); set (*q, "duck", duck); set (*q, "space", 0.0f);
            const auto in = musical (4.0, 0.3f);
            const auto [ql, qr] = run (*q, in);
            return rms (ql, (size_t) (2.0 * sr), ql.size());
        };
        const double d0 = dk (0.0f), d1 = dk (1.0f);
        check (d1 < d0 * 0.5, "DUCK: the swarm steps back under a playing sound (" + String (20.0 * std::log10 (d1 / d0), 1) + " dB)");
    }

    // pitched grains only land on allowed notes
    {
        auto p = make();
        calm (*p);
        set (*p, "x", 0.5f); set (*p, "y", 0.0f); set (*p, "pitch", 1.0f); set (*p, "scale", 2.0f); set (*p, "grain", 0.7f);  // minor
        const auto [l, r] = run (*p, sine (5.0, 220.0, 0.3f));
        const int bits = 17;
        const auto sp = spectrum (l, (size_t) (2.0 * sr), bits);
        double allowed = 0.0, moved = 0.0;
        const int semis[] = { 0, 12, 24, -12, 7, -5, 2, 3, 5, 8, 10, -10, -9, -7, -4, -2 };
        for (int s : semis)
        {
            const double f = 220.0 * std::pow (2.0, s / 12.0);
            const double e = band (sp, f * 0.971, f * 1.029, bits);   // within a quarter tone
            allowed += e;
            if (s != 0) moved += e;
        }
        const double total = band (sp, 40.0, 20000.0, bits);
        check (allowed / total > 0.9 && moved / total > 0.1,
               "PITCH in minor: " + String (100.0 * allowed / total, 1) + "% of the sound on allowed notes, " + String (100.0 * moved / total, 1) + "% moved off the original");
    }

    // FREEZE holds the sound after it stopped, and lets go
    {
        auto p = make();
        calm (*p);
        set (*p, "x", 0.2f); set (*p, "y", 0.2f);
        auto in = musical (0.9, 0.3f);
        run (*p, in);
        set (*p, "freeze", 1.0f);
        const auto [l1, r1] = run (*p, std::vector<float> ((size_t) (3.0 * sr), 0.0f));
        const double held = rms (l1, (size_t) (2.0 * sr), l1.size());
        set (*p, "freeze", 0.0f);
        const auto [l2, r2] = run (*p, std::vector<float> ((size_t) (5.0 * sr), 0.0f));
        const double after = rms (l2, (size_t) (4.0 * sr), l2.size());
        const double inR = rms (in, 0, in.size());
        check (held > inR * 0.25 && after < inR * 0.01,
               "FREEZE: 2-3 s after the sound stopped it still plays (" + String (20.0 * std::log10 (held / inR), 1) + " dB), off: it dies away ("
                   + String (20.0 * std::log10 (after / inR + 1e-9), 1) + " dB)");
    }

    // PULSE: grains on the 1/16 grid (120 BPM = a step every 6000 samples)
    {
        auto p = make();
        calm (*p);
        set (*p, "x", 1.0f); set (*p, "y", 0.0f); set (*p, "density", 1.0f); set (*p, "scatter", 0.0f); set (*p, "grain", 0.3f); set (*p, "sync", 1.0f);
        const auto [l, r] = run (*p, musical (4.0, 0.3f));
        double head = 0.0, tailE = 0.0;
        for (size_t s = 8; s < 30; ++s)
        {
            const size_t a = s * 6000;
            head += rms (l, a, a + 1500);
            tailE += rms (l, a + 3500, a + 6000);
        }
        check (head > tailE * 3.0, "PULSE chops in time: the start of each 1/16 is " + String (20.0 * std::log10 (head / (tailE + 1e-9)), 1) + " dB louder than its end");
    }

    // the lows stay dry and solid, whatever MIX says
    {
        bool ok = true;
        String res;
        for (float mix : { 0.0f, 1.0f })
        {
            auto p = make();
            set (*p, "x", 1.0f); set (*p, "y", 1.0f); set (*p, "mix", mix); set (*p, "life", 1.0f);
            const auto in = sine (4.0, 50.0, 0.3f);
            const auto [l, r] = run (*p, in);
            const int bits = 16;
            const auto si = spectrum (in, (size_t) (2.0 * sr), bits), so = spectrum (l, (size_t) (2.0 * sr), bits);
            const double db = 10.0 * std::log10 (band (so, 45.0, 55.0, bits) / band (si, 45.0, 55.0, bits));
            res << String (db, 1) << " ";
            ok &= std::abs (db) < 1.5;
        }
        check (ok, "a 50 Hz bass comes out as it went in, at MIX 0 and 100% (" + res.trim() + " dB)");
    }

    // LIFE moves the point round yours; without it, it stays
    {
        auto p = make();
        set (*p, "x", 0.5f); set (*p, "y", 0.5f); set (*p, "life", 1.0f);
        run (*p, musical (2.0, 0.3f));
        const float x1 = p->engine.wanderX.load(), y1 = p->engine.wanderY.load();
        run (*p, musical (4.0, 0.3f));
        const float x2 = p->engine.wanderX.load(), y2 = p->engine.wanderY.load();
        const float moved = std::hypot (x2 - x1, y2 - y1);
        auto q = make();
        set (*q, "x", 0.3f); set (*q, "y", 0.7f); set (*q, "life", 0.0f);
        run (*q, musical (3.0, 0.3f));
        check (moved > 0.03f && std::abs (q->engine.wanderX.load() - 0.3f) < 1e-4f && std::abs (q->engine.wanderY.load() - 0.7f) < 1e-4f,
               "LIFE wanders on its own (moved " + String (moved, 3) + " in 4 s), LIFE 0 stays exactly where you put it");
    }

    // presets: safe and not too loud or quiet
    {
        const auto in = musical (4.0, 0.3f);
        const double inR = rms (in, (size_t) (2.0 * sr), in.size());
        bool ok = true;
        String bad;
        for (int i = 0; i < (int) SwarmProcessor::presets().size(); ++i)
        {
            auto p = make();
            p->setCurrentProgram (i);
            const auto [l, r] = run (*p, in);
            const double db = 20.0 * std::log10 (rms (l, (size_t) (2.0 * sr), l.size()) / inR + 1e-9);
            const bool good = finite (l) && finite (r) && peak (l) <= 1.0f && peak (r) <= 1.0f && std::abs (db) < 6.0;
            if (! good) bad << SwarmProcessor::presets()[(size_t) i].name << " (" << String (db, 1) << " dB) ";
            ok &= good;
        }
        check (ok, String ((int) SwarmProcessor::presets().size()) + " presets: finite, under 0 dB, within 6 dB of the input " + bad);
    }

    // the dice: 12 rolls, all safe
    {
        auto p = make();
        bool ok = true;
        const auto in = musical (1.5, 0.3f);
        for (int i = 0; i < 12; ++i)
        {
            p->randomise();
            const auto [l, r] = run (*p, in);
            ok &= finite (l) && finite (r) && peak (l) <= 1.0f;
        }
        check (ok && p->apvts.getRawParameterValue ("mix")->load() == 0.6f, "the dice: 12 new sounds, all safe, MIX untouched");
    }

    // a new sample rate (the ring gets smaller) mid-way doesn't break anything
    {
        auto p = std::make_unique<SwarmProcessor>();
        p->setPlayConfigDetails (2, 2, 96000.0, block);
        p->prepareToPlay (96000.0, block);
        run (*p, musical (1.0, 0.3f));
        p->prepareToPlay (44100.0, 256);
        const auto [l, r] = run (*p, musical (1.0, 0.3f));
        check (finite (l) && peak (l) <= 1.0f, "96 kHz -> 44.1 kHz and smaller blocks: still fine");
    }

    // the state comes back
    {
        auto p = make();
        set (*p, "x", 0.83f); set (*p, "scale", 3.0f); set (*p, "life", 0.12f);
        MemoryBlock m;
        p->getStateInformation (m);
        auto q = make();
        q->setStateInformation (m.getData(), (int) m.getSize());
        check (std::abs (q->apvts.getRawParameterValue ("x")->load() - 0.83f) < 0.001f && q->apvts.getRawParameterValue ("scale")->load() == 3.0f
                   && std::abs (q->apvts.getRawParameterValue ("life")->load() - 0.12f) < 0.001f,
               "saved project: the point and the knobs come back");
    }

    std::printf (failures == 0 ? "SWARM CHECK PASSED\n" : "SWARM CHECK FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}

// Cat Box Pocket: tests of the whole instrument on the PC (no hardware).
//   build: see host/build.sh     run: ./pocket_test [out folder]
#include "../engine/pocket.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <new>

using namespace pocket;

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (! (cond)) { ++fails; std::printf ("FAIL %s:%d  ", __FILE__, __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static std::vector<int16_t> pool (4u * 1024 * 1024);   // 8 MB like the PSRAM
static Engine* E = new Engine ();                         // big (effect buffers): not on the stack
static Ui* U = nullptr;

static std::vector<int16_t> sine (float hz, float secs, float amp = 0.5f, uint32_t rate = SR) {
    std::vector<int16_t> v ((size_t) (secs * rate));
    for (size_t i = 0; i < v.size (); ++i) v[i] = (int16_t) (amp * 32767 * std::sin (2 * M_PI * hz * i / rate));
    return v;
}
static std::vector<float> run (float secs, const float* in = nullptr) {
    const int n = (int) (secs * SR);
    std::vector<float> out (n * 2);
    for (int i = 0; i < n; i += 64) {
        const int m = std::min (64, n - i);
        E->process (in ? in + i : nullptr, out.data () + 2 * i, m);
    }
    return out;
}
static float rms (const std::vector<float>& o, int a = 0, int b = -1) {
    if (b < 0) b = (int) o.size () / 2;
    double s = 0; for (int i = a; i < b; ++i) s += o[2 * i] * o[2 * i];
    return (float) std::sqrt (s / std::max (1, b - a));
}
// frequency by interpolated zero crossings of the left channel
static float freq (const std::vector<float>& o, int a, int b) {
    double first = -1, last = -1; int n = 0;
    for (int i = a + 1; i < b; ++i) {
        const float x0 = o[2 * (i - 1)], x1 = o[2 * i];
        if (x0 < 0 && x1 >= 0) { const double t = i - 1 + x0 / (x0 - x1); if (first < 0) first = t; last = t; ++n; }
    }
    return n > 1 ? (float) ((n - 1) * SR / (last - first)) : 0;
}
// pitch by autocorrelation over 4096 samples (works across grain joins)
static float pitchAC (const std::vector<float>& o, int a) {
    const int N = 4096; double best = -1; int lag = 0;
    for (int L = SR / 2000; L < SR / 50; ++L) {
        double s = 0; for (int i = 0; i < N; ++i) s += o[2 * (a + i)] * o[2 * (a + i + L)];
        if (s > best) { best = s; lag = L; }
    }
    // parabolic peak
    auto ac = [&] (int L) { double s = 0; for (int i = 0; i < N; ++i) s += o[2 * (a + i)] * o[2 * (a + i + L)]; return s; };
    const double y0 = ac (lag - 1), y1 = ac (lag), y2 = ac (lag + 1);
    const double d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2);
    return (float) (SR / (lag + d));
}
// the strongest frequency between lo and hi (a DFT scan, robust to grain joins)
static float peakHz (const std::vector<float>& o, int a, int n, float lo, float hi) {
    float best = 0, bh = 0;
    for (float f = lo; f <= hi; f += 0.25f) {
        double re = 0, im = 0;
        for (int i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * M_PI * i / n); re += w * o[2 * (a + i)] * std::cos (2 * M_PI * f * i / SR); im += w * o[2 * (a + i)] * std::sin (2 * M_PI * f * i / SR); }
        const float m = (float) (re * re + im * im); if (m > best) { best = m; bh = f; }
    }
    return bh;
}
static std::vector<int> onsets (const std::vector<float>& o, float thr = 0.05f) {
    std::vector<int> r; bool quiet = true; int q = 0;
    for (int i = 0; i < (int) o.size () / 2; ++i) {
        const float a = std::fabs (o[2 * i]);
        if (quiet && a > thr) { r.push_back (i); quiet = false; q = 0; }
        else if (! quiet) { if (a < thr * 0.2f) { if (++q > 400) quiet = true; } else q = 0; }
    }
    return r;
}
static void reset () {
    E->init (pool.data (), (uint32_t) pool.size ());
    E->delayAmt = 0; E->reverbAmt = 0; E->master = 1; E->bpm = 120; E->swing = 0; E->lofi = false; E->fx = 0;
    E->play (false); E->allOff ();
    U->~Ui (); new (U) Ui (*E);
}
static void writeWav (const std::string& path, const std::vector<float>& o) {
    std::vector<int16_t> s (o.size ());
    for (size_t i = 0; i < o.size (); ++i) s[i] = (int16_t) std::max (-32767.0f, std::min (32767.0f, o[i] * 32767));
    FILE* f = std::fopen (path.c_str (), "wb"); if (! f) return;
    uint8_t h[44]; const uint32_t frames = (uint32_t) o.size () / 2;
    std::memcpy (h, "RIFF", 4); uint32_t v = 36 + frames * 4; std::memcpy (h + 4, &v, 4); std::memcpy (h + 8, "WAVEfmt ", 8);
    v = 16; std::memcpy (h + 16, &v, 4); uint16_t w = 1; std::memcpy (h + 20, &w, 2); w = 2; std::memcpy (h + 22, &w, 2);
    v = SR; std::memcpy (h + 24, &v, 4); v = SR * 4; std::memcpy (h + 28, &v, 4); w = 4; std::memcpy (h + 32, &w, 2); w = 16; std::memcpy (h + 34, &w, 2);
    std::memcpy (h + 36, "data", 4); v = frames * 4; std::memcpy (h + 40, &v, 4);
    std::fwrite (h, 1, 44, f); std::fwrite (s.data (), 2, s.size (), f); std::fclose (f);
}
static void writeScreen (const std::string& path) {   // the OLED as a PGM (4x)
    uint8_t fb[1024]; U->render (fb);
    FILE* f = std::fopen (path.c_str (), "wb"); if (! f) return;
    std::fprintf (f, "P5 %d %d 255\n", 512, 256);
    for (int y = 0; y < 256; ++y) for (int x = 0; x < 512; ++x) {
        const int X = x / 4, Y = y / 4; const bool on = fb[(Y >> 3) * 128 + X] >> (Y & 7) & 1;
        std::fputc (on ? 235 : 12, f);
    }
    std::fclose (f);
}
static void press (int k) { U->key (k, true); }
static void lift (int k) { U->key (k, false); }

struct RamDisk : BlockDev {
    std::vector<uint8_t> d; explicit RamDisk (uint32_t secs) : d (secs * 512, 0xFF) {}
    uint32_t sectors () const override { return (uint32_t) (d.size () / 512); }
    bool read (uint32_t s, uint8_t* b) override { if (s >= sectors ()) return false; std::memcpy (b, &d[s * 512], 512); return true; }
    bool write (uint32_t s, const uint8_t* b) override { if (s >= sectors ()) return false; std::memcpy (&d[s * 512], b, 512); return true; }
};

#include "test_v2.inc"

int main (int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    U = new Ui (*E);

    // ---- playing a sound, pitch, lo-fi rate
    reset ();
    { auto s = sine (440, 1.0f); E->assign (0, s.data (), (uint32_t) s.size (), SR); }
    E->noteOn (0, 0);
    auto o = run (0.3f);
    CHECK (std::fabs (freq (o, 2000, 14000) - 440) < 2, "pad plays 440 Hz: %.1f", freq (o, 2000, 14000));
    CHECK (rms (o, 2000, 14000) > 0.2f, "pad is loud enough: %.3f", rms (o, 2000, 14000));
    E->slots[0].pitch = 12; E->noteOn (0, 0); o = run (0.2f);
    CHECK (std::fabs (freq (o, 2000, 9000) - 880) < 4, "pitch +12 = 880 Hz: %.1f", freq (o, 2000, 9000));
    E->slots[0].pitch = 0;
    { auto s = sine (440, 0.5f, 0.5f, 24000); E->assign (2, s.data (), (uint32_t) s.size (), 24000); }
    E->allOff (); E->noteOn (2, 2); o = run (0.2f);
    CHECK (std::fabs (freq (o, 2000, 9000) - 440) < 3, "a 24 kHz sound plays at the right pitch: %.1f", freq (o, 2000, 9000));
    // one-shot ends; the end is not cut off early
    E->allOff (); E->noteOn (2, 2); o = run (0.7f);
    CHECK (rms (o, (int) (0.45f * SR), (int) (0.49f * SR)) > 0.2f && rms (o, (int) (0.55f * SR), (int) (0.65f * SR)) < 0.001f, "one-shot plays 0.5 s and stops");
    // reverse + trim
    E->slots[0].reverse = true; E->slots[0].trimA = 0.5f; E->allOff (); E->noteOn (0, 0); o = run (0.6f);
    CHECK (rms (o, (int) (0.40f * SR), (int) (0.48f * SR)) > 0.2f && rms (o, (int) (0.52f * SR), (int) (0.58f * SR)) < 0.001f, "reverse plays the trimmed half (0.5 s)");
    E->slots[0].reverse = false; E->slots[0].trimA = 0;
    // loop while held
    E->slots[2].loopHold = true; E->allOff (); E->noteOn (2, 2); o = run (1.2f);
    CHECK (rms (o, (int) (1.0f * SR), (int) (1.1f * SR)) > 0.2f, "loop keeps playing while held");
    E->noteOff (2); o = run (0.2f);
    CHECK (rms (o, (int) (0.1f * SR), (int) (0.2f * SR)) < 0.001f, "loop stops after the pad is let go");
    E->slots[2].loopHold = false;
    // filter: low-pass at the bottom takes the 440 down
    E->slots[0].filter = -0.8f; E->allOff (); E->noteOn (0, 0); o = run (0.3f);
    CHECK (rms (o, 4000, 14000) < 0.05f, "low-pass closed = quiet: %.3f", rms (o, 4000, 14000));
    E->slots[0].filter = 0;

    // ---- recording from the input (clean and lo-fi)
    E->allOff ();
    { auto in = sine (330, 0.4f); std::vector<float> f (in.size ()); for (size_t i = 0; i < in.size (); ++i) f[i] = in[i] / 32768.0f;
      E->recStart (5, false); run (0.3f, f.data ()); E->recStop ();
      CHECK (std::abs ((int) E->slots[5].len - (int) (0.3f * SR)) < 128, "recorded 0.3 s: %u", E->slots[5].len);
      E->noteOn (5, 5); o = run (0.25f);
      CHECK (std::fabs (freq (o, 1000, 10000) - 330) < 2, "the recording plays back at 330 Hz: %.1f", freq (o, 1000, 10000));
      E->lofi = true; E->recStart (6, false); run (0.3f, f.data ()); E->recStop (); E->lofi = false;
      CHECK (std::abs ((int) E->slots[6].len - (int) (0.15f * SR)) < 128 && E->slots[6].rate == SR / 2, "lo-fi stores half the samples: %u", E->slots[6].len);
      E->allOff (); E->noteOn (6, 6); o = run (0.25f);
      CHECK (std::fabs (freq (o, 1000, 10000) - 330) < 3, "lo-fi recording plays at 330 Hz: %.1f", freq (o, 1000, 10000)); }
    // resampling: the output into a slot
    E->allOff (); E->noteOn (0, 0); E->recStart (7, true); run (0.2f); E->recStop ();
    { const int16_t* d; const uint32_t n = E->slotData (7, &d); double s = 0; for (uint32_t i = 0; i < n; ++i) s += (d[i] / 32768.0) * (d[i] / 32768.0);
      CHECK (n > 9000 && std::sqrt (s / n) > 0.2, "resampling recorded the output: %u samples", n); }

    // ---- pool: freeing a sound moves the ones after it, data intact
    { const int16_t* d; E->slotData (5, &d); const int16_t before = d[1234];
      const uint32_t used = E->memUsed; const uint32_t len2 = E->slots[2].len;
      E->clearSlot (2);
      E->slotData (5, &d);
      CHECK (E->memUsed == used - len2 && d[1234] == before, "freeing a sound compacts the memory");
      CHECK (E->slots[2].empty () && ! E->slots[5].empty (), "only that sound is gone"); }

    // ---- sequencer: 4 on the floor, tempo, swing
    reset ();
    { std::vector<int16_t> k (2400); for (size_t i = 0; i < k.size (); ++i) k[i] = (int16_t) (20000 * std::sin (2 * M_PI * 60 * i / SR) * (1 - i / 2400.0));
      E->assign (0, k.data (), (uint32_t) k.size (), SR); }
    for (int s : {0, 4, 8, 12}) E->toggleStep (s, 0);
    E->play (true); o = run (2.0f); E->play (false);
    { auto on = onsets (o); CHECK (on.size () == 4, "4 kicks in 2 s at 120 BPM: %zu", on.size ());
      if (on.size () >= 2) CHECK (std::abs (on[1] - on[0] - SR / 2) < 64, "kicks every 0.5 s: %d", on[1] - on[0]); }
    E->swing = 0.3f; E->clearPattern (0); E->toggleStep (0, 0); E->toggleStep (1, 0);
    E->play (true); o = run (0.5f); E->play (false); E->swing = 0;
    { auto on = onsets (o, 0.02f); CHECK (on.size () >= 2 && std::abs (on[1] - on[0] - (int) (0.125f * SR * 1.3f)) < 64, "swing pushes the 2nd step late: %d", on.size () >= 2 ? on[1] - on[0] : -1); }
    // euclid and chaos
    E->clearPattern (0); E->euclid (0, 5, 0);
    { int n = 0; for (int s = 0; s < 16; ++s) n += E->hasTrig (0, s, 0); CHECK (n == 5, "euclid 5 of 16: %d", n); }
    E->euclid (0, 5, 3);
    { int n = 0; for (int s = 0; s < 16; ++s) n += E->hasTrig (0, s, 0); CHECK (n == 5 && E->hasTrig (0, 3, 0), "euclid rotated by 3"); }
    { Pattern before = E->pats[0]; E->chaos (1.0f); bool diff = std::memcmp (&before, &E->pats[0], sizeof (Pattern)) != 0;
      int n = 0; for (auto& st : E->pats[0].s) n += st.n;
      CHECK (diff && n >= 2 && n <= 16, "chaos changes the pattern and keeps it playable: %d hits", n); }
    // p-lock pitch on a step
    E->clearPattern (0); E->toggleStep (0, 0); E->trig (0, 0, 0)->pitch = 12;
    { auto s = sine (440, 0.3f); E->assign (1, s.data (), (uint32_t) s.size (), SR); }
    E->clearPattern (0); E->toggleStep (0, 1); E->trig (0, 0, 1)->pitch = 12;
    E->play (true); o = run (0.1f); E->play (false);
    CHECK (std::fabs (freq (o, 500, 4500) - 880) < 6, "step pitch lock +12: %.1f", freq (o, 500, 4500));
    // chain of patterns: 1 then 2
    E->clearPattern (0); E->clearPattern (1); E->toggleStep (0, 0); E->toggleStep (8, 0); E->pat = 1; E->toggleStep (0, 1); E->pat = 0;
    { uint8_t ch[2] = {0, 1}; E->setChain (ch, 2); }
    E->play (true); o = run (8.0f); E->play (false);
    { auto on = onsets (o, 0.02f); CHECK (on.size () == 6, "chain 1+2 twice = 6 hits: %zu", on.size ());
      if (on.size () == 6) CHECK (std::abs (on[2] - 2 * SR) < 64 && std::abs (on[3] - 4 * SR) < 64, "pattern 2 starts on bar 2, then pattern 1 again"); }
    // live record is quantised
    E->clearPattern (0); { uint8_t ch[1] = {0}; E->setChain (ch, 1); }
    E->play (true); run (0.125f * 4.6f);   // in step 4, past its middle -> step 5
    E->liveRecord (0, NOLOCK); E->play (false);
    CHECK (E->hasTrig (0, 5, 0), "live record lands on the nearest step");
    // roll
    E->allOff (); E->rollSlot = 0; E->rollPad = 0; E->rollDiv = 4; o = run (1.0f); E->rollSlot = -1;
    { auto on = onsets (o); CHECK (on.size () == 8, "roll 1/16 at 120 BPM = 8 hits a second: %zu", on.size ()); }

    // ---- chop: 8 clicks -> 8 slices in the first free bank
    reset ();
    { std::vector<int16_t> s (SR * 2, 0);
      for (int k = 0; k < 8; ++k) for (int i = 0; i < 2000; ++i) s[k * SR / 4 + 100 + i] = (int16_t) (16000 * std::sin (2 * M_PI * 200 * i / SR) * (1 - i / 2000.0));
      E->assign (0, s.data (), (uint32_t) s.size (), SR); }
    { const int b = E->chop (0); int n = 0; for (int i = 0; i < 16; ++i) n += ! E->slots[16 + i].empty ();
      CHECK (b == 1 && n == 8, "chop finds the 8 hits (bank %d, %d slices)", b, n);
      CHECK (E->blocks[E->slots[0].block].refs == 9, "slices share the sample memory");
      E->noteOn (18, 18); o = run (0.2f); CHECK (rms (o, 0, 2000) > 0.1f, "a slice plays its hit"); }
    E->clearSlot (0);
    CHECK (! E->slots[16].empty () && E->memUsed > 0, "the slices live on after the source is cleared");

    // ---- time-stretch and freeze
    reset ();
    { auto s = sine (440, 2.0f); E->assign (0, s.data (), (uint32_t) s.size (), SR); }
    E->slots[0].stretch = true; E->slots[0].beats = 2; E->bpm = 120;      // 2 s -> 1 s
    E->noteOn (0, 0); o = run (1.5f);
    { const float hz = peakHz (o, 6000, 24000, 380, 500); CHECK (std::fabs (hz - 440) < 2, "stretch keeps the pitch: %.1f", hz); }
    CHECK (rms (o, (int) (0.85f * SR), (int) (0.95f * SR)) > 0.1f && rms (o, (int) (1.1f * SR), (int) (1.4f * SR)) < 0.01f, "stretch to 2 beats = 1 s");
    E->slots[0].stretch = false; E->slots[0].freeze = true; E->noteOn (0, 0); o = run (3.0f);
    CHECK (rms (o, (int) (2.6f * SR), (int) (2.9f * SR)) > 0.1f, "freeze holds the sound past its end");
    E->noteOff (0); o = run (0.2f); CHECK (rms (o, 4000, 9000) < 0.001f, "freeze fades when let go");
    E->slots[0].freeze = false;

    // ---- punch-in effects
    E->allOff (); E->noteOn (0, 0); run (0.3f);
    E->fx = 12; o = run (0.2f); E->fx = 0;
    CHECK (std::fabs (freq (o, 2000, 9000) - 880) < 6, "FX octave up: %.1f", freq (o, 2000, 9000));
    E->allOff (); E->noteOn (0, 0); run (0.3f);
    E->fx = 1; o = run (0.5f); E->fx = 0;
    { // loop 1/16 at 120 = 6000 samples: the output repeats with that period
      double diff = 0, ref = 0; for (int i = 8000; i < 14000; ++i) { diff += std::fabs (o[2 * i] - o[2 * (i + 6000)]); ref += std::fabs (o[2 * i]); }
      CHECK (ref > 100 && diff / ref < 0.05, "FX loop 1/16 repeats every 6000 samples (%.3f)", diff / std::max (ref, 1e-9)); }
    E->allOff (); E->fx = 9; E->noteOn (0, 0); o = run (0.2f); E->fx = 0;
    { int levels = 0; float last = 99; for (int i = 4000; i < 9000; ++i) if (o[2 * i] != last) { ++levels; last = o[2 * i]; }
      CHECK (levels < 1000, "FX bitcrush holds samples (%d changes in 5000)", levels); }
    // step-locked effect: a step with fx 13 plays an octave down
    E->allOff (); E->clearPattern (0); E->toggleStep (0, 0); E->pats[0].s[0].fx = 13;
    E->play (true); o = run (0.1f); E->play (false);
    CHECK (std::fabs (freq (o, 1000, 4500) - 220) < 4, "an effect locked on a step: %.1f", freq (o, 1000, 4500));

    // ---- delay and reverb
    reset ();
    { std::vector<int16_t> c (480); for (size_t i = 0; i < c.size (); ++i) c[i] = (int16_t) (20000 * std::sin (2 * M_PI * 1000 * i / SR));
      E->assign (0, c.data (), (uint32_t) c.size (), SR); }
    E->delayAmt = 1; E->noteOn (0, 0); o = run (1.0f);
    { const int d = (int) (3 * E->stepSamples ()); CHECK (rms (o, d + 50, d + 400) > 0.05f && rms (o, d - 1000, d - 100) < 0.01f, "delay echo after 3/16"); }
    E->delayAmt = 0; E->reverbAmt = 1; E->allOff (); run (1.0f); E->noteOn (0, 0); o = run (0.5f);
    CHECK (rms (o, 4000, 12000) > 0.002f, "reverb tail: %.4f", rms (o, 4000, 12000));
    CHECK (std::fabs (o[2 * 6000] - o[2 * 6000 + 1]) > 1e-5f, "reverb is stereo");

    // ---- looper: one bar overdub from the input
    reset ();
    { std::vector<float> in (SR * 3); for (size_t i = 0; i < in.size (); ++i) in[i] = 0.3f * std::sin (2 * M_PI * 500 * i / SR);
      E->looperNext (10); CHECK (E->looper () == Engine::LOOP_REC && E->playing (), "looper records and starts the beat");
      run (2.0f, in.data ());     // one bar at 120 = 2 s
      E->looperNext (10); CHECK (E->looper () == Engine::LOOP_PLAY, "looper plays");
      o = run (1.0f);
      CHECK (std::fabs (freq (o, 1000, 40000) - 500) < 3 && rms (o) > 0.1f, "the loop plays back what came in: %.1f Hz", freq (o, 1000, 40000));
      E->looperNext (10); CHECK (E->looper () == Engine::LOOP_OFF, "looper off"); E->play (false); }

    // ---- the UI: Pocket Operator style keys
    reset ();
    { auto s = sine (440, 0.5f); E->assign (0, s.data (), (uint32_t) s.size (), SR); E->assign (1, s.data (), (uint32_t) s.size (), SR); }
    U->tick (1000);
    press (K_REC); press (K_PAD0 + 3);
    { std::vector<float> in (SR / 4, 0.2f); run (0.25f, in.data ()); }
    lift (K_PAD0 + 3); lift (K_REC);
    CHECK (! E->slots[3].empty () && U->sel == 3, "REC + pad records into that pad");
    press (K_SOUND); press (K_SHIFT); press (K_PAD0 + 2); lift (K_PAD0 + 2); lift (K_SHIFT); lift (K_SOUND);
    CHECK (U->bank == 2, "SOUND + SHIFT + pad 3 = bank C");
    press (K_SOUND); press (K_SHIFT); press (K_PAD0 + 0); lift (K_PAD0); lift (K_SHIFT); lift (K_SOUND);
    CHECK (U->bank == 0, "back to bank A");
    press (K_SOUND); press (K_PAD0 + 1); lift (K_PAD0 + 1); lift (K_SOUND);
    CHECK (U->sel == 1, "SOUND + pad selects the sound");
    press (K_WRITE); lift (K_WRITE);
    CHECK (U->stepMode, "WRITE = step mode");
    press (K_PAD0 + 4); lift (K_PAD0 + 4);
    CHECK (E->hasTrig (0, 4, 1), "a pad in step mode adds a step");
    press (K_PAD0 + 4); U->encoder (0, 5); lift (K_PAD0 + 4);
    CHECK (E->hasTrig (0, 4, 1) && E->trig (0, 4, 1)->pitch == 5, "hold a step + turn A = pitch lock (step stays)");
    press (K_PAD0 + 4); lift (K_PAD0 + 4);
    CHECK (! E->hasTrig (0, 4, 1), "press it again: the step is gone");
    press (K_SHIFT); U->encoder (0, 4); lift (K_SHIFT);
    { int n = 0; for (int s = 0; s < 16; ++s) n += E->hasTrig (0, s, 1); CHECK (n == 8, "SHIFT + A in step mode = euclid (4+4=8): %d", n); }
    press (K_WRITE); lift (K_WRITE);
    CHECK (! U->stepMode, "WRITE again = play mode");
    press (K_PATTERN); press (K_PAD0 + 1); press (K_PAD0 + 2); lift (K_PAD0 + 1); lift (K_PAD0 + 2); lift (K_PATTERN);
    CHECK (E->chainLen == 2 && E->chain[0] == 1 && E->chain[1] == 2, "PATTERN + pads 2, 3 = a chain");
    U->tick (5000); press (K_BPM); lift (K_BPM); U->tick (5500); press (K_BPM); lift (K_BPM); U->tick (6000); press (K_BPM); lift (K_BPM);
    CHECK (std::fabs (E->bpm - 120) < 1, "tap tempo 2 taps a second = 120 BPM: %.1f", E->bpm);
    U->tick (20000); press (K_BPM); U->encoder (0, -20); U->encoder (1, 5); lift (K_BPM);
    CHECK (E->bpm == 100 && std::fabs (E->swing - 0.1f) < 1e-4f, "BPM + A = tempo, BPM + B = swing");
    press (K_BPM); press (K_PAD0); lift (K_PAD0); lift (K_BPM);
    CHECK (E->lofi, "BPM + pad 1 = lo-fi");
    press (K_FX); press (K_PAD0 + 8); CHECK (E->fx == 9, "FX + pad 9 = bitcrush"); lift (K_PAD0 + 8); lift (K_FX);
    CHECK (E->fx == 0, "the effect stops when let go");
    press (K_SHIFT); press (K_SOUND); lift (K_SOUND); lift (K_SHIFT);
    CHECK (U->keysMode, "SHIFT + SOUND = keys");
    press (K_PAD0 + 7); { bool any = false; for (auto& v : E->voices) any |= v.on && std::fabs (v.inc - std::pow (2.0f, 7 / 12.0f)) < 1e-3f; CHECK (any, "pad 8 in keys mode = a fifth up"); } lift (K_PAD0 + 7);
    press (K_SHIFT); press (K_SOUND); lift (K_SOUND); lift (K_SHIFT);
    U->sel = 0; press (K_SHIFT); press (K_REC); lift (K_REC); lift (K_SHIFT);
    CHECK (U->bank == 1 && ! E->slots[16].empty (), "SHIFT + REC = chop into a free bank");
    U->sel = 4; press (K_SHIFT); press (K_PLAY); lift (K_PLAY); lift (K_SHIFT);
    CHECK (E->looper () == Engine::LOOP_REC, "SHIFT + PLAY = looper");
    press (K_SHIFT); press (K_PLAY); lift (K_PLAY); press (K_PLAY); lift (K_PLAY); lift (K_SHIFT); E->play (false);
    press (K_SHIFT); press (K_WRITE); lift (K_WRITE); lift (K_SHIFT);
    CHECK (U->wantSave && ! U->stepMode, "SHIFT + WRITE = save (not step mode)");
    U->sel = 1; U->page = 0; U->encoder (0, 3); U->encoder (1, -10);
    CHECK (E->slots[1].pitch == 3 && E->slots[1].filter < -0.3f, "A / B = pitch / filter");
    press (K_ENCA); lift (K_ENCA); U->encoder (1, -20);
    CHECK (U->page == 1 && E->slots[1].trimB < 0.85f, "push A = next page (trim)");
    writeScreen (dir + "/screen_play.pgm");
    U->stepMode = true; writeScreen (dir + "/screen_steps.pgm"); U->stepMode = false;
    U->say ("CHOP -> BANKA B"); writeScreen (dir + "/screen_msg.pgm");
    { uint8_t rgb[48]; U->leds (rgb); int lit = 0; for (int i = 0; i < 16; ++i) lit += rgb[i * 3] + rgb[i * 3 + 1] + rgb[i * 3 + 2] > 0; CHECK (lit >= 3, "LEDs show the recorded pads: %d", lit); }

    // ---- storage: FAT16 disk, save and load, WAVs from a computer
    {
        RamDisk disk (28672);   // 14 MB
        CHECK (store::format (disk), "format the flash disk");
        CHECK (store::mounted (disk), "it mounts");
        E->slots[1].pitch = 5; E->bpm = 133; E->toggleStep (7, 1);
        const int16_t* d; const uint32_t n1 = E->slotData (1, &d); std::vector<int16_t> keep (d, d + n1);
        const Pattern pat0 = E->pats[0];
        CHECK (store::save (disk, *E, *U, 0), "save the project");
        char names[80][13]; uint32_t sizes[80]; const int nf = store::list (disk, names, sizes, 80);
        int wavs = 0; for (int i = 0; i < nf; ++i) wavs += std::strstr (names[i], ".WAV") != nullptr;
        int filled = 0; for (auto& s : E->slots) filled += ! s.empty ();
        CHECK (wavs == filled, "one WAV per sound: %d / %d", wavs, filled);
        // a stereo 24-bit WAV from the computer
        std::vector<uint8_t> w (44 + 1000 * 6);
        { uint8_t* h = w.data (); std::memcpy (h, "RIFF", 4); uint32_t v = (uint32_t) w.size () - 8; std::memcpy (h + 4, &v, 4); std::memcpy (h + 8, "WAVEfmt ", 8);
          v = 16; std::memcpy (h + 16, &v, 4); uint16_t s = 1; std::memcpy (h + 20, &s, 2); s = 2; std::memcpy (h + 22, &s, 2); v = 44100; std::memcpy (h + 24, &v, 4);
          v = 44100 * 6; std::memcpy (h + 28, &v, 4); s = 6; std::memcpy (h + 32, &s, 2); s = 24; std::memcpy (h + 34, &s, 2); std::memcpy (h + 36, "data", 4); v = 6000; std::memcpy (h + 40, &v, 4);
          for (int i = 0; i < 1000; ++i) { const int32_t x = (int32_t) (4000000 * std::sin (i * 0.1)); for (int c = 0; c < 2; ++c) { uint8_t* p = h + 44 + i * 6 + c * 3; p[0] = x & 255; p[1] = (x >> 8) & 255; p[2] = (x >> 16) & 255; } } }
        CHECK (store::writeFile (disk, "KICK.WAV", w.data (), (uint32_t) w.size ()), "copy a WAV onto the disk");
        reset ();
        CHECK (store::load (disk, *E, *U, 0), "load the project");
        const uint32_t n2 = E->slotData (1, &d);
        if (n2 != n1 || std::memcmp (d, keep.data (), n1 * 2)) { int bad = -1; for (uint32_t i = 0; i < std::min (n1, n2); ++i) if (d[i] != keep[i]) { bad = (int) i; break; } std::printf ("  A02: %u vs %u samples, first diff at %d\n", n2, n1, bad); }
        CHECK (n2 == n1 && std::memcmp (d, keep.data (), n1 * 2) == 0, "sound A02 came back sample for sample");
        CHECK (E->slots[1].pitch == 5 && E->bpm == 133 && std::memcmp (&pat0, &E->pats[0], sizeof (Pattern)) == 0, "parameters, tempo and patterns came back");
        int kick = -1; for (int s = 0; s < NSLOT; ++s) if (! E->slots[s].empty () && E->slots[s].rate == 44100) kick = s;
        CHECK (kick >= 0 && E->slots[kick].len == 1000, "the computer's WAV landed in an empty pad");
        if (kick >= 0) { const int16_t* k; E->slotData (kick, &k); CHECK (std::abs (k[16] - (int) (4000000 * std::sin (1.6) / 256)) < 4, "24-bit stereo -> mono 16-bit: %d", k[16]); }
        // a second save only rewrites what changed
        { RamDisk before = disk; E->slots[2].pitch = 7; CHECK (store::save (disk, *E, *U, 0), "save again (nothing recorded)");
          size_t changed = 0; for (size_t i = 0; i < disk.d.size (); i += 512) changed += std::memcmp (&disk.d[i], &before.d[i], 512) != 0;
          CHECK (changed < 40, "an unchanged project rewrites only the project file: %zu sectors", changed); }
        // overwrite and space: a save again with fewer sounds deletes the old WAVs
        E->clearSlot (1); CHECK (store::save (disk, *E, *U, 0), "save again");
        uint32_t got; uint8_t tmp[64]; CHECK (! store::readFile (disk, "P01A02.WAV", tmp, 64, &got), "the cleared pad's WAV is gone");
    }

    v2Tests (dir);

    // ---- a demo: a beat with effects, written for listening
    reset ();
    {
        std::vector<int16_t> kick (9000), snare (7000), hat (2000), bass (SR / 2);
        for (int i = 0; i < 9000; ++i) { const double t = i / (double) SR; kick[i] = (int16_t) (26000 * std::sin (2 * M_PI * (45 * t + 90 * (1 - std::exp (-t * 30)) / 30)) * std::exp (-t * 9)); }
        for (int i = 0; i < 7000; ++i) { const double t = i / (double) SR; snare[i] = (int16_t) ((14000 * ((E->rng () & 65535) / 32768.0 - 1) + 9000 * std::sin (2 * M_PI * 190 * t)) * std::exp (-t * 18)); }
        for (int i = 0; i < 2000; ++i) { const double t = i / (double) SR; hat[i] = (int16_t) (9000 * ((E->rng () & 65535) / 32768.0 - 1) * std::exp (-t * 60)); }
        for (int i = 0; i < SR / 2; ++i) { const double t = i / (double) SR; bass[i] = (int16_t) (15000 * std::tanh (3 * std::sin (2 * M_PI * 55 * t)) * std::exp (-t * 4)); }
        E->assign (0, kick.data (), 9000, SR); E->assign (1, snare.data (), 7000, SR); E->assign (2, hat.data (), 2000, SR); E->assign (3, bass.data (), SR / 2, SR);
        E->delayAmt = 0.1f; E->reverbAmt = 0.15f; E->master = 0.9f; E->bpm = 124; E->swing = 0.12f;
        E->euclid (0, 4, 0); E->toggleStep (4, 1); E->toggleStep (12, 1); E->euclid (2, 11, 1);
        for (int s : {0, 3, 6, 10, 14}) { E->toggleStep (s, 3); E->trig (0, s, 3)->pitch = (int8_t) (s % 7 == 0 ? 0 : 3 + (s % 5)); }
        E->slots[3].filter = -0.35f; E->slots[1].reverb = 0.4f;
        E->pats[1] = E->pats[0]; E->pat = 1; E->chaos (0.6f); E->pat = 0;
        uint8_t ch[2] = {0, 1}; E->setChain (ch, 2);
        E->play (true);
        std::vector<float> all;
        auto add = [&] (float secs) { auto x = run (secs); all.insert (all.end (), x.begin (), x.end ()); };
        add (7.74f); E->fx = 3; add (0.97f); E->fx = 0; add (2.9f); E->fx = 10; add (1.9f); E->fx = 6; add (0.97f); E->fx = 0; add (1.0f);
        E->play (false); add (1.0f);
        writeWav (dir + "/pocket-demo.wav", all);
        float peak = 0; for (float x : all) peak = std::max (peak, std::fabs (x));
        CHECK (peak > 0.3f && peak <= 1.0f, "the demo is loud and does not clip: %.2f", peak);
    }

    std::printf ("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}

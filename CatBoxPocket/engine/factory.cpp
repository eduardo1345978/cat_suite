// Cat Box Pocket: the factory sounds and the demo project. Everything is synthesized here (no samples to
// ship), so the Pico makes them on its first start and the simulator makes them on load.
//   bank A  drums           A01 808 kick .. A16 snap
//   bank B  percussion, FX   B01 conga .. B16 glitch
//   bank C  melodic, at C    C01 sub bass .. C16 kalimba (keys mode plays them in tune)
//   bank D  loops, textures  D01 break .. D08 rain; D09..D16 are left free for recording
#include "pocket.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace pocket {
namespace factory {

static constexpr float PI = 3.14159265358979f;
static uint32_t rs = 0x9E3779B9u;
static inline float noise () { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (float) (rs & 0xFFFFFF) / 8388608.0f - 1.0f; }
static inline float clampf (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline int16_t s16 (float x) { return (int16_t) clampf (x * 32767.0f + (x >= 0 ? 0.5f : -0.5f), -32767.0f, 32767.0f); }
static inline float semis (float s) { return std::pow (2.0f, s / 12.0f); }

// a state-variable filter (TPT), cutoff changeable per sample
struct Svf {
    float ic1 = 0, ic2 = 0, g = 0.1f, k = 1, a1 = 0, a2 = 0, a3 = 0, lp = 0, bp = 0, hp = 0; float R = SR;
    explicit Svf (float rate = SR) : R (rate) {}
    void set (float fc, float q) { g = std::tan (PI * clampf (fc, 10.0f, R * 0.45f) / R); k = 1.0f / q; a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2; }
    float run (float x) {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; lp = v2; bp = v1; hp = x - k * v1 - v2; return v2;
    }
};
// band-limited saw (polyBLEP)
struct Saw {
    float ph = 0;
    float run (float inc) {
        ph += inc; if (ph >= 1) ph -= 1;
        float y = 2 * ph - 1, t = ph;
        if (t < inc) { t /= inc; y -= t + t - t * t - 1; }
        else if (t > 1 - inc) { t = (t - 1) / inc; y -= t * t + t + t + 1; }
        return y;
    }
};

// the sound being made: its memory in the pool
struct Snd { int16_t* d = nullptr; uint32_t n = 0; float R = SR; int slot = -1; };
static Snd make (Engine& e, int slot, float secs, uint32_t rate = SR) {
    Snd s; e.clearSlot (slot);
    const uint32_t n = (uint32_t) (secs * rate);
    const int b = e.newBlock (n);
    if (b < 0) return s;
    Slot& sl = e.slots[slot]; sl = Slot (); sl.block = (int16_t) b; sl.start = 0; sl.len = n; sl.rate = rate;
    s.d = e.mem + e.blocks[b].off; s.n = n; s.R = (float) rate; s.slot = slot;
    std::memset (s.d, 0, n * sizeof (int16_t));
    return s;
}
// finish: a 1 ms fade-in, a fade-out over the last 3 ms, then the peak to -1 dB (or `peak`)
static void finish (Snd& s, float peak = 0.89f, bool loop = false) {
    if (! s.d) return;
    const uint32_t fi = (uint32_t) (s.R * 0.001f), fo = (uint32_t) (s.R * 0.003f);
    if (! loop) {
        for (uint32_t i = 0; i < fi && i < s.n; ++i) s.d[i] = (int16_t) (s.d[i] * (i / (float) fi));
        for (uint32_t i = 0; i < fo && i < s.n; ++i) s.d[s.n - 1 - i] = (int16_t) (s.d[s.n - 1 - i] * (i / (float) fo));
    }
    int m = 1; for (uint32_t i = 0; i < s.n; ++i) m = std::max (m, std::abs ((int) s.d[i]));
    const float g = peak * 32767.0f / m;
    for (uint32_t i = 0; i < s.n; ++i) s.d[i] = s16 (s.d[i] * g / 32767.0f);
}
template <typename F> static void fill (Snd& s, F f) { if (! s.d) return; for (uint32_t i = 0; i < s.n; ++i) s.d[i] = s16 (f (i / s.R, i) * 0.5f); }
// mixes a finished sound into another (for the loops), at a time and level
static void mixIn (Snd& to, const Engine& e, int slot, float at, float vel, int semi = 0) {
    if (! to.d || e.slots[slot].empty ()) return;
    const int16_t* src; const uint32_t n = e.slotData (slot, &src);
    const float rate = (float) e.slots[slot].rate, inc = rate / to.R * semis ((float) semi);
    uint32_t o = (uint32_t) (at * to.R);
    for (float p = 0; p < n - 1 && o < to.n; p += inc, ++o) {
        const int i = (int) p; const float fr = p - i;
        const float x = (src[i] + (src[i + 1] - src[i]) * fr) * vel * 0.5f;
        to.d[o] = (int16_t) clampf (to.d[o] + x, -32767.0f, 32767.0f);
    }
}

// ------------------------------------------------------------------ the voices of the synths used below
static float metal (float t, float mul) {       // six square oscillators, the 808 cymbal recipe
    static const float F[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};
    float x = 0; for (float f : F) x += std::sin (2 * PI * f * mul * t) > 0 ? 1.0f : -1.0f;
    return x / 6;
}
static void kick (Snd s, float f0, float f1, float bend, float decay, float drive, float click) {
    if (! s.d) return;
    float ph = 0;
    for (uint32_t i = 0; i < s.n; ++i) {
        const float t = i / s.R, f = f1 + (f0 - f1) * std::exp (-t * bend);
        ph += f / s.R; const float body = std::sin (2 * PI * ph) * std::exp (-t / decay);
        const float c = click * noise () * std::exp (-t * 400);
        s.d[i] = s16 (std::tanh ((body + c) * drive) * 0.5f);
    }
}
static void tom (Snd s, float f0, float f1, float decay) {
    if (! s.d) return;
    float ph = 0; Svf n (s.R); n.set (f0 * 4, 1.5f);
    for (uint32_t i = 0; i < s.n; ++i) {
        const float t = i / s.R; ph += (f1 + (f0 - f1) * std::exp (-t * 18)) / s.R;
        s.d[i] = s16 ((std::sin (2 * PI * ph) * std::exp (-t / decay) + 0.15f * n.run (noise ()) * std::exp (-t * 60)) * 0.5f);
    }
}
static void fm (Snd s, float fc, float ratio, float index, float idxDecay, float decay, float attack = 0.002f) {
    if (! s.d) return;
    float pc = 0, pm = 0;
    for (uint32_t i = 0; i < s.n; ++i) {
        const float t = i / s.R;
        pm += fc * ratio / s.R; if (pm > 1) pm -= 1;
        const float mod = index * std::exp (-t * idxDecay) * std::sin (2 * PI * pm);
        pc += fc / s.R; if (pc > 1) pc -= 1;
        const float env = std::min (1.0f, t / attack) * std::exp (-t / decay);
        s.d[i] = s16 (std::sin (2 * PI * pc + mod) * env * 0.5f);
    }
}
// a chord of detuned saws through a low-pass (pads, stabs); notes in Hz
static void sawChord (Snd s, const float* hz, int nn, int voices, float detune, float cutoff, float attack, float decay, float release, float sweep = 0) {
    if (! s.d) return;
    Saw osc[40]; for (int k = 0; k < nn * voices && k < 40; ++k) osc[k].ph = (k * 0.618f) - (int) (k * 0.618f);
    Svf l (s.R), r2 (s.R);
    const float relAt = (s.n / s.R) - release;
    for (uint32_t i = 0; i < s.n; ++i) {
        const float t = i / s.R;
        if ((i & 15) == 0) { const float c = cutoff * (1 + sweep * std::sin (2 * PI * 0.15f * t)); l.set (c, 0.9f); r2.set (c, 0.9f); }
        float x = 0;
        for (int a = 0; a < nn; ++a) for (int v = 0; v < voices; ++v) {
            const float dt = voices > 1 ? (v - (voices - 1) * 0.5f) / ((voices - 1) * 0.5f) : 0;
            x += osc[a * voices + v].run (hz[a] * (1 + detune * dt) / s.R);
        }
        x /= (float) (nn * voices);
        float env = std::min (1.0f, t / std::max (0.001f, attack)) * (decay > 0 ? std::exp (-t / decay) : 1.0f);
        if (t > relAt) env *= std::max (0.0f, 1 - (t - relAt) / release);
        s.d[i] = s16 (r2.run (l.run (x)) * env * 0.9f);
    }
}
static void pluck (Snd s, float hz, float damp) {          // Karplus-Strong
    if (! s.d) return;
    static float line[2048]; const int L = std::min (2047, (int) (s.R / hz));
    for (int i = 0; i < L; ++i) line[i] = noise ();
    int p = 0; float last = 0;
    for (uint32_t i = 0; i < s.n; ++i) {
        const float y = line[p]; const int q = (p + 1) % L;
        const float nx = damp * 0.5f * (line[p] + line[q]);
        line[p] = nx; p = q;
        last += (y - last) * 0.7f;
        s.d[i] = s16 (last * 0.6f);
    }
}
static void acid (Snd& s, float at, float hz, float len, float accent) {   // one TB-ish note mixed into s
    if (! s.d) return;
    Saw o; Svf f (s.R); const uint32_t a = (uint32_t) (at * s.R), n = (uint32_t) (len * s.R);
    for (uint32_t i = 0; i < n && a + i < s.n; ++i) {
        const float t = i / s.R;
        if ((i & 7) == 0) f.set (300 + (2200 + 2500 * accent) * std::exp (-t * 14), 6 + 6 * accent);
        float x = std::tanh (f.run (o.run (hz / s.R)) * 1.6f) * std::min (1.0f, (len - t) * 80) * 0.55f;
        s.d[a + i] = (int16_t) clampf (s.d[a + i] + x * 16000, -32767.0f, 32767.0f);
    }
}

// ------------------------------------------------------------------ the 56 sounds
static const float C2 = 65.406f, C3 = 130.813f, C4 = 261.626f, C5 = 523.251f;

void sounds (Engine& e) {
    rs = 0x9E3779B9u;
    // ---- bank A: drums
    { Snd s = make (e, 0, 1.2f); kick (s, 120, 47, 22, 0.42f, 1.4f, 0.15f); finish (s); }                  // 808 kick
    { Snd s = make (e, 1, 0.45f); kick (s, 170, 52, 40, 0.13f, 2.6f, 0.35f); finish (s); }                 // punchy kick
    { Snd s = make (e, 2, 0.6f); kick (s, 210, 54, 30, 0.2f, 2.0f, 0.5f); finish (s); }                    // 909 kick
    { Snd s = make (e, 3, 0.35f); Svf bp (s.R); bp.set (2600, 0.8f);                                        // snare
      fill (s, [&] (float t, uint32_t) { return (0.55f * (std::sin (2 * PI * 185 * t) + 0.6f * std::sin (2 * PI * 330 * t)) * std::exp (-t * 28)
                                                 + 0.9f * bp.run (noise ()) * std::exp (-t * 14)); }); finish (s); }
    { Snd s = make (e, 4, 0.3f); Svf hp (s.R); hp.set (1800, 0.7f);                                         // crisp snare
      fill (s, [&] (float t, uint32_t) { hp.run (noise ()); return std::tanh (1.6f * (hp.hp * std::exp (-t * 18) + 0.7f * std::sin (2 * PI * 220 * t) * std::exp (-t * 40))); }); finish (s); }
    { Snd s = make (e, 5, 0.09f); Svf bp (s.R); bp.set (1900, 4);                                           // rim
      fill (s, [&] (float t, uint32_t) { return std::sin (2 * PI * 1700 * t) * std::exp (-t * 60) + 0.5f * bp.run (noise ()) * std::exp (-t * 120); }); finish (s); }
    { Snd s = make (e, 6, 0.42f); Svf bp (s.R); bp.set (1150, 1.6f);                                        // clap
      fill (s, [&] (float t, uint32_t) { float env = 0.4f * std::exp (-t * 14); for (int k = 0; k < 4; ++k) { const float tt = t - k * 0.0105f; if (tt >= 0) env += std::exp (-tt * 170); }
                                         return bp.run (noise ()) * env; }); finish (s); }
    { Snd s = make (e, 7, 0.14f); Svf hp (s.R); hp.set (7200, 0.8f);                                        // closed hat
      fill (s, [&] (float t, uint32_t) { hp.run (metal (t, 1.0f) + 0.3f * noise ()); return hp.hp * std::exp (-t * 55); }); finish (s, 0.7f); }
    { Snd s = make (e, 8, 0.75f); Svf hp (s.R); hp.set (6800, 0.8f);                                        // open hat
      fill (s, [&] (float t, uint32_t) { hp.run (metal (t, 1.0f) + 0.3f * noise ()); return hp.hp * std::exp (-t * 6.5f); }); finish (s, 0.7f); }
    { Snd s = make (e, 9, 1.8f); Svf hp (s.R); hp.set (4800, 0.7f);                                         // crash
      fill (s, [&] (float t, uint32_t) { hp.run (metal (t, 1.43f) + 0.6f * noise ()); return hp.hp * std::exp (-t * 2.2f) * std::min (1.0f, t * 400); }); finish (s, 0.75f); }
    { Snd s = make (e, 10, 0.6f); tom (s, 125, 82, 0.28f); finish (s); }
    { Snd s = make (e, 11, 0.5f); tom (s, 170, 118, 0.24f); finish (s); }
    { Snd s = make (e, 12, 0.45f); tom (s, 240, 170, 0.2f); finish (s); }
    { Snd s = make (e, 13, 0.4f); Svf bp (s.R); bp.set (850, 2.2f);                                         // cowbell
      fill (s, [&] (float t, uint32_t) { const float x = (std::sin (2 * PI * 540 * t) > 0 ? 1 : -1) + (std::sin (2 * PI * 800 * t) > 0 ? 1 : -1);
                                         return bp.run (x) * (0.6f * std::exp (-t * 30) + 0.4f * std::exp (-t * 7)); }); finish (s, 0.8f); }
    { Snd s = make (e, 14, 0.16f); Svf hp (s.R); hp.set (5000, 0.8f);                                       // shaker
      fill (s, [&] (float t, uint32_t) { hp.run (noise ()); return hp.hp * std::min (1.0f, t * 90) * std::exp (-t * 35); }); finish (s, 0.6f); }
    { Snd s = make (e, 15, 0.15f); Svf bp (s.R); bp.set (2400, 2);                                          // snap
      fill (s, [&] (float t, uint32_t) { return bp.run (noise ()) * std::exp (-t * 70) + 0.4f * std::sin (2 * PI * 1250 * t) * std::exp (-t * 90); }); finish (s, 0.8f); }

    // ---- bank B: percussion and effects
    auto hand = [&] (int slot, float f0, float decay, float slap) {
        Snd s = make (e, slot, decay * 5); Svf bp (s.R); bp.set (f0 * 6, 1.2f); float ph = 0;
        fill (s, [&] (float t, uint32_t) { ph += f0 * (1 + 0.08f * std::exp (-t * 60)) / s.R; return std::sin (2 * PI * ph) * std::exp (-t / decay) + slap * bp.run (noise ()) * std::exp (-t * 140); });
        finish (s);
    };
    hand (16, 330, 0.06f, 0.5f); hand (17, 210, 0.08f, 0.5f); hand (18, 440, 0.04f, 0.7f);
    { Snd s = make (e, 19, 0.12f); fill (s, [&] (float t, uint32_t) { return std::sin (2 * PI * 2500 * t) * std::exp (-t * 45); }); finish (s, 0.8f); }                 // clave
    { Snd s = make (e, 20, 0.12f); fill (s, [&] (float t, uint32_t) { return (std::sin (2 * PI * 1050 * t) + 0.5f * std::sin (2 * PI * 1620 * t)) * std::exp (-t * 55); }); finish (s, 0.8f); }  // woodblock
    { Snd s = make (e, 21, 0.35f); Svf hp (s.R); hp.set (6500, 0.8f);                                       // tambourine
      fill (s, [&] (float t, uint32_t) { hp.run (metal (t, 2.1f) + noise ()); return hp.hp * (0.5f + 0.5f * std::sin (2 * PI * 38 * t)) * std::exp (-t * 11); }); finish (s, 0.7f); }
    { Snd s = make (e, 22, 1.4f); fill (s, [&] (float t, uint32_t) { return (std::sin (2 * PI * 3150 * t) + 0.4f * std::sin (2 * PI * 7480 * t) * std::exp (-t * 3)) * std::exp (-t * 2.4f); }); finish (s, 0.6f); }  // triangle
    { Snd s = make (e, 23, 0.18f); float ph = 0; fill (s, [&] (float t, uint32_t) { ph += (90 + 3200 * std::exp (-t * 30)) / s.R; return std::sin (2 * PI * ph) * std::exp (-t * 14); }); finish (s); }   // zap
    { Snd s = make (e, 24, 0.35f); float ph = 0; fill (s, [&] (float t, uint32_t) { ph += (300 + 1300 * std::exp (-t * 9)) * (1 + 0.03f * std::sin (2 * PI * 30 * t)) / s.R;
                                                                                     return (ph - (int) ph < 0.5f ? 0.6f : -0.6f) * std::exp (-t * 7); }); finish (s, 0.7f); }   // laser
    { Snd s = make (e, 25, 2.0f, 24000); Svf bp (s.R);                                                     // riser
      fill (s, [&] (float t, uint32_t i) { if ((i & 15) == 0) bp.set (300 * std::pow (26.0f, t / 2), 3); return bp.run (noise ()) * (t / 2) * (t / 2) * 2; }); finish (s); }
    { Snd s = make (e, 26, 2.0f, 24000); Svf lp (s.R);                                                     // downlifter
      fill (s, [&] (float t, uint32_t i) { if ((i & 15) == 0) lp.set (9000 * std::pow (0.025f, t / 2), 2); return lp.run (noise ()) * std::exp (-t * 1.2f); }); finish (s); }
    { Snd s = make (e, 27, 1.6f, 24000); Svf lp (s.R); lp.set (380, 0.8f); float ph = 0;                   // impact
      fill (s, [&] (float t, uint32_t) { ph += (40 + 50 * std::exp (-t * 6)) / s.R; return std::tanh (1.8f * (std::sin (2 * PI * ph) * std::exp (-t * 1.6f) + 1.2f * lp.run (noise ()) * std::exp (-t * 4))); }); finish (s); }
    { Snd s = make (e, 28, 1.5f); Svf hp (s.R); hp.set (4800, 0.7f);                                       // reverse cymbal
      fill (s, [&] (float t, uint32_t) { const float u = 1.5f - t; hp.run (metal (u, 1.43f) + 0.6f * noise ()); return hp.hp * std::exp (-u * 2.6f); }); finish (s, 0.75f); }
    { Snd s = make (e, 29, 0.08f); fill (s, [&] (float t, uint32_t) { return (std::sin (2 * PI * (880 + 880 * t / 0.08f) * t) > 0 ? 0.5f : -0.5f) * std::exp (-t * 30); }); finish (s, 0.6f); }  // blip
    { Snd s = make (e, 30, 2.0f, 24000); Svf lp (s.R); lp.set (3500, 0.7f);                                // vinyl crackle (a loop)
      fill (s, [&] (float, uint32_t) { float x = 0.04f * lp.run (noise ()); if ((rs & 1023) < 3) x += noise () * 0.9f; return x; }); finish (s, 0.5f, true); e.slots[30].loopHold = true; }
    { Snd s = make (e, 31, 0.4f); float held = 0;                                                           // glitch
      fill (s, [&] (float t, uint32_t i) { if (i % 9 == 0) held = std::round ((std::sin (2 * PI * 440 * t) + std::sin (2 * PI * 660 * t) + std::sin (2 * PI * 1100 * t)) * 4) / 12;
                                           return held * ((int) (t * 40) % 3 ? 1.0f : 0.0f) * std::exp (-t * 4); }); finish (s, 0.7f); }

    // ---- bank C: melodic, all at C (C2 bass, C3 pads, C4 leads)
    { Snd s = make (e, 32, 1.2f, 24000); fill (s, [&] (float t, uint32_t) { return (std::sin (2 * PI * C2 * t) + 0.12f * std::sin (4 * PI * C2 * t)) * std::exp (-t * 1.8f); }); finish (s); }   // sub bass
    { Snd s = make (e, 33, 1.6f, 24000); const float f[1] = {C2}; sawChord (s, f, 1, 2, 0.006f, 420, 0.005f, 0, 0.1f, 0.3f); finish (s); e.slots[33].loopHold = true; }             // reese
    { Snd s = make (e, 34, 0.5f); acid (s, 0, C2, 0.48f, 0.8f); finish (s); }                                                                                                      // acid
    { Snd s = make (e, 35, 1.5f, 24000); float ph = 0;                                                                                                                              // 808 bass
      fill (s, [&] (float t, uint32_t) { ph += C2 * (1 + 0.5f * std::exp (-t * 40)) / s.R; return std::tanh (1.6f * std::sin (2 * PI * ph)) * std::exp (-t * 1.4f); }); finish (s); }
    { Snd s = make (e, 36, 1.2f); pluck (s, C4, 0.996f); finish (s); }                                                                                                             // pluck
    { Snd s = make (e, 37, 1.6f); fm (s, C4, 1.0f, 2.2f, 4, 0.7f); Snd b = s;                                                                                                      // e-piano (+ a bell partial)
      for (uint32_t i = 0; i < b.n; ++i) { const float t = i / b.R; b.d[i] = s16 (b.d[i] / 32767.0f + 0.08f * std::sin (2 * PI * C4 * 14 * t) * std::exp (-t * 30)); } finish (s); }
    { Snd s = make (e, 38, 1.2f, 24000); fill (s, [&] (float t, uint32_t) { float x = 0; const float h[6] = {1, 2, 3, 4, 6, 8}, a[6] = {1, 0.8f, 0.6f, 0.5f, 0.3f, 0.25f};
                                                                         for (int k = 0; k < 6; ++k) x += a[k] * std::sin (2 * PI * C3 * h[k] * t); return x * 0.3f * (1 + 0.04f * std::sin (2 * PI * 6 * t)); });
      finish (s, 0.89f, true); e.slots[38].loopHold = true; }                                                                                                                        // organ
    { Snd s = make (e, 39, 2.4f, 24000); const float f[2] = {C3, C3 * 1.5f}; sawChord (s, f, 2, 5, 0.008f, 1600, 0.35f, 0, 0.6f, 0.4f); finish (s); e.slots[39].loopHold = true; }  // pad
    { Snd s = make (e, 40, 1.8f); fm (s, C5, 3.5f, 3.5f, 3, 0.7f); finish (s, 0.8f); }                                                                                             // bell
    { Snd s = make (e, 41, 0.8f); fill (s, [&] (float t, uint32_t) { return std::sin (2 * PI * C5 * t) * std::exp (-t * 6) + 0.35f * std::sin (2 * PI * C5 * 4 * t) * std::exp (-t * 30); }); finish (s); }  // marimba
    { Snd s = make (e, 42, 1.0f); Saw a, b; Svf lp (s.R); lp.set (3800, 0.8f);                                                                                                     // lead
      fill (s, [&] (float t, uint32_t) { const float v = 1 + 0.004f * std::sin (2 * PI * 5.5f * t) * std::min (1.0f, t * 3); return lp.run (a.run (C4 * v / s.R) - b.run (C4 * v * 1.003f / s.R) * 0.8f) * std::min (1.0f, t * 200); });
      finish (s); e.slots[42].loopHold = true; }
    { Snd s = make (e, 43, 0.8f); const float f[1] = {C4}; sawChord (s, f, 1, 3, 0.005f, 2600, 0.02f, 0.5f, 0.15f); finish (s); }                                               // brass stab
    { Snd s = make (e, 44, 0.9f); const float f[4] = {C3, C3 * semis (3), C3 * semis (7), C3 * semis (10)}; sawChord (s, f, 4, 2, 0.004f, 2200, 0.004f, 0.35f, 0.1f); finish (s); }  // Cm7 stab
    { Snd s = make (e, 45, 1.1f); const float f[5] = {C3, C3 * semis (4), C3 * semis (7), C3 * semis (11), C3 * semis (14)}; sawChord (s, f, 5, 2, 0.004f, 1900, 0.01f, 0.5f, 0.15f); finish (s); }  // Cmaj9 stab
    { Snd s = make (e, 46, 1.4f, 24000); Saw o; Svf f1 (s.R), f2 (s.R), f3 (s.R); f1.set (700, 6); f2.set (1220, 7); f3.set (2600, 8);                                             // vox "aah"
      fill (s, [&] (float t, uint32_t) { const float x = o.run (C3 * (1 + 0.006f * std::sin (2 * PI * 5 * t)) / s.R); f1.run (x); f2.run (x); f3.run (x);
                                         return (f1.bp + 0.7f * f2.bp + 0.3f * f3.bp) * std::min (1.0f, t * 8); });
      finish (s); Slot& sl = e.slots[46]; sl.freeze = true; sl.grainPos = 0.4f; sl.grainSize = 0.12f; sl.grainSpread = 0.05f; sl.grainSpeed = 0.0f; }
    { Snd s = make (e, 47, 1.0f); fill (s, [&] (float t, uint32_t) { return std::sin (2 * PI * C5 * t) * std::exp (-t * 4) + 0.3f * std::sin (2 * PI * C5 * 5.4f * t) * std::exp (-t * 25); }); finish (s); }  // kalimba

    // ---- bank D: loops at 120 BPM (time-stretched to the tempo) and textures
    const float st = 60.0f / 120 / 4;    // a 16th
    { Snd s = make (e, 48, 32 * st);     // break: 2 bars, swung, with ghost notes
      const int K[] = {0, 10, 16, 23, 26}, S[] = {4, 12, 20, 28}, G[] = {7, 9, 15, 25, 31};
      for (int k : K) mixIn (s, e, 1, k * st, 1.0f);
      for (int k : S) mixIn (s, e, 3, k * st, 0.9f);
      for (int k : G) mixIn (s, e, 3, k * st + 0.01f, 0.25f);
      for (int k = 0; k < 32; k += 2) mixIn (s, e, 7, k * st + (k % 4 ? 0.012f : 0), k % 4 ? 0.45f : 0.7f);
      mixIn (s, e, 8, 30 * st, 0.5f);
      finish (s, 0.89f, true); e.slots[48].stretch = true; }
    { Snd s = make (e, 49, 16 * st);     // house: 1 bar
      for (int k = 0; k < 16; k += 4) mixIn (s, e, 2, k * st, 1.0f);
      for (int k : {4, 12}) mixIn (s, e, 6, k * st, 0.8f);
      for (int k = 2; k < 16; k += 4) mixIn (s, e, 8, k * st, 0.55f);
      for (int k = 0; k < 16; ++k) mixIn (s, e, 14, k * st, k % 2 ? 0.35f : 0.2f);
      finish (s, 0.89f, true); e.slots[49].stretch = true; }
    { Snd s = make (e, 50, 16 * st);     // percussion: 1 bar
      const int clave[] = {0, 3, 6, 10, 12};
      for (int k : clave) mixIn (s, e, 19, k * st, 0.6f);
      const int conga[] = {2, 5, 8, 11, 14}; int c = 0;
      for (int k : conga) mixIn (s, e, (c++ % 2) ? 17 : 16, k * st, 0.8f);
      for (int k = 0; k < 16; ++k) mixIn (s, e, 14, k * st, 0.3f);
      finish (s, 0.89f, true); e.slots[50].stretch = true; }
    { Snd s = make (e, 51, 16 * st);     // acid bass line: 1 bar in C minor
      const int notes[][3] = {{0, 0, 1}, {3, 0, 0}, {4, 12, 1}, {6, 0, 0}, {8, 3, 1}, {10, 0, 0}, {11, 7, 0}, {14, 10, 1}};
      for (auto& nn : notes) acid (s, nn[0] * st, C2 * semis ((float) nn[1]), st * 1.6f, (float) nn[2]);
      finish (s, 0.89f, true); e.slots[51].stretch = true; }
    { Snd s = make (e, 52, 32 * st, 24000);   // chords: Cm7, Abmaj7, Ebmaj7, Bb (2 bars)
      const float ch[4][4] = {{0, 3, 7, 10}, {-4, 0, 3, 7}, {3, 7, 10, 14}, {-2, 2, 5, 10}};
      for (int c = 0; c < 4; ++c) {
        Snd part = s; part.d = s.d + (uint32_t) (c * 8 * st * s.R); part.n = (uint32_t) (8 * st * s.R);
        float f[4]; for (int k = 0; k < 4; ++k) f[k] = C3 * semis (ch[c][k]);
        sawChord (part, f, 4, 3, 0.006f, 1500, 0.08f, 0, 0.08f);
      }
      finish (s, 0.89f, true); e.slots[52].stretch = true; }
    { Snd s = make (e, 53, 16 * st);     // arpeggio: Cm, 16ths
      const int arp[] = {0, 3, 7, 12, 15, 12, 7, 3, 0, 3, 7, 10, 15, 10, 7, 3};
      for (int k = 0; k < 16; ++k) {
        Snd n = s; n.d = s.d + (uint32_t) (k * st * s.R); n.n = (uint32_t) (st * s.R * 0.95f);
        static int16_t tmp[24000]; Snd p; p.d = tmp; p.n = n.n; p.R = s.R; pluck (p, C4 * semis ((float) arp[k]), 0.993f);
        for (uint32_t i = 0; i < n.n; ++i) n.d[i] = tmp[i];
      }
      finish (s, 0.89f, true); e.slots[53].stretch = true; }
    { Snd s = make (e, 54, 4.0f, 24000); const float f[3] = {C3, C3 * semis (7), C3 * semis (15)};       // texture: a slow evolving pad
      sawChord (s, f, 3, 4, 0.01f, 900, 0.9f, 0, 1.0f, 0.9f);
      Svf lp (s.R); lp.set (1200, 0.7f); for (uint32_t i = 0; i < s.n; ++i) s.d[i] = s16 (s.d[i] / 32767.0f + 0.05f * lp.run (noise ()));
      finish (s); Slot& sl = e.slots[54]; sl.drone = true; sl.grainSize = 0.22f; sl.grainSpread = 0.3f; sl.grainSpeed = 0.1f; sl.glide = 0.25f; }
    { Snd s = make (e, 55, 3.0f, 24000); Svf lp (s.R); lp.set (2500, 0.6f);                                // rain
      fill (s, [&] (float, uint32_t) { float x = 0.25f * lp.run (noise ()); if ((rs & 2047) < 2) x += 0.8f * noise (); return x; });
      finish (s, 0.6f, true); e.slots[55].loopHold = true; }
    for (auto& sl : e.slots) sl.saved = false;
}

// ------------------------------------------------------------------ the demo project: 4 patterns and a song
static void hit (Engine& e, int st, int slot, int pitch = 0, int vel = 100, int cond = C_ALWAYS) {
    if (! e.hasTrig (e.pat, st, slot)) e.toggleStep (st, slot);
    Trig* t = e.trig (e.pat, st, slot); if (! t) return;
    t->pitch = pitch ? (int8_t) pitch : NOLOCK; t->vel = (uint8_t) vel; t->cond = (uint8_t) cond;
}

void project (Engine& e) {
    e.bpm = 120; e.swing = 0.06f; e.delayAmt = 0.12f; e.reverbAmt = 0.18f; e.master = 0.78f; e.duckAmt = 0.25f; e.compAmt = 0.2f;
    for (int s : {0, 1, 2}) e.slots[s].duckSource = true;
    e.slots[7].choke = e.slots[8].choke = 1;
    e.slots[3].reverb = 0.25f; e.slots[6].reverb = 0.3f; e.slots[44].delay = 0.35f; e.slots[40].delay = 0.4f; e.slots[40].reverb = 0.4f;
    e.slots[36].delay = 0.25f; e.slots[39].reverb = 0.5f; e.slots[8].volume = 0.6f; e.slots[7].volume = 0.65f; e.slots[14].volume = 0.5f;
    e.slots[34].volume = 0.75f; e.slots[39].volume = 0.55f; e.slots[44].volume = 0.6f; e.slots[52].volume = 0.6f;
    for (int p = 0; p < 4; ++p) e.clearPattern (p);
    // P1: house
    e.pat = 0;
    for (int s = 0; s < 16; s += 4) hit (e, s, 2);
    hit (e, 4, 6, 0, 90); hit (e, 12, 6, 0, 90);
    for (int s = 2; s < 16; s += 4) hit (e, s, 8, 0, 80);
    for (int s = 0; s < 16; ++s) hit (e, s, 14, 0, s % 2 ? 70 : 45);
    { const int b[][2] = {{0, 0}, {3, 0}, {6, 12}, {8, 0}, {10, 3}, {11, 0}, {14, 7}}; for (auto& x : b) hit (e, x[0], 34, x[1], x[0] % 4 == 2 ? 120 : 90); }
    hit (e, 0, 44, 0, 90); hit (e, 10, 44, 0, 70, C_1_2);
    // P2: P1 with fills and a crash on the first time
    e.pats[1] = e.pats[0]; e.pat = 1;
    hit (e, 0, 9, 0, 80, C_FIRST);
    hit (e, 12, 10, 0, 100, C_2_2); hit (e, 13, 11, 0, 100, C_2_2); hit (e, 14, 12, 0, 100, C_2_2); hit (e, 15, 12, 2, 110, C_2_2);
    for (int s = 12; s < 16; ++s) hit (e, s, 3, 0, 60 + (s - 12) * 15, C_FILL);
    hit (e, 6, 44, 5, 70, C_P50);
    // P3: breakbeat with a reese bass
    e.pat = 2;
    for (int s : {0, 10}) hit (e, s, 1); hit (e, 7, 1, 0, 60, C_P50);
    hit (e, 4, 3); hit (e, 12, 3);
    for (int s : {7, 9, 15}) hit (e, s, 3, 0, 35);
    for (int s = 0; s < 16; s += 2) hit (e, s, 7, 0, s % 4 ? 55 : 85);
    hit (e, 14, 8, 0, 60);
    hit (e, 0, 33); hit (e, 8, 33, -2); hit (e, 12, 33, 3);
    hit (e, 0, 9, 0, 70, C_1_4);
    hit (e, 0, 30, 0, 60);
    // P4: melodic (pluck arpeggio over the pad)
    e.pat = 3;
    { const int arp[] = {0, 3, 7, 10, 12, 10, 7, 3}; for (int k = 0; k < 8; ++k) hit (e, k * 2, 36, arp[k], k % 2 ? 80 : 105); }
    hit (e, 0, 39); hit (e, 8, 39, -4);
    hit (e, 0, 0); hit (e, 8, 0, 0, 90);
    hit (e, 4, 6, 0, 80); hit (e, 12, 6, 0, 80);
    for (int s = 2; s < 16; s += 4) hit (e, s, 7, 0, 60);
    hit (e, 6, 40, 7, 70); hit (e, 14, 40, 12, 60, C_1_2);
    e.pat = 0;
    // the song: P1 x2, P2, P3 x2, P4 x2, P2
    e.songLen = 0;
    const int song[][2] = {{0, 2}, {1, 1}, {2, 2}, {3, 2}, {1, 1}};
    for (auto& r : song) { e.songAdd (r[0]); e.song[e.songLen - 1].repeats = (uint8_t) r[1]; }
    e.songMode = false;
    uint8_t ch[1] = {0}; e.setChain (ch, 1);
    // scene B: everything darker and wetter (the crossfader blends to it)
    e.sceneClear ();
    e.sceneStore (1);
    for (int s = 0; s < NSLOT; ++s) e.scenes[1].filter[s] = std::min (e.slots[s].filter, -0.45f);
    e.scenes[1].delay = 0.45f; e.scenes[1].reverb = 0.55f;
}

} // namespace factory
} // namespace pocket

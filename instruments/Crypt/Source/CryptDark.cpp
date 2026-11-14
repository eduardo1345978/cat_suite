#include "CryptDark.h"

namespace tomb
{
namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;
inline float rnd (uint32_t& s) noexcept { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 8388608.0f - 1.0f; }

/** One step of a TPT state-variable filter; returns { low, band }. */
inline std::pair<float, float> svf (float x, float g, float k, float& ic1, float& ic2) noexcept
{
    const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const float v3 = x - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    return { v2, v1 };
}
inline float gFor (double hz, double sr) noexcept { return (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (20.0, sr * 0.45, hz) / sr); }
}

juce::StringArray droneNoteNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }

//==============================================================================
void Drone::prepare (double sampleRate) { sr = sampleRate; t = 0.0; gain = 0.0f; for (auto* v : { ic1, ic2 }) v[0] = v[1] = 0.0f; }

void Drone::process (float* l, float* r, int n, float level, int note, float move) noexcept
{
    if (level <= 0.0f && gain < 1.0e-5f) { gain = 0.0f; return; }
    const double base = 440.0 * std::exp2 ((36 + juce::jlimit (0, 11, note) - 69) / 12.0);   // C2 .. B2
    const double speed = 0.2 + 2.5 * (double) move;    // how fast the slow changes move
    for (int i = 0; i < n; ++i)
    {
        gain += (level - gain) * 0.0005f;
        t += 1.0 / sr;
        const double s = t * speed;
        // three slow, unrelated movements: the tuning, the filter, the wind
        const double drift = 0.12 * std::sin (twoPi * 0.071 * s) + 0.05 * std::sin (twoPi * 0.23 * s + 1.0);
        const double cut = 140.0 * std::exp2 (2.6 + 2.2 * std::sin (twoPi * 0.043 * s) + 0.8 * std::sin (twoPi * 0.11 * s + 2.0));
        const float wind = 0.5f + 0.5f * (float) std::sin (twoPi * 0.057 * s + 4.0);
        float x = a.tick (Wave::Triangle, base * std::exp2 (drift / 12.0), sr) * 0.8f
                + b.tick (Wave::Pulse25, base * 2.0 * std::exp2 ((drift - 0.09) / 12.0), sr) * 0.35f
                + c.tick (Wave::Pulse12, base * 1.5 * std::exp2 ((0.06 - drift) / 12.0), sr) * 0.18f;
        noiseHold += (noise.clock() - noiseHold) * 0.2f;
        x += noiseHold * 0.45f * wind;
        const float g = gFor (cut, sr);
        const float yl = svf (x, g, 0.5f, ic1[0], ic2[0]).first;
        const float yr = svf (x, gFor (cut * 1.07, sr), 0.5f, ic1[1], ic2[1]).first;
        l[i] += yl * gain * 0.32f;
        r[i] += yr * gain * 0.32f;
    }
}

//==============================================================================
void Madness::prepare (double sampleRate)
{
    sr = sampleRate;
    const int len = (int) (sr * 0.5);
    bl.assign ((size_t) len, 0.0f);
    br.assign ((size_t) len, 0.0f);
    write = 0;
    grain = (int) (sr * 0.22);
    age[0] = 0; age[1] = grain / 2;
    start[0] = start[1] = 0;
    lp = 0.0f;
}

void Madness::process (float* l, float* r, int n, float madness) noexcept
{
    const int len = (int) bl.size();
    if (len == 0) return;
    const float back = madness * madness * 0.45f;           // how much of the backwards sound
    const float hiss = madness * 0.012f;
    for (int i = 0; i < n; ++i)
    {
        bl[(size_t) write] = l[i];
        br[(size_t) write] = r[i];
        if (back > 0.0001f)
        {
            float wl = 0.0f, wr = 0.0f;
            for (int g = 0; g < 2; ++g)
            {
                if (++age[g] >= grain) { age[g] = 0; start[g] = write; }
                // read backwards from where the grain started, with a soft window
                int at = start[g] - age[g] * 2;          // twice as fast, backwards: a little higher, very strange
                at = ((at % len) + len) % len;
                const float w = (float) std::sin (juce::MathConstants<double>::pi * (double) age[g] / (double) grain);
                wl += bl[(size_t) at] * w;
                wr += br[(size_t) at] * w;
            }
            l[i] += wl * back * 0.7f;
            r[i] += wr * back * 0.7f;
        }
        if (hiss > 0.0f)
        {
            lp += (rnd (seed) - lp) * 0.35f;
            l[i] += lp * hiss;
            r[i] += rnd (seed) * hiss * 0.6f;
        }
        if (++write >= len) write = 0;
    }
}

//==============================================================================
void Formant::prepare (double sampleRate)
{
    sr = sampleRate;
    phase = 0.0;
    for (auto& c : ic1) for (auto& v : c) v = 0.0f;
    for (auto& c : ic2) for (auto& v : c) v = 0.0f;
    mixS.reset (sampleRate, 0.05);
}

void Formant::process (float* l, float* r, int n, float mix, float vowel, float lfo) noexcept
{
    mixS.setTargetValue (mix);
    if (mix <= 0.0f && ! mixS.isSmoothing()) { position = vowel * 4.0f; return; }
    // a male choir: the first three resonances of each vowel (Hz) and how loud each is
    static const float f[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 290, 2250, 2890 }, { 570, 840, 2410 }, { 320, 800, 2240 } };
    static const float a[3] = { 1.0f, 0.55f, 0.3f };
    const int step = 32;
    for (int i0 = 0; i0 < n; i0 += step)
    {
        const int m = juce::jmin (step, n - i0);
        phase += (double) m / sr * 0.18;                // one slow walk through the vowels every ~5 s
        if (phase >= 1.0) phase -= 1.0;
        position = juce::jlimit (0.0f, 4.0f, vowel * 4.0f + lfo * 2.0f * (float) std::sin (twoPi * phase));
        const int lo = juce::jmin (3, (int) position);
        const float fr = position - (float) lo;
        float g[3];
        for (int k = 0; k < 3; ++k) g[k] = gFor (f[lo][k] + (f[lo + 1][k] - f[lo][k]) * fr, sr);
        for (int i = i0; i < i0 + m; ++i)
        {
            const float w = mixS.getNextValue();
            float* ch[2] = { l + i, r + i };
            for (int c = 0; c < 2; ++c)
            {
                const float x = *ch[c];
                float y = 0.0f;
                for (int k = 0; k < 3; ++k) y += a[k] * svf (x, g[k], 0.12f, ic1[c][k], ic2[c][k]).second;
                *ch[c] = x * (1.0f - w) + y * w * 0.55f;
            }
        }
    }
}

//==============================================================================
void Vhs::prepare (double sampleRate)
{
    sr = sampleRate;
    const int len = (int) (sr * 0.05) + 4;
    bl.assign ((size_t) len, 0.0f);
    br.assign ((size_t) len, 0.0f);
    write = 0;
    ph1 = ph2 = 0.0;
    for (auto& c : lp) c[0] = c[1] = 0.0f;
    hissLp = drift = 0.0f;
}

void Vhs::process (float* l, float* r, int n, float hiss, float wow, float damp) noexcept
{
    const int len = (int) bl.size();
    if (len == 0 || (hiss <= 0.0f && wow <= 0.0f && damp <= 0.0f)) return;
    // damping: two one-pole low-passes, 18 kHz .. 2.2 kHz
    const double hz = 18000.0 * std::pow (2200.0 / 18000.0, (double) damp);
    const float k = damp > 0.0f ? (float) (1.0 - std::exp (-twoPi * hz / sr)) : 1.0f;
    for (int i = 0; i < n; ++i)
    {
        bl[(size_t) write] = l[i];
        br[(size_t) write] = r[i];
        float yl = l[i], yr = r[i];
        if (wow > 0.0f)
        {
            // wow: a slow wobble of the tape speed; flutter: a fast small one; plus a little wandering
            ph1 += 0.55 / sr; if (ph1 >= 1.0) ph1 -= 1.0;
            ph2 += 8.5 / sr;  if (ph2 >= 1.0) ph2 -= 1.0;
            drift += (rnd (seed) - drift) * 0.0002f;
            const double ms = 6.0 + wow * (4.5 * std::sin (twoPi * ph1) + 0.5 * std::sin (twoPi * ph2) + 6.0 * drift);
            const double back = juce::jlimit (1.0, (double) len - 2.0, ms * 0.001 * sr);
            double pos = (double) write - back;
            if (pos < 0.0) pos += len;
            const int p0 = (int) pos, p1 = (p0 + 1) % len;
            const float fr = (float) (pos - p0);
            const float wl = bl[(size_t) p0] + (bl[(size_t) p1] - bl[(size_t) p0]) * fr;
            const float wr = br[(size_t) p0] + (br[(size_t) p1] - br[(size_t) p0]) * fr;
            const float mixW = juce::jmin (1.0f, wow * 4.0f);       // fade the wobble in from 0
            yl += (wl - yl) * mixW;
            yr += (wr - yr) * mixW;
        }
        if (damp > 0.0f)
        {
            lp[0][0] += (yl - lp[0][0]) * k; lp[0][1] += (lp[0][0] - lp[0][1]) * k; yl = lp[0][1];
            lp[1][0] += (yr - lp[1][0]) * k; lp[1][1] += (lp[1][0] - lp[1][1]) * k; yr = lp[1][1];
        }
        if (hiss > 0.0f)
        {
            const float w = rnd (seed);
            hissLp += (w - hissLp) * 0.5f;
            const float h = (w - hissLp) * hiss * 0.03f;           // high, thin tape noise
            yl += h;
            yr += (rnd (seed) - hissLp) * hiss * 0.025f;
        }
        l[i] = yl;
        r[i] = yr;
        if (++write >= len) write = 0;
    }
}

//==============================================================================
float Bell::tick (double hz, double sampleRate, double t, float crack) noexcept
{
    // 0: the carrier, 1: its modulator; 2, 3: the "crack" partials that do not fit the note
    const double c = (double) crack;
    const double ratio[4] = { 1.0, 2.0 + 1.53 * c, 2.76 + 0.31 * c, 5.40 + 0.9 * c };
    for (int k = 0; k < 4; ++k)
    {
        ph[k] += juce::jmin (0.49, hz * ratio[k] / sampleRate);
        if (ph[k] >= 1.0) ph[k] -= 1.0;
    }
    const double index = (1.2 + 3.5 * c) * std::exp (-t / (0.35 + 0.4 * (1.0 - c)));   // bright strike, then pure
    double y = std::sin (twoPi * ph[0] + index * std::sin (twoPi * ph[1]));
    y += c * 0.45 * std::exp (-t / 1.4) * std::sin (twoPi * ph[2]);
    y += c * 0.3 * std::exp (-t / 0.5) * std::sin (twoPi * ph[3]);
    // the bell's own long ring (high parts die first, the body rings for seconds)
    return (float) (y * std::exp (-t / 6.0)) * 0.85f;
}
} // namespace tomb

#include "Engine.h"
#include "Dsp.h"
#include <cmath>
#include <complex>
#include <memory>

/**
    WAVETABLE: an oscillator that morphs through a table of 16 single-cycle waves (the position
    knob; an envelope and an LFO can move it), with unison, warp, a sub, a resonant filter with
    its own envelope, an amp envelope and chorus. The 8 tables are built at start from harmonic
    recipes (basic shapes, pulse, vowels, harmonic sweep, digital, organ, growl, glass), each in
    band-limited versions per octave so high notes don't alias.
*/
namespace catbox
{
namespace
{
enum
{
    wTable = 0, wPos, wPosEnv, wPosLfo, wLfoRate, wWarp, wUnison, wDetune, wSpread, wSub, wOctave, wVolume,
    wFType, wCutoff, wReso, wEnvAmt, wKeyTrack, wDrive, wFA, wFD, wFS, wFR, wVelFilt, wHp,
    wA, wD, wS, wR, wPA, wPD, wGlide, wMode, wChorus, wWidth, wLfoSync, wVelAmp,
    wCount
};

constexpr int tableSize = 2048, frames = 16, numTables = 8, mips = 9;   // mip m: harmonics up to 512 >> m ... (enough for C0 .. C9)
const char* tableNames[numTables] = { "Basic", "Pulse", "Vowels", "Harmonics", "Digital", "Organ", "Growl", "Glass" };

/** In-place radix-2 FFT (inverse when inv). */
void fft (std::vector<std::complex<float>>& a, bool inv)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const float ang = dsp::twoPi / (float) len * (inv ? 1.0f : -1.0f);
        const std::complex<float> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<float> w (1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

/** Harmonic amplitude (and sign) of harmonic h (1..) for table t at morph x (0..1). */
float recipe (int t, int h, float x)
{
    const float fh = (float) h;
    switch (t)
    {
        case 0:   // sine > triangle > saw > square
        {
            const float sine = h == 1 ? 1.0f : 0.0f;
            const float tri = (h % 2) ? ((((h - 1) / 2) % 2) ? -1.0f : 1.0f) / (fh * fh) : 0.0f;
            const float saw = 1.0f / fh;
            const float sq = (h % 2) ? 1.0f / fh : 0.0f;
            if (x < 0.333f) { const float k = x / 0.333f; return sine * (1 - k) + tri * k * 1.2f; }
            if (x < 0.666f) { const float k = (x - 0.333f) / 0.333f; return tri * 1.2f * (1 - k) + saw * k; }
            const float k = (x - 0.666f) / 0.334f; return saw * (1 - k) + sq * k;
        }
        case 1:   // pulse width 50% .. 3%
        {
            const float w = 0.5f - 0.47f * x;
            return 2.0f * std::sin (dsp::pi * fh * w) / (dsp::pi * fh);
        }
        case 2:   // vowels a e i o u: a saw through formant peaks
        {
            static const float f1[5] = { 800, 400, 300, 450, 325 }, f2[5] = { 1150, 1700, 2300, 800, 700 }, f3[5] = { 2900, 2600, 3000, 2830, 2530 };
            const float pos = x * 4.0f;
            const int v0 = juce::jlimit (0, 3, (int) pos);
            const float k = pos - (float) v0;
            const float hz = fh * 110.0f;   // the cycle as if played at A2
            auto peak = [hz] (float c, float bw) { const float d = (hz - c) / bw; return std::exp (-d * d); };
            auto shape = [&] (int v) { return peak (f1[v], 90.0f) + 0.6f * peak (f2[v], 120.0f) + 0.3f * peak (f3[v], 160.0f); };
            return (shape (v0) * (1 - k) + shape (v0 + 1) * k) / std::sqrt (fh) * 2.0f;
        }
        case 3:   // harmonic sweep: a band of harmonics moving up
        {
            const float centre = 1.0f + x * 40.0f;
            const float d = (fh - centre) / (2.0f + x * 6.0f);
            return std::exp (-d * d) + (h == 1 ? 0.3f : 0.0f);
        }
        case 4:   // digital: hard sync sweep (spectrum of a synced saw), then bit-like steps
        {
            const float ratio = 1.0f + x * 7.0f;
            const float d = fh - ratio;
            const float sinc = std::abs (d) < 1.0e-3f ? 1.0f : std::sin (dsp::pi * d) / (dsp::pi * d);
            return sinc / std::sqrt (fh) * 1.5f;
        }
        case 5:   // organ: drawbar combinations
        {
            static const int bars[9] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };   // harmonic numbers of the drawbars (approx.)
            float a = 0.0f;
            for (int b = 0; b < 9; ++b)
                if (bars[b] == h)
                {
                    const float on = juce::jlimit (0.0f, 1.0f, x * 9.0f - (float) b + 1.0f);
                    a = (b < 3 ? 1.0f : on) * (b == 0 ? 0.8f : 0.6f);
                }
            return a;
        }
        case 6:   // growl: phase-modulated cycles, more and more
        {
            // Bessel-like spread of harmonics around 1 and 2, with the index rising
            const float index = x * 6.0f;
            const float a = std::exp (-std::pow ((fh - 1.0f) / (0.5f + index), 2.0f)) + 0.7f * std::exp (-std::pow ((fh - 2.0f - index) / (0.7f + index * 0.6f), 2.0f));
            return a * ((h % 3 == 0) ? -1.0f : 1.0f) / std::sqrt (fh);
        }
        default:  // glass: sparse high partials, brighter as it moves
        {
            static const int set[6] = { 1, 3, 7, 11, 17, 23 };
            for (int i = 0; i < 6; ++i) if (set[i] == h) return i == 0 ? 1.0f : (0.25f + 0.75f * x) / (1.0f + 0.3f * (float) i);
            return 0.0f;
        }
    }
}

struct Tables
{
    // data[table][frame][mip][sample]
    std::vector<float> data;
    Tables()
    {
        data.assign ((size_t) numTables * frames * mips * (tableSize + 1), 0.0f);
        std::vector<std::complex<float>> buf (tableSize);
        for (int t = 0; t < numTables; ++t)
            for (int f = 0; f < frames; ++f)
            {
                const float x = (float) f / (float) (frames - 1);
                float amp[tableSize / 2] {};
                for (int h = 1; h < tableSize / 2; ++h) amp[h] = recipe (t, h, x);
                // normalise the loudest mip to a peak of ~1
                float norm = 1.0f;
                for (int m = 0; m < mips; ++m)
                {
                    const int maxH = juce::jmax (1, (tableSize / 4) >> m);
                    std::fill (buf.begin(), buf.end(), std::complex<float> (0.0f, 0.0f));
                    for (int h = 1; h <= maxH && h < tableSize / 2; ++h)
                    {
                        buf[(size_t) h] = std::complex<float> (0.0f, -0.5f * amp[h]);
                        buf[(size_t) (tableSize - h)] = std::complex<float> (0.0f, 0.5f * amp[h]);
                    }
                    fft (buf, true);
                    float* out = at (t, f, m);
                    float peak = 0.0f;
                    for (int i = 0; i < tableSize; ++i) { out[i] = buf[(size_t) i].real(); peak = juce::jmax (peak, std::abs (out[i])); }
                    if (m == 0) norm = peak > 1.0e-6f ? 0.9f / peak : 1.0f;
                    for (int i = 0; i < tableSize; ++i) out[i] *= norm;
                    out[tableSize] = out[0];
                }
            }
    }
    float* at (int t, int f, int m) { return data.data() + (((size_t) t * frames + (size_t) f) * mips + (size_t) m) * (tableSize + 1); }
    const float* at (int t, int f, int m) const { return data.data() + (((size_t) t * frames + (size_t) f) * mips + (size_t) m) * (tableSize + 1); }
};

const Tables& tables() { static const Tables t; return t; }
}

EngineDef makeWavetableDef()
{
    using namespace defs;
    EngineDef e;
    e.name = "Wavetable";
    juce::StringArray tn;
    for (auto* n : tableNames) tn.add (n);
    tables();   // built now (message thread), not on the first note
    e.params = {
        choice ("table", "Table", tn, 0), pct ("pos", "Position", 0.5f), bi ("posenv", "Env > pos", 0.0f), pct ("poslfo", "LFO > pos", 0.0f),
        hz ("lforate", "LFO rate", 0.02f, 12.0f, 0.5f), bi ("warp", "Warp", 0.0f), num ("unison", "Unison", 1, 7, 1), cents ("detune", "Detune", 0, 60, 15),
        pct ("spread", "Stereo", 0.6f), pct ("sub", "Sub osc", 0.0f), num ("octave", "Octave", -2, 2, 0), db ("volume", "Volume", -36.0f, 6.0f, -6.0f),
        choice ("ftype", "Filter", { "LP 24", "LP 12", "Band", "High", "Off" }, 0), hz ("cutoff", "Cutoff", 20.0f, 20000.0f, 6000.0f), pct ("reso", "Resonance", 0.15f),
        bi ("envamt", "Env amount", 0.2f), pct ("keytrack", "Key track", 0.5f), pct ("drive", "Drive", 0.0f),
        sec ("fa", "F attack", 0.001f, 10.0f, 0.005f), sec ("fd", "F decay", 0.005f, 10.0f, 0.5f), pct ("fs", "F sustain", 0.4f), sec ("fr", "F release", 0.005f, 10.0f, 0.5f),
        pct ("velfilt", "Vel > filter", 0.3f), hz ("hp", "Low cut", 20.0f, 2000.0f, 20.0f),
        sec ("a", "Attack", 0.001f, 10.0f, 0.005f), sec ("d", "Decay", 0.005f, 10.0f, 0.5f), pct ("s", "Sustain", 0.8f), sec ("r", "Release", 0.005f, 10.0f, 0.4f),
        sec ("pa", "Pos attack", 0.001f, 10.0f, 0.01f), sec ("pd", "Pos decay", 0.01f, 10.0f, 1.0f),
        sec ("glide", "Glide", 0.0f, 2.0f, 0.0f), choice ("mode", "Voices mode", { "Poly", "Mono", "Legato" }, 0), pct ("chorus", "Chorus", 0.0f), pct ("width", "Width", 1.0f),
        choice ("lfosync", "LFO sync", syncNames(), 0), pct ("velamp", "Velocity", 0.5f),
    };
    jassert ((int) e.params.size() == wCount);
    e.pages = {
        page ("WAVE",   { wTable, wPos, wPosEnv, wPosLfo, wLfoRate, wWarp, wUnison, wDetune, wSpread, wSub, wOctave, wVolume,
                          wPA, wPD, wLfoSync, wGlide, wMode, wChorus, wWidth, wVelAmp }),
        page ("FILTER", { wFType, wCutoff, wReso, wEnvAmt, wKeyTrack, wDrive, wFA, wFD, wFS, wFR, wVelFilt, wHp }),
        page ("ENV",    { wA, wD, wS, wR, wPA, wPD, wFA, wFD, wFS, wFR, wPosEnv, wVelAmp }),
    };

    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, const char* cat, V v) { e.presets.push_back ({ name, cat, std::move (v) }); };
    add ("Morph Pad", "Pad", { { "table", 0 }, { "pos", 0.3f }, { "poslfo", 0.4f }, { "lforate", 0.15f }, { "unison", 5 }, { "detune", 18 }, { "a", 1.0f }, { "r", 2.0f }, { "cutoff", 3000 }, { "chorus", 0.4f } });
    add ("Vowel Pad", "Pad", { { "table", 2 }, { "pos", 0.0f }, { "poslfo", 0.9f }, { "lforate", 0.1f }, { "unison", 3 }, { "detune", 12 }, { "a", 0.8f }, { "r", 2.0f }, { "chorus", 0.3f } });
    add ("Glass Pad", "Pad", { { "table", 7 }, { "pos", 0.4f }, { "poslfo", 0.3f }, { "unison", 3 }, { "a", 1.2f }, { "r", 2.5f }, { "chorus", 0.5f } });
    add ("Harmonic Drift", "Pad", { { "table", 3 }, { "pos", 0.2f }, { "poslfo", 0.6f }, { "lforate", 0.07f }, { "unison", 4 }, { "a", 1.5f }, { "r", 3.0f }, { "cutoff", 5000 } });
    add ("Choir Aah", "Vocal", { { "table", 2 }, { "pos", 0.0f }, { "unison", 5 }, { "detune", 10 }, { "a", 0.4f }, { "r", 1.0f }, { "cutoff", 4000 }, { "chorus", 0.5f } });
    add ("Talking Lead", "Vocal", { { "table", 2 }, { "mode", 2 }, { "pos", 0.1f }, { "posenv", 0.8f }, { "pa", 0.2f }, { "pd", 0.6f }, { "glide", 0.06f }, { "cutoff", 7000 } });
    add ("Wow Bass", "Bass", { { "table", 2 }, { "mode", 2 }, { "octave", -1 }, { "pos", 0.7f }, { "posenv", -0.7f }, { "pd", 0.25f }, { "sub", 0.5f }, { "cutoff", 1500 }, { "drive", 0.3f } });
    add ("Growl Bass", "Bass", { { "table", 6 }, { "mode", 2 }, { "octave", -1 }, { "pos", 0.4f }, { "poslfo", 0.7f }, { "lfosync", 5 }, { "sub", 0.4f }, { "drive", 0.5f }, { "cutoff", 2500 }, { "reso", 0.3f } });
    add ("Digital Bass", "Bass", { { "table", 4 }, { "mode", 2 }, { "octave", -1 }, { "pos", 0.2f }, { "posenv", 0.6f }, { "pd", 0.2f }, { "sub", 0.5f }, { "cutoff", 2000 } });
    add ("Reese WT", "Bass", { { "table", 0 }, { "mode", 2 }, { "octave", -1 }, { "pos", 0.75f }, { "unison", 3 }, { "detune", 25 }, { "spread", 0.3f }, { "cutoff", 900 }, { "drive", 0.3f } });
    add ("Sync Lead", "Lead", { { "table", 4 }, { "mode", 2 }, { "pos", 0.3f }, { "posenv", 0.6f }, { "pd", 0.4f }, { "unison", 2 }, { "detune", 8 }, { "glide", 0.05f } });
    add ("Supersaw WT", "Lead", { { "table", 0 }, { "pos", 0.67f }, { "unison", 7 }, { "detune", 30 }, { "spread", 1.0f }, { "cutoff", 9000 }, { "chorus", 0.2f } });
    add ("Pulse Lead", "Lead", { { "table", 1 }, { "mode", 2 }, { "pos", 0.3f }, { "poslfo", 0.4f }, { "lforate", 3.0f }, { "glide", 0.05f } });
    add ("Glass Pluck", "Pluck", { { "table", 7 }, { "pos", 0.8f }, { "posenv", -0.6f }, { "pd", 0.3f }, { "d", 0.6f }, { "s", 0.0f }, { "r", 0.5f } });
    add ("Digi Pluck", "Pluck", { { "table", 4 }, { "pos", 0.1f }, { "posenv", 0.8f }, { "pd", 0.15f }, { "d", 0.4f }, { "s", 0.0f }, { "cutoff", 5000 }, { "envamt", 0.4f }, { "fd", 0.2f }, { "fs", 0.0f } });
    add ("Harmonic Pluck", "Pluck", { { "table", 3 }, { "pos", 0.7f }, { "posenv", -0.7f }, { "pd", 0.2f }, { "d", 0.5f }, { "s", 0.0f }, { "r", 0.4f } });
    add ("Organ WT", "Keys", { { "table", 5 }, { "pos", 0.6f }, { "a", 0.003f }, { "s", 1.0f }, { "r", 0.08f }, { "chorus", 0.4f } });
    add ("Full Organ", "Keys", { { "table", 5 }, { "pos", 1.0f }, { "a", 0.003f }, { "s", 1.0f }, { "r", 0.08f }, { "chorus", 0.5f }, { "drive", 0.2f } });
    add ("Bell Keys", "Keys", { { "table", 7 }, { "pos", 0.6f }, { "d", 1.5f }, { "s", 0.2f }, { "r", 1.0f } });
    add ("Riser", "FX", { { "table", 3 }, { "pos", 0.0f }, { "posenv", 1.0f }, { "pa", 4.0f }, { "pd", 4.0f }, { "a", 2.0f }, { "unison", 5 }, { "detune", 30 } });
    add ("Robot", "FX", { { "table", 2 }, { "mode", 1 }, { "poslfo", 1.0f }, { "lfosync", 5 }, { "pos", 0.5f }, { "warp", 0.4f } });
    add ("Init", "Init", {});
    return e;
}

//==============================================================================
namespace
{
class WavetableEngine : public Engine
{
public:
    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        for (auto& v : voices) v = Voice();
        chorus.reset();
        volume.setTime (0.02f, sr);
    }

    void setParams (const float* v) override
    {
        for (int i = 0; i < wCount; ++i) p[(size_t) i] = v[i];
        for (auto& vo : voices)
        {
            vo.amp.set (p[wA], p[wD], p[wS], p[wR], sr);
            vo.fenv.set (p[wFA], p[wFD], p[wFS], p[wFR], sr);
            vo.penv.set (p[wPA], p[wPD], 0.0f, p[wPD], sr);
        }
    }
    void setTempo (double bpm) override { tempo = (float) bpm; }
    void setBend (float s) override { bend = s; }
    void setMod (float m) override { mod = m; }

    void noteOn (int note, float vel, bool slide, bool) override
    {
        const int mode = juce::roundToInt (p[wMode]);
        Voice* v = nullptr;
        bool legato = false;
        if (mode != 0)
        {
            v = &voices[0];
            legato = (mode == 2 && v->gate) || slide;
            if (numHeld < 16) held[(size_t) numHeld++] = note;
        }
        else
        {
            for (auto& x : voices) if (x.active && x.note == note) v = &x;
            if (v == nullptr) for (auto& x : voices) if (! x.active) { v = &x; break; }
            if (v == nullptr)
            {
                uint64_t best = ~(uint64_t) 0;
                for (int pass = 0; pass < 2 && v == nullptr; ++pass)
                    for (auto& x : voices) if ((pass == 1 || ! x.gate) && x.age < best) { best = x.age; v = &x; }
            }
        }
        if (v == nullptr) return;
        v->target = (float) note;
        if (! v->active || (p[wGlide] < 0.001f && ! legato)) v->pitch = (float) note;
        v->note = note;
        v->gate = true;
        if (! legato)
        {
            const bool sounding = v->amp.value > 0.001f;
            v->vel = vel;
            v->amp.set (p[wA], p[wD], p[wS], p[wR], sr);
            v->fenv.set (p[wFA], p[wFD], p[wFS], p[wFR], sr);
            v->penv.set (p[wPA], p[wPD], 0.0f, p[wPD], sr);
            v->amp.gateOn (true); v->fenv.gateOn (true); v->penv.gateOn (true);
            if (! sounding) for (auto& ph : v->phase) ph = rng.uni();
        }
        v->active = true;
        v->age = ++counter;
    }

    void noteOff (int note) override
    {
        if (juce::roundToInt (p[wMode]) != 0)
        {
            int n = 0;
            for (int i = 0; i < numHeld; ++i) if (held[(size_t) i] != note) held[(size_t) n++] = held[(size_t) i];
            numHeld = n;
            auto& v = voices[0];
            if (numHeld > 0) { v.target = (float) held[(size_t) numHeld - 1]; v.note = held[(size_t) numHeld - 1]; return; }
            v.gate = false; v.amp.gateOff(); v.fenv.gateOff(); v.penv.gateOff();
            return;
        }
        for (auto& v : voices) if (v.active && v.gate && v.note == note) { v.gate = false; v.amp.gateOff(); v.fenv.gateOff(); v.penv.gateOff(); }
    }

    void allOff() override { for (auto& v : voices) { v.amp.kill(); v.active = v.gate = false; } numHeld = 0; }

    void render (float* outL, float* outR, int n) override
    {
        bool any = false;
        for (auto& v : voices) any = any || v.active;
        if (! any && tail <= 0) return;
        tail = any ? (int) (sr * 0.3f) : tail - n;

        const auto& tb = tables();
        const int table = juce::jlimit (0, numTables - 1, juce::roundToInt (p[wTable]));
        const int uni = juce::jlimit (1, 7, juce::roundToInt (p[wUnison]));
        const float uniGain = 1.0f / std::sqrt ((float) uni);
        const int ftype = juce::roundToInt (p[wFType]);
        const double sb = syncBeats (juce::roundToInt (p[wLfoSync]));
        const float lfoHz = sb > 0.0 ? (float) (tempo / 60.0 / sb) : p[wLfoRate];
        const float glideK = p[wGlide] > 0.001f ? std::exp (-(float) control / (p[wGlide] * sr)) : 0.0f;
        const float vol = dsp::dbToGain (p[wVolume]);
        const float warp = p[wWarp];
        const float drive = 1.0f + p[wDrive] * 6.0f;

        for (int s0 = 0; s0 < n; s0 += control)
        {
            const int m = juce::jmin (control, n - s0);
            lfoPh += lfoHz * (float) m / sr; if (lfoPh >= 1.0f) lfoPh -= 1.0f;
            const float lfo = std::sin (dsp::twoPi * lfoPh);
            float bl[control] {}, br[control] {};
            for (auto& v : voices)
            {
                if (! v.active) continue;
                v.pitch = glideK > 0.0f ? v.target + (v.pitch - v.target) * glideK : v.target;
                const float note = v.pitch + 12.0f * p[wOctave] + bend;
                const float f0 = dsp::midiToHz (note);
                const int mip = juce::jlimit (0, mips - 1, (int) std::ceil (std::log2 (juce::jmax (1.0f, f0 * (float) (tableSize / 4) * 2.0f / sr))));
                // position: knob + envelope + LFO (+ mod wheel)
                float pos = p[wPos] + p[wPosEnv] * v.penv.value + p[wPosLfo] * 0.5f * lfo + mod * 0.5f;
                pos = juce::jlimit (0.0f, 1.0f, pos) * (frames - 1);
                const int f0i = juce::jmin (frames - 2, (int) pos);
                const float fk = pos - (float) f0i;
                const float* ta = tb.at (table, f0i, mip);
                const float* tc = tb.at (table, f0i + 1, mip);
                // filter cutoff for this block
                const float oct = p[wEnvAmt] * v.fenv.value * 7.0f + p[wKeyTrack] * (v.pitch - 60.0f) / 12.0f + p[wVelFilt] * (v.vel - 0.7f) * 3.0f;
                const float cut = juce::jlimit (20.0f, sr * 0.45f, p[wCutoff] * std::exp2 (oct));
                if (ftype == 0) v.ladder.rampTo (cut, p[wReso], sr, m);
                else if (ftype < 4) v.svf.rampTo (cut, 0.5f + p[wReso] * 12.0f, sr, m);
                v.hp.set (p[wHp], 0.7f, sr);
                const float velGain = 1.0f - p[wVelAmp] + p[wVelAmp] * v.vel;
                const float dt = f0 / sr;
                for (int i = 0; i < m; ++i)
                {
                    const float a = v.amp.process();
                    v.fenv.process(); v.penv.process();
                    float l = 0.0f, r = 0.0f;
                    for (int u = 0; u < uni; ++u)
                    {
                        const float det = uni > 1 ? (2.0f * (float) u / (float) (uni - 1) - 1.0f) : 0.0f;
                        auto& ph = v.phase[(size_t) u];
                        ph += dt * std::exp2 (det * p[wDetune] / 1200.0f);
                        if (ph >= 1.0f) ph -= 1.0f;
                        // warp: bends the phase (more of the cycle at the start or the end)
                        float wp = ph;
                        if (std::abs (warp) > 0.01f) wp = warp > 0.0f ? std::pow (ph, 1.0f + warp * 3.0f) : 1.0f - std::pow (1.0f - ph, 1.0f - warp * 3.0f);
                        const float x = wp * tableSize;
                        const int i0 = (int) x;
                        const float fr = x - (float) i0;
                        const float sa = ta[i0] + (ta[i0 + 1] - ta[i0]) * fr, sc = tc[i0] + (tc[i0 + 1] - tc[i0]) * fr;
                        const float sv = sa + (sc - sa) * fk;
                        const float pan = det * p[wSpread];
                        l += sv * (1.0f - pan) * 0.7071f; r += sv * (1.0f + pan) * 0.7071f;
                    }
                    float sub = 0.0f;
                    if (p[wSub] > 0.001f) { v.subPh += dt * 0.5f; if (v.subPh >= 1.0f) v.subPh -= 1.0f; sub = p[wSub] * std::sin (dsp::twoPi * v.subPh); }
                    l = l * uniGain + sub; r = r * uniGain + sub;
                    if (drive > 1.01f) { l = dsp::fastTanh (l * drive) / std::sqrt (drive); r = dsp::fastTanh (r * drive) / std::sqrt (drive); }
                    float mono = 0.5f * (l + r), side = 0.5f * (l - r);
                    if (ftype == 0) mono = v.ladder.process (mono);
                    else if (ftype < 4) { const auto o = v.svf.process (mono); mono = ftype == 1 ? o.low : ftype == 2 ? o.band : o.high; }
                    mono = v.hp.process (mono).high;
                    if (ftype != 4) side *= juce::jlimit (0.0f, 1.0f, cut / 4000.0f);   // the filter darkens the width too
                    const float g = a * velGain * 0.5f;
                    bl[i] += (mono + side) * g;
                    br[i] += (mono - side) * g;
                }
                if (! v.amp.active()) v.active = v.gate = false;
            }
            for (int i = 0; i < m; ++i)
            {
                float l = bl[i], r = br[i];
                if (p[wChorus] > 0.001f) chorus.process (l, r, p[wChorus], 0.4f, 0.6f, sr);
                const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * p[wWidth];
                const float g = volume.process (vol) * 2.4f;
                outL[s0 + i] += (mid + side) * g;
                outR[s0 + i] += (mid - side) * g;
            }
        }
    }

private:
    static constexpr int control = 16;
    struct Voice
    {
        bool active = false, gate = false;
        int note = 60;
        float vel = 0.8f, pitch = 60.0f, target = 60.0f, subPh = 0.0f;
        std::array<float, 7> phase {};
        uint64_t age = 0;
        dsp::Adsr amp, fenv, penv;
        dsp::Ladder ladder;
        dsp::Svf svf, hp;
    };
    std::array<float, wCount> p {};
    std::array<Voice, 8> voices;
    std::array<int, 16> held {};
    int numHeld = 0, tail = 0;
    uint64_t counter = 0;
    float sr = 48000.0f, tempo = 120.0f, bend = 0.0f, mod = 0.0f, lfoPh = 0.0f;
    dsp::Rng rng;
    dsp::Chorus chorus;
    dsp::Smooth volume;
};
} // namespace

std::unique_ptr<Engine> makeWavetableEngine() { return std::make_unique<WavetableEngine>(); }
} // namespace catbox

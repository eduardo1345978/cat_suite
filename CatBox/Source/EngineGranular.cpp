#include "Engine.h"
#include "Dsp.h"
#include "Samples.h"
#include <cmath>

/**
    GRANULAR: clouds of tiny grains read from a sample (any of the box's samples, also the ones you
    record). Where in the sample (position, and how it travels), grain size, how many, scatter in
    time and pitch, reverse grains, stereo spread; the notes you play set the pitch.
*/
namespace catbox
{
namespace
{
enum
{
    gSample = 0, gPos, gScan, gSize, gDensity, gSpray, gPitchRand, gReverse, gSpread, gShape, gOctave, gVolume,
    gA, gD, gS, gR, gCutoff, gReso, gFine, gGlideNotes, gVelAmp, gWidth, gFreeze, gDrift,
    gCount
};
}

EngineDef makeGranularDef()
{
    using namespace defs;
    EngineDef e;
    e.name = "Granular";
    auto sample = num ("sample", "Sample", 0, SampleBank::maxSamples - 1, 3);
    sample.formatFn = [] (float v) { return SampleBank::get().name (juce::roundToInt (v)); };
    e.params = {
        sample, pct ("pos", "Position", 0.3f), bi ("scan", "Travel", 0.0f), sec ("size", "Grain size", 0.01f, 0.5f, 0.08f),
        num ("density", "Grains", 1, 40, 12), pct ("spray", "Scatter", 0.1f), semi ("pitchrand", "Pitch scatter", 0, 12, 0), pct ("reverse", "Reverse", 0.0f),
        pct ("spread", "Stereo", 0.6f), choice ("shape", "Grain shape", { "Soft", "Sharp", "Long tail" }, 0), num ("octave", "Octave", -2, 2, 0),
        db ("volume", "Volume", -36.0f, 6.0f, -3.0f),
        sec ("a", "Attack", 0.001f, 10.0f, 0.3f), sec ("d", "Decay", 0.01f, 10.0f, 1.0f), pct ("s", "Sustain", 1.0f), sec ("r", "Release", 0.01f, 10.0f, 1.5f),
        hz ("cutoff", "Cutoff", 40.0f, 20000.0f, 16000.0f), pct ("reso", "Resonance", 0.1f), cents ("fine", "Fine", -50, 50, 0),
        toggle ("fixed", "Fixed pitch", false), pct ("velamp", "Velocity", 0.4f), pct ("width", "Width", 1.0f), toggle ("freeze", "Freeze", false),
        pct ("drift", "Drift", 0.2f),
    };
    jassert ((int) e.params.size() == gCount);
    e.pages = { page ("GRAINS", { gSample, gPos, gScan, gSize, gDensity, gSpray, gPitchRand, gReverse, gSpread, gShape, gOctave, gVolume,
                                  gA, gD, gS, gR, gCutoff, gReso, gFine, gGlideNotes, gVelAmp, gWidth, gFreeze, gDrift }) };
    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, const char* cat, V v) { e.presets.push_back ({ name, cat, std::move (v) }); };
    add ("Pad Cloud", "Texture", { { "sample", 3 }, { "pos", 0.4f }, { "size", 0.15f }, { "density", 16 }, { "spray", 0.3f } });
    add ("Vox Cloud", "Texture", { { "sample", 0 }, { "pos", 0.35f }, { "size", 0.12f }, { "density", 14 }, { "spray", 0.2f }, { "pitchrand", 0 }, { "drift", 0.4f } });
    add ("Choir Haze", "Texture", { { "sample", 6 }, { "pos", 0.5f }, { "scan", 0.1f }, { "size", 0.25f }, { "density", 20 }, { "spray", 0.5f }, { "a", 1.0f }, { "r", 3.0f } });
    add ("Piano Shimmer", "Texture", { { "sample", 1 }, { "pos", 0.05f }, { "size", 0.06f }, { "density", 24 }, { "spray", 0.15f }, { "pitchrand", 12 }, { "a", 0.5f } });
    add ("Bell Dust", "Texture", { { "sample", 2 }, { "pos", 0.1f }, { "scan", 0.3f }, { "size", 0.04f }, { "density", 20 }, { "spray", 0.6f }, { "shape", 1 }, { "pitchrand", 7 } });
    add ("Frozen Vox", "Pad", { { "sample", 0 }, { "pos", 0.4f }, { "freeze", 1 }, { "size", 0.2f }, { "density", 18 }, { "spray", 0.05f }, { "drift", 0.1f } });
    add ("Reverse Wash", "Pad", { { "sample", 3 }, { "pos", 0.6f }, { "reverse", 1.0f }, { "size", 0.3f }, { "density", 10 }, { "spray", 0.4f }, { "a", 1.5f }, { "r", 4.0f } });
    add ("Granular Strings", "Pad", { { "sample", 3 }, { "pos", 0.5f }, { "size", 0.1f }, { "density", 30 }, { "spray", 0.1f }, { "cutoff", 6000 }, { "a", 0.6f } });
    add ("Stutter Break", "Rhythm", { { "sample", 4 }, { "pos", 0.0f }, { "scan", 0.5f }, { "size", 0.06f }, { "density", 6 }, { "shape", 1 }, { "fixed", 1 }, { "a", 0.01f } });
    add ("Glitch Kick", "Rhythm", { { "sample", 5 }, { "pos", 0.02f }, { "size", 0.03f }, { "density", 8 }, { "shape", 1 }, { "pitchrand", 12 }, { "a", 0.005f }, { "r", 0.3f } });
    add ("Space Pluck", "Pluck", { { "sample", 7 }, { "pos", 0.05f }, { "scan", 0.4f }, { "size", 0.05f }, { "density", 12 }, { "a", 0.005f }, { "d", 1.0f }, { "s", 0.0f }, { "r", 1.0f } });
    add ("Deep Drone", "Drone", { { "sample", 6 }, { "octave", -1 }, { "pos", 0.5f }, { "size", 0.35f }, { "density", 12 }, { "spray", 0.6f }, { "cutoff", 2000 }, { "a", 2.0f }, { "r", 5.0f } });
    add ("Init", "Init", {});
    return e;
}

namespace
{
class GranularEngine : public Engine
{
public:
    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        for (auto& v : voices) v = Voice();
        for (auto& gr : grains) gr.active = false;
        volume.setTime (0.02f, sr);
    }
    void setParams (const float* v) override
    {
        for (int i = 0; i < gCount; ++i) p[(size_t) i] = v[i];
        for (auto& vo : voices) vo.amp.set (p[gA], p[gD], p[gS], p[gR], sr);
    }
    void setBend (float s) override { bend = s; }
    void setMod (float m) override { mod = m; }

    void noteOn (int note, float vel, bool, bool) override
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (x.active && x.note == note) v = &x;
        if (v == nullptr) for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr) { uint64_t best = ~(uint64_t) 0; for (auto& x : voices) if (x.age < best) { best = x.age; v = &x; } }
        if (v == nullptr) return;
        v->note = note;
        v->vel = vel;
        v->active = v->gate = true;
        v->age = ++counter;
        v->scanPos = 0.0f;
        v->nextGrain = 0.0f;
        v->amp.set (p[gA], p[gD], p[gS], p[gR], sr);
        v->amp.gateOn (true);
    }
    void noteOff (int note) override { for (auto& v : voices) if (v.active && v.gate && v.note == note) { v.gate = false; v.amp.gateOff(); } }
    void allOff() override { for (auto& v : voices) { v.amp.kill(); v.active = v.gate = false; } for (auto& g : grains) g.active = false; }

    void render (float* outL, float* outR, int n) override
    {
        const Sample* smp = SampleBank::get().at (juce::roundToInt (p[gSample]));
        bool any = false;
        for (auto& g : grains) any = any || g.active;
        for (auto& v : voices) any = any || v.active;
        if (! any || smp == nullptr || smp->length() < 64) return;
        const int len = smp->length();
        const float vol = dsp::dbToGain (p[gVolume]);
        const float sizeS = juce::jmax (0.005f, p[gSize]) * sr;
        const int shape = juce::roundToInt (p[gShape]);
        const bool filt = p[gCutoff] < 19500.0f;
        if (filt) { fl.set (p[gCutoff], 0.6f + p[gReso] * 8.0f, sr); fr.set (p[gCutoff], 0.6f + p[gReso] * 8.0f, sr); }
        const double srRatio = smp->sampleRate / sr;

        for (int i = 0; i < n; ++i)
        {
            // voices start grains
            for (auto& v : voices)
            {
                if (! v.active) continue;
                const float env = v.amp.process();
                v.level = env;
                if (! v.amp.active()) { v.active = v.gate = false; continue; }
                if (p[gFreeze] < 0.5f) v.scanPos += p[gScan] / sr * 0.2f;   // travel: up to 1/5 of the sample per second
                if ((v.nextGrain -= 1.0f) > 0.0f) continue;
                const float density = juce::jmax (1.0f, p[gDensity]);
                v.nextGrain = sizeS / density * (0.6f + 0.8f * rng.uni());
                // a new grain
                Grain* g = nullptr;
                for (auto& x : grains) if (! x.active) { g = &x; break; }
                if (g == nullptr) continue;
                float pos = p[gPos] + v.scanPos + p[gSpray] * 0.5f * rng.next() + mod * 0.3f;
                pos += p[gDrift] * 0.05f * std::sin (driftPh + (float) v.note);
                pos -= std::floor (pos);
                const float semis = (p[gGlideNotes] > 0.5f ? 0.0f : (float) (v.note - 60)) + 12.0f * p[gOctave] + p[gFine] * 0.01f + bend
                                  + (p[gPitchRand] > 0.0f ? std::round (rng.next() * p[gPitchRand]) : 0.0f);
                g->active = true;
                g->pos = pos * (double) (len - 1);
                g->rate = std::exp2 (semis / 12.0) * srRatio * (rng.uni() < p[gReverse] ? -1.0 : 1.0);
                g->t = 0.0f;
                g->dur = sizeS * (0.8f + 0.4f * rng.uni());
                const float pan = p[gSpread] * rng.next();
                g->gl = std::sqrt (0.5f * (1.0f - pan)); g->gr = std::sqrt (0.5f * (1.0f + pan));
                g->gain = (1.0f - p[gVelAmp] + p[gVelAmp] * v.vel) / std::sqrt (juce::jmax (1.0f, density * 0.5f));
                g->voice = (int) (&v - voices.data());
            }
            driftPh += 0.3f / sr;
            float l = 0.0f, r = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const float x = g.t / g.dur;
                if (x >= 1.0f) { g.active = false; continue; }
                float w;
                if (shape == 0) w = 0.5f - 0.5f * std::cos (dsp::twoPi * x);                    // hann
                else if (shape == 1) w = x < 0.1f ? x / 0.1f : (x > 0.9f ? (1.0f - x) / 0.1f : 1.0f);   // sharp edges (still no click)
                else w = x < 0.05f ? x / 0.05f : std::exp (-(x - 0.05f) * 4.0f) * (1.0f - x) * 1.1f;  // percussive tail
                double pos = g.pos;
                while (pos < 0.0) pos += len - 1;
                while (pos >= len - 1) pos -= len - 1;
                const int i0 = (int) pos;
                const float f = (float) (pos - i0);
                const float sl = smp->l[(size_t) i0] + (smp->l[(size_t) i0 + 1] - smp->l[(size_t) i0]) * f;
                const float sr2 = smp->r[(size_t) i0] + (smp->r[(size_t) i0 + 1] - smp->r[(size_t) i0]) * f;
                const float lv = voices[(size_t) g.voice].level;
                const float k = w * g.gain * lv;
                l += sl * k * g.gl; r += sr2 * k * g.gr;
                g.pos += g.rate;
                g.t += 1.0f;
            }
            if (filt) { l = fl.process (l).low; r = fr.process (r).low; }
            const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * p[gWidth];
            const float gg = volume.process (vol) * 1.2f;
            outL[i] += (mid + side) * gg;
            outR[i] += (mid - side) * gg;
        }
    }

private:
    struct Voice { bool active = false, gate = false; int note = 60; float vel = 0.8f, scanPos = 0.0f, nextGrain = 0.0f, level = 0.0f; uint64_t age = 0; dsp::Adsr amp; };
    struct Grain { bool active = false; double pos = 0.0, rate = 1.0; float t = 0.0f, dur = 1.0f, gl = 0.7f, gr = 0.7f, gain = 1.0f; int voice = 0; };
    std::array<float, gCount> p {};
    std::array<Voice, 8> voices;
    std::array<Grain, 96> grains;
    uint64_t counter = 0;
    float sr = 48000.0f, bend = 0.0f, mod = 0.0f, driftPh = 0.0f;
    dsp::Rng rng;
    dsp::Svf fl, fr;
    dsp::Smooth volume;
};
} // namespace

std::unique_ptr<Engine> makeGranularEngine() { return std::make_unique<GranularEngine>(); }
} // namespace catbox

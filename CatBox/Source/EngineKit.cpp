#include "Engine.h"
#include "Dsp.h"
#include "Samples.h"
#include <cmath>

/**
    KIT: a drum kit of 16 samples, one per pad (36 = pad 1). Any sample of the box: your own,
    the starter ones, or the libraries (VSCO 2 CE percussion, Dirt-Samples ...). Per pad: the sample,
    tune, level, pan, decay and a low-pass; the kit: volume, drive, hat choke, stereo spread.
*/
namespace catbox
{
namespace
{
constexpr int lanes = 16, perLane = 6;
enum { kSample = 0, kTune, kLevel, kPan, kDecay, kCut };   // per lane: lane * perLane + ...
enum { kVolume = lanes * perLane, kDrive, kChoke, kSpread, kCount };

struct KitDefault { const char* sample; };
}

int kitLaneParam (int lane, int what) { return lane * perLane + what; }

EngineDef makeKitDef()
{
    using namespace defs;
    EngineDef e;
    e.name = "Kit";
    e.drums = true;
    for (int l = 0; l < lanes; ++l)
    {
        const juce::String n (l + 1);
        auto smp = num ("", "", 0, SampleBank::maxSamples - 1, (float) (8 + l));   // the starter drum hits
        smp.id = "s" + n; smp.name = n + " sample";
        smp.formatFn = [] (float v) { return SampleBank::get().shortName (juce::roundToInt (v)); };
        smp.isSample = true;
        auto tune = semi ("", "", -24, 24, 0); tune.id = "t" + n; tune.name = n + " tune";
        auto lev = db ("", "", -36.0f, 6.0f, -4.0f); lev.id = "l" + n; lev.name = n + " level";
        auto pan = bi ("", "", 0.0f); pan.id = "p" + n; pan.name = n + " pan";
        auto dec = sec ("", "", 0.02f, 8.0f, 8.0f); dec.id = "d" + n; dec.name = n + " decay";
        auto cut = hz ("", "", 200.0f, 20000.0f, 20000.0f); cut.id = "c" + n; cut.name = n + " tone";
        for (auto* p : { &smp, &tune, &lev, &pan, &dec, &cut }) e.params.push_back (*p);
    }
    e.params.push_back (db ("volume", "Volume", -36.0f, 6.0f, -3.0f));
    e.params.push_back (pct ("drive", "Drive", 0.0f));
    e.params.push_back (toggle ("choke", "Hat choke", true));
    e.params.push_back (pct ("spread", "Stereo", 0.3f));
    jassert ((int) e.params.size() == kCount);
    for (int g = 0; g < 4; ++g)
    {
        KnobPage pg;
        pg.name = "PADS " + juce::String (g * 4 + 1) + "-" + juce::String (g * 4 + 4);
        for (int k = 0; k < 4; ++k)
        {
            const int l = g * 4 + k;
            pg.slot[(size_t) (k * 3 + 0)] = l * perLane + kSample;
            pg.slot[(size_t) (k * 3 + 1)] = l * perLane + kTune;
            pg.slot[(size_t) (k * 3 + 2)] = l * perLane + kLevel;
            pg.slot[(size_t) (12 + k * 3 + 0)] = l * perLane + kDecay;
            pg.slot[(size_t) (12 + k * 3 + 1)] = l * perLane + kPan;
            pg.slot[(size_t) (12 + k * 3 + 2)] = l * perLane + kCut;
        }
        e.pages.push_back (pg);
    }
    e.pages.push_back (page ("KIT", { kVolume, kDrive, kChoke, kSpread }));
    // the factory kits come with the library (PresetsExtra); this one uses the starter samples
    e.presets.push_back ({ "Starter Kit", "Kit", {} });
    e.presets.push_back ({ "Init", "Init", {} });
    return e;
}

namespace
{
class KitEngine : public Engine
{
public:
    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        for (auto& v : voices) v = Voice();
    }
    void setParams (const float* v) override { for (int i = 0; i < kCount; ++i) p[(size_t) i] = v[i]; }

    void noteOn (int note, float vel, bool, bool accent) override
    {
        const int lane = ((note - 36) % lanes + lanes) % lanes;
        const Sample* smp = SampleBank::get().at (juce::roundToInt (p[(size_t) (lane * perLane + kSample)]));
        if (smp == nullptr || smp->length() < 8) return;
        // hats: closed (pad 4) chokes open (pad 5)
        if (p[kChoke] > 0.5f && lane == 3) for (auto& v : voices) if (v.active && v.lane == 4) v.fadeOut = true;
        // the same pad again: the old hit fades quickly (no click)
        for (auto& v : voices) if (v.active && v.lane == lane) v.fadeOut = true;
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr) { v = &voices[0]; for (auto& x : voices) if (x.age < v->age) v = &x; }
        *v = Voice();
        v->active = true;
        v->lane = lane;
        v->smp = smp;
        v->age = ++counter;
        v->pos = 0.0;
        v->inc = smp->sampleRate / sr * std::exp2 (p[(size_t) (lane * perLane + kTune)] / 12.0f);
        v->gain = dsp::dbToGain (p[(size_t) (lane * perLane + kLevel)]) * juce::jlimit (0.05f, 1.0f, vel * (accent ? 1.15f : 1.0f));
        const float pan = juce::jlimit (-1.0f, 1.0f, p[(size_t) (lane * perLane + kPan)] + (lane % 2 == 0 ? -1.0f : 1.0f) * p[kSpread] * 0.3f * (lane > 2 ? 1.0f : 0.0f));
        v->gl = std::sqrt (0.5f * (1.0f - pan)) * 1.4142f;
        v->gr = std::sqrt (0.5f * (1.0f + pan)) * 1.4142f;
        const float dec = p[(size_t) (lane * perLane + kDecay)];
        v->decayMul = dec >= 7.9f ? 1.0f : std::exp (-6.9f / (dec * sr));
        v->env = 1.0f;
        const float cut = p[(size_t) (lane * perLane + kCut)];
        v->lpA = cut >= 19500.0f ? 1.0f : 1.0f - std::exp (-dsp::twoPi * cut / sr);
    }
    void noteOff (int) override {}
    void allOff() override { for (auto& v : voices) if (v.active) v.fadeOut = true; }

    void render (float* outL, float* outR, int n) override
    {
        const float vol = dsp::dbToGain (p[kVolume]);
        const float drive = 1.0f + p[kDrive] * 6.0f;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const Sample& s = *v.smp;
            const int len = s.length();
            for (int i = 0; i < n; ++i)
            {
                const int i0 = (int) v.pos;
                if (i0 + 1 >= len || v.env < 0.0005f) { v.active = false; break; }
                const float f = (float) (v.pos - i0);
                float a = s.l[(size_t) i0] + (s.l[(size_t) (i0 + 1)] - s.l[(size_t) i0]) * f;
                float b = s.r[(size_t) i0] + (s.r[(size_t) (i0 + 1)] - s.r[(size_t) i0]) * f;
                v.lpL += (a - v.lpL) * v.lpA; v.lpR += (b - v.lpR) * v.lpA;
                a = v.lpL; b = v.lpR;
                // a short fade at the start (no click on samples that start loud) and the decay
                const float in = v.pos < 32.0 ? (float) v.pos / 32.0f : 1.0f;
                if (v.fadeOut) v.env *= 0.995f; else v.env *= v.decayMul;
                const float g = v.gain * v.env * in;
                outL[i] += a * g * v.gl;
                outR[i] += b * g * v.gr;
                v.pos += v.inc;
            }
        }
        if (drive > 1.001f || vol != 1.0f)
            for (int i = 0; i < n; ++i)
            {
                outL[i] = (drive > 1.001f ? dsp::fastTanh (outL[i] * drive) / std::sqrt (drive) : outL[i]) * vol;
                outR[i] = (drive > 1.001f ? dsp::fastTanh (outR[i] * drive) / std::sqrt (drive) : outR[i]) * vol;
            }
    }

private:
    struct Voice
    {
        bool active = false, fadeOut = false;
        int lane = 0;
        const Sample* smp = nullptr;
        double pos = 0.0, inc = 1.0;
        float gain = 1.0f, gl = 1.0f, gr = 1.0f, env = 1.0f, decayMul = 1.0f, lpA = 1.0f, lpL = 0.0f, lpR = 0.0f;
        uint64_t age = 0;
    };
    std::array<Voice, 32> voices {};
    std::array<float, kCount> p {};
    float sr = 48000.0f;
    uint64_t counter = 0;
};
} // namespace

std::unique_ptr<Engine> makeKitEngine() { return std::make_unique<KitEngine>(); }
} // namespace catbox

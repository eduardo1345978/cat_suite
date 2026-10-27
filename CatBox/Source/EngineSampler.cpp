#include "Engine.h"
#include "Dsp.h"
#include "Samples.h"
#include "Music.h"
#include <cmath>

/**
    SAMPLER: plays a sample (recorded with a microphone in the box, a WAV you put in the samples
    folder, or one of the starter samples). Keys: across the keyboard from its root note, with a
    loop (forward or ping-pong), an envelope and a filter. Chop: the sample cut into 4-16 slices,
    one per pad / key (36 = slice 1). One shot: plays to the end whatever the key does.
*/
namespace catbox
{
namespace
{
enum
{
    sSample = 0, sMode, sRoot, sStart, sEnd, sLoop, sLoopStart, sTune, sFine, sReverse, sSlices, sVolume,
    sA, sD, sS, sR, sCutoff, sReso, sDrive, sCrush, sGlide, sVelAmp, sSpread, sWidth,
    sFType, sEnvAmt, sFA, sFD, sFS, sFR, sKeyTrack, sDown, sPitchEnv, sPitchDecay, sChoke, sHp,
    sCount
};
enum { ModeKeys = 0, ModeChop, ModeOneShot };
}

/** The starter samples (made by the box's own engines the first time) - always in these slots. */
juce::StringArray starterSampleNames()
{
    juce::StringArray n { "Vox Ahh", "Piano C4", "Bell C5", "Warm Pad C3", "Break 120", "Kick 808", "Choir Ooh", "Pluck C4" };
    // slots 8-23: one hit of each drum (the Kit engine starts with them)
    for (const auto* d : { "Kick", "Snare", "Clap", "Closed hat", "Open hat", "Low tom", "Mid tom", "High tom",
                           "Rim", "Cowbell", "Crash", "Ride", "Shaker", "Conga", "Snap", "Zap" })
        n.add (juce::String ("Kit ") + d);
    return n;
}

EngineDef makeSamplerDef()
{
    using namespace defs;
    EngineDef e;
    e.name = "Sampler";
    auto sample = num ("sample", "Sample", 0, SampleBank::maxSamples - 1, 0);
    sample.formatFn = [] (float v) { return SampleBank::get().shortName (juce::roundToInt (v)); };
    sample.isSample = true;
    auto root = num ("root", "Root note", 0, 127, 60);
    root.formatFn = [] (float v) { return music::noteName (juce::roundToInt (v)); };
    e.params = {
        sample, choice ("mode", "Play", { "Keys", "Chop", "One shot" }, 0), root,
        pct ("start", "Start", 0.0f), pct ("end", "End", 1.0f), choice ("loop", "Loop", { "Off", "Forward", "Ping-pong" }, 0), pct ("loopstart", "Loop start", 0.3f),
        semi ("tune", "Tune", -24, 24, 0), cents ("fine", "Fine", -50, 50, 0), toggle ("reverse", "Reverse", false), num ("slices", "Slices", 2, 16, 16),
        db ("volume", "Volume", -36.0f, 6.0f, -3.0f),
        sec ("a", "Attack", 0.001f, 8.0f, 0.002f), sec ("d", "Decay", 0.01f, 10.0f, 1.0f), pct ("s", "Sustain", 1.0f), sec ("r", "Release", 0.005f, 8.0f, 0.2f),
        hz ("cutoff", "Cutoff", 40.0f, 20000.0f, 20000.0f), pct ("reso", "Resonance", 0.1f), pct ("drive", "Drive", 0.0f), pct ("crush", "Bit crush", 0.0f),
        sec ("glide", "Glide", 0.0f, 1.0f, 0.0f), pct ("velamp", "Velocity", 0.6f), pct ("spread", "Stereo", 0.3f), pct ("width", "Width", 1.0f),
        choice ("ftype", "Filter", { "Low-pass", "Band", "High-pass" }, 0), bi ("envamt", "Env amount", 0.0f),
        sec ("fa", "F attack", 0.001f, 8.0f, 0.005f), sec ("fd", "F decay", 0.01f, 8.0f, 0.4f), pct ("fs", "F sustain", 0.5f), sec ("fr", "F release", 0.01f, 8.0f, 0.3f),
        pct ("keytrack", "Key track", 0.0f), pct ("down", "Lo-fi", 0.0f), semi ("penv", "Pitch env", -24, 24, 0), sec ("pdecay", "Pitch decay", 0.005f, 1.0f, 0.05f),
        toggle ("choke", "Mono (choke)", false), hz ("hp", "Low cut", 20.0f, 2000.0f, 20.0f),
    };
    jassert ((int) e.params.size() == sCount);
    e.pages = {
        page ("SAMPLE", { sSample, sMode, sRoot, sStart, sEnd, sLoop, sLoopStart, sTune, sFine, sReverse, sSlices, sVolume,
                          sA, sD, sS, sR, sCutoff, sReso, sDrive, sCrush, sGlide, sVelAmp, sSpread, sWidth }),
        page ("FILTER", { sFType, sCutoff, sReso, sEnvAmt, sFA, sFD, sFS, sFR, sKeyTrack, sDrive, sDown, sHp,
                          sPitchEnv, sPitchDecay, sChoke, sCrush }),
    };
    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, const char* cat, V v) { e.presets.push_back ({ name, cat, std::move (v) }); };
    add ("Vox Keys", "Vocal", { { "sample", 0 }, { "loop", 2 }, { "loopstart", 0.35f }, { "a", 0.05f }, { "r", 0.6f } });
    add ("Choir Ooh Keys", "Vocal", { { "sample", 6 }, { "loop", 2 }, { "loopstart", 0.3f }, { "a", 0.2f }, { "r", 1.0f } });
    add ("Vox Chop", "Vocal", { { "sample", 0 }, { "mode", 1 }, { "slices", 8 }, { "a", 0.003f }, { "r", 0.05f } });
    add ("Sampled Piano", "Keys", { { "sample", 1 }, { "s", 0.0f }, { "d", 4.0f }, { "r", 0.4f } });
    add ("Lofi Piano Sample", "Keys", { { "sample", 1 }, { "s", 0.0f }, { "d", 4.0f }, { "crush", 0.4f }, { "down", 0.3f }, { "cutoff", 4000 } });
    add ("Reverse Piano", "Keys", { { "sample", 1 }, { "reverse", 1 }, { "start", 0.3f }, { "a", 0.01f } });
    add ("Sampled Bell", "Bell", { { "sample", 2 }, { "root", 72 }, { "r", 1.5f } });
    add ("Pad Sample", "Pad", { { "sample", 3 }, { "root", 48 }, { "loop", 2 }, { "loopstart", 0.3f }, { "a", 0.5f }, { "r", 1.5f } });
    add ("Reverse Pad", "Pad", { { "sample", 3 }, { "root", 48 }, { "reverse", 1 }, { "a", 0.3f }, { "r", 1.0f } });
    add ("Break Chop", "Drums", { { "sample", 4 }, { "mode", 1 }, { "slices", 16 }, { "choke", 1 }, { "r", 0.03f } });
    add ("Break 8 Slices", "Drums", { { "sample", 4 }, { "mode", 1 }, { "slices", 8 }, { "choke", 1 }, { "r", 0.05f } });
    add ("Lofi Break", "Drums", { { "sample", 4 }, { "mode", 1 }, { "slices", 16 }, { "crush", 0.5f }, { "down", 0.4f }, { "choke", 1 } });
    add ("Kick Pitched", "Drums", { { "sample", 5 }, { "mode", 2 }, { "root", 36 } });
    add ("808 Bass", "Bass", { { "sample", 5 }, { "root", 36 }, { "glide", 0.06f }, { "choke", 1 }, { "drive", 0.3f } });
    add ("Sampled Pluck", "Pluck", { { "sample", 7 }, { "r", 0.3f } });
    add ("Init", "Init", {});
    return e;
}

namespace
{
class SamplerEngine : public Engine
{
public:
    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        for (auto& v : voices) v = Voice();
        volume.setTime (0.02f, sr);
    }

    void setParams (const float* v) override
    {
        for (int i = 0; i < sCount; ++i) p[(size_t) i] = v[i];
        for (auto& vo : voices) { vo.amp.set (p[sA], p[sD], p[sS], p[sR], sr); vo.fenv.set (p[sFA], p[sFD], p[sFS], p[sFR], sr); }
    }
    void setBend (float s) override { bend = s; }
    void setMod (float m) override { mod = m; }

    void noteOn (int note, float vel, bool slide, bool) override
    {
        const Sample* smp = SampleBank::get().at (juce::roundToInt (p[sSample]));
        if (smp == nullptr || smp->length() < 16) return;
        const int mode = juce::roundToInt (p[sMode]);
        const bool mono = p[sChoke] > 0.5f;
        Voice* v = nullptr;
        if (mono)
        {
            v = &voices[0];
            for (int i = 1; i < (int) voices.size(); ++i) voices[(size_t) i].amp.gateOff();
        }
        else
        {
            for (auto& x : voices) if (x.active && x.note == note) v = &x;
            if (v == nullptr) for (auto& x : voices) if (! x.active) { v = &x; break; }
            if (v == nullptr) { uint64_t best = ~(uint64_t) 0; for (auto& x : voices) if (x.age < best) { best = x.age; v = &x; } }
        }
        if (v == nullptr) return;
        const bool legato = mono && v->active && v->gate && (slide || p[sGlide] > 0.001f) && mode == ModeKeys;
        if (v->active && ! legato) { declickL += v->lastL; declickR += v->lastR; }   // the old sound fades instead of jumping
        const int len = smp->length();
        double a = juce::jlimit (0.0, 1.0, (double) p[sStart]) * len, b = juce::jlimit (0.0, 1.0, (double) p[sEnd]) * len;
        if (b < a + 16) b = juce::jmin ((double) len, a + 16.0);
        v->smp = smp;
        v->note = note;
        v->target = mode == ModeChop ? (float) (60 + p[sTune]) : (float) note;   // a slice plays at its own pitch (+ tune)
        if (! legato) v->pitch = v->target;
        if (mode == ModeChop)
        {
            const int slices = juce::jlimit (2, 16, juce::roundToInt (p[sSlices]));
            const int idx = ((note - 36) % slices + slices) % slices;
            const double w = (b - a) / slices;
            v->from = a + w * idx;
            v->to = v->from + w;
        }
        else { v->from = a; v->to = b; }
        v->reverse = p[sReverse] > 0.5f;
        if (! legato)
        {
            v->pos = v->reverse ? v->to - 1.0 : v->from;
            v->dir = v->reverse ? -1.0 : 1.0;
            v->vel = vel;
            v->amp.set (p[sA], p[sD], p[sS], p[sR], sr);
            v->fenv.set (p[sFA], p[sFD], p[sFS], p[sFR], sr);
            v->amp.gateOn (true); v->fenv.gateOn (true);
            v->pitchEnv = 1.0f;
            v->pan = ((float) (counter % 5) - 2.0f) * 0.25f * p[sSpread];
        }
        v->gate = true;
        v->active = true;
        v->age = ++counter;
    }

    void noteOff (int note) override
    {
        if (juce::roundToInt (p[sMode]) == ModeOneShot) return;   // plays to its end
        for (auto& v : voices) if (v.active && v.gate && v.note == note) { v.gate = false; v.amp.gateOff(); v.fenv.gateOff(); }
    }

    void allOff() override { for (auto& v : voices) { v.amp.kill(); v.active = v.gate = false; } }

    void render (float* outL, float* outR, int n) override
    {
        const float vol = dsp::dbToGain (p[sVolume]);
        const int loop = juce::roundToInt (p[sLoop]);
        const int mode = juce::roundToInt (p[sMode]);
        const int ftype = juce::roundToInt (p[sFType]);
        const bool filt = p[sCutoff] < 19500.0f || std::abs (p[sEnvAmt]) > 0.01f || ftype != 0;
        const float glideK = p[sGlide] > 0.001f ? std::exp (-1.0f / (p[sGlide] * sr)) : 0.0f;
        const float pdk = std::exp (-1.0f / juce::jmax (1.0f, p[sPitchDecay] * sr));
        const float crushBits = p[sCrush] > 0.001f ? std::exp2 (14.0f - 10.0f * p[sCrush]) : 0.0f;
        const int down = 1 + (int) (p[sDown] * p[sDown] * 12.0f);
        const float drive = 1.0f + p[sDrive] * 6.0f;
        const float dk = std::exp (-1.0f / (0.003f * sr));
        for (int i0 = 0; i0 < n; i0 += 16)
        {
            const int m = juce::jmin (16, n - i0);
            float bl[16] {}, br[16] {};
            for (auto& v : voices)
            {
                if (! v.active || v.smp == nullptr) continue;
                const auto* s = v.smp;
                const double loopA = v.from + (v.to - v.from) * juce::jlimit (0.0f, 0.98f, p[sLoopStart]);
                if (filt)
                {
                    const float oct = p[sEnvAmt] * v.fenv.value * 7.0f + p[sKeyTrack] * (v.pitch - 60.0f) / 12.0f + mod * 2.0f;
                    const float cut = juce::jlimit (30.0f, sr * 0.45f, p[sCutoff] * std::exp2 (oct));
                    v.fl.rampTo (cut, 0.6f + p[sReso] * 10.0f, sr, m); v.fr.rampTo (cut, 0.6f + p[sReso] * 10.0f, sr, m);
                }
                v.hpl.set (p[sHp], 0.7f, sr); v.hpr.set (p[sHp], 0.7f, sr);
                const float velGain = 1.0f - p[sVelAmp] + p[sVelAmp] * v.vel;
                const float gl = std::sqrt (0.5f * (1.0f - v.pan)), gr = std::sqrt (0.5f * (1.0f + v.pan));
                for (int i = 0; i < m; ++i)
                {
                    v.pitch = glideK > 0.0f ? v.target + (v.pitch - v.target) * glideK : v.target;
                    v.pitchEnv *= pdk;
                    const float root = mode == ModeChop ? 60.0f : p[sRoot];
                    const double rate = std::exp2 ((v.pitch - root + (mode == ModeChop ? 0.0f : p[sTune]) + p[sFine] * 0.01f + bend + v.pitchEnv * p[sPitchEnv]) / 12.0)
                                      * s->sampleRate / sr;
                    const float a = v.amp.process();
                    v.fenv.process();
                    // read (linear) and move
                    const int i1 = (int) v.pos;
                    const double frac = v.pos - i1;
                    const int i2 = juce::jmin (s->length() - 1, i1 + 1);
                    const int ic = juce::jlimit (0, s->length() - 1, i1);
                    float l = (float) (s->l[(size_t) ic] + (s->l[(size_t) i2] - s->l[(size_t) ic]) * frac);
                    float r = (float) (s->r[(size_t) ic] + (s->r[(size_t) i2] - s->r[(size_t) ic]) * frac);
                    v.pos += rate * v.dir;
                    bool ended = false;
                    const bool loops = loop > 0 && mode == ModeKeys;
                    if (v.dir > 0.0 && v.pos >= v.to - 1.0)
                    {
                        if (loops && loop == 1) v.pos = loopA + (v.pos - (v.to - 1.0));
                        else if (loops) { v.dir = -1.0; v.pos = v.to - 1.0; }
                        else ended = true;
                    }
                    else if (v.dir < 0.0 && v.pos <= (loops ? loopA : v.from))
                    {
                        if (loops && ! v.reverse) { v.dir = 1.0; v.pos = loopA; }
                        else if (loops) v.pos = v.to - 1.0;
                        else ended = true;
                    }
                    // a few ms fade at the very end of the sample (no click)
                    const double left = v.dir > 0.0 ? (v.to - v.pos) : (v.pos - v.from);
                    const float edge = loops ? 1.0f : (float) juce::jlimit (0.0, 1.0, left / (0.003 * s->sampleRate));
                    if (filt)
                    {
                        if (ftype == 0) { l = v.fl.process (l).low; r = v.fr.process (r).low; }
                        else if (ftype == 1) { l = v.fl.process (l).band; r = v.fr.process (r).band; }
                        else { l = v.fl.process (l).high; r = v.fr.process (r).high; }
                    }
                    l = v.hpl.process (l).high; r = v.hpr.process (r).high;
                    const float g = a * velGain * edge;
                    v.lastL = l * g * gl; v.lastR = r * g * gr;
                    bl[i] += v.lastL; br[i] += v.lastR;
                    if (ended || ! v.amp.active()) { v.active = v.gate = false; v.lastL = v.lastR = 0.0f; break; }
                }
            }
            for (int i = 0; i < m; ++i)
            {
                float l = bl[i] + declickL, r = br[i] + declickR;
                declickL *= dk; declickR *= dk;
                if (drive > 1.01f) { const float k = 1.0f / std::sqrt (drive); l = dsp::fastTanh (l * drive) * k; r = dsp::fastTanh (r * drive) * k; }
                if (crushBits > 0.0f) { l = std::round (l * crushBits) / crushBits; r = std::round (r * crushBits) / crushBits; }
                if (down > 1) { if (downCount++ % down == 0) { hl = l; hr = r; } l = hl; r = hr; }
                const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * p[sWidth];
                const float g = volume.process (vol);
                outL[i0 + i] += (mid + side) * g;
                outR[i0 + i] += (mid - side) * g;
            }
        }
    }

private:
    struct Voice
    {
        const Sample* smp = nullptr;
        bool active = false, gate = false, reverse = false;
        int note = 60;
        double pos = 0.0, from = 0.0, to = 0.0, dir = 1.0;
        float vel = 0.8f, pitch = 60.0f, target = 60.0f, pitchEnv = 0.0f, pan = 0.0f, lastL = 0.0f, lastR = 0.0f;
        uint64_t age = 0;
        dsp::Adsr amp, fenv;
        dsp::Svf fl, fr, hpl, hpr;
    };
    std::array<float, sCount> p {};
    std::array<Voice, 12> voices;
    uint64_t counter = 0;
    float sr = 48000.0f, bend = 0.0f, mod = 0.0f, declickL = 0.0f, declickR = 0.0f, hl = 0.0f, hr = 0.0f;
    int downCount = 0;
    dsp::Smooth volume;
};
} // namespace

std::unique_ptr<Engine> makeSamplerEngine() { return std::make_unique<SamplerEngine>(); }

//==============================================================================
/** The first start: the starter samples, played by the box's own engines. */
void makeStarterSamples (SampleBank& bank)
{
    const double rate = 48000.0;
    auto renderNotes = [rate] (int engineType, const char* preset, std::vector<int> notes, double seconds, double holdSeconds,
                               std::vector<float>& l, std::vector<float>& r)
    {
        const auto& d = engineDef (engineType);
        int pi = 0;
        for (size_t i = 0; i < d.presets.size(); ++i) if (d.presets[i].name == preset) pi = (int) i;
        float v[maxParams];
        d.applyPreset (pi, v);
        auto e = makeEngine (engineType);
        e->prepare (rate);
        e->setParams (v);
        e->setTempo (120.0);
        const int total = (int) (seconds * rate), hold = (int) (holdSeconds * rate);
        l.assign ((size_t) total, 0.0f); r.assign ((size_t) total, 0.0f);
        for (int n : notes) e->noteOn (n, 0.85f, false, false);
        for (int pos = 0; pos < total; pos += 256)
        {
            const int m = juce::jmin (256, total - pos);
            if (pos <= hold && pos + 256 > hold) for (int n : notes) e->noteOff (n);
            e->render (l.data() + pos, r.data() + pos, m);
        }
        float peak = 0.0f;
        for (size_t i = 0; i < l.size(); ++i) peak = juce::jmax (peak, std::abs (l[i]), std::abs (r[i]));
        if (peak > 1.0e-4f) for (size_t i = 0; i < l.size(); ++i) { l[i] *= 0.9f / peak; r[i] *= 0.9f / peak; }
        for (int i = 0; i < 480 && i < total; ++i) { const float f = (float) i / 480.0f; l[(size_t) (total - 1 - i)] *= f; r[(size_t) (total - 1 - i)] *= f; }
    };
    auto breakLoop = [rate] (std::vector<float>& l, std::vector<float>& r)
    {
        const auto& d = engineDef (EngineDrums);
        int pi = 0;
        for (size_t i = 0; i < d.presets.size(); ++i) if (d.presets[i].name == "Breakbeat") pi = (int) i;
        float v[maxParams];
        d.applyPreset (pi, v);
        auto e = makeEngine (EngineDrums);
        e->prepare (rate);
        e->setParams (v);
        const int stepLen = (int) (rate * 0.125);   // 16ths at 120 BPM
        const int total = stepLen * 16;
        l.assign ((size_t) total, 0.0f); r.assign ((size_t) total, 0.0f);
        // a classic break: kick, snare, hats, a ghost note
        const int pattern[16][3] = { { 0, 3, -1 }, { 3, -1, -1 }, { 3, -1, -1 }, { 3, 0, -1 }, { 1, 3, -1 }, { 3, -1, -1 }, { 0, 3, -1 }, { 1, 3, -1 },
                                     { 3, -1, -1 }, { 0, 3, -1 }, { 0, 3, -1 }, { 3, -1, -1 }, { 1, 3, -1 }, { 3, -1, -1 }, { 4, -1, -1 }, { 1, 3, -1 } };
        for (int s = 0; s < 16; ++s)
        {
            for (int k = 0; k < 3; ++k) if (pattern[s][k] >= 0) e->noteOn (36 + pattern[s][k], pattern[s][k] == 3 ? 0.55f : 0.9f, false, false);
            e->render (l.data() + s * stepLen, r.data() + s * stepLen, stepLen);
        }
        float peak = 0.0f;
        for (size_t i = 0; i < l.size(); ++i) peak = juce::jmax (peak, std::abs (l[i]), std::abs (r[i]));
        if (peak > 1.0e-4f) for (size_t i = 0; i < l.size(); ++i) { l[i] *= 0.9f / peak; r[i] *= 0.9f / peak; }
    };

    const auto names = starterSampleNames();
    for (int i = 0; i < names.size(); ++i)
    {
        std::vector<float> l, r;
        switch (i)
        {
            case 0: renderNotes (EngineChoir, "Breathy Ahh", { 60 }, 3.0, 2.4, l, r); break;
            case 1: renderNotes (EngineKeys, "Grand Piano", { 60 }, 3.0, 2.5, l, r); break;
            case 2: renderNotes (EngineFM, "Tubular Bell", { 72 }, 4.0, 0.5, l, r); break;
            case 3: renderNotes (EngineGlide, "Warm Analog Pad", { 48 }, 4.0, 3.0, l, r); break;
            case 4: breakLoop (l, r); break;
            case 5: renderNotes (EngineDrums, "808 Trap", { 36 }, 1.5, 0.1, l, r); break;
            case 6: renderNotes (EngineChoir, "Cathedral Ooh", { 60 }, 4.0, 3.2, l, r); break;
            case 7: renderNotes (EngineAnalog, "Pluck", { 60 }, 1.5, 0.3, l, r); break;
            default: renderNotes (EngineDrums, "909 Classic", { 36 + (i - 8) }, (i - 8) == 10 || (i - 8) == 11 ? 2.0 : 1.0, 0.05, l, r); break;
        }
        bank.add (names[i], std::move (l), std::move (r), rate, true);
    }
}
} // namespace catbox

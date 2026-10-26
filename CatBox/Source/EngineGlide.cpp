#include "Engine.h"
#include "GlideData.h"
#include <cmath>

/**
    GLIDE: the Glide-Cat synth (two layers, voices that glide from chord to chord, expression,
    modulators, the matrix and its own effects) with all its parameters and its 66 sounds.
*/
namespace catbox
{
namespace
{
ParamDef fromGlide (const glidecat::ParamInfo& g)
{
    ParamDef d;
    d.id = g.id;
    d.name = g.name;
    d.lo = g.lo; d.hi = g.hi; d.def = g.def; d.skew = g.skew;
    d.choices = g.choices;
    switch (g.type)
    {
        case glidecat::ParamInfo::Choice: d.unit = ParamDef::Choice; return d;
        case glidecat::ParamInfo::Int:    d.unit = ParamDef::Num; return d;
        case glidecat::ParamInfo::Bool:   d.unit = ParamDef::Toggle; return d;
        case glidecat::ParamInfo::Float:  break;
    }
    const auto& u = g.unit;
    if (u == "%") d.unit = ParamDef::Pct;
    else if (u == "Hz") d.unit = ParamDef::Hz;
    else if (u == "s") d.unit = ParamDef::Sec;
    else if (u == "dB") d.unit = ParamDef::Db;
    else if (u == "ms" || u == "ms/st") { d.unit = ParamDef::Ms; d.suffix = " " + u; }
    else { d.unit = ParamDef::Plain; d.suffix = u.isEmpty() ? juce::String() : " " + u; }
    return d;
}

/** The order readSettings asks for the parameters (it always asks in the same order): index into params(). */
const std::vector<int>& readOrder()
{
    static const std::vector<int> order = []
    {
        std::vector<int> o;
        glidecat::Settings s; glidecat::FxSettings f;
        glidecat::readSettings ([&o] (const char* id) { o.push_back (glidecat::paramIndex (id)); return 0.0f; }, s, f);
        return o;
    }();
    return order;
}
}

EngineDef makeGlideDef()
{
    EngineDef e;
    e.name = "Glide";
    for (const auto& g : glidecat::params()) e.params.push_back (fromGlide (g));
    e.params.push_back (defs::num ("octave", "Octave", -2, 2, 0));   // Cat Box's own: the whole sound up / down
    readOrder();   // built here (message thread), used on the audio thread

    auto page = [&e] (const char* name, std::initializer_list<const char*> ids)
    {
        KnobPage p;
        p.name = name;
        int i = 0;
        for (auto* id : ids) { if (i < 24) p.slot[(size_t) i] = e.index (id); ++i; }
        e.pages.push_back (p);
    };
    page ("SOUND", { "voices", "volume", "l1_lpf", "l1_lpRes", "l1_attack", "l1_release", "glideMs", "swell", "brighten", "drift", "vibDepth", "octave",
                     "ring", "panSpread", "l2_lpf", "l2_lpRes", "l2_attack", "l2_release", "curve", "swellTime", "wander", "vibRate", "roomMix", "echoMix" });
    page ("OSC", { "l1_on", "l1_feet", "l1_unison", "l1_saw", "l1_pulse", "l1_pw", "l1_pwmRate", "l1_pwm", "l1_sine", "l1_noise", "l1_detune", "l1_fine",
                   "l2_on", "l2_feet", "l2_unison", "l2_saw", "l2_pulse", "l2_pw", "l2_pwmRate", "l2_pwm", "l2_sine", "l2_noise", "l2_detune", "l2_fine" });
    page ("FILTER", { "l1_hpf", "l1_hpRes", "l1_lpf", "l1_lpRes", "l1_key", "l1_egDepth", "l1_egIL", "l1_egAL", "l1_egAtk", "l1_egDec", "l1_egRel", "l1_brill",
                      "l2_hpf", "l2_hpRes", "l2_lpf", "l2_lpRes", "l2_key", "l2_egDepth", "l2_egIL", "l2_egAL", "l2_egAtk", "l2_egDec", "l2_egRel", "l2_brill" });
    page ("AMP", { "l1_attack", "l1_decay", "l1_sustain", "l1_release", "l1_level", "voices", "l2_attack", "l2_decay", "l2_sustain", "l2_release", "l2_level", "volume",
                   "swell", "swellTime", "brighten", "drift", "wander", "ring", "vibDepth", "vibRate", "vibDelay", "panSpread", "bendRange" });
    page ("GLIDE", { "glideMode", "glideMs", "rateMs", "curve", "glideSpread", "leading", "newVoices", "merge", "staggerMs", "staggerOrd", "path", "pathDepth",
                     "startFrom", "startDepth", "releaseBend", "lineMode", "lineMemory", "bendRange" });
    page ("MOD", { "m1Type", "m1Mode", "m1Rate", "m2Type", "m2Mode", "m2Rate", "m3Type", "m3Mode", "m3Rate", "m4Type", "m4Mode", "m4Rate",
                   "mx1Src", "mx1Dst", "mx1Amt", "mx2Src", "mx2Dst", "mx2Amt", "mx3Src", "mx3Dst", "mx3Amt", "mx4Src", "mx4Dst", "mx4Amt" });
    page ("FX", { "ensMode", "ensMix", "ensRate", "ensDepth", "prismMix", "prismTime", "prismFb", "roomType", "roomMix", "roomSize", "drive", "driveTone",
                  "echoMix", "echoTime", "echoFb", "echoTone", "tapeWow", "tapeFlutter", "tapeAge", "prismI1", "prismI2", "prismI3" });

    for (const auto& sp : glidecat::soundPresets())
        e.presets.push_back ({ sp.name, sp.category, sp.values });
    e.presets.push_back ({ "Init", "Init", {} });
    return e;
}

namespace
{
class GlideEngine : public Engine
{
public:
    GlideEngine()
    {
        chordNotes.reserve (4);
        chordVel.reserve (4);
    }

    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        synth.prepare (sampleRate, block);
        fx.prepare (sampleRate, block);
    }

    void setParams (const float* v) override
    {
        const auto& order = readOrder();
        struct Reader { const float* v; const std::vector<int>* order; size_t k; } rd { v, &order, 0 };
        glidecat::readSettings ([r = &rd] (const char*) -> float
                                {
                                    const int i = r->k < r->order->size() ? (*r->order)[r->k] : -1;
                                    ++r->k;
                                    return i >= 0 ? r->v[i] : 0.0f;
                                }, settings, fxSettings);
        settings.pilot = glidecat::Pilot::Off;   // nothing plays by itself
        octave = 12 * juce::roundToInt (v[glidecat::params().size()]);
        synth.setSettings (settings);
    }

    void setTempo (double bpm) override { fxSettings.bpm = bpm; }
    void setBend (float semis) override { synth.pitchBend (1, juce::jlimit (-1.0f, 1.0f, semis / juce::jmax (0.01f, settings.bendRange))); }
    void setMod (float m) override { synth.modWheel (m); }

    void noteOn (int note, float vel, bool, bool accent) override
    {
        const int played = juce::jlimit (0, 127, note);
        note = juce::jlimit (0, 127, note + octave);
        mapped[(size_t) played] = (int8_t) note;   // its own note off, even when the octave changes meanwhile
        chordNotes.assign (1, { 1, note });
        chordVel.assign (1, juce::jlimit (0.01f, 1.0f, vel * (accent ? 1.2f : 1.0f)));
        synth.notesOn (chordNotes, chordVel);
        tail = (int) (sr * 10.0f);
    }

    void noteOff (int note) override
    {
        note = juce::jlimit (0, 127, note);
        synth.noteOff (1, mapped[(size_t) note] >= 0 ? mapped[(size_t) note] : note);
        mapped[(size_t) note] = -1;
    }
    void allOff() override { synth.allNotesOff(); }

    void render (float* outL, float* outR, int n) override
    {
        if (tail <= 0) return;
        if (synth.numSounding() == 0) tail -= n;
        for (int s0 = 0; s0 < n; s0 += block)
        {
            const int m = juce::jmin (block, n - s0);
            std::fill (bl, bl + m, 0.0f);
            std::fill (br, br + m, 0.0f);
            synth.render (bl, br, m);
            fx.process (bl, br, m, fxSettings);
            for (int i = 0; i < m; ++i) { outL[s0 + i] += bl[i] * settings.volume; outR[s0 + i] += br[i] * settings.volume; }
        }
    }

private:
    static constexpr int block = 256;
    glidecat::Synth synth;
    glidecat::Effects fx;
    glidecat::Settings settings;
    glidecat::FxSettings fxSettings;
    std::vector<std::pair<int, int>> chordNotes;
    std::vector<float> chordVel;
    float sr = 48000.0f;
    int tail = 0, octave = 0;
    std::array<int8_t, 128> mapped = [] { std::array<int8_t, 128> a {}; a.fill (-1); return a; }();
    float bl[block] {}, br[block] {};
};
} // namespace

std::unique_ptr<Engine> makeGlideEngine() { return std::make_unique<GlideEngine>(); }
} // namespace catbox

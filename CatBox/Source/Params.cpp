#include "Params.h"
#include <cmath>

namespace catbox
{
EngineDef makeAnalogDef();
EngineDef makeAcidDef();
EngineDef makeDrumsDef();
EngineDef makeKeysDef();
EngineDef makeGlideDef();
EngineDef makeFMDef();
EngineDef makeWavetableDef();
EngineDef makeChoirDef();
EngineDef makeSamplerDef();
EngineDef makeGranularDef();

juce::String ParamDef::format (float v) const
{
    if (formatFn) return formatFn (v);
    switch (unit)
    {
        case Choice: return choices[juce::jlimit (0, choices.size() - 1, juce::roundToInt (v))];
        case Toggle: return v > 0.5f ? "On" : "Off";
        case Num:    return juce::String (juce::roundToInt (v));
        case Semi:   { const int s = juce::roundToInt (v); return (s > 0 ? "+" : "") + juce::String (s) + " st"; }
        case Cents:  { const int c = juce::roundToInt (v); return (c > 0 ? "+" : "") + juce::String (c) + " ct"; }
        case Pct:    return juce::String (juce::roundToInt (v * 100.0f)) + "%";
        case BiPct:  { const int p = juce::roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + juce::String (p) + "%"; }
        case Hz:     return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                                         : (v < 10.0f ? juce::String (v, 2) : juce::String (juce::roundToInt (v))) + " Hz";
        case Sec:    return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";
        case Db:     return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
        case Ms:     return juce::String (juce::roundToInt (v)) + suffix;
        case Plain:  return juce::String (v, std::abs (v) >= 100.0f ? 0 : 2) + suffix;
    }
    return juce::String (v, 2);
}

float ParamDef::toNorm (float v) const noexcept
{
    if (hi <= lo) return 0.0f;
    const float n = juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo));
    return skew != 1.0f ? std::pow (n, skew) : n;
}

float ParamDef::fromNorm (float n) const noexcept
{
    n = juce::jlimit (0.0f, 1.0f, n);
    if (skew != 1.0f && n > 0.0f) n = std::pow (n, 1.0f / skew);
    return clamp (lo + (hi - lo) * n);
}

float ParamDef::clamp (float v) const noexcept
{
    v = juce::jlimit (lo, hi, v);
    return stepped() ? (float) juce::roundToInt (v) : v;
}

int EngineDef::index (const juce::String& id) const
{
    for (size_t i = 0; i < params.size(); ++i)
        if (params[i].id == id) return (int) i;
    return -1;
}

void EngineDef::defaults (float* v) const
{
    for (int i = 0; i < maxParams; ++i) v[i] = i < (int) params.size() ? params[(size_t) i].def : 0.0f;
}

void EngineDef::applyPreset (int preset, float* v) const
{
    defaults (v);
    if (presets.empty()) return;
    for (const auto& [id, x] : presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, preset)].values)
        if (const int i = index (id); i >= 0) v[i] = params[(size_t) i].clamp (x);
}

juce::StringArray EngineDef::categories() const
{
    juce::StringArray c;
    for (const auto& p : presets) c.addIfNotAlreadyThere (p.category);
    return c;
}

std::vector<int> EngineDef::presetsIn (const juce::String& category) const
{
    std::vector<int> out;
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].category == category) out.push_back ((int) i);
    return out;
}

const EngineDef& engineDef (int type)
{
    static const std::array<EngineDef, NumEngineTypes> all = []
    {
        std::array<EngineDef, NumEngineTypes> a { makeAnalogDef(), makeAcidDef(), makeDrumsDef(), makeKeysDef(), makeGlideDef(), makeFMDef(), makeWavetableDef(), makeChoirDef(), makeSamplerDef(), makeGranularDef() };
        for (int i = 0; i < NumEngineTypes; ++i) addExtraPresets (a[(size_t) i], i);
        return a;
    }();
    return all[(size_t) juce::jlimit (0, NumEngineTypes - 1, type)];
}

juce::StringArray engineNames()
{
    juce::StringArray n;
    for (int i = 0; i < NumEngineTypes; ++i) n.add (engineDef (i).name);
    return n;
}

juce::StringArray syncNames() { return { "Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4 T", "1/8 T", "1/16 T", "1/8 .", "2 bars", "4 bars" }; }

double syncBeats (int c)
{
    static const double b[] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 0.75, 8.0, 16.0 };
    return b[juce::jlimit (0, 11, c)];
}

namespace defs
{
    static ParamDef make (const char* id, const char* name, ParamDef::Unit u, float lo, float hi, float def, float skew = 1.0f)
    {
        ParamDef p; p.id = id; p.name = name; p.unit = u; p.lo = lo; p.hi = hi; p.def = def; p.skew = skew; return p;
    }
    ParamDef pct (const char* id, const char* name, float def)  { return make (id, name, ParamDef::Pct, 0.0f, 1.0f, def); }
    ParamDef bi (const char* id, const char* name, float def)   { return make (id, name, ParamDef::BiPct, -1.0f, 1.0f, def); }
    ParamDef hz (const char* id, const char* name, float lo, float hi, float def) { return make (id, name, ParamDef::Hz, lo, hi, def, 0.3f); }
    ParamDef sec (const char* id, const char* name, float lo, float hi, float def) { return make (id, name, ParamDef::Sec, lo, hi, def, 0.35f); }
    ParamDef db (const char* id, const char* name, float lo, float hi, float def) { return make (id, name, ParamDef::Db, lo, hi, def); }
    ParamDef semi (const char* id, const char* name, float lo, float hi, float def) { return make (id, name, ParamDef::Semi, lo, hi, def); }
    ParamDef cents (const char* id, const char* name, float lo, float hi, float def) { return make (id, name, ParamDef::Cents, lo, hi, def); }
    ParamDef num (const char* id, const char* name, float lo, float hi, float def) { return make (id, name, ParamDef::Num, lo, hi, def); }
    ParamDef choice (const char* id, const char* name, juce::StringArray items, int def)
    {
        auto p = make (id, name, ParamDef::Choice, 0.0f, (float) juce::jmax (0, items.size() - 1), (float) def);
        p.choices = std::move (items);
        return p;
    }
    ParamDef toggle (const char* id, const char* name, bool def) { return make (id, name, ParamDef::Toggle, 0.0f, 1.0f, def ? 1.0f : 0.0f); }
    KnobPage page (const char* name, std::initializer_list<int> slots)
    {
        KnobPage p;
        p.name = name;
        int i = 0;
        for (int s : slots) if (i < 24) p.slot[(size_t) i++] = s;
        return p;
    }
}
} // namespace catbox

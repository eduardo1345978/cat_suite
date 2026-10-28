#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <functional>

/**
    Cat Box 2: what every engine can be told. An engine has a list of parameters, knob pages
    (12 knobs, 12 more with Shift) and factory presets in categories.
*/
namespace catbox
{
constexpr int maxParams = 176;   // the largest engine (Glide) has 164

struct ParamDef
{
    enum Unit { Pct, BiPct, Hz, Sec, Db, Semi, Cents, Num, Choice, Toggle, Ms, Plain };
    juce::String id, name, suffix;   // suffix: for Ms / Plain (" ms", " st" ...)
    Unit unit = Pct;
    float lo = 0.0f, hi = 1.0f, def = 0.0f, skew = 1.0f;
    juce::StringArray choices;
    std::function<juce::String (float)> formatFn;   // a text that changes while it runs (e.g. sample names)
    bool isSample = false;                          // a slot of the SampleBank (saved by the sample's name)

    bool stepped() const noexcept { return unit == Choice || unit == Toggle || unit == Num || unit == Semi; }
    juce::String format (float value) const;
    float toNorm (float value) const noexcept;
    float fromNorm (float norm) const noexcept;
    float clamp (float value) const noexcept;
};

/** One page of knobs: 24 slots (12 knobs, then 12 with Shift); a slot is a parameter index or -1. */
struct KnobPage
{
    juce::String name;
    std::array<int, 24> slot;
    KnobPage() { slot.fill (-1); }
};

struct Preset
{
    juce::String name, category;
    std::vector<std::pair<juce::String, float>> values;   // only what differs from the defaults
    std::vector<std::pair<juce::String, juce::String>> samples;   // sample parameters by the sample's name (the library)
};

struct EngineDef
{
    juce::String name;
    std::vector<ParamDef> params;
    std::vector<KnobPage> pages;
    std::vector<Preset> presets;
    bool drums = false;   // pads = 16 drum sounds, the sequencer has a lane per drum

    int index (const juce::String& id) const;
    void defaults (float* values) const;
    /** Defaults, then the preset's values. */
    void applyPreset (int preset, float* values) const;
    juce::StringArray categories() const;
    /** Presets of one category (indices into presets). */
    std::vector<int> presetsIn (const juce::String& category) const;
};

enum EngineType { EngineAnalog = 0, EngineAcid, EngineDrums, EngineKeys, EngineGlide, EngineFM, EngineWavetable, EngineChoir, EngineSampler, EngineGranular, EngineKit, EngineSynth, NumEngineTypes };
const EngineDef& engineDef (int type);
/** The "FX Lab" sounds and more, added to each engine. */
void addExtraPresets (EngineDef&, int engine);
/** The FX chain a factory sound brings (-1 = none). */
int presetChain (int engine, int preset);
juce::StringArray engineNames();

/** Helpers to write engine definitions. */
namespace defs
{
    ParamDef pct (const char* id, const char* name, float def);
    ParamDef bi (const char* id, const char* name, float def);
    ParamDef hz (const char* id, const char* name, float lo, float hi, float def);
    ParamDef sec (const char* id, const char* name, float lo, float hi, float def);
    ParamDef db (const char* id, const char* name, float lo, float hi, float def);
    ParamDef semi (const char* id, const char* name, float lo, float hi, float def);
    ParamDef cents (const char* id, const char* name, float lo, float hi, float def);
    ParamDef num (const char* id, const char* name, float lo, float hi, float def);
    ParamDef choice (const char* id, const char* name, juce::StringArray items, int def);
    ParamDef toggle (const char* id, const char* name, bool def);
    KnobPage page (const char* name, std::initializer_list<int> slots);
}

/** LFO sync choices: Free, then note lengths; the length in quarter notes (0 = free). */
juce::StringArray syncNames();
double syncBeats (int choice);
} // namespace catbox

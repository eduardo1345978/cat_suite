#pragma once

#include "GlideSynth.h"
#include "GlideFX.h"
#include <functional>
#include <vector>

/**
    Glide-Cat's parameters and factory presets, without the plug-in around them, so the
    Glide-Cat plug-in and the Cat Box (its Glide engine) share one list.
*/
namespace glidecat
{
struct ParamInfo
{
    enum Type { Float, Choice, Int, Bool };
    juce::String id, name, unit;   // unit: "%", "Hz", "ms", "ms/st", "s", "dB", "st", "ct", "oct" or ""
    Type type = Float;
    float lo = 0.0f, hi = 1.0f, def = 0.0f, skew = 1.0f;
    juce::StringArray choices;

    /** The value as text, e.g. "450 Hz", "60%", "S-Curve". */
    juce::String format (float value) const;
    /** 0..1 along the knob (with the skew) and back. */
    float toNormalised (float value) const;
    float fromNormalised (float n) const;
};

/** Every parameter, in the plug-in's order. */
const std::vector<ParamInfo>& params();
/** Index into params(), or -1. */
int paramIndex (const juce::String& id);

struct Preset
{
    const char* name;
    const char* category;
    std::vector<std::pair<juce::String, float>> values;   // only what differs from the defaults
};
const std::vector<Preset>& soundPresets();   // the sound: synth, expression, modulation, effects
const std::vector<Preset>& glidePresets();   // the glide behaviour (and the autopilot)

juce::StringArray soundParamIds();   // what a sound preset touches
juce::StringArray glideParamIds();   // what a glide preset touches

/** Builds the synth and effect settings from parameter values (get (id) = the value in the parameter's own units). */
void readSettings (const std::function<float (const char*)>& get, Settings&, FxSettings&);
} // namespace glidecat

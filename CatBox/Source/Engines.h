#pragma once

#include <juce_core/juce_core.h>
#include "GlideData.h"
#include <array>
#include <vector>

/**
    Cat Box engines: a sound page (Arp, Bass) picks its engine, and the Sound button opens
    the engine's own knob pages, like on a Teenage Engineering box. Every engine has factory
    presets that can be changed, or a plain Init sound to start from.

    The Glide engine is Glide-Cat's synth (voices that glide from note to note, two layers,
    filters, modulation and its own effects) with all its factory sounds.
*/
namespace catbox
{
constexpr int maxEngineParams = 176;

/** The Glide engine of one instrument: every Glide-Cat parameter (glidecat::params() order). */
struct EngineState
{
    std::array<float, maxEngineParams> v {};
    int sound = 0;          // factory sound preset it started from
    int motion = 0;         // factory glide preset (how the notes glide)
    bool edited = false;    // changed since the preset was loaded
    bool operator== (const EngineState& o) const { return v == o.v && sound == o.sound && motion == o.motion && edited == o.edited; }
};

namespace engine
{
    /** The Glide engine's starting sound for an instrument (Arp: keys that glide a little, Bass: a gliding bass). */
    EngineState glideDefaults (int instrument);
    void initGlide (EngineState&);                       // a plain sound to start from
    void loadGlideSound (EngineState&, int preset);      // the glide behaviour stays
    void loadGlideMotion (EngineState&, int preset);     // the sound stays

    juce::StringArray glideSoundNames();
    juce::StringArray glideMotionNames();                // the glide presets that do not play by themselves

    float get (const EngineState&, const juce::String& id);
    void set (EngineState&, const juce::String& id, float value);

    /** Synth and effect settings for the audio thread (the autopilot stays off: nothing plays by itself). */
    void toSettings (const EngineState&, glidecat::Settings&, glidecat::FxSettings&);

    juce::var toVar (const EngineState&);
    void fromVar (const juce::var&, EngineState&);

    /** The knob pages of the Sound mode: 24 slots each (12 knobs, 12 more with Shift).
        "" = an unused knob; "@sound" = the page's engine, "@preset" / "@motion" = factory presets, "@init" = start from nothing. */
    struct Page { juce::String name; std::array<juce::String, 24> ids; };
    const std::vector<Page>& glidePages();
    /** A short knob name ("Saw" instead of "Layer I Saw"). */
    juce::String shortName (const glidecat::ParamInfo&);
}
} // namespace catbox

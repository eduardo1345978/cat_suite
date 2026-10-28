#pragma once

#include <juce_core/juce_core.h>

/**
    SYNTH: Cat Box's main instrument, a "digital analog" synthesizer with 10 models.
    One panel (controllers, 3 oscillators, mixer, filter, 2 envelopes, LFO, output); the model
    changes how it sounds: the filter circuit, the oscillators, saturation, drift, chorus.
*/
namespace catbox
{
enum SynthModel
{
    ModelMini = 0, ModelProphet, ModelOberheim, ModelJupiter, ModelJuno, ModelSH, ModelAcid, ModelMS, ModelOdyssey, ModelBuchla,
    NumSynthModels
};

struct SynthModelLook
{
    const char* name;        // on the panel
    const char* inspired;    // what it is modelled on
    const char* about;       // one line
    juce::uint32 panel, face, accent, ink, side;   // panel colour, knob face, accent, text, side cheeks (0 = none)
};
const SynthModelLook& synthModelLook (int model);
juce::StringArray synthModelNames();

/** The panel: sections of parameter ids, left to right (the screen draws it, the mouse turns it). */
struct SynthSection { const char* title; std::vector<const char*> ids; };
const std::vector<SynthSection>& synthPanel();
} // namespace catbox

#pragma once

#include "Params.h"
#include <memory>

namespace catbox
{
/**
    A sound engine on a track (runs on the audio thread). setParams gets the track's whole
    parameter array (in the engine's own order) whenever a knob or preset changes it.
    render() ADDS its sound to the buffers.
*/
class Engine
{
public:
    virtual ~Engine() = default;
    virtual void prepare (double sampleRate) = 0;
    virtual void setParams (const float* values) = 0;
    /** Drum engines: note 36..51 = pad 1..16. slide = played legato (the previous note still held). */
    virtual void noteOn (int note, float velocity, bool slide, bool accent) = 0;
    virtual void noteOff (int note) = 0;
    virtual void allOff() = 0;
    virtual void render (float* left, float* right, int numSamples) = 0;
    virtual void setBend (float semitones) { juce::ignoreUnused (semitones); }
    virtual void setMod (float amount) { juce::ignoreUnused (amount); }
    virtual void setTempo (double bpm) { juce::ignoreUnused (bpm); }
};

std::unique_ptr<Engine> makeEngine (int type);
} // namespace catbox

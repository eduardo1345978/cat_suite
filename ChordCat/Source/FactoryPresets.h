#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace chordcat
{
/** Built-in progressions for the preset browser, tagged by Genre / Mood / Extension. */
struct FactoryPreset
{
    juce::String name;
    juce::String genre;      // NeoSoul, RnB, LoFi, Jazz, Pop
    juce::String mood;       // Dark, Uplifting, Sad, Jazzy, Tension
    juce::String extension;  // Triads, 7ths, 9ths, 11ths, 13ths
    int keyRoot;
    juce::String scale;
    int voicing;
    std::vector<std::pair<int, juce::String>> chords; // (root pitch class, chord type id)
};

const std::vector<FactoryPreset>& getFactoryPresets();

} // namespace chordcat

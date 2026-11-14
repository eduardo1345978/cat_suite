#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace chordcat
{
struct ScaleInfo
{
    juce::String name;
    std::vector<int> intervals; // empty for the user-defined Custom scale
};

const std::vector<ScaleInfo>& getScales();    // 48 entries, last one is "Custom"
int getNumScales();
int getCustomScaleIndex();
int findScale (const juce::String& name);     // -1 if unknown
juce::StringArray getScaleNames();

/** 12-bit pitch-class mask relative to the key root. */
uint16_t getScaleMask (int scaleIndex, uint16_t customMask);

/** Absolute pitch classes of the scale, starting at the key root. */
std::vector<int> getScalePitchClasses (int keyRoot, int scaleIndex, uint16_t customMask);

bool isInScale (int pitchClass, int keyRoot, int scaleIndex, uint16_t customMask);

/** Nearest scale pitch class (ties resolve downwards). */
int quantizeToScale (int pitchClass, int keyRoot, int scaleIndex, uint16_t customMask);

/** Roman numeral of a chord within the key; chromatic chords are flagged (e.g. "bVI"). */
juce::String romanNumeral (int chordRoot, const juce::String& chordType, int keyRoot, int scaleIndex, uint16_t customMask);

} // namespace chordcat

#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace bendcat
{
/**
    Scala (.scl) tuning file parser.
    Pitch lines are cents (contain a '.') or ratios ("3/2", or "2" = 2/1).
    The last degree is the period (usually 2/1). Scale degree 0 is mapped to the
    reference MIDI note; tuningTableCents holds each note's offset from 12-TET.
*/
class ScalaParser
{
public:
    bool loadSclFile (const juce::File& file);
    bool parse (const juce::String& sclText);
    void clear();

    void setReferenceNote (int note);
    int getReferenceNote() const noexcept { return referenceNote; }

    bool isLoaded() const noexcept { return loaded; }
    const juce::String& getDescription() const noexcept { return description; }
    const juce::String& getLastError() const noexcept { return lastError; }
    const juce::String& getRawText() const noexcept { return rawText; }
    int getNumDegrees() const noexcept { return (int) degreesCents.size(); }
    const std::vector<double>& getDegreesCents() const noexcept { return degreesCents; }

    std::array<float, 128> tuningTableCents {};

private:
    void computeTable();
    static bool parsePitch (const juce::String& token, double& cents);

    std::vector<double> degreesCents;   // degree 1..N (N = period)
    juce::String description, lastError, rawText;
    int referenceNote = 60;
    bool loaded = false;
};

} // namespace bendcat

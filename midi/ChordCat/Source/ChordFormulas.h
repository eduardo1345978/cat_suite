#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace chordcat
{
enum ChordFamily
{
    FamilyMajor = 0,
    FamilyMinor,
    FamilyDominant,
    FamilyDiminished,
    FamilyAugmented,
    FamilySuspended,
    FamilyOther
};

/** One of the 80 chord types. Intervals are semitones above the root (extensions > 12). */
struct ChordTypeInfo
{
    juce::String id;          // stable identifier stored in presets ("m7b5")
    juce::String symbol;      // display suffix ("m7b5")
    std::vector<int> intervals;
    int complexity;           // 0 triad/basic, 1 seventh, 2 ninth, 3 eleventh, 4 thirteenth
    ChordFamily family;
    uint16_t pitchClassMask;  // derived
};

const std::vector<ChordTypeInfo>& getChordTypes();
int getNumChordTypes();
int findChordType (const juce::String& id);                       // -1 if unknown
const ChordTypeInfo& getChordTypeInfo (const juce::String& id);   // falls back to "maj"
std::vector<int> getChordFormula (const juce::String& id);

/** Extra tensions typed by the user, e.g. "9", "#11 b13", "add6". Returns intervals above root. */
std::vector<int> parseTensions (const juce::String& tension);

juce::String pitchClassName (int pitchClass, bool preferFlats = false);
juce::String chordDisplayName (int root, const juce::String& type, bool preferFlats = false);

struct RecognizedChord
{
    bool valid = false;
    int root = 0;
    juce::String type = "maj";
    int bassPitchClass = 0;
};

/** Identifies root and type from MIDI notes (used for Chord Track sync and MIDI import). */
RecognizedChord recognizeChord (const std::vector<int>& midiNotes);

/** Finds the chord type whose pitch-class set matches exactly (root-relative mask). -1 if none. */
int findTypeForMask (uint16_t rootRelativeMask, int preferredComplexity = -1);

} // namespace chordcat

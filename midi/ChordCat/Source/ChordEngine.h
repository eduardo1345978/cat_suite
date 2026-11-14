#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "ChordFormulas.h"
#include "Scales.h"
#include <array>
#include <vector>

namespace chordcat
{
enum VoicingModel
{
    VoicingClose = 0,
    VoicingOpen,
    VoicingSpread,
    VoicingDrop2,
    VoicingDrop4,
    VoicingWideNeoSoul,
    VoicingRootless,
    VoicingQuartal,
    VoicingCustom,
    NumVoicingModels
};

juce::StringArray getVoicingModelNames();

struct ChordDefinition
{
    int rootNote = 0;                     // 0-11
    juce::String chordType = "maj";       // id from ChordFormulas (80 types)
    int inversion = 0;                    // 0-3
    int voicingModel = VoicingClose;      // VoicingModel
    int octave = 3;                       // 2-6, Cubase numbering (C3 = MIDI 60)
    juce::String tension;                 // extra tensions, e.g. "9 #11"
    int rhythmPattern = 0;                // PatternPlayer index, 0 = hold
    float strumTimeMs = -1.0f;            // < 0 = use global strum
    std::array<int, 6> customOffsets {};  // per-voice semitone offsets (Custom editor), +-24
    bool isBorrowed = false;

    juce::String getName (bool preferFlats = false) const;
    juce::ValueTree toValueTree() const;
    static ChordDefinition fromValueTree (const juce::ValueTree&);
    bool operator== (const ChordDefinition& o) const;
};

struct ProgressionSlot
{
    ChordDefinition chord;
    bool isActive = false;
};

struct GlobalState
{
    juce::String scale = "Major";
    int scaleIndex = 0;
    int keyRoot = 0;
    uint16_t customScaleMask = 0x0AB5;    // major as starting point for the custom scale
    bool scaleLock = false;
    int swing = 50;                       // 50-75 %
    juce::String groove = "Off";
    bool chordTrackSync = false;
    int transpose = 0;                    // +-24
};

using Progression = std::array<ProgressionSlot, 16>;

/**
    Voicing + voice-leading core.
    getVoicedNotes(): formula -> voicing model -> all inversions x octave shifts ->
    voice-leading cost against the previous chord -> blend with the user's chosen
    inversion by voiceLeadingStrength -> custom offsets -> MIDI notes.
*/
class ChordEngine
{
public:
    ChordEngine();

    Progression progression;
    int currentSlot = -1;
    std::vector<int> lastPlayedNotes;
    float voiceLeadingStrength = 0.5f;   // 0..1

    std::vector<int> getVoicedNotes (const ChordDefinition& chord, const std::vector<int>& previousNotes) const;

    static int calculateVoiceLeadingCost (const std::vector<int>& candidate, const std::vector<int>& previous);
    static std::vector<int> getChordFormula (const juce::String& type);
    static std::vector<int> getFullFormula (const ChordDefinition& chord);    // formula + tensions
    static std::vector<int> applyVoicingModel (const std::vector<int>& formula, int model, ChordFamily family);
    static std::vector<int> invert (std::vector<int> voicing, int inversion);

    /** Snaps a chord to the key (root and quality) when Scale Lock is on. */
    static ChordDefinition applyScaleLock (const ChordDefinition& chord, const GlobalState& global);

    /** Builds a diatonic chord on a scale degree with the given number of stacked thirds (3-7). */
    static ChordDefinition buildDiatonicChord (int degree, int numTones, int keyRoot, int scaleIndex, uint16_t customMask);

    /** Resolves scale lock + transpose and voices against the last chord; updates lastPlayedNotes. */
    std::vector<int> voiceSlot (int slot, const GlobalState& global);
    std::vector<int> voiceChord (const ChordDefinition& chord, const GlobalState& global, bool updateHistory);

    /** Voiced notes for display, without touching voice-leading history. */
    std::vector<int> previewSlot (int slot, const GlobalState& global, const std::vector<int>& previous) const;

    void resetVoiceLeading() { lastPlayedNotes.clear(); }

    int getNumActiveSlots() const;

    juce::ValueTree progressionToValueTree() const;
    void progressionFromValueTree (const juce::ValueTree& tree);

    static inline const juce::Identifier progressionId { "PROGRESSION" };
};

} // namespace chordcat

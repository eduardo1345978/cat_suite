#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

/**
    Micro-Cat's tunings: a scale is a list of pitches in cents above its first note, the last one being
    the period (1200 = an octave; Bohlen-Pierce repeats at the tritave, 1901.96 cents). Like a Scala file.

    Keys map to the scale in one of two ways:
      - one key per step: every next key is the next step (19-EDO: 19 keys per octave, like Ableton)
      - 12-key layout: the 12 keys of an octave keep their names and are only moved by some cents
        (just intonation, meantone, maqam ... a C is still played on C)
*/
namespace micro
{
enum Category { CatEqual = 0, CatJust, CatWorld, CatMine, NumCategories };
enum Mapping { MapPerStep = 0, MapTwelve = 1 };

struct Tuning
{
    juce::String name, category, description;
    std::vector<double> cents;          // steps above the first note; the last = the period
    int mapping = MapPerStep;
    int categoryId = CatEqual;
    /** From an Ableton .ascl file: the frequency of the first note (degree 0) and the note names. 0 = not set. */
    double referenceHz = 0.0;
    juce::StringArray noteNames;

    int notesPerPeriod() const noexcept { return (int) cents.size(); }
    double period() const noexcept { return cents.empty() ? 1200.0 : cents.back(); }
    /** Cents of a degree above the first note (0 = the first note, notesPerPeriod() = the period). */
    double degreeCents (int degree) const noexcept;
    bool isValid (juce::String* why = nullptr) const;
};

/** Every built-in tuning (equal, just & historical, world). */
const std::vector<Tuning>& library();
juce::StringArray categoryNames();

/** Scala .scl text <-> a tuning ("Name", "description" lines, then pitches in cents or ratios).
    Ableton .ascl files are Scala files with "! @ABL ..." lines: REFERENCE_PITCH (octave, degree, Hz) sets
    referenceHz, NOTE_NAMES the names; the range lines are ignored (Micro-Cat centres on the root key). */
bool parseScl (const juce::String& text, Tuning& out, juce::String* error = nullptr);
juce::String toScl (const Tuning&);
/** Where an .ascl file's reference pitch puts the root: the MIDI key nearest its first note (around middle C)
    and that note's frequency. false = the tuning has no reference pitch. */
bool rootFromReference (const Tuning&, int& rootKey, double& rootHz);
/** A pitch in a .scl file: "386.31" (cents, has a dot), "5/4" or "2" (a ratio). */
bool parsePitch (const juce::String& token, double& cents);

/** My tunings (saved in <user app data>/CatSuite/Micro-Cat/Tunings, as .scl files). */
juce::File userFolder();
std::vector<Tuning> loadUserTunings();
bool saveUserTuning (const Tuning&);

//==============================================================================
/** A tuning in a fixed-size form the audio thread can copy without allocating. */
struct TuningData
{
    static constexpr int maxNotes = 128;
    std::array<double, maxNotes> cents {};
    int count = 12;
    int mapping = MapTwelve;
    static TuningData from (const Tuning&);
};

/** Where every MIDI key lands: the nearest 12-TET note to play and the pitch bend (in semitones) that
    moves it to the tuned pitch. valid = false: the pitch is out of the MIDI range (the key stays silent). */
struct KeyPitch { int note = 60; double bend = 0.0; double hz = 261.63; bool valid = true; };

/** rootKey sounds at rootHz; the scale goes up and down from there. */
KeyPitch pitchFor (const TuningData&, int key, int rootKey, double rootHz) noexcept;
} // namespace micro

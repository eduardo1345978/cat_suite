#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

/**
    Bass-Cat engine: bass lines written as chord degrees, so they follow any chord you hold.

    A style model (rhythm probabilities, degree habits, note lengths, ghost notes, slides and
    feel) generates a 1-bar core that is varied over 2 or 4 bars with an optional fill. At play
    time each degree is resolved against the chord that is held right now.
*/
namespace basscat
{
enum Degree : int
{
    Root = 0, Fifth, Octave, Third, Seventh, Approach, WalkUp, WalkDown, Ghost, NumDegrees
};

juce::StringArray getDegreeNames();     // Root, 5th, Oct, 3rd, 7th, Appr, Up, Down, Ghost
juce::StringArray getStyleNames();

enum Style
{
    PopRoots = 0, Walking, NeoSoul, Funk, BoomBap, Dilla, Trap808, Reggae, House, DiscoOctaves, Latin, Drone, NumStyles
};

struct BassNote
{
    double pos = 0.0;          // ppq from the pattern start
    double len = 0.5;          // ppq
    int degree = Root;
    int octave = 0;            // extra octaves on top of the degree
    int velocity = 100;
    bool slide = false;        // legato into the next note (808 glide / slide)
    double micro = 0.0;        // ppq: hand-played timing
};

struct Pattern
{
    int bars = 2;
    std::vector<BassNote> notes;
    double lengthPpq() const { return bars * 4.0; }

    static inline const juce::Identifier treeId { "BASSPATTERN" };
    juce::ValueTree toValueTree() const;
    static Pattern fromValueTree (const juce::ValueTree&);
};

struct GeneratorSettings
{
    int style = PopRoots;
    int bars = 2;
    float density = 0.5f;      // 0..1
    float fills = 0.35f;       // walk-ups / turnarounds at the end of the phrase
    float ghosts = 0.3f;       // dead notes between the main notes
    float slides = 0.3f;       // slides / glides
    int64_t seed = 1;
};

Pattern generatePattern (const GeneratorSettings&);

struct StyleHints { float swing; float human; };
StyleHints getStyleHints (int style);

//==============================================================================
/** The chord being played, analysed for the bass. */
struct Chord
{
    bool valid = false;
    int rootPc = 0;            // pitch class of the detected root
    int third = 4, fifth = 7, seventh = -1;   // intervals above the root (-1 = not in the chord)
    bool hasThird = false;
};

/** Finds the root of a set of held notes (works with inversions and extended chords). */
Chord analyseChord (const std::vector<int>& heldNotes);

/** MIDI note for a degree: the root sits in [lowNote, lowNote + 11]. prev = last note played (or -1). */
int resolveDegree (int degree, int octave, const Chord& chord, int lowNote, int prev);

} // namespace basscat

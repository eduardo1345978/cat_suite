#pragma once

#include "ChordEngine.h"
#include <juce_core/juce_core.h>

namespace chordcat
{
enum class Mood  { Dark = 0, Uplifting, Sad, Jazzy, Tension };
enum class Genre { NeoSoul = 0, RnB, LoFi, Jazz, Pop };

juce::StringArray getMoodNames();
juce::StringArray getGenreNames();
juce::StringArray getComplexityNames();   // Triads, 7ths, 9ths, 11ths, 13ths

struct GeneratorSettings
{
    int mood = (int) Mood::Jazzy;
    int genre = (int) Genre::NeoSoul;
    int complexity = 2;                    // 0 triads .. 4 thirteenths
    int keyRoot = 0;
    int scaleIndex = 0;
    uint16_t customMask = 0x0AB5;
    bool moodSetsScale = true;
    int numChords = 8;                     // 8-bar progression, one chord per bar
    juce::int64 seed = 0;                  // 0 = random
};

struct GeneratorResult
{
    Progression progression;
    int scaleIndex = 0;                    // scale actually used (mood may change it)
};

/**
    Markov-chain progression generator.
    Transition tables are learned at start-up from small corpora of typical
    Neo-Soul / R&B / Lo-Fi / Jazz / Pop progressions (scale-degree bigrams), then
    re-weighted by mood. States: the 7 diatonic degrees plus bII, bIII, bVI, bVII,
    V/V and V/vi. Chord extensions follow the complexity setting.
*/
class ProgressionGenerator
{
public:
    ProgressionGenerator();

    GeneratorResult generate (const GeneratorSettings& settings);

    // Dice
    static ChordDefinition diceRoot (const ChordDefinition& chord, juce::Random& rng, const GlobalState& g);
    static ChordDefinition diceType (const ChordDefinition& chord, juce::Random& rng);
    static ChordDefinition diceVoicing (const ChordDefinition& chord, juce::Random& rng);

    struct Substitution
    {
        juce::String label;
        ChordDefinition chord;
    };

    /** Right-click substitution suggestions for a pad. */
    static std::vector<Substitution> getSubstitutions (const ChordDefinition& chord, const GlobalState& g);

    static int defaultVoicingForGenre (int genre, int complexity);

private:
    static constexpr int numStates = 13;
    using Matrix = std::array<std::array<double, numStates>, numStates>;

    void train();
    ChordDefinition chordForState (int state, const GeneratorSettings& s, int scaleIndex, juce::Random& rng) const;

    std::array<Matrix, 5> genreTables {};
};

} // namespace chordcat

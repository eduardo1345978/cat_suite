#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>

/**
    Cat Ocean: the genes of a fish. Everything a fish looks like, how it swims and how it sings
    comes from here; babies mix their parents' genes and sometimes mutate.
*/
namespace ocean
{
enum Body      { BodySlim = 0, BodyRound, BodyLong, BodyPuffer, BodyCat, BodyFlat, NumBodies };
enum Tail      { TailFan = 0, TailFork, TailRound, TailVeil, NumTails };
enum Fin       { FinNone = 0, FinSmall, FinSail, FinSpiky, NumFins };
enum Eyes      { EyesDot = 0, EyesBig, EyesSleepy, EyesStar, NumEyes };
enum Pattern   { PatPlain = 0, PatStripes, PatSpots, PatGradient, PatChecker, PatBelly, NumPatterns };
enum Accessory { AccNone = 0, AccHat, AccGlasses, AccCrown, AccScarf, AccBow, AccLamp, AccHeadphones, NumAccessories };
enum Voice     { VoiceBubble = 0, VoicePluck, VoiceBell, VoiceFlute, VoiceChip, VoicePad, VoiceMarimba, VoiceWhistle, NumVoices };
enum Rare      { RareNone = 0, RareGold, RareRainbow, RareGhost, RareGlow, RareCrystal, NumRares };
enum Mood      { MoodCalm = 0, MoodShy, MoodSocial, MoodHyper, MoodLazy, NumMoods };
/** Something only a new species has (evolution invents it). */
enum Trait     { TraitNone = 0, TraitWhiskers, TraitAntenna, TraitTwinTail, TraitStars, TraitFrill, TraitHorn, NumTraits };
/** What a viewer's level unlocks: look categories. */
enum LookKind  { KindBody = 0, KindPattern, KindTail, KindFin, KindEyes, KindHat, KindVoice };
/** The level a look needs (1 = everyone has it). */
int unlockLevel (int kind, int value);

/** The colours a fish can have (names are what people type in chat). */
int numColours();
juce::String colourName (int index);
juce::uint32 colourARGB (int index);
int findColour (const juce::String& word);   // -1 if not a colour

juce::StringArray bodyNames();
juce::StringArray tailNames();
juce::StringArray finNames();
juce::StringArray eyeNames();
juce::StringArray patternNames();
juce::StringArray accessoryNames();
juce::StringArray voiceNames();
juce::StringArray rareNames();
juce::StringArray moodNames();
juce::StringArray traitNames();

struct Genes
{
    // looks
    int body = BodySlim, size = 1 /* 0 small 1 medium 2 big */, tail = TailFan, fin = FinSmall, eyes = EyesDot;
    int pattern = PatPlain, accessory = AccNone;
    int colour1 = 0, colour2 = 1, colour3 = 2;        // body, pattern / belly, fins
    int rare = RareNone;
    int species = 0, trait = TraitNone;                  // 0 = the common fish; new species come from evolution
    // personality (0..1)
    float speed = 0.5f, social = 0.5f, shy = 0.3f, bounce = 0.2f;
    // singing
    int voice = VoiceBubble, pitch = 1 /* 0 low 1 mid 2 high */, rhythm = 1 /* 0 sparse 1 normal 2 busy */, noteLength = 0 /* 0 short 1 long */;
    std::array<int, 8> melody { 0, 2, 4, 2, 0, 0, 0, 0 };   // scale steps (0 = the key note, 7 = an octave up)
    int melodyLength = 4;
    juce::uint32 rhythmPattern = 0x1111;                     // 16 bits: which 1/16 steps of a bar it may sing on
    // what the ocean's music drifts towards
    float tempo = 90.0f;                                     // bpm
    int key = 0;                                             // pitch class

    static Genes random (juce::Random&);
    /** A baby: each gene from one parent, numbers in between, sometimes a mutation (or a rare one). */
    static Genes breed (const Genes& a, const Genes& b, juce::Random&, bool* mutated = nullptr);

    void setMood (int mood);
    int mood() const;
    void setMelody (const juce::String& digits);             // "1353" (1 = key note .. 8 = octave)
    juce::String melodyString() const;
    void setRhythm (int density);                            // makes a new pattern too
    float lengthPixels() const;                              // body length on the 480x270 canvas

    juce::ValueTree toTree() const;
    static Genes fromTree (const juce::ValueTree&);
};
} // namespace ocean

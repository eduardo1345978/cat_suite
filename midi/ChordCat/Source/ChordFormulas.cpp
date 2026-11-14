#include "ChordFormulas.h"
#include <algorithm>
#include <climits>
#include <map>

namespace chordcat
{
namespace
{
    uint16_t maskOf (const std::vector<int>& intervals)
    {
        uint16_t m = 0;
        for (int i : intervals)
            m |= (uint16_t) (1u << (((i % 12) + 12) % 12));
        return m;
    }

    std::vector<ChordTypeInfo> buildTable()
    {
        struct Row { const char* id; const char* symbol; std::vector<int> iv; int cx; ChordFamily fam; };

        const std::vector<Row> rows = {
            // --- Triads & basic colours ---------------------------------------------------
            { "maj",       "",           { 0, 4, 7 },                 0, FamilyMajor },
            { "min",       "m",          { 0, 3, 7 },                 0, FamilyMinor },
            { "dim",       "dim",        { 0, 3, 6 },                 0, FamilyDiminished },
            { "aug",       "aug",        { 0, 4, 8 },                 0, FamilyAugmented },
            { "sus2",      "sus2",       { 0, 2, 7 },                 0, FamilySuspended },
            { "sus4",      "sus4",       { 0, 5, 7 },                 0, FamilySuspended },
            { "5",         "5",          { 0, 7 },                    0, FamilyOther },
            { "b5",        "(b5)",       { 0, 4, 6 },                 0, FamilyMajor },
            { "sus2sus4",  "sus2sus4",   { 0, 2, 5, 7 },              0, FamilySuspended },
            { "add2",      "add2",       { 0, 2, 4, 7 },              0, FamilyMajor },
            { "madd2",     "m(add2)",    { 0, 2, 3, 7 },              0, FamilyMinor },
            { "6",         "6",          { 0, 4, 7, 9 },              0, FamilyMajor },
            { "m6",        "m6",         { 0, 3, 7, 9 },              0, FamilyMinor },
            { "6/9",       "6/9",        { 0, 4, 7, 9, 14 },          2, FamilyMajor },
            { "m6/9",      "m6/9",       { 0, 3, 7, 9, 14 },          2, FamilyMinor },
            { "add9",      "add9",       { 0, 4, 7, 14 },             2, FamilyMajor },
            { "madd9",     "m(add9)",    { 0, 3, 7, 14 },             2, FamilyMinor },
            { "add11",     "add11",      { 0, 4, 7, 17 },             3, FamilyMajor },
            { "madd11",    "m(add11)",   { 0, 3, 7, 17 },             3, FamilyMinor },
            { "add#11",    "add#11",     { 0, 4, 7, 18 },             3, FamilyMajor },
            // --- Sevenths -----------------------------------------------------------------
            { "7",         "7",          { 0, 4, 7, 10 },             1, FamilyDominant },
            { "maj7",      "maj7",       { 0, 4, 7, 11 },             1, FamilyMajor },
            { "m7",        "m7",         { 0, 3, 7, 10 },             1, FamilyMinor },
            { "mMaj7",     "m(maj7)",    { 0, 3, 7, 11 },             1, FamilyMinor },
            { "m7b5",      "m7b5",       { 0, 3, 6, 10 },             1, FamilyDiminished },
            { "dim7",      "dim7",       { 0, 3, 6, 9 },              1, FamilyDiminished },
            { "7sus4",     "7sus4",      { 0, 5, 7, 10 },             1, FamilySuspended },
            { "7sus2",     "7sus2",      { 0, 2, 7, 10 },             1, FamilySuspended },
            { "aug7",      "7#5",        { 0, 4, 8, 10 },             1, FamilyAugmented },
            { "maj7#5",    "maj7#5",     { 0, 4, 8, 11 },             1, FamilyAugmented },
            { "7b5",       "7b5",        { 0, 4, 6, 10 },             1, FamilyDominant },
            { "maj7b5",    "maj7b5",     { 0, 4, 6, 11 },             1, FamilyMajor },
            { "maj7sus2",  "maj7sus2",   { 0, 2, 7, 11 },             1, FamilySuspended },
            { "maj7sus4",  "maj7sus4",   { 0, 5, 7, 11 },             1, FamilySuspended },
            { "m7#5",      "m7#5",       { 0, 3, 8, 10 },             1, FamilyMinor },
            { "dimMaj7",   "dim(maj7)",  { 0, 3, 6, 11 },             1, FamilyDiminished },
            { "7add13",    "7add13",     { 0, 4, 7, 9, 10 },          4, FamilyDominant },
            { "maj7add13", "maj7add13",  { 0, 4, 7, 9, 11 },          4, FamilyMajor },
            { "m7add11",   "m7(11)",     { 0, 3, 7, 10, 17 },         3, FamilyMinor },
            // --- Ninths -------------------------------------------------------------------
            { "9",         "9",          { 0, 4, 7, 10, 14 },         2, FamilyDominant },
            { "maj9",      "maj9",       { 0, 4, 7, 11, 14 },         2, FamilyMajor },
            { "m9",        "m9",         { 0, 3, 7, 10, 14 },         2, FamilyMinor },
            { "mMaj9",     "m(maj9)",    { 0, 3, 7, 11, 14 },         2, FamilyMinor },
            { "9sus4",     "9sus4",      { 0, 5, 7, 10, 14 },         2, FamilySuspended },
            { "maj9sus4",  "maj9sus4",   { 0, 5, 7, 11, 14 },         2, FamilySuspended },
            { "7b9",       "7b9",        { 0, 4, 7, 10, 13 },         2, FamilyDominant },
            { "7#9",       "7#9",        { 0, 4, 7, 10, 15 },         2, FamilyDominant },
            { "m7b9",      "m7b9",       { 0, 3, 7, 10, 13 },         2, FamilyMinor },
            { "m9b5",      "m9b5",       { 0, 3, 6, 10, 14 },         2, FamilyDiminished },
            { "7#5b9",     "7#5b9",      { 0, 4, 8, 10, 13 },         2, FamilyAugmented },
            { "7#5#9",     "7#5#9",      { 0, 4, 8, 10, 15 },         2, FamilyAugmented },
            { "7b5b9",     "7b5b9",      { 0, 4, 6, 10, 13 },         2, FamilyDominant },
            { "9#5",       "9#5",        { 0, 4, 8, 10, 14 },         2, FamilyAugmented },
            { "9b5",       "9b5",        { 0, 4, 6, 10, 14 },         2, FamilyDominant },
            { "sus4b9",    "7sus4(b9)",  { 0, 5, 7, 10, 13 },         2, FamilySuspended },
            // --- Elevenths ----------------------------------------------------------------
            { "7#11",      "7#11",       { 0, 4, 7, 10, 18 },         3, FamilyDominant },
            { "maj7#11",   "maj7#11",    { 0, 4, 7, 11, 18 },         3, FamilyMajor },
            { "9#11",      "9#11",       { 0, 4, 7, 10, 14, 18 },     3, FamilyDominant },
            { "maj9#11",   "maj9#11",    { 0, 4, 7, 11, 14, 18 },     3, FamilyMajor },
            { "7#9#11",    "7#9#11",     { 0, 4, 7, 10, 15, 18 },     3, FamilyDominant },
            { "7b9#11",    "7b9#11",     { 0, 4, 7, 10, 13, 18 },     3, FamilyDominant },
            { "11",        "11",         { 0, 7, 10, 14, 17 },        3, FamilyDominant },
            { "m11",       "m11",        { 0, 3, 7, 10, 14, 17 },     3, FamilyMinor },
            { "maj11",     "maj11",      { 0, 4, 7, 11, 14, 17 },     3, FamilyMajor },
            { "mMaj11",    "m(maj11)",   { 0, 3, 7, 11, 14, 17 },     3, FamilyMinor },
            { "m11b5",     "m11b5",      { 0, 3, 6, 10, 14, 17 },     3, FamilyDiminished },
            { "6/9#11",    "6/9#11",     { 0, 4, 7, 9, 14, 18 },      3, FamilyMajor },
            // --- Thirteenths & altered ------------------------------------------------------
            { "13",        "13",         { 0, 4, 7, 10, 14, 21 },     4, FamilyDominant },
            { "maj13",     "maj13",      { 0, 4, 7, 11, 14, 21 },     4, FamilyMajor },
            { "m13",       "m13",        { 0, 3, 7, 10, 14, 21 },     4, FamilyMinor },
            { "13b9",      "13b9",       { 0, 4, 7, 10, 13, 21 },     4, FamilyDominant },
            { "13#9",      "13#9",       { 0, 4, 7, 10, 15, 21 },     4, FamilyDominant },
            { "13#11",     "13#11",      { 0, 4, 7, 10, 14, 18, 21 }, 4, FamilyDominant },
            { "13sus4",    "13sus4",     { 0, 5, 7, 10, 14, 21 },     4, FamilySuspended },
            { "13b5",      "13b5",       { 0, 4, 6, 10, 14, 21 },     4, FamilyDominant },
            { "7b13",      "7b13",       { 0, 4, 7, 10, 20 },         4, FamilyDominant },
            { "7b9b13",    "7b9b13",     { 0, 4, 7, 10, 13, 20 },     4, FamilyDominant },
            { "7alt",      "7alt",       { 0, 4, 10, 13, 15, 20 },    4, FamilyDominant },
            // --- Fourth-based ----------------------------------------------------------------
            { "quartal",   "4ths",       { 0, 5, 10, 15 },            1, FamilyOther },
            { "sowhat",    "So What",    { 0, 5, 10, 15, 19 },        2, FamilyOther },
        };

        std::vector<ChordTypeInfo> table;
        table.reserve (rows.size());
        for (const auto& r : rows)
            table.push_back ({ r.id, r.symbol, r.iv, r.cx, r.fam, maskOf (r.iv) });

        jassert (table.size() == 80);
        return table;
    }
}

const std::vector<ChordTypeInfo>& getChordTypes()
{
    static const std::vector<ChordTypeInfo> table = buildTable();
    return table;
}

int getNumChordTypes() { return (int) getChordTypes().size(); }

int findChordType (const juce::String& id)
{
    const auto& t = getChordTypes();
    for (size_t i = 0; i < t.size(); ++i)
        if (t[i].id == id)
            return (int) i;
    return -1;
}

const ChordTypeInfo& getChordTypeInfo (const juce::String& id)
{
    const int i = findChordType (id);
    return getChordTypes()[(size_t) juce::jmax (0, i)];
}

std::vector<int> getChordFormula (const juce::String& id)
{
    return getChordTypeInfo (id).intervals;
}

std::vector<int> parseTensions (const juce::String& tension)
{
    static const std::map<juce::String, int> table = {
        { "b9", 13 }, { "9", 14 }, { "#9", 15 }, { "11", 17 }, { "#11", 18 }, { "b13", 20 }, { "13", 21 },
        { "6", 9 }, { "add6", 9 }, { "add2", 2 }, { "add9", 14 }, { "add11", 17 }, { "add13", 21 },
        { "b5", 6 }, { "#5", 8 }, { "7", 10 }, { "maj7", 11 }, { "b7", 10 }
    };

    std::vector<int> result;
    auto tokens = juce::StringArray::fromTokens (tension.replace ("(", " ").replace (")", " "), " ,/;", "");
    for (auto t : tokens)
    {
        t = t.trim();
        if (auto it = table.find (t); it != table.end())
            result.push_back (it->second);
    }
    return result;
}

juce::String pitchClassName (int pc, bool preferFlats)
{
    static const char* sharps[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flats[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    pc = ((pc % 12) + 12) % 12;
    return preferFlats ? flats[pc] : sharps[pc];
}

juce::String chordDisplayName (int root, const juce::String& type, bool preferFlats)
{
    return pitchClassName (root, preferFlats) + getChordTypeInfo (type).symbol;
}

int findTypeForMask (uint16_t mask, int preferredComplexity)
{
    const auto& t = getChordTypes();
    int best = -1, bestScore = INT_MAX;
    for (size_t i = 0; i < t.size(); ++i)
    {
        if (t[i].pitchClassMask != mask)
            continue;
        const int score = (preferredComplexity >= 0 ? std::abs (t[i].complexity - preferredComplexity) * 100 : 0) + (int) i;
        if (score < bestScore) { bestScore = score; best = (int) i; }
    }
    return best;
}

RecognizedChord recognizeChord (const std::vector<int>& notes)
{
    RecognizedChord result;
    if (notes.empty())
        return result;

    uint16_t mask = 0;
    int lowest = 128;
    for (int n : notes)
    {
        mask |= (uint16_t) (1u << (n % 12));
        lowest = std::min (lowest, n);
    }
    const int bassPc = lowest % 12;
    result.bassPitchClass = bassPc;

    const int numPcs = juce::countNumberOfBits ((juce::uint32) mask);
    const auto& types = getChordTypes();

    auto rotate = [] (uint16_t m, int root) -> uint16_t
    {
        return (uint16_t) (((m >> root) | (m << (12 - root))) & 0xFFF);
    };

    double bestScore = -1.0e9;

    for (int root = 0; root < 12; ++root)
    {
        if ((mask & (1u << root)) == 0)
            continue;

        const uint16_t rel = rotate (mask, root);

        for (size_t i = 0; i < types.size(); ++i)
        {
            const auto& t = types[i];
            if (t.id == "quartal" || t.id == "sowhat" || (t.id == "5" && numPcs > 2))
                continue;

            const uint16_t tm = t.pitchClassMask;
            const bool exact = tm == rel;
            const bool subset = (tm & rel) == tm; // all chord-type tones present
            if (! exact && ! subset)
                continue;

            const int extra = juce::countNumberOfBits ((juce::uint32) (rel & ~tm));
            double score = (exact ? 100.0 : 50.0) - extra * 15.0 + juce::countNumberOfBits ((juce::uint32) tm) * 2.0;
            if (root == bassPc) score += 12.0;
            score -= (double) i * 0.01;

            if (score > bestScore)
            {
                bestScore = score;
                result.valid = true;
                result.root = root;
                result.type = t.id;
            }
        }
    }

    return result;
}

} // namespace chordcat

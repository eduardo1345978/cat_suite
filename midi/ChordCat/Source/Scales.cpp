#include "Scales.h"
#include "ChordFormulas.h"

namespace chordcat
{
const std::vector<ScaleInfo>& getScales()
{
    static const std::vector<ScaleInfo> scales = {
        { "Major",              { 0, 2, 4, 5, 7, 9, 11 } },
        { "Natural Minor",      { 0, 2, 3, 5, 7, 8, 10 } },
        { "Harmonic Minor",     { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",      { 0, 2, 3, 5, 7, 9, 11 } },
        { "Dorian",             { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",           { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",             { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",         { 0, 2, 4, 5, 7, 9, 10 } },
        { "Locrian",            { 0, 1, 3, 5, 6, 8, 10 } },
        { "Major Pentatonic",   { 0, 2, 4, 7, 9 } },
        { "Minor Pentatonic",   { 0, 3, 5, 7, 10 } },
        { "Blues",              { 0, 3, 5, 6, 7, 10 } },
        { "Major Blues",        { 0, 2, 3, 4, 7, 9 } },
        { "Harmonic Major",     { 0, 2, 4, 5, 7, 8, 11 } },
        { "Double Harmonic",    { 0, 1, 4, 5, 7, 8, 11 } },
        { "Hungarian Minor",    { 0, 2, 3, 6, 7, 8, 11 } },
        { "Hungarian Major",    { 0, 3, 4, 6, 7, 9, 10 } },
        { "Neapolitan Minor",   { 0, 1, 3, 5, 7, 8, 11 } },
        { "Neapolitan Major",   { 0, 1, 3, 5, 7, 9, 11 } },
        { "Phrygian Dominant",  { 0, 1, 4, 5, 7, 8, 10 } },
        { "Lydian Dominant",    { 0, 2, 4, 6, 7, 9, 10 } },
        { "Lydian Augmented",   { 0, 2, 4, 6, 8, 9, 11 } },
        { "Altered",            { 0, 1, 3, 4, 6, 8, 10 } },
        { "Dorian b2",          { 0, 1, 3, 5, 7, 9, 10 } },
        { "Mixolydian b6",      { 0, 2, 4, 5, 7, 8, 10 } },
        { "Locrian #2",         { 0, 2, 3, 5, 6, 8, 10 } },
        { "Ionian #5",          { 0, 2, 4, 5, 8, 9, 11 } },
        { "Dorian #4",          { 0, 2, 3, 6, 7, 9, 10 } },
        { "Ultralocrian",       { 0, 1, 3, 4, 6, 8, 9 } },
        { "Whole Tone",         { 0, 2, 4, 6, 8, 10 } },
        { "Diminished W-H",     { 0, 2, 3, 5, 6, 8, 9, 11 } },
        { "Diminished H-W",     { 0, 1, 3, 4, 6, 7, 9, 10 } },
        { "Bebop Dominant",     { 0, 2, 4, 5, 7, 9, 10, 11 } },
        { "Bebop Major",        { 0, 2, 4, 5, 7, 8, 9, 11 } },
        { "Bebop Dorian",       { 0, 2, 3, 4, 5, 7, 9, 10 } },
        { "Enigmatic",          { 0, 1, 4, 6, 8, 10, 11 } },
        { "Persian",            { 0, 1, 4, 5, 6, 8, 11 } },
        { "Arabian",            { 0, 2, 4, 5, 6, 8, 10 } },
        { "Japanese In",        { 0, 1, 5, 7, 8 } },
        { "Hirajoshi",          { 0, 2, 3, 7, 8 } },
        { "Iwato",              { 0, 1, 5, 6, 10 } },
        { "Egyptian",           { 0, 2, 5, 7, 10 } },
        { "Chinese",            { 0, 4, 6, 7, 11 } },
        { "Prometheus",         { 0, 2, 4, 6, 9, 10 } },
        { "Romanian Minor",     { 0, 2, 3, 6, 7, 9, 10 } },
        { "Spanish 8-Tone",     { 0, 1, 3, 4, 5, 6, 8, 10 } },
        { "Chromatic",          { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
        { "Custom",             {} },
    };
    jassert (scales.size() == 48);
    return scales;
}

int getNumScales()         { return (int) getScales().size(); }
int getCustomScaleIndex()  { return getNumScales() - 1; }

int findScale (const juce::String& name)
{
    const auto& s = getScales();
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i].name.equalsIgnoreCase (name))
            return (int) i;
    return -1;
}

juce::StringArray getScaleNames()
{
    juce::StringArray names;
    for (const auto& s : getScales())
        names.add (s.name);
    return names;
}

uint16_t getScaleMask (int scaleIndex, uint16_t customMask)
{
    const auto& scales = getScales();
    scaleIndex = juce::jlimit (0, (int) scales.size() - 1, scaleIndex);
    if (scaleIndex == getCustomScaleIndex())
        return (uint16_t) ((customMask & 0xFFF) | 1u); // root always included

    uint16_t m = 0;
    for (int i : scales[(size_t) scaleIndex].intervals)
        m |= (uint16_t) (1u << i);
    return m;
}

std::vector<int> getScalePitchClasses (int keyRoot, int scaleIndex, uint16_t customMask)
{
    std::vector<int> pcs;
    const auto mask = getScaleMask (scaleIndex, customMask);
    for (int i = 0; i < 12; ++i)
        if (mask & (1u << i))
            pcs.push_back ((keyRoot + i) % 12);
    return pcs;
}

bool isInScale (int pc, int keyRoot, int scaleIndex, uint16_t customMask)
{
    const int rel = (((pc - keyRoot) % 12) + 12) % 12;
    return (getScaleMask (scaleIndex, customMask) & (1u << rel)) != 0;
}

int quantizeToScale (int pc, int keyRoot, int scaleIndex, uint16_t customMask)
{
    pc = ((pc % 12) + 12) % 12;
    for (int d = 0; d <= 6; ++d)
    {
        if (isInScale (pc - d, keyRoot, scaleIndex, customMask)) return ((pc - d) % 12 + 12) % 12;
        if (isInScale (pc + d, keyRoot, scaleIndex, customMask)) return (pc + d) % 12;
    }
    return pc;
}

juce::String romanNumeral (int chordRoot, const juce::String& chordType, int keyRoot, int scaleIndex, uint16_t customMask)
{
    static const char* numerals[] = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII" };
    static const char* chromatic[] = { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };

    const auto& info = getChordTypeInfo (chordType);
    const bool lower = info.family == FamilyMinor || info.family == FamilyDiminished;
    const int rel = (((chordRoot - keyRoot) % 12) + 12) % 12;

    juce::String numeral;
    const auto pcs = getScalePitchClasses (keyRoot, scaleIndex, customMask);
    const bool heptatonic = pcs.size() == 7;

    if (heptatonic && isInScale (chordRoot, keyRoot, scaleIndex, customMask))
    {
        for (size_t i = 0; i < pcs.size(); ++i)
            if (pcs[i] == ((chordRoot % 12) + 12) % 12)
                numeral = numerals[i];
    }
    else
    {
        numeral = chromatic[rel];
    }

    if (lower)
        numeral = numeral.toLowerCase();

    if (info.family == FamilyDiminished) numeral << "o";
    if (info.family == FamilyAugmented)  numeral << "+";

    // Flag chords that contain tones outside the key
    bool chromaticTones = false;
    for (int iv : info.intervals)
        if (! isInScale (chordRoot + iv, keyRoot, scaleIndex, customMask))
            chromaticTones = true;

    return chromaticTones ? numeral + "*" : numeral;
}

} // namespace chordcat

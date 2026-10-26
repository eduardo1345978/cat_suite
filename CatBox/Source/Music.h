#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

/** Keys and scales shared by the pads, the chords and the sequencer. */
namespace catbox::music
{
constexpr int numScales = 11;

inline const char* scaleName (int s)
{
    static const char* n[numScales] = { "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Harm. minor",
                                        "Pent. major", "Pent. minor", "Blues", "Chromatic" };
    return n[juce::jlimit (0, numScales - 1, s)];
}

/** Semitones of the scale (up to 12), count in n. */
inline const std::array<int, 12>& scaleSteps (int s, int& n)
{
    static const std::array<std::array<int, 12>, numScales> t {{
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 },
        { 0, 2, 4, 6, 7, 9, 11 }, { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 },
        { 0, 2, 4, 7, 9 }, { 0, 3, 5, 7, 10 }, { 0, 3, 5, 6, 7, 10 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } }};
    static const int count[numScales] = { 7, 7, 7, 7, 7, 7, 7, 5, 5, 6, 12 };
    s = juce::jlimit (0, numScales - 1, s);
    n = count[s];
    return t[(size_t) s];
}

inline const char* keyName (int k)
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((k % 12) + 12) % 12];
}

inline juce::String noteName (int note)
{
    if (note < 0) return "-";
    return juce::String (keyName (note)) + juce::String (note / 12 - 1);
}

inline bool inScale (int note, int key, int scale)
{
    int n = 0;
    const auto& st = scaleSteps (scale, n);
    const int pc = ((note - key) % 12 + 12) % 12;
    for (int i = 0; i < n; ++i) if (st[(size_t) i] == pc) return true;
    return false;
}

/** The scale note at or below this note: its octave and degree. */
inline void degreeOf (int note, int key, int scale, int& octave, int& degree)
{
    int n = 0;
    const auto& st = scaleSteps (scale, n);
    const int rel = note - key;
    octave = (int) std::floor (rel / 12.0);
    const int pc = rel - octave * 12;
    degree = 0;
    for (int i = 0; i < n; ++i) if (st[(size_t) i] <= pc) degree = i;
}

/** Note of a scale degree (any integer: wraps into octaves). */
inline int noteOfDegree (int key, int scale, int octave, int degree)
{
    int n = 0;
    const auto& st = scaleSteps (scale, n);
    const int o = (int) std::floor ((double) degree / n);
    const int d = degree - o * n;
    return key + 12 * (octave + o) + st[(size_t) d];
}

/** Moves a note by whole scale steps (snapping to the scale first). */
inline int moveInScale (int note, int key, int scale, int steps)
{
    int o = 0, d = 0;
    degreeOf (note, key, scale, o, d);
    return juce::jlimit (0, 127, noteOfDegree (key, scale, o, d + steps));
}
} // namespace catbox::music

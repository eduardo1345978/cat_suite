#pragma once

#include <array>
#include <vector>
#include <juce_core/juce_core.h>

/**
    Sing-Cat lyrics: turns typed words into sung syllables.

    "hel-lo my dar-ling"  ->  hel | lo | my | dar | ling
    Every new note sings the next syllable (looping). Onset consonants are articulated at the
    start of the note, the vowel is held, coda consonants (and the second half of a diphthong,
    e.g. the "i" of "my") are sung when the note is released.

      - spaces or '-' separate syllables (words with several vowels are split automatically)
      - '+' holds the previous syllable over the next note (melisma: "a + + men")
      - silent final e lengthens the vowel: "time" -> t-ai-m, "home" -> h-ou-m
*/
namespace singcat
{
enum class PhoneKind : uint8_t { Plosive, Fricative, Nasal, Liquid, Glide, Aspirate, Vowel };

/** Where in the vocal tract a consonant is made: index of the area control point (0 larynx .. 7 lips). */
enum Place : uint8_t { Velar = 4, Palatal = 5, Alveolar = 6, Lips = 7 };

struct Phone
{
    PhoneKind kind = PhoneKind::Vowel;
    uint8_t place = Lips;
    bool voiced = true;
    float area = 0.0f;        // constriction area (cm^2) for fricatives / liquids
    float colour = 0.5f;      // noise brightness 0 (f, h) .. 1 (s)
    float dur = 0.06f;        // seconds at Diction 50 %
    float x = 0.5f, y = 0.5f; // vowel-chart target (glides, vowels, r)
    bool round = false;       // lip rounding (w, r)
};

struct Syllable
{
    std::array<Phone, 4> onset {};
    int numOnset = 0;
    float vx = 0.55f, vy = 0.95f;          // nucleus on the vowel chart
    bool diphthong = false;
    float v2x = 0.5f, v2y = 0.5f;          // second vowel of a diphthong (sung at the release)
    std::array<Phone, 4> coda {};
    int numCoda = 0;
    bool hold = false;                     // '+': keep singing the previous syllable
    bool hum = false;                      // "hmm": the nasal below is held instead of a vowel
    Phone humPhone {};
    char text[16] {};
};

std::vector<Syllable> parseLyrics (const juce::String& text);

} // namespace singcat

#pragma once

#include <array>
#include <vector>
#include <juce_core/juce_core.h>

/**
    Talk-Cat phonetics: text in four languages -> syllables made of articulated phones.

      English   dictionary of common song words + spelling rules, split into syllables
      Slovak    regular spelling (c s z t d n l, dz dz ch, ia ie iu o, syllabic r / l, final devoicing)
      Japanese  romaji morae (ka shi tsu fu, kya, n, small tsu "kk", long vowels)
      Cat       meow, mew, mrrp, prr, purr, hiss, nya, yowl, chirp, trill ... (everything else: English)

    Anything can also be written as phonemes in brackets, e.g. "[l ah v]" or "[sh i]".
*/
namespace talkcat
{
enum class Language { English = 0, Slovak, Japanese, Cat };
juce::StringArray languageNames();

enum class PK : uint8_t { Vowel, Plosive, Fricative, Nasal, Lateral, Rhotic, Trill, Tap, Glide, Aspirate, Glottal };

/** Where in the vocal tract: index of the 8 area control points (0 larynx .. 7 lips). */
enum Place : uint8_t { GlottalPlace = 0, Uvular = 3, Velar = 4, Palatal = 5, Alveolar = 6, Lips = 7 };

struct Phone
{
    PK kind = PK::Vowel;
    uint8_t place = Lips;
    bool voiced = true;
    float area = 0.0f;       // constriction area (cm^2) for fricatives / liquids
    float colour = 0.5f;     // turbulence brightness 0 (f, h) .. 1 (s)
    float dur = 0.06f;       // seconds at Consonant Length 100 %
    float x = 0.5f, y = 0.5f;// vowel-chart target (vowels, glides, r)
    float lips = 0.0f;       // -1 extra rounding .. +1 spread
    bool aspirated = false;  // English p / t / k before a vowel
    bool rhotic = false;     // r-coloured vowel (er)
    char sym[5] {};
};

enum class CatGesture : uint8_t { None, Meow, Mew, Mrrp, Purr, Hiss, Yowl, Chirp, Trill };

struct Syllable
{
    std::array<Phone, 6> onset {};
    int numOnset = 0;
    Phone nucleus {};                 // a vowel, or a syllabic consonant (Slovak "vlk", "hmm", Japanese "n")
    bool diphthong = false;
    Phone nucleus2 {};                // second vowel of a diphthong (reached towards the end of the note)
    std::array<Phone, 6> coda {};
    int numCoda = 0;
    bool hold = false;                // "-" or "+": melisma, keep singing the previous vowel
    bool breath = false;              // "br": an audible breath instead of singing
    CatGesture gesture = CatGesture::None;
    char text[24] {};

    void addOnset (const Phone& p) { if (numOnset < (int) onset.size()) onset[(size_t) numOnset++] = p; }
    void addCoda (const Phone& p)  { if (numCoda < (int) coda.size()) coda[(size_t) numCoda++] = p; }
};

/** The phone for a symbol ("aa", "sh", "rr" ...), or nullptr. */
const Phone* phoneFor (const juce::String& symbol);

/** All phoneme symbols with a short description (for the help popup). */
juce::StringArray phonemeHelp();

/** A lyric text (one or many words) -> syllables. "-" / "+" tokens become holds. */
std::vector<Syllable> textToSyllables (const juce::String& text, Language language);

/** Brackets: "[l ah v]" -> one syllable. */
Syllable syllableFromPhonemes (const juce::String& symbols);

/** "l ah v" - for display under the notes. */
juce::String phonemeString (const Syllable&);

/** How long the onset consonants take before the vowel / the coda after it (seconds). */
float onsetSeconds (const Syllable&, float lengthScale);
float codaSeconds (const Syllable&, float lengthScale);

} // namespace talkcat

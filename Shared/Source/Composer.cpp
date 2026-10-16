#include "Composer.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace cat::muse
{
namespace
{
using u64 = juce::uint64;

//==============================================================================
// a small, platform-independent random generator (splitmix64): same seed = same song everywhere
struct Rng
{
    explicit Rng (u64 seed) : s (seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull) { next(); }
    u64 next() noexcept
    {
        s += 0x9E3779B97F4A7C15ull;
        u64 z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    double uni() noexcept { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }
    int range (int lo, int hi) noexcept { return hi <= lo ? lo : lo + (int) (next() % (u64) (hi - lo + 1)); }
    bool chance (double p) noexcept { return uni() < p; }
    template <typename T> const T& pick (const std::vector<T>& v) { return v[(size_t) range (0, (int) v.size() - 1)]; }
    int weighted (const double* w, int n)
    {
        double total = 0.0;
        for (int i = 0; i < n; ++i) total += juce::jmax (0.0, w[i]);
        if (total <= 0.0) return range (0, n - 1);
        double x = uni() * total;
        for (int i = 0; i < n; ++i) { x -= juce::jmax (0.0, w[i]); if (x < 0.0) return i; }
        return n - 1;
    }
    u64 s;
};

u64 mix (u64 a, u64 b) { Rng r (a * 1000003ull ^ (b + 0x51ED27ull) * 0x2545F4914F6CDD1Dull); return r.next(); }

//==============================================================================
const int scaleSteps[7][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },   // Major
    { 0, 2, 3, 5, 7, 8, 10 },   // Minor
    { 0, 2, 3, 5, 7, 9, 10 },   // Dorian
    { 0, 2, 4, 5, 7, 9, 10 },   // Mixolydian
    { 0, 2, 4, 6, 7, 9, 11 },   // Lydian
    { 0, 2, 3, 5, 7, 8, 11 },   // Harmonic minor
    { 0, 1, 3, 5, 7, 8, 10 },   // Phrygian
};
enum { Major = 0, Minor, Dorian, Mixolydian, Lydian, HarmonicMinor, Phrygian };
const char* const scaleNameList[] = { "Major", "Minor", "Dorian", "Mixolydian", "Lydian", "Harmonic minor", "Phrygian" };
const char* const keyNameList[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

bool isMinorScale (int scale) { return scaleSteps[scale][2] == 3; }

struct GenreInfo
{
    double bpmLo, bpmHi;
    float swingLo, swingHi;
    double swingGrid;        // 0.5 = swung 8ths, 0.25 = swung 16ths
    double chordBeats[2];
    float p7, p9;            // chance of 7ths / 9ths
    int density;             // melody: 0 slow, 1 normal, 2 busy
};

const GenreInfo genreInfo[NumGenres] = {
    {  96, 124, 0.50f, 0.50f, 0.25, { 4, 4 }, 0.12f, 0.05f, 1 },   // Pop
    {  70,  88, 0.56f, 0.60f, 0.25, { 4, 4 }, 0.90f, 0.45f, 1 },   // Lo-fi
    { 130, 150, 0.50f, 0.53f, 0.25, { 8, 4 }, 0.35f, 0.10f, 1 },   // Trap (half-time feel)
    { 120, 126, 0.50f, 0.54f, 0.25, { 4, 8 }, 0.60f, 0.30f, 2 },   // House
    {  60,  78, 0.50f, 0.50f, 0.25, { 8, 8 }, 0.30f, 0.50f, 0 },   // Ambient
    { 104, 150, 0.60f, 0.65f, 0.50, { 4, 2 }, 1.00f, 0.40f, 1 },   // Jazz
    { 112, 148, 0.50f, 0.50f, 0.25, { 4, 4 }, 0.00f, 0.00f, 1 },   // Rock
    {  72, 100, 0.50f, 0.50f, 0.25, { 4, 8 }, 0.05f, 0.10f, 0 },   // Cinematic
};

//==============================================================================
// chords are encoded as: degree (0-6) | variant << 3 | extensions << 6
enum Variant { VarDiatonic = 0, VarMajor, VarMinor, VarBorrowed, VarSus4, VarSus2 };
constexpr int ext7 = 1, ext9 = 2;
int code (int degree, int variant = 0, int ext = 0) { return degree | (variant << 3) | (ext << 6); }
int degreeOf (int c) { return c & 7; }
int variantOf (int c) { return (c >> 3) & 7; }
int extOf (int c) { return (c >> 6) & 3; }

// templates: value = degree + 10 * variant
const std::vector<std::vector<int>> majorTemplates = {
    { 0, 4, 5, 3 }, { 5, 3, 0, 4 }, { 0, 5, 3, 4 }, { 0, 3, 5, 4 }, { 3, 0, 4, 5 }, { 0, 2, 3, 4 },
    { 0, 3, 0, 4 }, { 0, 36, 3, 0 }, { 0, 5, 1, 4 }, { 3, 4, 2, 5 }, { 0, 3, 23, 0 }, { 1, 4, 0, 5 },
};
const std::vector<std::vector<int>> minorTemplates = {
    { 0, 5, 2, 6 }, { 0, 3, 6, 2 }, { 0, 6, 5, 6 }, { 0, 5, 3, 14 }, { 0, 3, 4, 0 }, { 5, 6, 0, 0 },
    { 0, 2, 6, 3 }, { 0, 4, 5, 3 }, { 0, 5, 6, 4 }, { 3, 6, 2, 5 },
};
const std::vector<std::vector<int>> dorianTemplates = { { 0, 3, 0, 3 }, { 0, 3, 6, 0 }, { 0, 6, 3, 0 }, { 0, 2, 3, 6 } };
const std::vector<std::vector<int>> phrygianTemplates = { { 0, 1, 0, 6 }, { 0, 1, 6, 0 }, { 0, 5, 1, 0 } };
const std::vector<std::vector<int>> majorPre = { { 3, 4, 3, 4 }, { 1, 3, 4, 4 }, { 5, 3, 4, 4 }, { 3, 2, 1, 4 } };
const std::vector<std::vector<int>> minorPre = { { 5, 6, 5, 6 }, { 3, 5, 6, 6 }, { 3, 14, 3, 14 }, { 5, 3, 6, 6 } };
const std::vector<std::vector<int>> majorBridge = { { 5, 3, 1, 4 }, { 3, 5, 3, 4 }, { 2, 5, 3, 4 }, { 5, 2, 3, 4 } };
const std::vector<std::vector<int>> minorBridge = { { 5, 2, 6, 4 }, { 3, 0, 5, 14 }, { 2, 6, 3, 5 }, { 5, 6, 3, 14 } };

// how likely each chord goes to each next one (functional harmony)
const double majorMarkov[7][7] = {
    { 0, 2, 1, 4, 4, 4, 0.2 }, { 0.5, 0, 0, 1, 5, 1, 0.5 }, { 0, 1, 0, 3, 0.5, 4, 0 }, { 3, 1.5, 0.5, 0, 4, 1.5, 0 },
    { 5, 0, 0.5, 1.5, 0, 3, 0 }, { 1, 3, 1, 4, 2, 0, 0 }, { 4, 0, 1, 0, 0, 0.5, 0 },
};
const double minorMarkov[7][7] = {
    { 0, 0.2, 2, 3, 2, 4, 3 }, { 1, 0, 0, 0, 3, 0, 0 }, { 1, 0, 0, 2, 0, 3, 3 }, { 3, 0, 0, 0, 2, 1.5, 3 },
    { 4, 0, 0, 1.5, 0, 3, 0 }, { 2, 0, 2, 2, 1, 0, 4 }, { 4, 0, 4, 0, 0, 2, 0 },
};

//==============================================================================
// rhythms: one bar of 16ths, 'x' = a note starts, '-' = it holds, '.' = rest
const std::vector<const char*> rhythmSlow = { "x-------x-------", "x-----------x---", "x-------x---x---", "x---x-------x---", "x-----x-x-------", "x-------..x-x---" };
const std::vector<const char*> rhythmMid = {
    "x---x---x-x-x---", "x-x-x---x---....", "..x-x-x-x---x---", "x--x--x-x-------", "x---x-x---x-x---", "....x-x-x-x-x---",
    "x-x-x-x-x---....", "x-------x---x---", "x-----x-----x---", "..x-x---x-x-x---", "x--x--x---x-x---", "x-x---x-x---x---",
};
const std::vector<const char*> rhythmBusy = {
    "x-x-x-x-x-x-x---", "x.x.x.xxx.x.x...", "x-xx-xx-x-x-x---", "..x.x.x.xx-x-x--", "x--x--x-x--x--x-", "xx-xx-x-xx-x----", "x-x-xx-x-x-xx---",
};
const std::vector<const char*> cadenceMid = { "x---x---x-------", "x-x-x-----------", "x-------........", "x-x-x-x-x-------", "x---------------", "..x-x-x---------" };
const std::vector<const char*> cadenceSlow = { "x---------------", "x-------x-------", "x-----------...." };

struct Hit { double start, length; };
std::vector<Hit> parseRhythm (const char* p, double barStart)
{
    std::vector<Hit> out;
    for (int i = 0; i < 16; ++i)
    {
        if (p[i] != 'x') continue;
        int len = 1;
        while (i + len < 16 && p[i + len] == '-') ++len;
        out.push_back ({ barStart + i * 0.25, len * 0.25 });
    }
    return out;
}

//==============================================================================
// lyrics
struct MoodWords { std::vector<const char*> verbs, adjs, nouns; std::vector<int> families; };
const MoodWords moodWords[NumMoods] = {
    { { "dance", "shine", "fly", "run", "sing", "glow", "rise", "jump", "smile", "play", "spin" },
      { "bright", "warm", "gold-en", "hap-py", "new", "wild", "sun-ny", "sweet" },
      { "sun", "light", "sky", "heart", "stars", "sum-mer", "mor-ning", "paws", "whis-kers", "rain-bow", "street" },
      { 0, 1, 4, 5, 7, 8, 11, 16 } },
    { { "fall", "cry", "fade", "wait", "miss", "break", "drift", "call", "leave" },
      { "cold", "gray", "lone-ly", "emp-ty", "bro-ken", "qui-et", "dis-tant" },
      { "rain", "tears", "night", "home", "heart", "ghost", "mem-o-ries", "win-dow", "paws", "si-lence", "road" },
      { 3, 1, 6, 13, 14, 5, 8 } },
    { { "burn", "hide", "crawl", "hunt", "fall", "creep", "prowl", "watch" },
      { "dark", "black", "cold", "si-lent", "hol-low", "wick-ed", "shad-owed" },
      { "shad-ows", "night", "fire", "smoke", "moon", "storm", "claws", "eyes", "al-ley", "crown" },
      { 0, 2, 6, 13, 14, 10, 11 } },
    { { "float", "drift", "fly", "glow", "fade", "dream", "swim", "wan-der" },
      { "soft", "blue", "gen-tle", "end-less", "sil-ver", "ha-zy", "qui-et" },
      { "clouds", "stars", "dreams", "sea", "moon", "haze", "sky", "light", "pil-lows", "o-cean", "whis-kers" },
      { 15, 12, 4, 8, 9, 7, 0 } },
    { { "rise", "fight", "stand", "run", "burn", "call", "roar", "climb", "break" },
      { "brave", "wild", "strong", "end-less", "gold-en", "fear-less", "migh-ty" },
      { "fire", "sky", "storm", "crown", "heart", "moun-tains", "thun-der", "light", "claws", "king-dom" },
      { 2, 4, 6, 0, 10, 5, 14 } },
    { { "float", "chill", "ride", "slide", "lay", "roll", "sway", "breathe", "stroll" },
      { "slow", "warm", "ea-sy", "la-zy", "soft", "mel-low", "cool" },
      { "waves", "sun", "breeze", "sea", "road", "cof-fee", "af-ter-noon", "sun-set", "paws", "ham-mock", "cat-nap" },
      { 1, 11, 12, 7, 8, 16, 13 } },
};
// rhyme families; every word is tagged: n = a thing ("the night"), a = says how / where ("fly tonight"),
// v = an action ("we dance and play")
struct RhymeWord { const char* word; char kind; };
const std::vector<std::vector<RhymeWord>> rhymeFamilies = {
    { { "night", 'n' }, { "light", 'n' }, { "flight", 'n' }, { "sight", 'n' }, { "de-light", 'n' }, { "bright", 'a' }, { "to-night", 'a' }, { "fight", 'v' }, { "shine bright", 'v' } },
    { { "day", 'n' }, { "way", 'n' }, { "a-way", 'a' }, { "to-day", 'a' }, { "stay", 'v' }, { "play", 'v' }, { "sway", 'v' } },
    { { "fire", 'n' }, { "de-sire", 'n' }, { "wire", 'n' }, { "high-er", 'a' }, { "in-spire", 'v' }, { "nev-er tire", 'v' } },
    { { "rain", 'n' }, { "chain", 'n' }, { "lane", 'n' }, { "train", 'n' }, { "a-gain", 'a' }, { "in vain", 'a' }, { "re-main", 'v' }, { "ex-plain", 'v' } },
    { { "sky", 'n' }, { "high", 'a' }, { "good-bye", 'a' }, { "close by", 'a' }, { "fly", 'v' }, { "cry", 'v' }, { "sigh", 'v' }, { "try", 'v' } },
    { { "heart", 'n' }, { "art", 'n' }, { "start", 'n' }, { "a-part", 'a' }, { "from the start", 'a' }, { "start", 'v' }, { "play our part", 'v' } },
    { { "town", 'n' }, { "crown", 'n' }, { "down", 'a' }, { "a-round", 'a' }, { "drown", 'v' }, { "turn a-round", 'v' } },
    { { "sea", 'n' }, { "tree", 'n' }, { "key", 'n' }, { "free", 'a' }, { "with me", 'a' }, { "see", 'v' }, { "be", 'v' }, { "break free", 'v' } },
    { { "snow", 'n' }, { "show", 'n' }, { "glow", 'n' }, { "slow", 'a' }, { "be-low", 'a' }, { "long a-go", 'a' }, { "go", 'v' }, { "know", 'v' }, { "flow", 'v' }, { "grow", 'v' } },
    { { "stars", 'n' }, { "cars", 'n' }, { "scars", 'n' }, { "gui-tars", 'n' }, { "ours", 'a' } },
    { { "flame", 'n' }, { "name", 'n' }, { "game", 'n' }, { "frame", 'n' }, { "the same", 'a' }, { "call your name", 'v' } },
    { { "ride", 'n' }, { "side", 'n' }, { "tide", 'n' }, { "wide", 'a' }, { "in-side", 'a' }, { "hide", 'v' }, { "slide", 'v' }, { "glide", 'v' }, { "ride", 'v' } },
    { { "moon", 'n' }, { "tune", 'n' }, { "bal-loon", 'n' }, { "af-ter-noon", 'n' }, { "soon", 'a' }, { "hum a tune", 'v' } },
    { { "gold", 'n' }, { "cold", 'a' }, { "bold", 'a' }, { "hold", 'v' }, { "nev-er fold", 'v' } },
    { { "wall", 'n' }, { "call", 'n' }, { "hall", 'n' }, { "tall", 'a' }, { "small", 'a' }, { "af-ter all", 'a' }, { "fall", 'v' }, { "call", 'v' } },
    { { "dream", 'n' }, { "stream", 'n' }, { "beam", 'n' }, { "team", 'n' }, { "it seems", 'a' }, { "dream", 'v' }, { "gleam", 'v' }, { "scream", 'v' } },
    { { "purr", 'n' }, { "fur", 'n' }, { "blur", 'n' }, { "with her", 'a' }, { "stir", 'v' }, { "purr", 'v' } },
};
const std::vector<const char*> subjects = { "I", "we", "you", "they" };
const std::vector<const char*> preps = { "through", "in-to", "un-der", "a-cross", "in", "to", "with", "a-bove", "be-yond", "from" };
// R = a rhyming thing, Q = a rhyming how / where, W = a rhyming action
const std::vector<const char*> lineTemplates = {
    "S V P the R", "S V P the A R", "P the A R", "V with me P the R", "S will V P the R", "A R", "all the A R",
    "we are the R", "S V P the N of R", "oh S V Q", "and S V Q", "S V Q", "S will V Q", "V with me Q", "S V P the N Q",
    "S V and W", "S can W", "S will W", "let us W", "S V P the N and W", "oh S W",
};
// pieces to fill the start of a long line (no rhyme)
const std::vector<const char*> fillTemplates = { "S V", "A N", "S V P the N", "oh", "yeah", "so", "and", "now", "the A N", "S V and V", "oh oh" };

int syllables (const juce::String& word) { return word.isEmpty() ? 0 : 1 + word.retainCharacters ("-").length(); }
int syllables (const juce::StringArray& words) { int n = 0; for (auto& w : words) n += syllables (w); return n; }

//==============================================================================
juce::String chordQuality (const std::vector<int>& iv)
{
    auto has = [&] (int i) { return std::find (iv.begin(), iv.end(), i) != iv.end(); };
    const bool maj3 = has (4), min3 = has (3), sus4 = has (5) && ! maj3 && ! min3, sus2 = has (2) && ! maj3 && ! min3;
    const bool dim5 = has (6) && ! has (7), aug5 = has (8) && ! has (7) && maj3;
    const bool maj7 = has (11), min7 = has (10), nine = has (14);
    juce::String q;
    if (sus4) q = min7 ? "7sus4" : "sus4";
    else if (sus2) q = "sus2";
    else if (min3 && dim5) q = min7 ? "m7b5" : "dim";
    else if (aug5) q = "aug";
    else if (min3) q = maj7 ? "m(maj7)" : (min7 ? (nine ? "m9" : "m7") : (nine ? "m(add9)" : "m"));
    else q = maj7 ? (nine ? "maj9" : "maj7") : (min7 ? (nine ? "9" : "7") : (nine ? "add9" : ""));
    return q;
}

//==============================================================================
/** Does the actual writing; works on a Song in place. */
class Writer
{
public:
    explicit Writer (Song& songToWrite) : s (songToWrite), g (genreInfo[juce::jlimit (0, NumGenres - 1, s.settings.genre)]),
                                          genre (juce::jlimit (0, NumGenres - 1, s.settings.genre)),
                                          mood (juce::jlimit (0, NumMoods - 1, s.settings.mood)) {}

    Rng partRng (int target, u64 salt) const
    {
        return Rng (mix (mix (s.settings.seed, (u64) target * 7919u + s.partSeeds[(size_t) target]), salt));
    }

    //==========================================================================
    void basics()
    {
        Rng r (mix (s.settings.seed, 77));
        s.key = s.settings.key >= 0 ? s.settings.key % 12 : r.range (0, 11);
        static const double moodScales[NumMoods][7] = {
            { 7, 0, 0, 2, 1, 0, 0 },   // Happy: Major, Mixolydian, Lydian
            { 0, 6, 3, 0, 0, 1, 0 },   // Sad
            { 0, 4, 0, 0, 0, 3, 3 },   // Dark
            { 4, 0, 2, 0, 4, 0, 0 },   // Dreamy
            { 2, 6, 0, 0, 0, 2, 0 },   // Epic
            { 4, 2, 4, 0, 0, 0, 0 },   // Chill
        };
        double w[7];
        for (int i = 0; i < 7; ++i) w[i] = moodScales[mood][i];
        if (genre == Jazz) w[Phrygian] = 0;
        if (genre == Rock && mood == Happy) w[Mixolydian] += 3;
        s.scale = r.weighted (w, 7);
        s.bpm = s.settings.bpm > 0.0 ? s.settings.bpm : (double) juce::roundToInt (g.bpmLo + r.uni() * (g.bpmHi - g.bpmLo));
        if (mood == Sad || mood == Dreamy || mood == Chill) s.bpm = juce::jmax (g.bpmLo, s.bpm - 6.0);
        s.swing = (float) (g.swingLo + r.uni() * (g.swingHi - g.swingLo));
        s.chordBeats = g.chordBeats[r.range (0, 1)];
        if (s.settings.bpm > 0.0) s.bpm = s.settings.bpm;
    }

    //==========================================================================
    // harmony
    std::vector<int> fromTemplate (const std::vector<int>& t, Rng& r)
    {
        std::vector<int> p;
        for (int v : t) p.push_back (code (v % 10, v / 10, chordExt (r)));
        return p;
    }

    int chordExt (Rng& r)
    {
        int e = 0;
        if (r.chance (g.p7)) e |= ext7;
        if (r.chance (g.p9)) e |= ext9;
        if (genre == Ambient && r.chance (0.4)) e |= ext9;
        return e;
    }

    std::vector<int> markovProgression (Rng& r, int first, bool endOnDominant)
    {
        const auto& table = isMinorScale (s.scale) ? minorMarkov : majorMarkov;
        std::vector<int> best;
        double bestScore = -1e9;
        for (int attempt = 0; attempt < 40; ++attempt)
        {
            std::vector<int> d { first };
            for (int i = 1; i < 4; ++i) d.push_back (r.weighted (table[d.back()], 7));
            double score = table[d.back()][d.front()] + r.uni();   // loops back nicely
            if (endOnDominant) score += (d.back() == 4 || d.back() == 6) ? 4.0 : -2.0;
            for (int i = 1; i < 4; ++i) if (d[i] == d[i - 1]) score -= 3.0;
            int distinct = 0;
            for (int i = 0; i < 7; ++i) if (std::find (d.begin(), d.end(), i) != d.end()) ++distinct;
            score += distinct;
            for (int x : d) if ((isMinorScale (s.scale) && x == 1) || (! isMinorScale (s.scale) && x == 6)) score -= 3.0;   // diminished
            if (score > bestScore) { bestScore = score; best = d; }
        }
        std::vector<int> p;
        for (int x : best) p.push_back (code (x, 0, chordExt (r)));
        return p;
    }

    const std::vector<std::vector<int>>& mainTemplates() const
    {
        if (s.scale == Dorian) return dorianTemplates;
        if (s.scale == Phrygian) return phrygianTemplates;
        return isMinorScale (s.scale) ? minorTemplates : majorTemplates;
    }

    void addColour (std::vector<int>& p, Rng& r)
    {
        // borrowed chords and dominants make it less "on rails"
        for (auto& c : p)
        {
            const int d = degreeOf (c), v = variantOf (c);
            if (v != 0) continue;
            if (isMinorScale (s.scale) && d == 4 && (mood == Dark || mood == Epic || genre == Jazz || genre == Cinematic) && r.chance (0.6))
                c = code (4, VarMajor, extOf (c) | (genre == Jazz ? ext7 : 0));
            else if (! isMinorScale (s.scale) && d == 3 && (mood == Sad || mood == Dreamy) && r.chance (0.25))
                c = code (3, VarMinor, extOf (c));
            else if (d == 4 && (genre == Pop || genre == Cinematic || genre == Rock) && r.chance (0.12))
                c = code (4, VarSus4, 0);
            else if (genre == Ambient && r.chance (0.15))
                c = code (d, VarSus2, 0);
        }
    }

    void makeProgressions()
    {
        Rng r = partRng (NewChords, 0);
        const bool minor = isMinorScale (s.scale);
        const auto& main = mainTemplates();
        s.progressions.clear();

        auto pickMain = [&] (const std::vector<int>* avoid)
        {
            for (int tries = 0; tries < 10; ++tries)
            {
                std::vector<int> p;
                if (r.chance (0.55)) p = fromTemplate (r.pick (main), r);
                else
                {
                    static const double firstW[7] = { 6, 0.5, 0.5, 2, 0.5, 2, 0.5 };
                    p = markovProgression (r, r.weighted (firstW, 7), false);
                }
                if (avoid == nullptr || degreesOf (p) != degreesOf (*avoid)) { addColour (p, r); return p; }
            }
            return fromTemplate (main.front(), r);
        };
        auto verse = pickMain (nullptr);
        auto chorus = pickMain (&verse);
        auto pre = fromTemplate (r.pick (minor ? minorPre : majorPre), r);
        auto bridge = r.chance (0.6) ? fromTemplate (r.pick (minor ? minorBridge : majorBridge), r) : markovProgression (r, minor ? 5 : 5, true);
        addColour (bridge, r);
        s.progressions = { verse, chorus, pre, bridge };
    }

    static std::vector<int> degreesOf (const std::vector<int>& p) { std::vector<int> d; for (int c : p) d.push_back (degreeOf (c)); return d; }

    /** Scale degree to pitch class (borrowed = from the parallel natural minor). */
    int degreePc (int degree, int scale) const { return (s.key + scaleSteps[scale][((degree % 7) + 7) % 7]) % 12; }

    Chord buildChord (int c, double start, double length) const
    {
        const int d = degreeOf (c), v = variantOf (c), e = extOf (c);
        const int sc = v == VarBorrowed ? Minor : s.scale;
        Chord ch;
        ch.start = start; ch.length = length;
        ch.root = degreePc (d, sc);
        auto rel = [&] (int deg) { return ((degreePc (deg, sc) - ch.root) % 12 + 12) % 12; };
        int third = rel (d + 2), fifth = rel (d + 4), seventh = rel (d + 6);
        if (v == VarMajor) { third = 4; fifth = 7; if (e & ext7) seventh = 10; }
        if (v == VarMinor) { third = 3; fifth = 7; }
        if (fifth == 8 && third == 4) fifth = 7;                     // no augmented triads (harmonic minor III)
        if (fifth == 6 && genre != Jazz && genre != Cinematic && genre != LoFi) fifth = 7;   // soften diminished
        if (v == VarSus4) third = 5;
        if (v == VarSus2) third = 2;
        ch.intervals = { 0, third, fifth };
        if (e & ext7) ch.intervals.push_back (seventh);
        if (e & ext9) ch.intervals.push_back (14);
        ch.name = keyNameList[ch.root] + chordQuality (ch.intervals);
        return ch;
    }

    void voice (Chord& ch, const std::vector<int>& prev) const
    {
        // try every inversion; pick the one closest to the last chord, kept in E3..D5
        std::vector<int> ivs (ch.intervals.begin(), ch.intervals.end());
        if (ivs.size() > 4) ivs.erase (ivs.begin() + 2);      // 9th chords: drop the 5th
        for (auto& iv : ivs) iv %= 12;
        std::sort (ivs.begin(), ivs.end());                    // close position: tones in order round the octave
        std::vector<int> pcs;
        for (int iv : ivs) pcs.push_back ((ch.root + iv) % 12);
        std::vector<int> best;
        double bestCost = 1e9;
        for (size_t inv = 0; inv < pcs.size(); ++inv)
            for (int base = 45; base < 66; ++base)
            {
                if (base % 12 != pcs[inv]) continue;
                std::vector<int> v { base };
                for (size_t k = 1; k < pcs.size(); ++k)
                {
                    int n = v.back() + 1;
                    while (n % 12 != pcs[(inv + k) % pcs.size()]) ++n;
                    v.push_back (n);
                }
                if (v.back() > 79) continue;
                double cost = 0.0;
                if (prev.empty()) { double mean = 0; for (int n : v) mean += n; mean /= (double) v.size(); cost = std::abs (mean - 62.0); }
                else
                    for (int n : v)
                    {
                        int dmin = 99;
                        for (int p : prev) dmin = juce::jmin (dmin, std::abs (n - p));
                        cost += dmin;
                    }
                double mean = 0; for (int n : v) mean += n; mean /= (double) v.size();
                cost += std::abs (mean - 62.0) * 0.35;           // don't drift away
                if (cost < bestCost) { bestCost = cost; best = v; }
            }
        if (best.empty()) for (int p : pcs) best.push_back (60 + p);
        ch.voicing = best;
    }

    void layChords()
    {
        s.chords.clear();
        std::vector<int> prevVoicing;
        for (size_t si = 0; si < s.sections.size(); ++si)
        {
            const auto& sec = s.sections[si];
            const auto& prog = s.progressions[(size_t) juce::jlimit (0, (int) s.progressions.size() - 1, sec.progression)];
            const double hb = sec.type == Outro && s.chordBeats < 4 ? 4.0 : s.chordBeats;
            int i = 0;
            for (double t = sec.start; t < sec.end() - 1e-6; t += hb, ++i)
            {
                int c = prog[(size_t) i % prog.size()];
                const double len = juce::jmin (hb, sec.end() - t);
                const bool last = t + hb >= sec.end() - 1e-6;
                if (last && sec.type == Outro && si + 1 == s.sections.size()) c = code (0, 0, extOf (c));   // end home
                if (last && si + 1 < s.sections.size())
                {
                    const int nextType = s.sections[si + 1].type;
                    // lead into the chorus / drop with a dominant if it would otherwise sit on the tonic
                    if ((nextType == Chorus || nextType == Drop) && nextType != sec.type && degreeOf (c) == 0)
                        c = isMinorScale (s.scale) ? code (6, 0, extOf (c)) : code (4, 0, extOf (c));
                }
                auto ch = buildChord (c, t, len);
                voice (ch, prevVoicing);
                prevVoicing = ch.voicing;
                s.chords.push_back (std::move (ch));
            }
        }
    }

    //==========================================================================
    // arrangement
    struct FormPart { int type, bars; };

    std::vector<FormPart> formFor (Rng& r) const
    {
        const bool alt = r.chance (0.4);
        switch (genre)
        {
            case Pop:  return alt ? std::vector<FormPart> { { Intro, 4 }, { Verse, 8 }, { Chorus, 8 }, { Verse, 8 }, { Chorus, 8 }, { Bridge, 8 }, { Chorus, 8 }, { Chorus, 8 }, { Outro, 4 } }
                                  : std::vector<FormPart> { { Intro, 4 }, { Verse, 8 }, { PreChorus, 4 }, { Chorus, 8 }, { Verse, 8 }, { PreChorus, 4 }, { Chorus, 8 }, { Bridge, 8 }, { Chorus, 8 }, { Outro, 4 } };
            case LoFi: return alt ? std::vector<FormPart> { { Intro, 4 }, { Verse, 8 }, { Verse, 8 }, { Chorus, 8 }, { Bridge, 8 }, { Chorus, 8 }, { Outro, 4 } }
                                  : std::vector<FormPart> { { Intro, 4 }, { Verse, 8 }, { Chorus, 8 }, { Verse, 8 }, { Chorus, 8 }, { Outro, 4 } };
            case Trap: return alt ? std::vector<FormPart> { { Intro, 4 }, { Verse, 16 }, { Chorus, 8 }, { Verse, 16 }, { Chorus, 8 }, { Outro, 4 } }
                                  : std::vector<FormPart> { { Intro, 4 }, { Chorus, 8 }, { Verse, 16 }, { Chorus, 8 }, { Verse, 8 }, { Chorus, 8 }, { Outro, 4 } };
            case House: return alt ? std::vector<FormPart> { { Intro, 8 }, { Verse, 8 }, { Build, 8 }, { Drop, 16 }, { Breakdown, 8 }, { Build, 4 }, { Drop, 16 }, { Outro, 8 } }
                                   : std::vector<FormPart> { { Intro, 8 }, { Build, 8 }, { Drop, 16 }, { Breakdown, 8 }, { Build, 8 }, { Drop, 16 }, { Outro, 8 } };
            case Ambient: return alt ? std::vector<FormPart> { { Intro, 8 }, { Verse, 8 }, { Verse, 8 }, { Chorus, 8 }, { Outro, 8 } }
                                     : std::vector<FormPart> { { Intro, 8 }, { Verse, 8 }, { Chorus, 8 }, { Breakdown, 8 }, { Chorus, 8 }, { Outro, 8 } };
            case Jazz: return alt ? std::vector<FormPart> { { Intro, 4 }, { Verse, 8 }, { Bridge, 8 }, { Verse, 8 }, { Verse, 8 }, { Outro, 4 } }
                                  : std::vector<FormPart> { { Intro, 4 }, { Verse, 8 }, { Verse, 8 }, { Bridge, 8 }, { Verse, 8 }, { Outro, 4 } };
            case Rock: return alt ? std::vector<FormPart> { { Intro, 4 }, { Verse, 8 }, { PreChorus, 4 }, { Chorus, 8 }, { Verse, 8 }, { PreChorus, 4 }, { Chorus, 8 }, { Bridge, 8 }, { Chorus, 8 }, { Chorus, 8 }, { Outro, 4 } }
                                  : std::vector<FormPart> { { Intro, 4 }, { Verse, 8 }, { Chorus, 8 }, { Verse, 8 }, { Chorus, 8 }, { Bridge, 8 }, { Chorus, 8 }, { Outro, 4 } };
            default:   return alt ? std::vector<FormPart> { { Intro, 8 }, { Verse, 8 }, { Chorus, 8 }, { Breakdown, 8 }, { Build, 8 }, { Chorus, 8 }, { Outro, 8 } }
                                  : std::vector<FormPart> { { Intro, 8 }, { Verse, 8 }, { Build, 8 }, { Chorus, 8 }, { Breakdown, 8 }, { Build, 4 }, { Chorus, 8 }, { Outro, 8 } };
        }
    }

    static int progressionFor (int type, Rng& r)
    {
        switch (type)
        {
            case Verse: return 0;
            case PreChorus: case Build: return 2;
            case Chorus: case Drop: return 1;
            case Bridge: return 3;
            case Breakdown: return r.chance (0.5) ? 3 : 1;
            case Intro: return r.chance (0.5) ? 0 : 1;
            default: return r.chance (0.5) ? 1 : 0;
        }
    }

    static float baseEnergy (int type)
    {
        static const float e[NumSectionTypes] = { 0.3f, 0.5f, 0.65f, 0.85f, 0.55f, 0.7f, 1.0f, 0.3f, 0.3f };
        return e[juce::jlimit (0, NumSectionTypes - 1, type)];
    }

    void decideParts (Section& sec, Rng& r) const
    {
        auto& on = sec.on;
        on.fill (false);
        on[Chords] = true;
        switch (sec.type)
        {
            case Intro:
                on[Counter] = genre == Ambient || genre == Cinematic || r.chance (0.5);
                on[Drums] = genre == House || ((genre == Rock || genre == LoFi) && r.chance (0.5));
                break;
            case Verse:
                on[Bass] = on[Melody] = true;
                on[Drums] = genre != Ambient || r.chance (0.4);
                on[Counter] = genre == Rock || genre == Cinematic || r.chance (0.35);
                break;
            case PreChorus:
                on[Bass] = on[Drums] = on[Melody] = true;
                on[Counter] = r.chance (0.6);
                break;
            case Chorus: case Drop:
                on[Bass] = on[Melody] = on[Counter] = true;
                on[Drums] = genre != Ambient || r.chance (0.6);
                break;
            case Bridge:
                on[Bass] = on[Melody] = on[Counter] = true;
                on[Drums] = r.chance (0.5);
                break;
            case Build:
                on[Counter] = on[Drums] = true;
                on[Bass] = genre == Cinematic;
                break;
            case Breakdown:
                on[Counter] = true;
                on[Melody] = r.chance (0.5);
                break;
            case Outro:
                on[Counter] = r.chance (0.5);
                on[Drums] = genre == House || r.chance (0.3);
                on[Bass] = genre == House;
                break;
            default: break;
        }
        if (genre == Jazz && sec.type != Intro) on[Counter] = r.chance (0.3);
    }

    void makeForm()
    {
        Rng r = partRng (NewArrangement, (u64) s.chunk);
        s.sections.clear();
        std::vector<FormPart> form;
        if (s.settings.endless) form = endlessForm (r, s.chunk == 0 ? -1 : lastTypeHint, s.chunk == 0);
        else form = formFor (r);
        double t = 0.0;
        int choruses = 0;
        for (auto& f : form)
        {
            Section sec;
            sec.type = f.type;
            sec.bars = f.bars;
            sec.start = t;
            sec.energy = baseEnergy (f.type);
            if (f.type == Chorus || f.type == Drop) sec.energy = juce::jmin (1.0f, sec.energy + 0.05f * (float) choruses++);
            if (s.settings.endless) sec.energy = juce::jlimit (0.2f, 1.0f, sec.energy + (float) (r.uni() * 0.2 - 0.1));
            Rng pr (mix (r.next(), (u64) f.type));
            sec.progression = progressionFor (f.type, pr);
            decideParts (sec, pr);
            s.sections.push_back (sec);
            t = sec.end();
        }
        s.lengthBeats = t;
    }

    int lastTypeHint = -1;

    std::vector<FormPart> endlessForm (Rng& r, int lastType, bool first) const
    {
        std::vector<FormPart> f;
        if (first) f.push_back ({ Intro, genre == House || genre == Ambient || genre == Cinematic ? 8 : 4 });
        int type = lastType < 0 ? Intro : lastType;
        int bars = 0;
        auto nextOf = [&] (int t) -> int
        {
            const bool edm = genre == House;
            switch (t)
            {
                case Intro: return edm ? Build : Verse;
                case Verse: return genre == Jazz ? (r.chance (0.5) ? Verse : Bridge) : (r.chance (0.45) && genre != Ambient ? PreChorus : (edm ? Build : Chorus));
                case PreChorus: return Chorus;
                case Chorus: { const int o[] = { Verse, Bridge, Breakdown, Verse }; return o[r.range (0, 3)]; }
                case Bridge: return genre == Jazz ? Verse : Chorus;
                case Breakdown: return (edm || genre == Cinematic) ? Build : Verse;
                case Build: return edm ? Drop : Chorus;
                case Drop: return r.chance (0.5) ? Breakdown : Verse;
                default: return Verse;
            }
        };
        while (bars < 28)
        {
            type = nextOf (type);
            if (genre == Ambient && (type == Build || type == Drop)) type = Chorus;
            if (genre != House && type == Drop) type = Chorus;
            int b = (type == PreChorus || type == Build) ? 4 : 8;
            if (type == Drop || (genre == Trap && type == Verse)) b = 16;
            if (type == Build && genre == House) b = 8;
            f.push_back ({ type, b });
            bars += b;
        }
        return f;
    }

    //==========================================================================
    // the chords part
    void writeChordsPart()
    {
        auto& out = s.parts[Chords];
        out.clear();
        Rng r = partRng (NewChords, 500 + (u64) s.chunk);
        std::map<int, std::vector<std::pair<double, double>>> grooves;   // per section type: hits over 2 bars

        for (const auto& sec : s.sections)
        {
            if (! sec.on[Chords]) continue;
            const bool sustain = genre == Ambient || genre == Cinematic || genre == Trap || sec.energy < 0.45f
                                 || sec.type == Intro || sec.type == Breakdown || sec.type == Outro;
            if (sustain)
            {
                for (const auto& ch : s.chords)
                    if (ch.start >= sec.start - 1e-6 && ch.start < sec.end() - 1e-6)
                        addChord (out, ch, ch.start, ch.length * 0.98, 0.5f + sec.energy * 0.2f);
                continue;
            }
            auto& groove = grooves[sec.type];
            if (groove.empty()) groove = chordGroove (sec, r);
            for (int bar = 0; bar < sec.bars; bar += 2)
                for (auto& h : groove)
                {
                    const double t = sec.start + bar * 4.0 + h.first;
                    if (t >= sec.end() - 1e-6) continue;
                    // split at chord changes
                    double a = t, b = juce::jmin (t + h.second, sec.end());
                    while (a < b - 0.2)
                    {
                        const auto* ch = s.chordAt (a + 1e-4);
                        if (ch == nullptr) break;
                        const double e = juce::jmin (b, ch->start + ch->length);
                        addChord (out, *ch, a, (e - a) * 0.95, (std::fmod (h.first, 4.0) < 1e-6 ? 0.62f : 0.52f) + sec.energy * 0.18f);
                        a = e;
                    }
                }
        }
        swingPart (out);
    }

    std::vector<std::pair<double, double>> chordGroove (const Section& sec, Rng& r) const
    {
        std::vector<const char*> pats;
        switch (genre)
        {
            case Pop:   pats = sec.energy > 0.75f ? std::vector<const char*> { "x-----x-----x---", "x---x---x---x---", "x-------x-x-x---" } : std::vector<const char*> { "x-------x-------", "x---------------" }; break;
            case LoFi:  pats = { "x---------------", "x-----------x---", "x-------x-------" }; break;
            case House: pats = sec.energy > 0.6f ? std::vector<const char*> { "..x-..x-..x-..x-", "x--x--x---x--x--", "x-.x-.x-..x-.x-." } : std::vector<const char*> { "x---------------" }; break;
            case Jazz:  pats = { "..x-....x---..x-", "x--x....x-x-....", ".x--..x-..x---..", "x---..x-...x-x--" }; break;
            case Rock:  pats = sec.energy > 0.6f ? std::vector<const char*> { "x-------x-------", "x-----x-x-------" } : std::vector<const char*> { "x---------------" }; break;
            default:    pats = { "x---------------" }; break;
        }
        std::vector<std::pair<double, double>> g;
        for (int bar = 0; bar < 2; ++bar)
            for (auto& h : parseRhythm (r.pick (pats), bar * 4.0))
                g.push_back ({ h.start, h.length });
        return g;
    }

    static void addChord (std::vector<Note>& out, const Chord& ch, double start, double length, float vel)
    {
        for (int n : ch.voicing) out.push_back ({ start, juce::jmax (0.1, length), n, juce::jlimit (0.1f, 1.0f, vel), -1 });
    }

    //==========================================================================
    // bass
    int bassNote (int pc, int lo = 33, int hi = 44) const
    {
        int n = lo + ((pc - lo) % 12 + 12) % 12;
        if (n > hi) n -= 12;
        return n;
    }

    void writeBass()
    {
        auto& out = s.parts[Bass];
        out.clear();
        Rng r = partRng (NewBass, (u64) s.chunk);
        std::map<int, int> patternFor;   // per section type

        for (size_t si = 0; si < s.sections.size(); ++si)
        {
            const auto& sec = s.sections[si];
            if (! sec.on[Bass]) continue;
            if (patternFor.find (sec.type) == patternFor.end()) patternFor[sec.type] = r.range (0, 3);
            const int pat = patternFor[sec.type];
            const float vel = 0.7f + sec.energy * 0.2f;

            if (genre == Trap && sec.on[Drums]) { trapBass (sec, vel, r); continue; }

            for (const auto& ch : s.chords)
            {
                if (ch.start < sec.start - 1e-6 || ch.start >= sec.end() - 1e-6) continue;
                const int root = bassNote (ch.root, genre == Trap ? 30 : 33, genre == Trap ? 41 : 44);
                const int fifth = root + 7 <= 50 ? root + 7 : root - 5;
                const auto* nextCh = s.chordAt (ch.start + ch.length + 1e-4);
                const int nextRoot = nextCh != nullptr ? bassNote (nextCh->root) : root;
                const double a = ch.start, L = ch.length;

                auto add = [&] (double t, double len, int pitch, float v)
                {
                    if (t < sec.end() - 1e-6 && t < a + L - 1e-6)
                        out.push_back ({ t, juce::jmin (len, a + L - t) * 0.95, pitch, juce::jlimit (0.1f, 1.0f, v), -1 });
                };
                auto approach = [&] (double t)
                {
                    // a step into the next chord's root
                    if (nextRoot == root) { add (t, 0.5, root + 12 <= 52 ? root + 12 : root, vel * 0.8f); return; }
                    const int dir = nextRoot > root ? 1 : -1;
                    const int n = genre == Jazz ? nextRoot - dir : nearestScaleNote (nextRoot - dir * 2, dir);
                    add (t, 0.5, n, vel * 0.85f);
                };

                switch (genre)
                {
                    case Ambient:
                        add (a, L, root, vel * 0.8f);
                        break;
                    case Cinematic:
                        if (sec.energy > 0.7f) for (double t = a; t < a + L - 1e-6; t += 0.5) add (t, 0.45, std::fmod (t, 1.0) < 0.25 ? root : root + 12, vel);
                        else add (a, L, root, vel);
                        break;
                    case Jazz:
                    {
                        // walking bass: root, chord / scale tones, approach note
                        int cur = root;
                        for (double t = a; t < a + L - 1e-6; t += 1.0)
                        {
                            const bool lastBeat = t + 1.0 >= a + L - 1e-6;
                            int n = cur;
                            if (t == a) n = root;
                            else if (lastBeat) n = nextRoot + (r.chance (0.5) ? 1 : -1);
                            else
                            {
                                const int tones[] = { 0, ch.intervals.size() > 1 ? ch.intervals[1] : 4, 7, ch.intervals.size() > 3 ? ch.intervals[3] : 12 };
                                n = root + tones[r.range (1, 3)];
                                if (n > 52) n -= 12;
                            }
                            add (t, 0.95, n, vel * (t == a ? 1.0f : 0.85f));
                            cur = n;
                        }
                        break;
                    }
                    case House:
                        for (double t = a + 0.5; t < a + L - 1e-6; t += 1.0)
                            add (t, 0.4, (pat == 1 && std::fmod (t, 2.0) > 1.0) ? root + 12 : root, vel);
                        break;
                    case Rock:
                        for (double t = a; t < a + L - 1e-6; t += sec.energy > 0.55f ? 0.5 : 1.0)
                            add (t, 0.45, root, vel * (std::fmod (t, 1.0) < 0.25 ? 1.0f : 0.85f));
                        break;
                    case LoFi:
                    {
                        static const char* pats[] = { "x-----x---x-----", "x--------x--x---", "x-------x-----x-", "x-----x-x-------" };
                        for (double bar = a; bar < a + L - 1e-6; bar += 4.0)
                        {
                            int k = 0;
                            for (auto& h : parseRhythm (pats[pat], bar))
                                add (h.start, h.length, k++ == 0 ? root : (r.chance (0.5) ? fifth : root + 12 <= 52 ? root + 12 : root), vel * (k == 1 ? 1.0f : 0.8f));
                        }
                        if (L >= 4.0 && r.chance (0.5)) approach (a + L - 0.5);
                        break;
                    }
                    default:   // Pop, Trap without drums
                    {
                        static const char* pats[] = { "x-------x-x-----", "x--x--x-x--x--x-", "x---x---x---x---", "x-x-x-x-x-x-x-x-" };
                        const char* p = sec.energy < 0.6f ? "x-------x-------" : pats[pat];
                        for (double bar = a; bar < a + L - 1e-6; bar += 4.0)
                        {
                            int k = 0;
                            for (auto& h : parseRhythm (p, bar))
                            {
                                const bool down = std::fmod (h.start - bar, 4.0) < 1e-6;
                                add (h.start, h.length, down || r.chance (0.7) ? root : (r.chance (0.5) ? fifth : root + 12), vel * (down ? 1.0f : 0.85f));
                                ++k;
                            }
                        }
                        if (L >= 4.0 && sec.energy >= 0.6f && r.chance (0.6)) { removeFrom (out, a + L - 0.5, a + L); approach (a + L - 0.5); }
                        break;
                    }
                }
            }
        }
        std::sort (out.begin(), out.end(), [] (const Note& x, const Note& y) { return x.start < y.start; });
        // one bass note at a time
        for (size_t i = 0; i + 1 < out.size(); ++i)
            out[i].length = juce::jmin (out[i].length, juce::jmax (0.05, out[i + 1].start - out[i].start - 0.02));
        swingPart (out);
    }

    static void removeFrom (std::vector<Note>& v, double a, double b)
    {
        v.erase (std::remove_if (v.begin(), v.end(), [=] (const Note& n) { return n.start >= a - 1e-6 && n.start < b - 1e-6; }), v.end());
    }

    int nearestScaleNote (int pitch, int dir) const
    {
        for (int k = 0; k < 12; ++k)
        {
            const int n = pitch + dir * k;
            const int rel = ((n - s.key) % 12 + 12) % 12;
            for (int st : scaleSteps[s.scale]) if (st == rel) return n;
        }
        return pitch;
    }

    void trapBass (const Section& sec, float vel, Rng& r)
    {
        // the 808 follows the kick and holds until the next one
        auto& out = s.parts[Bass];
        std::vector<double> kicks;
        for (auto& n : s.parts[Drums]) if (n.pitch == drum::kick && n.start >= sec.start - 0.01 && n.start < sec.end() - 0.01) kicks.push_back (n.start);
        std::sort (kicks.begin(), kicks.end());
        kicks.erase (std::unique (kicks.begin(), kicks.end(), [] (double x, double y) { return std::abs (x - y) < 0.05; }), kicks.end());
        if (kicks.empty() || kicks.front() > sec.start + 0.01) kicks.insert (kicks.begin(), sec.start);
        for (size_t i = 0; i < kicks.size(); ++i)
        {
            const double t = kicks[i];
            const double end = i + 1 < kicks.size() ? kicks[i + 1] : sec.end();
            const auto* ch = s.chordAt (t + 1e-4);
            if (ch == nullptr) continue;
            int n = bassNote (ch->root, 30, 41);
            if (r.chance (0.12)) n += 12;
            out.push_back ({ t, juce::jmin (end - t, 3.0) * 0.96, n, vel, -1 });
        }
    }

    //==========================================================================
    // drums
    struct Layer { int note; std::vector<const char*> variants; float vel; bool hat; };

    std::vector<Layer> kit() const
    {
        using namespace drum;
        switch (genre)
        {
            case Pop:  return { { kick, { "9000000090000000", "9000000090900000", "9000003090000000" }, 0.95f, false }, { snare, { "0000900000009000" }, 0.9f, false },
                                { hat, { "9090909090909090", "9090909090909099" }, 0.6f, true }, { openHat, { "0000000000000000", "0000000000000050" }, 0.5f, true } };
            case LoFi: return { { kick, { "9000003090000000", "9000000009500000", "9000000390000300" }, 0.9f, false }, { snare, { "0000900000009000" }, 0.8f, false },
                                { hat, { "9090909090909090", "9070907090709070" }, 0.45f, true }, { rim, { "0000002000000020" }, 0.3f, true } };
            case Trap: return { { kick, { "9000000300900000", "9000000000903000", "9000000090000300" }, 1.0f, false }, { clap, { "0000000090000000" }, 0.9f, false },
                                { hat, { "9999999999999999", "9909990999099909" }, 0.5f, true }, { openHat, { "0000000000000005" }, 0.45f, true } };
            case House: return { { kick, { "9000900090009000" }, 1.0f, false }, { clap, { "0000900000009000" }, 0.85f, false },
                                 { openHat, { "0090009000900090" }, 0.55f, true }, { hat, { "5555555555555555", "0505050505050505" }, 0.35f, true } };
            case Ambient: return { { kick, { "9000000000000000", "9000000000900000" }, 0.7f, false }, { rim, { "0000000090000000" }, 0.45f, false },
                                   { shaker, { "5050505050505050" }, 0.3f, true } };
            case Jazz: return { { ride, { "9000907090009070" }, 0.55f, true }, { hat, { "0000900000009000" }, 0.4f, false },
                                { kick, { "2000000000000000", "2000000000200000" }, 0.4f, true }, { snare, { "0002000200020002", "0000000200200000" }, 0.3f, true } };
            case Rock: return { { kick, { "9000000090900000", "9000000090000090", "9000000990000000" }, 1.0f, false }, { snare, { "0000900000009000" }, 0.95f, false },
                                { hat, { "9090909090909090" }, 0.65f, true } };
            default:   return { { kick, { "9000000000000000", "9000000090000000" }, 1.0f, false }, { tomLow, { "0000009000000090", "0000000000909090" }, 0.8f, true },
                                { snare, { "0000000000000000", "0000000090000000" }, 0.7f, false } };
        }
    }

    void writeDrums()
    {
        auto& out = s.parts[Drums];
        out.clear();
        const auto layers = kit();
        std::map<int, std::vector<std::vector<bool>>> grooves;   // per section type: per layer, 32 steps

        for (size_t si = 0; si < s.sections.size(); ++si)
        {
            const auto& sec = s.sections[si];
            const int nextType = si + 1 < s.sections.size() ? s.sections[si + 1].type : -1;
            const bool bigNext = nextType == Chorus || nextType == Drop || (nextType >= 0 && s.sections[si + 1].energy > sec.energy + 0.15f);
            Rng fr = partRng (NewDrums, 1000 + si + (u64) s.chunk * 100);

            // crash on big arrivals
            if (si > 0 && (sec.type == Chorus || sec.type == Drop || sec.energy >= 0.75f || s.sections[si - 1].type == Build) && sec.on[Drums])
                out.push_back ({ sec.start, 0.5, drum::crash, 0.8f, -1 });
            if (si == 0 && s.chunk > 0 && sec.on[Drums])
                out.push_back ({ sec.start, 0.5, drum::crash, 0.7f, -1 });
            if (! sec.on[Drums]) continue;

            if (sec.type == Build) { writeBuild (sec, nextType); continue; }

            auto& groove = grooves[sec.type];
            if (groove.empty())
            {
                Rng gr = partRng (NewDrums, 50 + (u64) sec.type);
                for (const auto& l : layers)
                {
                    const char* pat = gr.pick (l.variants);
                    std::vector<bool> steps (32, false);
                    for (int i = 0; i < 32; ++i)
                    {
                        const int d = pat[i % 16] - '0';
                        if (d >= 9) steps[(size_t) i] = true;
                        else if (d > 0) steps[(size_t) i] = gr.uni() < (d / 9.0) * (0.4 + sec.energy * 0.8);
                        // quiet sections: hats on the beat only
                        if (l.hat && l.note == drum::hat && sec.energy < 0.4f && i % 4 != 0 && genre != Trap) steps[(size_t) i] = false;
                    }
                    groove.push_back (steps);
                }
            }

            const bool intro = sec.type == Intro;
            for (int bar = 0; bar < sec.bars; ++bar)
            {
                const double barStart = sec.start + bar * 4.0;
                const bool lastBar = bar == sec.bars - 1;
                const bool fillBar = (lastBar && (bigNext || fr.chance (0.5))) || (bar % 8 == 7 && ! lastBar && fr.chance (0.5));
                const int fillFrom = fillBar ? (lastBar && bigNext ? 8 : 12) : 16;

                for (size_t li = 0; li < layers.size(); ++li)
                {
                    const auto& l = layers[li];
                    if (intro && genre == House && bar < sec.bars / 2 && l.note != drum::kick) continue;
                    for (int st = 0; st < fillFrom; ++st)
                    {
                        if (! groove[li][(size_t) ((bar % 2) * 16 + st)]) continue;
                        float v = l.vel;
                        if (l.hat) v *= (st % 4 == 0 ? 1.0f : (st % 2 == 0 ? 0.85f : 0.7f));
                        v *= 0.85f + sec.energy * 0.2f;
                        int note = l.note;
                        if (genre == Rock && note == drum::hat && sec.energy > 0.8f) note = drum::ride;
                        if (genre == Pop && note == drum::snare && sec.type == Chorus) note = drum::clap;
                        if (genre == Trap && note == drum::hat && st % 4 == 0 && fr.chance (0.16 * sec.energy))
                        {
                            hatRoll (out, barStart + st * 0.25, fr);
                            st += 3;
                            continue;
                        }
                        out.push_back ({ barStart + st * 0.25, 0.12, note, v, -1 });
                    }
                }
                if (fillBar) writeFill (out, barStart, fillFrom, fr, sec.energy);
            }
        }

        // humanize (velocity only, so it stays tight)
        Rng hr = partRng (NewDrums, 9999);
        for (auto& n : out) n.velocity = juce::jlimit (0.05f, 1.0f, n.velocity + (float) (hr.uni() * 0.1 - 0.05));
        std::sort (out.begin(), out.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        swingPart (out);
    }

    void hatRoll (std::vector<Note>& out, double t, Rng& r)
    {
        const bool triplet = r.chance (0.5);
        const int n = triplet ? 6 : 8;
        const double step = 1.0 / n;
        for (int i = 0; i < n; ++i) out.push_back ({ t + i * step, 0.05, drum::hat, 0.35f + 0.4f * (float) i / (float) n, -1 });
    }

    void writeFill (std::vector<Note>& out, double barStart, int fromStep, Rng& r, float energy)
    {
        using namespace drum;
        out.push_back ({ barStart, 0.12, kick, 0.9f, -1 });   // keep the downbeat
        const int kind = genre == Jazz ? 2 : r.range (0, 2);
        for (int st = fromStep; st < 16; ++st)
        {
            const double t = barStart + st * 0.25;
            const float ramp = 0.5f + 0.5f * (float) (st - fromStep) / (float) juce::jmax (1, 16 - fromStep);
            switch (kind)
            {
                case 0: out.push_back ({ t, 0.1, snare, (0.4f + 0.5f * ramp) * (0.8f + energy * 0.2f), -1 }); break;
                case 1:
                {
                    const int tom = st < fromStep + (16 - fromStep) / 3 ? tomHigh : (st < fromStep + 2 * (16 - fromStep) / 3 ? tomMid : tomLow);
                    if (st % 2 == 0 || r.chance (0.5)) out.push_back ({ t, 0.1, tom, 0.6f + 0.35f * ramp, -1 });
                    break;
                }
                default:
                    if (st % 2 == 0) out.push_back ({ t, 0.1, st % 4 == 0 ? snare : tomMid, 0.45f + 0.4f * ramp, -1 });
                    break;
            }
        }
    }

    void writeBuild (const Section& sec, int nextType)
    {
        // a snare roll that speeds up, then a breath before the drop
        auto& out = s.parts[Drums];
        const double len = sec.bars * 4.0;
        for (double t = 0.0; t < len - 1e-6;)
        {
            const double f = t / len;
            const double step = f < 0.5 ? 1.0 : (f < 0.75 ? 0.5 : 0.25);
            const bool gap = (nextType == Drop || nextType == Chorus) && t >= len - 1.0;
            if (! gap) out.push_back ({ sec.start + t, 0.1, genre == House ? drum::clap : drum::snare, (float) (0.35 + 0.6 * f), -1 });
            if (genre == House && std::fmod (t, 1.0) < 1e-6 && t < len - 4.0) out.push_back ({ sec.start + t, 0.12, drum::kick, 0.9f, -1 });
            t += step;
        }
    }

    //==========================================================================
    // counter line
    void writeCounter()
    {
        auto& out = s.parts[Counter];
        out.clear();
        Rng r = partRng (NewCounter, (u64) s.chunk);
        std::vector<int> bell;   // trap: a one-bar loop of chord-tone indexes (-1 = rest)
        {
            Rng br = partRng (NewCounter, 77);
            for (int i = 0; i < 16; ++i) bell.push_back (br.chance (i % 4 == 0 ? 0.7 : (i % 2 == 0 ? 0.45 : 0.2)) ? br.range (0, 4) : -1);
        }
        const int arpStyle = partRng (NewCounter, 78).range (0, 2);

        for (const auto& sec : s.sections)
        {
            if (! sec.on[Counter]) continue;
            for (const auto& ch : s.chords)
            {
                if (ch.start < sec.start - 1e-6 || ch.start >= sec.end() - 1e-6) continue;
                std::vector<int> tones = ch.voicing;
                std::sort (tones.begin(), tones.end());
                const double a = ch.start, L = juce::jmin (ch.length, sec.end() - a);
                auto add = [&] (double t, double len, int p, float v)
                {
                    if (t < a + L - 1e-6) out.push_back ({ t, juce::jmin (len, a + L - t) * 0.95, p, juce::jlimit (0.1f, 1.0f, v), -1 });
                };
                auto tone = [&] (int i, int octaveUp) { const int n = (int) tones.size(); return tones[(size_t) (((i % n) + n) % n)] + 12 * (octaveUp + (i >= n ? 1 : 0)); };
                const float vel = 0.45f + sec.energy * 0.25f;

                switch (genre)
                {
                    case Rock:
                    {
                        // power chords, palm-muted 8ths
                        const int root = bassNote (ch.root, 40, 51);
                        const double step = sec.energy > 0.6f ? 0.5 : 1.0;
                        for (double t = a; t < a + L - 1e-6; t += step)
                            for (int p : { root, root + 7, root + 12 }) add (t, step * 0.8, p, vel + (std::fmod (t, 1.0) < 0.25 ? 0.1f : 0.0f));
                        break;
                    }
                    case Cinematic:
                    {
                        // string ostinato: root - 5th - octave - 5th
                        const int root = bassNote (ch.root, 48, 59);
                        const int seq[] = { root, root + 7, root + 12, root + 7 };
                        const double step = sec.energy > 0.7f ? 0.25 : 0.5;
                        int k = 0;
                        for (double t = a; t < a + L - 1e-6; t += step) add (t, step * 0.9, seq[k++ % 4], vel * (k % 4 == 1 ? 1.0f : 0.8f));
                        break;
                    }
                    case Trap:
                        for (double bar = a; bar < a + L - 1e-6; bar += 4.0)
                            for (int i = 0; i < 16; ++i)
                                if (bell[(size_t) i] >= 0) add (bar + i * 0.25, 0.22, tone (bell[(size_t) i], 1), vel * (i % 4 == 0 ? 1.0f : 0.8f));
                        break;
                    case Ambient:
                    case LoFi:
                    case Jazz:
                    {
                        // a few sparkling chord tones
                        const int n = genre == Ambient ? r.range (1, 3) : r.range (2, 3);
                        for (int i = 0; i < n; ++i)
                        {
                            const double t = a + r.range (0, (int) (L * 2.0) - 1) * 0.5;
                            add (t, genre == Ambient ? L - (t - a) : 0.45, tone (r.range (0, 3), 1), vel * 0.85f);
                        }
                        break;
                    }
                    default:
                    {
                        // Pop / House: arpeggio (8ths, 16ths in a house drop)
                        const double step = (genre == House && sec.type == Drop) ? 0.25 : (sec.energy > 0.7f ? 0.5 : 1.0);
                        int k = 0;
                        for (double t = a; t < a + L - 1e-6; t += step, ++k)
                        {
                            int idx = k;
                            if (arpStyle == 1) { const int n = (int) tones.size() * 2 - 2; idx = n > 0 ? (k % n < (int) tones.size() ? k % n : n - k % n) : 0; }
                            if (arpStyle == 2) { static const int p[] = { 0, 2, 1, 3, 2, 4, 1, 3 }; idx = p[k % 8]; }
                            add (t, step * 0.8, tone (idx, 1), vel * (k % 4 == 0 ? 1.0f : 0.82f));
                        }
                        break;
                    }
                }
            }
        }
        std::sort (out.begin(), out.end(), [] (const Note& x, const Note& y) { return x.start < y.start; });
        swingPart (out);
    }

    //==========================================================================
    // melody
    static int motifKey (int type)
    {
        if (type == Drop) return Chorus;
        if (type == Breakdown) return Bridge;
        return type;
    }

    const std::vector<const char*>& pool (int type) const
    {
        int d = g.density;
        if (type == Verse && d > 0 && genre != Trap) d = juce::jmin (d, 1);
        if (type == PreChorus) d = juce::jmin (2, d + (genre == Pop || genre == Rock ? 0 : 1));
        if (type == Bridge || type == Breakdown) d = juce::jmax (0, d - 1);
        if (genre == Trap && type == Verse) d = 2;          // a rap-like flow
        return d == 0 ? rhythmSlow : (d == 1 ? rhythmMid : rhythmBusy);
    }

    std::vector<int> contour (int count, int dirBias, Rng& r) const
    {
        // scale steps between notes: mostly steps, leaps get answered by a step back
        std::vector<int> d;
        int lastLeap = 0;
        for (int i = 1; i < count; ++i)
        {
            int step;
            if (lastLeap != 0) step = lastLeap > 0 ? -1 : 1;
            else
            {
                static const double w[] = { 1.5, 5, 2, 0.8, 0.6 };   // 0, 1, 2, 3, 4 steps
                const int size = r.weighted (w, 5);
                int dir = r.chance (0.5 + 0.2 * dirBias) ? 1 : -1;
                step = size * dir;
            }
            lastLeap = std::abs (step) >= 3 ? step : 0;
            d.push_back (step);
        }
        return d;
    }

    Song::Motif makeMotif (int type, Rng& r) const
    {
        // A phrase plan, then rhythm and shape per bar.
        Song::Motif m;
        const bool pre = type == PreChorus;
        m.bars = pre ? 4 : 8;
        static const int centres[NumSectionTypes] = { 2, 2, 3, 4, 3, 3, 4, 4, 2 };
        m.centre = centres[juce::jlimit (0, NumSectionTypes - 1, type)];

        const auto& pl = pool (type);
        const auto& cad = g.density == 0 ? cadenceSlow : cadenceMid;
        const char* m1 = r.pick (pl);
        const char* m2 = r.pick (pl);
        const char* m3 = r.pick (pl);
        const char* c1 = r.pick (cad);
        const char* c2 = r.pick (cad);

        struct BarPlan { const char* rhythm; int shape; int anchor; bool reset; bool phraseEnd; };
        std::vector<BarPlan> plan;
        const int seqStep = r.chance (0.5) ? -1 : 1;
        if (type == Chorus)        // the hook: one bar idea repeated (and sequenced) = catchy
            plan = { { m1, 0, 0, true, false }, { m1, 0, seqStep, true, false }, { m2, 1, 0, false, false }, { c1, 2, 0, false, true },
                     { m1, 0, 0, true, false }, { m1, 0, seqStep, true, false }, { m3, 3, 0, false, false }, { c2, 4, 0, false, true } };
        else if (pre)              // climbing sequence into the chorus
            plan = { { m1, 0, 0, true, false }, { m1, 0, 1, true, false }, { m1, 0, 2, true, false }, { c1, 2, 3, true, true } };
        else                       // A A' : statement - answer, statement - other answer
            plan = { { m1, 0, 0, true, false }, { m2, 1, 0, false, false }, { m3, 2, 0, false, false }, { c1, 3, 0, false, true },
                     { m1, 0, 0, true, false }, { m2, 1, 0, false, false }, { r.pick (pl), 4, 0, false, false }, { c2, 5, 0, false, true } };

        std::map<int, std::vector<int>> shapes;
        for (int bar = 0; bar < (int) plan.size(); ++bar)
        {
            const auto& bp = plan[(size_t) bar];
            auto hits = parseRhythm (bp.rhythm, bar * 4.0);
            if (hits.empty()) continue;
            auto& sh = shapes[bp.shape * 100 + (int) hits.size()];
            if (sh.empty())
            {
                const int bias = pre ? 1 : (bp.phraseEnd ? -1 : (bar < 2 ? 1 : 0));
                sh = contour ((int) hits.size() + 1, bias, r);
            }
            for (size_t i = 0; i < hits.size(); ++i)
            {
                Song::MotifNote n;
                n.start = hits[i].start;
                n.length = hits[i].length;
                n.reset = i == 0 && bp.reset;
                n.step = n.reset ? bp.anchor : sh[i];
                n.phraseEnd = bp.phraseEnd && i + 1 == hits.size();
                n.sectionEnd = bar == (int) plan.size() - 1 && i + 1 == hits.size();
                const double inBar = std::fmod (n.start, 4.0);
                n.velocity = inBar < 1e-6 ? 0.9f : (std::fmod (inBar, 1.0) < 1e-6 ? 0.8f : 0.7f);
                m.notes.push_back (n);
            }
        }
        return m;
    }

    void makeMotifs()
    {
        for (int t = 0; t < NumSectionTypes; ++t)
        {
            Rng r = partRng (NewMelody, 300 + (u64) t);
            s.motifs[(size_t) t] = makeMotif (t, r);
        }
    }

    int tonicMidi() const { int base = 60 + s.key; if (base > 66) base -= 12; return base; }
    int idxToMidi (int idx) const
    {
        const int oct = (int) std::floor (idx / 7.0);
        return tonicMidi() + 12 * oct + scaleSteps[s.scale][idx - oct * 7];
    }
    bool idxIsTone (int idx, const Chord* ch) const { return ch == nullptr || ch->isTone (idxToMidi (idx)); }
    int nearestTone (int idx, const Chord* ch, int prefer) const
    {
        const int order[] = { 0, prefer, -prefer, 2 * prefer, -2 * prefer, 3 * prefer, -3 * prefer };
        for (int o : order) if (idxIsTone (idx + o, ch)) return idx + o;
        return idx;
    }

    void writeMelody()
    {
        auto& out = s.parts[Melody];
        out.clear();
        for (const auto& sec : s.sections)
        {
            if (! sec.on[Melody]) continue;
            const auto& m = s.motifs[(size_t) motifKey (sec.type)];
            if (m.notes.empty()) continue;
            const int lo = m.centre - 5, hi = m.centre + 7;
            int idx = m.centre;
            const float sparse = sec.type == Breakdown ? 0.5f : 1.0f;
            const int reps = juce::jmax (1, (sec.bars + m.bars - 1) / m.bars);
            for (int rep = 0; rep < reps; ++rep)
                for (size_t i = 0; i < m.notes.size(); ++i)
                {
                    const auto& mn = m.notes[i];
                    const double t = sec.start + rep * m.bars * 4.0 + mn.start;
                    if (t >= sec.end() - 1e-6) break;
                    if (sparse < 1.0f && ! mn.reset && ! mn.phraseEnd && std::fmod (mn.start, 1.0) > 1e-6) continue;   // breakdown: only the bones
                    const auto* ch = s.chordAt (t + 1e-4);
                    const int prev = idx;
                    idx = mn.reset ? m.centre + mn.step : idx + mn.step;
                    if (idx > hi || idx < lo) idx = juce::jlimit (lo, hi, prev - mn.step);
                    const bool strong = std::fmod (t - sec.start, 2.0) < 1e-6 || mn.length >= 1.0 || mn.phraseEnd || mn.reset;
                    const bool lastInSection = mn.sectionEnd ? (rep == reps - 1) : (t + 0.01 < sec.end() && nextStartAfter (m, i, rep, sec) >= sec.end() - 1e-6);
                    if (lastInSection)
                    {
                        // end on the home note if the chord allows, else the chord's root
                        int best = idx, bestD = 99;
                        for (int k = lo - 3; k <= hi + 3; ++k)
                        {
                            const int pc = ((idxToMidi (k) - s.key) % 12 + 12) % 12;
                            const bool good = (pc == 0 && idxIsTone (k, ch)) || (ch != nullptr && ! ch->isTone (s.key) && idxToMidi (k) % 12 == ch->root);
                            if (good && std::abs (k - idx) < bestD) { bestD = std::abs (k - idx); best = k; }
                        }
                        idx = best;
                    }
                    else if (strong) idx = nearestTone (idx, ch, mn.step >= 0 ? 1 : -1);
                    else if (ch != nullptr && ! ch->isTone (idxToMidi (idx)))
                    {
                        // passing tones are fine, but not a semitone clash held on long
                        const int p = idxToMidi (idx);
                        if (mn.length >= 0.75 && (ch->isTone (p + 1) || ch->isTone (p - 1))) idx = nearestTone (idx, ch, mn.step >= 0 ? 1 : -1);
                    }
                    out.push_back ({ t, mn.length, idxToMidi (idx), mn.velocity * (0.85f + sec.energy * 0.15f), -1 });
                }
        }
        std::sort (out.begin(), out.end(), [] (const Note& x, const Note& y) { return x.start < y.start; });
        for (size_t i = 0; i + 1 < out.size(); ++i)   // one note at a time, slightly detached
            out[i].length = juce::jmax (0.08, juce::jmin (out[i].length * 0.96, out[i + 1].start - out[i].start - 0.03));
        swingPart (out);
    }

    static double nextStartAfter (const Song::Motif& m, size_t i, int rep, const Section& sec)
    {
        if (i + 1 < m.notes.size()) return sec.start + rep * m.bars * 4.0 + m.notes[i + 1].start;
        return sec.start + (rep + 1) * m.bars * 4.0 + (m.notes.empty() ? 0.0 : m.notes.front().start);
    }

    //==========================================================================
    // lyrics
    juce::StringArray fillLine (const char* tpl, int family, const juce::String& avoid, Rng& r) const
    {
        const auto& w = moodWords[mood];
        juce::StringArray words;
        for (auto tok : juce::StringArray::fromTokens (tpl, " ", ""))
        {
            if (tok == "S") words.add (r.pick (subjects));
            else if (tok == "V") words.add (r.pick (w.verbs));
            else if (tok == "A") words.add (r.pick (w.adjs));
            else if (tok == "N") words.add (r.pick (w.nouns));
            else if (tok == "P") words.add (r.pick (preps));
            else if (tok == "R" || tok == "Q" || tok == "W")
            {
                const char kind = tok == "R" ? 'n' : (tok == "Q" ? 'a' : 'v');
                std::vector<juce::String> options;
                for (auto& rw : rhymeFamilies[(size_t) family])
                    if (rw.kind == kind && ! endsWithWord (avoid, rw.word)) options.push_back (rw.word);
                if (options.empty()) return {};
                words.addTokens (r.pick (options), " ", "");
            }
            else words.add (tok);
        }
        return words;
    }

    static bool endsWithWord (const juce::String& line, const char* word)
    {
        return line.isNotEmpty() && juce::String (word).endsWith (line.fromLastOccurrenceOf (" ", false, false));
    }

    juce::StringArray makeLine (int n, int family, const juce::String& avoid, Rng& r) const
    {
        const auto& fam = rhymeFamilies[(size_t) family];
        if (n <= 2)
        {
            for (int k = 0; k < 30; ++k)
            {
                const auto& rw = fam[(size_t) r.range (0, (int) fam.size() - 1)];
                juce::StringArray w;
                w.addTokens (rw.word, " ", "");
                if (endsWithWord (avoid, rw.word)) continue;
                if (syllables (w) == n) return w;
                if (n == 2 && syllables (w) == 1) { w.insert (0, r.chance (0.5) ? "oh" : "yeah"); return w; }
            }
            return n == 1 ? juce::StringArray { "oh" } : juce::StringArray { "oh", "yeah" };
        }
        juce::StringArray best;
        int bestS = -1;
        for (int attempt = 0; attempt < 120 && bestS != n; ++attempt)
        {
            auto words = fillLine (r.pick (lineTemplates), family, avoid, r);
            if (words.isEmpty()) continue;
            const int sy = syllables (words);
            if (sy <= n && sy > bestS) { best = words; bestS = sy; }
        }
        if (bestS < 0) { best.addTokens (fam.front().word, " ", ""); bestS = syllables (best); }
        // fill the start of long lines with short phrases
        int need = n - bestS;
        juce::StringArray prefix;
        int guard = 0;
        while (need > 0 && ++guard < 40)
        {
            auto piece = fillLine (r.pick (fillTemplates), family, avoid, r);
            const int sy = syllables (piece);
            if (piece.isEmpty()) continue;
            if (sy <= need && (sy >= juce::jmin (need, 2) || guard > 20)) { prefix.addArray (piece); need -= sy; }
        }
        while (need-- > 0) prefix.add ("oh");
        prefix.addArray (best);
        return prefix;
    }

    void writeLyrics()
    {
        s.syllables.clear();
        s.lyricLines.clear();
        s.lyricLineStarts.clear();
        juce::StringArray textLines;
        auto& mel = s.parts[Melody];
        for (auto& n : mel) n.syllable = -1;
        const auto& fams = moodWords[mood].families;
        juce::String firstHookLine;

        for (size_t si = 0; si < s.sections.size(); ++si)
        {
            const auto& sec = s.sections[si];
            if (! sec.on[Melody]) continue;
            // lines of two bars
            std::map<int, std::vector<size_t>> lines;
            for (size_t i = 0; i < mel.size(); ++i)
                if (mel[i].start >= sec.start - 0.05 && mel[i].start < sec.end() - 0.05)
                    lines[(int) ((mel[i].start + 0.05 - sec.start) / 8.0)].push_back (i);

            // choruses and pre-choruses repeat their words; verses get new ones
            const int key = motifKey (sec.type);
            const bool repeats = key == Chorus || key == PreChorus;
            const u64 base = repeats ? mix (5000, (u64) key) : mix (6000 + (u64) s.chunk * 1000, si);
            juce::String pairWord;
            for (auto& [li, idxs] : lines)
            {
                Rng famR = partRng (NewLyrics, mix (base, (u64) (li / 2)));
                const int family = fams[(size_t) famR.range (0, (int) fams.size() - 1)];
                Rng lr = partRng (NewLyrics, mix (base, 100 + (u64) li));
                auto words = makeLine ((int) idxs.size(), family, li % 2 == 1 ? pairWord : juce::String(), lr);
                if (li % 2 == 0) pairWord = words[words.size() - 1];   // the next line must not end the same
                juce::StringArray syl;
                for (auto& w : words) syl.addArray (juce::StringArray::fromTokens (w, "-", ""));
                for (size_t k = 0; k < idxs.size() && k < (size_t) syl.size(); ++k)
                {
                    mel[idxs[k]].syllable = s.syllables.size();
                    s.syllables.add (syl[(int) k]);
                }
                const auto shown = words.joinIntoString (" ").removeCharacters ("-");
                s.lyricLines.add (shown);
                s.lyricLineStarts.push_back (mel[idxs.front()].start);
                textLines.add (words.joinIntoString (" "));
                if (key == Chorus && firstHookLine.isEmpty()) firstHookLine = shown;
            }
            textLines.add ({});
        }
        while (textLines.size() > 0 && textLines[textLines.size() - 1].isEmpty()) textLines.remove (textLines.size() - 1);
        s.lyricsText = textLines.joinIntoString ("\n");
        if (s.chunk == 0) s.title = makeTitle (firstHookLine.isNotEmpty() ? firstHookLine : (s.lyricLines.isEmpty() ? juce::String() : s.lyricLines[0]));
    }

    juce::String makeTitle (const juce::String& line) const
    {
        auto words = juce::StringArray::fromTokens (line, " ", "");
        words.removeEmptyStrings();
        juce::StringArray t;
        for (int i = juce::jmax (0, words.size() - 3); i < words.size(); ++i)
            if (! (t.isEmpty() && (words[i] == "the" || words[i] == "and" || words[i] == "oh"))) t.add (words[i]);
        if (t.isEmpty())
        {
            Rng r = partRng (NewLyrics, 4242);
            const auto& w = moodWords[mood];
            t.add (juce::String (r.pick (w.adjs)).removeCharacters ("-"));
            t.add (juce::String (r.pick (w.nouns)).removeCharacters ("-"));
        }
        for (auto& w : t) w = w.substring (0, 1).toUpperCase() + w.substring (1);
        return t.joinIntoString (" ");
    }

    //==========================================================================
    void swingPart (std::vector<Note>& v) const
    {
        if (s.swing <= 0.505f) return;
        const double grid = g.swingGrid, cell = grid * 2.0;
        const double shift = (s.swing - 0.5) * cell;
        for (auto& n : v)
        {
            const double inCell = std::fmod (n.start, cell);
            if (std::abs (inCell - grid) < 1e-4) { n.start += shift; n.length = juce::jmax (0.05, n.length - shift); }
        }
    }

    void writeAllParts()
    {
        writeChordsPart();
        writeDrums();
        writeBass();
        writeCounter();
        writeMelody();
        writeLyrics();
    }

    Song& s;
    const GenreInfo& g;
    const int genre, mood;
};
} // namespace

//==============================================================================
juce::StringArray genreNames() { return { "Pop", "Lo-fi", "Trap", "House", "Ambient", "Jazz", "Rock", "Cinematic" }; }
juce::StringArray moodNames() { return { "Happy", "Sad", "Dark", "Dreamy", "Epic", "Chill" }; }
juce::StringArray partNames() { return { "Chords", "Bass", "Drums", "Melody", "Counter" }; }
juce::StringArray targetNames() { return { "Chords", "Bass", "Drums", "Melody", "Counter-melody", "Lyrics", "Song form" }; }
juce::String sectionName (int t)
{
    static const char* n[] = { "Intro", "Verse", "Pre-chorus", "Chorus", "Bridge", "Build", "Drop", "Breakdown", "Outro" };
    return n[juce::jlimit (0, NumSectionTypes - 1, t)];
}
juce::String scaleName (int s) { return scaleNameList[juce::jlimit (0, 6, s)]; }
juce::String keyName (int pc) { return keyNameList[((pc % 12) + 12) % 12]; }
int partChannel (int part) { static const int ch[NumParts] = { 1, 2, 10, 3, 4 }; return ch[juce::jlimit (0, NumParts - 1, part)]; }

bool Chord::isTone (int pitch) const
{
    const int pc = ((pitch % 12) + 12) % 12;
    for (int iv : intervals) if ((root + iv) % 12 == pc) return true;
    return false;
}

int Song::sectionIndexAt (double beat) const
{
    for (size_t i = 0; i < sections.size(); ++i) if (beat >= sections[i].start && beat < sections[i].end()) return (int) i;
    return -1;
}

const Chord* Song::chordAt (double beat) const
{
    auto it = std::upper_bound (chords.begin(), chords.end(), beat, [] (double b, const Chord& c) { return b < c.start; });
    if (it == chords.begin()) return nullptr;
    --it;
    return beat < it->start + it->length ? &*it : nullptr;
}

int Song::lyricLineAt (double beat) const
{
    int line = -1;
    for (size_t i = 0; i < lyricLineStarts.size(); ++i) if (lyricLineStarts[i] <= beat + 1e-6) line = (int) i;
    return line;
}

Song compose (const Settings& settings)
{
    Song s;
    s.settings = settings;
    Writer w (s);
    w.basics();
    w.makeProgressions();
    w.makeForm();
    w.layChords();
    w.makeMotifs();
    w.writeAllParts();
    return s;
}

Song composeNext (const Song& prev)
{
    Song s;
    s.settings = prev.settings;
    s.settings.endless = true;
    s.chunk = prev.chunk + 1;
    s.title = prev.title;
    s.key = prev.key; s.scale = prev.scale; s.bpm = prev.bpm; s.swing = prev.swing; s.chordBeats = prev.chordBeats;
    s.progressions = prev.progressions;
    s.motifs = prev.motifs;
    s.partSeeds = prev.partSeeds;
    Writer w (s);
    w.lastTypeHint = prev.sections.empty() ? -1 : prev.sections.back().type;
    // now and then a fresh bridge, so a long stream keeps surprising
    Rng r (mix (prev.settings.seed, 31337 + (u64) s.chunk));
    if (r.chance (0.35))
    {
        Song tmp = s;
        tmp.partSeeds[NewChords] += 1000u + (juce::uint32) s.chunk;
        Writer tw (tmp);
        tw.makeProgressions();
        s.progressions[3] = tmp.progressions[3];
        Rng mr (mix (prev.settings.seed, 4141 + (u64) s.chunk));
        s.motifs[Bridge] = tw.makeMotif (Bridge, mr);
    }
    w.makeForm();
    w.layChords();
    w.writeAllParts();
    return s;
}

void regenerate (Song& s, int target, juce::uint32 seed)
{
    if (! juce::isPositiveAndBelow (target, (int) NumTargets)) return;
    s.partSeeds[(size_t) target] = seed;
    Writer w (s);
    switch (target)
    {
        case NewChords:
            w.makeProgressions(); w.layChords();
            w.writeChordsPart(); w.writeBass(); w.writeCounter(); w.writeMelody(); w.writeLyrics();
            break;
        case NewBass: w.writeBass(); break;
        case NewDrums: w.writeDrums(); if (s.settings.genre == Trap) w.writeBass(); break;
        case NewMelody: w.makeMotifs(); w.writeMelody(); w.writeLyrics(); break;
        case NewCounter: w.writeCounter(); break;
        case NewLyrics: w.writeLyrics(); break;
        case NewArrangement: w.makeForm(); w.layChords(); w.writeAllParts(); break;
        default: break;
    }
}

juce::MidiFile toMidiFile (const Song& s)
{
    constexpr int tpq = 960;
    juce::MidiFile f;
    f.setTicksPerQuarterNote (tpq);
    juce::MidiMessageSequence meta;
    meta.addEvent (juce::MidiMessage::textMetaEvent (3, s.title.isEmpty() ? "Cat Suite song" : s.title), 0);
    meta.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / juce::jmax (20.0, s.bpm))), 0);
    meta.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    for (const auto& sec : s.sections) meta.addEvent (juce::MidiMessage::textMetaEvent (6, sectionName (sec.type)), sec.start * tpq);
    meta.addEvent (juce::MidiMessage::endOfTrack(), s.lengthBeats * tpq);
    f.addTrack (meta);

    for (int p = 0; p < NumParts; ++p)
    {
        juce::MidiMessageSequence seq;
        const int ch = partChannel (p);
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, partNames()[p]), 0);
        for (const auto& n : s.parts[(size_t) p])
        {
            if (p == Melody && juce::isPositiveAndBelow (n.syllable, s.syllables.size()))
                seq.addEvent (juce::MidiMessage::textMetaEvent (5, s.syllables[n.syllable]), n.start * tpq);
            seq.addEvent (juce::MidiMessage::noteOn (ch, n.pitch, juce::jlimit (0.01f, 1.0f, n.velocity)), n.start * tpq);
            seq.addEvent (juce::MidiMessage::noteOff (ch, n.pitch), (n.start + n.length) * tpq);
        }
        seq.updateMatchedPairs();
        seq.addEvent (juce::MidiMessage::endOfTrack(), juce::jmax (s.lengthBeats, seq.getEndTime() / tpq) * tpq);
        f.addTrack (seq);
    }
    return f;
}

//==============================================================================
void SongPlayer::setSong (std::shared_ptr<const Song> newSong)
{
    {
        const juce::SpinLock::ScopedLockType l (swapLock);
        pendingSong = newSong;
        pendingNext.reset();
        hasPendingSong = true;
    }
    if (currentMsg != nullptr) keepAlive.push_back (currentMsg);
    if (queuedMsg != nullptr) keepAlive.push_back (queuedMsg);
    while (keepAlive.size() > 6) keepAlive.erase (keepAlive.begin());
    currentMsg = std::move (newSong);
    queuedMsg.reset();
    nextRequested = false;
}

std::shared_ptr<const Song> SongPlayer::getSong() const
{
    if (queuedMsg != nullptr && queuedMsg->chunk == shownChunk.load()) return queuedMsg;
    return currentMsg;
}

void SongPlayer::queueNext (std::shared_ptr<const Song> nextSong)
{
    if (queuedMsg != nullptr && queuedMsg->chunk <= shownChunk.load())
    {
        keepAlive.push_back (currentMsg);
        while (keepAlive.size() > 6) keepAlive.erase (keepAlive.begin());
        currentMsg = queuedMsg;
    }
    queuedMsg = nextSong;
    {
        const juce::SpinLock::ScopedLockType l (swapLock);
        pendingNext = std::move (nextSong);
        hasPendingNext = true;
    }
    nextRequested = true;
}

void SongPlayer::allOff (std::array<juce::MidiBuffer*, NumParts>& out, int samplePos)
{
    for (auto& h : held)
        if (auto* b = out[(size_t) h.part]) b->addEvent (juce::MidiMessage::noteOff (partChannel (h.part), h.pitch), samplePos);
    held.clear();
}

void SongPlayer::endHeld (double upTo, double from, double ppqPerSample, int sampleOffset, int numSamples,
                          std::array<juce::MidiBuffer*, NumParts>& out)
{
    for (size_t i = 0; i < held.size();)
    {
        if (held[i].end <= upTo)
        {
            const int pos = juce::jlimit (0, numSamples - 1, sampleOffset + (int) ((held[i].end - from) / ppqPerSample));
            if (auto* b = out[(size_t) held[i].part]) b->addEvent (juce::MidiMessage::noteOff (partChannel (held[i].part), held[i].pitch), pos);
            held[i] = held.back();
            held.pop_back();
        }
        else ++i;
    }
}

void SongPlayer::emit (const Song& sg, double from, double to, double ppqPerSample, int sampleOffset, int numSamples,
                       std::array<juce::MidiBuffer*, NumParts>& out, bool catchUp)
{
    endHeld (to, from, ppqPerSample, sampleOffset, numSamples, out);
    for (int p = 0; p < NumParts; ++p)
    {
        auto* buf = out[(size_t) p];
        if (buf == nullptr) continue;
        const auto& notes = sg.parts[(size_t) p];
        auto it = std::lower_bound (notes.begin(), notes.end(), from, [] (const Note& n, double b) { return n.start < b; });
        if (catchUp && p != Drums)
        {
            // notes that already started but still have a while to ring
            auto back = it;
            for (int k = 0; k < 96 && back != notes.begin(); ++k)
            {
                --back;
                if (back->start + back->length > from + 0.25 && held.size() < held.capacity())
                {
                    buf->addEvent (juce::MidiMessage::noteOn (partChannel (p), back->pitch, juce::jlimit (0.01f, 1.0f, back->velocity * 0.85f)), sampleOffset);
                    held.push_back ({ p, back->pitch, back->start + back->length });
                }
            }
        }
        for (; it != notes.end() && it->start < to; ++it)
        {
            const int pos = juce::jlimit (0, numSamples - 1, sampleOffset + (int) ((it->start - from) / ppqPerSample));
            // the same key still down: lift it first
            for (size_t i = 0; i < held.size(); ++i)
                if (held[i].part == p && held[i].pitch == it->pitch)
                {
                    buf->addEvent (juce::MidiMessage::noteOff (partChannel (p), it->pitch), pos);
                    held[i] = held.back();
                    held.pop_back();
                    break;
                }
            if (held.size() >= held.capacity())
            {
                buf->addEvent (juce::MidiMessage::noteOff (partChannel (held.front().part), held.front().pitch), pos);
                held.erase (held.begin());
            }
            buf->addEvent (juce::MidiMessage::noteOn (partChannel (p), it->pitch, juce::jlimit (0.01f, 1.0f, it->velocity)), pos);
            const double end = it->start + juce::jmax (0.02, it->length);
            if (end <= to)
            {
                const int offPos = juce::jlimit (pos, numSamples - 1, sampleOffset + (int) ((end - from) / ppqPerSample));
                buf->addEvent (juce::MidiMessage::noteOff (partChannel (p), it->pitch), offPos);
            }
            else held.push_back ({ p, it->pitch, end });
        }
    }
}

void SongPlayer::process (double ppqStart, double ppqPerSample, int numSamples, bool playing,
                          std::array<juce::MidiBuffer*, NumParts>& out)
{
    if (hasPendingSong.load() || hasPendingNext.load())
    {
        const juce::SpinLock::ScopedTryLockType l (swapLock);
        if (l.isLocked())
        {
            if (hasPendingSong.exchange (false))
            {
                allOff (out, 0);
                song = std::move (pendingSong);
                next.reset();
                offset = std::floor (juce::jmax (0.0, ppqStart) / 4.0) * 4.0;
                if (song != nullptr && ! song->settings.endless) offset = 0.0;
                shownChunk = song != nullptr ? song->chunk : 0;
                catchUpNext = true;
            }
            if (hasPendingNext.exchange (false)) next = std::move (pendingNext);
        }
    }

    if (! playing || song == nullptr || song->lengthBeats <= 0.0 || ppqPerSample <= 0.0 || numSamples <= 0)
    {
        if (! held.empty()) allOff (out, 0);
        lastPpq = -1.0;
        catchUpNext = true;
        return;
    }

    const double blockBeats = ppqPerSample * numSamples;
    if (lastPpq >= 0.0 && (ppqStart < lastPpq - 1e-6 || ppqStart > lastPpq + blockBeats * 3.0 + 0.05))
    {
        // the host jumped: start clean from here
        allOff (out, 0);
        catchUpNext = true;
        if (song->settings.endless && ppqStart < offset) offset = std::floor (juce::jmax (0.0, ppqStart) / 4.0) * 4.0;
    }
    lastPpq = ppqStart;
    if (ppqStart + blockBeats <= 0.0) return;   // count-in before the song

    const bool endless = song->settings.endless;
    double local = endless ? ppqStart - offset : std::fmod (juce::jmax (0.0, ppqStart), song->lengthBeats);
    int sample = 0;
    if (ppqStart < 0.0) { sample = (int) (-ppqStart / ppqPerSample); local = 0.0; }

    int guard = 0;
    while (sample < numSamples && ++guard < 8)
    {
        const double remaining = (numSamples - sample) * ppqPerSample;
        const double len = song->lengthBeats;
        const double to = local + remaining;
        if (to <= len)
        {
            emit (*song, local, to, ppqPerSample, sample, numSamples, out, catchUpNext);
            catchUpNext = false;
            local = to;
            break;
        }
        // the piece ends inside this block
        emit (*song, local, len, ppqPerSample, sample, numSamples, out, catchUpNext);
        catchUpNext = false;
        sample += juce::jmax (1, (int) ((len - local) / ppqPerSample));
        allOff (out, juce::jmin (sample, numSamples - 1));
        if (endless)
        {
            offset += len;
            if (next != nullptr) { song = std::move (next); next.reset(); shownChunk = song->chunk; nextRequested = false; }
        }
        local = 0.0;
    }

    shownPos = local;
    wantNext = endless && next == nullptr && local > song->lengthBeats - 32.0;
}
} // namespace cat::muse

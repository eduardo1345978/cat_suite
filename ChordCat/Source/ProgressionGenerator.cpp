#include "ProgressionGenerator.h"
#include <map>

namespace chordcat
{
namespace
{
    enum State { I = 0, II, III, IV, V, VI, VII, FlatII, FlatIII, FlatVI, FlatVII, VofV, VofVI };

    using Corpus = std::vector<std::vector<int>>;

    const Corpus& corpusFor (int genre)
    {
        static const Corpus neoSoul = {
            { IV, III, II, I }, { II, V, I, VI }, { I, VI, II, V }, { IV, FlatVII, I, I }, { II, III, IV, V },
            { VI, II, V, I }, { I, VofV, II, V }, { IV, V, III, VI }, { I, III, IV, FlatVI }, { IV, III, VI, II, V, I },
            { II, FlatII, I }, { I, VofVI, VI, II, V }, { IV, IV, III, VI, II, V }
        };
        static const Corpus rnb = {
            { VI, IV, I, V }, { II, V, I, VI }, { I, III, VI, IV }, { IV, I, V, VI }, { VI, II, V, I },
            { I, FlatVI, FlatVII, I }, { IV, V, III, VI, II, V, I }, { I, VI, IV, V }, { II, III, IV, IV }
        };
        static const Corpus lofi = {
            { II, V, I, VI }, { IV, III, II, I }, { I, VI, IV, V }, { VI, IV, II, V }, { IV, VofVI, VI, I },
            { II, V, III, VI }, { I, IV, II, V }, { VI, V, IV, III }, { IV, FlatVII, I, VI }
        };
        static const Corpus jazz = {
            { II, V, I, I }, { I, VI, II, V }, { III, VI, II, V, I }, { I, VofV, II, V }, { II, FlatII, I },
            { IV, FlatVII, I }, { I, VofVI, VI, VofV, II, V, I }, { III, VI, II, V }, { I, FlatIII, II, FlatII }
        };
        static const Corpus pop = {
            { I, V, VI, IV }, { I, VI, IV, V }, { VI, IV, I, V }, { I, IV, V, V }, { IV, I, V, VI }, { I, IV, VI, V }
        };
        switch (genre)
        {
            case 1:  return rnb;
            case 2:  return lofi;
            case 3:  return jazz;
            case 4:  return pop;
            default: return neoSoul;
        }
    }

    const std::array<double, 13>& moodWeights (int mood)
    {
        static const std::array<double, 13> dark      { 1.0, 0.8, 1.0, 1.0, 0.7, 1.3, 0.6, 1.8, 1.3, 1.6, 1.4, 0.6, 0.8 };
        static const std::array<double, 13> uplifting { 1.6, 1.0, 0.8, 1.6, 1.4, 1.1, 0.4, 0.3, 0.5, 0.8, 1.0, 1.0, 0.6 };
        static const std::array<double, 13> sad       { 1.0, 1.2, 1.1, 1.4, 0.8, 1.6, 0.4, 0.6, 0.8, 1.5, 1.0, 0.6, 0.9 };
        static const std::array<double, 13> jazzy     { 1.0, 1.6, 1.1, 1.0, 1.6, 1.2, 0.6, 1.2, 0.8, 0.8, 1.0, 1.4, 1.3 };
        static const std::array<double, 13> tension   { 0.7, 1.0, 1.0, 0.9, 1.5, 1.0, 1.3, 1.6, 1.2, 1.4, 1.0, 1.8, 1.6 };
        switch (mood)
        {
            case 0:  return dark;
            case 1:  return uplifting;
            case 2:  return sad;
            case 4:  return tension;
            default: return jazzy;
        }
    }

    int pickWeighted (const std::vector<double>& w, juce::Random& rng)
    {
        double total = 0.0;
        for (double x : w) total += x;
        if (total <= 0.0) return 0;
        double r = rng.nextDouble() * total;
        for (size_t i = 0; i < w.size(); ++i)
        {
            r -= w[i];
            if (r <= 0.0) return (int) i;
        }
        return (int) w.size() - 1;
    }

    juce::String swapQuality (const juce::String& type, bool toMinor)
    {
        static const std::vector<std::pair<juce::String, juce::String>> pairs = {
            { "maj", "min" }, { "maj7", "m7" }, { "maj9", "m9" }, { "maj11", "m11" }, { "maj13", "m13" },
            { "6", "m6" }, { "6/9", "m6/9" }, { "add9", "madd9" }, { "add11", "madd11" }, { "add2", "madd2" },
            { "7", "m7" }, { "9", "m9" }, { "11", "m11" }, { "13", "m13" }, { "maj7#11", "m7add11" }, { "aug", "min" }
        };
        for (const auto& [maj, min] : pairs)
        {
            if (toMinor && type == maj) return min;
            if (! toMinor && type == min) return maj;
        }
        const auto fam = getChordTypeInfo (type).family;
        const int cx = getChordTypeInfo (type).complexity;
        if (toMinor)  return cx >= 2 ? "m9" : (cx == 1 ? "m7" : "min");
        juce::ignoreUnused (fam);
        return cx >= 2 ? "maj9" : (cx == 1 ? "maj7" : "maj");
    }

    bool chordInKey (const ChordDefinition& c, const GlobalState& g)
    {
        for (int iv : ChordEngine::getFullFormula (c))
            if (! isInScale (c.rootNote + iv, g.keyRoot, g.scaleIndex, g.customScaleMask))
                return false;
        return true;
    }
}

juce::StringArray getMoodNames()       { return { "Dark", "Uplifting", "Sad", "Jazzy", "Tension" }; }
juce::StringArray getGenreNames()      { return { "NeoSoul", "RnB", "LoFi", "Jazz", "Pop" }; }
juce::StringArray getComplexityNames() { return { "Triads", "7ths", "9ths", "11ths", "13ths" }; }

ProgressionGenerator::ProgressionGenerator()
{
    train();
}

void ProgressionGenerator::train()
{
    for (int genre = 0; genre < 5; ++genre)
    {
        auto& m = genreTables[(size_t) genre];
        for (int a = 0; a < numStates; ++a)
            for (int b = 0; b < numStates; ++b)
                m[(size_t) a][(size_t) b] = (b <= VII ? 0.15 : 0.04); // smoothing prior

        for (const auto& seq : corpusFor (genre))
            for (size_t i = 0; i < seq.size(); ++i)
            {
                const int from = seq[i];
                const int to = seq[(i + 1) % seq.size()]; // progressions loop
                m[(size_t) from][(size_t) to] += 1.0;
            }
    }
}

int ProgressionGenerator::defaultVoicingForGenre (int genre, int complexity)
{
    switch ((Genre) genre)
    {
        case Genre::NeoSoul: return VoicingWideNeoSoul;
        case Genre::RnB:     return VoicingDrop2;
        case Genre::LoFi:    return complexity >= 1 ? VoicingRootless : VoicingOpen;
        case Genre::Jazz:    return complexity >= 1 ? VoicingRootless : VoicingClose;
        case Genre::Pop:
        default:             return VoicingClose;
    }
}

ChordDefinition ProgressionGenerator::chordForState (int state, const GeneratorSettings& s, int scaleIndex, juce::Random& rng) const
{
    const int complexity = juce::jlimit (0, 4, s.complexity);
    const int numTones = complexity + 3;
    ChordDefinition c;

    if (state <= VII)
    {
        c = ChordEngine::buildDiatonicChord (state, numTones, s.keyRoot, scaleIndex, s.customMask);

        if ((Mood) s.mood == Mood::Tension && getChordTypeInfo (c.chordType).family == FamilyDominant && complexity >= 1)
        {
            static const char* altered[] = { "7b9", "7#9", "7alt", "13b9", "7b9b13" };
            c.chordType = altered[rng.nextInt (complexity >= 3 ? 5 : 2)];
        }
    }
    else
    {
        const uint16_t mask = getScaleMask (scaleIndex, s.customMask);
        std::vector<int> rel;
        for (int i = 0; i < 12; ++i) if (mask & (1u << i)) rel.push_back (i);
        auto degreeSemis = [&rel] (int d) { return rel.empty() ? 0 : rel[(size_t) (d % (int) rel.size())]; };

        static const char* majorFamily[] = { "maj", "maj7", "maj9", "maj9#11", "maj13" };
        static const char* dominant[]    = { "maj", "7", "9", "9#11", "13" };
        static const char* altered[]     = { "7b9", "7#9", "7alt", "13b9", "7b9b13" };

        int relRoot = 0;
        const char* type = "maj";
        switch (state)
        {
            case FlatII:  relRoot = 1;  type = majorFamily[complexity]; break;
            case FlatIII: relRoot = 3;  type = majorFamily[complexity]; break;
            case FlatVI:  relRoot = 8;  type = majorFamily[complexity]; break;
            case FlatVII: relRoot = 10; type = dominant[complexity]; break;
            case VofV:    relRoot = (degreeSemis (4) + 7) % 12; type = dominant[complexity]; break;
            case VofVI:   relRoot = (degreeSemis (5) + 7) % 12; type = dominant[complexity]; break;
            default: break;
        }

        if ((state == VofV || state == VofVI || state == FlatII) && complexity >= 1
            && ((Mood) s.mood == Mood::Tension || (Mood) s.mood == Mood::Jazzy) && rng.nextFloat() < 0.5f)
            type = altered[rng.nextInt (complexity >= 3 ? 5 : 2)];

        if (state == FlatII && (Mood) s.mood == Mood::Jazzy && complexity >= 1)
            type = dominant[complexity]; // tritone substitute

        c.rootNote = (s.keyRoot + relRoot) % 12;
        c.chordType = type;
    }

    GlobalState g;
    g.keyRoot = s.keyRoot;
    g.scaleIndex = scaleIndex;
    g.customScaleMask = s.customMask;
    c.isBorrowed = ! chordInKey (c, g);
    c.voicingModel = defaultVoicingForGenre (s.genre, complexity);
    c.octave = 3;
    return c;
}

GeneratorResult ProgressionGenerator::generate (const GeneratorSettings& s)
{
    juce::Random rng (s.seed != 0 ? s.seed : juce::Time::currentTimeMillis() ^ (juce::int64) juce::Random::getSystemRandom().nextInt());
    GeneratorResult result;

    int scaleIndex = s.scaleIndex;
    const auto mood = (Mood) s.mood;
    const bool heptatonic = getScalePitchClasses (s.keyRoot, scaleIndex, s.customMask).size() == 7;

    if (s.moodSetsScale || ! heptatonic)
    {
        switch (mood)
        {
            case Mood::Dark:      scaleIndex = findScale ("Natural Minor"); break;
            case Mood::Sad:       scaleIndex = findScale ("Natural Minor"); break;
            case Mood::Tension:   scaleIndex = findScale ("Harmonic Minor"); break;
            case Mood::Uplifting: scaleIndex = findScale ("Major"); break;
            case Mood::Jazzy:     scaleIndex = (s.genre == (int) Genre::LoFi ? findScale ("Dorian") : findScale ("Major")); break;
        }
    }
    result.scaleIndex = scaleIndex;

    const auto& table = genreTables[(size_t) juce::jlimit (0, 4, s.genre)];
    const auto& mw = moodWeights (s.mood);
    const int count = juce::jlimit (1, 16, s.numChords);

    std::vector<int> states;
    std::vector<double> w (numStates, 0.0);

    // First chord: tonic-heavy
    for (int i = 0; i < numStates; ++i) w[(size_t) i] = 0.0;
    w[I] = 3.0; w[II] = 1.0; w[IV] = 1.6; w[VI] = 1.2;
    if (mood == Mood::Dark || mood == Mood::Sad) w[VI] = 1.8;
    states.push_back (pickWeighted (w, rng));

    for (int i = 1; i < count; ++i)
    {
        const int prev = states.back();
        for (int t = 0; t < numStates; ++t)
        {
            double weight = table[(size_t) prev][(size_t) t] * mw[(size_t) t];
            if (t == prev) weight *= 0.12;
            if (i == count - 1) // last chord should lead back to the first
            {
                const auto& row = table[(size_t) t];
                double rowSum = 0.0;
                for (double x : row) rowSum += x;
                weight *= 0.5 + 3.0 * row[(size_t) states.front()] / juce::jmax (1.0e-6, rowSum);
                if (t == states.front()) weight *= 0.2;
            }
            w[(size_t) t] = weight;
        }
        states.push_back (pickWeighted (w, rng));
    }

    for (int i = 0; i < count; ++i)
    {
        result.progression[(size_t) i].isActive = true;
        result.progression[(size_t) i].chord = chordForState (states[(size_t) i], s, scaleIndex, rng);
    }
    return result;
}

ChordDefinition ProgressionGenerator::diceRoot (const ChordDefinition& chord, juce::Random& rng, const GlobalState& g)
{
    auto c = chord;
    int root = chord.rootNote;
    for (int attempt = 0; attempt < 24 && root == chord.rootNote; ++attempt)
    {
        root = rng.nextInt (12);
        if (g.scaleLock && ! isInScale (root, g.keyRoot, g.scaleIndex, g.customScaleMask))
            root = chord.rootNote;
    }
    c.rootNote = root;
    c.isBorrowed = ! chordInKey (c, g);
    return c;
}

ChordDefinition ProgressionGenerator::diceType (const ChordDefinition& chord, juce::Random& rng)
{
    auto c = chord;
    const int cx = getChordTypeInfo (chord.chordType).complexity;
    juce::StringArray candidates;
    for (const auto& t : getChordTypes())
        if (t.complexity == cx && t.id != chord.chordType && t.id != "5")
            candidates.add (t.id);
    if (! candidates.isEmpty())
        c.chordType = candidates[rng.nextInt (candidates.size())];
    return c;
}

ChordDefinition ProgressionGenerator::diceVoicing (const ChordDefinition& chord, juce::Random& rng)
{
    auto c = chord;
    int v = chord.voicingModel;
    while (v == chord.voicingModel)
        v = rng.nextInt (VoicingCustom); // any model except Custom
    c.voicingModel = v;
    c.inversion = rng.nextInt (4);
    return c;
}

std::vector<ProgressionGenerator::Substitution> ProgressionGenerator::getSubstitutions (const ChordDefinition& chord, const GlobalState& g)
{
    std::vector<Substitution> subs;
    const auto& info = getChordTypeInfo (chord.chordType);
    const bool isMinor = info.family == FamilyMinor || info.family == FamilyDiminished;
    const int cx = info.complexity;

    auto add = [&] (const juce::String& label, int root, const juce::String& type)
    {
        ChordDefinition c = chord;
        c.rootNote = ((root % 12) + 12) % 12;
        c.chordType = type;
        c.tension = {};
        c.customOffsets = {};
        c.isBorrowed = ! chordInKey (c, g);
        subs.push_back ({ label + "  ->  " + c.getName(), c });
    };

    if (isMinor) add ("Relative major", chord.rootNote + 3, swapQuality (chord.chordType, false));
    else         add ("Relative minor", chord.rootNote + 9, swapQuality (chord.chordType, true));

    add (isMinor ? "Parallel major" : "Parallel minor", chord.rootNote, swapQuality (chord.chordType, ! isMinor));
    add ("Tritone substitution", chord.rootNote + 6, info.family == FamilyDominant ? chord.chordType : juce::String (cx >= 2 ? "9" : "7"));
    add ("Secondary dominant (V7 of this)", chord.rootNote + 7, cx >= 2 ? "9" : "7");
    add ("V/V of key", g.keyRoot + 2, cx >= 2 ? "9" : "7");
    add ("Borrowed iv", g.keyRoot + 5, cx >= 2 ? "m9" : (cx == 1 ? "m7" : "min"));
    add ("Neapolitan (bII)", g.keyRoot + 1, cx >= 1 ? "maj7" : "maj");
    add ("Backdoor bVII", g.keyRoot + 10, cx >= 2 ? "9" : "7");
    add ("Borrowed bVI", g.keyRoot + 8, cx >= 1 ? "maj7" : "maj");

    // Diatonic mediant substitutes (share two common tones)
    for (int offset : { 4, 9 })
    {
        const int root = quantizeToScale (chord.rootNote + offset, g.keyRoot, g.scaleIndex, g.customScaleMask);
        const auto pcs = getScalePitchClasses (g.keyRoot, g.scaleIndex, g.customScaleMask);
        int degree = 0;
        for (size_t i = 0; i < pcs.size(); ++i) if (pcs[i] == root) degree = (int) i;
        auto d = ChordEngine::buildDiatonicChord (degree, (int) info.intervals.size(), g.keyRoot, g.scaleIndex, g.customScaleMask);
        add (offset == 4 ? "Diatonic mediant" : "Diatonic submediant", d.rootNote, d.chordType);
    }

    return subs;
}

} // namespace chordcat

#include "ChordEngine.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <numeric>
#include <set>

namespace chordcat
{
juce::StringArray getVoicingModelNames()
{
    return { "Close", "Open", "Spread", "Drop 2", "Drop 4", "Wide Neo-Soul", "Rootless", "Quartal", "Custom" };
}

//==============================================================================
juce::String ChordDefinition::getName (bool preferFlats) const
{
    // Conventional spelling: Db Eb Ab Bb, but F#
    const bool flats = preferFlats || rootNote == 1 || rootNote == 3 || rootNote == 8 || rootNote == 10;
    auto name = chordDisplayName (rootNote, chordType, flats);
    if (tension.isNotEmpty())
        name << "(" << tension.trim() << ")";
    return name;
}

juce::ValueTree ChordDefinition::toValueTree() const
{
    juce::ValueTree t ("CHORD");
    t.setProperty ("root", rootNote, nullptr);
    t.setProperty ("type", chordType, nullptr);
    t.setProperty ("inversion", inversion, nullptr);
    t.setProperty ("voicing", voicingModel, nullptr);
    t.setProperty ("octave", octave, nullptr);
    t.setProperty ("tension", tension, nullptr);
    t.setProperty ("rhythm", rhythmPattern, nullptr);
    t.setProperty ("strum", strumTimeMs, nullptr);
    t.setProperty ("borrowed", isBorrowed, nullptr);

    juce::StringArray offs;
    for (int o : customOffsets) offs.add (juce::String (o));
    t.setProperty ("offsets", offs.joinIntoString (","), nullptr);
    return t;
}

ChordDefinition ChordDefinition::fromValueTree (const juce::ValueTree& t)
{
    ChordDefinition c;
    c.rootNote = juce::jlimit (0, 11, (int) t.getProperty ("root", 0));
    c.chordType = t.getProperty ("type", "maj").toString();
    if (findChordType (c.chordType) < 0) c.chordType = "maj";
    c.inversion = juce::jlimit (0, 3, (int) t.getProperty ("inversion", 0));
    c.voicingModel = juce::jlimit (0, NumVoicingModels - 1, (int) t.getProperty ("voicing", 0));
    c.octave = juce::jlimit (2, 6, (int) t.getProperty ("octave", 3));
    c.tension = t.getProperty ("tension", "").toString();
    c.rhythmPattern = juce::jmax (0, (int) t.getProperty ("rhythm", 0));
    c.strumTimeMs = (float) (double) t.getProperty ("strum", -1.0);
    c.isBorrowed = (bool) t.getProperty ("borrowed", false);

    auto offs = juce::StringArray::fromTokens (t.getProperty ("offsets", "").toString(), ",", "");
    for (int i = 0; i < 6 && i < offs.size(); ++i)
        c.customOffsets[(size_t) i] = juce::jlimit (-24, 24, offs[i].getIntValue());
    return c;
}

bool ChordDefinition::operator== (const ChordDefinition& o) const
{
    return rootNote == o.rootNote && chordType == o.chordType && inversion == o.inversion
        && voicingModel == o.voicingModel && octave == o.octave && tension == o.tension
        && rhythmPattern == o.rhythmPattern && customOffsets == o.customOffsets && isBorrowed == o.isBorrowed
        && std::abs (strumTimeMs - o.strumTimeMs) < 1.0e-4f;
}

//==============================================================================
ChordEngine::ChordEngine()
{
    // Default: I - vi - IV - V in C so the plug-in plays something sensible out of the box.
    const std::pair<int, const char*> defaults[] = { { 0, "maj7" }, { 9, "m7" }, { 5, "maj7" }, { 7, "7" },
                                                     { 2, "m9" }, { 4, "m7" }, { 5, "maj9" }, { 7, "13" } };
    for (size_t i = 0; i < 8; ++i)
    {
        progression[i].isActive = true;
        progression[i].chord.rootNote = defaults[i].first;
        progression[i].chord.chordType = defaults[i].second;
    }
}

std::vector<int> ChordEngine::getChordFormula (const juce::String& type)
{
    return chordcat::getChordFormula (type);
}

std::vector<int> ChordEngine::getFullFormula (const ChordDefinition& chord)
{
    auto f = getChordFormula (chord.chordType);
    for (int t : parseTensions (chord.tension))
        f.push_back (t);

    std::sort (f.begin(), f.end());
    f.erase (std::unique (f.begin(), f.end()), f.end());

    // Remove duplicate pitch classes (keep the lowest occurrence)
    std::vector<int> result;
    uint16_t seen = 0;
    for (int i : f)
    {
        const auto bit = (uint16_t) (1u << (i % 12));
        if ((seen & bit) == 0) { result.push_back (i); seen |= bit; }
    }
    return result;
}

int ChordEngine::calculateVoiceLeadingCost (const std::vector<int>& a, const std::vector<int>& b)
{
    if (a.empty() || b.empty())
        return 0;

    auto sa = a, sb = b;
    std::sort (sa.begin(), sa.end());
    std::sort (sb.begin(), sb.end());

    if (sa.size() == sb.size())
    {
        int cost = 0;
        for (size_t i = 0; i < sa.size(); ++i)
            cost += std::abs (sa[i] - sb[i]);
        return cost;
    }

    auto nearest = [] (int n, const std::vector<int>& set)
    {
        int best = INT_MAX;
        for (int m : set) best = std::min (best, std::abs (n - m));
        return best;
    };

    int cost = 0;
    for (int n : sa) cost += nearest (n, sb);
    for (int n : sb) cost += nearest (n, sa);
    return (cost + 1) / 2;
}

std::vector<int> ChordEngine::invert (std::vector<int> v, int inversion)
{
    std::sort (v.begin(), v.end());
    if (v.size() < 2)
        return v;

    for (int i = 0; i < inversion; ++i)
    {
        int low = v.front();
        v.erase (v.begin());
        while (low <= v.back()) low += 12;
        v.push_back (low);
    }
    return v;
}

std::vector<int> ChordEngine::applyVoicingModel (const std::vector<int>& formula, int model, ChordFamily family)
{
    std::vector<int> pcs;
    for (int i : formula) pcs.push_back (((i % 12) + 12) % 12);
    std::sort (pcs.begin(), pcs.end());
    pcs.erase (std::unique (pcs.begin(), pcs.end()), pcs.end());

    if (pcs.empty())
        return {};

    auto has = [&pcs] (int pc) { return std::find (pcs.begin(), pcs.end(), pc) != pcs.end(); };
    std::vector<int> v = pcs;
    const size_t n = v.size();

    switch (model)
    {
        case VoicingOpen:
            if (n >= 3) v[1] += 12;
            break;

        case VoicingSpread:
        {
            v = { pcs[0] };
            for (size_t k = 1; k < n; ++k)
                v.push_back (pcs[k] + 12 + ((k % 2 == 0) ? 12 : 0));
            break;
        }

        case VoicingDrop2:
            if (n >= 3) v[n - 2] -= 12;
            break;

        case VoicingDrop4:
            if (n >= 4) v[n - 4] -= 12;
            else if (n >= 3) v[n - 2] -= 12;
            break;

        case VoicingWideNeoSoul:
        {
            // Root low, fifth an octave+ above, third two octaves up, colours stacked above:
            // Cmaj9 -> C2 G3 E4 B4 D5 (spans ~2.5 octaves)
            v = { 0 };
            int fifth = -1, third = -1;
            for (int f : { 7, 6, 8 })    if (has (f)) { fifth = f; break; }
            for (int t : { 4, 3, 5, 2 }) if (has (t)) { third = t; break; }
            if (fifth >= 0) v.push_back (12 + fifth);
            int top = v.back();
            if (third >= 0) { v.push_back (24 + third); top = v.back(); }

            std::vector<int> rest;
            for (int pc : pcs)
                if (pc != 0 && pc != fifth && pc != third)
                    rest.push_back (pc);

            const int ref = third >= 0 ? third : (fifth >= 0 ? fifth : 0);
            std::sort (rest.begin(), rest.end(), [ref] (int a, int b) { return (a - ref + 12) % 12 < (b - ref + 12) % 12; });

            for (int pc : rest)
            {
                int note = pc;
                while (note <= top) note += 12;
                v.push_back (note);
                top = note;
            }

            for (auto& x : v) x -= 12; // the root sits an octave below the chord octave
            break;
        }

        case VoicingRootless:
        {
            std::set<int> tones (pcs.begin() + 1, pcs.end());
            const bool hasNinthLike = std::any_of (formula.begin(), formula.end(), [] (int i) { return i == 13 || i == 14 || i == 15; })
                                      || tones.count (2) > 0;
            if (! hasNinthLike) tones.insert (2);
            if (family == FamilyDominant && tones.size() < 4 && tones.count (9) == 0 && tones.count (8) == 0)
                tones.insert (9);
            if (tones.empty()) tones.insert (7);

            int third = *tones.begin();
            for (int t : { 4, 3, 5, 2 }) if (tones.count (t)) { third = t; break; }

            v.clear();
            for (int t : tones) v.push_back (t < third ? t + 12 : t);
            break;
        }

        case VoicingQuartal:
        {
            std::set<int> tones (pcs.begin(), pcs.end());
            if (family == FamilyMinor) { if (! tones.count (5)) tones.insert (5); }
            else if (! tones.count (2) && ! tones.count (1) && family != FamilyDiminished) tones.insert (2);

            const std::vector<int> list (tones.begin(), tones.end());
            int bestCost = INT_MAX;
            std::vector<int> bestChain;

            for (int start : list)
            {
                std::vector<int> remaining;
                for (int t : list) if (t != start) remaining.push_back (t);

                std::vector<int> chain { start };
                int cur = start, cost = 0;
                while (! remaining.empty())
                {
                    size_t bestIdx = 0; int bestD = 0, bestScore = INT_MAX;
                    for (size_t i = 0; i < remaining.size(); ++i)
                    {
                        const int d = ((remaining[i] - (cur % 12)) % 12 + 12) % 12;
                        const int score = std::abs (d - 5) * 10 + (d == 0 ? 1000 : 0) + d;
                        if (score < bestScore) { bestScore = score; bestIdx = i; bestD = d; }
                    }
                    cost += std::abs (bestD - 5);
                    cur += bestD;
                    chain.push_back (cur);
                    remaining.erase (remaining.begin() + (std::ptrdiff_t) bestIdx);
                }

                if (start != 0) cost += 1; // prefer root-based stacks on ties
                if (cost < bestCost) { bestCost = cost; bestChain = chain; }
            }
            v = bestChain;
            break;
        }

        case VoicingClose:
        case VoicingCustom:
        default:
            break;
    }

    std::sort (v.begin(), v.end());
    v.erase (std::unique (v.begin(), v.end()), v.end());
    return v;
}

std::vector<int> ChordEngine::getVoicedNotes (const ChordDefinition& chord, const std::vector<int>& previousNotes) const
{
    // 1. formula
    const auto formula = getFullFormula (chord);
    const auto& info = getChordTypeInfo (chord.chordType);

    // 2. voicing model
    const auto templ = applyVoicingModel (formula, chord.voicingModel, info.family);
    if (templ.empty())
        return {};

    const int base = (chord.octave + 2) * 12 + chord.rootNote;
    const int n = (int) templ.size();
    const int userInv = juce::jlimit (0, n - 1, chord.inversion);
    const float s = previousNotes.empty() ? 0.0f : juce::jlimit (0.0f, 1.0f, voiceLeadingStrength);
    const double centre = base + std::accumulate (templ.begin(), templ.end(), 0.0) / n;

    std::vector<int> best;
    double bestScore = 1.0e12;

    // 3. every inversion x octave shift is a candidate
    for (int inv = 0; inv < n; ++inv)
    {
        const auto inverted = invert (templ, inv);
        for (int shift : { 0, -12, 12, -24 })
        {
            std::vector<int> notes;
            bool valid = true;
            for (int iv : inverted)
            {
                const int note = base + iv + shift;
                if (note < 12 || note > 120) { valid = false; break; }
                notes.push_back (note);
            }
            if (! valid) continue;

            // 4. voice-leading cost vs. 5. distance to the user's chosen inversion
            const double userDistance = std::abs (inv - userInv) * 1.0 + std::abs (shift) / 12.0 * 1.5;
            const double vl = previousNotes.empty() ? 0.0 : (double) calculateVoiceLeadingCost (notes, previousNotes);
            const double mean = std::accumulate (notes.begin(), notes.end(), 0.0) / (double) notes.size();

            double score = (1.0 - s) * userDistance * 12.0 + s * vl + s * 0.35 * std::abs (mean - centre);
            if (inv == userInv && shift == 0) score -= 1.0e-3;

            if (score < bestScore) { bestScore = score; best = notes; }
        }
    }

    if (best.empty()) // extreme octave: fall back to the plain template
        for (int iv : templ) best.push_back (juce::jlimit (0, 127, base + iv));

    // 6. per-voice custom offsets (Custom editor)
    std::sort (best.begin(), best.end());
    for (size_t i = 0; i < best.size() && i < chord.customOffsets.size(); ++i)
        best[i] = juce::jlimit (0, 127, best[i] + juce::jlimit (-24, 24, chord.customOffsets[i]));

    std::sort (best.begin(), best.end());
    best.erase (std::unique (best.begin(), best.end()), best.end());
    return best;
}

//==============================================================================
ChordDefinition ChordEngine::buildDiatonicChord (int degree, int numTones, int keyRoot, int scaleIndex, uint16_t customMask)
{
    const uint16_t mask = getScaleMask (scaleIndex, customMask);
    std::vector<int> rel;
    for (int i = 0; i < 12; ++i) if (mask & (1u << i)) rel.push_back (i);

    ChordDefinition c;
    const int n = (int) rel.size();
    if (n == 0) return c;

    degree = ((degree % n) + n) % n;
    auto semis = [&] (int idx) { return rel[(size_t) (idx % n)] + 12 * (idx / n); };

    std::vector<int> full;
    for (int k = 0; k < juce::jlimit (3, 7, numTones); ++k)
        full.push_back (semis (degree + 2 * k) - semis (degree));

    // Leave out avoid-note extensions (b9 / b13 above the root): diatonic iii or v chords
    // would otherwise come out as harsh m7b9 / m11b13 colours.
    full.erase (std::remove_if (full.begin() + 1, full.end(), [] (int i) { return i == 13 || i == 20; }), full.end());

    c.rootNote = (keyRoot + rel[(size_t) degree]) % 12;

    auto tryMatch = [&] (const std::vector<int>& ivs) -> bool
    {
        uint16_t m = 0;
        for (int i : ivs) m |= (uint16_t) (1u << (i % 12));
        int cx = 0;
        for (int i : ivs) if (i > 12) cx = std::max (cx, i >= 21 ? 4 : (i >= 17 ? 3 : 2));
        if (ivs.size() == 4 && cx == 0) cx = 1;
        const int idx = findTypeForMask (m, cx);
        if (idx < 0) return false;
        c.chordType = getChordTypes()[(size_t) idx].id;
        return true;
    };

    for (int t = (int) full.size(); t >= 3; --t)
    {
        std::vector<int> ivs (full.begin(), full.begin() + t);
        auto without = [] (std::vector<int> v, int interval) { v.erase (std::remove (v.begin(), v.end(), interval), v.end()); return v; };

        if (tryMatch (ivs)) return c;
        const bool majorThird = std::find (ivs.begin(), ivs.end(), 4) != ivs.end();
        if (majorThird && tryMatch (without (ivs, 17))) return c;          // avoid natural 11 over a major 3rd
        if (tryMatch (without (ivs, 17))) return c;
        if (tryMatch (without (without (ivs, 17), 7))) return c;
    }

    std::vector<int> notes;
    for (int i : full) notes.push_back (60 + c.rootNote + i);
    const auto rec = recognizeChord (notes);
    if (rec.valid) { c.rootNote = rec.root; c.chordType = rec.type; }
    return c;
}

ChordDefinition ChordEngine::applyScaleLock (const ChordDefinition& chord, const GlobalState& g)
{
    if (! g.scaleLock)
        return chord;

    ChordDefinition c = chord;
    const auto& info = getChordTypeInfo (c.chordType);

    bool allIn = true;
    for (int iv : getFullFormula (c))
        if (! isInScale (c.rootNote + iv, g.keyRoot, g.scaleIndex, g.customScaleMask))
            allIn = false;

    if (allIn)
    {
        c.isBorrowed = false;
        return c;
    }

    const int root = quantizeToScale (c.rootNote, g.keyRoot, g.scaleIndex, g.customScaleMask);
    const auto pcs = getScalePitchClasses (g.keyRoot, g.scaleIndex, g.customScaleMask);
    int degree = 0;
    for (size_t i = 0; i < pcs.size(); ++i)
        if (pcs[i] == root) degree = (int) i;

    const auto diatonic = buildDiatonicChord (degree, (int) info.intervals.size(), g.keyRoot, g.scaleIndex, g.customScaleMask);
    c.rootNote = diatonic.rootNote;
    c.chordType = diatonic.chordType;
    c.tension = {};
    c.isBorrowed = false;
    return c;
}

std::vector<int> ChordEngine::voiceChord (const ChordDefinition& chord, const GlobalState& g, bool updateHistory)
{
    auto c = applyScaleLock (chord, g);
    const int absolute = (c.octave + 2) * 12 + c.rootNote + g.transpose;
    c.rootNote = ((absolute % 12) + 12) % 12;
    c.octave = (int) std::floor (absolute / 12.0) - 2;

    auto notes = getVoicedNotes (c, lastPlayedNotes);
    if (updateHistory && ! notes.empty())
        lastPlayedNotes = notes;
    return notes;
}

std::vector<int> ChordEngine::voiceSlot (int slot, const GlobalState& g)
{
    slot = juce::jlimit (0, (int) progression.size() - 1, slot);
    currentSlot = slot;
    return voiceChord (progression[(size_t) slot].chord, g, true);
}

std::vector<int> ChordEngine::previewSlot (int slot, const GlobalState& g, const std::vector<int>& previous) const
{
    auto c = applyScaleLock (progression[(size_t) juce::jlimit (0, 15, slot)].chord, g);
    const int absolute = (c.octave + 2) * 12 + c.rootNote + g.transpose;
    c.rootNote = ((absolute % 12) + 12) % 12;
    c.octave = (int) std::floor (absolute / 12.0) - 2;
    return getVoicedNotes (c, previous);
}

int ChordEngine::getNumActiveSlots() const
{
    int n = 0;
    for (const auto& s : progression) if (s.isActive) ++n;
    return n;
}

juce::ValueTree ChordEngine::progressionToValueTree() const
{
    juce::ValueTree t (progressionId);
    for (size_t i = 0; i < progression.size(); ++i)
    {
        auto slot = progression[i].chord.toValueTree();
        slot.setProperty ("index", (int) i, nullptr);
        slot.setProperty ("active", progression[i].isActive, nullptr);
        t.appendChild (slot, nullptr);
    }
    return t;
}

void ChordEngine::progressionFromValueTree (const juce::ValueTree& t)
{
    if (! t.isValid())
        return;

    for (auto& s : progression) s = {};

    for (const auto& child : t)
    {
        const int idx = child.getProperty ("index", -1);
        if (juce::isPositiveAndBelow (idx, (int) progression.size()))
        {
            progression[(size_t) idx].chord = ChordDefinition::fromValueTree (child);
            progression[(size_t) idx].isActive = (bool) child.getProperty ("active", true);
        }
    }
    resetVoiceLeading();
}

} // namespace chordcat

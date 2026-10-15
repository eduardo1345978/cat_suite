#include "LifeMusic.h"
#include <algorithm>

namespace lifecat
{
namespace
{
    struct ScaleDef { const char* name; std::vector<int> steps; };
    const std::vector<ScaleDef>& scales()
    {
        static const std::vector<ScaleDef> s = {
            { "Major",            { 0, 2, 4, 5, 7, 9, 11 } },
            { "Natural Minor",    { 0, 2, 3, 5, 7, 8, 10 } },
            { "Dorian",           { 0, 2, 3, 5, 7, 9, 10 } },
            { "Mixolydian",       { 0, 2, 4, 5, 7, 9, 10 } },
            { "Lydian",           { 0, 2, 4, 6, 7, 9, 11 } },
            { "Phrygian",         { 0, 1, 3, 5, 7, 8, 10 } },
            { "Harmonic Minor",   { 0, 2, 3, 5, 7, 8, 11 } },
            { "Major Pentatonic", { 0, 2, 4, 7, 9 } },
            { "Minor Pentatonic", { 0, 3, 5, 7, 10 } },
            { "Japanese (In)",    { 0, 1, 5, 7, 8 } },
            { "Whole Tone",       { 0, 2, 4, 6, 8, 10 } },
        };
        return s;
    }

    /** All notes of the scale between low and high. */
    int scaleNotes (int key, int scale, int low, int high, int* out, int maxOut)
    {
        const auto& st = scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, scale)].steps;
        int n = 0;
        for (int note = low; note <= high && n < maxOut; ++note)
        {
            const int pc = ((note - key) % 12 + 12) % 12;
            if (std::find (st.begin(), st.end(), pc) != st.end()) out[n++] = note;
        }
        return n;
    }

    /** Note of scale degree `deg` (0 = root, may be > 7) in the octave starting at `octaveBase`. */
    int degreeNote (int key, int scale, int octaveBase, int deg)
    {
        const auto& st = scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, scale)].steps;
        const int n = (int) st.size();
        const int oct = deg >= 0 ? deg / n : -((-deg + n - 1) / n);
        const int idx = deg - oct * n;
        return octaveBase + key + oct * 12 + st[(size_t) idx];
    }
}

juce::StringArray scaleNames()
{
    juce::StringArray n;
    for (const auto& s : scales()) n.add (s.name);
    return n;
}
std::vector<juce::uint16> scaleMasks()
{
    std::vector<juce::uint16> m;
    for (const auto& sc : scales())
    {
        juce::uint16 v = 0;
        for (int st : sc.steps) v |= (juce::uint16) (1u << st);
        m.push_back (v);
    }
    return m;
}

juce::StringArray pitchModeNames() { return { "Position (height)", "Species", "Held chord" }; }
juce::StringArray gridNames() { return { "1/4", "1/8", "1/16", "1/8 T", "1/16 T", "1/32" }; }
double gridBeats (int i) { static const double g[] = { 1.0, 0.5, 0.25, 1.0 / 3.0, 1.0 / 6.0, 0.125 }; return g[juce::jlimit (0, 5, i)]; }

int Conductor::scaleStep (int note, int steps, int key, int scale)
{
    int notes[128];
    const int n = scaleNotes (key, scale, 0, 127, notes, 128);
    if (n == 0) return note;
    int idx = 0;
    while (idx < n - 1 && notes[idx] < note) ++idx;
    return notes[juce::jlimit (0, n - 1, idx + steps)];
}

double Conductor::divisionFor (const Creature& c)
{
    static const double d[] = { 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };
    return d[juce::jlimit (0, 5, (int) (c.genes[gRhythm] * 6.0f))];
}

float Conductor::pitchFor (const Creature& c, const MusicSettings& m, const int* held, int numHeld) const
{
    const auto& g = c.genes;
    const int octShift = g[gRegister] < 0.2f ? -12 : (g[gRegister] > 0.8f ? 12 : 0);
    int mode = m.pitchMode;
    if (mode == 2 && numHeld == 0) mode = 1;     // nothing held: fall back to species

    int note = 60;
    if (mode == 0)
    {
        int notes[128];
        const int n = scaleNotes (m.key, m.scale, m.lowNote, m.highNote, notes, 128);
        if (n == 0) return 60.0f;
        const float h = 1.0f - c.y;
        int idx = juce::roundToInt (h * (float) (n - 1)) + juce::roundToInt ((g[gPitch] - 0.5f) * 4.0f);
        if (c.role == Role::Swarm) idx += 2 * (c.groupRank % 4);
        idx = juce::jlimit (0, n - 1, idx);
        note = notes[idx];
        if (octShift != 0 && note + octShift >= m.lowNote && note + octShift <= m.highNote) note += octShift;
    }
    else if (mode == 1)
    {
        // chord tones per role (scale degrees): grazers 1-3-5 in the middle, hunters root / fifth low,
        // drifters 5-7-9 high
        static const int tables[numRoles][4] = {
            { 0, 2, 4, 7 },      // grazers: 1-3-5 in the middle
            { 0, 4, 0, 7 },      // hunters: root / fifth, low
            { 4, 6, 8, 9 },      // drifters: 5-7-9 high
            { 7, 9, 11, 14 },    // pollinators: high sparkle
            { 0, 4, 2, 7 },      // scavengers: percussion around the root
            { 0, 0, 4, 0 },      // giants: deep root drones
            { 0, 2, 4, 7 },      // swarms: arpeggio of the chord
            { 0, 2, 4, 7 },      // singers: phrases start on a chord tone
            { 0, 0, 4, 7 },      // drummers: tuned around the root, low
            { 0, 4, 2, 5 } };    // builders: wooden middle notes
        static const int bases[numRoles] = { 48, 36, 60, 60, 48, 24, 60, 60, 36, 48 };
        const int ri = (int) c.role;
        int degree = tables[ri][juce::jlimit (0, 3, (int) (g[gPitch] * 4.0f))];
        if (c.role == Role::Swarm) degree = tables[ri][c.groupRank % 4] + 7 * ((c.groupRank / 4) % 2);
        note = degreeNote (m.key, m.scale, bases[ri], degree) + octShift;
    }
    else
    {
        // a tone of the held chord; height in the world adds octaves
        int pick = juce::jlimit (0, numHeld - 1, (int) (g[gPitch] * (float) numHeld));
        int octaves = juce::roundToInt ((1.0f - c.y) * 2.0f) - 1;
        if (c.role == Role::Swarm) { pick = c.groupRank % numHeld; octaves = (c.groupRank / numHeld) % 2; }
        if (c.role == Role::Giant) { pick = 0; octaves = -2; }
        note = held[pick] + 12 * octaves + octShift;
    }
    if (c.role == Role::Giant)
        while (note > m.lowNote + 12) note -= 12;      // giants stay at the bottom of the range
    while (note < m.lowNote) note += 12;
    while (note > m.highNote) note -= 12;
    note += 12 * m.speciesOctave[(size_t) c.role];
    return (float) juce::jlimit (0, 127, note);
}

void Conductor::process (World& w, const MusicSettings& m, double ppqStart, double ppqPerSample, int numSamples,
                         const int* held, int numHeld, std::vector<SungNote>& out)
{
    if (ppqPerSample <= 0.0 || numSamples <= 0) return;
    const double ppqEnd = ppqStart + ppqPerSample * numSamples;
    const double grid = gridBeats (m.grid);

    auto sing = [&] (int ci, double atPpq, double divisionBeats, int forcedNote = -1)
    {
        auto& c = w.creatures[(size_t) ci];
        SungNote s;
        s.sample = juce::jlimit (0, numSamples - 1, (int) ((atPpq - ppqStart) / ppqPerSample));
        s.creature = ci;
        s.midiNote = forcedNote >= 0 ? (float) forcedNote : pitchFor (c, m, held, numHeld);
        s.velocity = juce::jlimit (0.1f, 1.0f, 0.3f + 0.45f * juce::jlimit (0.0f, 1.0f, c.energy) + 0.25f * c.genes[gSize]);
        s.lengthBeats = (float) (juce::jmax (grid, divisionBeats) * (0.35 + 1.2 * c.genes[gRelease]) * m.lengthScale);
        s.pan = c.x * 2.0f - 1.0f;
        // accents: louder on the beat, a little random variety
        const bool onBeat = std::abs (atPpq - std::round (atPpq)) < 1.0e-3;
        s.velocity = juce::jlimit (0.08f, 1.0f, s.velocity * (1.0f + c.genes[gAccent] * (0.35f * rng.bi() + (onBeat ? 0.25f : -0.12f))));
        c.singFlash = 1.0f;
        out.push_back (s);
        if (forcedNote >= 0) return;     // phrase notes: no extra interval
        // interval gene: now and then a second note, a third or a fifth above
        if (rng.uni() < c.genes[gInterval] * 0.5f)
        {
            auto s2 = s;
            const int base = juce::roundToInt (s.midiNote);
            int second = scaleStep (base, c.genes[gHarm] < 0.5f ? 2 : 4, m.key, m.scale);
            if (m.pitchMode == 2 && numHeld > 0)
            {
                // following a chord: the next chord tone above instead
                second = base + 12;
                for (int k = 0; k < numHeld; ++k)
                    for (int o = -1; o <= 2; ++o)
                    {
                        const int cand = held[k] + 12 * (o + (base - held[k]) / 12);
                        if (cand > base && cand < second) second = cand;
                    }
            }
            s2.midiNote = (float) juce::jlimit (0, 127, second);
            s2.velocity *= 0.8f;
            out.push_back (s2);
        }
    };
    auto wants = [&] (const Creature& c)
    {
        if (! m.speciesSing[(size_t) c.role]) return false;
        const float p = m.density * (0.2f + 0.8f * c.genes[gDensity]) * (0.5f + 0.5f * juce::jlimit (0.0f, 1.0f, c.energy))
                      * (0.15f + 0.85f * c.activity);              // sleepers sing rarely
        return rng.uni() < p;
    };

    // swarm flocks play arpeggios: one member per grid step, in flock order
    for (int i = 0; i < maxCreatures; ++i)
    {
        const auto& c = w.creatures[(size_t) i];
        int n = 0;
        if (c.alive && c.role == Role::Swarm)
            for (const auto& o : w.creatures) n += (o.alive && o.role == Role::Swarm && o.group == c.group) ? 1 : 0;
        flockSize[(size_t) i] = n;
    }

    if (m.quantize)
    {
        const long long k0 = (long long) std::ceil (ppqStart / grid - 1.0e-9);
        for (long long k = k0;; ++k)
        {
            double at = (double) k * grid;
            const bool offBeat = (k & 1) != 0 && (grid == 0.5 || grid == 0.25 || grid == 0.125);
            if (offBeat) at += grid * 0.5 * (m.swing / 100.0);
            if (at >= ppqEnd) break;
            if (at < ppqStart) continue;

            candidates.clear();
            int sungHere = 0;
            for (int i = 0; i < maxCreatures; ++i)
            {
                const auto& c = w.creatures[(size_t) i];
                if (! c.alive) continue;
                auto& ph = phrases[(size_t) i];
                if (ph.id != c.id) ph = Phrase { c.id };

                // singers: a phrase in progress goes on, note by note, stepping through the scale
                if (c.role == Role::Singer && ph.left > 0)
                {
                    if (k >= ph.nextK && m.speciesSing[(size_t) c.role])
                    {
                        int step = rng.uni() < 0.3f + 0.4f * c.genes[gHarm] ? 2 : 1;
                        if (ph.note + 2 > m.highNote) ph.dir = -1;
                        if (ph.note - 2 < m.lowNote) ph.dir = 1;
                        if (rng.uni() < 0.25f) ph.dir = -ph.dir;
                        ph.note = scaleStep (ph.note, ph.dir * step, m.key, m.scale);
                        if (m.pitchMode == 2 && numHeld > 0 && ph.left == 1)
                            ph.note = juce::roundToInt (pitchFor (c, m, held, numHeld));      // end on a chord tone
                        --ph.left;
                        ph.nextK = k + ph.stride;
                        sing (i, at, grid * ph.stride * 1.3, juce::jlimit (0, 127, ph.note));
                        ++sungHere;
                    }
                    continue;
                }
                // drummers: an euclidean pattern on the grid
                if (c.role == Role::Drummer)
                {
                    const long long nSteps = grid <= 0.25 ? 16 : 8;
                    const long long hits = 2 + (long long) (c.genes[gDensity] * 5.0f);
                    const long long pos = ((k + (long long) (c.id % (uint32_t) nSteps)) % nSteps + nSteps) % nSteps;
                    if ((pos * hits) % nSteps < hits && m.speciesSing[(size_t) c.role]
                        && rng.uni() < 0.35f + 0.65f * m.density * (0.4f + 0.6f * c.activity))
                    {
                        sing (i, at, grid);
                        ++sungHere;
                    }
                    continue;
                }
                const double d = juce::jmax (grid, divisionFor (c));
                long long stepsPer = juce::jmax (1LL, (long long) std::llround (d / grid));
                long long offset = (long long) ((c.id * 7u + (uint32_t) (c.genes[gPitch] * 5.0f)) % (uint32_t) stepsPer);
                if (c.role == Role::Swarm && flockSize[(size_t) i] > 1)
                {
                    stepsPer = juce::jlimit (2LL, 16LL, (long long) flockSize[(size_t) i]);
                    offset = c.groupRank % stepsPer;
                }
                if (((k - offset) % stepsPer + stepsPer) % stepsPer != 0) continue;
                if (wants (c)) candidates.push_back (i);
            }
            // the strongest sing first
            std::sort (candidates.begin(), candidates.end(), [&w] (int a, int b) { return w.creatures[(size_t) a].energy > w.creatures[(size_t) b].energy; });
            for (int j = 0; j < (int) candidates.size() && j < juce::jmax (1, m.maxNotes - sungHere / 2); ++j)
            {
                const int ci = candidates[(size_t) j];
                auto& c = w.creatures[(size_t) ci];
                if (c.role == Role::Singer)
                {
                    // start a phrase: 3..8 notes, one every 1 or 2 grid steps
                    auto& ph = phrases[(size_t) ci];
                    ph.note = juce::roundToInt (pitchFor (c, m, held, numHeld));
                    ph.left = 2 + (int) (c.genes[gDensity] * 6.0f);
                    ph.stride = c.genes[gRhythm] > 0.6f ? 1 : 2;
                    ph.dir = rng.uni() < 0.5f ? 1 : -1;
                    ph.nextK = k + ph.stride;
                    sing (ci, at, grid * ph.stride * 1.3, ph.note);
                    continue;
                }
                sing (ci, at, divisionFor (c));
            }
        }
    }
    else
    {
        // free floating: every creature on its own clock, slightly faster or slower
        const double beats = ppqEnd - ppqStart;
        int sungThisBlock = 0;
        for (int i = 0; i < maxCreatures; ++i)
        {
            auto& c = w.creatures[(size_t) i];
            if (! c.alive) continue;
            const double d = divisionFor (c) * (0.85 + 0.3 * c.genes[gHarm]);
            const double before = c.freePhase;
            c.freePhase += (float) beats;
            if (c.freePhase >= d)
            {
                c.freePhase -= (float) (d * std::floor (c.freePhase / d));
                const double crossAt = ppqStart + juce::jlimit (0.0, beats, d - before);
                if (wants (c) && sungThisBlock < m.maxNotes)
                {
                    sing (i, crossAt, divisionFor (c));
                    ++sungThisBlock;
                }
            }
        }
    }
    std::sort (out.begin(), out.end(), [] (const SungNote& a, const SungNote& b) { return a.sample < b.sample; });
}

} // namespace lifecat

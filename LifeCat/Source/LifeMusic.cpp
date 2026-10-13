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
juce::StringArray pitchModeNames() { return { "Position (height)", "Species", "Held chord" }; }
juce::StringArray gridNames() { return { "1/4", "1/8", "1/16", "1/8 T", "1/16 T", "1/32" }; }
double gridBeats (int i) { static const double g[] = { 1.0, 0.5, 0.25, 1.0 / 3.0, 1.0 / 6.0, 0.125 }; return g[juce::jlimit (0, 5, i)]; }

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
        idx = juce::jlimit (0, n - 1, idx);
        note = notes[idx];
        if (octShift != 0 && note + octShift >= m.lowNote && note + octShift <= m.highNote) note += octShift;
    }
    else if (mode == 1)
    {
        // chord tones per role (scale degrees): grazers 1-3-5 in the middle, hunters root / fifth low,
        // drifters 5-7-9 high
        static const int grazer[] = { 0, 2, 4, 7 }, hunter[] = { 0, 4, 0, 7 }, drifter[] = { 4, 6, 8, 9 };
        const int* table = c.role == Role::Grazer ? grazer : (c.role == Role::Hunter ? hunter : drifter);
        const int pick = juce::jlimit (0, 3, (int) (g[gPitch] * 4.0f));
        const int base = c.role == Role::Hunter ? 36 : (c.role == Role::Drifter ? 60 : 48);
        note = degreeNote (m.key, m.scale, base, table[pick]) + octShift;
    }
    else
    {
        // a tone of the held chord; height in the world adds octaves
        const int pick = juce::jlimit (0, numHeld - 1, (int) (g[gPitch] * (float) numHeld));
        const int octaves = juce::roundToInt ((1.0f - c.y) * 2.0f) - 1;
        note = held[pick] + 12 * octaves + octShift;
    }
    while (note < m.lowNote) note += 12;
    while (note > m.highNote) note -= 12;
    return (float) note;
}

void Conductor::process (World& w, const MusicSettings& m, double ppqStart, double ppqPerSample, int numSamples,
                         const int* held, int numHeld, std::vector<SungNote>& out)
{
    if (ppqPerSample <= 0.0 || numSamples <= 0) return;
    const double ppqEnd = ppqStart + ppqPerSample * numSamples;
    const double grid = gridBeats (m.grid);

    auto sing = [&] (int ci, double atPpq, double divisionBeats)
    {
        auto& c = w.creatures[(size_t) ci];
        SungNote s;
        s.sample = juce::jlimit (0, numSamples - 1, (int) ((atPpq - ppqStart) / ppqPerSample));
        s.creature = ci;
        s.midiNote = pitchFor (c, m, held, numHeld);
        s.velocity = juce::jlimit (0.1f, 1.0f, 0.3f + 0.45f * juce::jlimit (0.0f, 1.0f, c.energy) + 0.25f * c.genes[gSize]);
        s.lengthBeats = (float) (juce::jmax (grid, divisionBeats) * (0.35 + 1.2 * c.genes[gRelease]) * m.lengthScale);
        s.pan = c.x * 2.0f - 1.0f;
        c.singFlash = 1.0f;
        out.push_back (s);
    };
    auto wants = [&] (const Creature& c)
    {
        const float p = m.density * (0.2f + 0.8f * c.genes[gDensity]) * (0.5f + 0.5f * juce::jlimit (0.0f, 1.0f, c.energy));
        return rng.uni() < p;
    };

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
            for (int i = 0; i < maxCreatures; ++i)
            {
                const auto& c = w.creatures[(size_t) i];
                if (! c.alive) continue;
                const double d = juce::jmax (grid, divisionFor (c));
                const long long stepsPer = juce::jmax (1LL, (long long) std::llround (d / grid));
                const long long offset = (long long) ((c.id * 7u + (uint32_t) (c.genes[gPitch] * 5.0f)) % (uint32_t) stepsPer);
                if (((k - offset) % stepsPer + stepsPer) % stepsPer != 0) continue;
                if (wants (c)) candidates.push_back (i);
            }
            // the strongest sing first
            std::sort (candidates.begin(), candidates.end(), [&w] (int a, int b) { return w.creatures[(size_t) a].energy > w.creatures[(size_t) b].energy; });
            for (int j = 0; j < (int) candidates.size() && j < m.maxNotes; ++j)
                sing (candidates[(size_t) j], at, divisionFor (w.creatures[(size_t) candidates[(size_t) j]]));
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

#include "BassEngine.h"

namespace basscat
{
juce::StringArray getDegreeNames() { return { "Root", "5th", "Oct", "3rd", "7th", "Appr", "Up", "Down", "Ghost" }; }

juce::StringArray getStyleNames()
{
    return { "Pop Roots", "Walking", "Neo Soul", "Funk", "Boom Bap", "Dilla", "808", "Reggae", "House", "Disco Octaves", "Latin", "Drone",
             "Rock", "Dub", "Country", "Afrobeats", "Drill", "Synth Arp" };
}

StyleHints getStyleHints (int style)
{
    switch (style)
    {
        case Walking:  return { 20.0f, 4.0f };
        case NeoSoul:  return { 12.0f, 5.0f };
        case Dilla:    return { 0.0f, 9.0f };
        case BoomBap:  return { 10.0f, 4.0f };
        case Trap808:  return { 0.0f, 0.0f };
        case House:    return { 0.0f, 1.0f };
        case Drill:
        case SynthArp: return { 0.0f, 0.0f };
        case Afrobeats: return { 15.0f, 4.0f };
        case Dub:      return { 0.0f, 5.0f };
        default:       return { 0.0f, 3.0f };
    }
}

//==============================================================================
juce::ValueTree Pattern::toValueTree() const
{
    juce::ValueTree t (treeId);
    t.setProperty ("bars", bars, nullptr);
    for (const auto& n : notes)
    {
        juce::ValueTree c ("N");
        c.setProperty ("p", n.pos, nullptr);
        c.setProperty ("l", n.len, nullptr);
        c.setProperty ("d", n.degree, nullptr);
        c.setProperty ("o", n.octave, nullptr);
        c.setProperty ("v", n.velocity, nullptr);
        c.setProperty ("s", n.slide, nullptr);
        c.setProperty ("m", n.micro, nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

Pattern Pattern::fromValueTree (const juce::ValueTree& t)
{
    Pattern p;
    p.bars = juce::jlimit (1, 4, (int) t.getProperty ("bars", 2));
    for (const auto& c : t)
    {
        BassNote n;
        n.pos = c.getProperty ("p", 0.0);
        n.len = c.getProperty ("l", 0.5);
        n.degree = juce::jlimit (0, NumDegrees - 1, (int) c.getProperty ("d", 0));
        n.octave = juce::jlimit (-2, 2, (int) c.getProperty ("o", 0));
        n.velocity = juce::jlimit (1, 127, (int) c.getProperty ("v", 100));
        n.slide = c.getProperty ("s", false);
        n.micro = c.getProperty ("m", 0.0);
        p.notes.push_back (n);
    }
    return p;
}

//==============================================================================
namespace
{
    struct StyleModel
    {
        std::array<float, 16> onset;   // probability of a note on each 16th of the bar (>= 0.9 = anchor)
        float legato;                  // 0 = short notes, 1 = held until the next note
        float shortLen;                // ppq length when not legato
        float ghostWeight, slideWeight;
        float late;                    // ppq the style lays behind the beat
        float looseness;               // ppq random timing
    };

    StyleModel model (int style)
    {
        switch (style)
        {
            //                    1    e    &    a    2    e    &    a    3    e    &    a    4    e    &    a
            case PopRoots:    return { { 1, 0, .9f, 0, .95f, 0, .9f, 0, .95f, 0, .9f, 0, .95f, 0, .9f, .15f }, 0.85f, 0.4f, 0.2f, 0.1f, 0.0f, 0.004f };
            case Walking:     return { { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, .12f },               0.95f, 0.9f, 0.3f, 0.0f, 0.0f, 0.006f };
            case NeoSoul:     return { { 1, 0, 0, .35f, 0, 0, .55f, .25f, .2f, 0, .55f, 0, .3f, .25f, 0, .4f }, 0.7f, 0.3f, 1.0f, 0.6f, 0.02f, 0.012f };
            case Funk:        return { { 1, 0, .45f, .5f, .3f, .5f, .2f, .6f, .7f, 0, .45f, .4f, .3f, .6f, .4f, .5f }, 0.25f, 0.18f, 1.4f, 0.2f, 0.0f, 0.008f };
            case BoomBap:     return { { 1, 0, 0, 0, 0, 0, 0, .45f, .25f, 0, .65f, 0, 0, 0, .35f, 0 },      0.8f, 0.5f, 0.4f, 0.2f, 0.03f, 0.012f };
            case Dilla:       return { { 1, 0, 0, .4f, 0, 0, .35f, 0, 0, .35f, .45f, 0, 0, .3f, 0, .25f },   0.75f, 0.35f, 0.7f, 0.4f, 0.045f, 0.03f };
            case Trap808:     return { { 1, 0, 0, 0, 0, 0, .3f, 0, 0, 0, .55f, 0, 0, .35f, .25f, 0 },       1.0f, 1.0f, 0.0f, 1.2f, 0.0f, 0.0f };
            case Reggae:      return { { 0, 0, .45f, 0, .95f, 0, .45f, .5f, 0, 0, .65f, 0, .9f, 0, .45f, .35f }, 0.6f, 0.4f, 0.3f, 0.1f, 0.02f, 0.01f };
            case House:       return { { 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, .2f },                0.35f, 0.35f, 0.3f, 0.0f, 0.0f, 0.002f };
            case DiscoOctaves:return { { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 },                  0.4f, 0.3f, 0.3f, 0.0f, 0.0f, 0.003f };
            case Latin:       return { { 0, 0, 0, .3f, 0, 0, 1, 0, 0, 0, .3f, 0, 1, 0, .25f, 0 },           0.9f, 0.6f, 0.2f, 0.1f, 0.0f, 0.006f };
            case Drone:       return { { 1, 0, 0, 0, 0, 0, 0, 0, .3f, 0, 0, 0, 0, 0, 0, 0 },                1.0f, 4.0f, 0.0f, 0.3f, 0.0f, 0.0f };
            case Rock:        return { { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, .2f },                  0.75f, 0.42f, 0.1f, 0.0f, 0.0f, 0.004f };
            case Dub:         return { { 1, 0, 0, 0, 0, 0, .5f, 0, .95f, 0, 0, 0, 0, 0, .4f, .3f },         0.95f, 0.9f, 0.2f, 0.3f, 0.03f, 0.012f };
            case Country:     return { { 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, .25f, 0 },               0.6f, 0.8f, 0.1f, 0.0f, 0.0f, 0.006f };
            case Afrobeats:   return { { 1, 0, 0, .5f, 0, 0, .7f, 0, .4f, 0, .6f, 0, 0, .4f, .3f, 0 },       0.55f, 0.3f, 0.5f, 0.2f, 0.01f, 0.01f };
            case Drill:       return { { 1, 0, 0, .35f, 0, 0, .55f, 0, 0, .3f, .5f, 0, 0, .4f, .3f, 0 },     1.0f, 1.0f, 0.0f, 1.4f, 0.0f, 0.0f };
            case SynthArp:    return { { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },                  0.3f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f };
            default:          return model (PopRoots);
        }
    }

    /** Which degree a note on this step plays, by style habit. */
    int pickDegree (int style, int step, juce::Random& r, int previousDegree)
    {
        const float x = r.nextFloat();
        const bool down = step == 0, beat = step % 4 == 0, offbeat = step % 2 == 1;
        switch (style)
        {
            case Walking:
                if (step == 0) return Root;
                if (step == 12) return Approach;
                if (step == 4) return x < 0.4f ? Third : (x < 0.75f ? WalkUp : Fifth);
                if (step == 8) return x < 0.45f ? Fifth : (x < 0.7f ? WalkUp : (x < 0.85f ? Octave : WalkDown));
                return Approach;
            case DiscoOctaves:
                return (step / 2) % 2 == 0 ? Root : Octave;
            case House:
                return x < 0.75f ? Root : Octave;
            case Funk:
                if (down) return Root;
                if (offbeat && x < 0.35f) return Octave;
                return x < 0.55f ? Root : (x < 0.75f ? Seventh : (x < 0.9f ? Fifth : Approach));
            case NeoSoul:
                if (down) return Root;
                if (step == 15 || step == 3) return x < 0.6f ? Approach : WalkUp;
                return x < 0.35f ? Root : (x < 0.55f ? Fifth : (x < 0.75f ? Seventh : (x < 0.9f ? Octave : Third)));
            case Trap808:
                if (down) return Root;
                return x < 0.5f ? Root : (x < 0.8f ? Octave : (x < 0.92f ? Seventh : Fifth));
            case Reggae:
                if (step == 4) return Root;
                return x < 0.4f ? Root : (x < 0.65f ? Fifth : (x < 0.85f ? Third : WalkUp));
            case Latin:
                if (step == 6) return Fifth;
                if (step == 12) return Root;
                return x < 0.5f ? Octave : Fifth;
            case Drone:
                return step == 0 ? Root : Fifth;
            case Rock:
                if (step == 14 || step == 15) return x < 0.5f ? Approach : WalkUp;
                return x < 0.85f ? Root : (x < 0.95f ? Fifth : Octave);
            case Dub:
                if (down) return Root;
                return x < 0.45f ? Root : (x < 0.7f ? Fifth : (x < 0.85f ? Seventh : Octave));
            case Country:
                return step == 0 ? Root : (step == 8 ? Fifth : (x < 0.5f ? WalkUp : Approach));
            case Afrobeats:
                if (down) return Root;
                return x < 0.45f ? Root : (x < 0.65f ? Octave : (x < 0.85f ? Fifth : Third));
            case Drill:
                if (down) return Root;
                return x < 0.45f ? Root : (x < 0.75f ? Octave : (x < 0.9f ? Third : Fifth));
            case SynthArp:
            {
                static const int arp[] = { Root, Fifth, Octave, Fifth, Root, Third, Octave, Third };
                return arp[step % 8];
            }
            case BoomBap:
            case Dilla:
                if (down) return Root;
                return x < 0.5f ? Root : (x < 0.7f ? Fifth : (x < 0.85f ? Octave : Seventh));
            case PopRoots:
            default:
                if (beat || previousDegree < 0) return x < 0.85f ? Root : Fifth;
                return x < 0.7f ? Root : (x < 0.85f ? Octave : Fifth);
        }
    }

    std::vector<BassNote> makeBar (const GeneratorSettings& s, juce::Random& r, const std::vector<BassNote>* base, float rerollAmount)
    {
        const auto m = model (s.style);
        std::vector<BassNote> bar;
        int prevDegree = -1;

        for (int step = 0; step < 16; ++step)
        {
            const float p = m.onset[(size_t) step];
            const bool anchor = p >= 0.9f;

            // Variations keep most of the core bar and re-roll the rest
            if (base != nullptr && ! anchor && r.nextFloat() >= rerollAmount)
            {
                for (const auto& n : *base)
                    if ((int) std::round (n.pos * 4.0) == step && n.degree != Ghost)
                    {
                        bar.push_back (n);
                        prevDegree = n.degree;
                    }
                continue;
            }

            const float scaled = anchor ? 1.0f : juce::jlimit (0.0f, 1.0f, p * (0.3f + 1.4f * s.density));
            if (r.nextFloat() >= scaled)
                continue;

            BassNote n;
            n.pos = step * 0.25;
            n.degree = pickDegree (s.style, step, r, prevDegree);
            n.velocity = juce::jlimit (1, 127, (step % 4 == 0 ? 112 : (step % 2 == 0 ? 100 : 90)) + r.nextInt (11) - 5);
            n.micro = m.late + (r.nextFloat() * 2.0f - 1.0f) * m.looseness;
            if (step == 0) n.micro = juce::jmax (0.0, n.micro);
            bar.push_back (n);
            prevDegree = n.degree;
        }

        std::sort (bar.begin(), bar.end(), [] (const BassNote& a, const BassNote& b) { return a.pos < b.pos; });

        // Lengths: up to the next note (legato styles) or short
        for (size_t i = 0; i < bar.size(); ++i)
        {
            const double next = i + 1 < bar.size() ? bar[i + 1].pos : 4.0;
            const double gap = juce::jmax (0.125, next - bar[i].pos);
            bar[i].len = juce::jmax (0.08, juce::jmin ((double) m.shortLen + (gap - m.shortLen) * m.legato, gap * 0.98));
            if (s.style == Drone) bar[i].len = gap * 0.99;

            // Slides into a different degree (808 glides, neo-soul slides)
            bar[i].slide = i + 1 < bar.size() && bar[i + 1].degree != bar[i].degree
                           && r.nextFloat() < s.slides * m.slideWeight;
            if (bar[i].slide) bar[i].len = gap + 0.03;
        }

        // Ghost notes: muted plucks in the gaps
        if (m.ghostWeight > 0.0f)
        {
            std::vector<BassNote> ghosts;
            for (int step = 1; step < 16; ++step)
            {
                const bool taken = std::any_of (bar.begin(), bar.end(), [step] (const BassNote& n)
                {
                    return n.pos <= step * 0.25 + 1.0e-6 && n.pos + n.len > step * 0.25 + 0.01;
                });
                if (taken || r.nextFloat() >= s.ghosts * m.ghostWeight * 0.35f) continue;
                BassNote g;
                g.pos = step * 0.25;
                g.len = 0.08;
                g.degree = Ghost;
                g.velocity = 30 + r.nextInt (20);
                g.micro = m.late + (r.nextFloat() * 2.0f - 1.0f) * m.looseness;
                ghosts.push_back (g);
            }
            bar.insert (bar.end(), ghosts.begin(), ghosts.end());
            std::sort (bar.begin(), bar.end(), [] (const BassNote& a, const BassNote& b) { return a.pos < b.pos; });
        }
        return bar;
    }

    /** Last beat of the phrase: a walk-up into the next downbeat. */
    void addFill (std::vector<BassNote>& bar, int style, juce::Random& r)
    {
        if (style == Drone || style == Walking) return;
        bar.erase (std::remove_if (bar.begin(), bar.end(), [] (const BassNote& n) { return n.pos >= 3.0; }), bar.end());
        if (! bar.empty() && bar.back().pos + bar.back().len > 3.0)
            bar.back().len = juce::jmax (0.1, 3.0 - bar.back().pos);
        const bool eighths = style == Trap808 || style == Drill || style == Reggae || r.nextBool();
        const int count = eighths ? 2 : 4;
        for (int k = 0; k < count; ++k)
        {
            BassNote n;
            n.pos = 3.0 + k * (eighths ? 0.5 : 0.25);
            n.len = (eighths ? 0.45 : 0.22);
            n.degree = k == count - 1 ? Approach : WalkUp;
            n.velocity = 92 + k * 6;
            bar.push_back (n);
        }
    }
}

Pattern generatePattern (const GeneratorSettings& s)
{
    juce::Random r (s.seed);
    Pattern p;
    p.bars = juce::jlimit (1, 4, s.bars);

    const auto a = makeBar (s, r, nullptr, 1.0f);
    const auto b = makeBar (s, r, &a, 0.35f);
    std::vector<std::vector<BassNote>> bars;
    if (p.bars == 1)      bars = { a };
    else if (p.bars == 2) bars = { a, b };
    else                  bars = { a, b, a, makeBar (s, r, &a, 0.45f) };

    if (r.nextFloat() < s.fills)
        addFill (bars.back(), s.style, r);

    for (int i = 0; i < (int) bars.size(); ++i)
        for (auto n : bars[(size_t) i])
        {
            n.pos += i * 4.0;
            p.notes.push_back (n);
        }
    return p;
}

//==============================================================================
Chord analyseChord (const std::vector<int>& held)
{
    Chord c;
    if (held.empty()) return c;
    int lowest = 128;
    std::array<bool, 12> pcs {};
    for (int n : held)
    {
        pcs[(size_t) (n % 12)] = true;
        lowest = juce::jmin (lowest, n);
    }

    // Best root: the pitch class with the most chord-like intervals above it
    float best = -1.0f;
    for (int r = 0; r < 12; ++r)
    {
        if (! pcs[(size_t) r]) continue;
        auto has = [&] (int iv) { return pcs[(size_t) ((r + iv) % 12)]; };
        float score = 0.0f;
        if (has (4) || has (3)) score += 2.0f;
        if (has (7)) score += 2.0f;
        if (has (10) || has (11)) score += 1.0f;
        if (has (6) && has (3) && ! has (7)) score += 1.5f;   // diminished
        if (r == lowest % 12) score += 1.2f;                    // the bass note wins ties
        if (score > best) { best = score; c.rootPc = r; }
    }

    auto has = [&] (int iv) { return pcs[(size_t) ((c.rootPc + iv) % 12)]; };
    c.valid = true;
    c.hasThird = has (4) || has (3);
    c.third = has (4) ? 4 : (has (3) ? 3 : (has (5) ? 5 : (has (2) ? 2 : 4)));
    c.fifth = has (7) ? 7 : (has (6) && c.third == 3 ? 6 : (has (8) && c.third == 4 ? 8 : 7));
    c.seventh = has (10) ? 10 : (has (11) ? 11 : (has (9) && c.fifth == 6 ? 9 : -1));
    return c;
}

int resolveDegree (int degree, int octave, const Chord& chord, int lowNote, int prev)
{
    const int base = lowNote + ((chord.rootPc - lowNote % 12) + 12) % 12;
    const int tones[] = { 0, chord.third, chord.fifth, chord.seventh > 0 ? chord.seventh : 12 };
    int note = base;

    auto chordToneNear = [&] (int from, bool up)
    {
        // Next chord tone strictly above / below 'from', searching three octaves
        int bestNote = up ? base + 24 : base - 12;
        for (int o = -1; o <= 2; ++o)
            for (int t : tones)
            {
                const int n = base + 12 * o + t;
                if (up && n > from && n < bestNote) bestNote = n;
                if (! up && n < from && n > bestNote) bestNote = n;
            }
        return bestNote;
    };

    switch (degree)
    {
        case Root:     note = base; break;
        case Fifth:    note = base + chord.fifth; break;
        case Octave:   note = base + 12; break;
        case Third:    note = base + chord.third; break;
        case Seventh:  note = base + (chord.seventh > 0 ? chord.seventh : 10); break;
        case Approach: note = (prev >= 0 && prev > base + 6) ? base + 13 : base - 1; break;   // half step into the root
        case WalkUp:   note = prev < 0 ? base : chordToneNear (prev, true); break;
        case WalkDown: note = prev < 0 ? base + 12 : chordToneNear (prev, false); break;
        case Ghost:    note = prev < 0 ? base : prev; break;
        default: break;
    }
    // Walks stay within an octave and a half above the low note
    if (degree == WalkUp || degree == WalkDown)
    {
        while (note > lowNote + 19) note -= 12;
        while (note < lowNote - 2) note += 12;
    }
    return juce::jlimit (0, 127, note + 12 * octave);
}

} // namespace basscat

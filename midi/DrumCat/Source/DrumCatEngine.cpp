#include "DrumCatEngine.h"
#include <algorithm>
#include <cmath>

namespace drumcat
{
juce::StringArray getLaneNames()  { return { "Kick", "Snare", "Rim", "Hat", "Open Hat", "Ride", "Crash", "Tom", "Perc" }; }
juce::StringArray getStyleNames() { return { "Boom Bap", "Dilla", "Neo Soul", "Lo-Fi", "Funky", "Trap", "Detroit",
                                             "House", "Drill", "Dembow", "Afrobeats", "Jungle", "Rock" }; }
juce::StringArray getMapNames()   { return { "General MIDI", "Sequential C1", "Sequential C2" }; }
juce::StringArray getGridNames()  { return { "1/8", "1/16", "1/32" }; }

double gridToPpq (int index)
{
    static const double values[] = { 0.5, 0.25, 0.125 };
    return values[juce::jlimit (0, 2, index)];
}

int noteFor (int map, int lane, int variant)
{
    variant = juce::jlimit (0, 2, variant);
    if (map == 0)
    {
        // General MIDI drum map (also what Groove Agent SE / most kits use)
        static const int gm[] = { 36, 38, 37, 42, 46, 51, 49, 47, 70 };
        if (lane == Tom) { static const int toms[] = { 45, 47, 50 }; return toms[variant]; }
        return gm[juce::jlimit (0, NumLanes - 1, lane)];
    }

    // Sequential pads from C1 (MIDI 36, Cubase naming) or C2; toms take three pads
    const int base = map == 1 ? 36 : 48;
    if (lane < Tom) return base + lane;
    if (lane == Tom) return base + Tom + variant;
    return base + Tom + 3;
}

//==============================================================================
void Pattern::sort()
{
    std::stable_sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b)
    {
        if (std::abs (a.pos - b.pos) > 1.0e-9) return a.pos < b.pos;
        return a.lane < b.lane;
    });
}

juce::ValueTree Pattern::toValueTree() const
{
    juce::ValueTree t (treeId);
    t.setProperty ("bars", bars, nullptr);
    for (const auto& h : hits)
    {
        juce::ValueTree c ("HIT");
        c.setProperty ("l", h.lane, nullptr);
        c.setProperty ("p", h.pos, nullptr);
        c.setProperty ("m", h.micro, nullptr);
        c.setProperty ("v", h.velocity, nullptr);
        c.setProperty ("x", h.variant, nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

Pattern Pattern::fromValueTree (const juce::ValueTree& t)
{
    Pattern p;
    if (! t.isValid()) return p;
    p.bars = juce::jlimit (1, 8, (int) t.getProperty ("bars", 2));
    for (const auto& c : t)
    {
        Hit h;
        h.lane = juce::jlimit (0, NumLanes - 1, (int) c.getProperty ("l", 0));
        h.pos = juce::jlimit (0.0, p.lengthPpq(), (double) c.getProperty ("p", 0.0));
        h.micro = juce::jlimit (-0.25, 0.25, (double) c.getProperty ("m", 0.0));
        h.velocity = juce::jlimit (1, 127, (int) c.getProperty ("v", 100));
        h.variant = juce::jlimit (0, 2, (int) c.getProperty ("x", 0));
        p.hits.push_back (h);
    }
    p.sort();
    return p;
}

//==============================================================================
namespace
{
    enum HatMode { Eighths = 0, Sixteenths, TrapRolls, Sparse };

    struct StyleModel
    {
        std::array<float, 16> kick;     // chance of a kick on each 16th of the bar
        std::array<int, 2> backbeat;    // snare steps
        float ghost;                    // ghost-snare chance on weak 16ths
        int hatMode;
        float hatSkip;                  // chance a hat is left out
        float openHat;                  // open hat on the "and" of 2 / 4
        float rimBackbeat;              // chance the whole pattern uses rim clicks instead of the snare
        float rimClicks;                // extra rim accents
        float perc;                     // shaker / tambourine
        float ride;                     // chance the pattern rides instead of hats
        float crash;                    // crash on the first downbeat
        // Feel, in quarter notes
        float kickLoose;                // random kick placement (+-)
        float snareLate;                // backbeat sits behind the beat
        float hatLoose;                 // random hat placement (+-)
        float swing16;                  // delay of off-beat 16ths, as a fraction of a 16th
        float velocityScale;
    };

    StyleModel modelFor (int style)
    {
        switch (style)
        {
            case 1: // Dilla: drunk, unquantised, late snare, floating 16th hats
                return { { 0.95f, 0, 0.15f, 0.4f, 0, 0, 0.2f, 0.35f, 0.25f, 0.35f, 0.3f, 0.25f, 0, 0.15f, 0.35f, 0.2f },
                         { 4, 12 }, 0.14f, Sixteenths, 0.25f, 0.15f, 0.1f, 0.08f, 0.05f, 0.0f, 0.05f,
                         0.03f, 0.04f, 0.035f, 0.45f, 0.95f };
            case 2: // Neo Soul: ghost notes, rim clicks, laid-back 16ths
                return { { 0.95f, 0, 0, 0.2f, 0, 0, 0.3f, 0.25f, 0.1f, 0.3f, 0.15f, 0.1f, 0, 0, 0.35f, 0.1f },
                         { 4, 12 }, 0.2f, Sixteenths, 0.15f, 0.2f, 0.4f, 0.35f, 0.1f, 0.25f, 0.05f,
                         0.015f, 0.025f, 0.02f, 0.35f, 0.9f };
            case 3: // Lo-Fi: sparse, soft, lazy, shaker
                return { { 0.95f, 0, 0.1f, 0.1f, 0, 0, 0.15f, 0.4f, 0.15f, 0.3f, 0.1f, 0, 0, 0, 0.25f, 0.05f },
                         { 4, 12 }, 0.06f, Sparse, 0.2f, 0.1f, 0.3f, 0.05f, 0.35f, 0.0f, 0.0f,
                         0.02f, 0.03f, 0.03f, 0.4f, 0.82f };
            case 4: // Funky: busy syncopated kicks, lots of ghosts, open hats
                return { { 0.97f, 0, 0.35f, 0.2f, 0, 0.1f, 0.35f, 0.3f, 0.3f, 0.2f, 0.45f, 0.2f, 0, 0.15f, 0.3f, 0.25f },
                         { 4, 12 }, 0.3f, Sixteenths, 0.05f, 0.35f, 0.0f, 0.05f, 0.15f, 0.0f, 0.2f,
                         0.008f, 0.008f, 0.01f, 0.15f, 1.0f };
            case 5: // Trap: half-time snare, rolling hats, on the grid
                return { { 0.97f, 0, 0, 0.3f, 0, 0, 0.25f, 0.35f, 0, 0.2f, 0.3f, 0.2f, 0, 0.15f, 0.2f, 0.1f },
                         { 8, 8 }, 0.03f, TrapRolls, 0.08f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f,
                         0.0f, 0.0f, 0.002f, 0.0f, 1.0f };
            case 6: // Detroit: heavy swung 16ths, kick-driven, loose
                return { { 0.97f, 0, 0.2f, 0.3f, 0, 0.15f, 0.2f, 0.4f, 0.2f, 0.4f, 0.2f, 0.2f, 0, 0.2f, 0.3f, 0.2f },
                         { 4, 12 }, 0.1f, Sixteenths, 0.1f, 0.3f, 0.05f, 0.05f, 0.1f, 0.0f, 0.1f,
                         0.02f, 0.02f, 0.025f, 0.5f, 1.0f };
            case 7: // House: four on the floor, claps on 2 and 4, open hats on the off-beats
                return { { 0.97f, 0, 0, 0, 0.97f, 0, 0, 0, 0.97f, 0, 0, 0, 0.97f, 0, 0.1f, 0 },
                         { 4, 12 }, 0.02f, Sixteenths, 0.3f, 0.85f, 0.0f, 0.05f, 0.3f, 0.0f, 0.1f,
                         0.0f, 0.0f, 0.004f, 0.1f, 1.0f };
            case 8: // Drill: half-time, sliding syncopated kicks, rolling hats
                return { { 0.97f, 0, 0, 0.3f, 0, 0, 0.45f, 0, 0, 0.3f, 0.4f, 0, 0, 0.35f, 0.2f, 0 },
                         { 8, 8 }, 0.14f, TrapRolls, 0.12f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 0.05f,
                         0.0f, 0.0f, 0.003f, 0.12f, 1.0f };
            case 9: // Dembow: kick on every beat, the snare answers in between
                return { { 0.97f, 0, 0, 0, 0.97f, 0, 0, 0, 0.97f, 0, 0, 0, 0.97f, 0, 0, 0 },
                         { 6, 14 }, 0.4f, Eighths, 0.1f, 0.1f, 0.0f, 0.15f, 0.45f, 0.0f, 0.05f,
                         0.0f, 0.004f, 0.006f, 0.05f, 1.0f };
            case 10: // Afrobeats: bouncing kicks, rim clicks, shakers, swung 16ths
                return { { 0.97f, 0, 0, 0.5f, 0, 0, 0.6f, 0, 0.6f, 0, 0.3f, 0, 0, 0, 0.5f, 0 },
                         { 4, 12 }, 0.08f, Sixteenths, 0.25f, 0.15f, 0.6f, 0.4f, 0.6f, 0.0f, 0.05f,
                         0.005f, 0.008f, 0.012f, 0.3f, 0.95f };
            case 11: // Jungle: two-step kick, busy ghost snares, fast 16ths
                return { { 0.97f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.97f, 0, 0, 0, 0, 0.15f },
                         { 4, 12 }, 0.35f, Sixteenths, 0.1f, 0.15f, 0.0f, 0.05f, 0.1f, 0.15f, 0.2f,
                         0.004f, 0.004f, 0.006f, 0.05f, 1.0f };
            case 12: // Rock: straight 8ths, big backbeat, crashes
                return { { 0.97f, 0, 0, 0, 0, 0, 0, 0.3f, 0.97f, 0, 0.4f, 0, 0, 0, 0, 0.2f },
                         { 4, 12 }, 0.03f, Eighths, 0.02f, 0.1f, 0.0f, 0.0f, 0.0f, 0.3f, 0.5f,
                         0.004f, 0.004f, 0.004f, 0.0f, 1.0f };
            case 0: // Boom Bap: hard kick/snare, swung 8th hats, pickup kicks
            default:
                return { { 0.97f, 0, 0.25f, 0.15f, 0, 0, 0.1f, 0.55f, 0.2f, 0.45f, 0.35f, 0.1f, 0, 0, 0.3f, 0.15f },
                         { 4, 12 }, 0.08f, Eighths, 0.05f, 0.25f, 0.0f, 0.0f, 0.05f, 0.0f, 0.15f,
                         0.01f, 0.012f, 0.015f, 0.2f, 1.0f };
        }
    }

    struct Gen
    {
        juce::Random rng;
        StyleModel m;
        float density;

        float chance (float p) const { return juce::jlimit (0.0f, 0.97f, p * (0.4f + 1.2f * density)); }
        bool roll (float p) { return rng.nextFloat() < p; }
        float bipolar() { return rng.nextFloat() * 2.0f - 1.0f; }
        int vel (int base, int spread) { return juce::jlimit (1, 127, juce::roundToInt ((float) (base + bipolar() * (float) spread) * m.velocityScale)); }

        /** Hand-played timing for a hit on 16th step s (in quarter notes). */
        double feel (int lane, int step, bool backbeat)
        {
            double t = 0.0;
            if (step % 2 == 1) t += m.swing16 * 0.125 * (lane == Hat || lane == OpenHat || lane == Ride || lane == Perc ? 1.0 : 0.7);
            switch (lane)
            {
                case Kick:  t += bipolar() * m.kickLoose; break;
                case Snare:
                case Rim:   t += backbeat ? m.snareLate * (0.6 + 0.4 * rng.nextFloat()) : rng.nextFloat() * m.hatLoose; break;
                default:    t += bipolar() * m.hatLoose; break;
            }
            return juce::jlimit (-0.12, 0.2, t);
        }
    };

    /** One bar of hits at bar-relative positions. */
    std::vector<Hit> buildBar (Gen& g, bool useRim, bool useRide)
    {
        std::vector<Hit> bar;
        const auto& m = g.m;
        auto add = [&bar, &g] (int lane, int step, int velocity, bool backbeat = false, int variant = 0, double extra = 0.0)
        {
            bar.push_back ({ lane, step * 0.25 + extra, g.feel (lane, step, backbeat), velocity, variant });
        };
        auto isBackbeat = [&m] (int s) { return s == m.backbeat[0] || s == m.backbeat[1]; };

        // Kick: anchors stay, optional kicks scale with density; rarely on top of the backbeat
        for (int s = 0; s < 16; ++s)
        {
            const float p = m.kick[(size_t) s] > 0.9f ? 1.0f : g.chance (m.kick[(size_t) s]);   // anchors always play
            if (isBackbeat (s) && g.roll (0.85f)) continue;
            if (g.roll (p)) add (Kick, s, g.vel (s % 4 == 0 ? 112 : 96, 10));
        }

        // Backbeat
        for (int k = 0; k < 2; ++k)
        {
            if (k == 1 && m.backbeat[1] == m.backbeat[0]) break;   // half-time styles have a single backbeat
            add (useRim ? Rim : Snare, m.backbeat[(size_t) k], g.vel (useRim ? 100 : 112, 8), true);
        }

        // Ghost snares on weak 16ths (never right on top of the backbeat)
        for (int s : { 1, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15 })
            if (! isBackbeat (s) && g.roll (g.chance (m.ghost)))
                add (Snare, s, g.vel (38, 10));

        // Rim accents
        for (int s : { 3, 7, 10, 13 })
            if (g.roll (m.rimClicks * 0.5f)) add (Rim, s, g.vel (70, 10));

        // Hats / ride
        const int hatLane = useRide ? Ride : Hat;
        const int openStep = g.roll (0.5f) ? 14 : 6;
        const bool open = g.roll (m.openHat);
        auto hatVel = [&g] (int s) { return s % 4 == 0 ? g.vel (96, 8) : (s % 2 == 0 ? g.vel (74, 10) : g.vel (52, 12)); };

        for (int s = 0; s < 16; ++s)
        {
            if (open && s == openStep) { add (OpenHat, s, g.vel (88, 8)); continue; }
            if (open && s == openStep + 1) continue; // the open hat rings over the next 16th

            switch (m.hatMode)
            {
                case Eighths:
                    if (s % 2 == 0 && ! g.roll (m.hatSkip)) add (hatLane, s, hatVel (s));
                    else if (s % 2 == 1 && g.roll (g.chance (0.08f))) add (hatLane, s, g.vel (50, 10));
                    break;

                case Sixteenths:
                    if (s % 2 == 0 ? ! g.roll (m.hatSkip) : g.roll (g.chance (0.65f) - m.hatSkip))
                        add (hatLane, s, hatVel (s));
                    break;

                case TrapRolls:
                {
                    if (g.roll (m.hatSkip)) break;
                    // Every so often the second half of a beat becomes a 32nd or triplet roll
                    if (s % 4 == 2 && g.roll (g.chance (0.3f)))
                    {
                        const bool triplet = g.roll (0.5f);
                        const int n = triplet ? 3 : 4;
                        for (int k = 0; k < n; ++k)
                            add (Hat, s, juce::jlimit (1, 127, 55 + k * 12 + (int) (g.bipolar() * 6.0f)), false, 0,
                                 k * (triplet ? 0.5 / 3.0 : 0.125));
                        ++s; // the roll covers the next 16th as well
                        break;
                    }
                    add (Hat, s, hatVel (s));
                    break;
                }

                case Sparse:
                default:
                    if (s % 2 == 0 && ! g.roll (m.hatSkip + (s % 4 == 2 ? 0.0f : 0.25f))) add (hatLane, s, g.vel (64, 12));
                    break;
            }
        }

        // Shaker / tambourine
        for (int s = 0; s < 16; ++s)
            if (g.roll (m.perc * (s % 2 == 0 ? 0.9f : 0.5f))) add (Perc, s, s % 4 == 2 ? g.vel (70, 10) : g.vel (48, 10));

        return bar;
    }

    /** Small drummer-like changes for the next bar: move / drop / add a kick, reshuffle ghosts. */
    std::vector<Hit> vary (Gen& g, std::vector<Hit> bar)
    {
        const auto& m = g.m;
        for (auto& h : bar)
        {
            h.micro = juce::jlimit (-0.12, 0.2, h.micro + g.bipolar() * 0.005);
            h.velocity = juce::jlimit (1, 127, h.velocity + (int) (g.bipolar() * 5.0f));
        }

        bar.erase (std::remove_if (bar.begin(), bar.end(), [&g] (const Hit& h)
        {
            const int s = (int) std::round (h.pos * 4.0);
            return (h.lane == Kick && s != 0 && g.roll (0.15f)) || (h.lane == Snare && h.velocity < 60 && g.roll (0.3f));
        }), bar.end());

        for (int k = 0; k < 2; ++k)
        {
            const int s = 1 + g.rng.nextInt (15);
            if (s != m.backbeat[0] && s != m.backbeat[1] && g.roll (g.chance (m.kick[(size_t) s] + 0.1f)))
                bar.push_back ({ Kick, s * 0.25, g.feel (Kick, s, false), g.vel (95, 10), 0 });
        }
        return bar;
    }

    void addFill (Gen& g, std::vector<Hit>& bar)
    {
        // Last beat (or two) becomes a snare / tom run with a crescendo
        const int start = g.roll (0.35f) ? 8 : 12;
        bar.erase (std::remove_if (bar.begin(), bar.end(), [start] (const Hit& h)
        {
            return h.pos >= start * 0.25 - 1.0e-9 && (h.lane == Snare || h.lane == Rim || h.lane == Hat || h.lane == OpenHat || h.lane == Ride);
        }), bar.end());

        const bool toms = g.roll (0.6f);
        for (int s = start; s < 16; ++s)
        {
            if (s % 2 == 1 && g.roll (0.35f)) continue;
            const float progress = (float) (s - start) / (float) (16 - start);
            const int lane = toms && s >= start + 2 ? Tom : Snare;
            const int variant = 2 - juce::jlimit (0, 2, (int) (progress * 3.0f));    // high -> low toms
            bar.push_back ({ lane, s * 0.25, g.feel (lane, s, false), g.vel (70 + (int) (progress * 45.0f), 6), variant });
        }
    }
}

Pattern generatePattern (const GeneratorSettings& settings)
{
    Gen g { juce::Random (settings.seed), modelFor (settings.style), juce::jlimit (0.0f, 1.0f, settings.density) };

    Pattern p;
    p.bars = juce::jlimit (1, 8, settings.bars);

    const bool useRim = g.roll (g.m.rimBackbeat);
    const bool useRide = g.roll (g.m.ride);

    const auto barA = buildBar (g, useRim, useRide);
    const auto barB = vary (g, barA);

    for (int b = 0; b < p.bars; ++b)
    {
        // A B A B' ... : repetition with small changes, like a drummer
        auto bar = b % 2 == 0 ? (b == 0 ? barA : vary (g, barA)) : (b == 1 ? barB : vary (g, barB));
        if (b == p.bars - 1 && g.roll (settings.fills))
            addFill (g, bar);
        if (b == 0 && g.roll (g.m.crash))
            bar.push_back ({ Crash, 0.0, 0.0, g.vel (100, 8), 0 });

        for (auto h : bar)
        {
            h.pos += b * 4.0;
            p.hits.push_back (h);
        }
    }

    p.sort();
    return p;
}

StyleHints getStyleHints (int style)
{
    switch (style)
    {
        case 1:  return { 0.0f, 6.0f };   // Dilla: the looseness is in the pattern, plus a little more
        case 3:  return { 0.0f, 5.0f };
        case 5:
        case 8:  return { 0.0f, 0.0f };
        case 7:
        case 9:
        case 12: return { 0.0f, 1.5f };
        case 10: return { 0.0f, 4.0f };
        default: return { 0.0f, 3.0f };
    }
}

} // namespace drumcat

#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <vector>

namespace drumcat
{
enum Lane { Kick = 0, Snare, Rim, Hat, OpenHat, Ride, Crash, Tom, Perc, NumLanes };

juce::StringArray getLaneNames();
juce::StringArray getStyleNames();          // Boom Bap, Dilla, Neo Soul, Lo-Fi, Funky, Trap, Detroit
juce::StringArray getMapNames();            // General MIDI, Sequential C1, Sequential C2
juce::StringArray getGridNames();           // 1/8, 1/16, 1/32
double gridToPpq (int index);

/** MIDI note for a lane (variant picks low / mid / high tom). */
int noteFor (int map, int lane, int variant);

/** One drum hit. pos is the nominal grid position, micro the hand-played offset (both in quarter notes). */
struct Hit
{
    int lane = Kick;
    double pos = 0.0;
    double micro = 0.0;
    int velocity = 100;
    int variant = 0;

    bool operator== (const Hit& o) const
    {
        return lane == o.lane && std::abs (pos - o.pos) < 1.0e-9 && std::abs (micro - o.micro) < 1.0e-9
            && velocity == o.velocity && variant == o.variant;
    }
};

struct Pattern
{
    int bars = 2;
    std::vector<Hit> hits;

    double lengthPpq() const { return bars * 4.0; }
    void sort();
    juce::ValueTree toValueTree() const;
    static Pattern fromValueTree (const juce::ValueTree&);
    static inline const juce::Identifier treeId { "DRUMPATTERN" };
};

struct GeneratorSettings
{
    int style = 0;
    int bars = 2;
    float density = 0.5f;   // 0..1: how busy the optional parts are
    float fills = 0.35f;    // 0..1: chance of a fill at the end of the pattern
    int64_t seed = 1;
};

/**
    Hand-built style models for golden-era hip-hop and related grooves.

    Each style knows where its kicks like to fall around the backbeat, how its hats move
    (8ths, 16ths, rides, trap rolls), how many ghost notes it uses and - importantly - how
    "late" or loose it is played: Dilla-style patterns drag the snare and float the hats,
    Boom Bap pushes hats with a light swing, Trap stays on the grid. A one-bar core is built
    first and then varied for the following bars, so patterns repeat like a drummer would,
    with small changes and an optional fill at the end.
*/
Pattern generatePattern (const GeneratorSettings& settings);

/** Musical defaults the style suggests (applied when the style is picked). */
struct StyleHints { float swing; float human; };
StyleHints getStyleHints (int style);

} // namespace drumcat

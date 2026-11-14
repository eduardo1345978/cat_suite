#include "PatternPlayer.h"

namespace chordcat
{
namespace
{
    /** Pattern from a 16-step string over one bar: 'X' accent, 'x' normal, 'o' soft, '-' tie, '.' rest. */
    RhythmPattern fromString (const juce::String& name, const juce::String& steps, double stepBeats = 0.25, double gate = 0.9)
    {
        RhythmPattern p { name, steps.length() * stepBeats, {} };
        for (int i = 0; i < steps.length(); ++i)
        {
            const auto c = steps[i];
            if (c != 'X' && c != 'x' && c != 'o')
                continue;

            int ties = 0;
            while (i + 1 + ties < steps.length() && steps[i + 1 + ties] == '-')
                ++ties;

            const float vel = c == 'X' ? 1.15f : (c == 'x' ? 0.95f : 0.7f);
            p.hits.push_back ({ i * stepBeats, (1 + ties) * stepBeats * gate, vel });
        }
        return p;
    }
}

const std::vector<RhythmPattern>& getRhythmPatterns()
{
    static const std::vector<RhythmPattern> patterns = {
        { "Hold", 4.0, {} },
        fromString ("Whole Note",       "X---------------"),
        fromString ("Half Notes",       "X-------x-------"),
        fromString ("Quarter Stabs",    "X...x...x...x...", 0.25, 0.6),
        fromString ("8th Pulse",        "X.x.x.x.X.x.x.x.", 0.25, 0.8),
        fromString ("16th Pulse",       "XoxoXoxoXoxoXoxo", 0.25, 0.7),
        fromString ("Off-Beat 8ths",    "..x...x...x...x."),
        fromString ("Charleston",       "X-----x---------"),
        fromString ("Dotted Push",      "X-----x-----x---"),
        fromString ("Neo-Soul Push",    "X--.....o.x-----"),
        fromString ("Lo-Fi Lazy",       "X-------..x-----"),
        fromString ("Syncopated",       "X..x..x...x..x.."),
        fromString ("Clave 3-2",        "X..x..x...x.x..."),
        fromString ("Bossa Comp",       "X..x..x..x..x..."),
        fromString ("House Stab",       "..X...X...X...X.", 0.25, 0.4),
        fromString ("Gospel Pump",      "X.ox.oX.ox.oX.ox"),
        fromString ("Ballad Arp Feel",  "X---o---x---o---"),
        fromString ("Skank",            "..x...x...x...x.", 0.25, 0.3),
        fromString ("Tresillo",         "X--x--x-X--x--x-"),
        fromString ("Two Bar Hold",     "X-------------------------------"),
    };
    return patterns;
}

juce::StringArray getRhythmPatternNames()
{
    juce::StringArray names;
    for (const auto& p : getRhythmPatterns())
        names.add (p.name);
    return names;
}

juce::StringArray getGrooveNames()
{
    return { "Off (use Swing)", "MPC 60%", "SP1200", "Straight" };
}

PatternPlayer::GrooveSettings PatternPlayer::makeGroove (Groove groove, int swingPercent)
{
    GrooveSettings g;
    g.accents.fill (1.0f);
    g.swing = juce::jlimit (50, 75, swingPercent) / 100.0;

    switch (groove)
    {
        case Groove::MPC60:
            g.swing = 0.60;
            g.accents = { 1.0f, 0.78f, 0.9f, 0.8f, 1.0f, 0.78f, 0.9f, 0.82f, 1.0f, 0.78f, 0.9f, 0.8f, 1.0f, 0.78f, 0.92f, 0.85f };
            break;
        case Groove::SP1200:
            g.swing = 0.58;
            g.accents = { 1.05f, 0.72f, 0.88f, 0.75f, 0.98f, 0.72f, 0.88f, 0.78f, 1.05f, 0.72f, 0.88f, 0.75f, 0.98f, 0.74f, 0.9f, 0.8f };
            break;
        case Groove::Straight:
            g.swing = 0.5;
            break;
        case Groove::Off:
        default:
            break;
    }
    return g;
}

double PatternPlayer::nextHitAfter (int patternIndex, double ppq, const GrooveSettings& groove)
{
    double found = -1.0;
    forEachHit (patternIndex, ppq, ppq + 8.0, groove, [&found] (double t, double, float)
    {
        if (found < 0.0 || t < found) found = t;
    });
    return found;
}

} // namespace chordcat

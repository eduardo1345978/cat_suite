#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <vector>

namespace chordcat
{
struct RhythmHit
{
    double startBeat;    // within the pattern loop
    double lengthBeats;
    float velocity;      // multiplier
};

struct RhythmPattern
{
    juce::String name;
    double lengthBeats;
    std::vector<RhythmHit> hits;
};

/** Index 0 is "Hold" (chord sustains while the key is held). */
const std::vector<RhythmPattern>& getRhythmPatterns();
juce::StringArray getRhythmPatternNames();

enum class Groove { Off = 0, MPC60, SP1200, Straight };
juce::StringArray getGrooveNames();

/**
    Internal per-chord rhythm for Chord-Cat. Hits are locked to the host grid
    (ppq 0 = song start) and can be swung on the 16th grid. Groove templates
    override the swing amount and add per-16th velocity accents.
*/
class PatternPlayer
{
public:
    struct GrooveSettings
    {
        double swing = 0.5;                  // 0.5 .. 0.75 (16th-note swing ratio)
        std::array<float, 16> accents {};    // per 16th within the bar
    };

    static GrooveSettings makeGroove (Groove groove, int swingPercent);

    /**
        Calls fn (hitPpq, lengthPpq, velocityMultiplier) for every hit whose (swung)
        start lies in [ppqFrom, ppqTo).
    */
    template <typename Fn>
    static void forEachHit (int patternIndex, double ppqFrom, double ppqTo, const GrooveSettings& groove, Fn&& fn)
    {
        const auto& patterns = getRhythmPatterns();
        if (patternIndex <= 0 || patternIndex >= (int) patterns.size() || ppqTo <= ppqFrom)
            return;

        const auto& p = patterns[(size_t) patternIndex];
        const double L = p.lengthBeats;
        const double firstLoop = std::floor ((ppqFrom - 1.0) / L);
        const double lastLoop = std::floor (ppqTo / L);

        for (double loop = firstLoop; loop <= lastLoop; loop += 1.0)
        {
            for (const auto& h : p.hits)
            {
                const double straight = loop * L + h.startBeat;
                const double t = straight + swingOffset (straight, groove.swing);
                if (t >= ppqFrom && t < ppqTo)
                {
                    const double inBar = std::fmod (std::fmod (straight, 4.0) + 4.0, 4.0);
                    const int sixteenth = juce::jlimit (0, 15, (int) std::floor (inBar * 4.0 + 1.0e-6));
                    fn (t, h.lengthBeats, h.velocity * groove.accents[(size_t) sixteenth]);
                }
            }
        }
    }

    static double swingOffset (double ppq, double swing)
    {
        // Off-beat 16ths (x.25, x.75) move later: at 0.5 straight, at 0.75 hard swing.
        const double inPair = std::fmod (std::fmod (ppq, 0.5) + 0.5, 0.5);
        if (std::abs (inPair - 0.25) < 1.0e-6)
            return (swing - 0.5) * 0.5;
        return 0.0;
    }

    /** The next hit start at or after ppq, or -1 if the pattern is empty / Hold. */
    static double nextHitAfter (int patternIndex, double ppq, const GrooveSettings& groove);
};

} // namespace chordcat

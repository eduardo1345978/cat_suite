#include "StrumEngine.h"
#include <numeric>

namespace chordcat
{
juce::StringArray getStrumDirectionNames()
{
    return { "Up", "Down", "Up / Down", "Down / Up", "Random", "Root First", "Low then High", "Outside In", "Inside Out", "Guitar" };
}

juce::StringArray getStrumShapeNames()
{
    return { "Linear", "Accelerate", "Decelerate", "S-Curve", "Reverse S", "Fast Rake", "Slow Roll", "Elastic",
             "Bounce", "Reverse Bounce", "Steps 2", "Steps 3", "Steps 4", "Swing Pairs", "Exponential", "Random Gaps" };
}

juce::StringArray getStrumBurstNames() { return { "Off", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; }

double strumBurstToPpq (int index)
{
    static const double v[] = { 0.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return v[juce::jlimit (0, 6, index)];
}

float strumShapeTiming (int shape, int k, int n, juce::Random& rng)
{
    if (n <= 1) return 0.0f;
    const float p = (float) k / (float) (n - 1);
    auto smooth = [] (float x) { return x * x * (3.0f - 2.0f * x); };
    auto steps = [&] (int groups)
    {
        // Notes come in clusters: each group is quick, with a gap before the next group
        const int g = juce::jmin (groups - 1, k * groups / n);
        const float within = (float) (k - g * n / groups) / (float) juce::jmax (1, n / groups);
        return juce::jlimit (0.0f, 1.0f, ((float) g + 0.18f * within) / (float) juce::jmax (1, groups - 1));
    };
    auto gaps = [&] (float ratio)
    {
        // Geometric gaps: ratio < 1 = each gap shorter (bouncing ball), > 1 = longer
        double t = 0.0, total = 0.0, g = 1.0;
        for (int i = 0; i < n - 1; ++i) { total += g; if (i < k) t += g; g *= ratio; }
        return (float) (t / total);
    };

    switch (shape)
    {
        case 1:  return p * p;                                                // accelerate: slow start
        case 2:  return 1.0f - (1.0f - p) * (1.0f - p);                       // decelerate
        case 3:  return smooth (p);
        case 4:  return juce::jlimit (0.0f, 1.0f, 2.0f * p - smooth (p));     // reverse S: fast middle ends
        case 5:  return p * p * p;                                            // fast rake: bunched at the end
        case 6:  return 1.0f - std::pow (1.0f - p, 3.0f);                     // slow roll: bunched at the start
        case 7:  return juce::jlimit (0.0f, 1.0f, p + 0.18f * std::sin (p * juce::MathConstants<float>::twoPi * 1.5f) * (1.0f - p));
        case 8:  return gaps (0.68f);
        case 9:  return gaps (1.45f);
        case 10: return steps (2);
        case 11: return steps (3);
        case 12: return steps (4);
        case 13: { const float pair = (float) (k / 2) * 2.0f / (float) juce::jmax (1, n - 1);    // long-short pairs
                   return juce::jlimit (0.0f, 1.0f, k % 2 == 0 ? pair : pair + 0.3f / (float) juce::jmax (1, n - 1)); }
        case 14: return (std::pow (8.0f, p) - 1.0f) / 7.0f;
        case 15: return juce::jlimit (0.0f, 1.0f, p + (rng.nextFloat() - 0.5f) * 0.6f / (float) n);
        case 0:
        default: return p;
    }
}

std::vector<StrumEvent> buildStrum (const std::vector<int>& pitches, const StrumSettings& s, float velocity,
                                    int counter, int rootPc, juce::Random& rng)
{
    std::vector<StrumEvent> events;
    const int n = (int) pitches.size();
    if (n == 0) return events;

    // Base order: ascending pitch
    std::vector<int> up ((size_t) n);
    std::iota (up.begin(), up.end(), 0);
    std::sort (up.begin(), up.end(), [&] (int a, int b) { return pitches[(size_t) a] < pitches[(size_t) b]; });

    std::vector<int> order;
    switch (s.direction)
    {
        case 1: order.assign (up.rbegin(), up.rend()); break;
        case 2: order = up; if (counter % 2 == 1) std::reverse (order.begin(), order.end()); break;
        case 3: order = up; if (counter % 2 == 0) std::reverse (order.begin(), order.end()); break;
        case 4:
            order = up;
            for (int i = n - 1; i > 0; --i) std::swap (order[(size_t) i], order[(size_t) rng.nextInt (i + 1)]);
            break;
        case 5:
        {
            // The chord root first (lowest one if doubled), then the rest upwards
            int root = up[0];
            if (rootPc >= 0)
                for (int i : up) if (((pitches[(size_t) i] % 12) + 12) % 12 == rootPc) { root = i; break; }
            order.push_back (root);
            for (int i : up) if (i != root) order.push_back (i);
            break;
        }
        case 6:
        {
            // Left hand then right hand: the low half rolls up, then the high half
            const int half = (n + 1) / 2;
            for (int i = 0; i < half; ++i) order.push_back (up[(size_t) i]);
            for (int i = half; i < n; ++i) order.push_back (up[(size_t) i]);
            break;
        }
        case 7:
            for (int lo = 0, hi = n - 1; lo <= hi; ++lo, --hi)
            {
                order.push_back (up[(size_t) lo]);
                if (hi != lo) order.push_back (up[(size_t) hi]);
            }
            break;
        case 8:
        {
            const int mid = (n - 1) / 2;
            order.push_back (up[(size_t) mid]);
            for (int d = 1; (int) order.size() < n; ++d)
            {
                if (mid + d < n) order.push_back (up[(size_t) (mid + d)]);
                if (mid - d >= 0) order.push_back (up[(size_t) (mid - d)]);
            }
            break;
        }
        case 9:   // guitar: down-strokes (low to high) on even strums, up-strokes on odd
            order = up;
            if (counter % 2 == 1) std::reverse (order.begin(), order.end());
            break;
        case 0:
        default: order = up; break;
    }

    const int passes = juce::jlimit (1, 16, s.repeats);
    const int win = s.window >= 2 && s.window < n ? s.window : n;
    const int slideRange = n - win + 1;
    const double noteGap = s.spreadMs / (double) juce::jmax (1, win - 1);
    double passStart = 0.0;

    for (int pass = 0; pass < passes; ++pass)
    {
        // Classic: a window of neighbouring notes that slides one step every pass / strum
        const int start = win < n ? (counter + pass) % slideRange : 0;
        std::vector<int> passOrder;
        if (win < n)
        {
            std::vector<int> sel (up.begin() + start, up.begin() + start + win);
            // keep the direction of the full order for the selected notes
            for (int i : order)
                if (std::find (sel.begin(), sel.end(), i) != sel.end()) passOrder.push_back (i);
        }
        else
            passOrder = order;
        if (s.roundTrip && pass % 2 == 1)
            std::reverse (passOrder.begin(), passOrder.end());

        const int m = (int) passOrder.size();
        for (int k = 0; k < m; ++k)
        {
            if (s.roundTrip && pass > 0 && k == 0)
                continue;   // the turning note is not struck twice
            float t = strumShapeTiming (s.shape, k, m, rng);
            if (s.curve > 0.0f)      t = std::pow (t, 1.0f + s.curve * 2.0f);
            else if (s.curve < 0.0f) t = 1.0f - std::pow (1.0f - t, 1.0f - s.curve * 2.0f);

            const float pos = m > 1 ? (float) k / (float) (m - 1) : 0.0f;
            float v = velocity * (1.0f + s.tilt * 1.6f * (pos - 0.5f) * (s.spreadMs > 0.0f ? 1.0f : 0.0f));
            if (s.target > 0)
                v = velocity + ((float) s.target - velocity) * pos;
            if (pass > 0)
                v *= 0.88f;   // repeats a touch softer than the first stroke

            events.push_back ({ passOrder[(size_t) k], passStart + s.spreadMs * t, juce::jlimit (1.0f, 127.0f, v), pass });
        }
        passStart += s.spreadMs + noteGap;
    }
    std::stable_sort (events.begin(), events.end(), [] (const StrumEvent& a, const StrumEvent& b) { return a.timeMs < b.timeMs; });
    return events;
}

//==============================================================================
const std::vector<StrumPreset>& getStrumPresets()
{
    // directions: 0 Up, 1 Down, 2 Up/Down, 3 Down/Up, 4 Random, 5 Root First, 6 Low then High, 7 Outside In, 8 Inside Out, 9 Guitar
    // shapes: 0 Linear, 1 Accel, 2 Decel, 3 S, 4 Rev S, 5 Fast Rake, 6 Slow Roll, 7 Elastic, 8 Bounce, 9 Rev Bounce, 10-12 Steps, 13 Swing, 14 Exp, 15 Random
    // burst: 0 Off, 1 1/4, 2 1/8, 3 1/8T, 4 1/16, 5 1/16T, 6 1/32
    static const std::vector<StrumPreset> list = {
        { "Off (block chords)",  { { "strumMs", 0 } } },
        { "Guitar Down",         { { "strumMs", 28 }, { "strumDir", 1 }, { "strumCurve", 15 } } },
        { "Guitar Up",           { { "strumMs", 22 }, { "strumDir", 0 }, { "strumCurve", -20 } } },
        { "Folk Strumming",      { { "strumMs", 25 }, { "strumDir", 9 }, { "strumBurst", 2 }, { "humanTime", 4 }, { "humanVel", 6 } } },
        { "Harp Glissando",      { { "strumMs", 420 }, { "strumDir", 0 }, { "strumShapeType", 2 }, { "strumStack", 1 }, { "strumCurve", 10 } } },
        { "Harp Round Trip",     { { "strumMs", 300 }, { "strumDir", 0 }, { "strumShapeType", 3 }, { "strumStack", 1 }, { "strumRepeats", 2 }, { "strumRoundTrip", 1 } } },
        { "Piano Roll",          { { "strumMs", 70 }, { "strumDir", 0 }, { "strumShapeType", 6 }, { "humanTime", 2 } } },
        { "Fast Rake",           { { "strumMs", 60 }, { "strumDir", 1 }, { "strumShapeType", 5 }, { "strumCurve", 40 } } },
        { "Rasgueado",           { { "strumMs", 30 }, { "strumDir", 9 }, { "strumRepeats", 4 }, { "strumShapeType", 8 }, { "strumCurve", 20 } } },
        { "Mandolin Tremolo",    { { "strumMs", 8 }, { "strumDir", 9 }, { "strumBurst", 6 }, { "humanVel", 8 } } },
        { "Banjo Roll",          { { "strumMs", 180 }, { "strumDir", 0 }, { "strumWindow", 3 }, { "strumRepeats", 3 } } },
        { "Orchestral Stack",    { { "strumMs", 55 }, { "strumDir", 0 }, { "strumStack", 2 }, { "strumShapeType", 3 } } },
        { "Arp Burst",           { { "strumMs", 120 }, { "strumDir", 0 }, { "strumRepeats", 2 }, { "strumRoundTrip", 1 }, { "strumShapeType", 0 } } },
        { "Random Cascade",      { { "strumMs", 160 }, { "strumDir", 4 }, { "strumShapeType", 15 }, { "humanVel", 10 } } },
        { "Root First Ballad",   { { "strumMs", 140 }, { "strumDir", 5 }, { "strumShapeType", 2 } } },
        { "Two Hands",           { { "strumMs", 110 }, { "strumDir", 6 }, { "strumShapeType", 10 } } },
        { "Outside In",          { { "strumMs", 90 }, { "strumDir", 7 }, { "strumShapeType", 3 } } },
        { "Bouncing Ball",       { { "strumMs", 350 }, { "strumDir", 0 }, { "strumShapeType", 8 }, { "strumTarget", 40 } } },
        { "Stair Steps",         { { "strumMs", 240 }, { "strumDir", 0 }, { "strumShapeType", 11 }, { "strumStack", 1 } } },
        { "Elastic Pad",         { { "strumMs", 500 }, { "strumDir", 8 }, { "strumShapeType", 7 }, { "strumTarget", 100 } } },
        { "Muted Funk",          { { "strumMs", 12 }, { "strumDir", 9 }, { "strumBurst", 4 }, { "strumMute", 45 }, { "humanTime", 2 } } },
        { "Reverse Swell",       { { "strumMs", 380 }, { "strumDir", 1 }, { "strumShapeType", 1 }, { "strumTarget", 127 } } },
        { "Ghost Strum",         { { "strumMs", 200 }, { "strumDir", 0 }, { "strumTarget", 25 }, { "strumShapeType", 2 } } },
        { "Release Sweep",       { { "strumMs", 90 }, { "strumDir", 0 }, { "strumRelease", 1 } } },
    };
    return list;
}

} // namespace chordcat

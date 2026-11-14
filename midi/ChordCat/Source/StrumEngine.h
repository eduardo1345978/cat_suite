#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/**
    Chord-Cat strum engine: turns a chord into a strummed / rolled / re-ordered burst of notes.

      direction  - which note goes first (10 modes, incl. alternating, random, root first,
                   low-then-high split, outside-in, inside-out and guitar down/up strokes)
      spread     - time from the first to the last note of one pass (5..1000 ms or synced)
      shape      - 16 timing curves (accelerating, decelerating, elastic, bouncing, stepped...)
                   plus the free Curve control on top
      window     - Classic: only N neighbouring notes per pass, sliding one step every pass/strum
      repeats    - several passes; Round Trip turns every second pass around (up-down-up...)
      tilt/target- velocity across the strum (tilt) or towards a target velocity
*/
namespace chordcat
{
juce::StringArray getStrumDirectionNames();   // 10
juce::StringArray getStrumShapeNames();       // 16
juce::StringArray getStrumBurstNames();       // Off, 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32
double strumBurstToPpq (int index);           // 0 = off

struct StrumSettings
{
    float spreadMs = 0.0f;
    int direction = 0;
    int shape = 0;
    float curve = 0.0f;       // -1..1 (the old Strum Shape control)
    float tilt = 0.0f;        // -1..1 velocity tilt across the strum
    int target = 0;           // 0 = off, else velocity of the last note
    int window = 0;           // 0 = all notes, 2..6 = sliding window size
    int repeats = 1;          // passes
    bool roundTrip = false;
};

struct StrumEvent
{
    int index = 0;            // into the pitch list given to buildStrum
    double timeMs = 0.0;
    float velocity = 100.0f;
    int pass = 0;
};

/** counter = number of the strum (alternating directions, sliding windows); rootPc = chord root (-1 = unknown). */
std::vector<StrumEvent> buildStrum (const std::vector<int>& pitches, const StrumSettings& s, float velocity,
                                    int counter, int rootPc, juce::Random& rng);

/** Timing curve: position 0..1 of note k of n -> time fraction 0..1. */
float strumShapeTiming (int shape, int k, int n, juce::Random& rng);

struct StrumPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};
const std::vector<StrumPreset>& getStrumPresets();

} // namespace chordcat

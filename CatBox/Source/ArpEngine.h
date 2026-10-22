#pragma once

#include "BoxModel.h"
#include <array>
#include <cstdint>

namespace catbox
{
/** Everything the arp needs from the knobs (copied to the audio thread). */
struct ArpSettings
{
    int division = 4;          // index for arpDivisionPpq
    int mode = ModeUp;
    int octaves = 2;
    float gate = 0.6f;         // 0.1 .. 1 of a step
    float swing = 0.0f;        // 0 .. 0.75: the off-beat steps are pushed late
    int length = 16;           // pattern steps used
    int ratchet = 1;           // 1..4 notes per step
    float probability = 1.0f;  // chance a step plays
    float accent = 0.5f;       // how much louder accent steps are
    std::array<uint8_t, 16> steps {};
};

/**
    The Cat Box arpeggiator (runs on the audio thread, never allocates there).

    Like a classic synth arp (up / down / up-down, octaves, hold) plus a 16-step rhythm pattern
    where every step is On, Rest, Tie (the previous note keeps sounding) or Accent, with swing,
    ratchets (2-4 quick repeats inside a step) and a chance per step.
*/
class ArpEngine
{
public:
    ArpEngine();

    void noteOn (int note, int velocity);
    void noteOff (int note);
    void setLatch (bool on);
    bool getLatch() const noexcept { return latch; }
    void clear();
    bool hasNotes() const noexcept { return numActive > 0; }

    /** The next step starts at this quarter-note position (call when the arp (re)starts). */
    void restartAt (double ppq, bool firstStepNow = false) noexcept
    {
        phase = ppq; nextStep = 0; seqPos = 0; lastStep = -1; firstNow = firstStepNow;
    }
    bool isRunning() const noexcept { return running; }
    void setRunning (bool r) noexcept { running = r; }

    /** Plays the steps that start in [from, to): emit (note, velocity, onPpq, offPpq) per note. */
    template <typename Emit>
    void process (double from, double to, const ArpSettings& s, Emit&& emit)
    {
        if (! running || numActive == 0) return;
        if (s.division != lastDivision)
        {
            if (lastDivision >= 0) { phase = stepStart (nextStep, arpDivisionPpq (lastDivision), s.swing); nextStep = 0; }
            lastDivision = s.division;
        }
        const double stepLen = arpDivisionPpq (s.division);
        if (firstNow)   // a note pressed between two grid steps sounds at once, the next steps stay on the grid
        {
            firstNow = false;
            if (stepStart (nextStep, stepLen, s.swing) < from) { playStep (nextStep, from, stepLen, s, emit); ++nextStep; }
        }
        for (int guard = 0; guard < 64; ++guard)
        {
            const double start = stepStart (nextStep, stepLen, s.swing);
            if (start >= to) break;
            if (start >= from - 1.0e-9) playStep (nextStep, start, stepLen, s, emit);
            ++nextStep;
        }
    }

    int currentStep() const noexcept { return lastStep; }
    /** Notes currently feeding the arp (for the screen), as a 128-bit set. */
    void activeNotes (uint64_t& lo, uint64_t& hi) const noexcept;

private:
    double stepStart (int64_t k, double stepLen, float swing) const noexcept;
    void buildSequence (const ArpSettings& s, int& count) noexcept;

    template <typename Emit>
    void playStep (int64_t k, double start, double stepLen, const ArpSettings& s, Emit& emit)
    {
        const int len = juce::jlimit (1, 16, s.length);
        const int idx = (int) (k % len);
        lastStep = idx;
        const auto state = s.steps[(size_t) idx];
        if (state == StepRest || state == StepTie) return;
        if (s.probability < 0.999f && rnd.nextFloat() >= s.probability) return;

        // ties that follow make this note longer
        int ties = 0;
        for (int j = 1; j < len; ++j)
            if (s.steps[(size_t) ((idx + j) % len)] == StepTie) ++ties; else break;

        int count = 0;
        buildSequence (s, count);
        if (count == 0) return;

        const int vel = state == StepAccent ? juce::jlimit (1, 127, 92 + juce::roundToInt (s.accent * 35.0f))
                                            : juce::jlimit (1, 127, 92 - juce::roundToInt (s.accent * 20.0f));
        const int r = ties > 0 ? 1 : juce::jlimit (1, 4, s.ratchet);
        const double sub = stepLen / r;
        const double noteLen = ties > 0 ? stepLen * (ties + s.gate) : sub * s.gate;

        for (int rr = 0; rr < r; ++rr)
        {
            const double on = start + rr * sub;
            const int rv = juce::jlimit (1, 127, vel - rr * 10);
            if (s.mode == ModeChord)
            {
                const int oct = (int) (seqPos % juce::jmax (1, s.octaves));
                for (int i = 0; i < numActive; ++i)
                    emit (juce::jlimit (0, 127, sortedNotes[(size_t) i] + 12 * oct), rv, on, on + noteLen);
            }
            else
            {
                int pick = 0;
                if (s.mode == ModeRandom) pick = rnd.nextInt (count);
                else pick = (int) (seqPos % count);
                emit (sequence[(size_t) pick], rv, on, on + noteLen);
            }
        }
        ++seqPos;
    }

    static constexpr int maxNotes = 32;
    std::array<int, maxNotes> pressed {}, active {}, sortedNotes {};
    std::array<int, maxNotes * 2 * 4> sequence {};
    int numPressed = 0, numActive = 0;
    bool latch = false, running = false, firstNow = false;
    double phase = 0.0;
    int64_t nextStep = 0, seqPos = 0;
    int lastStep = -1, lastDivision = -1;
    juce::Random rnd;
};
} // namespace catbox

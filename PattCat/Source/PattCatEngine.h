#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <cmath>
#include <vector>

namespace pattcat
{
enum VoiceSelect
{
    VoiceAll = -1,      // every chord tone
    VoiceTop = -2,      // highest chord tone
    VoiceBottom = -3    // lowest chord tone
};

enum Division
{
    DivQuarter = 0,
    DivEighth,
    DivSixteenth,
    DivThirtySecond,
    DivEighthTriplet,
    DivSixteenthTriplet,
    NumDivisions
};

juce::StringArray getDivisionNames();
double divisionToPpq (int division);
juce::String voiceName (int voiceIndex);

struct Step
{
    bool active = true;
    float velocity = 1.0f;     // 0 - 1.5
    float gate = 0.8f;         // 0.1 - 1.5 (fraction of the step)
    int octaveOffset = 0;      // -2 .. +2
    int voiceIndex = VoiceAll; // -1 all, -2 top, -3 bottom, 0-5 single voice
    bool isTie = false;        // extends the previous step's notes
    bool isRest = false;
    int ratchet = 1;           // 1-4 repeats inside the step
    float probability = 1.0f;  // 0 - 1

    juce::ValueTree toValueTree() const;
    static Step fromValueTree (const juce::ValueTree&);
};

struct PatternLayer
{
    std::array<Step, 32> steps;
    int length = 16;              // 1-32
    int division = DivSixteenth;
    int swing = 0;                // 0-100 (100 = off-steps delayed by half a step)
    float velocityScale = 1.0f;
    int midiChannel = 1;
    bool muted = false;
    bool isPolyrhythmic = false;  // true: free-running cycle; false: restarts on bar lines
    juce::String name;

    // Mixer / extras
    float volume = 1.0f;
    bool solo = false;
    bool mpe = false;             // spread notes over MPE member channels 2-16
    int euclidFill = 4;
    int euclidRotation = 0;

    juce::ValueTree toValueTree() const;
    static PatternLayer fromValueTree (const juce::ValueTree&);
};

struct PatternHit
{
    int layer;
    int step;
    double ppq;
    double lengthPpq;
    float velocity;      // step velocity x layer scale x volume (0 - ~2)
    int voiceIndex;
    int octaveOffset;
    float probability;
};

/**
    4-layer step sequencer: A = Chords, B = Bass, C = Inner voices, D = Top line.
    Each layer has its own length, division, swing and channel, so layers with
    different lengths drift against each other (polyrhythm, e.g. 12 vs 16 steps).
*/
class PattCatEngine
{
public:
    PattCatEngine();

    std::array<PatternLayer, 4> layers;
    int currentLayer = 0;

    void resetToDefault();
    void clearLayer (int layer);
    void applyEuclidean (int layer, int pulses, int steps, int rotation);
    void randomizeLayer (int layer, juce::Random& rng);

    bool isLayerAudible (int layer) const;
    double getCycleLengthPpq (int layer, double barLengthPpq) const;
    int getStepAt (int layer, double ppq, double barLengthPpq) const;

    /**
        Calls fn (const PatternHit&) for every hit starting in [ppqFrom, ppqTo).
        When `applyRandomness` is false, probability and gate randomisation are
        ignored (used for display and MIDI export previews).
    */
    template <typename Fn>
    void collectHits (double ppqFrom, double ppqTo, double barLengthPpq, int globalSwing, float gateRandom,
                      juce::Random& rng, bool applyRandomness, Fn&& fn) const
    {
        for (int l = 0; l < 4; ++l)
        {
            if (! isLayerAudible (l))
                continue;

            const auto& layer = layers[(size_t) l];
            const int len = juce::jlimit (1, 32, layer.length);
            const double stepLen = divisionToPpq (layer.division);
            const double cycle = getCycleLengthPpq (l, barLengthPpq);
            const double swingAmount = juce::jlimit (0.0, 1.0, (layer.swing + globalSwing) / 100.0);

            const double firstCycle = std::floor ((ppqFrom - 2.0 * stepLen) / cycle);
            const double lastCycle = std::floor (ppqTo / cycle);

            for (double c = firstCycle; c <= lastCycle; c += 1.0)
            {
                const double cycleStart = c * cycle;
                for (int s = 0; s < len; ++s)
                {
                    const auto& st = layer.steps[(size_t) s];
                    if (! st.active || st.isRest || st.isTie)
                        continue;

                    double base = cycleStart + s * stepLen;
                    if (s % 2 == 1)
                        base += swingAmount * 0.5 * stepLen;

                    int ties = 0;
                    while (ties < len - 1)
                    {
                        const auto& next = layer.steps[(size_t) ((s + 1 + ties) % len)];
                        if (next.active && next.isTie && ! next.isRest) ++ties;
                        else break;
                    }

                    const int ratchets = juce::jlimit (1, 4, st.ratchet);
                    const double sub = stepLen / ratchets;

                    for (int k = 0; k < ratchets; ++k)
                    {
                        const double t = base + k * sub;
                        if (t < ppqFrom || t >= ppqTo)
                            continue;

                        if (applyRandomness && st.probability < 1.0f && rng.nextFloat() > st.probability)
                            continue;

                        double gate = st.gate;
                        if (applyRandomness && gateRandom > 0.0f)
                            gate *= 1.0 + (rng.nextFloat() * 2.0f - 1.0f) * gateRandom;
                        gate = juce::jlimit (0.05, 1.5, gate);

                        const double length = (k == ratchets - 1) ? sub * gate + ties * stepLen
                                                                  : sub * juce::jmin (gate, 0.9);

                        fn (PatternHit { l, s, t, length, st.velocity * layer.velocityScale * layer.volume,
                                         st.voiceIndex, st.octaveOffset, st.probability });
                    }
                }
            }
        }
    }

    /** Chord tones selected by a step's voice setting (indices wrap around small chords). */
    static void selectVoices (const std::vector<int>& sortedChord, int voiceIndex, std::vector<int>& out);

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

    static inline const juce::Identifier treeId { "PATTERN" };
};

/** 80 factory patterns. */
juce::StringArray getPatternPresetNames();
void applyPatternPreset (PattCatEngine& engine, int index);

} // namespace pattcat

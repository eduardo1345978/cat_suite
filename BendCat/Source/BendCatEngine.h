#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <vector>

namespace bendcat
{
enum BendMode    { ModeOff = 0, ModeGlide, ModeEnvelope, ModeLFO, ModePattern, NumModes };
enum GlideCurve  { CurveLinear = 0, CurveExponential, CurveSCurve };
enum LfoWave     { WaveSine = 0, WaveTriangle, WaveSaw, WaveRandom, WaveSampleHold };
enum EnvPlayMode { PlayRetrigger = 0, PlayGlobal };

juce::StringArray getModeNames();
juce::StringArray getGlideCurveNames();
juce::StringArray getLfoWaveNames();
juce::StringArray getDivisionNames();            // for LFO and pattern steps
double divisionToPpq (int index);
juce::StringArray getEnvelopeLengthNames();      // "Free (ms)", "1/16" ... "4 Bars"
double envelopeLengthToPpq (int index);          // 0 for Free

/** Multi-node bend envelope. time 0-1, bend -1..+1 (x range). curves: per segment -1..+1. */
struct BendEnvelope
{
    std::vector<std::pair<float, float>> points;
    std::vector<float> curves;
    float durationMs = 250.0f;
    bool loop = false;
    int syncDivision = 0;   // index into getEnvelopeLengthNames(), 0 = free (durationMs)

    BendEnvelope() { setDefaultADSR(); }
    void setDefaultADSR();
    float evaluate (float t) const;
    void sortAndClamp();

    juce::ValueTree toValueTree() const;
    static BendEnvelope fromValueTree (const juce::ValueTree&);
};

juce::StringArray getShapeNames();               // 27 palette shapes
BendEnvelope makeShape (int index);

/** Per-voice modulation state. */
struct BendVoice
{
    bool active = false;
    bool releasing = false;
    int inChannel = 1;
    int inNote = 60;
    int outChannel = 1;
    int noteId = -1;
    int64_t startSample = 0;
    int64_t releaseSample = 0;
    double startPpq = 0.0;
    float glideFromSemis = 0.0f;
    float detuneCents = 0.0f;
    float lfoPhaseOffset = 0.0f;
    float lfoDepthMul = 1.0f;
    float envDepthMul = 1.0f;
    bool envEnabled = true;
    float driftPhase = 0.0f;
    uint32_t seed = 1;
    int lastSentValue = -1;
    float currentBend = 0.0f;   // semitones (for the visualiser)
    float envPhase = 0.0f;      // 0-1 (for the envelope editor playhead)
};

/** Pitch-bend expression engine shared by all voices. */
class BendCatEngine
{
public:
    BendMode mode = ModeOff;
    float rangeSemitones = 2.0f;     // 1-48, must match the instrument's pitch-bend range
    float bendAmountSemitones = 2.0f; // how far the envelope / LFO bend at 100 % depth (independent of the range)
    float glideTimeMs = 120.0f;
    int glideCurve = CurveLinear;
    float lfoRateHz = 2.0f;
    bool lfoSync = false;
    int lfoDivision = 3;
    float lfoDepth = 0.3f;
    int lfoWave = WaveSine;
    float lfoVoiceRandom = 0.0f;
    bool lfoPhaseRandom = false;
    float humanDetuneCents = 0.0f;
    float driftCents = 0.0f;
    float driftRateHz = 0.05f;
    BendEnvelope envelope;
    float envDepth = 1.0f;
    float envRandom = 0.0f;
    float envChance = 1.0f;
    int envCycles = 1;
    int envPlayMode = PlayRetrigger;
    std::array<float, 16> patternCents {};   // -50..+50 per step
    int patternDivision = 3;
    float patternSmoothing = 0.3f;
    float patternBend = 0.0f;                // last evaluated pattern value (cents), for display
    bool mpeEnabled = true;
    bool scalaEnabled = false;

    void initVoice (BendVoice& v, int inChannel, int inNote, int outChannel, int64_t nowSample, double nowPpq,
                    float glideFromSemis, juce::Random& rng) const;

    /** Total bend for a voice in semitones (mode + detune + drift + microtuning). */
    float computeBend (BendVoice& v, int64_t nowSample, double nowPpq, double bpm, double sampleRate, bool hostRunning,
                       const std::array<float, 128>& tuningCents) const;

    /** 14-bit pitch wheel value: 8192 + bend / range * 8192, clamped to 0..16383. */
    static int bendToPitchWheel (float semitones, float rangeSemitones);

    static float lfoValue (int wave, double phase, uint32_t seed);
    static float shapeGlide (float p, int curve);

    juce::ValueTree toValueTree() const;    // non-parameter data (envelope, pattern)
    void fromValueTree (const juce::ValueTree&);
    static inline const juce::Identifier treeId { "BENDDATA" };
};

} // namespace bendcat

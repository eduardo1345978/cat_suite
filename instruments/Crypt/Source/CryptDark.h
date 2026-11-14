#pragma once

#include "CryptDsp.h"

/**
    Crypt's "dark arts" - the parts made for the game's zones:
    DRONE    one low tone that never stops and slowly changes (its filter, its tuning, a bit of wind)
    MADNESS  the mad version of the same music: hiss and pieces of the sound played backwards
             (the synth adds the slow pitch drift and the detune itself)
    FORMANT  three resonances like a mouth singing A E I O U - a pad becomes a choir of monks
    VHS      an old worn tape: hiss, wow + flutter (the pitch wobbles), the highs damped
*/
namespace tomb
{
/** Two moving tones + noise through a slowly moving low-pass. */
class Drone
{
public:
    void prepare (double sampleRate);
    /** adds into l/r. level 0..1 (0 = off), note 0..11 (C..B, low), move 0..1 (how fast it changes) */
    void process (float* l, float* r, int n, float level, int note, float move) noexcept;

private:
    double sr = 48000.0, t = 0.0;
    ChipOsc a, b, c;
    Lfsr noise;
    float ic1[2] {}, ic2[2] {}, gain = 0.0f, noiseHold = 0.0f;
};

/** MADNESS after the synth: hiss and backwards pieces of the last half second. */
class Madness
{
public:
    void prepare (double sampleRate);
    void process (float* l, float* r, int n, float madness) noexcept;

private:
    double sr = 48000.0;
    std::vector<float> bl, br;
    int write = 0, grain = 1, age[2] { 0, 0 }, start[2] { 0, 0 };
    uint32_t seed = 12345u;
    float lp = 0.0f;
};

/** The vowel filter. vowel 0..1 = A .. U, lfo 0..1 = how far it slowly walks, mix 0..1. */
class Formant
{
public:
    void prepare (double sampleRate);
    void process (float* l, float* r, int n, float mix, float vowel, float lfo) noexcept;
    /** the vowel the filter is on now, 0..4 (for the screen) */
    float now() const noexcept { return position; }

private:
    double sr = 48000.0, phase = 0.0;
    float ic1[2][3] {}, ic2[2][3] {}, position = 0.0f;
    juce::SmoothedValue<float> mixS;
};

/** The worn tape. All 0..1. */
class Vhs
{
public:
    void prepare (double sampleRate);
    void process (float* l, float* r, int n, float hiss, float wow, float damp) noexcept;

private:
    double sr = 48000.0, ph1 = 0.0, ph2 = 0.0;
    std::vector<float> bl, br;
    int write = 0;
    float lp[2][2] {}, hissLp = 0.0f, drift = 0.0f;
    uint32_t seed = 777u;
};

/** The FM bell: crack 0..1 (0 = a clean bell, 1 = a cracked one), t = seconds since the note. */
struct Bell
{
    double ph[4] {};
    float tick (double hz, double sampleRate, double t, float crack) noexcept;
};

/** The note names for the drone. */
juce::StringArray droneNoteNames();
} // namespace tomb

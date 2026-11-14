#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

/**
    The dark chip sound shared by Crypt (the synth + the sound-effect maker) and Crypt FX (the effect):
    old game-console waves (NES pulse / triangle / noise, Game Boy 4-bit wave tables), the crusher (fewer bits,
    a lower sample rate), and the crypt itself (a cold stone room, a dotted echo, gloom = the dark filter).
*/
namespace tomb
{
enum class Wave { Pulse12 = 0, Pulse25, Square, Triangle, Organ, Bells, Ghost, Noise, Metal };
juce::StringArray waveNames();

/** A Game Boy style table: 32 steps, 4 bits (0..15). */
using Table = std::array<uint8_t, 32>;
const Table& tableFor (Wave);

/** The NES noise: a 15-bit shift register; `metal` is its short mode (93 steps, a ringing tone). */
struct Lfsr
{
    uint16_t reg = 1;
    bool metal = false;
    float clock (void) noexcept
    {
        const int tap = metal ? 6 : 1;
        const uint16_t bit = (uint16_t) ((reg ^ (reg >> tap)) & 1u);
        reg = (uint16_t) ((reg >> 1) | (bit << 14));
        return (reg & 1u) ? 1.0f : -1.0f;
    }
};

/** One chip oscillator: phase 0..1, the wave picked per sample. No smoothing - the steps are the sound. */
struct ChipOsc
{
    double phase = 0.0;
    float noiseValue = 0.0f;
    Lfsr lfsr;
    float tick (Wave w, double hz, double sampleRate, float duty = -1.0f) noexcept;
};

/** Fewer bits and a lower sample rate (the "8-bit" sound). */
class Crusher
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { holdL = holdR = 0.0f; phase = 1.0; }
    /** bits 2..16 (16 = clean), rate in Hz (sr = clean). */
    void process (float* l, float* r, int n, float bits, float rateHz) noexcept;

private:
    double sr = 48000.0, phase = 1.0;
    float holdL = 0.0f, holdR = 0.0f;
};

/** The crypt: GLOOM (a dark low-pass), DRIVE, a cold stone room and a dotted echo. */
class Space
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    /** all 0..1; bpm for the echo (a dotted 1/8). */
    void process (float* l, float* r, int n, float gloom, float drive, float room, float echo, double bpm) noexcept;

private:
    double sr = 48000.0;
    juce::Reverb reverb;
    std::vector<float> dl, dr, wl, wr;
    int pos = 0;
    float lpL = 0.0f, lpR = 0.0f, lp2L = 0.0f, lp2R = 0.0f, fbL = 0.0f, fbR = 0.0f;
    juce::SmoothedValue<float> gloomS, roomS, echoS, driveS;
};

/** Gloom 0..1 -> the low-pass corner in Hz (18 kHz .. 700 Hz). */
float gloomHz (float gloom) noexcept;
} // namespace tomb

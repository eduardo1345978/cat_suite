#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "CatSounds.h"
#include <array>
#include <vector>
#include <cstdint>
#include <cmath>

namespace catbox
{
/** The FX page knobs, as the audio thread needs them. */
struct MasterSettings
{
    float reverb = 0.25f, size = 0.6f, delay = 0.0f, pump = 0.0f, djFilter = 0.5f;
    float tape = 0.0f, vinyl = 0.0f, crush = 0.0f, glue = 0.0f, width = 0.5f;
    int pumpTarget = 0;                         // 0 Arp + Bass, 1 Bass, 2 Arp, 3 Everything
    std::array<float, 3> sends { 0.15f, 0.35f, 0.05f };   // reverb send per instrument (Drums, Arp, Bass)
    double bpm = 110.0;
};

/** The 16 performance pads of the FX page (held = on). */
enum Perf
{
    PerfRepeat4 = 0, PerfRepeat8, PerfRepeat16, PerfRepeat32,
    PerfLowSweep, PerfHighSweep, PerfTapeStop, PerfReverse,
    PerfKillDrums, PerfKillArp, PerfKillBass, PerfCrush,
    PerfWash, PerfRiser, PerfRepeat2, PerfDelayThrow,
    NumPerf
};

const char* perfName (int pad);

/**
    Everything after the three instruments: reverb sends, sidechain pump from the kick, master
    echo, glue compressor, tape, vinyl, bit crush, the DJ filter, stereo width, and the
    performance pads (beat repeat, sweeps, tape stop, reverse, kills, wash, riser, echo throw).
*/
class MasterFx
{
public:
    void prepare (double sampleRate, int maxBlock);

    /** inL / inR: the three instruments (volume already applied). Writes the mix to outL / outR.
        songPpq = quarter notes since Play at the first sample, kickOffsets = samples where a kick hit. */
    void process (float* const* inL, float* const* inR, int n, float* outL, float* outR,
                  const MasterSettings&, uint32_t perfHeld, double songPpq, double ppqPerSample,
                  const int* kickOffsets, int numKicks);

    /** Pump amount right now (for the screen), 0..1. */
    float pumpLevel() const noexcept { return pumpEnv; }

private:
    struct Svf
    {
        float ic1 = 0, ic2 = 0;
        inline float process (float x, float g, float k, int mode) noexcept   // 0 LP, 1 HP, 2 BP
        {
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            if (! std::isfinite (ic1) || ! std::isfinite (ic2)) ic1 = ic2 = 0.0f;
            return mode == 0 ? v2 : mode == 1 ? x - k * v1 - v2 : v1;
        }
    };

    float readCapture (const std::vector<float>& b, double pos) const noexcept;

    double sr = 48000.0;
    juce::Reverb reverb;
    hub::TrackFx echo;
    std::vector<float> revL, revR, capL, capR;
    int capSize = 1, capWrite = 0;

    // smoothed values
    std::array<float, 3> kill { 1.0f, 1.0f, 1.0f };
    float washAmt = 0.0f, perfMix = 0.0f;
    int lastMode = 0;
    float pumpEnv = 0.0f, glueEnv = 0.0f, glueGain = 1.0f;
    float lpCut = 20000.0f, hpCut = 20.0f;
    float lowSweep = 0.0f, highSweep = 0.0f;   // 0..1 progress while the pad is held
    Svf lpL, lpR, hpL, hpR, riserF;
    float tapeLpL = 0, tapeLpR = 0, vinylLpL = 0, vinylLpR = 0, crackle = 0;
    float holdL = 0, holdR = 0;
    int holdCount = 0;
    hub::Noise noise;

    // performance state
    uint32_t lastHeld = 0;
    int repeatPad = -1;
    double repeatStart = 0.0, repeatLen = 1.0, repeatPhase = 0.0;
    double reverseStart = 0.0, reversePhase = 0.0, reverseLen = 1.0;
    double tapePos = 0.0, tapeRate = 1.0;
    float riserT = 0.0f, riserLevel = 0.0f;
};
} // namespace catbox

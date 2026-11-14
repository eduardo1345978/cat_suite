#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

/**
    Glide-Cat effects (stereo, in this order):
      Ensemble - chorus (2 voices) or string ensemble (3 voices, slow + fast modulation)
      Prism    - delay whose repeats are pitch-shifted to three intervals; the feedback goes
                 through the shifters again, so the intervals cascade (+7 -> +14 -> ...)
      Drive    - warm tube-like saturation with a tone control (before everything else)
      Echo     - stereo ping-pong delay, synced to the tempo, with a darkening feedback path
      Tape     - wow, flutter and age (gentle lo-fi: bandwidth, hiss)
      Rooms    - room, hall, plate, cathedral, shimmer (octave-up feedback) and bloom (slow swell)
*/
namespace glidecat
{
juce::StringArray ensembleModeNames();
juce::StringArray roomTypeNames();
juce::StringArray intervalNames();      // -24 .. +24 in musical steps
float intervalSemis (int index);

struct FxSettings
{
    int ensembleMode = 1;
    float ensembleMix = 0.3f, ensembleRate = 0.6f, ensembleDepth = 0.5f;
    float prismMix = 0.0f, prismTimeMs = 380.0f, prismFeedback = 0.35f;
    int prismI1 = 6, prismI2 = 8, prismI3 = 4;
    int roomType = 1;
    float roomMix = 0.25f, roomSize = 0.6f;
    float drive = 0.0f, driveTone = 0.5f;
    float echoMix = 0.0f, echoFeedback = 0.4f, echoBeats = 0.75f, echoTone = 0.5f;
    double bpm = 120.0;
    float tapeWow = 0.0f, tapeFlutter = 0.0f, tapeAge = 0.0f;
};
juce::StringArray echoTimeNames();       // 1/16 .. 1/2 dotted
float echoTimeBeats (int index);

class PitchShifter
{
public:
    void prepare (double sr);
    void reset();
    float process (float in, float semis) noexcept;

private:
    std::vector<float> buf;
    int write = 0;
    float phase = 0.0f, window = 3000.0f;
};

/** Removes DC and rumble (one-pole high-pass). Used inside every feedback loop: a loop that
    passes DC (reverb, pitch shifters, delays) turns the tiniest offset into a roaring sub-bass. */
struct DcBlock
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.995f;
    void setCutoff (double sr, float hz) noexcept { r = 1.0f - 6.2831853f * hz / (float) (sr > 0.0 ? sr : 48000.0); }
    void reset() noexcept { x1 = y1 = 0.0f; }
    inline float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = std::isfinite (y) ? y : 0.0f;
        return y1;
    }
};

class Effects
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* left, float* right, int numSamples, const FxSettings&);

private:
    double sr = 44100.0;
    // ensemble
    std::vector<float> ensL, ensR;
    int ensWrite = 0;
    float ensPhaseSlow = 0.0f, ensPhaseFast = 0.0f;
    // prism
    std::vector<float> prismBuf;
    int prismWrite = 0;
    PitchShifter shifters[3];
    float prismFbState = 0.0f;
    DcBlock prismDc, shimmerDc, echoDcL, echoDcR, driveDcL, driveDcR;
    // rooms
    juce::Reverb reverb;
    int lastRoom = -1;
    float lastSize = -1.0f;
    PitchShifter shimmer;
    float shimmerState = 0.0f;
    std::vector<float> preDelay;
    int preWrite = 0;
    float bloomEnv = 0.0f;
    std::vector<float> wetL, wetR;
    // drive
    float driveLpL = 0.0f, driveLpR = 0.0f;
    // echo
    std::vector<float> echoL, echoR;
    int echoWrite = 0;
    float echoSamples = 0.0f, echoLpL = 0.0f, echoLpR = 0.0f;
    // tape
    std::vector<float> tapeL, tapeR;
    int tapeWrite = 0;
    float wowPh = 0.0f, flutPh = 0.0f, ageLpL = 0.0f, ageLpR = 0.0f;
    uint32_t hiss = 12345u;
};

} // namespace glidecat

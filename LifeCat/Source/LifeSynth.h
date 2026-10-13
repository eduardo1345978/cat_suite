#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "LifeWorld.h"

/**
    Life-Cat sound: every note is built from the singing creature's genes.

      Timbre      soft pad (detuned saws, low-pass)  <->  bell (2-op FM)  <->  texture (resonant noise)
      Brightness  filter / FM index / resonance
      Harmonics   FM ratio, saw detune, resonator spread
      Attack / Release, Vibrato, Shimmer (octave sparkle), Wobble (filter drift), Pan

    The world theme colours the sound: Garden = bells / kalimba, Ocean = watery pads,
    Space = shimmering textures, Cells = glassy clicks. Then a stereo delay and a big reverb.
*/
namespace lifecat
{
enum class Theme { Garden = 0, Ocean, Space, Cells };
juce::StringArray themeNames();
constexpr int maxVoices = 32;

struct SoundSettings
{
    Theme theme = Theme::Garden;
    float brightness = 0.0f;     // -1..1
    float attack = 0.0f;         // -1..1 (shorter .. longer)
    float release = 0.0f;        // -1..1
    float reverbMix = 0.35f, reverbSize = 0.75f;
    float delayMix = 0.15f, delayFeedback = 0.4f;
    double delayBeats = 0.75;
    float width = 0.8f;
    float volume = 1.0f;
    double bpm = 120.0;
};

struct NoteRequest
{
    float midiNote = 60.0f;
    float velocity = 0.7f;
    float lengthSec = 1.0f;
    float pan = 0.0f;            // -1..1
    Genes genes;
    Role role = Role::Grazer;
};

class Synth
{
public:
    void prepare (double sampleRate, int maxBlock);
    void setSettings (const SoundSettings& s) { settings = s; }
    void noteOn (const NoteRequest&);
    void allOff();
    void render (float* left, float* right, int numSamples);
    int activeVoices() const noexcept;
    std::atomic<float> uiLevel { 0.0f };

private:
    struct Voice
    {
        bool active = false;
        uint32_t age = 0;
        NoteRequest req;
        float freq = 220.0f;
        double t = 0.0;                         // seconds since start
        float env = 0.0f, envTarget = 0.0f;
        bool releasing = false;
        float attackSec = 0.01f, releaseSec = 1.0f, bellDecay = 1.0f;
        float wPad = 0.3f, wBell = 0.4f, wTex = 0.3f;
        float ph[3] {}, detune[3] { 1.0f, 1.0f, 1.0f };
        float fmCar = 0.0f, fmMod = 0.0f, fmRatio = 2.0f, fmIndex = 2.0f;
        float shimPh = 0.0f, vibPh = 0.0f, wobPh = 0.0f, panPh = 0.0f;
        float lpIc1 = 0.0f, lpIc2 = 0.0f;                 // SVF low-pass state
        float bp1a = 0.0f, bp1b = 0.0f, bp2a = 0.0f, bp2b = 0.0f;
        uint32_t noise = 22222u;
        float gain = 0.5f, panL = 0.7f, panR = 0.7f;
    };

    void setupVoice (Voice&, const NoteRequest&);
    std::array<Voice, maxVoices> voices;
    uint32_t ageCounter = 0;
    SoundSettings settings;
    double sampleRate = 48000.0;

    // FX
    std::vector<float> delayL, delayR;
    int delayPos = 0;
    float delaySamples = 0.0f;
    juce::Reverb reverb;
    float lastSize = -1.0f;
    std::vector<float> wetL, wetR;
};

} // namespace lifecat

#pragma once

#include "CryptDark.h"

/**
    Crypt's synth: 8 voices of chip waves. Each note can be a dark chord - played at once, or as a fast
    arpeggio the way old consoles faked chords on one channel. SWEEP is the NES pitch sweep (lasers, kicks),
    STEPS makes the volume move in 16 steps like the real chips.
*/
namespace tomb
{
enum class Chord { Off = 0, Minor, Diminished, Power, Phrygian, Octaves, Sus };
enum class Arp { Off = 0, Up, Down, UpDown, Random };
juce::StringArray chordNames();
juce::StringArray arpNames();
juce::StringArray arpRateNames();
std::vector<int> chordIntervals (Chord);

struct SynthSettings
{
    Wave wave = Wave::Pulse25;
    float sub = 0.0f, detune = 0.0f, sweep = 0.0f, vibrato = 0.0f;
    float attack = 0.0f, decay = 0.3f, sustain = 0.7f, release = 0.15f;   // 0..1, mapped to seconds
    Chord chord = Chord::Off;
    Arp arp = Arp::Off;
    int arpRate = 1;              // 0 = 1/8, 1 = 1/16, 2 = 1/32, 3 = 1/64
    int octave = 0;
    float madness = 0.0f;         // slow drift + more detune (the rest of MADNESS is after the synth)
    float crack = 0.3f;           // the FM bell: 0 clean .. 1 cracked
    bool steps = true;
    double bpm = 120.0;
};

/** 0..1 -> seconds for the envelope knobs. */
float envSeconds (float v, float most) noexcept;

class Synth
{
public:
    static constexpr int maxVoices = 8;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const SynthSettings& s) noexcept { settings = s; }
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void allNotesOff();
    void sustain (bool down);
    void pitchBend (float minus1to1) noexcept { bend = minus1to1; }
    void render (float* l, float* r, int n);
    int activeVoices() const noexcept;

private:
    struct Voice
    {
        bool active = false, held = false, sustained = false, releasing = false;
        int note = 60;
        float velocity = 0.8f, env = 0.0f;
        double time = 0.0, arpClock = 0.0;
        int arpIndex = 0, arpDir = 1;
        uint32_t age = 0;
        std::array<ChipOsc, 5> osc;    // up to 4 chord tones + the sub
        ChipOsc detuned;
        std::array<Bell, 6> bells;     // the FM bell, when the wave is FM Bell (4 tones, the detune, the sub unused)
        uint32_t seed = 1;
    };
    float renderVoice (Voice&) noexcept;

    SynthSettings settings;
    double sr = 48000.0;
    std::array<Voice, maxVoices> voices;
    uint32_t clock = 0;
    bool pedal = false;
    float bend = 0.0f;
};
} // namespace tomb

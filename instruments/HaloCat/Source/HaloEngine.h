#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

/**
    Halo-Cat's engine: a choir in the middle, four planets around it.

    THE CHORD: hold several keys and that is the chord. Hold ONE key and Halo-Cat plays a PROGRESSION in that key,
    a new chord every CHANGE bars (Chord-Cat's idea). With no key held the choir fades and the planets go quiet.

    THE CHOIR: one voice per chord note (up to 5): three slightly detuned saws through two vowel resonances
    (Sing-Cat's idea). When the chord changes each voice GLIDES to the nearest new note (Glide-Cat's voice leading).

    THE PLANETS: four planets orbit at different speeds (Orbit-Cat). Each time one passes the top it rings a bell
    on one of the chord's notes, an octave or two up - the orbits make the pattern (Patt-Cat), in time with the DAW.

    STAR DUST: tiny high pings on the chord's notes at random (Swarm), and a big shimmering hall around it all.
*/
namespace halo
{
juce::StringArray progressionNames();
juce::StringArray orbitNames();
juce::StringArray vowelNames();          // "Ah", "Oh", "Oo", "Ee"

struct Settings
{
    int progression = 0;
    int orbits = 0;
    float change = 1.0f;     // bars per chord (1, 2, 4)
    float vowel = 0.0f;      // 0..3 (between vowels too)
    float choir = 0.7f, bells = 0.6f, dust = 0.3f, glide = 0.4f, shimmer = 0.5f, brightness = 0.5f;
    float spin = 1.0f;       // how fast the planets go (x)
    float outDb = 0.0f;
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
};

class Engine
{
public:
    void prepare (double sampleRate, int blockSize);
    void reset();
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void allOff();
    void process (juce::AudioBuffer<float>&, const Settings&);

    /** for the screen */
    std::array<std::atomic<float>, 4> planetPhase {};
    std::array<std::atomic<float>, 4> planetFlash {};
    std::atomic<int> chordRoot { -1 }, chordKind { 0 };
    std::atomic<float> level { 0.0f };

    /** the chord sounding now (MIDI notes), for tests and the screen */
    std::vector<int> currentChord() const;

    static constexpr int maxChord = 5;

private:
    struct ChoirVoice
    {
        double hz = 0.0, target = 0.0;
        double ph[3] {};
        float amp = 0.0f, ampTarget = 0.0f;
        float b1[2] {}, b2[2] {};       // the two vowel resonances (state)
    };
    struct Bell
    {
        bool on = false;
        double hz = 0.0, ph = 0.0, mph = 0.0, t = 0.0;
        float amp = 0.0f, pan = 0.5f, decay = 1.5f;
        double env = 0.0, envMul = 1.0, idx = 1.0;   // the level and the strike, falling by a factor each sample
    };

    void buildChord (const Settings&);
    void ring (double hz, float amp, float pan, float decay);

    double sr = 48000.0, ownBeats = 0.0, lastBeats = 0.0;
    bool wasPlaying = false;
    std::vector<int> held;                  // keys down, in order
    std::array<int, maxChord> chord {};     // MIDI notes, -1 = none
    int chordSize = 0, step = -1;
    std::array<ChoirVoice, maxChord> choir;
    std::array<Bell, 24> bellsPool;
    std::array<double, 4> phase {};
    juce::Random rng { 1234 };
    juce::Reverb hall;
    juce::SmoothedValue<float> outS;
    float lpL = 0.0f, lpR = 0.0f;
};
} // namespace halo

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cstdint>

// Little built-in instruments for the Cat Hub, so Chord-Cat, Bass-Cat and Drum-Cat
// (which only make MIDI) can be heard without any other synth.
namespace hub
{
struct Noise
{
    uint32_t s = 0x9e3779b9u;
    inline float next() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

/** One-pole low-pass / high-pass helpers. */
struct OnePole
{
    float z = 0.0f;
    inline float lp (float x, float a) noexcept { z += a * (x - z); return z; }
    inline float hp (float x, float a) noexcept { z += a * (x - z); return x - z; }
};

/** Band-limited-ish saw (PolyBLEP). */
inline float polyBlepSaw (float ph, float inc) noexcept
{
    float v = 2.0f * ph - 1.0f;
    if (ph < inc)               { const float t = ph / inc;           v -= t + t - t * t - 1.0f; }
    else if (ph > 1.0f - inc)   { const float t = (ph - 1.0f) / inc;  v -= t * t + t + t + 1.0f; }
    return v;
}

//==============================================================================
/** Polyphonic keys for Chord-Cat: Soft Pad, E-Piano, Pluck, Strings. */
class KeysSynth
{
public:
    static juce::StringArray soundNames() { return { "Soft Pad", "E-Piano", "Pluck", "Strings" }; }

    void prepare (double sampleRate);
    void setSound (int s) noexcept { sound = juce::jlimit (0, 3, s); }
    void setBendRange (float semitones) noexcept { bendRange = semitones; }
    void handle (const juce::MidiMessage&);
    void allOff();
    void render (float* l, float* r, int n);

private:
    struct Voice
    {
        bool active = false, released = false;
        int note = -1, ch = 1;
        float freq = 0.0f, vel = 0.0f, pan = 0.5f, t = 0.0f;
        float ph[3] {}, mph = 0.0f;
        float env = 0.0f;
        OnePole f1, f2;
        uint32_t age = 0;
    };
    void noteOn (int ch, int note, float vel);
    void noteOff (int ch, int note);

    std::array<Voice, 16> voices;
    std::array<float, 17> chanBend {};     // semitones per MIDI channel (Bend-Cat: one voice per channel)
    float bendRange = 2.0f;
    double sr = 48000.0;
    int sound = 0;
    uint32_t ageCounter = 0;
};

//==============================================================================
/** Mono bass for Bass-Cat (last note wins, short glide): Sub, Saw, Reese, Pluck. */
class BassSynth
{
public:
    static juce::StringArray soundNames() { return { "Sub", "Saw", "Reese", "Pluck" }; }

    void prepare (double sampleRate);
    void setSound (int s) noexcept { sound = juce::jlimit (0, 3, s); }
    void setBendRange (float semitones) noexcept { bendRange = semitones; }
    void handle (const juce::MidiMessage&);
    void allOff();
    void render (float* l, float* r, int n);

private:
    double sr = 48000.0;
    int sound = 0;
    std::array<float, 17> chanBend {};
    float bendRange = 2.0f;
    int lastChannel = 1;
    std::array<int, 16> stack {};
    int stackSize = 0;
    float freq = 55.0f, target = 55.0f, vel = 0.0f, env = 0.0f, fenv = 0.0f;
    bool gate = false;
    float ph[3] {};
    OnePole f1, f2, dc;
};

//==============================================================================
/** Synthesized drum kit for Drum-Cat (General MIDI notes): Boom Bap, 808, Lo-fi, Live. */
class DrumKit
{
public:
    static juce::StringArray soundNames() { return { "Boom Bap", "808", "Lo-fi", "Live" }; }

    void prepare (double sampleRate);
    void setSound (int s) noexcept { kit = juce::jlimit (0, 3, s); }
    void handle (const juce::MidiMessage&);
    void allOff();
    void render (float* l, float* r, int n);

private:
    enum Type { Kick, Snare, Clap, Rim, ClosedHat, OpenHat, Crash, Ride, Tom, Cowbell, Shaker };
    struct Hit
    {
        bool active = false;
        Type type = Kick;
        float vel = 0.0f, t = 0.0f, ph = 0.0f, ph2 = 0.0f, pitch = 1.0f, pan = 0.5f;
        OnePole f1, f2;
    };
    void trigger (int note, float vel);

    std::array<Hit, 24> hits;
    double sr = 48000.0;
    int kit = 0;
    Noise noise;
    float crushHold = 0.0f;
    int crushCount = 0;
};
//==============================================================================
/** The audio effects on every Hub track: filter, drive, chorus, tempo echo, bit crush. */
struct TrackFxSettings
{
    float filter = 0.0f;   // 0 = open .. 1 = dark low-pass
    float drive = 0.0f, chorus = 0.0f, delay = 0.0f, crush = 0.0f;
    double bpm = 100.0;
};

class TrackFx
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float* l, float* r, int n, const TrackFxSettings&);

private:
    double sr = 48000.0;
    float fl1 = 0, fl2 = 0, fr1 = 0, fr2 = 0, cutSmooth = 20000.0f;
    std::vector<float> chL, chR, dlL, dlR;
    int chW = 0, dlW = 0;
    float chPh = 0, dlLp = 0, dlLpR = 0, dlSmooth = 0;
    float holdL = 0, holdR = 0;
    int holdCount = 0;
};

} // namespace hub

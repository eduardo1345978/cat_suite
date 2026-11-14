#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

/**
    Poly Board's pedals: seven classic effects in a chain you can reorder, made for a synth like the Behringer
    Poly D (but any sound works). Each pedal is small and readable - they are written to be learned from:

    DRIVE      more and more gain into a soft clipper (tanh): the wave's tops get round, then square = overtones
    COMPRESSOR follows the loudness; above a threshold it turns the sound down (quiet stays, loud gets squashed)
    FILTER     a resonant low-pass whose cutoff moves (an LFO in time with the song) = auto-wah
    PHASER     4 all-pass filters shift the phase; mixed with the dry sound some frequencies cancel = moving notches
    CHORUS     two short delays (7..25 ms) whose length wobbles = slightly out of tune copies = wide and thick
    DELAY      a long delay that feeds back into itself, darker every time (like tape)
    REVERB     thousands of tiny echoes (a room)
*/
namespace board
{
enum Pedal { Drive = 0, Comp, Filter, Phaser, Chorus, Delay, Reverb, numPedals };
const char* pedalId (int p);          // "drive", "comp" ...
const char* pedalName (int p);        // "Drive", "Compressor" ...
const char* pedalKind (int p);        // "overdrive", "sustain" ...

struct Settings
{
    std::array<bool, numPedals> on {};
    std::array<float, numPedals> a {}, b {};     // the two knobs of each pedal, 0..1
    std::array<int, numPedals> order { 0, 1, 2, 3, 4, 5, 6 };
    float input = 0.0f, output = 0.0f;           // dB
    double bpm = 120.0;
};

/** What the screen shows: how hard each pedal works right now (0..1), and the levels. */
struct Meters
{
    std::array<std::atomic<float>, numPedals> work {};
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f }, gainReduction { 0.0f }, lfo { 0.0f };
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const Settings&);
    Meters meters;

private:
    void drive (float* l, float* r, int n, float gain, float tone);
    void comp (float* l, float* r, int n, float squash, float level);
    void filter (float* l, float* r, int n, float sweep, float speed, double bpm);
    void phaser (float* l, float* r, int n, float rate, float depth);
    void chorus (float* l, float* r, int n, float rate, float mix);
    void delay (float* l, float* r, int n, float time, float repeats, double bpm);
    void reverb (float* l, float* r, int n, float size, float mix);

    double sr = 48000.0;
    // drive
    float toneL = 0.0f, toneR = 0.0f;
    juce::SmoothedValue<float> driveGain, driveTone;
    // comp
    float env = 0.0f, gr = 1.0f;
    // filter
    struct Svf { float ic1 = 0.0f, ic2 = 0.0f; float run (float x, float g, float k, float& band) noexcept; };
    Svf fL, fR;
    double lfoPhase = 0.0;
    float filterCut = 0.0f;
    // phaser
    std::array<float, 4> apL {}, apR {};
    float phFbL = 0.0f, phFbR = 0.0f;
    double phPhase = 0.0;
    // chorus
    std::vector<float> chL, chR;
    int chPos = 0;
    double chPhase = 0.0;
    // delay
    std::vector<float> dlL, dlR;
    int dlPos = 0;
    float dlLpL = 0.0f, dlLpR = 0.0f;
    juce::SmoothedValue<float> dlTime;
    double wow = 0.0;
    // reverb
    juce::Reverb room;
    std::vector<float> wetL, wetR;
};

/** For the delay's TIME knob: 0..1 -> seconds, snapped to note values in time with the song (1/16 .. 1/2, dotted). */
double delaySeconds (float v, double bpm);
juce::String delayName (float v);
/** For the filter's SPEED knob: 0..1 -> LFO cycles per beat (1/4 bar .. 4 bars). */
double filterBeats (float v);
juce::String filterSpeedName (float v);
} // namespace board

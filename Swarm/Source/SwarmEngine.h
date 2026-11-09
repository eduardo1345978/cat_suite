#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <vector>

/**
    Swarm: a granular organism. The sound coming in is cut into tiny grains, and a swarm of them plays it back
    as a texture: stretched, pitched, reversed, scattered, chopped in time with Cubase, frozen.

    Where you put the point on the nebula picks the character: DREAM (top left: long, soft, an octave up, big space),
    PULSE (top right: short grains on the grid), DARK (bottom left: an octave down, murky, dirty), WILD (bottom right:
    random sizes, jumps, everything everywhere). In between they blend.

    LIFE makes it an organism: a slow chaotic attractor moves the point round where you put it and nudges the
    grain size, density and scatter, faster when the sound is loud; sometimes a grain digs up something from seconds
    ago. The knobs pull on each other too (dense swarms scatter wider, loud ones get shorter).

    After the grains: dirt, tone, a tempo echo and a big reverb. The lows (under 120 Hz) never go through the swarm:
    they stay dry and solid, whatever MIX says. Auto gain keeps it as loud as what came in.
*/
namespace swarm
{
juce::StringArray syncNames();       // grid for PULSE
double syncBeats (int index);
juce::StringArray scaleNames();
juce::StringArray echoNames();       // the echo's time
double echoBeats (int index);
juce::StringArray splitNames();      // where the dry lows end
double splitHz (int index);

struct Settings
{
    float x = 0.5f, y = 0.5f;        // the nebula (0,0 = DREAM top left, 1,0 = PULSE, 0,1 = DARK, 1,1 = WILD)
    float grain = 0.45f;             // grain size 20 ms .. 0.8 s
    float density = 0.6f;
    float pitch = 0.5f;              // how often grains jump to other notes
    float scatter = 0.4f;
    float life = 0.6f;
    float space = 0.5f;
    float tone = 0.5f;               // 0 dark .. 0.5 neutral .. 1 bright
    float dirt = 0.15f;
    float mix = 0.6f;
    float outDb = 0.0f;
    bool freeze = false;
    int sync = 1;
    int scale = 0;                   // which notes the grains may jump to (from the note they were cut from)
    // MORE: finer control (0.5 = what the nebula's character does by itself)
    float shape = 0.5f;              // 0 soft .. 1 chopped
    float reverse = 0.5f;            // 0 never .. 1 always
    float upDown = 0.5f;             // 0 jumps only down .. 1 only up
    float detune = 0.0f;             // random cents per grain (0 .. 50)
    float width = 0.67f;             // stereo spread 0 .. 150 %
    float reach = 0.5f;              // how far back in time grains are taken from
    float lifeSpeed = 0.5f;          // how fast LIFE wanders (x0.25 .. x4)
    float echo = 0.5f;               // echo amount (0.5 = the character's)
    int echoTime = 2;
    float size = 0.5f;               // reverb length
    float duck = 0.0f;               // the swarm ducks under what's coming in
    int split = 2;
    double bpm = 120.0;
    double ppq = -1.0;               // host position in quarter notes (-1 = not playing)
};

inline float grainMs (float v) noexcept { return 20.0f * std::pow (40.0f, juce::jlimit (0.0f, 1.0f, v)); }   // 20 ms .. 0.8 s

/** One grain, as the screen sees it. */
struct GrainEvent { float x, y, semis, pan, seconds, level; bool reverse; };

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const Settings&);

    // for the screen
    std::atomic<float> wanderX { 0.5f }, wanderY { 0.5f }, energy { 0.0f }, autoGainDb { 0.0f };
    std::atomic<int> activeGrains { 0 }, grainsStarted { 0 };
    std::atomic<bool> frozen { false };
    int popGrainEvents (GrainEvent* dest, int max);

private:
    struct Grain
    {
        bool on = false;
        double pos = 0.0, step = 1.0;  // read position in the ring, samples per output sample (negative = reversed)
        int len = 0, age = 0;
        float tukey = 1.0f;            // window: 1 = Hann (soft), small = flat with short fades (chopped)
        float gl = 0.0f, gr = 0.0f;
        int ringFromMemory = 0;
    };
    struct Character
    {
        float size, dens, up, down, fifth, scaleJump, rev, scatter, rhythm, chaos, space, tone, dirt, echo, spread, soft;
    };
    static Character blend (float x, float y) noexcept;
    void startGrain (const Settings&, const Character&, double gridAligned);
    float readRing (const std::vector<float>& r, double pos) const noexcept;
    void stepLife (const Settings&, int n);

    double sr = 48000.0;
    std::vector<float> ringL, ringR;
    int ringMask = 0, writePos = 0, frozenAt = -1;
    double frozenStart = 0.0, frozenEnd = 0.0;   // the part of the ring FREEZE holds (where the sound actually is)
    void captureFreeze();
    std::array<Grain, 96> grains {};
    juce::Random rng { 0x5a12 };
    double untilNext = 0.0, freeClock = 0.0, lastStep = -1.0;
    float freezeCursor = 0.0f;

    // life: a slowed-down Lorenz attractor + the input's energy
    double lx = 0.1, ly = 0.0, lz = 20.0;
    float wx = 0.5f, wy = 0.5f, env = 0.0f, lifeDens = 0.0f, lifeSize = 0.0f;

    // after the grains
    struct Biquad { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
                    float run (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
    static Biquad lowPass (double sr, double f, double q);
    static Biquad highPass (double sr, double f, double q);
    std::array<Biquad, 4> wetHp, dryLp;   // L/R x 2 stages at 120 Hz (24 dB/oct)
    float toneL = 0.0f, toneR = 0.0f, toneHpL = 0.0f, toneHpR = 0.0f;
    std::vector<float> echoL, echoR;
    int echoPos = 0;
    float echoLpL = 0.0f, echoLpR = 0.0f;
    // reverb: 8 delay lines, Householder feedback, damping, slow modulation
    static constexpr int numLines = 8;
    std::array<std::vector<float>, numLines> lines;
    std::array<int, numLines> lineLen {}, linePos {};
    std::array<float, numLines> lineLp {};
    double modPhase = 0.0;
    std::vector<float> preDelayL, preDelayR;
    int preDelayPos = 0;

    double inPow = 0.0, inPowAll = 0.0, outPow = 0.0;
    int splitNow = -1;
    void setSplit (int index);
    float duckEnv = 0.0f;
    std::array<Biquad, 4> inLp;             // high-pass: to measure only what the swarm replaces (above 120 Hz)
    float agGain = 1.0f;
    juce::SmoothedValue<float> sMix, sOut, sSpace, sDirt, sTone, sEcho;
    std::vector<float> wetL, wetR, dryL, dryR;

    juce::AbstractFifo eventFifo { 256 };
    std::array<GrainEvent, 256> events {};
};
} // namespace swarm

#pragma once

#include "Params.h"
#include "OrbitEngine.h"
#include <array>

/**
    NOTE FX: what happens to the notes of a track before they reach its sound engine.
    Any track can have one: an arpeggiator, chords (one key = one chord in the key and scale)
    or Orbit (every held note is a planet that plays when it crosses a line). It works on
    everything the track plays: the keys and pads, and its own sequencer steps.
    Runs on the audio thread; time is in quarter notes (ppq) of one clock that always runs.
*/
namespace catbox
{
enum NoteFxType { FxOff = 0, FxArp, FxChords, FxOrbit, NumNoteFx };
constexpr int numFxParams = 16;

/** Parameters, pages and presets of a note FX (same shape as an engine's). */
const EngineDef& noteFxDef (int type);
juce::StringArray noteFxNames();

/** Where the note FX sends its notes. */
struct NoteSink
{
    virtual ~NoteSink() = default;
    virtual void fxNoteOn (int note, float velocity, bool slide, bool accent) = 0;
    virtual void fxNoteOff (int note) = 0;
};

class NoteFx
{
public:
    void prepare (double sampleRate);
    /** values: numFxParams (in the type's order). */
    void set (int type, const float* values, int key, int scale);
    int getType() const noexcept { return type; }

    void noteOn (int note, float velocity, bool slide, bool accent, double ppqNow, NoteSink&);
    void noteOff (int note, double ppqNow, NoteSink&);
    /** Runs the clock from 'from' to 'to' (in ppq); 'origin' = where bar 1 began (the grid). */
    void advance (double from, double to, double origin, double bpm, NoteSink&);
    /** Everything off (the keys are forgotten too). */
    void reset (NoteSink&);

    /** For the screen: the planets (Orbit) and the last arp step. */
    const orbitcat::OrbitEngine& orbitState() const noexcept { return orbit; }
    int arpStepNow() const noexcept { return arpStep; }
    int numHeld() const noexcept { return numKeys; }

private:
    struct Pending { double ppq; int note; float vel; bool on, accent; };
    void schedule (double ppq, int note, float vel, bool on, bool accent);
    void runPending (double upTo, NoteSink&);
    void releaseAll (NoteSink&);
    void arpTick (double t, double stepLen, NoteSink&);
    void buildChord (int root, std::array<int, 8>& out, int& n) const;
    float p (int i) const noexcept { return values[(size_t) i]; }

    int type = FxOff, key = 0, scale = 1;
    std::array<float, numFxParams> values {};
    double sr = 48000.0;
    float bpmCache = 120.0f;
    int strumAlt = 0;

    // the keys held (in the order they were pressed)
    std::array<int, 16> keys {};
    std::array<float, 16> keyVel {};
    int numKeys = 0;
    bool latchedNew = true;      // Hold: the next key starts a new set
    int physicalDown = 0;

    // notes sounding / waiting
    std::array<Pending, 96> pending {};
    int numPending = 0;
    std::array<int, 128> sounding {};    // how many times each note is on in the engine

    // arp
    double nextArp = -1.0;
    int arpStep = 0, arpIndex = 0, arpDir = 1, hitCount = 0;
    juce::Random rng;

    // chords: what each key started
    std::array<std::array<int, 8>, 128> chordOf {};
    std::array<int, 128> chordSize {};

    // orbit
    orbitcat::OrbitEngine orbit;
    orbitcat::GlobalParams og;
    std::array<orbitcat::BodyParams, orbitcat::numSlots> op;
    std::array<bool, 128> orbitDown {};
};
} // namespace catbox

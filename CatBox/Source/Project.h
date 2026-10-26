#pragma once

#include "Params.h"
#include "NoteFx.h"
#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>

/**
    Cat Box 2: everything a song is. Eight equal tracks; each has a sound engine, a note FX,
    a mixer strip and a pattern (melodic steps with up to 4 notes, or 16 drum lanes).
    Plain data (no pointers, no strings) so the audio thread can take a copy at any time.
*/
namespace catbox
{
constexpr int numTracks = 8;
constexpr int maxSteps = 64;
constexpr int stepNotes = 4;
constexpr int numLanes = 16;
constexpr int numScenes = 8;
constexpr int numKnobs = 12;
constexpr int numPads = 16;

struct Step
{
    std::array<int8_t, stepNotes> notes { -1, -1, -1, -1 };
    uint8_t vel = 100;
    uint16_t len = 75;        // % of a step (10 .. 1600)
    bool slide = false, accent = false;
    uint8_t chance = 100;     // %
    int8_t nudge = 0;         // % of a step, -50 .. 50

    bool on() const noexcept { return notes[0] >= 0; }
    int count() const noexcept { int n = 0; for (auto x : notes) n += x >= 0 ? 1 : 0; return n; }
    void clear() noexcept { *this = Step(); }
    bool has (int note) const noexcept { for (auto x : notes) if (x == note) return true; return false; }
    bool add (int note) noexcept
    {
        if (has (note)) return true;
        for (auto& x : notes) if (x < 0) { x = (int8_t) note; return true; }
        return false;
    }
};

enum Rate { Rate8 = 0, Rate16, Rate32, Rate16T, NumRates };
inline double rateBeats (int r) { static const double b[] = { 0.5, 0.25, 0.125, 1.0 / 6.0 }; return b[juce::jlimit (0, NumRates - 1, r)]; }
inline const char* rateName (int r) { static const char* n[] = { "1/8", "1/16", "1/32", "1/16 T" }; return n[juce::jlimit (0, NumRates - 1, r)]; }

/** A knob turned while recording: on this step the sound parameter has this value. */
struct Lock { int16_t param = -1; float value = 0.0f; };
constexpr int locksPerStep = 4;

struct Pattern
{
    int length = 16;          // steps
    int rate = Rate16;
    std::array<Step, maxSteps> steps {};
    std::array<std::array<uint8_t, maxSteps>, numLanes> drums {};   // velocity, 0 = off
    std::array<std::array<Lock, locksPerStep>, maxSteps> locks {};   // recorded knob moves

    bool hasLocks (int step) const { for (const auto& l : locks[(size_t) step]) if (l.param >= 0) return true; return false; }
    void setLock (int step, int param, float value)
    {
        auto& ls = locks[(size_t) step];
        for (auto& l : ls) if (l.param == param) { l.value = value; return; }
        for (auto& l : ls) if (l.param < 0) { l.param = (int16_t) param; l.value = value; return; }
        ls[0] = { (int16_t) param, value };   // full: the oldest goes
    }

    void clear() { const int l = length, r = rate; *this = Pattern(); length = l; rate = r; }
    bool empty() const
    {
        for (const auto& s : steps) if (s.on()) return false;
        for (const auto& lane : drums) for (auto v : lane) if (v > 0) return false;
        for (int i = 0; i < maxSteps; ++i) if (hasLocks (i)) return false;
        return true;
    }
};

struct Track
{
    int engine = EngineAnalog;
    std::array<float, maxParams> params {};
    int fxType = FxOff;
    std::array<float, numFxParams> fx {};
    float volume = 0.75f, pan = 0.0f, sendA = 0.12f, sendB = 0.0f;
    bool mute = false, solo = false;
    Pattern pat;

    bool isDrums() const { return engineDef (engine).drums; }
};

/** Project-wide settings (the PROJECT page) and the master / send effects. */
struct Master
{
    float tempo = 110.0f, swing = 0.0f;
    int key = 0, scale = 1;               // C minor
    float volume = 0.8f;
    float djFilter = 0.5f;                // the slider: 0 = low-pass closed .. 0.5 = off .. 1 = high-pass
    float glue = 0.25f, tape = 0.0f, width = 0.5f;
    float revSize = 0.6f, revDamp = 0.5f, revWidth = 1.0f, revLowCut = 0.3f;
    int delayTime = 4;                    // index into delayNames()
    float delayFb = 0.4f, delayTone = 0.5f, delayPingPong = 1.0f;
    bool metronome = false;
    int countIn = 0;                      // bars of count-in before Rec (0 / 1)
};

juce::StringArray delayNames();
double delayBeats (int index);

struct Project
{
    Master m;
    std::array<Track, numTracks> tracks;

    /** A fresh project: the default tracks (drums, bass, chords, lead ...). */
    static Project makeDefault();
    juce::var toVar() const;
    void fromVar (const juce::var&);
};

/** One track's sound as JSON (engine + parameter values by id) and back. */
juce::var soundToVar (const Track&);
void soundFromVar (const juce::var&, Track&);
juce::var patternToVar (const Pattern&);
void patternFromVar (const juce::var&, Pattern&);

/** Loads a factory preset into the track (engine + its values). */
void loadEnginePreset (Track&, int engine, int preset);
void loadFxPreset (Track&, int fxType, int preset);
} // namespace catbox

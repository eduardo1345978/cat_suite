#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include "Phonetics.h"

/**
    The Talk-Cat score: monophonic notes with a lyric each (plus optional phonemes and per-note
    expression), and curve lanes drawn under the piano roll. Times are in beats (quarter notes)
    from the score start.
*/
namespace talkcat
{
struct ScoreNote
{
    int id = 0;
    double start = 0.0, length = 1.0;
    int pitch = 60;
    juce::String lyric { "la" };
    juce::String phonemes;            // "[l ah v]" - overrides the lyric's pronunciation when not empty
    float velocity = 0.8f;
    int language = -1;                // -1 = the plug-in's language
    float portaMs = -1.0f, vibDepth = -1.0f, vibRate = -1.0f, vibDelay = -1.0f;   // -1 = global
    float consScale = 1.0f, consStrength = -1.0f;
    double end() const noexcept { return start + length; }
};

enum Lane { LanePitch = 0, LaneDynamics, LaneBreath, LaneBright, LaneGender, LaneTension, LaneGrowl, LaneVibrato, LaneCat, LaneMouth, numLanes };

struct LaneInfo { const char* name; float min, max, def; const char* unit; };
const LaneInfo& laneInfo (int lane);

struct CurvePoint { double beat; float value; };

struct Score
{
    std::vector<ScoreNote> notes;
    std::array<std::vector<CurvePoint>, numLanes> lanes;
    double lengthBeats = 32.0;        // 8 bars
    int startBar = 1;                 // where in the song the score begins
    bool loop = false;
    int nextId = 1;

    void sort();
    void makeMonophonic();            // trims overlapping notes (the singer sings one note at a time)
    int addNote (ScoreNote n);        // returns id
    ScoreNote* find (int id);
    int indexOf (int id) const;
    float laneValue (int lane, double beat) const;
    void setLaneRange (int lane, double from, double to, const std::vector<CurvePoint>& points);
    void clearLaneRange (int lane, double from, double to);

    juce::ValueTree toValueTree() const;
    static Score fromValueTree (const juce::ValueTree&);

    /** Assigns syllables of a text to notes starting at `firstIndex` (one syllable per note, '-' holds). */
    void fillLyrics (const juce::String& text, int firstIndex, Language language, int maxNotes = 100000);

    juce::MidiMessageSequence toMidi() const;                                // with lyric meta events (ppq ticks 960)
    static Score fromMidi (const juce::MidiFile&, int track = -1);           // the busiest track, made monophonic
};

/** What the audio thread plays: notes with parsed syllables (sub-notes when one note has several syllables). */
struct CompiledNote
{
    double start = 0.0, end = 0.0;
    float pitch = 60.0f, velocity = 0.8f;
    Syllable syl {};
    bool legatoFromPrev = false, phraseEnd = true;
    float portaMs = -1.0f, vibDepth = -1.0f, vibRate = -1.0f, vibDelay = -1.0f, consScale = 1.0f, consStrength = -1.0f;
    int sourceId = 0;
};

struct CompiledScore
{
    std::vector<CompiledNote> notes;
    std::array<std::vector<CurvePoint>, numLanes> lanes;
    double lengthBeats = 32.0, startBeat = 0.0;
    bool loop = false;
    float laneValue (int lane, double beat) const;
};

std::unique_ptr<CompiledScore> compileScore (const Score&, Language defaultLanguage);

/** Syllables a note will sing (for display). */
std::vector<Syllable> noteSyllables (const ScoreNote&, Language defaultLanguage);

} // namespace talkcat

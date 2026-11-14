#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>

/**
    Hum-Cat's brain: sung or hummed audio -> notes -> a song.

    1. Pitch: YIN pitch tracking (16 kHz, 10 ms hops), voicing from the YIN dip and the loudness.
    2. Notes: the pitch curve is cut where the voice stops or moves to a new note; each note gets the
       median pitch of its frames (so slides and vibrato don't make extra notes) and a velocity from loudness.
    3. Song: notes go onto the beat grid (tempo from Cubase), the key is found (Krumhansl), the melody can be
       snapped to it, and chords (Viterbi over the diatonic chords that fit the melody best), a bass line and
       drums are written around it in the chosen style.
*/
namespace humcat
{
struct Note
{
    double start = 0.0, length = 1.0;   // seconds (sung) or beats (song)
    int pitch = 60;
    float velocity = 0.8f;
};

struct PitchTrack
{
    double hopSeconds = 0.01;
    std::vector<float> midi;     // per hop: MIDI pitch (fractional) or -1 = no voice
    std::vector<float> level;    // per hop: 0..1 loudness
};

/** Pitch-tracks mono audio. sensitivity 0..1: higher finds quieter / breathier singing. */
PitchTrack trackPitch (const float* samples, int numSamples, double sampleRate, float sensitivity = 0.5f);

/** Cuts a pitch track into notes (times in seconds). */
std::vector<Note> segment (const PitchTrack&, float minNoteSeconds = 0.07f);

enum Style { Pop = 0, Ballad, HipHop, Rock, LoFi, House, NumStyles };
juce::StringArray styleNames();
enum Part { Chords = 0, Bass, Lead, Drums, NumParts };
juce::StringArray partNames();
int partChannel (int part);   // chords 1, bass 2, lead 3, drums 10 (like Muse-Cat)

struct SongSettings
{
    double bpm = 120.0;
    double grid = 0.25;          // beats; 0 = no quantize
    int key = -1;                // pitch class, -1 = detect
    int scale = -1;              // 0 major, 1 minor, -1 = detect
    bool snapToScale = true;
    int leadOctave = 0;          // shift the sung melody by octaves
    int style = Pop;
    double chordBeats = 4.0;     // one chord per bar (4) or per half bar (2)
    bool trimStart = true;       // the first note starts the song (bar 1), else the recording start does
    double startOffsetBeats = 0.0;   // when not trimming: where the recording started, in beats
};

struct ChordSlot { double start = 0.0, length = 4.0; int root = 0; bool minor = false; int degree = 0; };

struct Song
{
    int key = 0, scale = 0;      // detected or chosen
    double bpm = 120.0;
    double lengthBeats = 0.0;    // whole bars
    std::array<std::vector<Note>, NumParts> parts;   // beats
    std::vector<ChordSlot> chords;
    juce::String chordName (const ChordSlot&) const;
};

/** Finds the key (pitch class + major / minor) of notes, weighted by length. */
void detectKey (const std::vector<Note>& notes, int& key, int& scale);
/** Builds the whole song from sung notes (seconds). */
Song makeSong (const std::vector<Note>& sung, const SongSettings&);
/** The selected parts as a MIDI file (one track per part, plus tempo). */
juce::MidiFile toMidiFile (const Song&, const std::array<bool, NumParts>& parts);
juce::String keyName (int pitchClass);
} // namespace humcat

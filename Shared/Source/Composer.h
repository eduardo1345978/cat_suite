#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <memory>

/**
    The Cat Suite composer: writes whole songs (or endless music) from a seed, a genre and a mood.

    Everything is decided with music theory so it sounds good:
      - harmony: proven progressions per genre / mood plus Markov progressions with cadences,
        borrowed chords and dominant resolutions; smooth voice leading
      - melody: a motif repeated and varied in 4-bar phrases (A A' B cadence), chord tones on strong
        beats, mostly steps, leaps answered by steps, a chorus hook that comes back every chorus
      - bass, drums (with fills, builds and drops), a counter line (arps / pads / stabs) per genre
      - arrangement: intro, verse, pre-chorus, chorus, bridge, build, drop, breakdown, outro
      - lyrics: rhyming lines with exactly one syllable per melody note ("to-night" = 2 notes)

    The same settings always give the same song (share a song by its seed).
*/
namespace cat::muse
{
enum Genre { Pop = 0, LoFi, Trap, House, Ambient, Jazz, Rock, Cinematic, Aero, Synthwave, BossaNova, Chiptune, Orchestral, NumGenres };
enum Mood { Happy = 0, Sad, Dark, Dreamy, Epic, Chill, NumMoods };
enum Part { Chords = 0, Bass, Drums, Melody, Counter, NumParts };
enum SectionType { Intro = 0, Verse, PreChorus, Chorus, Bridge, Build, Drop, Breakdown, Outro, NumSectionTypes };
/** What "regenerate" can renew. */
enum Target { NewChords = 0, NewBass, NewDrums, NewMelody, NewCounter, NewLyrics, NewArrangement, NumTargets };

juce::StringArray genreNames();
juce::StringArray moodNames();
juce::StringArray partNames();
juce::StringArray targetNames();
juce::String sectionName (int type);
juce::String scaleName (int scale);
/** The 7 notes of a scale (semitones above the key). */
std::array<int, 7> scaleIntervals (int scale);
juce::String keyName (int pitchClass);

/** General MIDI drum notes the composer writes. */
namespace drum { constexpr int kick = 36, snare = 38, rim = 37, clap = 39, hat = 42, openHat = 46, ride = 51, crash = 49,
                               tomLow = 45, tomMid = 47, tomHigh = 50, shaker = 70; }

struct Note
{
    double start = 0.0, length = 1.0;   // in beats (quarter notes) from the song start
    int pitch = 60;
    float velocity = 0.8f;
    int syllable = -1;                  // melody: index into Song::syllables
};

struct Chord
{
    double start = 0.0, length = 4.0;   // beats
    int root = 0;                       // pitch class (0 = C)
    std::vector<int> intervals;         // above the root, e.g. { 0, 4, 7, 11 }
    std::vector<int> voicing;           // MIDI notes as played
    juce::String name;                  // "Cmaj7", "Am/E" ...
    bool isTone (int pitch) const;      // is this pitch class in the chord?
};

struct Section
{
    int type = Verse;
    double start = 0.0;                 // beats
    int bars = 8;
    float energy = 0.5f;                // 0 .. 1
    std::array<bool, NumParts> on {};
    int progression = 0;                // which of the song's progressions it uses
    double end() const { return start + bars * 4.0; }
};

struct Settings
{
    juce::uint32 seed = 1;
    int genre = Pop;
    int mood = Happy;
    bool endless = false;
    int key = -1;                       // -1 = the seed picks
    double bpm = 0.0;                   // 0 = the seed picks
    std::array<juce::uint32, NumTargets> partSeeds {};   // set by "regenerate": the same settings = the same song
};

struct Song
{
    Settings settings;
    juce::String title;
    int key = 0, scale = 0;
    double bpm = 100.0;
    float swing = 0.5f;                 // 0.5 = straight 8ths
    std::vector<Section> sections;
    std::vector<Chord> chords;
    std::array<std::vector<Note>, NumParts> parts;
    juce::StringArray syllables;        // one per melody note that sings
    juce::StringArray lyricLines;       // for showing
    juce::String lyricsText;            // hyphenated, for Talk-Cat / Sing-Cat
    double lengthBeats = 0.0;
    int chunk = 0;                      // endless mode: which piece of the stream this is

    std::vector<double> lyricLineStarts;   // beat where each lyric line starts

    // what the composer remembers to keep the song (or the endless stream) recognisable
    struct MotifNote { double start = 0, length = 1; int step = 0; bool reset = false, phraseEnd = false, sectionEnd = false; float velocity = 0.8f; };
    struct Motif { std::vector<MotifNote> notes; int bars = 0; int centre = 2; };
    std::vector<std::vector<int>> progressions;   // encoded chords: verse, chorus, pre-chorus, bridge
    std::array<Motif, NumSectionTypes> motifs;    // the melody idea for each kind of section
    double chordBeats = 4.0;                      // harmonic rhythm

    int sectionIndexAt (double beat) const;
    const Chord* chordAt (double beat) const;
    int lyricLineAt (double beat) const;
};

/** A whole song (or, in endless mode, the first piece of the stream). */
Song compose (const Settings&);
/** Endless mode: the next piece, carrying the key, motifs and feel on. */
Song composeNext (const Song& previous);
/** Renews one part (chords also re-fit bass, melody and counter to the new chords). */
void regenerate (Song&, int target, juce::uint32 seed);
/** All parts as a MIDI file: chords ch 1, bass ch 2, melody ch 3 (with lyrics), counter ch 4, drums ch 10. */
juce::MidiFile toMidiFile (const Song&);
/** The MIDI channel each part plays on. */
int partChannel (int part);

//==============================================================================
/**
    Plays a song in real time: give it the song position of each block, it adds every part's
    notes (sample accurate) to one MidiBuffer per part. In endless mode it moves on to the next
    piece by itself (needNext() tells the message thread to compose one).
*/
class SongPlayer
{
public:
    SongPlayer() { held.reserve (1024); }
    void setSong (std::shared_ptr<const Song>);           // any thread (the swap is lock-free for the audio thread)
    std::shared_ptr<const Song> getSong() const;
    void queueNext (std::shared_ptr<const Song>);          // endless: the piece after the current one
    bool needNext() const noexcept { return wantNext.load() && ! nextRequested.load(); }

    /** Audio thread. ppqStart = song position at the block start; adds note-ons / offs. */
    void process (double ppqStart, double ppqPerSample, int numSamples, bool playing,
                  std::array<juce::MidiBuffer*, NumParts>& out);
    /** Stops every sounding note (audio thread). */
    void allOff (std::array<juce::MidiBuffer*, NumParts>& out, int samplePos);
    double getPlayPosition() const noexcept { return shownPos.load(); }   // beats into the current song / piece
    /** Audio thread: where ppq falls in the piece playing now, and that piece's length (0 = none). */
    double localPosition (double ppq, double& length) const noexcept;
    /** Message thread: the endless piece composed to come next (if any). */
    std::shared_ptr<const Song> getQueued() const { return queuedMsg; }
    int getCurrentChunk() const noexcept { return shownChunk.load(); }
    /** Audio thread: the tempo the song playing now was written for. */
    double songBpm() const noexcept { return song != nullptr ? song->bpm : 120.0; }

private:
    struct Held { int part, pitch; double end; };
    void emit (const Song&, double from, double to, double ppqPerSample, int sampleOffset, int numSamples,
               std::array<juce::MidiBuffer*, NumParts>& out, bool catchUp);
    void endHeld (double upTo, double from, double ppqPerSample, int sampleOffset, int numSamples,
                  std::array<juce::MidiBuffer*, NumParts>& out);

    // audio thread
    std::shared_ptr<const Song> song, next;
    // handed over under the lock
    std::shared_ptr<const Song> pendingSong, pendingNext;
    // message thread
    std::shared_ptr<const Song> currentMsg, queuedMsg;
    std::vector<std::shared_ptr<const Song>> keepAlive;
    juce::SpinLock swapLock;
    std::atomic<bool> hasPendingSong { false }, hasPendingNext { false }, wantNext { false }, nextRequested { false };
    std::vector<Held> held;
    double offset = 0.0;                // endless: beats of earlier pieces
    double lastPpq = -1.0;
    bool catchUpNext = true;
    std::atomic<double> shownPos { 0.0 };
    std::atomic<int> shownChunk { 0 };
};
} // namespace cat::muse

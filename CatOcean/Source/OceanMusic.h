#pragma once

#include "OceanSim.h"
#include "OceanArt.h"
#include "Composer.h"
#include "SongRenderer.h"
#include "SafetyLimiter.h"

/**
    Cat Ocean's music: a never-ending song from the composer, whose tempo and key slowly follow the
    genes of the fish on screen, plus every fish singing its own little tune with its own voice.
    Runs on the stream thread together with the ocean.
*/
namespace ocean
{
/** The fish voices: bubble, pluck, bell, flute, chip, pad, marimba, whistle. */
class FishSynth
{
public:
    void prepare (double sampleRate);
    void noteOn (int midiNote, float velocity, float seconds, int voice, float pan, bool shimmer);
    void render (float* l, float* r, int n);   // adds
    int activeVoices() const;

private:
    struct V
    {
        bool on = false;
        int voice = 0;
        float freq = 440, vel = 0, pan = 0.5f, t = 0, dur = 0.2f, ph = 0, ph2 = 0, lp = 0, env = 0;
        bool shimmer = false;
        juce::uint32 noise = 1;
    };
    std::array<V, 32> voices;
    double sr = 48000.0;
    juce::uint32 age = 0;
};

/** The ocean itself: waves, rain, bubbles, whale songs far away, drips in the cave. */
class Ambience
{
public:
    void prepare (double sampleRate) { sr = sampleRate; }
    void render (float* l, float* r, int n, const Sim&);   // adds
    int whales = 0;                                        // test counter
private:
    float next() noexcept { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 9) / 4194304.0f - 1.0f; }
    juce::uint32 seed = 12345;
    double sr = 48000.0, t = 0.0;
    float lpL = 0, lpR = 0, rainLp = 0;
    struct Blip { float t = -1, f = 800, pan = 0.5f, amp = 0.03f, len = 0.05f, rise = 1.8f; };
    std::array<Blip, 8> blips;
    float whaleT = -1, whaleLen = 5, whaleBase = 150, whalePh = 0, whaleLp = 0, nextWhale = 40;
    float nextBlip = 0.5f, nextDrip = 2.0f;
};

class Music
{
public:
    void prepare (double sampleRate);
    /** Renders n samples (overwrites l / r) and lets the fish sing (marks them in the sim). */
    void render (float* l, float* r, int n, Sim& sim);
    MusicInfo info (const Sim&) const;

    juce::ValueTree save() const;
    void load (const juce::ValueTree&);
    /** A new song in this style. crossfade: it starts on the next bar and the old song fades out under it. */
    void startStyle (int style, int key, double bpm, bool crossfade = false);
    bool isCrossfading() const noexcept { return decks[0].active && decks[1].active; }
    int songsStarted = 0;                               // test counter

    int pieces() const noexcept { return pieceCount; }
    double currentBpm() const noexcept { return song != nullptr ? song->bpm : 90.0; }
    juce::String currentTitle() const { return song != nullptr ? song->title : juce::String(); }
    int currentKey() const noexcept { return song != nullptr ? song->key : 0; }
    int fishNotes = 0;                                  // test counter
    int whales() const noexcept { return ambience.whales; }
    int outOfKey = 0;                                   // test counter: fish notes not in the song's scale

private:
    void nextPiece (Sim&);
    void singFish (Sim&, double fromBeat, double toBeat, double beatsPerSample, int n, int bufferOffset);

    /** Two decks, like a DJ: a new song starts on the free one and the two crossfade. */
    struct Deck
    {
        cat::muse::SongPlayer player;
        cat::muse::SongRenderer renderer;
        std::array<juce::MidiBuffer, cat::muse::NumParts> parts;
        double ppq = 0.0;
        float gain = 0.0f, target = 0.0f, step = 0.0f;   // gain moves towards target by step per sample
        float baseMelodyLevel = 0.55f;
        bool active = false;
    };
    void renderDeck (Deck&, float* l, float* r, int n, const Sim&);
    std::array<Deck, 2> decks;
    int cur = 0;
    juce::AudioBuffer<float> deckBuf;
    FishSynth fish;
    Ambience ambience;
    juce::Reverb fishVerb;
    cat::SafetyLimiter limiter;
    std::shared_ptr<const cat::muse::Song> song;
    juce::AudioBuffer<float> fishBuf;
    double sr = 48000.0;
    int style = StyleDreamy, pieceCount = 0, singRotate = 0, piecesInSong = 0;
    juce::uint32 seed = 1;
    juce::Random rng { 99 };
    int pendingStyle = -1;
};
} // namespace ocean

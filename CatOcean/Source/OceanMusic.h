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

class Music
{
public:
    void prepare (double sampleRate);
    /** Renders n samples (overwrites l / r) and lets the fish sing (marks them in the sim). */
    void render (float* l, float* r, int n, Sim& sim);
    MusicInfo info (const Sim&) const;

    juce::ValueTree save() const;
    void load (const juce::ValueTree&);
    void startStyle (int style, int key, double bpm);   // a new song in this style

    int pieces() const noexcept { return pieceCount; }
    double currentBpm() const noexcept { return song != nullptr ? song->bpm : 90.0; }
    int currentKey() const noexcept { return song != nullptr ? song->key : 0; }
    int fishNotes = 0;                                  // test counter
    int outOfKey = 0;                                   // test counter: fish notes not in the song's scale

private:
    void nextPiece (const Sim&);
    void singFish (Sim&, double fromBeat, double toBeat, double beatsPerSample, int n, int bufferOffset);

    cat::muse::SongPlayer player;
    cat::muse::SongRenderer renderer;
    FishSynth fish;
    juce::Reverb fishVerb;
    cat::SafetyLimiter limiter;
    std::shared_ptr<const cat::muse::Song> song;
    std::array<juce::MidiBuffer, cat::muse::NumParts> parts;
    juce::AudioBuffer<float> fishBuf;
    double sr = 48000.0, ppq = 0.0;
    int style = StyleAmbient, pieceCount = 0, singRotate = 0;
    juce::uint32 seed = 1;
    juce::Random rng { 99 };
    int pendingStyle = -1;
};
} // namespace ocean

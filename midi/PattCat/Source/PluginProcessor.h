#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "PattCatEngine.h"
#include "HostSync.h"
#include "MidiLearn.h"
#include "MidiScheduler.h"

/**
    Patt-Cat: 4-layer pattern sequencer for held chords.

    MIDI In (chord, e.g. from Chord-Cat) -> chord memory -> PattCatEngine
    (gates, ties, ratchets, probability, polyrhythm, swing) -> MIDI Out,
    locked to Cubase's ppq position with sample-accurate event placement.
*/
class PattCatProcessor : public juce::AudioProcessor
{
public:
    PattCatProcessor();
    ~PattCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Patt-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return JucePlugin_IsMidiEffect != 0; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return presetManager.factoryNames().size(); }
    int getCurrentProgram() override { return juce::jmax (0, getCurrentPreset()); }
    void setCurrentProgram (int i) override { presetManager.applyFactory (i); }
    const juce::String getProgramName (int i) override { return presetManager.factoryNames()[i]; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    cat::MidiLearnManager midiLearn { *this };
    cat::NoteMonitor noteMonitor;

    pattcat::PattCatEngine getEngineCopy() const;
    void editEngine (const std::function<void (pattcat::PattCatEngine&)>& fn);
    void applyPreset (int index);
    int getCurrentPreset() const { return currentPreset; }
    void clearChordMemory() { clearMemoryRequest = true; }
    std::vector<int> getLastChord() const;
    int getChordMemorySize() const { return memorySize.load(); }

    juce::MidiMessageSequence buildPatternSequence() const;
    double getHostBpm() const { return lastBpm.load(); }

    std::atomic<uint32_t> engineVersion { 1 };
    std::atomic<double> uiPpq { 0.0 };
    std::atomic<double> uiBarLength { 4.0 };
    std::atomic<bool> uiRunning { false };

    // Surprise (randomise with one-step undo) and the reactive cat's beat source
    void surprise();
    juce::MemoryBlock surpriseUndo;
    /** Factory + user presets and favourites (shared preset bar). */
    cat::PresetManager presetManager { *this, "Patt-Cat",
        { [] { return pattcat::getPatternPresetNames(); }, [this] (int i) { applyPreset (i); }, [this] { return getCurrentPreset(); } } };

    std::atomic<bool> uiTransportRunning { false };

    //==============================================================================
    // Bounce: every note (pattern notes or played keys) bounces like a ball - each bounce
    // re-plays it with a gap that shrinks / grows (Curve), fading velocity and optional octaves.
    static juce::StringArray getBounceRateNames();
    static double bounceRateToPpq (int index);

    /** One bouncing note for the display (written by the audio thread, read by the editor). */
    struct BounceEvent
    {
        int note = 60;
        int count = 0;            // number of bounces after the first hit (0 = stop this note)
        float velocity = 1.0f;
        float firstGapMs = 500.0f;
        float ratio = 1.0f;       // gap multiplier from one bounce to the next
        float fade = 0.0f;
        double startMs = 0.0;     // wall clock (Time::getMillisecondCounterHiRes) of the first hit
    };
    juce::AbstractFifo bounceFifo { 512 };
    std::array<BounceEvent, 512> bounceEvents {};


private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void handleMessage (const juce::MidiMessage& m, int pos);
    void spawnBounces (int channel, int note, int velocity, int64_t start, bool includeFirst, int sourceKey);
    void stopKeyBounces (int key, int64_t now);
    void pushBounceEvent (const BounceEvent& e);

    struct BounceRefs { std::atomic<float>* mode, *sync, *rate, *ms, *notes, *curve, *fade, *octave, *octRnd, *dur, *human, *hold; };
    BounceRefs bounce {};
    struct KeyEchoes { int channel = 1; int count = 0; std::array<uint8_t, 34> notes {}; };
    std::array<KeyEchoes, 128> keyEchoes {};
    double uiWallClockAtBlock = 0.0;
    void applyHeldKeys (int pos);
    void onChordChanged (int pos);
    void renderPattern (int fromSample, int toSample);
    void releaseActiveNotes (int pos);
    void syncMixerFromParams();
    void storeChordInMemory();

    pattcat::PattCatEngine engine;
    mutable juce::SpinLock engineLock;
    cat::HostSync hostSync;
    cat::MidiScheduler scheduler;
    juce::MidiBuffer outBuffer;
    juce::Random rng;
    int64_t sampleCounter = 0;

    // Chord input
    struct HeldKey { int note; int velocity; int64_t time; };
    std::vector<HeldKey> heldKeys;          // keys currently down, in press order
    std::vector<int> held;                  // notes of the current chord group that are still down
    std::vector<int> chord;                 // chord the pattern plays (sorted)
    bool keysChanged = false;               // note on/off seen at the current sample, not yet applied
    bool noteOnInGroup = false;
    int64_t lastNoteOnTime = -1000000;
    int64_t chordGroupStart = -1000000;
    std::vector<std::vector<int>> memory;   // Advance mode: chord per step
    int chordVelocity = 100;
    bool chordFromLatch = false;
    int64_t memoryPendingTime = -1;
    std::atomic<int> memorySize { 0 };
    std::atomic<bool> clearMemoryRequest { false };
    int mpeRoundRobin = 0;

    /** A pattern hit that is currently sounding (or was due while no chord was held). */
    struct ActiveHit
    {
        int64_t start = 0, end = 0;
        int layer = 0, voiceIndex = 0, octave = 0, step = 0;
        float velocity = 1.0f;                           // step velocity factor (x chord velocity)
        int numNotes = 0;
        std::array<std::pair<int, int>, 16> notes {};   // (channel, note)
    };
    std::vector<ActiveHit> activeHits;
    void playHitNotes (ActiveHit& hit, const std::vector<int>& source, int64_t from);
    std::vector<int> voiceScratch;

    mutable juce::SpinLock chordLock;
    std::vector<int> lastChordForUi;

    std::atomic<double> lastBpm { 120.0 };
    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PattCatProcessor)
};

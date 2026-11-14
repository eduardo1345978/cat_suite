#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "ChordEngine.h"
#include "FactoryPresets.h"
#include "ProgressionGenerator.h"
#include "PatternPlayer.h"
#include "HostSync.h"
#include "MidiLearn.h"
#include "MidiScheduler.h"

/**
    Chord-Cat: one-finger chord progression engine.

    MIDI In -> Trigger Mapper -> ChordEngine (voicing / voice leading) ->
    Performance (strum, humanise, swing/groove, rhythm) -> MIDI Out.
*/
class ChordCatProcessor : public juce::AudioProcessor
{
public:
    ChordCatProcessor();
    ~ChordCatProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chord-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return JucePlugin_IsMidiEffect != 0; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return presetManager.factoryNames().size(); }
    int getCurrentProgram() override { return juce::jmax (0, factoryIndex.load()); }
    void setCurrentProgram (int i) override { presetManager.applyFactory (i); }
    const juce::String getProgramName (int i) override { return presetManager.factoryNames()[i]; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Editor API (message thread)
    juce::AudioProcessorValueTreeState apvts;
    cat::MidiLearnManager midiLearn { *this };
    cat::NoteMonitor noteMonitor;

    chordcat::GlobalState getGlobalState() const;
    chordcat::Progression getProgression() const;
    void setProgression (const chordcat::Progression& p, bool undoable = true);
    void editSlot (int slot, const std::function<void (chordcat::ProgressionSlot&)>& fn, bool undoable = true);
    std::array<std::vector<int>, 16> getPreviewVoicings() const;
    juce::String getRomanNumeral (int slot) const;

    void undo();
    void redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

    void selectAB (int index);
    int getCurrentAB() const { return currentAB; }

    void generate (const chordcat::GeneratorSettings& settings);
    int getNumHistory() const { return (int) history.size(); }
    void recallHistory (int index);

    void diceSlot (int slot, int what); // 0 root, 1 type, 2 voicing

    void auditionSlot (int slot, bool on);

    void setCapturing (bool shouldCapture);
    bool isCapturing() const { return capturing.load(); }
    bool hasCapture() const;
    juce::MidiMessageSequence getCaptureSequence() const;

    juce::MidiMessageSequence buildProgressionSequence() const;
    juce::MidiMessageSequence buildSlotSequence (int slot) const;
    bool importMidiFile (const juce::File& file);

    void loadFactoryPreset (int index);
    bool saveUserPreset (const juce::File& file);
    bool loadUserPreset (const juce::File& file);
    static juce::File getUserPresetFolder();

    int getNumPads() const;
    double getHostBpm() const { return lastBpm.load(); }

    std::atomic<int> customScaleMask { 0x0AB5 };   // used when the scale is "Custom"
    std::atomic<int> playingSlot { -1 };
    std::atomic<int> selectedSlot { 0 };
    std::atomic<uint32_t> progressionVersion { 1 };
    std::atomic<double> uiPpq { 0.0 };
    std::atomic<bool> uiRunning { false };
    std::atomic<int> uiPlayingRhythm { 0 };

    // Surprise (randomise with one-step undo) and the reactive cat's beat source
    void surprise();
    juce::MemoryBlock surpriseUndo;

    // Strum page: presets, dice and the live view of what the strum engine sends
    void applyStrumPreset (int index);
    void randomStrum();
    std::atomic<int> currentStrumPreset { -1 };
    struct StrumUiEvent { double time = -1.0; int note = 0; float velocity = 0.0f; bool stack = false; };
    std::array<StrumUiEvent, 256> uiStrumEvents {};
    std::atomic<int> uiStrumWrite { 0 };
    std::atomic<double> uiNowSeconds { 0.0 };
    std::array<std::atomic<bool>, 128> uiChordNotes {};   // notes of the chord being strummed (incl. stack)
    std::atomic<int> uiBaseTop { 127 };

    std::atomic<int> factoryIndex { -1 };   // last factory progression loaded (-1 = none)
    /** Factory + user presets and favourites (shared preset bar). User presets live in the same folder as before. */
    cat::PresetManager presetManager { *this, "Chord-Cat",
        { [] { juce::StringArray n; for (auto& p : chordcat::getFactoryPresets()) n.add (p.name); return n; },
          [this] (int i) { loadFactoryPreset (i); }, [this] { return factoryIndex.load(); } },
        { "ccpreset" } };
    std::atomic<bool> uiTransportRunning { false };


private:
    struct Params
    {
        int transpose, inChannel, outChannel, triggerLow, triggerHigh, triggerMode, numPads, bassChannel;
        int strumDir, swing, groove, octSpread, keyRoot, scale, fixedVelocity;
        float voiceLeading, strumMs, strumCurve, strumShape, humanTime, humanVel, velSpread;
        int strumShapeType, strumStack, strumTarget, strumWindow, strumRepeats, strumBurst;
        bool strumRoundTrip, strumRelease;
        float strumMute;
        bool legato, bassSplit, scaleLock, chordTrackSync;
    };

    struct Trigger { int note; int slot; int velocity; };

    struct ActiveChord
    {
        bool active = false;
        int slot = -1;
        int rhythm = 0;
        float velocity = 100.0f;
        float strumMs = 0.0f;
        int64_t startTime = 0;
        int baseTop = 127;                        // highest chord note before the octave stack
        int rootPc = -1;
        std::vector<std::pair<int, int>> notes; // (channel, note)
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    Params readParams() const;
    chordcat::GlobalState makeGlobal (const Params&) const;

    void handleMessage (const juce::MidiMessage& m, int pos, const Params& p);
    int mapTrigger (int note, const Params& p) const;
    void triggerOn (int note, int slot, int velocity, int pos, const Params& p);
    void triggerOff (int note, int pos, const Params& p);
    void startChord (int slot, float velocity, int pos, bool legato, const Params& p);
    void releaseChord (int pos, bool strummedRelease = false, const Params* p = nullptr);
    void playNotes (const std::vector<std::pair<int, int>>& notes, float velocity, int64_t startTime, int64_t offTime,
                    float strumMs, const Params& p, int counter = -1, bool muted = false);
    void renderPattern (int fromSample, int toSample, const Params& p);
    void passThrough (const juce::MidiMessage& m, int pos);
    void evaluateChordTrackSync (int numSamples, const Params& p);
    void recordCapture (const std::vector<std::pair<int, int>>& notes, int velocity, bool on, int pos);
    void pushUndo();
    void bumpVersion() { progressionVersion.fetch_add (1); }

    // Audio-thread state
    chordcat::ChordEngine engine;
    mutable juce::SpinLock engineLock;
    cat::HostSync hostSync;
    cat::MidiScheduler scheduler;
    juce::MidiBuffer outBuffer;
    juce::Random rng;
    int64_t sampleCounter = 0;
    std::vector<Trigger> heldTriggers;
    ActiveChord current;
    bool strumFlip = false;
    int strumCounter = 0;

    std::vector<int> syncHeld;
    int64_t syncPendingTime = -1;
    int syncVelocity = 100;
    int syncSlot = -1;
    bool wasSyncing = false;

    juce::AbstractFifo uiFifo { 64 };
    std::array<std::pair<int, bool>, 64> uiEvents {};
    bool auditionActive = false;

    // Capture
    struct CaptureEvent { double ppq; int channel; int note; int velocity; bool on; };
    std::vector<CaptureEvent> captureEvents;
    mutable juce::SpinLock captureLock;
    std::atomic<bool> capturing { false };

    std::atomic<double> lastBpm { 120.0 };

    // Message-thread state
    chordcat::ProgressionGenerator generator;
    std::vector<chordcat::Progression> undoStack, redoStack, history;
    std::array<chordcat::Progression, 2> abSlots {};
    std::array<bool, 2> abValid { false, false };
    int currentAB = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordCatProcessor)
};

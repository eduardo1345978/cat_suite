#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ComputerKeyboard.h"
#include "PresetBar.h"
#include "HostSync.h"
#include "MidiLearn.h"
#include "MidiScheduler.h"
#include "LifeWorld.h"
#include "LifeMusic.h"
#include "LifeSynth.h"

/**
    Life-Cat: a tiny living ecosystem that makes music.

    The world (plants, grazers, hunters, drifters) lives on the audio thread. Every creature sings
    on the beat with a sound grown from its genes; babies inherit and mutate those genes, so the
    music evolves by itself. You are the gardener: feed, add, remove, build walls and zones,
    inspect, clone and breed. Own sound + optional MIDI out for other synths.
*/
class LifeCatProcessor : public juce::AudioProcessor, public cat::ComputerKeyboardTarget
{
public:
    LifeCatProcessor();
    ~LifeCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Life-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    cat::MidiLearnManager midiLearn { *this };
    cat::NoteMonitor noteMonitor;

    // ---- world presets
    struct WorldPreset
    {
        const char* name;
        int theme;
        uint32_t seed;
        int grazers, hunters, drifters, plants;
        std::vector<std::pair<const char*, float>> values;
        int layout = 0;          // 0 open, 1 two ponds (wall), 2 warm centre, 3 river current, 4 four rooms
    };
    static const std::vector<WorldPreset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { 0 };
    void surprise();
    juce::MemoryBlock surpriseUndo;
    cat::PresetManager presetManager { *this, "Life-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); }, [this] { return currentPreset.load(); } } };

    // ---- gardener commands (message thread -> audio thread)
    struct Command
    {
        enum Type { Food, Add, Remove, Wall, Zone, EraseZone, ClearZones, Breed, Clone, Favourite, RemoveId, NewWorld, Listen };
        Type type = Food;
        float x = 0, y = 0, x2 = 0, y2 = 0, r = 0;
        int role = 0, zone = 0;
        uint32_t id = 0, id2 = 0, seed = 0;
        bool flag = false;
        int counts[4] { 10, 3, 4, 40 };
        int layout = 0;
    };
    void send (const Command&);

    /** Latest copy of the world for drawing (message thread). Returns false if none yet. */
    bool getSnapshot (lifecat::World& into, double& ppq, bool& singing) const;

    juce::MidiMessageSequence takeRecording();          // what the ecosystem played (for Drag MIDI)
    double getHostBpm() const { return lastBpm.load(); }
    std::atomic<double> uiPpq { 0.0 };
    std::atomic<bool> uiTransportRunning { false };

    static void applyLayout (lifecat::World&, int layout);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const;
    lifecat::LifeSettings lifeSettings() const;
    lifecat::MusicSettings musicSettings() const;
    lifecat::SoundSettings soundSettings (double bpm) const;
    void apply (const Command&);
    static juce::ValueTree worldToTree (const lifecat::World&);
    static void worldFromTree (const juce::ValueTree&, lifecat::World&);

    std::map<juce::String, std::atomic<float>*> refs;
    cat::HostSync hostSync;
    juce::AudioBuffer<float> work;

    std::unique_ptr<lifecat::World> world, pendingWorld;
    juce::SpinLock worldSwapLock;
    bool hasPendingWorld = false;     // guarded by worldSwapLock
    lifecat::Conductor conductor;
    lifecat::Synth synth;
    std::vector<lifecat::SungNote> sung;
    float stepAccumulator = 0.0f;
    double freePpq = 0.0;

    juce::SpinLock commandLock;
    std::vector<Command> commands, commandsAudio;

    mutable juce::SpinLock snapLock;
    std::unique_ptr<lifecat::World> snapshot;
    double snapPpq = 0.0;
    bool snapSinging = false, hasSnapshot = false;
    int snapCounter = 0;

    std::array<int, 16> held {};
    int numHeld = 0;

    struct PendingOff { int note, channel; double samplesLeft; bool midi; };
    std::array<PendingOff, 128> offs {};
    int numOffs = 0;

    // recording for Drag MIDI (audio -> message via FIFO)
    struct RecNote { double ppq; int note; float vel; float lenBeats; int channel; };
    juce::AbstractFifo recFifo { 1024 };
    std::array<RecNote, 1024> recBuffer {};
    std::vector<RecNote> recorded;
    std::atomic<double> lastBpm { 120.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LifeCatProcessor)
};

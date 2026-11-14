#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "BassEngine.h"
#include "HostSync.h"
#include "MidiLearn.h"
#include "MidiScheduler.h"

/**
    Bass-Cat: bass-line generator that follows the chords you hold (MIDI only).

    MIDI In (a chord, e.g. from your keyboard or Chord-Cat's MIDI Out) -> chord analysis (root,
    third, fifth, seventh) -> a style pattern written in chord degrees, locked to the host
    tempo (swing, human, chance, slides) -> bass notes on the output channel.
*/
class BassCatProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener,
                         private juce::Timer
{
public:
    BassCatProcessor();
    ~BassCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Bass-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return JucePlugin_IsMidiEffect != 0; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override { return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (presets()[(size_t) i].name) : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    cat::MidiLearnManager midiLearn { *this };
    cat::NoteMonitor noteMonitor;

    static constexpr int numVariations = 20;

    basscat::Pattern getPattern() const;
    void setPattern (const basscat::Pattern& p);
    void generate (bool newSeed);
    void setVariation (int index);
    int getVariation() const { return variation.load(); }
    juce::MidiMessageSequence buildPatternSequence() const;
    double getHostBpm() const { return lastBpm.load(); }

    void surprise();
    juce::MemoryBlock surpriseUndo;

    struct Preset
    {
        const char* name;
        int64_t seed;
        std::vector<std::pair<const char*, float>> values;
    };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    int getCurrentPreset() const { return currentPreset.load(); }
    std::atomic<int> currentPreset { -1 };

    /** Factory + user presets and favourites (shared preset bar), chains and the Drum-Cat groove link. */
    cat::PresetManager presetManager { *this, "Bass-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); }, [this] { return currentPreset.load(); } } };

    /** Chord currently followed (for the display). */
    juce::String getChordText() const;

    std::atomic<uint32_t> patternVersion { 1 };
    std::atomic<double> uiPpq { 0.0 };
    std::atomic<bool> uiTransportRunning { false };
    std::atomic<int> uiChordRoot { -1 };     // pitch class, -1 = no chord
    std::atomic<int> uiChordQuality { 0 };   // third interval (3 / 4 / ...)
    std::atomic<int> uiChordSeventh { -1 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override { regenerate = true; }
    void timerCallback() override { if (regenerate.exchange (false)) generate (false); }
    std::atomic<bool> regenerate { false };
    void render (int fromSample, int toSample);
    float hash (int64_t loop, int index, int salt) const;
    basscat::GeneratorSettings currentSettings() const;

    basscat::Pattern pattern;
    mutable juce::SpinLock patternLock;
    cat::HostSync hostSync;
    cat::MidiScheduler scheduler;
    juce::MidiBuffer outBuffer;
    int64_t sampleCounter = 0;

    std::vector<int> held;            // keys down (MIDI notes)
    basscat::Chord chord;             // chord being followed
    int lastNote = -1;

    std::atomic<int64_t> seedBase { 1 };
    std::atomic<int> variation { 1 };
    std::atomic<double> lastBpm { 90.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassCatProcessor)
};

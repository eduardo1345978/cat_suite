#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "DrumCatEngine.h"
#include "HostSync.h"
#include "MidiLearn.h"
#include "MidiScheduler.h"

/**
    Drum-Cat: hip-hop drum pattern generator (MIDI only).

    Style model -> editable pattern (with hand-played micro-timing) -> playback locked to the
    host's ppq (swing, humanise, chance, velocity, open hats, lane mutes) -> drum-map notes
    on the output channel. With the transport stopped, holding a key plays the pattern.
*/
class DrumCatProcessor : public juce::AudioProcessor
{
public:
    DrumCatProcessor();
    ~DrumCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Drum-Cat"; }
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
    // Editor API (message thread)
    juce::AudioProcessorValueTreeState apvts;
    cat::MidiLearnManager midiLearn { *this };
    cat::NoteMonitor noteMonitor;

    static constexpr int numVariations = 20;

    drumcat::Pattern getPattern() const;
    void setPattern (const drumcat::Pattern& p);
    void generate (bool newSeed);            // new pattern from the current style / length / density / fills
    void selectStyle (int style);            // picks the style and generates
    void setVariation (int index);           // 1..numVariations of the current seed
    int getVariation() const { return variation.load(); }
    juce::MidiMessageSequence buildPatternSequence() const;
    double getHostBpm() const { return lastBpm.load(); }

    void surprise();
    juce::MemoryBlock surpriseUndo;

    /** Factory grooves: style + feel + a fixed seed, so a preset always gives the same pattern. */
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
    /** Factory + user presets and favourites (shared preset bar). */
    cat::PresetManager presetManager { *this, "Drum-Cat",
        { [] { return [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; }(); }, [this] (int i) { applyPreset (i); }, [this] { return currentPreset.load(); } } };


    std::atomic<uint32_t> patternVersion { 1 };
    std::atomic<double> uiPpq { 0.0 };
    std::atomic<bool> uiTransportRunning { false };
    std::array<std::atomic<uint32_t>, drumcat::NumLanes> laneHits {};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void render (int fromSample, int toSample);
    float hash (int64_t loop, int index, int salt) const;
    drumcat::GeneratorSettings currentSettings() const;

    drumcat::Pattern pattern;
    mutable juce::SpinLock patternLock;
    cat::HostSync hostSync;
    cat::MidiScheduler scheduler;
    juce::MidiBuffer outBuffer;
    int64_t sampleCounter = 0;
    int heldKeys = 0;

    std::atomic<int64_t> seedBase { 1 };
    std::atomic<int> variation { 1 };
    std::atomic<double> lastBpm { 90.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumCatProcessor)
};

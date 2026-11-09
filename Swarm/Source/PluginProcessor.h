#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "SafetyLimiter.h"
#include "SwarmEngine.h"

/**
    Swarm: a granular organism. Drag the point on the nebula (DREAM / PULSE / DARK / WILD), the knobs shape the
    grains, LIFE lets it move on its own, FREEZE holds the last seconds forever. The lows stay dry, auto gain keeps
    the level.
*/
class SwarmProcessor : public juce::AudioProcessor
{
public:
    SwarmProcessor();
    ~SwarmProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Swarm"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    swarm::Engine engine;

    struct Preset { const char* name; std::vector<std::pair<juce::String, float>> values; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    /** A new sound that still sounds good: everything but MIX, OUTPUT, FREEZE, the grid and the notes. */
    void randomise();
    std::atomic<int> currentPreset { 0 };
    bool moreOpen = false;          // the MORE panel (remembered while the window is closed)
    cat::PresetManager presetManager { *this, "Swarm",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); }, [this] { return currentPreset.load(); } } };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void set (const juce::String& id, float v);
    cat::SafetyLimiter limiter;
    double lastBpm = 120.0;
    int preparedBlock = 512;
    juce::Random dice;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwarmProcessor)
};

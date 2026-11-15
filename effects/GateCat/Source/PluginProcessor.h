#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "SafetyLimiter.h"
#include "GateEngine.h"

/**
    Gate-Cat: an audio effect that chops any sound into a rhythm (the trance gate).
    16 steps (draw them), RATE, SWING, LENGTH, ATTACK, RELEASE, DEPTH, FILTER, MIX, OUTPUT.
*/
class GateCatProcessor : public juce::AudioProcessor
{
public:
    GateCatProcessor();
    ~GateCatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Gate-Cat"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.3; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { if (i != currentPreset.load()) applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    gate::Engine engine;

    /** pattern: one character per step, '0'..'9' = level 0..100 %, '.' = 0 */
    struct Preset { const char* name; const char* pattern; std::vector<std::pair<juce::String, float>> values; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { -1 };
    cat::PresetManager presetManager { *this, "Gate-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); updateHostDisplay (ChangeDetails().withProgramChanged (true)); },
          [this] { return currentPreset.load(); } } };

    static juce::String stepId (int k) { return "step" + juce::String (k + 1); }
    gate::Settings currentSettings() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    cat::SafetyLimiter limiter;
    double lastBpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateCatProcessor)
};

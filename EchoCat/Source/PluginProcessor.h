#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include "PresetBar.h"
#include "SafetyLimiter.h"
#include "EchoEngine.h"

/**
    Echo-Cat: an echo and space effect for vocals, synths and the other Cats.
    Tape echo (in time with Cubase or free), ping-pong, tone / low cut / wobble / drive each time round,
    ducking (the echoes step back while you sing), freeze, and a shimmer space.
*/
class EchoCatProcessor : public juce::AudioProcessor
{
public:
    EchoCatProcessor();
    ~EchoCatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Echo-Cat"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    struct Preset
    {
        const char* name;
        const char* category;
        std::vector<std::pair<juce::String, float>> values;
    };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { 0 };
    cat::PresetManager presetManager { *this, "Echo-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); }, [this] { return currentPreset.load(); } } };

    echocat::Engine engine;
    /** What the engine plays with now (for the screen). */
    echocat::Settings shownSettings() const { return readSettings (uiBpm.load()); }
    double getBpm() const noexcept { return uiBpm.load(); }
    bool isHostSynced() const noexcept { return uiHostBpm.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    echocat::Settings readSettings (double bpm) const;
    float p (const char* id) const;

    std::map<std::string, std::atomic<float>*> refs;
    cat::SafetyLimiter limiter;
    std::vector<float> monoRight;
    std::atomic<double> uiBpm { 120.0 };
    std::atomic<bool> uiHostBpm { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoCatProcessor)
};

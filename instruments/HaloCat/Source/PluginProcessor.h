#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PresetBar.h"
#include "SafetyLimiter.h"
#include "HaloEngine.h"

/**
    Halo-Cat: a singing choir with four planets around it (an instrument).
    Hold a chord - or one key and it plays a progression - the choir sings and glides, the planets ring bells.
*/
class HaloCatProcessor : public juce::AudioProcessor
{
public:
    HaloCatProcessor();
    ~HaloCatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Halo-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { if (i != currentPreset.load()) applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    halo::Engine engine;
    juce::MidiKeyboardState keyboard;

    struct Preset { const char* name; std::vector<std::pair<juce::String, float>> values; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { -1 };
    cat::PresetManager presetManager { *this, "Halo-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); updateHostDisplay (ChangeDetails().withProgramChanged (true)); },
          [this] { return currentPreset.load(); } } };

    /** "Am7" - the chord sounding now, for the screen */
    juce::String chordName() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    cat::SafetyLimiter limiter;
    double lastBpm = 110.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaloCatProcessor)
};

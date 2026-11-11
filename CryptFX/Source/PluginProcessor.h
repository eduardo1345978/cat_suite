#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "SafetyLimiter.h"
#include "CryptDsp.h"

/**
    Crypt FX: any sound into a dark 8-bit one - fewer BITS, a lower sample RATE, DRIVE, GLOOM (dark), a stone
    room (CRYPT) and a dotted ECHO. The same crypt as in the Crypt synth.
*/
class CryptFxProcessor : public juce::AudioProcessor
{
public:
    CryptFxProcessor();
    ~CryptFxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Crypt FX"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { if (i != currentPreset.load()) applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    struct Preset { const char* name; std::vector<std::pair<juce::String, float>> values; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { -1 };
    cat::PresetManager presetManager { *this, "Crypt FX",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); updateHostDisplay (ChangeDetails().withProgramChanged (true)); },
          [this] { return currentPreset.load(); } } };

    /** The last 512 output samples (for the screen), written by the audio thread. */
    std::array<std::atomic<float>, 512> scope {};
    std::atomic<int> scopePos { 0 };
    std::atomic<float> level { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    tomb::Crusher crusher;
    tomb::Space space;
    cat::SafetyLimiter limiter;
    double lastBpm = 120.0;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> mixS;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CryptFxProcessor)
};

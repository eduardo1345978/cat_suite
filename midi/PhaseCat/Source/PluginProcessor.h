#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include "PresetBar.h"
#include "PhaseEngine.h"

/**
    Phase-Cat: experimental rhythm for Cubase. Four lanes of steps that drift apart (phasing), have their own
    lengths (polymeter), speed up forever (Risset rhythm) or change their beat by 3/2, 4/3 ... (metric
    modulation). MIDI out (drum notes, channel 10 by default) to Drum-Cat or any drum instrument.
*/
class PhaseCatProcessor : public juce::AudioProcessor
{
public:
    PhaseCatProcessor();
    ~PhaseCatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Phase-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return JucePlugin_IsMidiEffect != 0; }
    double getTailLengthSeconds() const override { return 0.0; }

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
        std::array<const char*, phasecat::numLanes> patterns;   // "x.x." (x = full, o = soft, . = rest)
    };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { 0 };
    cat::PresetManager presetManager { *this, "Phase-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); }, [this] { return currentPreset.load(); } } };

    /** Steps (velocity 0..1): the screen writes, the audio thread reads. */
    float getStep (int lane, int step) const noexcept { return steps[(size_t) lane][(size_t) step].load(); }
    void setStep (int lane, int step, float v) noexcept { steps[(size_t) lane][(size_t) step] = juce::jlimit (0.0f, 1.0f, v); }
    void setPattern (int lane, const juce::String& pattern);

    phasecat::Settings currentSettings() const;
    phasecat::Engine engine;
    double getBpm() const noexcept { return uiBpm.load(); }
    bool isRunning() const noexcept { return uiPlaying.load(); }

    /** Plays without Cubase's transport (the editor's Play button / tests): its own clock. */
    std::atomic<bool> freeRun { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const juce::String& id) const;

    std::map<juce::String, std::atomic<float>*> refs;
    std::array<std::array<std::atomic<float>, phasecat::maxSteps>, phasecat::numLanes> steps {};
    std::atomic<double> uiBpm { 120.0 };
    std::atomic<bool> uiPlaying { false };
    juce::MidiBuffer outBuffer;
    double freePpq = 0.0;
    bool freeWas = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseCatProcessor)
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PresetBar.h"
#include "SafetyLimiter.h"
#include "MeowEngine.h"

/**
    Meow-Cat: an instrument where every note is a cat. Play a melody and a cat meows it; play a chord and a choir
    of cats sings it. Made from scratch (no samples): SIZE kitten .. lion, MOOD sweet .. angry, LENGTH, CHOIR,
    CHAOS, PURR (also the mod wheel), HISS (hit hard), ALLEY (the echo), MODE meow / sing / chatter / purr, OCTAVE.
*/
class MeowCatProcessor : public juce::AudioProcessor
{
public:
    MeowCatProcessor();
    ~MeowCatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Meow-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { if (i != currentPreset.load()) applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    meow::Engine engine;
    juce::MidiKeyboardState keyboard;      // the keys on the screen

    struct Preset { const char* name; std::vector<std::pair<juce::String, float>> values; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { -1 };
    cat::PresetManager presetManager { *this, "Meow-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); updateHostDisplay (ChangeDetails().withProgramChanged (true)); },
          [this] { return currentPreset.load(); } } };

    /** "kitten", "house cat", "big cat", "lion" - for the screen. */
    static juce::String sizeName (float size);
    /** "sweet", "chatty", "hungry", "grumpy", "furious". */
    static juce::String moodName (float mood);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    cat::SafetyLimiter limiter;
    double lastBpm = 120.0;
    std::vector<float> monoSpare;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeowCatProcessor)
};

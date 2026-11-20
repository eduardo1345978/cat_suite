#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "SafetyLimiter.h"
#include "ChopEngine.h"

/**
    Chop-Cat: an audio effect that glitches drums in time.
    12 live pads (mouse, or MIDI keys C..B), a 16-step glitch sequencer, CHAOS (random glitches locked to the groove),
    24 presets, CRAFT (a whole new glitch style) and UNDO.
*/
class ChopCatProcessor : public juce::AudioProcessor
{
public:
    ChopCatProcessor();
    ~ChopCatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chop-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { if (i != currentPreset.load()) applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    chop::Engine engine;

    /** steps: one character per step - '.' off, 'a'..'l' = the 12 glitches (a Repeat 1/8 .. l Mute) */
    struct Preset { const char* name; const char* steps; std::vector<std::pair<juce::String, float>> values; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { -1 };
    cat::PresetManager presetManager { *this, "Chop-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); updateHostDisplay (ChangeDetails().withProgramChanged (true)); },
          [this] { return currentPreset.load(); } } };

    /** CRAFT: a new glitch style (steps, chaos, lock ...) with a made-up tape name; UNDO goes back. Message thread. */
    juce::String craft (juce::Random&);
    bool undoCraft();
    bool canUndo() const { return undoState.isValid(); }
    juce::String craftName;

    static juce::String stepId (int k) { return "st" + juce::String (k + 1); }
    void setStep (int k, int fx);
    int getStep (int k) const { return juce::roundToInt (p (stepId (k))); }
    chop::Settings settings() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    cat::SafetyLimiter limiter;
    double lastBpm = 120.0;
    juce::ValueTree undoState;
    juce::String undoName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChopCatProcessor)
};

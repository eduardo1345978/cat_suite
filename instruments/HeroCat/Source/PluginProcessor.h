#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PresetBar.h"
#include "SafetyLimiter.h"
#include "HeroEngine.h"

/**
    Hero-Cat: a lead synth for 90s game / anime melodies (an instrument that can also send its notes out as MIDI).
    Two sounds per note, Chord Lock, a waterfall / drawn arp with CHAOS, textures, an LFO and an effect chain.
*/
class HeroCatProcessor : public juce::AudioProcessor
{
public:
    HeroCatProcessor();
    ~HeroCatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Hero-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
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
    hero::Engine engine;
    juce::MidiKeyboardState keyboard;

    struct Preset { const char* name; std::vector<std::pair<juce::String, float>> values; const char* pattern = nullptr; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { -1 };
    cat::PresetManager presetManager { *this, "Hero-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); updateHostDisplay (ChangeDetails().withProgramChanged (true)); },
          [this] { return currentPreset.load(); } } };

    /** "Am7" - the chord sounding now, for the screen */
    juce::String chordName() const;
    /** CRAFT: a whole new sound - every part picked at random, but only from combinations that sound good.
        Returns its made-up name. undoCraft() goes back to the sound before it. (Message thread.) */
    juce::String craft (juce::Random&);
    bool undoCraft();
    bool canUndo() const { return undoState.isValid(); }
    juce::String craftName;

    /** the settings as the engine sees them (for tests) */
    hero::Settings settings() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    int c (const char* id) const { return juce::roundToInt (p (id)); }
    cat::SafetyLimiter limiter;
    double lastBpm = 120.0;
    juce::ValueTree undoState;
    std::array<float, 16> undoPattern {};
    juce::String undoName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeroCatProcessor)
};

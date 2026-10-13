#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ComputerKeyboard.h"
#include <map>
#include "PresetBar.h"
#include "GlideSynth.h"
#include "GlideFX.h"
#include "HostSync.h"
#include "MidiLearn.h"
#include "MidiScheduler.h"

/**
    Glide-Cat: polyphonic synth whose voices glide from chord to chord (voice-leading poly glide).
    MIDI / MPE in -> Synth (16 voices) -> Ensemble -> Prism -> Rooms -> stereo out.

    Two preset banks: SOUND presets (synth, expression, modulation, effects - the preset bar) and
    GLIDE presets (glide behaviour + autopilot), so any sound can be tried with any glide.
*/
class GlideCatProcessor : public juce::AudioProcessor, public cat::ComputerKeyboardTarget
{
public:
    GlideCatProcessor();
    ~GlideCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Glide-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentSound.load()); }
    void setCurrentProgram (int i) override { applySoundPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    cat::MidiLearnManager midiLearn { *this };
    cat::NoteMonitor noteMonitor;
    glidecat::Synth synth;

    struct Preset
    {
        const char* name;
        const char* category;
        std::vector<std::pair<juce::String, float>> values;
    };
    static const std::vector<Preset>& soundPresets();
    static const std::vector<Preset>& glidePresets();
    void applySoundPreset (int index);
    void applyGlidePreset (int index);
    void glideDice();
    void surprise();                       // sound dice
    juce::MemoryBlock surpriseUndo;
    std::atomic<int> currentSound { 0 }, currentGlide { 0 };

    cat::PresetManager presetManager { *this, "Glide-Cat",
        { [] { juce::StringArray n; for (auto& p : soundPresets()) n.add (p.name); return n; },
          [this] (int i) { applySoundPreset (i); }, [this] { return currentSound.load(); } } };

    static juce::StringArray soundParamIds();   // what a sound preset / sound dice touches
    static juce::StringArray glideParamIds();   // what a glide preset / glide dice touches

    std::atomic<double> uiPpq { 0.0 };
    std::atomic<bool> uiTransportRunning { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void readSettings();
    float p (const char* id) const;
    void resetToDefaults (const juce::StringArray& ids);

    std::map<std::string, std::atomic<float>*> refs;
    glidecat::Settings settings;
    glidecat::FxSettings fxSettings;
    glidecat::Effects effects;
    cat::HostSync hostSync;
    juce::AudioBuffer<float> work;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlideCatProcessor)
};

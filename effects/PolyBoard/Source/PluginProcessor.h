#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "SafetyLimiter.h"
#include "BoardEngine.h"

/**
    Poly Board: a pedalboard for a synth (made with the Behringer Poly D in mind): Drive, Compressor, Filter,
    Phaser, Chorus, Delay, Reverb - drag them into any order, click a footswitch to switch one on or off (or a MIDI
    footswitch: CC 80..86), a tuner for the analog synth that drifts. A VST3 effect and a program for playing live.
*/
class PolyBoardProcessor : public juce::AudioProcessor
{
public:
    PolyBoardProcessor();
    ~PolyBoardProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Poly Board"; }
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

    struct Preset { const char* name; std::vector<std::pair<juce::String, float>> values; std::array<int, board::numPedals> order; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { -1 };
    cat::PresetManager presetManager { *this, "Poly-Board",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); updateHostDisplay (ChangeDetails().withProgramChanged (true)); },
          [this] { return currentPreset.load(); } } };

    /** The order of the pedals (message thread writes, audio thread reads). */
    std::array<std::atomic<int>, board::numPedals> order;
    std::array<int, board::numPedals> getOrder() const;
    void setOrder (const std::array<int, board::numPedals>&);
    /** Moves the pedal in slot `from` to slot `to`. */
    void movePedal (int from, int to);

    board::Engine engine;
    std::atomic<double> bpm { 120.0 };

    /** The tuner: the input's pitch (Hz, 0 = none); the audio thread fills a buffer, the screen asks. */
    float detectPitch();
    std::atomic<int> footswitchHits { 0 };

    static juce::String paramId (int pedal, const char* what) { return juce::String (board::pedalId (pedal)) + "_" + what; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    cat::SafetyLimiter limiter;
    std::vector<float> monoSpare;
    std::vector<float> tunerBuf;
    std::atomic<int> tunerPos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolyBoardProcessor)
};

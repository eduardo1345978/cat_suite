#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "ComputerKeyboard.h"
#include "SafetyLimiter.h"
#include "PresetBar.h"
#include "GlitchEngine.h"

/**
    Glitch-Cat: an instrument where every note falls apart into a swarm.

    Play it from the keys. One big knob, MADNESS, goes from a warm, wobbly, dreamy swarm (on Cubase's grid)
    to rolls, ratchets, detuned jumps, stutters, reversed bits and rhythms that drift apart (and off the grid).
    It plays its own chopped beat to the melody; the low keys (below C2) can take the drums over.
    Its voices are made inside (whisper, vowels, choir, mouth) - or your own voice: drop a recording on it,
    or record it through its side-chain input.
*/
class GlitchCatProcessor : public juce::AudioProcessor, public cat::ComputerKeyboardTarget,
                           private juce::Timer
{
public:
    GlitchCatProcessor();
    ~GlitchCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Glitch-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    glitch::Engine engine;

    struct Preset { const char* name; std::vector<std::pair<juce::String, float>> values; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    /** Shakes the sound: new settings round the current ones (the madness knob stays). */
    void mutate();
    std::atomic<int> currentPreset { 0 };
    cat::PresetManager presetManager { *this, "Glitch-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); }, [this] { return currentPreset.load(); } } };

    // --- my voice (message thread) ---
    bool loadVoice (const juce::File&, juce::String& error);
    void clearVoice();
    juce::String voiceName() const;
    float voiceSeconds() const;
    void startRecording();
    void stopRecording();
    bool isRecording() const noexcept { return recording.load(); }
    float recordedSeconds() const noexcept { return (float) recPos.load() / (float) juce::jmax (1.0, sr); }
    bool hasInput() const noexcept { return inputSeen.load(); }
    std::atomic<float> inputLevel { 0.0f };
    static constexpr double maxVoiceSeconds = 12.0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void setVoice (std::unique_ptr<glitch::Sample>);
    void timerCallback() override;

    std::unique_ptr<glitch::Sample> voice;                   // what the engine reads
    std::vector<std::unique_ptr<glitch::Sample>> retired;    // old voices, kept until the audio thread surely let go
    juce::uint32 retiredAt = 0;
    mutable juce::CriticalSection voiceLock;                 // (message thread only)

    std::vector<float> recBuffer;
    std::atomic<int> recPos { 0 };
    std::atomic<bool> recording { false }, inputSeen { false };
    double sr = 48000.0;
    cat::SafetyLimiter limiter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchCatProcessor)
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "BendCatEngine.h"
#include "MPEAllocator.h"
#include "ScalaParser.h"
#include "HostSync.h"
#include "MidiLearn.h"
#include "MidiScheduler.h"

/**
    Bend-Cat: per-voice pitch-bend expression.

    MIDI In -> MPEAllocator (one channel per voice) -> BendCatEngine (glide /
    envelope / LFO / pattern + detune, drift, Scala microtuning) -> 14-bit
    pitch-bend per channel at control rate -> MPE MIDI Out.
*/
class BendCatProcessor : public juce::AudioProcessor
{
public:
    BendCatProcessor();
    ~BendCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Bend-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return JucePlugin_IsMidiEffect != 0; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override { return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (presets()[(size_t) i].name) : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    cat::MidiLearnManager midiLearn { *this };
    cat::NoteMonitor noteMonitor;

    bendcat::BendEnvelope getEnvelope() const;
    void setEnvelope (const bendcat::BendEnvelope& env);
    std::array<float, 16> getPatternCents() const;
    void setPatternCents (const std::array<float, 16>& cents);

    bool loadScala (const juce::File& file, juce::String& error);
    void clearScala();
    juce::String getScalaDescription() const;
    int getScalaNumDegrees() const;
    std::array<float, 128> getTuningTable() const;

    // Visualiser feed (written by the audio thread)
    struct VoiceView
    {
        std::atomic<bool> active { false };
        std::atomic<float> bend { 0.0f };
        std::atomic<int> note { 60 };
    };
    std::array<VoiceView, 16> voiceViews;
    std::atomic<float> uiEnvPhase { -1.0f };
    std::atomic<uint32_t> dataVersion { 1 };
    std::atomic<double> uiPpq { 0.0 };

    // Surprise (randomise with one-step undo) and the reactive cat's beat source
    void surprise();
    juce::MemoryBlock surpriseUndo;

    /** Factory presets: expression only - Bend Range and the MPE settings are never touched. */
    struct Preset
    {
        const char* name;
        int shape;                                            // envelope palette shape, -1 = keep
        std::vector<std::pair<const char*, float>> values;
        std::vector<float> pattern;                           // up to 16 steps in cents (-50..50), empty = flat
    };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    int getCurrentPreset() const { return currentPreset.load(); }
    std::atomic<int> currentPreset { -1 };
    /** Factory + user presets and favourites (shared preset bar). */
    cat::PresetManager presetManager { *this, "Bend-Cat",
        { [] { return [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; }(); }, [this] (int i) { applyPreset (i); }, [this] { return currentPreset.load(); } } };

    std::atomic<bool> uiTransportRunning { false };


private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void readParams();
    void handleMessage (const juce::MidiMessage& m, int pos);
    void noteOn (int channel, int note, int velocity, int pos);
    void noteOff (int channel, int note, int pos);
    void renderControl (int fromSample, int toSample);
    void sendBend (int voiceIndex, int pos, bool force);
    void sendConfiguration (int pos);
    float glideSourceFor (int note);
    int findVoice (int noteId) const;

    bendcat::BendCatEngine engine;
    bendcat::MPEAllocator allocator;
    bendcat::ScalaParser scala;
    mutable juce::SpinLock dataLock;
    cat::HostSync hostSync;
    juce::MidiBuffer outBuffer;
    juce::Random rng;
    int64_t sampleCounter = 0;
    double sampleRate = 44100.0;

    std::array<bendcat::BendVoice, 16> voices;     // indexed by output channel - 1 in MPE mode
    std::array<int, 16> leadVoiceForChannel {};    // non-MPE: newest voice per channel
    std::array<float, 128> tuningCache {};
    int controlInterval = 128;
    int mpeVoices = 6;
    bool mpeEnabled = true;
    bool sendConfig = true;
    float lastRange = -1.0f;
    int lastMpeVoices = -1;
    bool lastMpeEnabled = false;
    bool configPending = true;
    int64_t lastConfigSample = -1000000000;
    int blockSize = 512;

    // Glide pairing
    std::vector<int> heldPitches, previousChord, groupUsed, lastReleasedChord;
    int64_t groupStartSample = -1000000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BendCatProcessor)
};

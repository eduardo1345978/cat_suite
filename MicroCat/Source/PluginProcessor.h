#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <map>
#include "PresetBar.h"
#include "MicroEngine.h"
#include "MtsEsp.h"

/**
    Micro-Cat: microtonal tunings for Cubase. Like Ableton's tuning systems, but for any synth: it sits on a
    MIDI track before an MPE synth (Cubase's MIDI Out routing, like the other Cats) and sends every note on
    its own channel with the pitch bend that tunes it. About 40 tunings (equal, just & historical, world),
    your own in the editor, saved as Scala .scl files.
*/
class MicroCatProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    MicroCatProcessor();
    ~MicroCatProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Micro-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return JucePlugin_IsMidiEffect != 0; }
    double getTailLengthSeconds() const override { return 0.0; }

    // programs = the built-in tunings
    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    void applyPreset (int index);
    std::atomic<int> currentPreset { 0 };
    cat::PresetManager presetManager { *this, "Micro-Cat",
        { [] { juce::StringArray n; for (auto& t : micro::library()) n.add (t.name); return n; },
          [this] (int i) { applyPreset (i); }, [this] { return currentPreset.load(); } } };

    /** The tuning playing now (message thread). */
    micro::Tuning getTuning() const;
    /** Plays this tuning from now on (message thread). edited = it came from the editor (not a preset). */
    void setTuning (const micro::Tuning&, bool edited = false);
    std::atomic<int> tuningVersion { 1 };

    /** Root key and its pitch (message thread: sets the parameters). */
    void setRoot (int key, double hz);
    /** The root key at concert pitch (A4 = 440 Hz, 12-EDO): its frequency. */
    static double concertHz (int key) { return 440.0 * std::pow (2.0, (key - 69) / 12.0); }
    micro::Settings currentSettings() const;
    /** How the keys map now (the tuning's own way, or the one forced in the settings). */
    int effectiveMapping() const;

    /** Plays a note from the screen (the keyboard): it goes out through the tuning like a real one. */
    void previewNote (int key, bool on);

    micro::Engine engine;

    /** Cat Link: the pitch (MIDI semitones, fractional) every key plays now; out-of-range keys keep k. */
    std::array<float, 128> keyPitches() const;
    /** Everything that decides the pitches now (tunings A and B, morph, root, live just + its anchor). */
    micro::PitchMap currentMap() const;
    /** Tuning B: what the Morph knob moves towards (message thread). */
    micro::Tuning getTuningB() const;
    void setTuningB (const micro::Tuning&);
    int linkPublishes = 0;                        // for the tests
    /** MTS-ESP master: the tuning goes to every MTS-ESP synth (the MIDI then passes through untouched). */
    micro::MtsMaster mts;
    std::atomic<bool> mtsConnected { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const;
    void syncEngineTuning();
    void timerCallback() override;              // sends the tuning to Glide / Sing / Talk-Cat over Cat Link
    juce::String publishedKey;
    int mtsPollCount = 0;

    std::map<std::string, std::atomic<float>*> refs;
    mutable juce::SpinLock tuningLock;            // the editor writes, the audio thread copies (try-lock)
    micro::Tuning tuning;                         // message thread
    micro::TuningData pendingData, pendingDataB;  // under tuningLock
    micro::Tuning tuningB;
    int mappingFor (const micro::TuningData&) const;
    int engineVersion = 0, lastForced = -1;
    juce::MidiMessageCollector preview;
    juce::MidiBuffer previewBuffer, outBuffer;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicroCatProcessor)
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PresetBar.h"
#include "SafetyLimiter.h"
#include "CatSounds.h"
#include "HumAnalysis.h"

/**
    Hum-Cat: sing or hum a tune, get MIDI back.

    Put it on the audio track with your microphone and press Record (or drop a recording on it). It hears the
    melody, finds the key and writes the song around it: Lead (what you sang, on the grid), Chords, Bass and
    Drums in a style. Drag the MIDI into Cubase, or let it play along with Cubase (MIDI out + its own sound).
*/
class HumCatProcessor : public juce::AudioProcessor,
                        private juce::AudioProcessorValueTreeState::Listener,
                        private juce::AsyncUpdater
{
public:
    HumCatProcessor();
    ~HumCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Hum-Cat"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    struct Preset { const char* name; std::vector<std::pair<juce::String, float>> values; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { 0 };
    cat::PresetManager presetManager { *this, "Hum-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); }, [this] { return currentPreset.load(); } } };

    // --- recording (message thread starts / stops, the audio thread writes) ---
    void startRecording();
    void stopRecording();
    bool isRecording() const noexcept { return recording.load(); }
    float recordedSeconds() const noexcept { return (float) recPos.load() / (float) juce::jmax (1.0, recRate); }
    static constexpr double maxRecordSeconds = 180.0;

    /** Reads a recording (wav / aiff / flac / mp3) and listens to it. Message thread. */
    bool loadFile (const juce::File&, juce::String& error);
    void clear();
    /** A sung demo tune (Ode to Joy), so it can be tried without a microphone. */
    void loadDemo();

    // --- what it heard (message thread) ---
    std::shared_ptr<const humcat::PitchTrack> getPitchTrack() const { const juce::SpinLock::ScopedLockType l (songLock); return pitchTrack; }
    std::shared_ptr<const humcat::Song> getSong() const { const juce::SpinLock::ScopedLockType l (songLock); return song; }
    std::vector<humcat::Note> getSung() const { const juce::ScopedLock l (sungLock); return sung; }
    double getSungOffsetBeats() const noexcept { return startOffsetBeats; }
    bool isAnalysing() const noexcept { return analysing.load(); }
    std::atomic<uint32_t> songVersion { 0 };
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<double> playPosition { -1.0 };     // beats into the song, -1 = not playing

    // --- preview ---
    std::atomic<bool> previewPlaying { false };
    double hostBpm() const noexcept { return lastBpm.load(); }
    double songBpm() const;

    juce::MidiFile midiFile() const;
    std::array<bool, humcat::NumParts> partsOn() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { rebuild(); }
    void rebuild();
    void analyse (std::vector<float> audio, double rate, double offsetBeats);
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void emit (double from, double to, int sampleStart, double samplesPerBeat, juce::MidiBuffer& out, const humcat::Song&);
    void allNotesOff (juce::MidiBuffer& out, int samplePos);

    // recording
    std::vector<float> recBuffer;
    std::atomic<int> recPos { 0 };
    std::atomic<bool> recording { false };
    double recRate = 48000.0;
    std::atomic<double> recStartPpq { -1.0 };
    std::atomic<bool> sawHostPpq { false };

    // results
    juce::CriticalSection sungLock;
    std::vector<humcat::Note> sung;
    double startOffsetBeats = 0.0;           // where the recording started on Cubase's timeline (beats)
    bool recordedWithTransport = false;
    mutable juce::SpinLock songLock;
    std::shared_ptr<const humcat::PitchTrack> pitchTrack;
    std::shared_ptr<const humcat::Song> song;
    std::atomic<bool> analysing { false };
    juce::ThreadPool pool { 1 };

    // playback
    std::atomic<double> lastBpm { 0.0 };
    double sr = 48000.0;
    double previewBeat = 0.0;
    bool wasPlaying = false;
    std::array<std::vector<int>, humcat::NumParts> held;
    hub::KeysSynth leadSynth, chordSynth;
    hub::BassSynth bassSynth;
    hub::DrumKit drumKit;
    int leadSound = -1, chordSound = -1, kitSound = -1;
    juce::AudioBuffer<float> synthBuffer;
    juce::MidiBuffer partEvents;
    cat::SafetyLimiter limiter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HumCatProcessor)
};

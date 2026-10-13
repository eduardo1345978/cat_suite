#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "ChoirEngine.h"
#include "HostSync.h"
#include "MidiLearn.h"
#include "MidiScheduler.h"

/**
    Sing-Cat: a physically modelled choir of human and cat voices.

    MIDI In -> singers (glottis + vocal tract + nose, one per singer, up to 64) -> stage -> room -> audio.
    Unlike the other Cat Suite plug-ins this one makes sound: load it as an instrument.
*/
class SingCatProcessor : public juce::AudioProcessor
{
public:
    SingCatProcessor();
    ~SingCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Sing-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset.load(); }
    void setCurrentProgram (int index) override { applyPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    cat::MidiLearnManager midiLearn { *this };
    cat::NoteMonitor noteMonitor;
    singcat::Engine engine;

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
        std::vector<juce::Point<float>> path;
        const char* lyrics = nullptr;        // word presets set the lyric line
    };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    int getCurrentPreset() const { return currentPreset.load(); }
    void surprise();

    /** Lyrics typed in the editor (message thread). */
    void setLyricsText (const juce::String& text);
    juce::String getLyricsText() const { return lyricsText; }
    void rollDice();                     // new individual traits + shuffled stage positions
    void resetStage();                   // default choir arrangement for the current cast
    juce::MemoryBlock surpriseUndo;
    /** Factory + user presets and favourites (shared preset bar). */
    cat::PresetManager presetManager { *this, "Sing-Cat",
        { [] { return [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; }(); }, [this] (int i) { applyPreset (i); }, [this] { return getCurrentPreset(); } } };


    std::atomic<int> faceView { 0 };     // 0 = Stage, 1 = Lips, 2 = 3D
    std::atomic<double> uiPpq { 0.0 };
    std::atomic<bool> uiTransportRunning { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    singcat::Settings readSettings() const;
    void handleMidi (const juce::MidiMessage&);

public:
    /** Parameter indices (order matches paramIds in the .cpp). */
    enum P { mode, singers, unison, cast, style, onset, vowelX, vowelY, vowelX2, vowelY2, move, moveRate, spread,
             attack, release, vibrato, breath, rasp, glide, drift, human, purr, neck, jaw, lips, nose, chest,
             room, roomMix, micWidth, micDist, mpe, bendRange, quality, volume, seed, words, diction, numParams };
    static const char* const paramIds[numParams];

private:
    std::array<std::atomic<float>*, numParams> refs {};
    float p (P index) const { return refs[(size_t) index]->load(); }

    cat::HostSync hostSync;
    juce::String lyricsText { "la la la la" };
    juce::AudioBuffer<float> work;
    std::atomic<int> currentPreset { 0 };
    int lastCast = -1, lastSingers = -1;
    std::atomic<bool> stageResetPending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SingCatProcessor)
};

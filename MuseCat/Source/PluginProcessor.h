#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "HostSync.h"
#include "Composer.h"
#include "SongRenderer.h"

/**
    Muse-Cat: writes songs that sound good, from a genre, a mood and a number (the seed).

    Chords, a melody with a hook, words for it, bass, drums and a counter-melody, arranged into
    intro / verse / chorus / bridge / outro - or endless music that keeps writing itself.
    It plays them with its own sounds and sends every part as MIDI on its own channel:
    chords 1, bass 2, melody 3 (Sing-Cat / Talk-Cat can sing the words), counter-melody 4, drums 10.
*/
class MuseCatProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    MuseCatProcessor();
    ~MuseCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Muse-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    struct Preset { const char* name; int genre, mood; juce::uint32 seed; bool endless; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { 0 };
    cat::PresetManager presetManager { *this, "Muse-Cat",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); }, [this] { return currentPreset.load(); } } };

    // ---- the song (message thread)
    std::shared_ptr<const cat::muse::Song> getSong() const { return player.getSong(); }
    void regenerate (int target);
    void newSeed();
    void setSeed (juce::uint32);
    cat::muse::Settings currentSettings() const;
    /** Writes the song (or one part) as a WAV with Muse-Cat's sounds, faster than real time. */
    bool exportWav (const juce::File&, int onlyPart = -1, std::function<bool (float)> progress = {}) const;
    bool exportMidi (const juce::File&) const;
    cat::muse::SoundSet soundSet() const;
    /** Composes now if the genre / mood / seed / song-or-endless changed (also called by the timer). */
    void updateSong (bool force = false);

    // ---- playing
    std::atomic<bool> previewPlaying { false };     // play by itself (standalone app, or the host is stopped)
    bool isRunning() const noexcept { return uiRunning.load(); }
    bool isHostPlaying() const noexcept { return uiHostPlaying.load(); }
    double getSongBeat() const noexcept { return player.getPlayPosition(); }
    double getHostBpm() const noexcept { return uiBpm.load(); }
    float partMeter (int part) const noexcept { return renderer.partMeter (part); }

    static juce::StringArray soundChoices (int part);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    float p (const char* id) const;
    void setSongNow (std::shared_ptr<const cat::muse::Song>);

    std::map<juce::String, std::atomic<float>*> refs;
    cat::HostSync hostSync;
    cat::muse::SongPlayer player;
    cat::muse::SongRenderer renderer;
    std::array<juce::MidiBuffer, cat::muse::NumParts> parts;
    juce::MidiBuffer outMidi;
    double internalPpq = 0.0;
    bool wasPreview = false;
    std::array<int, 5> shownSounds { -1, -1, -1, -1, -1 };
    std::atomic<bool> uiRunning { false }, uiHostPlaying { false };
    std::atomic<double> uiBpm { 120.0 };

    // message thread
    std::array<juce::uint32, cat::muse::NumTargets> partSeeds {};
    cat::muse::Settings composedFor;
    bool composedOnce = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuseCatProcessor)
};

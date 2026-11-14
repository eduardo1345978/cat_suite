#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "NegativeCore.h"

/**
    Negative: a photo you can hear. The picture is turned into sound in the background (whenever it or a setting
    changes) and then played (a VST3 instrument): with Cubase in time (SYNC), on MIDI notes (C4 = as it is, other keys
    play it higher or lower), or with PLAY, at LEVEL. At a low LEVEL in a high range it hides in a song and only shows
    in a spectrogram.

    In the standalone program it also opens sounds (a song to hide the photo in) and saves .wav files and pictures.
*/
class NegativeProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener, private juce::Thread
{
public:
    NegativeProcessor();
    ~NegativeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Negative"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- the message thread's side ----
    void setPhoto (const juce::Image&, const juce::String& name);
    juce::Image getPhoto() const;
    juce::String getPhotoName() const;
    bool loadPhotoFile (const juce::File&);
    /** A sound becomes the photo: its spectrogram (LOW .. HIGH), so it can be seen, painted on and played back. */
    bool loadSoundAsPhoto (const juce::File&);
    /** A sound to hide the photo in (the standalone program): it plays with PLAY and goes into the saved .wav. */
    bool loadSongFile (const juce::File&);
    void clearSong();
    juce::String getSongName() const;
    /** What PLAY / the saved .wav sound like: the photo at LEVEL (+ the song). */
    juce::AudioBuffer<float> getMix() const;
    bool saveWav (const juce::File&) const;
    juce::File writeTempWav() const;
    /** The spectrogram of what comes out (for the screen and for "save picture"). */
    juce::Image getSpectrogram() const;
    int getRenderCount() const noexcept { return renderCount.load(); }
    bool isRendering() const noexcept { return rendering.load(); }
    double renderSampleRate() const;
    void waitForRender (int timeoutMs = 20000);          // tests: until the picture has become sound

    std::atomic<bool> previewing { false };
    std::atomic<float> playPos { -1.0f };                 // 0..1 where it plays now (-1 = silent), for the screen
    bool isStandalone() const noexcept { return wrapperType == wrapperType_Standalone; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override;
    void run() override;
    void markDirty();
    bool readAudio (const juce::File&, double maxSeconds, juce::AudioBuffer<float>& out, double& rate);
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    mutable juce::CriticalSection dataLock;               // the message thread and the render thread
    juce::Image photo;
    juce::String photoName;
    juce::AudioBuffer<float> song;
    double songRate = 48000.0;
    juce::String songName;
    juce::Image spectrogram;

    juce::SpinLock audioLock;                             // the audio thread only try-locks this
    juce::AudioBuffer<float> sound;                       // the photo as sound (normalised)
    juce::AudioBuffer<float> songAudio;                   // the song, for PLAY

    std::atomic<bool> dirty { true }, cancel { false }, rendering { false };
    std::atomic<int> renderCount { 0 };
    std::atomic<juce::int64> dirtySince { 0 };
    std::atomic<double> hostBpm { 120.0 }, sr { 48000.0 }, renderedBpm { 0.0 };

    struct Voice { bool on = false, releasing = false; double pos = 0.0, rate = 1.0; float gain = 0.0f, env = 0.0f; int note = -1; };
    std::array<Voice, 8> voices {};
    double previewPos = 0.0;
    juce::SmoothedValue<float> level;
    juce::AudioFormatManager formats;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NegativeProcessor)
};

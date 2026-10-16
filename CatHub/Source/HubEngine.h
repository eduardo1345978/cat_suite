#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "HubSounds.h"
#include "SafetyLimiter.h"
#include <array>
#include <atomic>

namespace hub
{
enum Part { ChordPart = 0, BassPart, DrumPart, LifePart, NumParts };

/** The Cat plug-ins the Hub plays (Glide-Cat is an optional sound for the chords). */
enum CatId { ChordCat = 0, BassCat, DrumCat, LifeCat, GlideCat, NumCats };

const char* catName (int id);          // "Chord-Cat" ...
const char* partName (int part);       // "Chords" ...

/** Mixer + sound settings per part, shared between the UI and the audio thread. */
struct PartSettings
{
    std::atomic<float> volume { 0.8f };
    std::atomic<float> reverb { 0.15f };
    std::atomic<bool> mute { false }, solo { false };
    std::atomic<int> sound { 0 };          // built-in sound; chords: the last entry = Glide-Cat
    std::atomic<float> meter { 0.0f };     // peak, read by the UI
    std::atomic<double> energy { 0.0 };    // sum of squares since the start (self test)
    // which notes are sounding (128 bits); drums: hit since the UI last looked
    std::array<std::atomic<uint32_t>, 4> notes {};
    std::array<std::atomic<uint32_t>, 4> hits {};

    /** Notes currently held (and, for drums, hit since the last call). Message thread. */
    std::vector<int> takeNotes()
    {
        std::vector<int> out;
        for (int w = 0; w < 4; ++w)
        {
            const uint32_t bits = notes[(size_t) w].load() | hits[(size_t) w].exchange (0);
            for (int b = 0; b < 32; ++b) if (bits & (1u << b)) out.push_back (w * 32 + b);
        }
        return out;
    }
};

/**
    Hosts Chord-Cat, Bass-Cat, Drum-Cat and Life-Cat (the real VST3s) and wires them like a
    small studio: the song steps through Chord-Cat's pads, Chord-Cat's chords go to Bass-Cat,
    Life-Cat and the keys, Bass-Cat plays the bass synth, Drum-Cat plays the drum kit. One
    transport (play / tempo) drives every Cat. Works with an audio device or offline (self test).
*/
class HubEngine : public juce::AudioIODeviceCallback
{
public:
    HubEngine();
    ~HubEngine() override;

    //==== loading (message thread)
    /** Where to look for the Cat .vst3 bundles, best first. */
    static juce::Array<juce::File> defaultSearchFolders (const juce::File& extraFolder);
    /** Loads every Cat it finds; returns a short report. Call before audio starts. */
    juce::String loadCats (const juce::Array<juce::File>& folders);
    juce::AudioPluginInstance* getCat (int id) const { return cats[(size_t) id].get(); }
    juce::File getCatFile (int id) const { return catFiles[(size_t) id]; }
    bool hasCat (int id) const { return cats[(size_t) id] != nullptr; }
    void releaseCats();

    //==== transport + song (any thread)
    void setPlaying (bool shouldPlay);
    bool isPlaying() const noexcept { return playing.load(); }
    std::atomic<double> bpm { 92.0 };
    std::atomic<int> barsPerChord { 1 };
    std::atomic<bool> autoChords { true };
    std::atomic<float> masterVolume { 0.8f };
    /** Hold a pad yourself (0..15, -1 = none). While held it overrides the song. */
    void setLivePad (int pad) noexcept { livePad.store (pad); }
    int getCurrentPad() const noexcept { return currentPad.load(); }
    int getNumPads() const noexcept { return numPads.load(); }
    double getPpq() const noexcept { return ppqShown.load(); }
    std::array<PartSettings, NumParts> parts;
    bool glideAvailable() const noexcept { return hasCat (GlideCat); }
    int keysGlideIndex() const { return KeysSynth::soundNames().size(); }

    /** Reads Chord-Cat's pad count etc. (message thread, a few times a second). */
    void pollPluginSettings();

    //==== recording (message thread)
    bool startRecording (const juce::File&);
    void stopRecording();
    bool isRecording() const noexcept { return recording.load(); }
    juce::File getRecordingFile() const { return recordFile; }

    //==== rendering
    void prepare (double sampleRate, int maxBlock);
    void render (float* outL, float* outR, int numSamples);

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    //==== state (message thread)
    juce::ValueTree saveState() const;
    void restoreState (const juce::ValueTree&);

private:
    struct PlayHead : public juce::AudioPlayHead
    {
        double ppq = 0.0, bpm = 120.0, sampleRate = 48000.0;
        juce::int64 samples = 0;
        bool playing = false;
        juce::Optional<PositionInfo> getPosition() const override;
    };

    void processCat (int id, juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi);
    void trackNotes (int part, const juce::MidiBuffer& midi);
    void stopAllNotes();
    void renderBlock (float* outL, float* outR, int n);

    std::array<std::unique_ptr<juce::AudioPluginInstance>, NumCats> cats;
    std::array<juce::File, NumCats> catFiles;
    juce::AudioPluginFormatManager formats;

    PlayHead playHead;
    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    bool prepared = false;
    juce::CriticalSection renderLock;     // render vs. prepare / release

    std::atomic<bool> playing { false }, stopRequested { false }, startRequested { false };
    std::atomic<int> livePad { -1 }, currentPad { -1 }, numPads { 8 };
    std::atomic<double> ppqShown { 0.0 };
    int heldTrigger = -1;                 // the trigger note we are holding on Chord-Cat
    std::atomic<int> triggerLow { 60 };

    KeysSynth keys;
    BassSynth bass;
    DrumKit drums;
    juce::Reverb reverb;
    cat::SafetyLimiter limiter;

    juce::AudioBuffer<float> catAudio, partAudio, sendAudio, mixAudio;
    juce::MidiBuffer songMidi, chordOut, scratchMidi;

    // recording
    juce::TimeSliceThread writerThread { "Cat Hub recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    juce::CriticalSection writerLock;
    std::atomic<bool> recording { false };
    juce::File recordFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HubEngine)
};
} // namespace hub

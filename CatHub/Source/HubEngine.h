#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "CatSounds.h"
#include "SafetyLimiter.h"
#include "Composer.h"
#include <array>
#include <atomic>

namespace hub
{
/** Every Cat the Hub can load. The first four play once for the whole jam; the others are
    loaded per track when a track uses them (as a MIDI effect or as its sound). */
enum CatId { ChordCat = 0, BassCat, DrumCat, LifeCat, PattCat, BendCat, OrbitCat, GlideCat, SingCat, TalkCat, NumCats };
const char* catName (int id);           // "Chord-Cat" ...

/** Where a track's notes come from. */
enum Source { SrcChords = 0, SrcBass, SrcDrums, SrcLife, SrcLead, SrcCounter, NumSources };
const char* sourceName (int);           // "Chords" ...
int catForSource (int);                 // ChordCat ... (Lead -> ChordCat)

/** The Cat between the notes and the sound (a MIDI effect). */
enum MidiFx { FxNone = 0, FxPatt, FxBend, FxOrbit, NumMidiFx };
const char* midiFxName (int);           // "None", "Patt-Cat (patterns)" ...
int catForMidiFx (int);                 // -1, PattCat ...

/** What the notes play: built-in sounds, a sound Cat, or Life-Cat's own sound. */
enum Sound
{
    SndPad = 0, SndEPiano, SndPluck, SndStrings,      // keys
    SndSub, SndSaw, SndReese, SndBassPluck,           // bass
    SndBoomBap, Snd808, SndLofi, SndLive,             // drum kits
    SndGlide, SndSing, SndTalk,                       // sound Cats
    SndLifeOwn,                                       // Life-Cat's own sound (Life source only)
    SndChip, SndBell,                                 // more keys (added later, so saved jams keep their numbers)
    NumSounds
};
/** A keys sound by its number in the keys synth (0 pad .. 5 glass bell). */
int keysSound (int keysIndex);
bool isKeysSound (int s);
const char* soundName (int);
int catForSound (int);                  // -1 for built-in
juce::StringArray soundGroups();        // for menus: "Keys", "Bass", "Drums", "Cats"
int soundGroup (int sound);             // 0..4

constexpr int maxTracks = 8;

/** One track (a row in the rack). Settings are atomics: the UI writes, the audio thread reads. */
struct Track
{
    std::atomic<int> source { SrcChords }, midiFx { FxNone }, sound { SndPad };
    std::atomic<float> volume { 0.75f }, pan { 0.5f }, reverb { 0.15f };
    std::atomic<float> filter { 0.0f }, drive { 0.0f }, chorus { 0.0f }, delay { 0.0f }, crush { 0.0f };
    std::atomic<bool> mute { false }, solo { false };
    std::atomic<float> meter { 0.0f };
    std::atomic<double> energy { 0.0 };                 // self test
    std::array<std::atomic<uint32_t>, 4> notes {}, hits {};

    /** Notes held now, plus (for drums) anything hit since the last call. Message thread. */
    std::vector<int> takeNotes();
    void copySettingsFrom (const Track&);

    // ---- owned by the engine (audio thread, swapped under the render lock)
    KeysSynth keys;
    BassSynth bass;
    DrumKit drums;
    TrackFx fx;
    std::unique_ptr<juce::AudioPluginInstance> fxCat, soundCat;   // per-track Cats
    int fxCatId = -1, soundCatId = -1;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> audio;
    juce::SortedSet<int> leadHeld;
    int leadNote = -1;
};

//==============================================================================
/**
    Hosts the Cat VST3s and plays them like a little studio: the song steps through Chord-Cat's
    pads; Bass-Cat, Drum-Cat and Life-Cat follow; every track takes notes from one of them, can
    run them through a MIDI-effect Cat (Patt / Bend / Orbit), plays them with a built-in sound or
    a sound Cat (Glide / Sing / Talk), then filter, drive, chorus, echo, crush, room, pan, volume.
*/
/** The master bus: tone (tilt EQ), glue (a gentle compressor), warmth (tape-like saturation), width. */
struct MasterFx
{
    void prepare (double sampleRate);
    void process (float* l, float* r, int n, float tone, float glue, float warmth, float width);
    float gainReduction() const noexcept { return lastReduction; }
private:
    double sr = 48000.0;
    float lpL = 0, lpR = 0, env = 0, gain = 1.0f, lastReduction = 0;
};

class HubEngine : public juce::AudioIODeviceCallback, public juce::MidiInputCallback
{
public:
    HubEngine();
    ~HubEngine() override;

    //==== Cats (message thread)
    static juce::Array<juce::File> defaultSearchFolders (const juce::File& extraFolder);
    /** Finds every Cat and loads the four core ones; returns what is missing. */
    juce::String loadCats (const juce::Array<juce::File>& folders);
    bool catAvailable (int id) const { return catFiles[(size_t) id] != juce::File(); }
    juce::AudioPluginInstance* getCoreCat (int id) const { return id < PattCat ? core[(size_t) id].get() : nullptr; }
    bool hasCat (int id) const { return getCoreCat (id) != nullptr; }
    void releaseCats();

    //==== tracks (message thread). Structural changes close the plug-in windows first (the UI does).
    int getNumTracks() const noexcept { return numTracks.load(); }
    Track& track (int i) { return *tracks[(size_t) juce::jlimit (0, maxTracks - 1, i)]; }
    bool addTrack (int source, int midiFx, int sound);
    void removeTrack (int index);
    /** Sets the MIDI effect / sound, loading the Cat they need. Returns false if it is missing. */
    bool setMidiFx (int track, int fx);
    bool setSound (int track, int sound);
    void setDefaultTracks();

    //==== song (any thread)
    void setPlaying (bool shouldPlay);
    bool isPlaying() const noexcept { return playing.load(); }
    std::atomic<double> bpm { 92.0 };
    std::atomic<int> barsPerChord { 1 };
    std::atomic<bool> autoChords { true };
    std::atomic<float> masterVolume { 0.8f };
    // the master effects (Master FX button)
    std::atomic<float> masterRoom { 0.5f }, masterTone { 0.0f }, masterGlue { 0.0f }, masterWarmth { 0.0f }, masterWidth { 1.0f };
    float getMasterReduction() const noexcept { return masterReduction.load(); }

    //==== a MIDI keyboard: notes from every enabled MIDI input play one track (-1 = the Lead, or the first keys track)
    std::atomic<int> keyboardTrack { -1 };
    int getKeyboardTrackNow() const noexcept { return keyboardTrackNow.load(); }
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    /** Plays a note from the screen / tests as if it came from a keyboard. */
    void playKeyboardNote (const juce::MidiMessage& m) { if (prepared) liveMidiIn.addMessageToQueue (m); }
    void setLivePad (int pad) noexcept { livePad.store (pad); }
    int getCurrentPad() const noexcept { return currentPad.load(); }
    int getNumPads() const noexcept { return numPads.load(); }
    double getPpq() const noexcept { return ppqShown.load(); }
    float getMasterMeter() const noexcept { return masterMeter.load(); }

    //==== Chord-Cat's key and progression (message thread)
    juce::StringArray getProgressionNames() const;
    int getProgression() const;
    void setProgression (int index);
    int getKey() const;                   // 0 = C .. 11 = B (sounding key)
    void setKey (int pitchClass);
    void pollPluginSettings();
    void keepLifeRunning();
    double getLifeTime() const;

    //==== styles and dice (message thread; the UI closes plug-in windows first)
    static juce::StringArray styleNames();
    void applyStyle (int index);
    void roll (juce::Random&);            // the dice: a new jam
    juce::String currentStyle;
    /** The last Cat that could not be loaded (shown in the status line); cleared by the UI. */
    juce::String lastLoadError;

    //==== composed songs (message thread). In song mode the tracks play the composer's parts:
    //     Chords, Bass line, Drums, Lead = the melody, Counter-melody (Life-Cat follows the song's chords).
    /** Writes a song and plays it. newTracks: also sets up parts and sounds that suit the genre. */
    void composeSong (const cat::muse::Settings&, bool newTracks, juce::Random* dice = nullptr);
    /** Renews one thing in the song (see cat::muse::Target) and keeps playing. */
    void regenerateSong (int target, juce::uint32 seed);
    /** The arranger: plays the song with these sections (see cat::muse::arrange). Not in endless mode. */
    bool arrangeSong (std::vector<cat::muse::Section> sections);
    void setSongEndless (bool endless);
    void leaveSongMode();
    bool isSongMode() const noexcept { return songMode.load(); }
    std::shared_ptr<const cat::muse::Song> getSong() const { return songPlayer.getSong(); }
    cat::muse::Settings getSongSettings() const { return songSettings; }
    double getSongBeat() const noexcept { return songBeatShown.load(); }
    /** Timer (message thread): endless mode composes ahead; Talk- / Sing-Cat get the words to sing. */
    void tickSong();
    /** Renders the song (one piece in endless mode) to a WAV file, faster than real time. The audio
        device must not be calling the engine meanwhile. onlyTrack >= 0: that part alone (a stem). */
    bool exportSong (const juce::File&, int onlyTrack = -1, std::function<bool (float)> progress = {});
    bool exportSongMidi (const juce::File&) const;
    juce::String getSungLyrics() const { return sungLyrics; }   // what the singing Cats were given (tests)

    //==== recording (message thread)
    bool startRecording (const juce::File&);
    void stopRecording();
    bool isRecording() const noexcept { return recording.load(); }
    juce::File getRecordingFile() const { return recordFile; }

    //==== rendering
    void prepare (double sampleRate, int maxBlock);
    void render (float* outL, float* outR, int numSamples);
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

    std::unique_ptr<juce::AudioPluginInstance> createCat (int id, juce::String& error);
    void prepareCat (juce::AudioPluginInstance&);
    void prepareTrack (Track&);
    void runCat (juce::AudioPluginInstance*, juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi, int n);
    void renderBlock (float* outL, float* outR, int n);
    void makeLead (Track&, const juce::MidiBuffer& chords, juce::MidiBuffer& out);
    static void trackNotes (Track&, const juce::MidiBuffer&);
    float bendRangeOf (juce::AudioPluginInstance*) const;

    std::array<std::unique_ptr<juce::AudioPluginInstance>, 4> core;     // Chord, Bass, Drum, Life
    std::array<juce::File, NumCats> catFiles;
    juce::AudioPluginFormatManager formats;
    std::array<std::unique_ptr<Track>, maxTracks> tracks;
    std::atomic<int> numTracks { 0 };

    PlayHead playHead;
    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    bool prepared = false;
    juce::CriticalSection renderLock;

    std::atomic<bool> playing { false }, stopRequested { false }, startRequested { false };
    std::atomic<int> livePad { -1 }, currentPad { -1 }, numPads { 8 }, triggerLow { 60 };
    std::atomic<double> ppqShown { 0.0 };
    std::atomic<float> masterMeter { 0.0f };
    int heldTrigger = -1;

    juce::Reverb reverb;
    MasterFx masterFx;
    std::atomic<float> masterReduction { 0.0f };
    juce::MidiMessageCollector liveMidiIn;
    juce::MidiBuffer liveMidi;
    std::atomic<int> keyboardTrackNow { -1 };
    int pickKeyboardTrack() const;
    cat::SafetyLimiter limiter;
    juce::AudioBuffer<float> catAudio, lifeAudio, sendAudio, mixAudio;
    juce::MidiBuffer songMidi, chordOut, bassOut, drumOut, lifeOut, leadOut, melodyOut, counterOut;

    // composed song
    void setSong (std::shared_ptr<const cat::muse::Song>);
    void setUpSongTracks (int genre, juce::Random* dice);
    void sendLyrics (const cat::muse::Song&);
    cat::muse::SongPlayer songPlayer;
    cat::muse::Settings songSettings;
    std::atomic<bool> songMode { false };
    std::array<juce::MidiBuffer, cat::muse::NumParts> songParts;
    double clockPpq = 0.0;
    std::atomic<double> songBeatShown { 0.0 };
    const cat::muse::Song* lyricsSentFor = nullptr;
    std::vector<cat::muse::Section> arrangedForm;   // the arranger's sections (saved with the jam)
    juce::String sungLyrics;

    juce::TimeSliceThread writerThread { "Cat Hub recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    juce::CriticalSection writerLock;
    std::atomic<bool> recording { false };
    juce::File recordFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HubEngine)
};
} // namespace hub

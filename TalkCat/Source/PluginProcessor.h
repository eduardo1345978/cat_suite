#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "VoiceEngine.h"
#include "Score.h"
#include "HostSync.h"
#include "MidiLearn.h"
#include "MidiScheduler.h"

/**
    Talk-Cat: a vocaloid-style singer (people and cats) with its own piano roll.

    SCORE: notes + lyrics drawn in the editor play in sync with Cubase (consonants start before the
           beat so the vowel lands on it).
    LIVE:  MIDI from a Cubase track or a keyboard sings the next syllable of the live lyrics.
    Both can run together. English, Slovak, Japanese (romaji) and Cat language.
*/
class TalkCatProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    TalkCatProcessor();
    ~TalkCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Talk-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentVoice.load()); }
    void setCurrentProgram (int i) override { applyVoicePreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    cat::MidiLearnManager midiLearn { *this };
    cat::NoteMonitor noteMonitor;
    talkcat::Engine engine;

    // ---- voice presets (the preset bar) and demo songs
    struct VoicePreset { const char* name; const char* category; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<VoicePreset>& voicePresets();
    void applyVoicePreset (int index);
    std::atomic<int> currentVoice { 0 };
    void surprise();
    juce::MemoryBlock surpriseUndo;
    cat::PresetManager presetManager { *this, "Talk-Cat",
        { [] { juce::StringArray n; for (auto& p : voicePresets()) n.add (p.name); return n; },
          [this] (int i) { applyVoicePreset (i); }, [this] { return currentVoice.load(); } } };

    struct DemoSong { const char* name; int language; int style; double bpm; const char* notes; };   // "pitch:beats:lyric ..."
    static const std::vector<DemoSong>& demoSongs();
    void loadDemoSong (int index);

    // ---- score (message thread owns it; the audio thread plays a compiled copy)
    talkcat::Score getScore() const;
    void setScore (const talkcat::Score&, bool undoable = true);
    bool undo();
    bool redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    std::atomic<uint32_t> scoreVersion { 1 };
    talkcat::Language language() const;

    // ---- live
    void setLiveLyrics (const juce::String&);
    juce::String getLiveLyrics() const { return liveLyricsText; }
    void restartLiveLyrics() { liveRestart = true; }
    std::atomic<int> uiLiveSyllable { -1 };
    std::atomic<bool> recordArmed { false };
    /** Sings a note right now (piano-roll preview / clicking the keyboard). */
    void preview (int pitch, const juce::String& lyric, int language = -1);
    void previewStop();

    // ---- UI
    std::atomic<double> uiPpq { 0.0 };
    std::atomic<double> uiScoreBeat { -1.0 };        // playhead in score beats (-1 = not playing)
    std::atomic<bool> uiTransportRunning { false };
    std::atomic<double> uiBpm { 120.0 };
    std::atomic<int> uiActiveNote { -1 };            // id of the note being sung

    juce::MidiFile exportMidi() const;
    double getHostBpm() const { return uiBpm.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    talkcat::Settings readSettings() const;
    void handleLiveMidi (const juce::MidiMessage&, int samplePos);
    void playScore (int numSamples, const cat::TransportState&);
    void timerCallback() override;
    void recompile();
    float p (const char* id) const;

    std::map<juce::String, std::atomic<float>*> refs;
    cat::HostSync hostSync;
    juce::AudioBuffer<float> work;

    // score
    talkcat::Score score;
    mutable juce::CriticalSection scoreLock;
    std::vector<talkcat::Score> undoStack, redoStack;
    juce::SpinLock swapLock;
    std::unique_ptr<talkcat::CompiledScore> active, incoming;
    bool hasIncoming = false;
    std::atomic<bool> recompilePending { true };
    int lastLanguage = -1;

    // score player (audio thread)
    struct Event { int sample; int type; int note; };      // type 0 start, 1 release, 2 breath
    std::vector<Event> events;
    double expectedBeat = -1.0e9;
    int playingNote = -1;
    bool wasPlaying = false;
    double lastBpm = 120.0;

    // live
    juce::String liveLyricsText { "la la la" };
    juce::SpinLock liveLock;
    std::vector<talkcat::Syllable> liveSyllables, liveSyllablesAudio;
    bool liveChanged = true;
    int livePos = -1;
    std::atomic<bool> liveRestart { false };
    struct Held { int note; float vel; };
    std::vector<Held> liveHeld;
    talkcat::Syllable liveSyl {};

    // preview requests (message -> audio)
    struct PreviewReq { int pitch; talkcat::Syllable syl; bool stop; };
    juce::SpinLock previewLock;
    std::vector<PreviewReq> previewQueue, previewAudio;
    int previewSamplesLeft = -1;
    bool previewActive = false;

    // recording (audio -> message)
    struct RecEvent { double beat; int pitch; float vel; bool on; };
    juce::AbstractFifo recFifo { 512 };
    std::array<RecEvent, 512> recBuffer {};
    std::map<int, std::pair<double, float>> recOpen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TalkCatProcessor)
};

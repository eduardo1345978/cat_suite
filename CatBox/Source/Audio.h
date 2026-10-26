#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "Project.h"
#include "Engine.h"
#include "NoteFx.h"
#include "MixFx.h"
#include <atomic>
#include <cstring>
#include <limits>
#include <memory>

/**
    Cat Box 2's sound: the clock and the eight tracks (sequencer > note FX > engine > mixer),
    the sends and the master. The controller hands over the whole project (a copy) whenever
    it changes, and posts the notes played live; the audio thread reports back what it plays
    and what Rec wrote. Used by the sound card and by the self test (render()).
*/
namespace catbox
{
struct Event
{
    enum Type : uint8_t { NoteOn, NoteOff, Play, Stop, RecOn, RecOff, AllOff, Bend, Mod, Preview };
    Type type = NoteOn;
    int8_t track = 0;
    int16_t note = 0;
    float value = 0.0f;   // velocity 0..1, bend in semitones, mod 0..1
};

/** What Rec wrote (the controller writes the same into its project). */
struct RecNote
{
    enum Kind : uint8_t { DrumHit, NoteAdd, NoteLength, SlideOn };
    Kind kind = NoteAdd;
    int8_t track = 0;
    int16_t step = 0, note = 0, value = 0;   // value: velocity (1..127) or length (% of a step)
    bool accent = false;
};

class Audio : public juce::AudioIODeviceCallback
{
public:
    Audio();
    ~Audio() override;

    void prepare (double sampleRate, int maxBlock);
    /** Adds the box's sound to l / r (n samples). */
    void render (float* l, float* r, int n);

    /** The controller's project (copied). atBar: wait for the next bar while playing (scenes). */
    void push (const Project&, bool atBar = false);
    void post (const Event&);
    bool popRec (RecNote&);

    bool isPlaying() const noexcept { return playing.load(); }
    bool isRecording() const noexcept { return recording.load(); }
    void setRecording (bool r) noexcept { recording = r; }
    /** The PC linked to a box shows the box's transport. */
    void mirrorTransport (bool play, bool rec) { playing = play; recording = rec; }
    /** Linked: the PC itself stays silent. */
    void setSilent (bool s) { silent = s; }

    // ---- for the screen (written by the audio thread)
    std::array<std::atomic<int>, numTracks> stepNow;        // -1 = stopped
    std::array<std::atomic<float>, numTracks> level;        // peak since last read (the UI takes it)
    std::array<std::atomic<uint32_t>, numTracks> hits;      // drum lanes / notes that played (bits), the UI clears
    std::atomic<float> peak { 0.0f };
    std::atomic<double> songBeats { 0.0 };                   // quarter notes since Play
    std::atomic<bool> sceneWaiting { false };
    std::atomic<int> viewTrack { 0 };                        // whose planets / arp the screen shows
    struct PlanetView { std::atomic<bool> active { false }; std::atomic<int> note { 0 }; std::atomic<float> theta { 0 }, radius { 0 }, mass { 0 }; };
    std::array<PlanetView, orbitcat::maxBodies> planets;
    static constexpr int scopeSize = 512;
    std::array<std::atomic<float>, scopeSize> scope;
    std::atomic<int> scopeWrite { 0 };

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int n,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}

private:
    struct Queued { double ppq; int16_t note; float vel; bool on, slide, accent; };

    struct TrackRt : public NoteSink
    {
        std::array<std::unique_ptr<Engine>, NumEngineTypes> engines;
        Engine* engine = nullptr;
        int engineType = -1;
        std::array<float, maxParams> applied {};
        std::array<float, numFxParams> fxApplied {};
        int fxType = -1, fxKey = -1, fxScale = -1;
        NoteFx fx;
        std::array<Queued, 128> queue {};
        int numQueued = 0;
        // sequencer
        double nextStep = 0.0;
        int step = 0;
        int lastSlideNote = -1;
        // live
        std::array<bool, 128> liveDown {};
        int liveCount = 0;
        std::array<double, 128> recStart {};
        std::array<int, 128> recStep {};
        int lastRecNote = -1;
        struct Skip { int step, note; double until; };
        std::array<Skip, 16> skips {};
        int numSkips = 0;
        // mixer
        float gainL = 0.0f, gainR = 0.0f;
        bool trackDrums = false;
        uint32_t hitBits = 0;

        void fxNoteOn (int note, float vel, bool slide, bool accent) override;
        void fxNoteOff (int note) override;
    };

    void applyIncoming (bool atBarBoundary);
    void handleEvents();
    void syncTrack (int t);
    void queueNote (TrackRt&, double ppq, int note, float vel, bool on, bool slide, bool accent);
    void runQueue (TrackRt&, double upTo);
    void runSequencer (int t, double from, double to);
    void liveNoteOn (int t, int note, float vel);
    void liveNoteOff (int t, int note);
    void startPlaying();
    void stopPlaying();
    void pushRec (const RecNote&);
    void updateViews();

    double sr = 48000.0;
    int maxBlock = 512;
    Project live;                       // the audio thread's copy
    juce::SpinLock lock;
    std::unique_ptr<Project> incoming;   // waiting for the audio thread
    bool hasIncoming = false, incomingAtBar = false;

    juce::AbstractFifo eventFifo { 512 };
    std::array<Event, 512> events;
    juce::AbstractFifo recFifo { 256 };
    std::array<RecNote, 256> recs;

    std::array<TrackRt, numTracks> tracks;
    double clock = 0.0;                 // quarter notes, always running
    double playStart = 0.0;             // the clock when Play was pressed (bar 1)
    std::atomic<bool> playing { false }, recording { false };
    bool wasPlaying = false, silent = false;

    std::vector<float> tl, tr_, ml, mr, al, ar, bl, br, fa, fb;
    SendReverb reverb;
    SendDelay delay;
    MasterBus master;
    float clickPhase = 0.0f, clickEnv = 0.0f, clickHz = 1000.0f;
    int scopeDecim = 0;
    juce::Random rng;
};
} // namespace catbox

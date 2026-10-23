#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "BoxModel.h"
#include "ArpEngine.h"
#include "AcidSynth.h"
#include "SuperSaw.h"
#include "MasterFx.h"
#include "CatSounds.h"
#include "SafetyLimiter.h"
#include <atomic>

namespace catbox
{
/** Everything the audio thread plays, built on the message thread from BoxData (fixed size: no allocation). */
struct AudioSnapshot
{
    double bpm = 110.0;
    float masterGain = 0.8f;
    std::array<bool, NumInstruments> muted {};
    std::array<float, NumInstruments> volume { 0.85f, 0.85f, 0.8f };
    std::array<hub::TrackFxSettings, NumInstruments> fx {};

    std::array<std::array<uint8_t, maxSteps>, numPads> drumSteps {};
    std::array<float, numPads> drumLevel {}, drumChance {};
    int drumLength = 16, drumKit = 0;
    float drumSwing = 0.0f, drumHuman = 0.0f;

    std::array<BassStep, maxSteps> bassSteps {};
    int bassLength = 16, bassOctave = 0, bassSound = 0;
    float bassSwing = 0.0f;
    AcidSynth::Params acid;

    ArpSettings arp;
    int arpSound = 2;
    bool arpOn = true;          // false = the Arp page plays like a plain synth
    bool bassFollow = false;    // the bass line moves to the root of the chord held on the Arp
    int key = 0;

    MasterSettings master;
};

/** Small events from the pads / keyboard to the audio thread. */
struct BoxEvent
{
    enum Type : uint8_t { DrumHit, ArpOn, ArpOff, BassOn, BassOff, ArpLatch, ArpClear, AllOff, PerfOn, PerfOff } type = DrumHit;
    int a = 0, b = 0, c = 0;
};

/** A note played in while Rec runs, handed back to the message thread to be written into the pattern. */
struct RecordedNote
{
    int instrument = Drums;
    int step = 0;
    int value = 0;          // Drums: pad, Bass: note
    int velocity = 100;
    int slideFrom = -1;     // Bass: played legato (another pad still held) -> that note's step slides into this one
};

/**
    The Cat Box sound engine: one clock and three instruments. Drums and Bass are step
    sequencers on a 1/16 grid, the Arp runs on its own division. Each instrument has its own
    effects; the safety limiter keeps the output clean. Used by the sound card and the self test.
*/
class BoxAudio : public juce::AudioIODeviceCallback
{
public:
    BoxAudio();

    void prepare (double sampleRate, int maxBlock);
    void render (float* left, float* right, int numSamples);   // the audio thread

    // any thread
    /** atBar = wait for the next bar while playing (scenes), else right away. */
    void setSnapshot (const AudioSnapshot&, bool atBar = false);
    void post (const BoxEvent&);
    void setPlaying (bool);
    bool isPlaying() const noexcept { return playing.load(); }
    void setRecording (bool r) noexcept { recording = r; }
    bool isRecording() const noexcept { return recording.load(); }
    bool popRecorded (RecordedNote&);
    double getSampleRate() const noexcept { return sr; }

    /** Mirror mode (the PC app linked to a box): no sound here, the screen atomics come from the box. */
    void setMirror (bool m) noexcept { mirror = m; }
    bool isMirror() const noexcept { return mirror.load(); }
    void mirrorTransport (bool isPlaying, bool isRecording) noexcept { playing = isPlaying; recording = isRecording; }

    // what the screen shows
    std::atomic<int> drumStep { -1 }, bassStep { -1 }, arpStep { -1 };
    std::atomic<uint64_t> arpNotesLo { 0 }, arpNotesHi { 0 };
    std::atomic<uint32_t> drumFlash { 0 };   // bit per pad, set when it plays (the screen clears it)
    std::atomic<uint32_t> drumFlashLink { 0 };   // the same, read by the link to a PC
    std::atomic<int> bassNoteNow { -1 };
    std::atomic<float> peak { 0.0f };
    std::atomic<bool> arpLatched { false };
    std::array<std::atomic<float>, 32> scope {};   // a few recent output values for the screen
    static constexpr int spectrumSize = 1024;
    std::array<std::atomic<float>, spectrumSize> spectrumRing {};   // the output at half rate, for the spectrum
    std::atomic<int> spectrumWrite { 0 };
    std::atomic<uint32_t> perfHeld { 0 };           // FX page pads held
    std::atomic<float> pumpNow { 0.0f };
    std::atomic<bool> sceneWaiting { false };

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputs, int numOutputs,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}

private:
    void handleEvents();
    void tick (double from, double to);
    void playDrumStep (int64_t k, double from, double to);
    void playBassStep (int64_t k, double from, double to);
    void bassOn (int note, int velocity, bool accent, bool slide);
    void arpNoteOn (int note, int velocity);
    void arpNoteOff (int note);
    void arpAllOff();
    int followOffset() const noexcept;
    void bassOff (int note);
    void scheduleOff (int instrument, int note, double offPpq);
    void cancelOffs (int instrument, int note);
    void releaseDue (double upTo);
    void allNotesOff();
    void flashDrum (int note);
    void pushRecorded (const RecordedNote&);
    static float hashNoise (int64_t k, int salt) noexcept;

    double sr = 48000.0;
    double clock = 0.0;            // quarter notes since start (always runs)
    double songStart = 0.0;        // where step 1 of Play is
    std::atomic<bool> playing { false }, recording { false };
    std::atomic<bool> startRequest { false }, stopRequest { false }, mirror { false };
    bool running = false;

    hub::DrumKit drums;
    hub::KeysSynth keys;
    hub::BassSynth bassSynth;
    AcidSynth acid, arpAcid;
    SuperSaw supersaw;
    MasterFx master;
    std::array<int, 64> kickOffsets {};
    int numKicks = 0, subOffset = 0;
    std::array<hub::TrackFx, NumInstruments> fx;
    cat::SafetyLimiter limiter;
    ArpEngine arp;
    juce::Random rnd;
    int liveBassHeld = 0, lastLiveBass = -1, lastRecordedBassStep = -1;

    struct PendingOff { int instrument = 0, note = 0; double ppq = 0.0; };
    std::array<PendingOff, 256> offs;
    int numOffs = 0;

    AudioSnapshot live, pending;
    juce::SpinLock snapshotLock;
    bool snapshotFresh = false, snapshotAtBar = false;

    std::array<BoxEvent, 512> events;
    juce::AbstractFifo eventFifo { 512 };
    juce::SpinLock postLock;

    std::array<RecordedNote, 256> recorded;
    juce::AbstractFifo recordFifo { 256 };

    std::vector<float> bufL, bufR;
    int scopeWrite = 0, scopeCount = 0, spectrumCount = 0;
    std::vector<float> mixL, mixR;
};
} // namespace catbox

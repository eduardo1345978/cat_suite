#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "BoxModel.h"
#include "ArpEngine.h"
#include "CatSounds.h"
#include "SafetyLimiter.h"
#include "BassEngine.h"
#include <atomic>

namespace catbox
{
/** A drum hit in the loop (quarter notes from the loop start). */
struct SeqHit
{
    double ppq = 0.0;
    int note = 36;
    int velocity = 100;
};

/** Everything the audio thread plays, built on the message thread from BoxData. */
struct AudioSnapshot
{
    double bpm = 110.0;
    float master = 0.8f;
    std::array<bool, NumInstruments> muted {};
    std::array<float, NumInstruments> volume { 0.85f, 0.6f, 0.8f };
    std::array<hub::TrackFxSettings, NumInstruments> fx {};

    std::vector<SeqHit> drumHits;      // sorted by ppq
    double drumLoop = 8.0;
    int drumKit = 0;

    std::vector<basscat::BassNote> bassNotes;
    double bassLoop = 8.0;
    int bassSound = 0;
    int bassLowNote = 36;

    ArpSettings arp;
    int arpSound = 2;
};

/** Small events from the pads / keyboard to the audio thread. */
struct BoxEvent
{
    enum Type : uint8_t { DrumHit, ArpOn, ArpOff, BassOn, BassOff, BassChord, ArpLatch, ArpClear, AllOff } type = DrumHit;
    int a = 0, b = 0, c = 0, d = 0, e = 0;
};

/**
    The Cat Box sound engine: one clock, three instruments (Drum-Cat grooves on the drum kit,
    the arp on the keys synth, Bass-Cat lines on the bass synth), each with its own effects,
    then the safety limiter. Used by the audio device and by the offline self test alike.
*/
class BoxAudio : public juce::AudioIODeviceCallback
{
public:
    BoxAudio();

    void prepare (double sampleRate, int maxBlock);
    void render (float* left, float* right, int numSamples);   // the audio thread

    // any thread
    void setSnapshot (const AudioSnapshot&);
    void post (const BoxEvent&);
    void setPlaying (bool);
    bool isPlaying() const noexcept { return playing.load(); }
    void setRecording (bool r) noexcept { recording = r; }
    bool isRecording() const noexcept { return recording.load(); }

    /** Hits played in with Rec, picked up by the message thread: (step, note, velocity). */
    bool popRecordedHit (DrumUserHit&);

    // what the screen shows
    std::atomic<int> drumStep { -1 }, arpStep { -1 }, bassChordRoot { -1 };
    std::atomic<uint64_t> arpNotesLo { 0 }, arpNotesHi { 0 };
    std::atomic<uint32_t> drumFlash { 0 };   // bit per pad, set when it plays (the screen clears it)
    std::atomic<float> peak { 0.0f };
    std::atomic<bool> arpLatched { false };

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputs, int numOutputs,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}

private:
    void handleEvents();
    void tick (double from, double to);
    void playDrums (double from, double to, double songFrom);
    void playBass (double from, double to, double songFrom);
    void scheduleOff (int instrument, int note, double offPpq);
    void releaseDue (double upTo);
    void allNotesOff();
    void flashDrum (int note);

    double sr = 48000.0;
    double clock = 0.0;            // quarter notes since start (always runs)
    double songStart = 0.0;        // where the bar 1 of Play is
    std::atomic<bool> playing { false }, recording { false };
    std::atomic<bool> startRequest { false }, stopRequest { false };
    bool running = false;

    hub::DrumKit drums;
    hub::KeysSynth keys;
    hub::BassSynth bass;
    std::array<hub::TrackFx, NumInstruments> fx;
    cat::SafetyLimiter limiter;
    ArpEngine arp;
    basscat::Chord chord;
    int bassPrev = -1;

    struct PendingOff { int instrument = 0, note = 0; double ppq = 0.0; };
    std::array<PendingOff, 256> offs;
    int numOffs = 0;

    AudioSnapshot live, pending;
    juce::SpinLock snapshotLock;
    bool snapshotFresh = false;

    std::array<BoxEvent, 512> events;
    juce::AbstractFifo eventFifo { 512 };
    juce::SpinLock postLock;

    std::array<DrumUserHit, 128> recorded;
    juce::AbstractFifo recordFifo { 128 };

    std::vector<float> bufL, bufR;
};
} // namespace catbox

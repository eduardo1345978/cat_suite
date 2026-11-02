#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Tunings.h"
#include <array>
#include <atomic>

namespace micro
{
/** What the engine plays with (read from the parameters every block). */
struct Settings
{
    int rootKey = 60;
    double rootHz = 261.6255653;
    double bendRange = 48.0;           // semitones; must match the synth's MPE pitch-bend range
    int voices = 15;                   // member channels 2 .. 16
    bool sendConfig = true;            // the MPE Configuration Message + bend range (RPN 0)
    bool bypass = false;               // play as it comes in (normal 12-EDO, no MPE)
};

/**
    Turns incoming notes into tuned MPE notes: every note gets its own channel (2-16) and a pitch bend
    from the nearest normal note to its tuned pitch, sent just before the note-on. Note-offs follow the
    note to its channel. Pitch wheel, sustain and other controllers go to the master channel (1), which an
    MPE synth applies to every note; polyphonic aftertouch becomes pressure on the note's own channel.
    Fixed-size tables only: safe on the audio thread.
*/
class Engine
{
public:
    Engine() { reset(); }
    void reset();
    void setTuning (const TuningData& t) noexcept { tuning = t; }
    const TuningData& getTuning() const noexcept { return tuning; }
    /** Sends the MPE set-up again (at the next block): when loaded, when the transport starts. */
    void requestConfig() noexcept { configPending = true; }
    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, const Settings&);
    /** All notes off on every channel (e.g. before changing the number of voices). */
    void panic (juce::MidiBuffer& out, int pos);

    // for the screen (any thread)
    std::array<std::atomic<bool>, 128> held {};
    std::atomic<int> notesPlaying { 0 };
    std::atomic<int> configsSent { 0 };

    static int bendValue (double semitones, double range) noexcept;

private:
    void sendConfig (juce::MidiBuffer& out, int pos, const Settings&);
    int allocate (int noteId, int voices, int& stolen) noexcept;
    void release (int noteId) noexcept;

    TuningData tuning;
    std::array<int, 16> channelNote {};                 // member channel index -> noteId (-1 = free)
    std::array<juce::uint64, 16> startOrder {}, releaseOrder {};
    std::array<int8_t, 16 * 128> noteChannel {};        // noteId -> output channel (-1 = none)
    std::array<int8_t, 16 * 128> noteOut {};            // noteId -> the note number sent
    juce::uint64 counter = 0;
    bool configPending = true;
    double lastRange = -1.0;
    int lastVoices = -1;
    bool lastBypass = false;
};
} // namespace micro

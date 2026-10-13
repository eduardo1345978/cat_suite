#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <limits>
#include <vector>

namespace cat
{
/** Lock-free view of the notes a plug-in is currently sending, for the on-screen keyboards. */
struct NoteMonitor
{
    NoteMonitor() { clear(); }

    void clear() noexcept
    {
        for (auto& v : velocity) v.store (0, std::memory_order_relaxed);
        for (auto& c : channel)  c.store (0, std::memory_order_relaxed);
    }

    void noteOn (int ch, int note, int vel) noexcept
    {
        velocity[(size_t) note].store ((uint8_t) juce::jlimit (1, 127, vel), std::memory_order_relaxed);
        channel[(size_t) note].store ((uint8_t) ch, std::memory_order_relaxed);
    }

    void noteOff (int note) noexcept { velocity[(size_t) note].store (0, std::memory_order_relaxed); }

    std::array<std::atomic<uint8_t>, 128> velocity;
    std::array<std::atomic<uint8_t>, 128> channel;
};

/**
    Time-ordered queue of outgoing MIDI events in absolute sample time.

    Lets the plug-ins schedule events beyond the current block (strum, humanise,
    gate lengths, ratchets) with zero added latency, and keeps note-on/off pairs
    balanced per channel so chained plug-ins never produce stuck or doubled notes.
    All memory is reserved in prepare(); nothing allocates on the audio thread
    unless the reserved capacity is exceeded.
*/
class MidiScheduler
{
public:
    void prepare (size_t capacity = 8192);
    void clear() noexcept;

    void schedule (int64_t sampleTime, const juce::MidiMessage& message);
    void scheduleNote (int64_t onTime, int64_t offTime, int channel, int note, int velocity);

    /**
        Removes note-ons for this channel/note that have not been sent yet (and their paired
        note-offs). Only note-ons scheduled at or after fromTime are removed.
    */
    void cancelPendingNoteOns (int channel, int note, int64_t fromTime = std::numeric_limits<int64_t>::min());

    /**
        Ends a note at the given time: cancels note-ons scheduled at or after that time, pulls
        pending note-offs forward to it, or schedules a note-off if the note is sustaining
        without one. Note-ons scheduled before the time (e.g. earlier in the current block)
        still play and are cut at the given time.
    */
    void releaseNote (int channel, int note, int64_t time);

    /** Moves every pending note-off for channel/note to the given time (or earlier). */
    void rescheduleNoteOffs (int channel, int note, int64_t newTime);

    /** Writes all events due in [blockStart, blockStart + numSamples) into out. */
    void emit (juce::MidiBuffer& out, int64_t blockStart, int numSamples);

    /** Sends note-off for everything sounding and drops all pending note events. */
    void allNotesOff (juce::MidiBuffer& out, int samplePosition);

    bool isSounding (int channel, int note) const noexcept;
    size_t getNumPending() const noexcept { return events.size(); }

    NoteMonitor* monitor = nullptr;

private:
    struct Event
    {
        int64_t time;
        int priority;
        uint64_t order;
        juce::MidiMessage message;
    };

    static int priorityFor (const juce::MidiMessage& m) noexcept;

    std::vector<Event> events;
    uint64_t orderCounter = 0;
    std::array<std::array<uint8_t, 128>, 16> counts {};
};

} // namespace cat

#include "MidiScheduler.h"
#include <algorithm>

namespace cat
{
void MidiScheduler::prepare (size_t capacity)
{
    events.reserve (capacity);
    clear();
}

void MidiScheduler::clear() noexcept
{
    events.clear();
    orderCounter = 0;
    for (auto& ch : counts)
        ch.fill (0);
    if (monitor != nullptr)
        monitor->clear();
}

int MidiScheduler::priorityFor (const juce::MidiMessage& m) noexcept
{
    // Same-sample ordering: note-offs first (so a re-struck note is not cut),
    // then controllers / pitch-bend (so per-voice bend precedes its note-on), then note-ons.
    if (m.isNoteOff()) return 0;
    if (m.isNoteOn())  return 2;
    return 1;
}

void MidiScheduler::schedule (int64_t sampleTime, const juce::MidiMessage& message)
{
    Event e { sampleTime, priorityFor (message), orderCounter++, message };

    auto it = std::upper_bound (events.begin(), events.end(), e, [] (const Event& a, const Event& b)
    {
        if (a.time != b.time)         return a.time < b.time;
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.order < b.order;
    });

    events.insert (it, std::move (e));
}

void MidiScheduler::scheduleNote (int64_t onTime, int64_t offTime, int channel, int note, int velocity)
{
    if (note < 0 || note > 127)
        return;

    channel = juce::jlimit (1, 16, channel);
    schedule (onTime, juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, velocity)));
    schedule (juce::jmax (onTime + 1, offTime), juce::MidiMessage::noteOff (channel, note));
}

void MidiScheduler::cancelPendingNoteOns (int channel, int note, int64_t fromTime)
{
    auto matches = [=] (const Event& e, bool on)
    {
        return (on ? e.message.isNoteOn() : e.message.isNoteOff())
            && e.message.getChannel() == channel && e.message.getNoteNumber() == note;
    };

    for (size_t i = 0; i < events.size();)
    {
        if (! matches (events[i], true) || events[i].time < fromTime) { ++i; continue; }

        for (size_t j = i + 1; j < events.size(); ++j)
            if (matches (events[j], false))
            {
                events.erase (events.begin() + (std::ptrdiff_t) j);
                break;
            }

        events.erase (events.begin() + (std::ptrdiff_t) i);
    }
}

void MidiScheduler::releaseNote (int channel, int note, int64_t time)
{
    // Note-ons that are due before `time` (typically earlier in the block being rendered) are
    // kept: cancelling them would silently drop a hit that should already have sounded.
    cancelPendingNoteOns (channel, note, time);

    bool hasPendingOff = false, hasPendingOn = false;
    for (const auto& e : events)
    {
        if (e.message.getChannel() != channel || e.message.getNoteNumber() != note)
            continue;
        hasPendingOff = hasPendingOff || e.message.isNoteOff();
        hasPendingOn = hasPendingOn || e.message.isNoteOn();
    }

    if (hasPendingOff)
        rescheduleNoteOffs (channel, note, time);
    else if (hasPendingOn || isSounding (channel, note))
        schedule (time, juce::MidiMessage::noteOff (channel, note));
}

void MidiScheduler::rescheduleNoteOffs (int channel, int note, int64_t newTime)
{
    int numMoved = 0;

    events.erase (std::remove_if (events.begin(), events.end(), [&] (const Event& e)
    {
        if (e.message.isNoteOff() && e.message.getChannel() == channel && e.message.getNoteNumber() == note && e.time > newTime)
        {
            ++numMoved;
            return true;
        }
        return false;
    }), events.end());

    for (int i = 0; i < numMoved; ++i)
        schedule (newTime, juce::MidiMessage::noteOff (channel, note));
}

void MidiScheduler::emit (juce::MidiBuffer& out, int64_t blockStart, int numSamples)
{
    const int64_t blockEnd = blockStart + numSamples;
    size_t numDone = 0;

    for (; numDone < events.size(); ++numDone)
    {
        auto& e = events[numDone];
        if (e.time >= blockEnd)
            break;

        const int pos = (int) juce::jlimit<int64_t> (0, juce::jmax (0, numSamples - 1), e.time - blockStart);
        const auto& m = e.message;

        if (m.isNoteOn())
        {
            const int ch = m.getChannel() - 1, n = m.getNoteNumber();
            auto& c = counts[(size_t) ch][(size_t) n];

            if (c > 0) // already sounding: re-strike cleanly
                out.addEvent (juce::MidiMessage::noteOff (ch + 1, n), pos);

            c = (uint8_t) juce::jmin (255, c + 1);
            out.addEvent (m, pos);

            if (monitor != nullptr)
                monitor->noteOn (ch + 1, n, m.getVelocity());
        }
        else if (m.isNoteOff())
        {
            const int ch = m.getChannel() - 1, n = m.getNoteNumber();
            auto& c = counts[(size_t) ch][(size_t) n];

            if (c == 0)
                continue;

            if (--c == 0)
            {
                out.addEvent (m, pos);
                if (monitor != nullptr)
                    monitor->noteOff (n);
            }
        }
        else
        {
            out.addEvent (m, pos);
        }
    }

    if (numDone > 0)
        events.erase (events.begin(), events.begin() + (std::ptrdiff_t) numDone);
}

void MidiScheduler::allNotesOff (juce::MidiBuffer& out, int samplePosition)
{
    events.erase (std::remove_if (events.begin(), events.end(), [] (const Event& e)
    {
        return e.message.isNoteOnOrOff();
    }), events.end());

    for (int ch = 0; ch < 16; ++ch)
        for (int n = 0; n < 128; ++n)
            if (counts[(size_t) ch][(size_t) n] > 0)
            {
                out.addEvent (juce::MidiMessage::noteOff (ch + 1, n), samplePosition);
                counts[(size_t) ch][(size_t) n] = 0;
            }

    if (monitor != nullptr)
        monitor->clear();
}

bool MidiScheduler::isSounding (int channel, int note) const noexcept
{
    return counts[(size_t) juce::jlimit (0, 15, channel - 1)][(size_t) juce::jlimit (0, 127, note)] > 0;
}

} // namespace cat

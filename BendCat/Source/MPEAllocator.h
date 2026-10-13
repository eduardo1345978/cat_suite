#pragma once

#include <array>
#include <cstdint>

namespace bendcat
{
/**
    Assigns each incoming voice its own MIDI channel (MPE lower zone: master = 1,
    member channels 2-16). Free channels are reused least-recently-released first so
    release tails keep their bend; when all are busy the oldest voice is stolen.
    Fixed-size tables only, so it is safe to call from the audio thread.
*/
class MPEAllocator
{
public:
    static constexpr int firstChannel = 2;
    static constexpr int maxChannels = 15;   // channels 2-16 (full MPE lower zone)

    MPEAllocator() { reset(); }

    void setNumChannels (int n);
    int getNumChannels() const noexcept { return numChannels; }

    /** Returns the output channel (2-16). If a voice had to be stolen, its noteId is written to stolenNoteId. */
    int allocateChannel (int noteId, int* stolenNoteId = nullptr);
    void releaseChannel (int noteId);
    int getChannelForNote (int noteId) const;
    int getNoteForChannel (int channel) const;
    void reset();

    /** noteId = (inputChannel - 1) * 128 + noteNumber */
    static int makeNoteId (int channel, int note) noexcept { return (channel - 1) * 128 + note; }

    std::array<int, maxChannels> channelMap {};     // noteId using channel (index + 2), -1 = free

private:
    std::array<int8_t, 16 * 128> noteToChannel {};  // noteId -> channel, -1 = none
    std::array<uint64_t, maxChannels> startOrder {};
    std::array<uint64_t, maxChannels> releaseOrder {};
    uint64_t counter = 0;
    int numChannels = maxChannels;
};

} // namespace bendcat

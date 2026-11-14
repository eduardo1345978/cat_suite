#include "MPEAllocator.h"
#include <algorithm>

namespace bendcat
{
void MPEAllocator::setNumChannels (int n)
{
    numChannels = std::clamp (n, 1, maxChannels);
}

void MPEAllocator::reset()
{
    channelMap.fill (-1);
    noteToChannel.fill (-1);
    startOrder.fill (0);
    releaseOrder.fill (0);
    counter = 0;
}

int MPEAllocator::allocateChannel (int noteId, int* stolenNoteId)
{
    if (stolenNoteId != nullptr)
        *stolenNoteId = -1;

    if (noteId < 0 || noteId >= (int) noteToChannel.size())
        return firstChannel;

    if (noteToChannel[(size_t) noteId] >= 0)
        return noteToChannel[(size_t) noteId];

    int best = -1;

    // Free channel that was released longest ago
    for (int i = 0; i < numChannels; ++i)
        if (channelMap[(size_t) i] < 0 && (best < 0 || releaseOrder[(size_t) i] < releaseOrder[(size_t) best]))
            best = i;

    // Otherwise steal the oldest sounding voice
    if (best < 0)
    {
        for (int i = 0; i < numChannels; ++i)
            if (best < 0 || startOrder[(size_t) i] < startOrder[(size_t) best])
                best = i;

        const int victim = channelMap[(size_t) best];
        if (victim >= 0)
        {
            noteToChannel[(size_t) victim] = -1;
            if (stolenNoteId != nullptr)
                *stolenNoteId = victim;
        }
    }

    channelMap[(size_t) best] = noteId;
    startOrder[(size_t) best] = ++counter;
    const int channel = best + firstChannel;
    noteToChannel[(size_t) noteId] = (int8_t) channel;
    return channel;
}

void MPEAllocator::releaseChannel (int noteId)
{
    if (noteId < 0 || noteId >= (int) noteToChannel.size())
        return;

    const int channel = noteToChannel[(size_t) noteId];
    if (channel < 0)
        return;

    noteToChannel[(size_t) noteId] = -1;
    const int idx = channel - firstChannel;
    if (idx >= 0 && idx < maxChannels && channelMap[(size_t) idx] == noteId)
    {
        channelMap[(size_t) idx] = -1;
        releaseOrder[(size_t) idx] = ++counter;
    }
}

int MPEAllocator::getChannelForNote (int noteId) const
{
    if (noteId < 0 || noteId >= (int) noteToChannel.size())
        return -1;
    return noteToChannel[(size_t) noteId];
}

int MPEAllocator::getNoteForChannel (int channel) const
{
    const int idx = channel - firstChannel;
    return (idx >= 0 && idx < maxChannels) ? channelMap[(size_t) idx] : -1;
}

} // namespace bendcat

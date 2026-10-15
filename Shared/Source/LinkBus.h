#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace cat
{
/**
    Cat Link bus: a small block of shared memory (a memory-mapped file in the temp folder) that
    every Cat in the host - and every Cat app on the computer - can read and write without locks
    or files on the audio thread.

      members   which Cats are open right now (for the Link panel)
      key       the shared key + scale (absolute pitch-class mask)
      chord     the chord Chord-Cat is playing (up to 16 notes)
      events    births and deaths from Life-Cat (a ring of the last 64)
      linkAll   a "link everything" request from any Link panel

    Every field is written with a sequence counter around it (seqlock), so readers on the audio
    thread see either the old or the new value, never half of each.
*/
class LinkBus
{
public:
    LinkBus();
    ~LinkBus();

    bool isValid() const noexcept { return shared != nullptr; }

    // ---- members (message thread)
    struct MemberInfo { juce::String plugin; juce::uint64 id = 0; bool self = false; juce::uint32 flags = 0; };
    int join (const juce::String& plugin, juce::uint64 id);
    void heartbeat (int slot, juce::uint64 id, juce::uint32 flags);
    void leave (int slot, juce::uint64 id);
    std::vector<MemberInfo> members (juce::uint64 self) const;

    // ---- key (any thread)
    void publishKey (int root, juce::uint16 absMask, juce::uint64 source);
    bool readKey (juce::uint32& seq, int& root, juce::uint16& absMask, juce::uint64& source) const;   // true if seq changed
    juce::uint32 keySeq() const noexcept;

    // ---- chord (any thread, real-time safe)
    void publishChord (const int* notes, int count, juce::uint64 source);
    bool readChord (juce::uint32& seq, std::array<int, 16>& notes, int& count, juce::uint64& source) const;
    juce::uint32 chordSeq() const noexcept;

    // ---- life events (any thread, real-time safe)
    void publishEvent (int kind, int role, juce::uint64 source);
    juce::uint32 eventCount() const noexcept;
    /** Calls fn (kind, role) for every event after `lastSeen` (from other sources); updates lastSeen. */
    void readEvents (juce::uint32& lastSeen, juce::uint64 self, const std::function<void (int, int)>& fn) const;

    // ---- link all
    void requestLinkAll();
    juce::uint32 linkAllSeq() const noexcept;

    static juce::File busFile();

private:
    struct Shared;
    bool lock() const noexcept;
    void unlock() const noexcept;

    std::unique_ptr<juce::MemoryMappedFile> mapping;
    Shared* shared = nullptr;
};

//==============================================================================
/** Pitch-class helpers for sharing keys between Cats with different scale lists. */
namespace keys
{
    juce::uint16 maskFromSteps (const std::vector<int>& relativeSteps) noexcept;    // relative to the root
    juce::uint16 rotate (juce::uint16 mask, int semitones) noexcept;               // absolute <-> relative
    /** Index of the scale (relative masks) that best matches, -1 if the list is empty. */
    int nearestScale (juce::uint16 wantedRelative, const std::vector<juce::uint16>& candidates) noexcept;
}

} // namespace cat

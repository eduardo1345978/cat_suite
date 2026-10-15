#include "LinkBus.h"

namespace cat
{
namespace
{
    constexpr juce::uint32 magicValue = 0x4341544cu;    // "CATL"
    constexpr juce::uint32 versionValue = 2;
    constexpr int maxMembers = 32;
    constexpr int numEvents = 64;

    template <typename T>
    std::atomic_ref<T> at (T& v) noexcept { return std::atomic_ref<T> (v); }

    juce::int64 nowMs() noexcept { return juce::Time::currentTimeMillis(); }
}

struct alignas (16) LinkBus::Shared
{
    juce::uint32 magic, version, lockWord, reserved;

    struct Member { juce::uint64 id; juce::int64 lastSeen; juce::uint32 flags; char name[20]; };
    Member members[maxMembers];

    juce::uint32 keySeq, keyMask;
    juce::int32 keyRoot, reserved2;
    juce::uint64 keySource;

    juce::uint32 chordSeq;
    juce::int32 chordCount;
    juce::int8 chordNotes[16];
    juce::uint64 chordSource;

    juce::uint32 linkAll, eventWrite;
    struct Ev { juce::uint32 seq; juce::uint8 kind, role, pad1, pad2; juce::uint64 source; };
    Ev events[numEvents];
};

juce::File LinkBus::busFile()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("CatSuiteLink.bus");
}

LinkBus::LinkBus()
{
    const auto f = busFile();
    const size_t size = (sizeof (Shared) + 4095) & ~(size_t) 4095;
    {
        juce::InterProcessLock ipl ("CatSuiteLinkBus");
        const juce::InterProcessLock::ScopedLockType scoped (ipl);
        if (! f.existsAsFile() || f.getSize() < (juce::int64) size)
        {
            f.deleteFile();
            juce::FileOutputStream out (f);
            if (out.openedOk())
            {
                juce::HeapBlock<char> zeros (size, true);
                out.write (zeros.get(), size);
                out.flush();
            }
        }
        mapping = std::make_unique<juce::MemoryMappedFile> (f, juce::MemoryMappedFile::readWrite, false);
        if (mapping->getData() != nullptr && mapping->getSize() >= sizeof (Shared))
        {
            shared = static_cast<Shared*> (mapping->getData());
            if (shared->magic != magicValue || shared->version != versionValue)
            {
                std::memset (shared, 0, sizeof (Shared));
                shared->version = versionValue;
                shared->magic = magicValue;
            }
        }
    }
}

LinkBus::~LinkBus()
{
    shared = nullptr;
    mapping.reset();
}

bool LinkBus::lock() const noexcept
{
    if (shared == nullptr) return false;
    auto l = at (shared->lockWord);
    for (int spin = 0; spin < 2000; ++spin)
    {
        juce::uint32 expected = 0;
        if (l.compare_exchange_weak (expected, 1u, std::memory_order_acquire)) return true;
    }
    return false;   // someone holds it for ages (crashed?): give up rather than block audio
}

void LinkBus::unlock() const noexcept
{
    if (shared != nullptr) at (shared->lockWord).store (0u, std::memory_order_release);
}

//==============================================================================
int LinkBus::join (const juce::String& plugin, juce::uint64 id)
{
    if (shared == nullptr) return -1;
    if (! lock()) return -1;
    int slot = -1;
    const auto t = nowMs();
    for (int i = 0; i < maxMembers && slot < 0; ++i)
        if (shared->members[i].id == 0 || t - shared->members[i].lastSeen > 4000) slot = i;
    if (slot >= 0)
    {
        auto& m = shared->members[slot];
        m.id = id;
        m.lastSeen = t;
        m.flags = 0;
        std::memset (m.name, 0, sizeof (m.name));
        plugin.copyToUTF8 (m.name, sizeof (m.name) - 1);
    }
    unlock();
    return slot;
}

void LinkBus::heartbeat (int slot, juce::uint64 id, juce::uint32 flags)
{
    if (shared == nullptr || slot < 0 || slot >= maxMembers) return;
    auto& m = shared->members[slot];
    if (m.id != id) return;
    m.lastSeen = nowMs();
    m.flags = flags;
}

void LinkBus::leave (int slot, juce::uint64 id)
{
    if (shared == nullptr || slot < 0 || slot >= maxMembers) return;
    if (! lock()) return;
    if (shared->members[slot].id == id) shared->members[slot].id = 0;
    unlock();
}

std::vector<LinkBus::MemberInfo> LinkBus::members (juce::uint64 self) const
{
    std::vector<MemberInfo> list;
    if (shared == nullptr) return list;
    const auto t = nowMs();
    for (int i = 0; i < maxMembers; ++i)
    {
        const auto& m = shared->members[i];
        if (m.id == 0 || t - m.lastSeen > 3000) continue;
        char name[21] {};
        std::memcpy (name, m.name, sizeof (m.name));
        list.push_back ({ juce::String::fromUTF8 (name), m.id, m.id == self, m.flags });
    }
    std::sort (list.begin(), list.end(), [] (const MemberInfo& a, const MemberInfo& b) { return a.plugin < b.plugin; });
    return list;
}

//==============================================================================
void LinkBus::publishKey (int root, juce::uint16 absMask, juce::uint64 source)
{
    if (! lock()) return;
    auto seq = at (shared->keySeq);
    seq.fetch_add (1u, std::memory_order_acq_rel);          // odd: writing
    shared->keyRoot = root;
    shared->keyMask = absMask;
    shared->keySource = source;
    seq.fetch_add (1u, std::memory_order_release);          // even: done
    unlock();
}

juce::uint32 LinkBus::keySeq() const noexcept { return shared != nullptr ? at (shared->keySeq).load (std::memory_order_acquire) : 0u; }

bool LinkBus::readKey (juce::uint32& seq, int& root, juce::uint16& absMask, juce::uint64& source) const
{
    if (shared == nullptr) return false;
    for (int tries = 0; tries < 8; ++tries)
    {
        const auto s1 = at (shared->keySeq).load (std::memory_order_acquire);
        if (s1 == seq) return false;
        if ((s1 & 1u) != 0u) continue;
        const int r = shared->keyRoot;
        const auto m = (juce::uint16) shared->keyMask;
        const auto src = shared->keySource;
        std::atomic_thread_fence (std::memory_order_acquire);
        if (at (shared->keySeq).load (std::memory_order_relaxed) != s1) continue;
        seq = s1; root = r; absMask = m; source = src;
        return true;
    }
    return false;
}

//==============================================================================
void LinkBus::publishChord (const int* notes, int count, juce::uint64 source)
{
    if (! lock()) return;
    auto seq = at (shared->chordSeq);
    seq.fetch_add (1u, std::memory_order_acq_rel);
    const int n = juce::jlimit (0, 16, count);
    for (int i = 0; i < 16; ++i) shared->chordNotes[i] = (juce::int8) (i < n ? juce::jlimit (0, 127, notes[i]) : 0);
    shared->chordCount = n;
    shared->chordSource = source;
    seq.fetch_add (1u, std::memory_order_release);
    unlock();
}

juce::uint32 LinkBus::chordSeq() const noexcept { return shared != nullptr ? at (shared->chordSeq).load (std::memory_order_acquire) : 0u; }

bool LinkBus::readChord (juce::uint32& seq, std::array<int, 16>& notes, int& count, juce::uint64& source) const
{
    if (shared == nullptr) return false;
    for (int tries = 0; tries < 8; ++tries)
    {
        const auto s1 = at (shared->chordSeq).load (std::memory_order_acquire);
        if (s1 == seq) return false;
        if ((s1 & 1u) != 0u) continue;
        std::array<int, 16> n {};
        for (int i = 0; i < 16; ++i) n[(size_t) i] = shared->chordNotes[i];
        const int c = juce::jlimit (0, 16, (int) shared->chordCount);
        const auto src = shared->chordSource;
        std::atomic_thread_fence (std::memory_order_acquire);
        if (at (shared->chordSeq).load (std::memory_order_relaxed) != s1) continue;
        seq = s1; notes = n; count = c; source = src;
        return true;
    }
    return false;
}

//==============================================================================
void LinkBus::publishEvent (int kind, int role, juce::uint64 source)
{
    if (! lock()) return;
    const auto w = at (shared->eventWrite).load (std::memory_order_relaxed);
    auto& e = shared->events[w % numEvents];
    e.kind = (juce::uint8) kind;
    e.role = (juce::uint8) role;
    e.source = source;
    at (e.seq).store (w + 1u, std::memory_order_release);
    at (shared->eventWrite).store (w + 1u, std::memory_order_release);
    unlock();
}

juce::uint32 LinkBus::eventCount() const noexcept { return shared != nullptr ? at (shared->eventWrite).load (std::memory_order_acquire) : 0u; }

void LinkBus::readEvents (juce::uint32& lastSeen, juce::uint64 self, const std::function<void (int, int)>& fn) const
{
    if (shared == nullptr) return;
    const auto w = at (shared->eventWrite).load (std::memory_order_acquire);
    if (w - lastSeen > (juce::uint32) numEvents) lastSeen = w - (juce::uint32) numEvents;
    for (auto s = lastSeen + 1u; s <= w && s != 0u; ++s)
    {
        auto& e = shared->events[(s - 1u) % numEvents];
        if (at (e.seq).load (std::memory_order_acquire) != s) continue;
        if (e.source != self) fn ((int) e.kind, (int) e.role);
    }
    lastSeen = w;
}

void LinkBus::requestLinkAll()
{
    if (shared != nullptr) at (shared->linkAll).fetch_add (1u, std::memory_order_acq_rel);
}

juce::uint32 LinkBus::linkAllSeq() const noexcept { return shared != nullptr ? at (shared->linkAll).load (std::memory_order_acquire) : 0u; }

//==============================================================================
namespace keys
{
    juce::uint16 maskFromSteps (const std::vector<int>& steps) noexcept
    {
        juce::uint16 m = 0;
        for (int s : steps) m |= (juce::uint16) (1u << (((s % 12) + 12) % 12));
        return m;
    }

    juce::uint16 rotate (juce::uint16 mask, int semitones) noexcept
    {
        const int n = ((semitones % 12) + 12) % 12;
        const unsigned m = mask & 0xfffu;
        return (juce::uint16) (((m << n) | (m >> (12 - n))) & 0xfffu);
    }

    int nearestScale (juce::uint16 wanted, const std::vector<juce::uint16>& candidates) noexcept
    {
        int best = -1, bestScore = -1000;
        for (int i = 0; i < (int) candidates.size(); ++i)
        {
            const unsigned c = candidates[(size_t) i] & 0xfffu, w = wanted & 0xfffu;
            const int common = juce::countNumberOfBits (c & w);
            const int extra = juce::countNumberOfBits (c & ~w), missing = juce::countNumberOfBits (w & ~c);
            const int score = common * 4 - extra * 3 - missing * 2;
            if (score > bestScore) { bestScore = score; best = i; }
        }
        return best;
    }
}

} // namespace cat

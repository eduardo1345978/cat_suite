#include "ArpEngine.h"
#include <algorithm>

namespace catbox
{
ArpEngine::ArpEngine() : rnd (0x5eed) {}

namespace
{
template <size_t N>
bool contains (const std::array<int, N>& a, int n, int note)
{
    for (int i = 0; i < n; ++i) if (a[(size_t) i] == note) return true;
    return false;
}

template <size_t N>
void removeFrom (std::array<int, N>& a, int& n, int note)
{
    for (int i = 0; i < n; ++i)
        if (a[(size_t) i] == note)
        {
            for (int j = i; j < n - 1; ++j) a[(size_t) j] = a[(size_t) j + 1];
            --n;
            return;
        }
}
} // namespace

void ArpEngine::noteOn (int note, int)
{
    if (latch && numPressed == 0) numActive = 0;   // a fresh hand on the pads replaces the held chord
    if (! contains (pressed, numPressed, note) && numPressed < maxNotes) pressed[(size_t) numPressed++] = note;
    if (! contains (active, numActive, note) && numActive < maxNotes) active[(size_t) numActive++] = note;
}

void ArpEngine::noteOff (int note)
{
    removeFrom (pressed, numPressed, note);
    if (! latch) removeFrom (active, numActive, note);
}

void ArpEngine::setLatch (bool on)
{
    latch = on;
    if (! latch)
    {
        numActive = numPressed;
        for (int i = 0; i < numPressed; ++i) active[(size_t) i] = pressed[(size_t) i];
    }
}

void ArpEngine::clear()
{
    numActive = 0;
    numPressed = 0;
}

void ArpEngine::activeNotes (uint64_t& lo, uint64_t& hi) const noexcept
{
    lo = hi = 0;
    for (int i = 0; i < numActive; ++i)
    {
        const int n = active[(size_t) i];
        if (n < 64) lo |= (uint64_t) 1 << n;
        else if (n < 128) hi |= (uint64_t) 1 << (n - 64);
    }
}

double ArpEngine::stepStart (int64_t k, double stepLen, float swing) const noexcept
{
    return phase + (double) k * stepLen + ((k & 1) != 0 ? (double) swing * stepLen * 0.5 : 0.0);
}

void ArpEngine::buildSequence (const ArpSettings& s, int& count) noexcept
{
    // sorted copy of the held notes
    for (int i = 0; i < numActive; ++i) sortedNotes[(size_t) i] = active[(size_t) i];
    std::sort (sortedNotes.begin(), sortedNotes.begin() + numActive);

    const int oct = juce::jlimit (1, 4, s.octaves);
    const auto& base = s.mode == ModeAsPlayed ? active : sortedNotes;

    int n = 0;
    std::array<int, maxNotes * 4> up {};
    for (int o = 0; o < oct; ++o)
        for (int i = 0; i < numActive; ++i)
            up[(size_t) n++] = juce::jlimit (0, 127, base[(size_t) i] + 12 * o);

    count = 0;
    auto push = [this, &count] (int note) { if (count < (int) sequence.size()) sequence[(size_t) count++] = note; };

    switch (s.mode)
    {
        case ModeDown:
            for (int i = n - 1; i >= 0; --i) push (up[(size_t) i]);
            break;
        case ModeUpDown:
            for (int i = 0; i < n; ++i) push (up[(size_t) i]);
            for (int i = n - 2; i >= 1; --i) push (up[(size_t) i]);
            break;
        case ModeDownUp:
            for (int i = n - 1; i >= 0; --i) push (up[(size_t) i]);
            for (int i = 1; i <= n - 2; ++i) push (up[(size_t) i]);
            break;
        case ModeConverge:
            for (int lo = 0, hi = n - 1; lo <= hi; ++lo, --hi)
            {
                push (up[(size_t) lo]);
                if (hi != lo) push (up[(size_t) hi]);
            }
            break;
        case ModeUp: case ModeAsPlayed: case ModeRandom: case ModeChord: default:
            for (int i = 0; i < n; ++i) push (up[(size_t) i]);
            break;
    }
}

} // namespace catbox

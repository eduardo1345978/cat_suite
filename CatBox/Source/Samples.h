#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include <functional>

/**
    The box's samples: WAV files in <data>/samples (yours, or recorded with a microphone in the
    box), loaded into memory at start. Up to 64 slots; the audio thread reads a slot without locks
    (a slot is only ever filled, never freed while running).
*/
namespace catbox
{
struct Sample
{
    juce::String name;
    std::vector<float> l, r;
    double sampleRate = 48000.0;
    int length() const noexcept { return (int) l.size(); }
};

class SampleBank
{
public:
    static constexpr int maxSamples = 64;
    static SampleBank& get();

    /** Loads the starter samples (slots 0..n-1, made by makeStarters when a file is missing), then every other WAV in the folder.
        Only the first call does anything. */
    void loadFolder (const juce::File& folder, const juce::StringArray& starterNames, const std::function<void (SampleBank&)>& makeStarters);
    const juce::File& folder() const noexcept { return dir; }

    const Sample* at (int slot) const noexcept
    {
        return slot >= 0 && slot < maxSamples ? slots[(size_t) slot].load (std::memory_order_acquire) : nullptr;
    }
    int count() const noexcept { return used.load(); }
    juce::String name (int slot) const;
    int find (const juce::String& name) const;

    /** Adds a sample (message thread): writes it as a WAV and puts it in the next slot. Returns the slot or -1. */
    int add (const juce::String& name, std::vector<float> l, std::vector<float> r, double sampleRate, bool writeFile = true);

private:
    juce::File dir;
    std::array<std::atomic<const Sample*>, maxSamples> slots {};
    std::vector<std::unique_ptr<Sample>> owned;
    std::atomic<int> used { 0 };
};
} // namespace catbox

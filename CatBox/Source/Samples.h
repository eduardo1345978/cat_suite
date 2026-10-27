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
    int loopFrom = -1, loopTo = -1;   // where the sound holds steady (found when read; -1 = it doesn't: play it to the end)
    int length() const noexcept { return (int) l.size(); }
};

/** A whole instrument from one folder: for every key the nearest recorded note, with up to 4 velocity layers. */
struct ZoneMap
{
    struct Zone { std::array<int16_t, 4> slot { -1, -1, -1, -1 }; int8_t layers = 0; int8_t root = 60; };
    std::array<Zone, 128> keys;
};

class SampleBank
{
public:
    static constexpr int maxSamples = 16384;
    static SampleBank& get();
    SampleBank() { names.resize ((size_t) maxSamples); files.resize ((size_t) maxSamples); }

    /** Loads the starter samples (slots 0..n-1, made by makeStarters when a file is missing), then every other WAV in the folder.
        Only the first call does anything. */
    void loadFolder (const juce::File& folder, const juce::StringArray& starterNames, const std::function<void (SampleBank&)>& makeStarters);
    const juce::File& folder() const noexcept { return dir; }

    const Sample* at (int slot) const noexcept
    {
        return slot >= 0 && slot < maxSamples ? slots[(size_t) slot].load (std::memory_order_acquire) : nullptr;
    }
    int count() const noexcept { return used.load(); }
    juce::String name (int slot) const;           // library samples: "pack/folder/file"
    juce::String shortName (int slot) const;      // the last part
    juce::String folderOf (int slot) const;       // "pack/folder" ("" for your own samples)
    int find (const juce::String& name) const;

    /** A library (any folder tree of WAVs, e.g. VSCO 2 CE, Dirt-Samples): only indexed; a sample is
        read the first time it is needed (ensureLoaded, message thread). Returns how many were found. */
    int addLibrary (const juce::File& root);
    bool ensureLoaded (int slot);
    /** Multi-sample: reads every sample of the slot's folder and maps them over the keys (message thread). */
    bool buildZones (int slot);
    const ZoneMap* zonesFor (int slot) const noexcept { return slot >= 0 && slot < maxSamples ? zones[(size_t) slot].load (std::memory_order_acquire) : nullptr; }
    bool isLibrary (int slot) const noexcept { return slot >= firstLibrary && slot < firstLibrary + libraryCount; }
    juce::StringArray libraryFolders() const;     // the folders, sorted
    std::vector<int> librarySlotsIn (const juce::String& folder) const;
    /** The library's root (searched next to the app, in the data folder, or CATBOX_LIBRARY). */
    static juce::File findLibrary (const juce::File& dataDir);
    /** A note in a sample's name ("Violin_susVib_C4_v1" -> 60), or -1. */
    static int noteFromName (const juce::String&);

    /** Adds a sample (message thread): writes it as a WAV and puts it in the next slot. Returns the slot or -1. */
    int add (const juce::String& name, std::vector<float> l, std::vector<float> r, double sampleRate, bool writeFile = true);

private:
    bool readInto (int slot, const juce::File&);
    juce::File dir;
    std::vector<juce::String> names;     // reserved: never reallocates
    std::vector<juce::File> files;       // where a slot's sample comes from (library)
    int firstLibrary = -1, libraryCount = 0;
    std::array<std::atomic<const Sample*>, maxSamples> slots {};
    std::array<std::atomic<const ZoneMap*>, maxSamples> zones {};
    std::vector<std::unique_ptr<ZoneMap>> ownedZones;
    std::vector<std::unique_ptr<Sample>> owned;
    std::atomic<int> used { 0 };
};
} // namespace catbox

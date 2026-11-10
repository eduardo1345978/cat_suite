#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

/**
    The Pro versions ("Chord-Cat Pro", "Vortex Pro" ...): the same plug-in and the same sound, with a plain panel
    instead of the artwork: every parameter with its exact value (type a number, reset, search, only the changed
    ones), the numbers of the moment (CPU, output peak / RMS, MIDI in / out, sample rate, latency), A / B, undo,
    copy / paste of the whole setting.

    Each plug-in's createPluginFilter() returns cat::makePlugin<ItsProcessor>(); the build defines CAT_PRO=1 for the
    Pro target (cmake/CatPlugin.cmake), so that one gets ProProcessor<ItsProcessor>: it measures around the real
    processBlock and opens the ProEditor.
*/
namespace cat
{
struct ProStats
{
    std::atomic<float> cpu { 0.0f }, cpuPeak { 0.0f }, blockMs { 0.0f };
    std::atomic<float> outPeakL { 0.0f }, outPeakR { 0.0f }, outRmsL { 0.0f }, outRmsR { 0.0f };
    std::atomic<float> inPeakL { 0.0f }, inPeakR { 0.0f };
    std::atomic<int> midiIn { 0 }, midiOut { 0 }, blocks { 0 };
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> blockSize { 0 };

    void measure (const juce::AudioBuffer<float>& b, bool input) noexcept;
};

/** The plain panel. */
class ProEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    ProEditor (juce::AudioProcessor&, ProStats&);
    ~ProEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    class Row;
    class Laf;

private:
    void timerCallback() override;
    void rebuildList();
    void snapshot();
    void restore (const juce::MemoryBlock&);
    void switchAB (int slot);
    juce::String groupOf (const juce::AudioProcessorParameter*) const;

    juce::AudioProcessor& proc;
    ProStats& stats;
    std::unique_ptr<Laf> laf;

    juce::ComboBox programs, groups;
    juce::TextButton prevB { "<" }, nextB { ">" }, aB { "A" }, bB { "B" }, copyAB { "A > B" }, undoB { "Undo" }, resetB { "Reset all" },
                     copyB { "Copy" }, pasteB { "Paste" };
    juce::ToggleButton changedOnly { "Only changed" };
    juce::TextEditor search;
    juce::Viewport viewport;
    juce::Component list;
    juce::OwnedArray<Row> rows;
    std::map<const juce::AudioProcessorParameter*, juce::String> groupNames;

    juce::MemoryBlock slots[2];
    int slot = 0;
    std::vector<juce::MemoryBlock> undoStack;
    int lastMidiIn = 0, lastMidiOut = 0;
    float midiInRate = 0.0f, midiOutRate = 0.0f;
    juce::uint32 lastTick = 0;
    int changedCount = 0;
    juce::Rectangle<int> statsArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProEditor)
};

/** The plug-in, measured, with the plain panel. */
template <class P>
class ProProcessor : public P
{
public:
    ProProcessor() = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        stats.sampleRate = sampleRate;
        stats.blockSize = samplesPerBlock;
        P::prepareToPlay (sampleRate, samplesPerBlock);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        stats.midiIn += midi.getNumEvents();
        if (this->getTotalNumInputChannels() > 0) stats.measure (buffer, true);
        const auto t0 = juce::Time::getHighResolutionTicks();
        P::processBlock (buffer, midi);
        const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        const double sr = stats.sampleRate.load() > 0.0 ? stats.sampleRate.load() : 48000.0;
        const float load = (float) (secs / juce::jmax (1.0e-6, buffer.getNumSamples() / sr));
        stats.cpu = stats.cpu.load() * 0.9f + load * 0.1f;
        stats.cpuPeak = juce::jmax (stats.cpuPeak.load() * 0.995f, load);
        stats.blockMs = (float) (secs * 1000.0);
        stats.midiOut += midi.getNumEvents();
        stats.measure (buffer, false);
        ++stats.blocks;
    }
    using P::processBlock;

    juce::AudioProcessorEditor* createEditor() override { return new ProEditor (*this, stats); }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return P::getName() + " Pro"; }

    ProStats stats;
};

/** What createPluginFilter() returns: the plug-in, or (in the Pro build) its Pro version. */
template <class P>
juce::AudioProcessor* makePlugin()
{
   #if defined (CAT_PRO) && CAT_PRO
    return new ProProcessor<P>();
   #else
    return new P();
   #endif
}
} // namespace cat

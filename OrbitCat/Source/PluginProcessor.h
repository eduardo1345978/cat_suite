#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetBar.h"
#include "OrbitEngine.h"
#include "HostSync.h"
#include "MidiLearn.h"
#include "MidiScheduler.h"

/**
    Orbit-Cat: orbital-physics arpeggiator.

    MIDI In (held notes) -> OrbitEngine (Kepler orbits, mutual pull, drag, turbulence) ->
    a note on every alignment-spoke crossing (optionally quantised to the host grid) -> MIDI Out.
*/
class OrbitCatProcessor : public juce::AudioProcessor
{
public:
    OrbitCatProcessor();
    ~OrbitCatProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Orbit-Cat"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return JucePlugin_IsMidiEffect != 0; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) orbitcat::getOrbitPresets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override { return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (orbitcat::getOrbitPresets()[(size_t) i].name) : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    cat::MidiLearnManager midiLearn { *this };
    cat::NoteMonitor noteMonitor;

    static juce::String slotParam (int slot, const char* name) { return "s" + juce::String (slot + 1) + name; }

    void applyPreset (int index);
    int getCurrentPreset() const { return currentPreset.load(); }
    void surprise();
    juce::MemoryBlock surpriseUndo;
    /** Factory + user presets and favourites (shared preset bar). */
    cat::PresetManager presetManager { *this, "Orbit-Cat",
        { [] { return [] { juce::StringArray n; for (auto& p : orbitcat::getOrbitPresets()) n.add (p.name); return n; }(); }, [this] (int i) { applyPreset (i); }, [this] { return getCurrentPreset(); } } };


    /** Lock-free view of the bodies for the orbit display (written by the audio thread). */
    struct BodyView
    {
        std::atomic<bool> active { false };
        std::atomic<float> x { 0 }, y { 0 }, semiMajor { 8 }, eccentricity { 0 }, omega { 0 }, flash { 0 }, mass { 50 };
        std::atomic<int> note { 60 }, slot { 0 };
    };
    std::array<BodyView, orbitcat::maxBodies> bodyViews;
    std::atomic<double> uiPpq { 0.0 };
    std::atomic<bool> uiTransportRunning { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void readParams();
    void advance (int fromSample, int toSample);

    // Parameter pointers cached once (no string lookups on the audio thread)
    struct SlotRefs { std::atomic<float>* v[8]; };
    std::array<SlotRefs, orbitcat::numSlots> slotRefs {};
    std::atomic<float>* gRefs[9] {};
    std::atomic<float>* swingRef = nullptr;

    orbitcat::OrbitEngine engine;
    orbitcat::GlobalParams global;
    std::array<orbitcat::BodyParams, orbitcat::numSlots> slots;
    double quantize = 0.0;
    float accent = 100.0f;
    int outChannel = 0;              // 0 = same channel as the held note
    bool latch = false;

    cat::HostSync hostSync;
    cat::MidiScheduler scheduler;
    juce::MidiBuffer outBuffer;
    juce::Random rng;
    int64_t sampleCounter = 0;
    double sampleRate = 44100.0;
    std::vector<std::pair<int, int>> heldKeys;   // (channel, note) physically down
    std::array<float, orbitcat::maxBodies> flashes {};
    std::atomic<int> currentPreset { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitCatProcessor)
};

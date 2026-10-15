#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LinkBus.h"

namespace cat
{
/**
    One Cat's connection to Cat Link (owned by its PresetManager).

    What it can share depends on the Cat (Caps). The switches (Link on, Key, Chords, Life) are
    kept in the plug-in state, so a project remembers them.

      Key     set the key + scale in one Cat and the others follow (Chord-Cat, Talk-Cat, Life-Cat)
      Chords  Chord-Cat sends the chord it plays; followers get it as if Chord-Cat's MIDI were
              routed to them (no Cubase routing needed). Your own MIDI still works too.
      Life    Life-Cat sends births and deaths; Drum-Cat answers with hits
*/
class LinkClient : private juce::Timer
{
public:
    struct Caps
    {
        bool key = false;            // has a key + scale to share
        bool sendChords = false;     // Chord-Cat
        bool receiveChords = false;  // can follow a chord
        juce::String receiveChordsText = "Follow Chord-Cat's chords";
        bool sendLife = false;       // Life-Cat
        bool receiveLife = false;    // answers Life-Cat
        juce::String receiveLifeText = "React to Life-Cat";
        juce::String grooveParam;    // e.g. "followGroove" (the existing Drum-Cat groove link)
    };
    enum Option { On = 0, Key, Chords, Life };

    LinkClient (juce::AudioProcessor&, juce::AudioProcessorValueTreeState&, const juce::String& pluginName, Caps);
    ~LinkClient() override;

    const Caps caps;
    const juce::String pluginName;
    juce::AudioProcessorValueTreeState& state;

    bool option (Option o) const noexcept { return options[(size_t) o].load (std::memory_order_relaxed); }
    void setOption (Option, bool);
    bool sharing (Option o) const noexcept { return option (On) && option (o); }

    /** Key + scale of this Cat (root 0..11, absolute pitch-class mask) and how to apply one. */
    std::function<std::pair<int, juce::uint16>()> getKey;
    std::function<void (int root, juce::uint16 absMask)> applyKey;

    // ---- audio thread
    /** Followers: adds the linked chord to the incoming MIDI (note-ons / note-offs when it changes). */
    void processInput (juce::MidiBuffer& midi, int numSamples);
    /** Chord-Cat: watches the outgoing notes and shares the chord once it has settled. */
    void processOutput (const juce::MidiBuffer& midi, int numSamples, double sampleRate);
    void publishLifeEvent (int kind, int role);
    /** Calls fn (kind, role) for Life-Cat events since the last call (only while Life is on). */
    void pollLifeEvents (const std::function<void (int, int)>& fn);

    // ---- panel
    std::vector<LinkBus::MemberInfo> members() const;
    void linkAll();
    juce::String lastChordText() const;
    bool busOk() const noexcept { return bus->isValid(); }

private:
    void timerCallback() override;
    void readOptions();
    void releaseInjected (juce::MidiBuffer&);

    juce::AudioProcessor& processor;
    juce::SharedResourcePointer<LinkBus> bus;
    const juce::uint64 id;
    int slot = -1;
    std::array<std::atomic<bool>, 4> options {};

    // key (message thread)
    juce::uint32 seenKeySeq = 0;
    std::pair<int, juce::uint16> lastOwnKey { -1, 0 };
    bool keyInitialised = false;
    juce::uint32 seenLinkAll = 0;
    int beatCount = 0;

    // chords (audio thread)
    juce::uint32 seenChordSeq = 0;
    std::array<int, 16> injected {};
    int numInjected = 0;
    std::array<juce::uint8, 128> outHeld {};
    bool outChanged = false;
    double outSettle = 0.0;
    std::array<int, 16> lastSent {};
    int numLastSent = -1;
    std::atomic<int> uiChord[16] {};
    std::atomic<int> uiChordCount { 0 };

    // life (audio thread)
    juce::uint32 seenEvents = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LinkClient)
};

/** The Link panel (normally opened by the LINK button). */
std::unique_ptr<juce::Component> createLinkPanel (LinkClient&);

//==============================================================================
/** The LINK button in every Cat's preset bar: shows who is linked; click for the Link panel. */
class LinkButton : public juce::Button, private juce::Timer
{
public:
    explicit LinkButton (LinkClient&);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;

private:
    void timerCallback() override;
    LinkClient& client;
    int others = 0;
};

} // namespace cat

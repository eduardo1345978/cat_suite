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
      Tempo   the standalone apps share tempo and play / stop, so they play in sync
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
        bool sendTuning = false;     // Micro-Cat
        bool receiveTuning = false;  // Glide-, Sing-, Talk-Cat: play in Micro-Cat's tuning
    };
    enum Option { On = 0, Key, Chords, Life, Tempo, Tuning };
    static constexpr int numOptions = 6;

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

    // ---- tuning
    /** Micro-Cat (message thread): the pitch (MIDI semitones) every key plays; active = false: normal tuning. */
    void publishTuning (const std::array<float, 128>& pitches, bool active);
    /** Followers (audio thread, real-time safe): a pitch in MIDI semitones (may be fractional: glides, bends)
        moved into the linked tuning; unchanged when no tuning is linked. */
    float tuned (float semis) const noexcept   // (inline: engines built without the rest of Cat Link, e.g. in Cat Box, still link)
    {
        if (! tuningActive.load (std::memory_order_relaxed) || ! std::isfinite (semis)) return semis;
        const float s = juce::jlimit (0.0f, 127.0f, semis);
        const int k = juce::jmin (126, (int) std::floor (s));
        const float frac = s - (float) k;
        const float a = tunedPitch[(size_t) k].load (std::memory_order_relaxed), b = tunedPitch[(size_t) k + 1].load (std::memory_order_relaxed);
        return a + (b - a) * frac + (semis - s);   // beyond the MIDI range: keep the distance
    }
    bool followingTuning() const noexcept { return tuningActive.load (std::memory_order_relaxed); }
    juce::String tuningText() const;

    // ---- shared tempo + play / stop (standalone apps)
    /** Use instead of getPlayHead(): in a standalone app with Tempo linked, returns a play head that
        follows the shared transport; otherwise returns `host` unchanged. Audio thread. */
    juce::AudioPlayHead* playHead (juce::AudioPlayHead* host, int numSamples, double sampleRate);
    bool isStandalone() const noexcept { return processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone; }
    void setTransport (bool playing, double bpm);
    bool transportPlaying() const;
    double transportBpm() const;

    // ---- panel
    std::vector<LinkBus::MemberInfo> members() const;
    void linkAll();
    juce::String lastChordText() const;
    bool busOk() const noexcept { return bus->isValid(); }

    // ---- the Link panel pop-up (message thread). It is closed again when the LINK
    // button or this client goes away, so it can never outlive the plug-in.
    void showPanel (juce::Component& nextTo);
    void closePanel();

private:
    void timerCallback() override;
    void readOptions();
    std::unique_ptr<juce::Component> panelContent;
    std::unique_ptr<juce::CallOutBox> panelBox;
    void releaseInjected (juce::MidiBuffer&);

    juce::AudioProcessor& processor;
    juce::SharedResourcePointer<LinkBus> bus;
    const juce::uint64 id;
    int slot = -1;
    std::array<std::atomic<bool>, numOptions> options {};

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

    // tuning: published (message thread), followed (written by the timer, read on the audio thread)
    std::array<float, 128> ownTuning {};
    bool ownTuningActive = false, ownTuningSent = false;
    juce::uint32 seenTuningSeq = 0;
    std::array<std::atomic<float>, 128> tunedPitch {};
    std::atomic<bool> tuningActive { false };

    // life (audio thread)
    juce::uint32 seenEvents = 0;

    // shared transport (audio thread)
    struct SharedPlayHead : public juce::AudioPlayHead
    {
        bool playing = false;
        double bpm = 120.0, ppq = 0.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setTimeSignature (TimeSignature { 4, 4 });
            info.setIsPlaying (playing);
            if (playing)
            {
                info.setPpqPosition (ppq);
                info.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
            }
            return info;
        }
    } sharedHead;
    double nextPpq = 0.0;

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
    ~LinkButton() override;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;

private:
    void timerCallback() override;
    LinkClient& client;
    int others = 0;
};

} // namespace cat

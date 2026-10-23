#pragma once

#include "BoxModel.h"
#include "BoxAudio.h"
#include <atomic>
#include <functional>

namespace catbox
{
/**
    The Cat Box brain: what the 12 knobs, 16 pads and 8 buttons do, presets, scenes and the
    remembered state. The same object is driven by the real hardware (a Raspberry Pi Pico
    that shows up as a USB MIDI device, see the MIDI map below), by a normal MIDI keyboard,
    and on the PC by the mouse and the computer keyboard.

    MIDI map of the controller (channel 16):
      CC 20..31   knobs 1..12, relative (1..63 = clicks right, 65..127 = clicks left, two's complement)
      notes 36..51  pads 1..16 (velocity = how hard)
      CC 102..110 buttons Play, Steps, Shift, Scene, Instr, Oct-, Oct+, Rec, Chaos (127 = pressed, 0 = released)
    Any other channel: notes play the instrument on screen.
*/
class BoxController : public juce::MidiInputCallback, private juce::Timer
{
public:
    static constexpr int controllerChannel = 16;
    static constexpr int firstKnobCC = 20, firstPadNote = 36, firstButtonCC = 102;

    explicit BoxController (juce::File dataDirectory);
    ~BoxController() override;

    BoxAudio& getAudio() noexcept { return audio; }
    juce::File getDataDirectory() const { return dataDir; }

    // knobs and buttons (message thread)
    void turnKnob (int knob, float clicks);
    void buttonDown (int button);
    void buttonUp (int button);
    // pads (any thread)
    void padDown (int pad, int velocity);
    void padUp (int pad);

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    // what the screen shows (message thread)
    struct KnobView { juce::String name, value; float fraction = 0.0f; bool used = false, global = false, stepEdit = false; };
    KnobView knobView (int knob) const;
    KnobView slotView (int page, int slotIndex) const;
    /** The knob turned last (page, slot with Shift, when) - the box screen shows it big. */
    int lastKnobPage() const noexcept { return lastKnobPg; }
    int lastKnobSlot() const noexcept { return lastKnobSl; }
    juce::uint32 lastKnobTime() const noexcept { return lastKnobAt; }

    const BoxData& getData() const noexcept { return data; }
    int getPage() const noexcept { return page.load(); }
    bool isShiftHeld() const noexcept { return shift.load(); }
    bool isStepView() const noexcept { const int p = page.load(); return p < NumInstruments && stepViews[(size_t) p].load(); }
    bool isSceneMode() const noexcept { return sceneMode.load(); }
    bool isLatched() const noexcept { return latchOn; }
    bool isButtonLit (int button) const;
    juce::String buttonLabel (int button) const;   // what the button does right now
    bool presetExists (int page, int presetSlot) const;
    bool sceneExists (int sceneSlot) const;
    juce::String getFlash() const;
    int arpPadNote (int pad) const;
    int bassPadNote (int pad) const;
    int stepPage (int instrument) const noexcept { return stepPages[(size_t) juce::jlimit (0, NumPages - 1, instrument)].load(); }
    int numStepPages (int instrument) const;
    int selectedBassStep() const noexcept { return bassSel; }
    int selectedDrum() const noexcept { return data.selectedDrum; }
    bool isLightScreen() const noexcept { return data.lightScreen; }
    void setLightScreen (bool);
    void setPage (int page);
    void pickDrum (int pad);

    // presets, scenes and memory
    bool savePreset (int page, int presetSlot);
    bool loadPreset (int page, int presetSlot);
    bool saveScene (int sceneSlot);
    bool loadScene (int sceneSlot);
    juce::File presetFile (int page, int presetSlot) const;
    juce::File sceneFile (int sceneSlot) const;
    void saveState();
    void setMuted (int instrument, bool muted);

    // names of presets and scenes ("" = empty slot); page -1 = scenes
    juce::String presetName (int page, int slotIndex) const;      // the box's names while linked, else this one's
    juce::String localPresetName (int page, int slotIndex) const;
    bool renamePreset (int page, int slotIndex, const juce::String& name);
    juce::var localPresetJson (int page, int slotIndex) const;
    bool storeLocalPresetJson (int page, int slotIndex, const juce::var& preset);
    void rescanNames();
    juce::var namesVar() const;
    std::function<void()> onNamesChanged;

    // the link between the box and the PC app (see BoxLink.h)
    /** PC app linked to a box: every knob, pad and button goes there, this one only mirrors it. nullptr = play here. */
    void setRemoteSink (std::function<void (const juce::var&)>);
    bool isRemote() const noexcept { return remoteSink != nullptr; }
    juce::var makeState() const;     // the box's state for a linked PC
    juce::var makeLive();            // what is playing right now (and the sound picture)
    void applyState (const juce::var&);
    void applyLive (const juce::var&);
    /** The box receives a message from the PC; reply sends an answer to that PC. */
    void handleLinkMessage (const juce::var& message, const std::function<void (const juce::var&)>& reply);

    /** Chaos: new random sound settings for the page on screen (patterns, tempo, key and volume stay).
        The button must be held for 0.6 s, so it is never pressed by accident. Shift + Chaos takes the last one back. */
    void chaos();
    bool undoChaos();

    /** For the self test: run the timer work now (recorded notes, autosave). */
    void pump() { timerCallback(); }

private:
    void timerCallback() override;
    void paramChanged();
    void publish (bool atBar = false);
    void setFlash (const juce::String&);
    void markDirty() { dirty = true; lastChange = juce::Time::getMillisecondCounter(); }
    void syncAtomics();
    void runOnMessageThread (std::function<void()>);
    void tapTempo();
    void clearPage();
    void tapDrumStep (int step);
    void tapArpStep (int step);
    void tapBassStep (int step);
    void selectDrum (int pad);
    void editBassStep (int knob, int clicks);
    int nextScaleNote (int note, int dir) const;

    juce::File dataDir;
    BoxAudio audio;
    BoxData data;

    std::atomic<int> page { Drums }, key { 0 }, scale { 1 }, lastBassNote { 36 };
    std::atomic<bool> shift { false }, rec { false }, saveArm { false }, sceneMode { false };
    std::array<std::atomic<bool>, NumInstruments> stepViews {};
    std::array<std::atomic<int>, NumPages> stepPages {};
    std::array<std::atomic<int>, NumInstruments> padOct {};
    std::array<std::atomic<int>, numPads> padHeldNote {}, padHeldInst {};   // -1 = not held / not a note
    bool latchOn = false;
    std::array<std::array<float, numSlots>, NumPages> chaosUndo {};
    std::array<bool, NumPages> hasChaosUndo {};
    juce::Random chaosRnd;
    std::function<void (const juce::var&)> remoteSink;
    std::array<std::array<juce::String, numPresetSlots>, NumPages> localNames, remoteNames;
    std::array<juce::String, numPresetSlots> localSceneNames, remoteSceneNames;
    void sendRemote (juce::DynamicObject*);
    int lastKnobPg = -1, lastKnobSl = -1;
    juce::uint32 lastKnobAt = 0;
    bool chaosArmed = false;            // Chaos only fires when held for a moment (no accidents)
    juce::uint32 chaosDownAt = 0;
    static constexpr juce::uint32 chaosHoldMs = 600;
    int bassSel = -1;

    std::array<float, numSlots> knobRemainder {};
    juce::Array<double> taps;
    juce::String flash;
    juce::uint32 flashTime = 0, lastChange = 0;
    bool dirty = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (BoxController)
    JUCE_DECLARE_NON_COPYABLE (BoxController)
};
} // namespace catbox

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
    /** Sets the knob's parameter back to its starting value (a double-click on the PC). */
    void resetKnob (int knob);
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
    bool isStepView() const noexcept { const int p = page.load(); return p != PageFx && stepViews[(size_t) juce::jlimit (0, NumPages - 1, p)].load(); }
    /** Sound mode (the Sound button, on Arp and Bass): the knobs edit the page's engine, Oct - / Oct + flip its knob pages. */
    bool isSoundMode() const noexcept { const int p = page.load(); return soundMode.load() && (p == Arp || p == Bass); }
    bool isGlide (int page) const;                  // the page's Sound is the Glide engine
    int enginePage() const noexcept { const int p = page.load(); return p < NumInstruments ? enginePages[(size_t) p].load() : 0; }
    int numEnginePages() const;
    juce::String enginePageName() const;            // e.g. "Osc I"
    juce::String engineTitle() const;               // e.g. "Glide: Glossy Keys *"
    KnobView engineView (int slotIndex) const;      // a knob in Sound mode (slot 0..23)

    // the Chords page (played on the Arp's sound)
    int selectedChord() const noexcept { return chordSel; }
    juce::String chordNow() const { return chordNowName; }       // the chord sounding now, e.g. "Am7  A C E G"
    juce::String chordPadLabel (int pad) const;                   // what a pad plays on the Chords page
    bool chordPadSounding (int pad) const noexcept { return pad >= 0 && pad < numPads && padChordCount[(size_t) pad] > 0; }
    int orchidHeldType() const noexcept { return orchidType.load(); }
    // the Orbit page (held notes are planets)
    int orbitPadNote (int pad) const;
    int selectedPlanet() const noexcept { return planetSel; }
    bool isOrbitHeld() const noexcept { return orbitLatch; }
    bool isOrchid() const noexcept { return juce::roundToInt (data.chordVals[slot::CPlay]) == 1; }
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
    /** 0..1 while Chaos is held and not fired yet (the screen fills a bar), else 0. */
    float chaosHoldProgress() const;

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
    std::array<std::atomic<bool>, NumPages> stepViews {};
    std::atomic<bool> soundMode { false };
    std::array<std::atomic<int>, NumInstruments> enginePages {};
    std::array<float, numSlots> engineRemainder {};
    juce::String engineId (int slotIndex) const;
    static constexpr int numChordKeys = numPads + 128;            // the 16 pads, then a MIDI keyboard's notes
    std::array<std::array<int, 8>, numChordKeys> padChord {};
    std::array<int, numChordKeys> padChordCount {};
    std::atomic<int> orchidType { -1 };
    int orchidRootPad = -1, orchidVelocity = 100;
    std::array<int, 8> lastChord {};
    int lastChordCount = 0;
    bool strumFlip = false;
    int chordSel = -1;
    int planetSel = 0;
    bool orbitLatch = false;
    void editPlanet (int knob, int clicks);
    juce::String chordNowName;
    ChordSlot chordForPad (int pad) const;
    ChordSlot chordForNote (int note, int& octaveShift) const;
    int buildChord (const ChordSlot&, int octaveShift, std::array<int, 8>& out);
    void playChord (int key, const ChordSlot&, int octaveShift, int velocity);
    void releaseChord (int key);
    void chordPadDown (int pad, int velocity);
    void chordPadUp (int pad);
    void editChord (int knob, int clicks);
    void editEngine (int slotIndex, float clicks);
    void resetEngineSlot (int slotIndex);
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
    bool instrHeld = false, instrTurned = false;   // Instr held: the knobs flip through the pages
    float instrRemainder = 0.0f;
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

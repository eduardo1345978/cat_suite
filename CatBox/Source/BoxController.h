#pragma once

#include "BoxModel.h"
#include "BoxAudio.h"
#include "DrumCatEngine.h"
#include "BassEngine.h"
#include <atomic>

namespace catbox
{
/**
    The Cat Box brain: what the 12 knobs, 16 pads and 8 buttons do, the presets and the
    remembered state. The same object is driven by the real hardware (a Raspberry Pi Pico
    that shows up as a USB MIDI device, see the MIDI map below), by a normal MIDI keyboard,
    and on the PC by the mouse and the computer keyboard.

    MIDI map of the controller (channel 16):
      CC 20..31   knobs 1..12, relative (1..63 = clicks right, 65..127 = clicks left, two's complement)
      notes 36..51  pads 1..16 (velocity = how hard)
      CC 102..109 buttons Play, Hold, Shift, Tap, Instrument, Oct-, Oct+, Rec (127 = pressed, 0 = released)
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
    const BoxData& getData() const noexcept { return data; }
    int getPage() const noexcept { return page.load(); }
    bool isShiftHeld() const noexcept { return shift.load(); }
    bool isRecHeld() const noexcept { return rec.load(); }
    bool isStepView() const noexcept { return stepView.load(); }
    bool isLatched() const noexcept { return latchOn; }
    bool isButtonLit (int button) const;
    bool presetExists (int instrument, int presetSlot) const;
    juce::String getFlash() const;
    juce::String bassChordName() const;
    int arpPadNote (int pad) const;
    int bassPadNote (int pad) const;
    const drumcat::Pattern& getDrumPattern() const noexcept { return drumPattern; }

    // presets and memory
    bool savePreset (int instrument, int presetSlot);
    bool loadPreset (int instrument, int presetSlot);
    juce::File presetFile (int instrument, int presetSlot) const;
    void saveState();
    void setMuted (int instrument, bool muted);

    /** For the self test: run the timer work now (recorded hits, autosave). */
    void pump() { timerCallback(); }

private:
    void timerCallback() override;
    void paramChanged (int instrument, int slotIndex);
    void regenerateDrums();
    void regenerateBass();
    void publish();
    void sendBassChord();
    void setFlash (const juce::String&);
    void markDirty() { dirty = true; lastChange = juce::Time::getMillisecondCounter(); }
    void syncAtomics();
    void runOnMessageThread (std::function<void()>);
    void tapTempo();

    juce::File dataDir;
    BoxAudio audio;
    BoxData data;
    drumcat::Pattern drumPattern;
    basscat::Pattern bassPattern;

    std::atomic<int> page { Drums }, key { 0 }, scale { 1 };
    std::atomic<bool> shift { false }, rec { false }, stepView { false }, saveArm { false };
    std::array<std::atomic<int>, NumInstruments> padOct {};
    std::array<std::atomic<int>, numPads> padHeldNote {}, padHeldInst {};   // -1 = not held / preset press
    bool latchOn = false;

    std::array<float, numSlots> knobRemainder {};
    juce::Array<double> taps;
    juce::String flash;
    juce::uint32 flashTime = 0, lastChange = 0;
    bool dirty = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (BoxController)
    JUCE_DECLARE_NON_COPYABLE (BoxController)
};
} // namespace catbox

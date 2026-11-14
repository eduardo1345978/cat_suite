#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "LoopEngine.h"

/**
    Cat Loop's window: design "A2 · Pedalboard, refined".
    Top: the logo + SAVE / LOAD / EXPORT / MIDI / SETUP, the amber display (tempo, bar, the 5 tracks at a glance,
    QUANTIZE / CLICK / INPUT), the INPUT / RHYTHM / MASTER knobs and the beat. Then 5 track strips (LEVEL, the LED
    ring around the big footswitch, STOP / UNDO / CLEAR), and PLAY ALL / STOP ALL.

    Keys: 1-5 the big buttons, Q-T stop, A-G undo, Z-B clear, Space play / stop all, Backspace undo (last track),
    Enter tap tempo, B beat. MIDI: pads 36-40 the big buttons, 41 play / stop all, 42 undo, 43 beat (MIDI learn
    changes any of them).
*/
namespace looper
{
enum Action
{
    actPress = 0,      // + track (0..4)
    actStop = 5,
    actUndo = 10,
    actClear = 15,
    actPlayAll = 20, actStopAll, actToggleAll, actBeat, actTap, actUndoLast,
    numActions
};
juce::String actionName (int action);

/** A MIDI control that does an action: a note (on) or a CC (crossing 64 upwards). */
struct Binding { bool cc = false; int number = 36; int action = actPress; };

class LoopView : public juce::Component, private juce::Timer
{
public:
    LoopView (Engine&, juce::AudioDeviceManager*, juce::PropertiesFile*);
    ~LoopView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    /** Called on the message thread for every incoming MIDI message. */
    void handleMidi (const juce::MidiMessage&);
    void doAction (int action);

    std::vector<Binding> bindings;
    static std::vector<Binding> defaultBindings();
    void saveSettings();
    void loadSettings();

    /** Session files (.catloop = a zip with session.xml + one .wav per track). */
    bool saveSession (const juce::File&);
    bool loadSession (const juce::File&);
    bool exportMix (const juce::File&);
    bool exportStems (const juce::File& folder);

    void showMessage (const juce::String& text);
    bool learning = false;
    int learnTarget = -1;

    // for the UI stress test
    void randomPoke (juce::Random&);

    class Strip;
    class Display;
    class Knob;
    class LedButton;

private:
    void timerCallback() override;
    void openSetup();
    void openBeatMenu();
    void openTempoMenu();
    void doSave();
    void doLoad();
    void doExport();
    void toggleLearn();
    void target (int action);    // a click in MIDI-learn mode picks what the next pad does

    Engine& engine;
    juce::AudioDeviceManager* devices;
    juce::PropertiesFile* props;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::OwnedArray<Strip> strips;
    std::unique_ptr<Display> display;
    std::unique_ptr<Knob> inputKnob, rhythmKnob, masterKnob;
    std::unique_ptr<LedButton> saveB, loadB, exportB, midiB, setupB, playAllB, stopAllB, beatB;
    juce::String message;
    double messageUntil = 0.0;
    std::array<int, 128> lastCc {};
    bool autoLatency = true;
    int manualLatencyMs = 0;

    friend class Strip;
    friend class Display;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopView)
};
} // namespace looper

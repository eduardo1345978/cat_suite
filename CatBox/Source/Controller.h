#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include "Project.h"
#include "Audio.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

/**
    Cat Box 2: what the knobs, buttons, pads and keys do. Owns the project (message thread)
    and hands every change to the audio. The box's hardware (the Pico), any MIDI keyboard
    (learned once), the PC panel and the computer keyboard all end up here.

    Buttons: Track 1-8, Sound, Seq, Note FX, Mix, Project, Prev, Next, Shift, Scene, Rec, Play, Chaos.
    Modes:   SOUND (the engine's knob pages; Sound again = the sound browser), SEQ (steps),
             NOTE FX (arp / chords / orbit), MIX (levels, pan, sends), PROJECT (tempo, key ...).
*/
namespace catbox
{
enum Mode { ModeSound = 0, ModeBrowse, ModeSeq, ModeFx, ModeMix, ModeProject, ModeMenu, NumModes };
enum Button
{
    BtnTrack1 = 0, BtnTrack8 = 7,
    BtnSound = 8, BtnSeq, BtnFx, BtnMix, BtnProject, BtnPrev, BtnNext, BtnShift, BtnScene, BtnRec, BtnPlay, BtnChaos,
    NumButtons
};
const char* buttonName (int b);
const char* modeName (int m);

/** The hardware (the Pico in the box) sends on MIDI channel 16. */
constexpr int controllerChannel = 16;
constexpr int firstKnobCC = 20;       // 20..31: the 12 encoders (relative: 1..63 right, 65..127 left)
constexpr int firstButtonCC = 102;    // 102..121: the 20 buttons (127 down, 0 up)
constexpr int sliderCC = 8;           // the slider (absolute 0..127): the DJ filter
constexpr int joyXCC = 16, joyYCC = 17; // the joystick (absolute, 64 = middle): bend and mod
constexpr int firstPadNote = 36;      // 36..51: the 16 pads

/** What one of the 12 knobs does right now (for the screen and the panel). */
struct KnobView
{
    bool used = false;
    juce::String name, value;
    float norm = 0.0f;       // 0..1 (where the ring is)
    bool bipolar = false;    // the ring starts in the middle
};

class Controller : public juce::MidiInputCallback, private juce::Timer
{
public:
    explicit Controller (const juce::File& dataDirectory);
    ~Controller() override;

    Audio& getAudio() noexcept { return audio; }
    const Project& getProject() const noexcept { return project; }
    const juce::File& getDataDirectory() const noexcept { return dataDir; }

    // ---- the controls
    void buttonDown (int b);
    void buttonUp (int b);
    void turnKnob (int knob, float clicks);    // one click = the smallest step
    void turnKnobBy (int knob, float fraction); // an absolute knob / fader moved by this much of its travel
    void resetKnob (int knob);                 // back to its default (double-click on the PC)
    void padDown (int pad, int velocity);
    void padUp (int pad);
    void keyDown (int note, int velocity);     // a key of a MIDI keyboard
    void keyUp (int note);
    void setSlider (float v);                   // 0..1: the DJ filter
    void setJoystick (float x, float y);        // -1..1: x = pitch bend, y = mod (up) / filter (down)
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    // ---- shortcuts (the PC, the self test)
    void selectTrack (int t);
    void setMode (int m);
    void setPlaying (bool);
    void setMute (int t, bool);
    void setSolo (int t, bool);
    void loadSound (int track, int engine, int preset);
    void setParam (int track, int index, float value);
    void setFxParam (int track, int index, float value);
    void setMaster (std::function<void (Master&)> change);
    void clearPattern (int track);
    void editPattern (int track, std::function<void (Pattern&)> change);
    void saveScene (int s);
    bool loadScene (int s);
    bool sceneExists (int s) const { return scenes[(size_t) juce::jlimit (0, numScenes - 1, s)] != nullptr; }
    void saveUserSound();
    void chaos();
    void factoryReset();   // a new project (undoable); your sounds, samples and the MIDI keyboard stay
    void undo();   // Shift + Chaos: back one step (up to 40)
    int undoSteps() const { return (int) undoStack.size(); }
    void tapTempo();
    void saveState();
    void pump();   // handles what the audio thread reported (the self test calls it; normally the timer does)

    // ---- what the screen shows
    int getTrack() const noexcept { return track.load(); }
    int getMode() const noexcept { return mode.load(); }
    bool isShift() const noexcept { return shift.load(); }
    bool isSceneMode() const noexcept { return sceneMode.load(); }
    bool isRecArmed() const noexcept { return audio.isRecording(); }
    KnobView knob (int k) const;                 // 0..11 (Shift shows the second layer)
    juce::String pageName() const;               // the knob page on screen
    int pageIndex() const;
    int numPagesNow() const;
    juce::StringArray pageNames() const;
    juce::String trackTitle (int t) const;       // e.g. "Acid · Squelch *"
    juce::String presetName (int t) const;
    bool isEdited (int t) const { return edited[(size_t) juce::jlimit (0, numTracks - 1, t)]; }
    juce::String flashText() const;
    juce::String hint() const;                   // one line: what the buttons do now
    // browser
    juce::StringArray browseCategories() const;
    juce::StringArray browsePresets() const;     // in the category
    int browseCategory() const { return browseCat; }
    int browsePresetIndex() const { return browsePos; }
    // sequencer
    int seqBar() const { return bar[(size_t) track.load()]; }
    int seqLane() const { return lane[(size_t) track.load()]; }
    bool isStepHeld (int step) const;
    int heldStepCount() const;
    // pads
    int padNote (int track, int pad) const;      // -1 = no note
    bool padLit (int pad) const;                  // a pad that should glow (step on, scene saved ...)
    int padOctave (int t) const { return octave[(size_t) juce::jlimit (0, numTracks - 1, t)].load(); }
    bool padsAsScale() const { return scalePads.load(); }

    // ---- the MENU (Project pressed again): 16 tiles, one per pad
    struct MenuTile { juce::String label, detail; bool enabled = true, danger = false, on = false; };
    std::array<MenuTile, numPads> menuTiles() const;
    juce::String menuTitle() const;
    void menuPress (int tile);
    int menuConfirming() const;                  // a tile waiting for its second press, or -1
    // help
    bool isHelpShown() const noexcept { return helpShown.load(); }
    void toggleHelp() { helpShown = ! helpShown.load(); if (onChange) onChange(); }
    juce::StringArray helpLines() const;         // for the mode on screen
    int selectedStep() const { return selStep[(size_t) track.load()]; }
    void saveProject (int slot);
    bool loadProject (int slot);
    bool projectExists (int slot) const;

    // ---- MIDI learn: any MIDI keyboard / controller plays the box
    void startLearn();
    void learnSkip();
    void learnDone();
    void forgetMidiMap();
    bool isLearning() const noexcept { return learnIndex.load() >= 0; }
    juce::String learnPrompt() const;
    juce::String learnHint() const;
    int learnStep() const noexcept { return learnIndex.load(); }
    int learnSteps() const noexcept { return (int) learnTargets.size(); }
    juce::String learnLast() const { return learnLastText; }
    int numLearned() const;

    // ---- the link (a PC plays a box over the network)
    void setRemoteSink (std::function<void (const juce::var&)>);
    bool isRemote() const noexcept { return remoteSink != nullptr; }
    juce::var makeState() const;
    juce::var makeLive();
    void applyState (const juce::var&);
    void applyLive (const juce::var&);
    void handleLinkMessage (const juce::var&, const std::function<void (const juce::var&)>& reply);

    std::function<void()> onChange;   // the screen repaints
    /** The app switches the sound card's input on (returns false when there is none). */
    std::function<bool()> enableMicrophone;
    bool isSampling() const noexcept { return audio.capturing.load(); }
    void startSampling();
    void stopSampling();

private:
    void timerCallback() override;
    void changed (bool atBar = false);
    void setFlash (const juce::String&);
    void sendRemote (juce::DynamicObject*);
    void runOnMessageThread (std::function<void()> f);
    Track& cur() { return project.tracks[(size_t) track.load()]; }
    const Track& cur() const { return project.tracks[(size_t) track.load()]; }

    // knob handling per mode
    struct Target { int kind = -1; int index = 0; };   // what a knob slot edits
    enum Kind { KParam = 0, KFx, KMaster, KMix, KSeq, KBrowse };
    Target targetFor (int slot) const;
    float targetValue (const Target&) const;
    const ParamDef* targetDef (const Target&) const;
    void changeTarget (const Target&, float clicks, bool absoluteFraction);
    void applyRecNote (const RecNote&);
    void browseApply (bool preview);
    void browseSync();
    void pagesClamp();
    void stepTap (int step);
    void stepKey (int note, int velocity);
    void seqKnob (int slot, float clicks);
    KnobView seqKnobView (int slot) const;
    bool padDownMessageThread (int pad, int velocity);
    void loadState();
    void loadScenes();
    void saveScenes() const;
    void loadMidiMap();
    void saveMidiMap() const;
    void learnInput (int ch, int num, bool note, int value);
    void finishLearnCandidate();
    bool handleMapped (const juce::MidiMessage&);
    juce::String targetName (int t) const;

    juce::File dataDir;
    Audio audio;
    Project project;
    std::array<std::unique_ptr<Project>, numScenes> scenes;
    int loadedScene = -1;

    std::atomic<int> track { 0 }, mode { ModeSound };
    std::atomic<bool> shift { false }, sceneMode { false }, scalePads { false };
    std::array<std::atomic<int>, numTracks> octave;
    std::array<int, numTracks> soundPage {}, fxPage {}, bar {}, lane {};
    std::array<int, numTracks> selStep = [] { std::array<int, numTracks> a {}; a.fill (-1); return a; }();
    int menuPage = 0, menuConfirm = -1;
    juce::uint32 menuConfirmUntil = 0;
    std::atomic<bool> helpShown { false };
    int inputKeysDown = 0;          // step input (SEQ, stopped): keys down now
    bool inputWritten = false;      // a note went into the selected step: the next key moves on
    juce::var stateVar() const;
    void applyStateVar (const juce::var&);
    void stepInput (int note, int velocity);
    void selectStep (int step);
    int stepTargets (std::array<int, maxSteps>& out) const;   // the held steps, else the selected one
    int mixPage = 0, projectPage = 0;
    std::array<int, numTracks> presetIdx {};
    std::array<bool, numTracks> edited {};
    std::array<juce::String, numTracks> userName;     // a user sound's name (else the factory preset's)
    int browseEngine = 0, browseCat = 0, browsePos = 0;
    std::array<bool, numPads> padHeld {};
    std::array<int, maxSteps> heldSteps {};           // steps held in SEQ (order of pressing), -1 = none
    int numHeldSteps = 0;
    bool stepEdited = false;                          // a held step was edited (so letting go doesn't toggle it)
    int lastNote = 60, lastVel = 100;
    juce::uint32 chaosDownAt = 0;
    bool chaosDone = false;
    struct UndoState
    {
        Project project;
        std::array<int, numTracks> presetIdx {};
        std::array<bool, numTracks> edited {};
        std::array<juce::String, numTracks> userName;
        juce::String what;
        juce::uint32 at = 0;
    };
    std::vector<std::unique_ptr<UndoState>> undoStack;
    std::array<Step, 16> clipSteps {};
    std::array<std::array<uint8_t, 16>, numLanes> clipDrums {};
    std::array<std::array<Lock, locksPerStep>, 16> clipLocks {};
    std::array<std::array<uint8_t, 16>, numLanes> clipRolls {};
    bool hasClip = false;
    /** Remembers the state before a change. Changes of the same kind within a few seconds are one step. */
    void pushUndo (const juce::String& what, bool merge = false);
    void resetToDefaults();
    float resetProgress = 0.0f;
    std::array<std::unique_ptr<Project>, numScenes> scenesBeforeReset;
    juce::uint32 resetTouched = 0;
    juce::Array<double> taps;
    juce::String flash;
    juce::uint32 flashUntil = 0;
    std::array<bool, NumButtons> btnDown {};
    bool dirty = false;
    int saveCountdown = 0;
    bool sliderMoved = false;

    // MIDI learn
    struct MidiCtl { int ch = 1, num = 0; bool note = false; int target = -1; bool relative = false; int last = -1; };
    enum { LearnKnob0 = 0, LearnButton0 = 100, LearnPad0 = 200, LearnKnobBank = 300, LearnSlider = 301 };
    std::vector<MidiCtl> midiMap;
    juce::SpinLock midiMapLock;
    std::vector<int> learnTargets;
    std::atomic<int> learnIndex { -1 };
    std::atomic<bool> knobBank { false };
    MidiCtl learnCandidate;
    juce::Array<int> learnValues;
    bool learnCollecting = false;
    juce::uint32 learnStartedAt = 0, learnQuietUntil = 0;
    int learnIgnoreCh = -1, learnIgnoreNum = -1;
    bool learnIgnoreNote = false;
    juce::String learnLastText;

    // the link
    std::function<void (const juce::var&)> remoteSink;
    juce::var remoteUi;   // the box's screen state (while linked)

    JUCE_DECLARE_WEAK_REFERENCEABLE (Controller)
};
} // namespace catbox

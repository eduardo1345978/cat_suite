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
    Cat Box 3: what the knobs, buttons, pads and keys do. Owns the project (message thread)
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
// ---- the box, version 2 (a synth panel): 28 potentiometers, 12 push encoders, 16 step buttons, screen buttons
constexpr int numPots = 28;
constexpr int firstPotCC = 40;        // 40..67: the potentiometers (absolute 0..127; they pick the value up when they pass it)
constexpr int firstPushCC = 70;       // 70..81: the 12 encoders' pushes (push + turn = the second layer of that knob)
constexpr int screenButtonCC = 85;    // 85 HOME (86 DRUMS, 87 STUDIO: the first panel firmware)
constexpr int firstStepNote = 60;     // 60..75: the 16 step buttons (and their LEDs, sent back)
constexpr int ledScreenNote = 85, ledPlayNote = 88, ledRecNote = 89;

/** What one of the 12 knobs does right now (for the screen and the panel). */
struct KnobView
{
    bool used = false;
    juce::String name, value;
    float norm = 0.0f;       // 0..1 (where the ring is)
    bool bipolar = false;    // the ring starts in the middle
};

/** How the screen looks (the PROJECT page LOOK, the MENU). Kept for the box, not per project. */
struct Look
{
    int theme = 0, accent = 0, textSize = 0, knobStyle = 0;
    bool trackColours = false, hintLine = true;
    bool xyEverywhere = false;   // the round XY scope wherever a line scope was
    bool circleAll = false;      // every synth sound with Scope XY Off plays as Circle
};
juce::StringArray themeNames();
juce::StringArray accentNames();

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
    /** An encoder clicked (pushed and let go without turning): back to the default (in SCOPE the turns go upright),
        in the browser = take this sound, in SEQ = the step's setting back to normal (knob 1: hear the step). */
    void knobPush (int knob);
    /** HOME held + step 1-4: the pattern's page 1-4 (16 steps each; a new page starts as a copy of the last one).
        HOME + Shift + step 1-4: the pattern is that many pages long. HOME alone (let go) = the home screen. */
    void pageButton (int page, bool setLength);

    // ---- typing text on the box (no keyboard): a keyboard on the screen
    //      touch a key, or: knob 1 = move along the keys, knob 2 = up / down a row, click knob 1 = type it,
    //      click knob 2 (or Prev) = delete, click knob 12 (or Next) = OK, HOME = cancel. A PC keyboard types too.
    static const juce::String& textKeys();          // the letters; then DEL, ABC/abc, OK
    static constexpr int textCols = 11;
    int numTextKeys() const { return textKeys().length() + 3; }
    int keyDel() const { return textKeys().length(); }
    void startText (const juce::String& title, const juce::String& initial, std::function<void (const juce::String&)> done);
    bool isTyping() const noexcept { return typing; }
    const juce::String& typingTitle() const noexcept { return typeTitle; }
    const juce::String& typingText() const noexcept { return typeText; }
    int typingKey() const noexcept { return typeKey; }
    bool typingLower() const noexcept { return typeLower; }
    juce::String textKeyLabel (int i) const;
    void textPress (int key);                        // a key of the screen keyboard
    void textType (juce::juce_wchar c);              // a letter from a real keyboard
    void textFinish (bool ok);
    juce::String projectName (int slot) const;      // a saved project's name ("" = empty place)
    void homeDown (bool down);
    bool homeIsHeld() const noexcept { return homeHeld.load(); }
    int barNow() const noexcept { return bar[(size_t) track.load()]; }   // the page of the pattern on the step buttons
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
    void setVolume (int t, float v);   // 0..1 (a touch on the mixer's fader)

    // ---- SONG: the scenes one after another (MENU > Song). Play with the song on plays it from the top
    struct SongStep { int scene = 0, bars = 4; };
    static constexpr int maxSongSteps = 14;
    const std::vector<SongStep>& getSong() const noexcept { return song; }
    void setSong (const std::vector<SongStep>& s);
    bool isSongOn() const noexcept { return songOn.load(); }
    void setSongOn (bool on);
    int songStepNow() const noexcept { return songEntry; }
    int songBars() const;
    // ---- EXPORT (MENU > Export): a WAV of the song (or 8 bars of the loop), everything onto a USB stick
    void exportWav (bool wholeSong);
    bool isExporting() const noexcept { return exportJob != nullptr; }
    float exportProgress() const noexcept { return exportDone; }
    void exportToUsb();
    static juce::File usbStick();          // a USB stick plugged in (Linux / the Pi: /media/...), or none
    juce::File exportFolder() const;       // where the WAVs go (and a copy onto the stick)
    void songTick();                       // the timer calls these two (the self test too)
    void exportTick();
    void loadSound (int track, int engine, int preset);
    void setParam (int track, int index, float value);
    void setFxParam (int track, int index, float value);
    void setMaster (std::function<void (Master&)> change);
    void clearPattern (int track);
    void editPattern (int track, std::function<void (Pattern&)> change);
    void saveScene (int s);
    bool loadScene (int s, bool quiet = false);   // quiet: the song changing parts (no undo step, no message)
    bool sceneExists (int s) const { return scenes[(size_t) juce::jlimit (0, numScenes - 1, s)] != nullptr; }
    void saveUserSound();                      // asks a name (the screen keyboard), then saves
    void saveUserSoundAs (const juce::String& name);
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
    int browseEngineIndex() const { return browseEngine; }
    /** The mouse in the browser: column 0 = engine, 1 = category, 2 = sound; the sound is loaded and heard. */
    void browseTo (int column, int index);
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
    void saveProject (int slot, const juce::String& name = {});
    bool loadProject (int slot);
    bool projectExists (int slot) const;

    // ---- customization: inserts, macros, modulation, MY knobs, the look
    static constexpr int numSoundExtras = 4;      // the SOUND pages after the engine's: FX, MACRO, MOD, MY
    enum SoundExtra { ExtraFx = 0, ExtraMacro, ExtraMod, ExtraMy };
    int soundExtraPage() const;                    // -1 = an engine page
    int fxSlotNow() const { return fxSlot[(size_t) track.load()]; }
    /** A touch on the FX chain: 0 = pick the slot, -1 / 1 = the effect before / after, 2 = on / bypassed. */
    void fxTouch (int slot, int what);
    int modSlotNow() const { return modSlot[(size_t) track.load()]; }
    juce::String destName (int track, int dest, bool shortForm = false) const;
    juce::String laneName (int track, int lane) const;   // a drum lane: "Kick", or the Kit's sample
    juce::String modSlotText (int track, int slot) const;   // e.g. "LFO 1 > Cutoff +40%"
    /** The next knob you turn goes onto a macro (0-7) or the MY page (assignMy). */
    static constexpr int assignMy = 100;
    void startAssign (int what);
    int assigning() const noexcept { return assignPending; }
    void randomModulation();
    const Look& getLook() const noexcept { return look; }
    void setLook (const Look&);

    // ---- the apps: HOME (the icons), SYNTH (the instrument), DRUMS (a simple drum machine), STUDIO (everything else),
    //      SCOPE (pictures on the oscilloscope), SAMPLER (samples and the libraries), SETTINGS
    enum Screen { ScreenSynth = 0, ScreenDrums, ScreenStudio, ScreenHome, ScreenScope, ScreenSampler, ScreenSettings };
    enum App { AppSynth = 0, AppDrums, AppStudio, AppScope, AppSampler, AppSettings, NumApps };
    static juce::StringArray appNames();
    static int appScreen (int app);
    juce::String appDetail (int app) const;   // one line under the icon (what is in it now)
    int getScreen() const noexcept { return screen.load(); }
    void goSynth();
    void goDrums();
    void goStudio();
    void goHome();
    void homeButton();                     // the box's HOME button: HOME, and from HOME back to the app you came from
    void goScope();
    void goSampler();
    void goSettings();
    void openApp (int app);
    int samplerTrack() const;            // the first Sampler / Kit / Granular track (the SAMPLER app's)
    // ---- the SCOPE app: pictures on the oscilloscope (the Synth track draws them)
    void setPicture (int picture);
    void nextPicture (int dir);
    int pictureNow() const;               // -1 = the synth isn't drawing a picture
    void setScopeText (const juce::String&);
    juce::String scopeTextNow() const;
    void scopeDrawBegin();                // a new stroke (undo remembers the drawing before it)
    void setScopeDrawing (const std::vector<float>&);
    std::vector<float> scopeDrawingNow() const;
    void clearScopeDrawing();
    void setScopeHold (bool);             // a note held so the picture stays
    bool isScopeHold() const noexcept { return scopeHold; }
    static constexpr int scopeHoldNote = 48;
    int synthTrack() const;              // the first track playing the Synth (the SYNTH screen's)
    int drumTrack() const;               // the first drum track (the DRUMS screen's)
    /** The panel and the mouse: a parameter of the current track moved by a part of its travel (undo merges a turn). */
    void nudgeParam (int index, float normDelta);
    /** The panel's EFFECTS section (the send delay and reverb): codes from 1000 ("#dtime" ...). */
    static constexpr int panelFx0 = 1000;
    static int panelFxCode (const juce::String& id);   // -1 = not one
    KnobView panelFxView (int code) const;
    bool panelFxStepped (int code) const;
    void cycleParam (int index, int dir);  // a switch / choice: the next position
    void resetParam (int index);
    int hardwareKnobFor (int index) const; // which of the 12 knobs turns it now (1-24 with Shift), 0 = none
    void nextModel (int dir);             // the Synth's model
    void nextKit (int dir);               // the drum track's kit
    void toggleDrumStep (int lane, int step);
    // ---- the box v2
    static const char* potId (int pot);   // which synth knob a potentiometer is ("cutoff", "#dtime" ...); pot + numPots = with Shift
    void potMoved (int pot, int value);    // 0..127
    bool potHolds (int pot) const { return potGrab[(size_t) juce::jlimit (0, numPots * 2 - 1, pot)]; }
    void stepButton (int step, bool down);
    /** What the box's LEDs show: the 16 steps (0 off, 1 on, 2 the playing step, 3 picked), the screen, play, rec. */
    struct Leds { std::array<uint8_t, 16> steps {}; int screen = 0; bool home = false, playing = false, rec = false; };
    /** The knob turned last on the box (the screen shows it big for a moment). potNorm: where the pot is while it waits to take over. */
    struct Touch { juce::String name, value; float norm = 0.0f, potNorm = -1.0f; bool waiting = false; juce::uint32 at = 0; };
    const Touch& lastTouch() const noexcept { return touch; }
    Leds leds() const;
    void clickStep (int step);            // a melodic step (like tapping its pad in SEQ)
    void nextDrumPattern (int dir);       // the DRUMS screen: a ready-made beat
    juce::String drumPatternName() const;

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
    std::function<void()> disableMicrophone;   // after a recording: the input is closed again (an open input can make the sound stutter)
    void notify (const juce::String& s) { setFlash (s); }
    bool isSampling() const noexcept { return audio.capturing.load(); }
    void startSampling();
    void stopSampling();

private:
    void timerCallback() override;
    void changed (bool atBar = false);
    void prepareSamples();   // the samples the tracks use are read (the library loads when used)
    void setFlash (const juce::String&);
    void sendRemote (juce::DynamicObject*);
    void runOnMessageThread (std::function<void()> f);
    Track& cur() { return project.tracks[(size_t) track.load()]; }
    const Track& cur() const { return project.tracks[(size_t) track.load()]; }

    // knob handling per mode
    struct Target { int kind = -1; int index = 0; };   // what a knob slot edits
    enum Kind { KParam = 0, KFx, KMaster, KMix, KSeq, KBrowse, KIns, KInsType, KInsOn, KInsPreset, KFxSel, KMacro, KModSet,
                KModSel, KModField, KModAmt, KLook, KSendC, KSendFx, KSendFxPreset, KChain };
    Target destTarget (int dest) const;
    Target panelFxTarget (int code, int forTrack = -1) const;
    std::array<float, numPots * 2> potLast {}, potSet {};     // [pot] = the main layer, [pot + numPots] = with Shift
    std::array<bool, numPots * 2> potGrab {};
    std::array<float, numPots> potPhys {};                     // where the pot really is
    std::array<int8_t, numPots> potLayer {};                    // the layer it moved last
    std::array<std::atomic<bool>, numKnobs> encPush {};
    std::vector<SongStep> song;
    std::atomic<bool> songOn { false };
    int songEntry = -1, songQueued = -1, songBarSeen = -1;   // the part playing, the part handed to the sound for the next bar
    bool wasPlayingSong = false;
    void saveSong() const;
    void loadSong();
    juce::var songVar() const;
    void songFromVar (const juce::var&);
    struct ExportJob;
    std::unique_ptr<ExportJob> exportJob;
    float exportDone = 0.0f;
    bool typing = false, typeLower = false;
    juce::String typeTitle, typeText;
    int typeKey = 0;
    std::function<void (const juce::String&)> typeDone;
    std::atomic<bool> homeHeld { false };
    bool homeUsed = false;                                       // a step was pressed while HOME was held
    std::array<std::atomic<bool>, numKnobs> encTurned {};       // turned while pushed: that was push + turn, not a click
    std::array<std::atomic<uint32_t>, numKnobs> encPushAt {};
    int targetDest (const Target&) const;
    bool tryAssign (const Target&);
    void loadLook();
    void saveLook() const;
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
    std::array<int, numTracks> fxSlot {}, modSlot {}, insPreset {};
    std::array<bool, numTracks> chainFromSound {};   // the track's effects came with an FX Lab sound (the next sound takes them away)
    int sendFxPreset = 0, chainIdx = 0;
    std::atomic<int> screen { ScreenStudio };
    bool scopeHold = false;
    int lastApp = AppSynth;               // the app HOME was opened from
    int scopeHoldTrack = -1;
    std::array<juce::String, numTracks> scopeTextBuilt, scopeDrawBuilt;
    bool scopeSynced = false;
    void syncScope();   // the text and drawing pictures follow the project
    int drumPattern = -1;
    Touch touch;
    int assignPending = -1;
    Look look;
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
    std::atomic<int> joyTrack { -1 };   // the track the joystick bends now (MIDI thread and screen)
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

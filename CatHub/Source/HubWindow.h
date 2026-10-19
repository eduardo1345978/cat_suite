#pragma once

#include "HubEngine.h"
#include "HubLook.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace hub
{
class HubMain;

/** A knob with a small caption under it. */
class CaptionKnob : public juce::Component
{
public:
    CaptionKnob (const juce::String& caption, const juce::String& tip, bool bipolar = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider knob;
private:
    juce::String caption;
};

/** One track: a row in the rack. */
class RackRow : public juce::Component
{
public:
    RackRow (HubMain&, HubEngine&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void tick();
    int getIndex() const noexcept { return index; }

    juce::ComboBox sourceBox, fxBox, fxPreset, soundBox, soundPreset;
    juce::TextButton fxEdit { "Edit" }, soundEdit { "Edit" }, mute { "M" }, solo { "S" }, remove { "x" };
    CaptionKnob filter { "FILTER", "Darker sound (low-pass filter)" }, drive { "DRIVE", "Warm distortion" },
                chorus { "CHORUS", "Wide, shimmering doubling" }, echo { "ECHO", "Tempo echo (dotted 1/8, ping-pong)" },
                crush { "CRUSH", "Lo-fi bit crusher" }, room { "ROOM", "How much goes into the reverb" },
                pan { "PAN", "Left / right", true };
    juce::Slider volume;

private:
    void fillPresets (juce::ComboBox&, juce::AudioPluginInstance*);
    juce::AudioPluginInstance* soundPresetCat();

    HubMain& owner;
    HubEngine& engine;
    const int index;
    juce::String nowBig, nowSmall;
    juce::uint32 lastHit = 0;
    float shownMeter = 0.0f;
    int ticks = 0;
    juce::Rectangle<int> iconArea, nameArea, nowArea, meterArea;
    bool updating = false;
};

/** The rack: all tracks, scrollable, with "+ Add part". */
class RackView : public juce::Component
{
public:
    RackView (HubMain&, HubEngine&);
    void rebuild();
    void refresh();
    void tick();
    void resized() override;
    void paint (juce::Graphics&) override;
    RackRow* getRow (int i) { return juce::isPositiveAndBelow (i, rows.size()) ? rows[i] : nullptr; }
    juce::TextButton addButton { "+  Add a part" };
private:
    HubMain& owner;
    HubEngine& engine;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<RackRow> rows;
};

/** Chord-Cat's pads: the one playing lights up; hold one with the mouse or keys 1-8. */
class PadStrip : public juce::Component
{
public:
    explicit PadStrip (HubEngine& e) : engine (e) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
private:
    int padAt (juce::Point<float>) const;
    HubEngine& engine;
    int shownPad = -2, shownPads = 0;
};

/** A Cat's own window inside a tab, centred and scaled to fit. */
class CatTab : public juce::Component
{
public:
    explicit CatTab (juce::AudioPluginInstance&);
    ~CatTab() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    int naturalW = 0, naturalH = 0;
};

/** Compose: the song writer. Genre, mood, seed, song / endless, "new ..." for each part,
    the song's map with the playhead, the words, and export. */
class ComposePage : public juce::Component
{
public:
    ComposePage (HubMain&, HubEngine&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();
    void tick();
    void showSettings (const cat::muse::Settings&);

    juce::ComboBox genreBox, moodBox;
    juce::TextEditor seedBox;
    juce::TextButton newSeedButton { "New seed" }, songButton { "Song" }, endlessButton { "Endless" }, writeButton { "Write a song!" },
                     exportButton { "Export..." }, jamButton { "Back to jamming" };
    juce::OwnedArray<juce::TextButton> renewButtons;

private:
    cat::muse::Settings settingsFromControls() const;
    void rebuildMap();
    juce::Rectangle<float> mapArea() const { return mapBounds.toFloat(); }

    HubMain& owner;
    HubEngine& engine;
    juce::Image mapImage;
    juce::Rectangle<int> headerArea, mapBounds, lyricsArea, renewLabelArea;
    const cat::muse::Song* shownSong = nullptr;
    double shownBeat = -1.0;
    int shownLine = -2;
};

/** Settings: theme, mode, audio, where the Cats are, tutorial, reset. */
class SettingsPage : public juce::Component
{
public:
    SettingsPage (HubMain&, HubEngine&, juce::AudioDeviceManager&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    juce::ComboBox themeBox;
    juce::ToggleButton advancedToggle { "Advanced mode: effects, MIDI-effect Cats, extra parts" };
    juce::TextButton tutorialButton { "Show the tutorial again" }, findButton { "Find Cats folder..." }, resetButton { "Start a fresh jam" };
private:
    HubMain& owner;
    HubEngine& engine;
    juce::AudioDeviceSelectorComponent audio;
    juce::Rectangle<int> catsArea;
};

/** First-launch tutorial: bubbles that point at the real controls. */
class Tutorial : public juce::Component
{
public:
    struct Step { juce::String title, text; std::function<juce::Rectangle<int>()> target; };
    Tutorial (std::vector<Step>, std::function<void()> onFinish);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override {}
    void next();
    int getStep() const noexcept { return step; }
private:
    juce::Rectangle<int> bubbleBounds() const;
    std::vector<Step> steps;
    std::function<void()> onFinish;
    int step = 0;
    juce::TextButton nextButton { "Next" }, skipButton { "Skip tutorial" };
};

//==============================================================================
class HubMain : public juce::Component, private juce::Timer
{
public:
    HubMain (HubEngine&, juce::AudioDeviceManager&, juce::PropertiesFile&);
    ~HubMain() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

    // things the rows and pages ask for
    void showCatWindow (juce::AudioPluginInstance*);
    /** Closes the Cats' windows, runs fn (which may load / remove Cats), then rebuilds everything. */
    void changeCats (std::function<void()> fn);
    void changeCatsLater (std::function<void()> fn);
    void setAdvanced (bool);
    bool isAdvanced() const noexcept { return advanced; }
    void applyTheme (int index);
    void startTutorial();
    void findCats();
    /** Writes the song (or each part as a stem) to Music \ Cat Hub, faster than real time. */
    void exportSong (int what);       // 0 = WAV, 1 = stems, 2 = MIDI
    void songChanged();
    void refreshAll();
    juce::PropertiesFile& getSettings() { return settings; }

    // for the tests
    bool selectTab (int index) { if (index >= tabs.getNumTabs()) return false; tabs.setCurrentTabIndex (index); return true; }
    ComposePage* getComposePage() { return composePage; }
    void tickForSnapshot() { timerCallback(); }
    void restoreStateSafely (const juce::ValueTree& t) { changeCats ([this, t] { engine.restoreState (t); }); }
    bool isTutorialShowing() const { return tutorial != nullptr; }
    void finishTutorialForTests() { tutorial.reset(); }

private:
    void timerCallback() override;
    void togglePlay();
    void toggleRecord();
    void rebuildTabs();
    void closeCatTabs();
    void updateTopBar();
    void refreshSongBar();
    std::vector<Tutorial::Step> tutorialSteps();
    juce::Rectangle<int> boundsOf (juce::Component&) const;

    HubEngine& engine;
    juce::AudioDeviceManager& deviceManager;
    juce::PropertiesFile& settings;
    HubLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };
    bool advanced = false;

    // transport bar
    juce::TextButton playButton { "Play" }, recordButton { "Rec" }, simpleButton { "Simple" }, advancedButton { "Advanced" },
                     helpButton { "?" }, diceButton { "Dice" };
    juce::Slider bpm, master;
    // song bar
    juce::ComboBox styleBox, keyBox, progressionBox, lengthBox;
    juce::ToggleButton autoChords { "Song plays the pads" };

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    RackView* rack = nullptr;
    ComposePage* composePage = nullptr;
    SettingsPage* settingsPage = nullptr;
    bool exporting = false;
    PadStrip pads { engine };
    std::unique_ptr<Tutorial> tutorial;

    juce::Rectangle<int> logoArea, positionArea, masterMeterArea, songLabelArea, padsLabelArea;
    juce::String status;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::ThreadWithProgressWindow> exportJob;
    int heldKeyPad = -1, heldKeyCode = 0, ticks = 0;
    bool shownPlaying = false, shownRecording = false;
    float shownMasterMeter = 0.0f;
    double shownPpq = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HubMain)
};
} // namespace hub

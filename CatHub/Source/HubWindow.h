#pragma once

#include "HubEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "CatLookAndFeel.h"

namespace hub
{
class HubMain;

/** One mixer card: a Cat, its sound, volume, echo (reverb), mute / solo and a meter. */
class CatCard : public juce::Component
{
public:
    CatCard (HubMain&, HubEngine&, int part);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void tick();

private:
    HubMain& owner;
    HubEngine& engine;
    const int part;
    juce::Slider volume, reverb;
    juce::TextButton mute { "M" }, solo { "S" }, open;
    juce::ComboBox sound;
    juce::Label soundLabel, volLabel, revLabel;
    float shownMeter = 0.0f;
};

/** Chord-Cat's pads: the one playing lights up; hold one with the mouse (or keys 1-8). */
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

/** The Jam page: the four cards and the pads. */
class JamPage : public juce::Component
{
public:
    JamPage (HubMain&, HubEngine&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void tick();

private:
    HubEngine& engine;
    juce::OwnedArray<CatCard> cards;
    PadStrip pads;
    juce::Label padsLabel;
};

/** A Cat's own window inside a tab (scrolls when it is bigger than the Hub). */
class CatTab : public juce::Component
{
public:
    CatTab (juce::AudioPluginInstance&);
    ~CatTab() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    int naturalW = 0, naturalH = 0;
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

    void showCat (int catId);
    void rebuildTabs();
    void updateGlideTab();
    juce::String getMissingText() const;

private:
    void timerCallback() override;
    void togglePlay();
    void toggleRecord();
    void showAudioSettings();
    void findCats();
    void updateTopBar();

    HubEngine& engine;
    juce::AudioDeviceManager& deviceManager;
    juce::PropertiesFile& settings;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    juce::TextButton playButton { "Play" }, recordButton { "Rec" }, audioButton { "Audio" }, findButton { "Find Cats..." };
    juce::Slider bpm, master;
    juce::ComboBox chordLength;
    juce::ToggleButton autoChords { "Song plays the pads" };
    juce::Label bpmLabel, masterLabel, lengthLabel, positionLabel, statusLabel;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    JamPage* jam = nullptr;
    std::array<int, NumCats> tabIndexForCat {};

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::DialogWindow> audioWindow;
    int heldKeyPad = -1;
    int heldKeyCode = 0;
    bool shownPlaying = false, shownRecording = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HubMain)
};
} // namespace hub

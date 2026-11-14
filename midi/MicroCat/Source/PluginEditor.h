#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"
#include "ScaledEditor.h"

/** The astrolabe: the tuning as a brass circle (one turn = the period), 12-EDO's notes as the dial's
    ticks and names, the tuning's notes as brass marks joined in a star. The notes sounding now glow and
    the needle points at the last one played. Drag a mark to move the note (in cents), double-click an
    empty spot to add a note there, double-click a mark (or right-click) to remove it. */
class TuningCircle : public juce::Component, public juce::SettableTooltipClient
{
public:
    TuningCircle();
    void setTuning (const micro::Tuning& t) { tuning = t; repaint(); }
    const micro::Tuning& getTuning() const noexcept { return tuning; }
    int selected = 1;                                  // degree 1 .. N-1 (0 is the first note and never moves)
    std::function<void (const micro::Tuning&)> onChange;
    std::function<void()> onSelect;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    /** Cents of a point (its angle). */
    double centsAt (juce::Point<float>) const;
    juce::Point<float> pointFor (double cents, float radius) const;
    int degreeAt (juce::Point<float>) const;           // -1 = none
    /** What sounds now: cents above the first note (within the period). The needle points at `needle` (-1 = the selected note). */
    void setSounding (std::vector<double> cents, double needle) { sounding = std::move (cents); needleCents = needle; repaint(); }

private:
    juce::Rectangle<float> ring() const;
    micro::Tuning tuning;
    std::vector<double> sounding;
    double needleCents = -1.0;
    int dragging = -1;
    bool moved = false;
};

/** Two and a half octaves of keys from below the root: each shows how far it is from normal tuning
    (in cents), the root is marked, held notes light up; click a key to hear it. */
class TuningKeyboard : public juce::Component, private juce::Timer
{
public:
    explicit TuningKeyboard (MicroCatProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    int firstKey() const;
    int keyAt (juce::Point<float>) const;
    static constexpr int numKeys = 30;
private:
    void timerCallback() override;
    MicroCatProcessor& processor;
    std::array<bool, 128> shownHeld {};
    int playing = -1, shownVersion = -1, shownRoot = -1, shownMap = -1;
};

class MicroCatEditor : public juce::Component, private juce::Timer, private juce::ListBoxModel
{
public:
    explicit MicroCatEditor (MicroCatProcessor&);
    ~MicroCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // the tuning list (ListBoxModel)
    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;

    /** For the tests: pick a tuning by name, change the selected note. */
    bool pickByName (const juce::String& name);
    void setSelectedCents (double cents);

private:
    void timerCallback() override;
    void refreshList();
    void showTuning();                                  // the circle, the note editor, the name
    void edited (micro::Tuning t);                      // the editor changed the tuning
    void makeEqual (int n);
    void showTab (int tab);
    /** Which keys sound now and what they really are (the microtones), for the read-out and the astrolabe. */
    void updateSounding();
    struct Sounding { int key; double semis, cents; };   // cents above the tuning's first note, within the period

    MicroCatProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "Tuning" };
    juce::TextEditor search;
    juce::OwnedArray<juce::TextButton> categoryButtons;
    int category = -1;                                  // -1 = all
    juce::ListBox list { "tunings", this };
    std::vector<micro::Tuning> userTunings, shown;
    std::vector<int> shownLibraryIndex;                 // -1 = a user tuning

    TuningCircle circle;
    juce::TextEditor nameBox, centsBox;
    juce::TextButton downButton { "-1" }, upButton { "+1" }, addButton { "Add note" }, removeButton { "Remove note" },
                     saveButton { "Save as mine" }, loadButton { "Open file..." }, exportButton { "Export .scl..." },
                     newButton { "New equal..." }, concertButton { "A4 = 440 Hz" };
    TuningKeyboard keyboard { processor };

    cat::ParamCombo rootBox { processor.apvts, "rootKey", "Root", nullptr, 44 },
                    mapBox { processor.apvts, "mapping", "Keys", nullptr, 44 },
                    rangeBox { processor.apvts, "bendRange", "Bend", nullptr, 44 },
                    justBox { processor.apvts, "liveJust", "Live just", nullptr, 60 };
    juce::ComboBox tuningBBox;
    juce::Slider morph;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttach;
    juce::Rectangle<int> morphLabelArea;
    juce::String shownB;
    juce::Slider rootHz, voices;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rootHzAttach, voicesAttach;
    cat::ParamToggle configToggle { processor.apvts, "sendConfig", "Send MPE set-up" }, bypassToggle { processor.apvts, "bypass", "Bypass" },
                     mtsToggle { processor.apvts, "mtsEsp", "MTS-ESP master" };
    juce::TextButton mtsResetButton { "Reset MTS-ESP" };
    juce::TextButton catalogueTab { "Catalogue" }, instrumentTab { "Instrument" };
    int tab = 0;
    std::array<bool, 128> shownHeld {};
    std::vector<Sounding> soundingNow;
    int lastKey = -1;                                   // the last key played (-1 = show the selected note)
    juce::Rectangle<int> asideArea, readoutArea, rowsArea, rightArea;
    // its own sound
    cat::ParamToggle ownSoundToggle { processor.apvts, "ownSound", "Own sound" };
    cat::ParamCombo soundBox { processor.apvts, "sound" };
    cat::LineSlider volumeSlider { processor.apvts, "volume", "Volume" };
    juce::Rectangle<int> mtsStatusArea;
    juce::String mtsStatus;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> logoArea, listHeader, infoArea, noteLabelArea, settingsArea, helpArea;
    juce::String status;
    int shownVersion = -1;
    bool updatingList = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicroCatEditor)
};

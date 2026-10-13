#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "CatVisualizer.h"
#include "Surprise.h"
#include "PresetBar.h"

class ChordCatEditor;


//==============================================================================
/** One chord pad: chord name chip over a voicing graphic (Chordimist style). */
class ChordPad : public juce::Component
{
public:
    ChordPad (ChordCatEditor& owner, int index);

    void setContent (const chordcat::ProgressionSlot& slot, const std::vector<int>& notes, const juce::String& roman);
    void setState (bool selected, bool playing, float playheadFraction);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    ChordCatEditor& owner;
    const int index;
    chordcat::ProgressionSlot slot;
    std::vector<int> notes;
    juce::String roman;
    bool selected = false, playing = false, auditioning = false, dragStarted = false;
    float playhead = -1.0f;
};

//==============================================================================
/** Piano-roll view of all pads; dragging a voice edits its custom offset. */
class RollView : public juce::Component
{
public:
    explicit RollView (ChordCatEditor& owner);
    void setContent (const chordcat::Progression& p, const std::array<std::vector<int>, 16>& voicings, int numPads, int selected, int playing);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> noteRect (int column, int note) const;
    juce::Rectangle<float> gridArea() const;
    std::pair<int, int> hitTestNote (juce::Point<float>) const;

    ChordCatEditor& owner;
    chordcat::Progression progression;
    std::array<std::vector<int>, 16> voicings;
    int numPads = 8, selected = 0, playingSlot = -1, lowNote = 36, highNote = 84;
    int dragColumn = -1, dragVoice = -1, dragDelta = 0;
    float dragStartY = 0.0f;
};

//==============================================================================
class CircleOfFifths : public juce::Component
{
public:
    std::function<void (int)> onRootSelected;
    void setRoot (int root, int keyRoot) { selectedRoot = root; key = keyRoot; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int selectedRoot = 0, key = 0;
};

//==============================================================================
class EditPanel : public juce::Component
{
public:
    EditPanel (ChordCatEditor& owner, ChordCatProcessor& p);
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void edit (const std::function<void (chordcat::ProgressionSlot&)>& fn);

    ChordCatEditor& owner;
    ChordCatProcessor& processor;
    CircleOfFifths circle;
    juce::ComboBox typeBox, voicingBox, tensionBox, rhythmBox;
    std::array<juce::TextButton, 4> inversionButtons;
    std::unique_ptr<cat::Stepper> octaveStepper;
    juce::Slider strumOverride;
    juce::TextButton activeButton { "Pad On" };
    juce::Label nameLabel, romanLabel;

    cat::Knob voiceLeading, transpose, velocity;
    cat::ParamToggle legato, bassSplit;
    std::unique_ptr<cat::Stepper> bassChannel, inChannel, outChannel, triggerLow, triggerHigh;
    cat::ParamCombo triggerMode;
    bool updating = false;
};

//==============================================================================
/** Live view of the strum: every note the engine sends as a dot on a time line (last 2.5 s). */
class StrumDisplay : public juce::Component, private juce::Timer
{
public:
    explicit StrumDisplay (ChordCatProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    ChordCatProcessor& processor;
};

/** STRUM tab: the strum engine's controls, presets and dice. */
class StrumPanel : public juce::Component
{
public:
    explicit StrumPanel (ChordCatProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ChordCatProcessor& processor;
    std::unique_ptr<cat::Stepper> preset, stack, target, window, repeats;
    juce::TextButton dice { "Dice" };
    cat::ParamCombo sync, direction, shape, burst;
    cat::BarSlider time, curve, tilt, human, velRnd, mute;
    cat::ParamToggle roundTrip, release;
    StrumDisplay display;
};

//==============================================================================
class PerformPanel : public juce::Component, private juce::Timer
{
public:
    PerformPanel (ChordCatEditor& owner, ChordCatProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    ChordCatEditor& owner;
    ChordCatProcessor& processor;
    cat::Knob strum, strumShape, strumCurve, humanTime, humanVel, swing, velSpread, octSpread;
    cat::ParamCombo strumDir, strumSync, groove;
    cat::ParamToggle followGroove;
    juce::TextButton recordButton { "Capture" };
    cat::DragButton dragCaptureButton { "Drag Capture" };
};

//==============================================================================
class GeneratePanel : public juce::Component, private juce::Timer
{
public:
    GeneratePanel (ChordCatEditor& owner, ChordCatProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void generate();

private:
    void timerCallback() override;
    void chooseImportFile();

    ChordCatEditor& owner;
    ChordCatProcessor& processor;
    cat::ParamCombo mood, genre, complexity, keyRoot, scale;
    cat::ParamToggle scaleLock, chordTrackSync;
    juce::TextButton generateButton { "Generate" }, moodScaleButton { "Mood sets scale" }, importButton { "Import MIDI / Chord Track..." };
    std::unique_ptr<cat::Stepper> chordCount;
    int numChords = 8;
    std::array<juce::TextButton, 9> historyButtons;
    std::array<juce::TextButton, 12> customScaleButtons;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    PresetBrowser (ChordCatEditor& owner, ChordCatProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refreshList();

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    struct Entry { juce::String name, tags; int factoryIndex; juce::File file; };

    ChordCatEditor& owner;
    ChordCatProcessor& processor;
    juce::TextEditor search;
    juce::OwnedArray<juce::TextButton> tagButtons;
    juce::ListBox list;
    juce::TextButton saveButton { "Save" }, loadButton { "Load" };
    std::vector<Entry> entries;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
class ChordCatEditor : public juce::Component,
                       public juce::DragAndDropContainer,
                       private juce::Timer
{
public:
    explicit ChordCatEditor (ChordCatProcessor&);
    ~ChordCatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Used by child components
    ChordCatProcessor& getProcessor() noexcept { return processor; }
    void selectSlot (int slot);
    int getSelectedSlot() const { return processor.selectedSlot.load(); }
    void showPadMenu (int slot);
    void dragSlotOut (int slot);
    void dragSequenceOut (const juce::MidiMessageSequence& seq, const juce::String& name);
    void refreshFromProcessor();
    static juce::Colour colourForChord (const chordcat::ChordDefinition&);

private:
    void timerCallback() override;
    void layoutPads();
    void setRollView (bool roll);

    ChordCatProcessor& processor;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // Top bar
    cat::ParamCombo keyCombo, scaleCombo;
    std::unique_ptr<cat::Stepper> padsStepper;
    std::unique_ptr<cat::PresetBar> presetBar;
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" }, aButton { "A" }, bButton { "B" };
    cat::DragButton dragButton { "Drag MIDI" };

    // Toolbar
    juce::TextButton padsViewButton { "Pads" }, rollViewButton { "Roll" };
    juce::TextButton generateButton { "Generate" };
    cat::ParamCombo genreCombo, moodCombo;
    juce::TextButton diceRoot { "Dice Root" }, diceType { "Dice Type" }, diceVoicing { "Dice Voicing" };

    juce::OwnedArray<ChordPad> pads;
    juce::Component padArea;
    RollView roll { *this };
    bool rollVisible = false;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    EditPanel* editPanel = nullptr;
    GeneratePanel* generatePanel = nullptr;
    PresetBrowser browser;
    cat::PianoDisplay keyboard;

    uint32_t shownVersion = 0;
    int shownPads = -1;
    std::array<std::vector<int>, 16> voicings;
    chordcat::Progression progression;
    chordcat::GlobalState lastGlobal;
    std::optional<chordcat::ChordDefinition> clipboard;


    // Music-reactive cat, boot screen, Surprise / undo
    cat::CatVisualizer catView;
    cat::SurpriseControls surpriseControls;
    cat::BootScreen bootScreen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordCatEditor)
};

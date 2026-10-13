#pragma once

#include <set>
#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "CatVisualizer.h"
#include "Surprise.h"
#include "PresetBar.h"

/**
    The piano roll: notes with lyrics (and the phonemes they turn into), the sung pitch traced
    on top while the song plays.

      Draw tool    click = new note (drag for its length), drag a note = move, drag its end = length
      Select tool  drag on empty space = rubber band, drag notes = move
      Erase tool   click / drag over notes deletes them
      Double-click a note = type its lyric (Tab jumps to the next note, several words fill the following notes)
      Right-click a note = menu (phonemes, split, language, delete ...)
      Keys: Delete, Ctrl+A / C / V / D / Z / Y, arrows (Shift = octave / bar), wheel = scroll, Ctrl+wheel = zoom
*/
class PianoRoll : public juce::Component, public juce::SettableTooltipClient, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    explicit PianoRoll (TalkCatProcessor&);
    ~PianoRoll() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // view
    float beatToX (double beat) const;
    double xToBeat (float x) const;
    float pitchToY (float pitch) const;
    float yToPitch (float y) const;
    juce::Rectangle<float> gridArea() const;
    static constexpr int keysWidth = 52, rulerHeight = 20, barThickness = 12;

    void zoomTime (float factor);
    void zoomPitch (float factor);
    int tool = 0;                 // 0 draw, 1 select, 2 erase
    double snap = 0.25;
    bool follow = true;

    // editing
    talkcat::Score edit;
    std::set<int> selection;
    void commit();                 // edit -> processor (one undo step)
    void reload();                 // processor -> edit
    void selectAll();
    void deleteSelection();
    void copySelection();
    void paste();
    void duplicateSelection();
    void transposeSelection (int semis);
    void moveSelection (double beats);
    void editLyric (int noteId);
    void splitNote (int noteId);
    std::vector<int> selectedIndices() const;

    std::function<void()> onViewChanged, onSelectionChanged;

private:
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void updateScrollBars();
    int noteAt (juce::Point<float>, bool& onEdge) const;
    juce::Rectangle<float> noteRect (const talkcat::ScoreNote&) const;
    double snapBeat (double b) const;
    void previewNote (const talkcat::ScoreNote&);
    void showNoteMenu (int noteId);
    void finishLyricEdit (bool commitText, int moveBy);

    TalkCatProcessor& processor;
    juce::ScrollBar hBar { false }, vBar { true };
    double scrollBeat = 0.0;
    float topPitch = 80.5f, rowH = 15.0f;
    float pxPerBeat = 64.0f;

    enum class Drag { None, Create, Move, Resize, Band, Erase, Keys };
    Drag drag = Drag::None;
    juce::Point<float> dragStart;
    std::vector<talkcat::ScoreNote> dragOriginals;
    int dragNoteId = -1;
    double lastLength = 1.0;
    int lastPreviewPitch = -1;
    juce::Rectangle<float> band;
    bool changed = false;
    uint32_t seenVersion = 0;
    int hoverNote = -1;
    bool hoverEdge = false;

    struct TrailPoint { double beat; float pitch; };
    std::vector<TrailPoint> trail;
    double lastTrailBeat = -1.0;

    struct LyricEditor : public juce::TextEditor
    {
        std::function<void (int)> onTab;
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k.getKeyCode() == juce::KeyPress::tabKey) { if (onTab) onTab (k.getModifiers().isShiftDown() ? -1 : 1); return true; }
            return juce::TextEditor::keyPressed (k);
        }
    };
    std::unique_ptr<LyricEditor> lyricEditor;
    int lyricNoteId = -1;
    static std::vector<talkcat::ScoreNote>& clipboard();
};

/** Curve lanes under the roll (pitch bend, dynamics, breath, brightness, gender, tension, growl, vibrato, cat, mouth). */
class LaneEditor : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    LaneEditor (TalkCatProcessor&, PianoRoll&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> area() const;
    float valueToY (float v) const;
    float yToValue (float y) const;

    TalkCatProcessor& processor;
    PianoRoll& roll;
    juce::ComboBox laneBox;
    juce::TextButton clearButton { "Clear" };
    int lane = talkcat::LaneDynamics;
    std::vector<talkcat::CurvePoint> stroke;
    bool erasing = false;
    double dragFrom = 0.0, dragTo = 0.0;
};

/** The singer's vocal tract (larynx -> lips) as it moves. */
class TractView : public juce::Component, private juce::Timer
{
public:
    explicit TractView (TalkCatProcessor& p) : processor (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    TalkCatProcessor& processor;
    std::array<float, talkcat::numCtrl> smooth {};
};

/** A titled group of controls (combos / toggles on the left, knobs on the right). */
class SingSection : public juce::Component
{
public:
    SingSection (juce::String title, juce::Colour colour) : titleText (std::move (title)), tint (colour) {}
    void addLeft (juce::Component* c) { left.push_back (c); addAndMakeVisible (c); }
    void addKnob (juce::Component* c) { knobs.push_back (c); addAndMakeVisible (c); }
    void paint (juce::Graphics&) override;
    void resized() override;
    int leftWidth = 150;
private:
    juce::String titleText;
    juce::Colour tint;
    std::vector<juce::Component*> left, knobs;
};

/** Base for tab pages: owns its widgets. */
class SingPage : public juce::Component
{
public:
    explicit SingPage (TalkCatProcessor& p) : processor (p) {}
protected:
    cat::Knob* knob (const juce::String& id, const juce::String& caption, juce::Colour c = cat::Colours::accent);
    cat::ParamCombo* combo (const juce::String& id, const juce::String& caption, int captionW = 56);
    SingSection* section (const juce::String& title, juce::Colour c);
    void layoutRow (juce::Rectangle<int> area, std::initializer_list<std::pair<juce::Component*, float>> parts);
    TalkCatProcessor& processor;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<SingSection> sections;
};

class TalkCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit TalkCatEditor (TalkCatProcessor&);
    ~TalkCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void showSongsMenu();
    void importMidi();

    TalkCatProcessor& processor;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    cat::PresetBar presetBar;
    juce::TextButton songsButton { "Songs" }, undoButton { "Undo" }, redoButton { "Redo" };
    cat::ParamCombo mode, languageBox, styleBox;
    cat::DragButton dragButton { "Drag MIDI" };

    // tool bar
    std::array<juce::TextButton, 3> toolButtons;
    juce::ComboBox snapBox;
    juce::TextButton zoomOut { "-" }, zoomIn { "+" }, rowsOut { "v" }, rowsIn { "^" };
    std::unique_ptr<cat::Stepper> barsStepper, startStepper;
    juce::ToggleButton loopToggle { "Loop" }, followToggle { "Follow" };
    juce::TextButton recordButton { "Rec" };
    juce::Label infoLabel;

    PianoRoll roll;
    LaneEditor lanes;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    cat::CatVisualizer catView;
    cat::SurpriseControls surpriseControls;
    cat::BootScreen bootScreen;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TalkCatEditor)
};

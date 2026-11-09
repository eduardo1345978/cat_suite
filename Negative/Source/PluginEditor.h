#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"

namespace negui
{
/** A hairline slider: caption, a line, a dot, the value under it. */
class Line : public juce::Slider
{
public:
    Line (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& caption);
    void paint (juce::Graphics&) override;

private:
    juce::String caption;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

/** A quiet pill button (filled = the main one, or on). */
class Chip : public juce::TextButton
{
public:
    explicit Chip (const juce::String& label, bool isPrimary = false) : juce::TextButton (label), primary (isPrimary) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    bool primary;
};

/** The fonts you can write with. */
struct TextFont { juce::String name; juce::Font font (float height) const; const char* data = nullptr; int size = 0; };
const std::vector<TextFont>& textFonts();

/** The photo, and the place to paint on it: brush, eraser, text in different fonts, undo. */
class PhotoView : public juce::Component
{
public:
    enum class Tool { brush, eraser, text };
    explicit PhotoView (NegativeProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void refresh();
    void undo();
    void clear();
    bool canUndo() const noexcept { return ! history.empty(); }

    Tool tool = Tool::brush;
    float brush = 0.3f;                 // 0..1: brush size
    juce::String text { "HELLO" };
    int fontIndex = 0;
    float textSize = 0.35f;             // 0..1: text height (part of the photo's height)

    /** Draws the text into an image at a point given in the image's own pixels (also used by the tests). */
    static void stampText (juce::Image&, juce::Point<float> centre, const juce::String&, int fontIndex, float size);
    juce::Point<float> toImage (juce::Point<float> viewPos) const;

private:
    juce::Rectangle<float> imageArea() const;
    void paintAt (juce::Point<float>, bool erase);
    void remember();
    void commit();
    NegativeProcessor& processor;
    juce::Image shown;
    std::vector<juce::Image> history;
    bool stroking = false, hovering = false;
    juce::Point<float> last, hover;
};

/** The spectrogram of what comes out, with Hz and bars, and where it plays now. */
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (NegativeProcessor&);
    void paint (juce::Graphics&) override;
    void refresh();

private:
    NegativeProcessor& processor;
    juce::Image pic;
};

/** "drag .wav": drag the sound straight onto a Cubase track. */
class DragChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DragChip (NegativeProcessor& p) : processor (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    NegativeProcessor& processor;
    bool busy = false;
};
} // namespace negui

class NegativeEditor : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit NegativeEditor (NegativeProcessor&);
    ~NegativeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragMove (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void timerCallback() override;
    void openPhoto();
    void openSound();
    void savePicture();
    void saveWav();
    bool loadAny (const juce::File&, bool asSong);
    int dropZone = 0;                   // while a file is dragged over: 1 = the photo, 2 = the song

    NegativeProcessor& processor;
    juce::StringArray modes = negative::modeNames();
    juce::OwnedArray<negui::Chip> modeTabs;
    cat::ParamCombo key { processor.apvts, "key", "key", nullptr, 26 }, scale { processor.apvts, "scale", "", nullptr, 0 };
    negui::PhotoView photo { processor };
    negui::SpectrumView spectrum { processor };
    std::vector<std::unique_ptr<negui::Line>> lines;
    negui::Chip brushB { "Brush" }, eraserB { "Eraser" }, textB { "Text" }, undoB { "Undo" }, clearB { "Clear" };
    juce::Slider brushSize { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox }, textSize { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextEditor textBox;
    juce::ComboBox fontBox;
    void setTool (negui::PhotoView::Tool);
    negui::Chip openPhotoB { "Open photo / sound" }, openSoundB { "Hide in a song" }, songX { "x" },
                invertB { "Negative" }, syncB { "Play with Cubase" }, playB { "Play", true }, saveWavB { "Save .wav" }, savePicB { "Save picture" };
    juce::AudioProcessorValueTreeState::ButtonAttachment invertA { processor.apvts, "invert", invertB }, syncA { processor.apvts, "sync", syncB };
    negui::DragChip dragChip { processor };
    std::unique_ptr<juce::FileChooser> chooser;
    int lastRender = -1;
    juce::String status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NegativeEditor)
};

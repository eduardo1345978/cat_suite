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

/** The photo (and the place to draw on it). */
class PhotoView : public juce::Component
{
public:
    explicit PhotoView (NegativeProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool drawing = false;
    void refresh();

private:
    juce::Rectangle<float> imageArea() const;
    void paintAt (juce::Point<float>, bool erase);
    NegativeProcessor& processor;
    juce::Image shown;
    bool stroking = false;
    juce::Point<float> last;
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

private:
    void timerCallback() override;
    void openPhoto();
    void openSound();
    void savePicture();
    void saveWav();
    bool loadAny (const juce::File&);

    NegativeProcessor& processor;
    juce::StringArray modes = negative::modeNames();
    juce::OwnedArray<negui::Chip> modeTabs;
    cat::ParamCombo key { processor.apvts, "key", "key", nullptr, 26 }, scale { processor.apvts, "scale", "", nullptr, 0 };
    negui::PhotoView photo { processor };
    negui::SpectrumView spectrum { processor };
    std::vector<std::unique_ptr<negui::Line>> lines;
    negui::Chip openPhotoB { "Open photo" }, openSoundB { "Open sound" }, drawB { "Draw" }, clearB { "Clear" }, songX { "x" },
                invertB { "Negative" }, syncB { "Play with Cubase" }, playB { "Play", true }, saveWavB { "Save .wav" }, savePicB { "Save picture" };
    juce::AudioProcessorValueTreeState::ButtonAttachment invertA { processor.apvts, "invert", invertB }, syncA { processor.apvts, "sync", syncB };
    negui::DragChip dragChip { processor };
    std::unique_ptr<juce::FileChooser> chooser;
    int lastRender = -1;
    juce::String status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NegativeEditor)
};

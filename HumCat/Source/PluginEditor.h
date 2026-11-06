#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"

/** The big round record button with the input meter round it. */
class RecButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit RecButton (HumCatProcessor& p) : processor (p) { setTooltip ("Record: sing or hum into the mic on this track. Press again to stop"); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void()> onToggle;
private:
    HumCatProcessor& processor;
};

/** What you sang: the pitch curve and the notes it heard (top), and the song made from it (bottom). */
class SongView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SongView (HumCatProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void refresh();
    bool dropHover = false;
private:
    void paintHeard (juce::Graphics&, juce::Rectangle<float>);
    void paintSong (juce::Graphics&, juce::Rectangle<float>);
    HumCatProcessor& processor;
    std::shared_ptr<const humcat::PitchTrack> track;
    std::shared_ptr<const humcat::Song> song;
    std::vector<humcat::Note> sung;
};

class HumCatEditor : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit HumCatEditor (HumCatProcessor&);
    ~HumCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { view.dropHover = true; view.repaint(); }
    void fileDragExit (const juce::StringArray&) override { view.dropHover = false; view.repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void loadFile (const juce::File&);
    void dragOut (int part);   // -1 = the whole song
    void chooseFile();

    HumCatProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "Preset" };
    RecButton rec { processor };
    juce::TextButton loadButton { "Load recording" }, clearButton { "Clear" }, demoButton { "Demo tune" }, playButton { "Play" }, dragSong { "Drag song to Cubase" };
    SongView view { processor };
    std::unique_ptr<juce::FileChooser> chooser;

    struct PartStrip
    {
        std::unique_ptr<cat::ParamToggle> on;
        std::unique_ptr<juce::TextButton> drag;
        std::unique_ptr<cat::ParamCombo> sound;
    };
    std::array<PartStrip, humcat::NumParts> parts;

    cat::ParamCombo style { processor.apvts, "style", "Style", nullptr, 52 },
                    chordRate { processor.apvts, "chordRate", "Chords", nullptr, 58 },
                    key { processor.apvts, "key", "Key", nullptr, 40 },
                    scale { processor.apvts, "scale", "Scale", nullptr, 46 },
                    grid { processor.apvts, "grid", "Grid", nullptr, 42 };
    cat::ParamToggle snap { processor.apvts, "snap", "Snap to key" }, trim { processor.apvts, "trim", "Start at 1st note" },
                     withHost { processor.apvts, "withHost", "Play with Cubase" }, ownSound { processor.apvts, "ownSound", "Own sound" };
    cat::Knob sensitivity { processor.apvts, "sensitivity", "Ears" }, leadOct { processor.apvts, "leadOct", "Lead oct" },
              tempo { processor.apvts, "tempo", "Tempo" }, volume { processor.apvts, "volume", "Volume" };

    juce::String status;
    uint32_t shownVersion = 0;
    bool wasAnalysing = false, dragging = false;
    juce::Rectangle<int> header, micArea, partsArea, settingsArea, statusArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HumCatEditor)
};

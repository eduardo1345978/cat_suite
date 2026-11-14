#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"

/** The broken screen: every bug a pixel flying round its note (left / right = where you hear it,
    up / down = its pitch), trails, the drums flashing, and the picture tearing when the swarm glitches.
    The strip at the bottom is a keyboard you can click (the low keys play the drums). */
class SwarmScreen : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SwarmScreen (GlitchCatProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();

private:
    int keyAt (juce::Point<float>) const;
    juce::Rectangle<float> keyboardArea() const;
    GlitchCatProcessor& processor;
    juce::Image layer;
    static constexpr int trail = 6;
    struct Trail { std::array<juce::Point<float>, trail> p {}; int n = 0; };
    std::array<Trail, glitch::Engine::maxNotes * glitch::Engine::bugsPerNote> trails;
    float lo = 40.0f, hi = 90.0f;
    int playing = -1;
    juce::uint32 frame = 0;
    juce::Random rng;
};

/** MADNESS: the big knob. A ring of pixels from acid green to error red; the text breaks up as it turns. */
class MadnessKnob : public juce::Slider
{
public:
    MadnessKnob();
    void paint (juce::Graphics&) override;
    juce::uint32 frame = 0;
private:
    juce::Random rng;
};

class GlitchCatEditor : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit GlitchCatEditor (GlitchCatProcessor&);
    ~GlitchCatEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHover = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void timerCallback() override;
    void loadVoice (const juce::File&);

    GlitchCatProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "Preset" };
    juce::TextButton mutateButton { "MUTATE" };
    SwarmScreen screen { processor };
    MadnessKnob madness;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> madnessAttach;
    cat::ParamToggle detune { processor.apvts, "doDetune", "DETUNE" }, rolls { processor.apvts, "doRolls", "ROLLS" },
                     stutter { processor.apvts, "doStutter", "STUTTER + REVERSE" }, odd { processor.apvts, "doOdd", "ODD RHYTHM" };

    cat::Knob swarm { processor.apvts, "swarm", "Swarm" }, flight { processor.apvts, "flight", "Flight" }, tape { processor.apvts, "tape", "Tape" },
              attack { processor.apvts, "attack", "Attack" }, release { processor.apvts, "release", "Release" },
              synth { processor.apvts, "synthLevel", "Synth" }, tone { processor.apvts, "tone", "Tone" }, shape { processor.apvts, "shape", "Shape" },
              voice { processor.apvts, "voiceLevel", "Voice" }, drums { processor.apvts, "drumLevel", "Drums" },
              space { processor.apvts, "space", "Space" }, volume { processor.apvts, "volume", "Volume" }, tempo { processor.apvts, "tempo", "Tempo" };
    cat::ParamCombo voiceType { processor.apvts, "voiceType" }, kit { processor.apvts, "kit", "Kit", nullptr, 36 },
                    beatMode { processor.apvts, "beatMode" }, beatStyle { processor.apvts, "beatStyle", "Style", nullptr, 44 };
    juce::TextButton loadButton { "Load voice" }, recButton { "REC" }, clearButton { "X" };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> logoArea, statusArea, madnessArea;
    std::array<juce::Rectangle<int>, 5> panels;
    juce::Rectangle<int> voiceInfo;
    juce::String status;
    bool dropHover = false;
    juce::uint32 frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchCatEditor)
};

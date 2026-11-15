#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"

/** The path of 16 torii gates: drag a gate up / down to set its level, double-click: on / off, shift-drag: a line. */
class ToriiPath : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ToriiPath (GateCatProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    int stepAtX (float x) const;
    float levelAtY (float y) const;
    juce::Rectangle<float> area() const;
    void setStep (int k, float v);
    GateCatProcessor& processor;
    std::array<juce::RangedAudioParameter*, gate::maxSteps> params {};
    juce::Point<float> last, lineStart;
    bool lineMode = false;
    std::array<bool, gate::maxSteps> grabbed {};
};

/** A rotary knob: a vermilion arc for the value, the name and the value under it. */
class ToriiKnob : public juce::Component
{
public:
    ToriiKnob (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& name);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }

private:
    struct Invisible : juce::Slider { using Slider::Slider; void paint (juce::Graphics&) override {} };
    Invisible slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::RangedAudioParameter* param = nullptr;
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class GateCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit GateCatEditor (GateCatProcessor&);
    ~GateCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setAll (std::function<float (int)>);
    void setParam (const juce::String& id, float value);

    GateCatProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "" };
    ToriiPath path { processor };
    std::vector<std::unique_ptr<ToriiKnob>> knobs;
    juce::TextButton clearB { "Clear" }, fillB { "Fill" }, randomB { "Random" }, shiftB { "Shift" };
    juce::TextButton aB { "A" }, bB { "B" };
    std::array<juce::TextButton, 6> rateB;
    juce::TextButton stepsDown { "-" }, stepsUp { "+" };
    float meterL = 0.0f, meterR = 0.0f, gateShown = 0.0f;
    juce::Rectangle<int> sideArea, knobArea, meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateCatEditor)
};

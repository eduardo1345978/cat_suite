#pragma once

#include "PluginProcessor.h"
#include "CryptLook.h"
#include "PresetBar.h"

class CryptFxEditor : public juce::Component, private juce::Timer
{
public:
    explicit CryptFxEditor (CryptFxProcessor&);
    ~CryptFxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    CryptFxProcessor& processor;
    tomb::look::Laf laf;
    juce::TooltipWindow tips { this, 600 };
    cat::PresetBar presetBar { processor.presetManager, "" };
    juce::OwnedArray<tomb::look::PixelSlider> sliders;
    float time = 0.0f, jaw = 0.0f;
    juce::uint32 lastTick = 0;
    juce::Rectangle<float> screenArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CryptFxEditor)
};

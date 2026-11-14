#pragma once

#include "PluginProcessor.h"
#include "CryptLook.h"
#include "PresetBar.h"

/** The screen of the synth: the wave (in pixels), the envelope, a skull that sings along. */
class CryptScreen : public juce::Component
{
public:
    explicit CryptScreen (CryptProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    float time = 0.0f, jaw = 0.0f;

private:
    CryptProcessor& processor;
};

/** The sound-effect picture: its wave in pixel columns, where the preview is. */
class SfxScreen : public juce::Component
{
public:
    explicit SfxScreen (CryptProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void setSound (const juce::AudioBuffer<float>& b, double sr);
    juce::String message;

private:
    CryptProcessor& processor;
    std::vector<float> peaks;
    float seconds = 0.0f;
};

/** A knob that is not a plug-in parameter (the effect maker's DARKNESS, PITCH, LENGTH). */
class SfxKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    SfxKnob (const juce::String& caption, float lo, float hi, float def, std::function<juce::String (float)> text, juce::Colour);
    void paint (juce::Graphics&) override;
    void resized() override { slider.setBounds (getLocalBounds()); }
    juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::function<void (float)> onChange;

private:
    juce::String caption;
    std::function<juce::String (float)> text;
    juce::Colour colour;
};

class CryptEditor : public juce::Component, private juce::Timer
{
public:
    explicit CryptEditor (CryptProcessor&);
    ~CryptEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showTab (int);
    void refreshSfx (bool play);
    void saveToGame();
    void saveAs();
    void chooseFolder();
    void rebuildHistory();

    CryptProcessor& processor;
    tomb::look::Laf laf;
    juce::TooltipWindow tips { this, 600 };
    cat::PresetBar presetBar { processor.presetManager, "" };
    tomb::look::PixelButton synthTab { "Synth", tomb::look::ember }, sfxTab { "Sound FX", tomb::look::poison },
                             darkTab { "Dark arts", tomb::look::blood };

    // synth
    CryptScreen screen { processor };
    juce::ComboBox wave, chord, arp, arpRate;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveA, chordA, arpA, rateA;
    tomb::look::PixelButton steps { "16 steps", tomb::look::flame };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stepsA;
    juce::OwnedArray<tomb::look::PixelSlider> sliders;
    juce::MidiKeyboardComponent keys { processor.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard };

    // sound effects
    SfxScreen sfxScreen { processor };
    juce::OwnedArray<tomb::look::PixelButton> kinds;
    tomb::look::PixelButton generate { "Generate", tomb::look::ember }, mutate { "Mutate", tomb::look::violet },
                             play { "Play", tomb::look::poison }, back { "Back", tomb::look::boneDim },
                             saveGame { "Save to game", tomb::look::flame }, saveAsB { "Save as...", tomb::look::bone },
                             folderB { "Folder...", tomb::look::boneDim }, packB { "Make a whole pack (14 x 3)", tomb::look::ember };
    SfxKnob darkness { "Darkness", 0.0f, 1.0f, 0.5f, [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }, tomb::look::violet },
            pitch { "Pitch", -12.0f, 12.0f, 0.0f, [] (float v) { const int s = juce::roundToInt (v); return (s > 0 ? "+" : "") + juce::String (s) + " st"; }, tomb::look::poison },
            length { "Length", 0.5f, 2.0f, 1.0f, [] (float v) { return "x" + juce::String (v, 2); }, tomb::look::flame };
    juce::OwnedArray<tomb::look::PixelButton> historyButtons;
    std::unique_ptr<juce::FileChooser> chooser;

    float time = 0.0f;
    juce::uint32 lastTick = 0;
    std::vector<juce::Component*> synthParts, sfxParts, darkParts;
    juce::OwnedArray<tomb::look::PixelSlider> darkSliders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CryptEditor)
};

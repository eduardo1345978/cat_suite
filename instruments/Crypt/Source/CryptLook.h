#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

/** The pixel look of Crypt and Crypt FX: a 5x7 pixel font, a stone wall, torches, a skull. */
namespace tomb::look
{
const juce::Colour stone (0xff15131c), stoneLight (0xff221f2c), mortar (0xff0b0a10), bone (0xffe8dcc0), boneDim (0xff8a8070),
                   ember (0xffff7a2e), flame (0xffffc94a), poison (0xff7dff9a), blood (0xffd02848), violet (0xff9b6cff), ink (0xff07060a);

/** Draws text in the 5x7 pixel font (capitals, digits, a few signs); `px` is the size of one pixel. Returns the width. */
float pixelText (juce::Graphics&, const juce::String&, float x, float y, float px, juce::Colour, bool shadow = true);
float pixelTextWidth (const juce::String&, float px);
/** Centred in the box. */
void pixelTextCentred (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float px, juce::Colour, bool shadow = true);

/** A dungeon wall of pixel bricks, lit by torches (light 0..1 flickers). */
void wall (juce::Graphics&, juce::Rectangle<float>, float light, float px = 4.0f);
/** A pixel torch with a flame that moves (t = seconds). */
void torch (juce::Graphics&, juce::Point<float> base, float t, float px = 4.0f);
/** A pixel skull; jaw 0..1 opens its mouth, eyes glow with the colour. */
void skull (juce::Graphics&, juce::Point<float> centre, float px, float jaw, juce::Colour eyes);
/** A dark pixel panel with a stone frame. */
void panel (juce::Graphics&, juce::Rectangle<float>, float px = 3.0f);

/** A button drawn as a pixel stone block with pixel text. */
class PixelButton : public juce::Button
{
public:
    explicit PixelButton (const juce::String& text, juce::Colour accent = bone);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::Colour accent;
    float textPx = 2.0f;
};

/** A parameter as a row of pixel blocks: the caption, the blocks (drag / click / wheel / double-click resets), the value. */
class PixelSlider : public juce::Component, private juce::AudioProcessorParameter::Listener, private juce::AsyncUpdater
{
public:
    PixelSlider (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& caption, juce::Colour colour);
    ~PixelSlider() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }

private:
    void parameterValueChanged (int, float) override { triggerAsyncUpdate(); }
    void parameterGestureChanged (int, bool) override {}
    void handleAsyncUpdate() override { repaint(); }
    struct Invisible : juce::Slider { using juce::Slider::Slider; void paint (juce::Graphics&) override {} };
    Invisible slider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::RangedAudioParameter* param;
    juce::String caption;
    juce::Colour colour;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** The look for the sliders and combos (dark, pixel-ish). */
class Laf : public juce::LookAndFeel_V4
{
public:
    Laf();
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
};
} // namespace tomb::look

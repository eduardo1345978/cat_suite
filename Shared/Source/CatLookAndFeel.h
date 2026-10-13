#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cat
{
namespace Colours
{
    // Minimal, near-monochrome palette: flat dark surfaces, one soft blue accent.
    inline const juce::Colour background  { 0xff0f1013 };
    inline const juce::Colour panel       { 0xff15161a };
    inline const juce::Colour panelLight  { 0xff1b1c21 };
    inline const juce::Colour raised      { 0xff24262c };
    inline const juce::Colour outline     { 0xff2a2c33 };
    inline const juce::Colour text        { 0xffe4e6ea };
    inline const juce::Colour textDim     { 0xff7a7e88 };
    inline const juce::Colour accent      { 0xff8fa3f0 };
    inline const juce::Colour blue        { 0xff7f93c9 };
    inline const juce::Colour purple      { 0xff9a88c4 };
    inline const juce::Colour red         { 0xffc27a78 };
    inline const juce::Colour teal        { 0xff6fa3a8 };
    inline const juce::Colour green       { 0xff7fb393 };
    inline const juce::Colour slate       { 0xff8a95b0 };

    /** Palette used for chord pads / layers / voices. */
    juce::Colour palette (int index);
}

juce::Font catFont (float height, bool bold = false);

/** Flat, minimal dark look shared by Chord-Cat, Patt-Cat and Bend-Cat. */
class CatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CatLookAndFeel();

    static void drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float corner = 8.0f,
                                juce::Colour tint = Colours::panel);
    static void drawTriangle (juce::Graphics& g, juce::Rectangle<float> bounds, bool pointsRight, juce::Colour colour);
    static void drawLogo (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& first, const juce::String& second);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;

    int getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int w, int h) override;

    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isScrollbarVertical,
                        int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown) override;
};

} // namespace cat

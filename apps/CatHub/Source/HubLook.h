#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CatLookAndFeel.h"

namespace hub
{
/** A colour theme for the Hub (picked in Settings). */
struct Theme
{
    juce::String name;
    juce::Colour bg, panel, panelHi, raised, outline, text, textDim, accent, rackEar, screw;
    std::array<juce::Colour, 5> source;   // Chords, Bass, Drums, Life, Lead
    bool light = false;
};

const std::vector<Theme>& themes();
const Theme& theme();                  // the current one
void setTheme (int index);
int themeIndex();

inline juce::Font hubFont (float h, bool bold = false) { return cat::catFont (h, bold); }

/** Flat studio-rack look that follows the current theme. */
class HubLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HubLookAndFeel() { applyTheme(); }
    void applyTheme();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return hubFont (juce::jmin (14.0f, h * 0.48f), true); }
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return hubFont (juce::jmin (14.0f, b.getHeight() * 0.5f)); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return hubFont (14.5f); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getLabelFont (juce::Label& l) override { return hubFont (juce::jmin (14.0f, l.getHeight() * 0.7f)); }
    int getTabButtonBestWidth (juce::TabBarButton&, int depth) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool over, bool down) override;
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
};
} // namespace hub

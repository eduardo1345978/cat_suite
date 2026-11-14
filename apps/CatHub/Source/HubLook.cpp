#include "HubLook.h"

namespace hub
{
namespace { int currentTheme = 0; }

const std::vector<Theme>& themes()
{
    using C = juce::Colour;
    static const std::vector<Theme> t = {
        { "Midnight", C (0xff0f1013), C (0xff17181d), C (0xff1e2026), C (0xff272a31), C (0xff30333b), C (0xffe6e8ec), C (0xff80848e),
          C (0xff8fa3f0), C (0xff22242a), C (0xff3a3d45),
          { C (0xff8fa3f0), C (0xff9a88c4), C (0xffd08a7e), C (0xff7fb393), C (0xffd9b26a) } },
        { "Ocean", C (0xff071419), C (0xff0d1f26), C (0xff12282f), C (0xff19343c), C (0xff22414a), C (0xffe2f3f6), C (0xff7a9aa3),
          C (0xff4fd1c5), C (0xff0f242b), C (0xff2c4a52),
          { C (0xff4fd1c5), C (0xff5aa9e6), C (0xffff8c69), C (0xff8bd17c), C (0xfff4d35e) } },
        { "Sunset", C (0xff1a0f14), C (0xff24151c), C (0xff2d1b23), C (0xff3a222c), C (0xff472b36), C (0xfffbe9e4), C (0xffa58b90),
          C (0xffff9e64), C (0xff2a1820), C (0xff563442),
          { C (0xffff9e64), C (0xffe86a92), C (0xfff25f5c), C (0xffffd166), C (0xffc98bdb) } },
        { "Forest", C (0xff0d130e), C (0xff141c15), C (0xff1a241b), C (0xff223024), C (0xff2c3b2e), C (0xffe7f0e3), C (0xff86967f),
          C (0xff9bd27c), C (0xff18211a), C (0xff3a4b3c),
          { C (0xff9bd27c), C (0xffc7a76c), C (0xffd9825b), C (0xff6fc3a0), C (0xffe8d77b) } },
        { "Candy", C (0xff1b1426), C (0xff251b33), C (0xff2d2240), C (0xff382a4f), C (0xff45345f), C (0xfffff1fb), C (0xffb19cc4),
          C (0xffff7ac6), C (0xff2a1f3a), C (0xff56437a),
          { C (0xffff7ac6), C (0xff8f7bff), C (0xff5ce1e6), C (0xffb8f36b), C (0xffffd36e) } },
        { "Paper (light)", C (0xffeeebe5), C (0xfff8f6f2), C (0xffffffff), C (0xffe4e0d8), C (0xffd2cdc3), C (0xff23252b), C (0xff6e6a63),
          C (0xff4a68d8), C (0xffdcd7cd), C (0xffbcb6aa),
          { C (0xff4a68d8), C (0xff8a5cc7), C (0xffd4553d), C (0xff3f9a62), C (0xffc48a12) }, true },
    };
    return t;
}

const Theme& theme() { return themes()[(size_t) juce::jlimit (0, (int) themes().size() - 1, currentTheme)]; }
void setTheme (int i) { currentTheme = juce::jlimit (0, (int) themes().size() - 1, i); }
int themeIndex() { return currentTheme; }

//==============================================================================
void HubLookAndFeel::applyTheme()
{
    const auto& t = theme();
    setColour (juce::ResizableWindow::backgroundColourId, t.bg);
    setColour (juce::DocumentWindow::backgroundColourId, t.bg);
    setColour (juce::Label::textColourId, t.text);
    setColour (juce::TextButton::buttonColourId, t.raised);
    setColour (juce::TextButton::buttonOnColourId, t.accent);
    setColour (juce::TextButton::textColourOffId, t.text);
    setColour (juce::TextButton::textColourOnId, t.light ? juce::Colours::white : t.bg);
    setColour (juce::ToggleButton::textColourId, t.text);
    setColour (juce::ToggleButton::tickColourId, t.accent);
    setColour (juce::ComboBox::backgroundColourId, t.raised);
    setColour (juce::ComboBox::textColourId, t.text);
    setColour (juce::ComboBox::outlineColourId, t.outline);
    setColour (juce::ComboBox::arrowColourId, t.textDim);
    setColour (juce::PopupMenu::backgroundColourId, t.panelHi);
    setColour (juce::PopupMenu::textColourId, t.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId, t.text);
    setColour (juce::PopupMenu::headerTextColourId, t.textDim);
    setColour (juce::Slider::textBoxTextColourId, t.text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, t.panelHi);
    setColour (juce::TooltipWindow::textColourId, t.text);
    setColour (juce::TooltipWindow::outlineColourId, t.outline);
    setColour (juce::ScrollBar::thumbColourId, t.outline);
    setColour (juce::TextEditor::backgroundColourId, t.raised);
    setColour (juce::TextEditor::textColourId, t.text);
    setColour (juce::TextEditor::outlineColourId, t.outline);
    setColour (juce::TextEditor::focusedOutlineColourId, t.accent);
    setColour (juce::ListBox::backgroundColourId, t.panel);
    setColour (juce::ListBox::textColourId, t.text);
    setColour (juce::AlertWindow::backgroundColourId, t.panelHi);
    setColour (juce::AlertWindow::textColourId, t.text);
    setColour (juce::TabbedComponent::backgroundColourId, t.bg);
    setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::CaretComponent::caretColourId, t.accent);
    setColour (juce::HyperlinkButton::textColourId, t.accent);
    getCurrentColourScheme().setUIColour (juce::LookAndFeel_V4::ColourScheme::windowBackground, t.panel);
    getCurrentColourScheme().setUIColour (juce::LookAndFeel_V4::ColourScheme::widgetBackground, t.raised);
    getCurrentColourScheme().setUIColour (juce::LookAndFeel_V4::ColourScheme::menuBackground, t.panelHi);
    getCurrentColourScheme().setUIColour (juce::LookAndFeel_V4::ColourScheme::outline, t.outline);
    getCurrentColourScheme().setUIColour (juce::LookAndFeel_V4::ColourScheme::defaultText, t.text);
    getCurrentColourScheme().setUIColour (juce::LookAndFeel_V4::ColourScheme::defaultFill, t.accent);
    getCurrentColourScheme().setUIColour (juce::LookAndFeel_V4::ColourScheme::highlightedText, t.text);
    getCurrentColourScheme().setUIColour (juce::LookAndFeel_V4::ColourScheme::highlightedFill, t.accent.withAlpha (0.35f));
    getCurrentColourScheme().setUIColour (juce::LookAndFeel_V4::ColourScheme::menuText, t.text);
}

void HubLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto& t = theme();
    const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId, true);
    const auto fill = colour == juce::Colour() || colour == findColour (juce::Slider::rotarySliderFillColourId) ? t.accent : colour;
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float radius = size * 0.5f, line = juce::jmax (2.0f, size * 0.09f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - line, radius - line, 0.0f, start, end, true);
    g.setColour (t.outline);
    g.strokePath (track, juce::PathStrokeType (line, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float angle = start + pos * (end - start);
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, radius - line, radius - line, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (fill);
        g.strokePath (arc, juce::PathStrokeType (line, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // knob cap with a pointer
    const float capR = radius - line * 2.4f;
    g.setColour (t.panelHi);
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
    g.setColour (t.outline.brighter (0.15f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2, 1.0f);
    const juce::Point<float> tip (c.x + std::sin (angle) * capR * 0.8f, c.y - std::cos (angle) * capR * 0.8f);
    g.setColour (s.isEnabled() ? t.text : t.textDim);
    g.drawLine ({ c, tip }, juce::jmax (1.5f, line * 0.7f));
}

void HubLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    const auto& t = theme();
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto colour = s.findColour (juce::Slider::trackColourId, true);
    const auto fill = s.getProperties().contains ("fill") ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["fill"]) : t.accent;
    juce::ignoreUnused (colour);
    if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
    {
        g.setColour (t.raised);
        g.fillRoundedRectangle (r, 5.0f);
        auto f = style == juce::Slider::LinearBar ? r.withRight (pos) : r.withTop (pos);
        g.setColour (fill.withAlpha (s.isEnabled() ? 0.55f : 0.2f));
        g.fillRoundedRectangle (f, 5.0f);
        return;
    }
    // horizontal fader: groove + cap
    const bool horiz = style == juce::Slider::LinearHorizontal;
    auto groove = horiz ? r.withSizeKeepingCentre (r.getWidth() - 8, 4.0f) : r.withSizeKeepingCentre (4.0f, r.getHeight() - 8);
    g.setColour (t.outline);
    g.fillRoundedRectangle (groove, 2.0f);
    g.setColour (fill);
    if (horiz) g.fillRoundedRectangle (groove.withRight (pos), 2.0f);
    else g.fillRoundedRectangle (groove.withTop (pos), 2.0f);
    const auto cap = horiz ? juce::Rectangle<float> (10.0f, juce::jmin (r.getHeight(), 22.0f)).withCentre ({ pos, r.getCentreY() })
                           : juce::Rectangle<float> (juce::jmin (r.getWidth(), 26.0f), 10.0f).withCentre ({ r.getCentreX(), pos });
    g.setColour (t.panelHi.brighter (t.light ? 0.0f : 0.25f));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (t.outline.brighter (0.2f));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.setColour (fill);
    if (horiz) g.drawLine (cap.getCentreX(), cap.getY() + 4, cap.getCentreX(), cap.getBottom() - 4, 2.0f);
    else g.drawLine (cap.getX() + 4, cap.getCentreY(), cap.getRight() - 4, cap.getCentreY(), 2.0f);
}

juce::Label* HubLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (hubFont (12.5f, true));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, theme().text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

void HubLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgColour, bool over, bool down)
{
    const auto& t = theme();
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto c = on ? b.findColour (juce::TextButton::buttonOnColourId) : bgColour;
    if (over) c = c.brighter (t.light ? 0.04f : 0.12f);
    if (down) c = c.darker (0.15f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (on ? c.brighter (0.2f) : t.outline);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
}

void HubLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const auto& t = theme();
    const bool on = b.getToggleState();
    g.setColour (! b.isEnabled() ? t.textDim.withAlpha (0.5f)
                                 : (on ? b.findColour (juce::TextButton::textColourOnId) : b.findColour (juce::TextButton::textColourOffId)));
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), juce::Justification::centred, 2);
}

void HubLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto& t = theme();
    auto r = b.getLocalBounds().toFloat();
    auto sw = r.removeFromLeft (36.0f).withSizeKeepingCentre (32.0f, 18.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? t.accent : t.outline);
    g.fillRoundedRectangle (sw, 9.0f);
    g.setColour (on ? (t.light ? juce::Colours::white : t.bg) : (over ? t.text : t.textDim));
    const float kx = on ? sw.getRight() - 16.0f : sw.getX() + 2.0f;
    g.fillEllipse (kx, sw.getY() + 2.0f, 14.0f, 14.0f);
    g.setColour (b.isEnabled() ? t.text : t.textDim);
    g.setFont (hubFont (13.5f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft, 2);
}

void HubLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto& t = theme();
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isEnabled() ? t.raised : t.raised.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (box.hasKeyboardFocus (true) ? t.accent : t.outline);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (box.isEnabled() ? t.textDim : t.textDim.withAlpha (0.4f));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void HubLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, box.isEnabled() ? theme().text : theme().textDim);
}

void HubLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    const auto& t = theme();
    g.fillAll (t.panelHi);
    g.setColour (t.outline);
    g.drawRect (0, 0, w, h);
}

int HubLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& b, int)
{
    return juce::jmax (90, (int) juce::GlyphArrangement::getStringWidth (hubFont (14.0f, true), b.getButtonText()) + 36);
}

void HubLookAndFeel::drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool)
{
    const auto& t = theme();
    auto r = b.getLocalBounds().toFloat().reduced (3.0f, 4.0f);
    const bool front = b.isFrontTab();
    if (front || over)
    {
        g.setColour (front ? t.raised : t.panelHi);
        g.fillRoundedRectangle (r, 8.0f);
    }
    if (front)
    {
        g.setColour (t.accent);
        g.fillRoundedRectangle (r.removeFromBottom (3.0f).reduced (14.0f, 0.0f), 1.5f);
    }
    g.setColour (front ? t.text : t.textDim);
    g.setFont (hubFont (14.0f, front));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void HubLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                    int thumbStart, int thumbSize, bool over, bool)
{
    const auto& t = theme();
    juce::Rectangle<float> thumb = vertical ? juce::Rectangle<float> ((float) x + 2, (float) thumbStart, (float) w - 4, (float) thumbSize)
                                            : juce::Rectangle<float> ((float) thumbStart, (float) y + 2, (float) thumbSize, (float) h - 4);
    g.setColour (over ? t.textDim : t.outline);
    g.fillRoundedRectangle (thumb, 3.0f);
}

void HubLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    const auto& t = theme();
    g.fillAll (t.panelHi);
    g.setColour (t.outline);
    g.drawRect (0, 0, w, h);
    g.setColour (t.text);
    g.setFont (hubFont (13.0f));
    g.drawFittedText (text, juce::Rectangle<int> (0, 0, w, h).reduced (8, 4), juce::Justification::centredLeft, 6);
}
} // namespace hub

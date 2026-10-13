#include "CatLookAndFeel.h"

namespace cat
{
juce::Colour Colours::palette (int index)
{
    // Muted, low-saturation tones: they still tell chord families / layers / voices apart
    // without shouting. Same order as before so existing colour roles stay consistent.
    static const juce::Colour colours[] = {
        juce::Colour (0xff8a9bd0), juce::Colour (0xffa08fcc), juce::Colour (0xffc48a88), juce::Colour (0xff76aab0),
        juce::Colour (0xff8590ad), juce::Colour (0xffb194c4), juce::Colour (0xff7e9bd6), juce::Colour (0xffc4a184),
    };
    return colours[(size_t) (((index % 8) + 8) % 8)];
}

juce::Font catFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height).withStyle (bold ? "Bold" : "Regular"));
}

CatLookAndFeel::CatLookAndFeel()
{
    using namespace juce;
    setColourScheme ({ Colours::panel, Colours::background, Colours::panelLight, Colours::outline, Colours::text,
                       Colours::accent, Colours::background, Colours::accent, Colours::text });

    setColour (ResizableWindow::backgroundColourId, Colours::background);
    setColour (Label::textColourId, Colours::text);
    setColour (TextButton::buttonColourId, Colours::panelLight);
    setColour (TextButton::buttonOnColourId, Colours::accent);
    setColour (TextButton::textColourOffId, Colours::text);
    setColour (TextButton::textColourOnId, Colours::text);
    setColour (ComboBox::backgroundColourId, Colours::panelLight);
    setColour (ComboBox::textColourId, Colours::text);
    setColour (ComboBox::outlineColourId, Colours::outline);
    setColour (ComboBox::arrowColourId, Colours::textDim);
    setColour (PopupMenu::backgroundColourId, Colours::panelLight);
    setColour (PopupMenu::highlightedBackgroundColourId, Colours::raised);
    setColour (PopupMenu::highlightedTextColourId, Colours::text);
    setColour (PopupMenu::textColourId, Colours::text);
    setColour (PopupMenu::headerTextColourId, Colours::textDim);
    setColour (Slider::textBoxTextColourId, Colours::text);
    setColour (Slider::textBoxOutlineColourId, Colours::background.withAlpha (0.0f));
    setColour (Slider::textBoxBackgroundColourId, Colours::background.withAlpha (0.0f));
    setColour (Slider::rotarySliderFillColourId, Colours::accent);
    setColour (Slider::rotarySliderOutlineColourId, Colours::raised);
    setColour (Slider::trackColourId, Colours::accent);
    setColour (Slider::backgroundColourId, Colours::panelLight);
    setColour (Slider::thumbColourId, Colours::text);
    setColour (TextEditor::backgroundColourId, Colours::panelLight);
    setColour (TextEditor::textColourId, Colours::text);
    setColour (TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (TextEditor::focusedOutlineColourId, Colours::accent.withAlpha (0.6f));
    setColour (TextEditor::highlightColourId, Colours::accent.withAlpha (0.4f));
    setColour (CaretComponent::caretColourId, Colours::accent);
    setColour (ListBox::backgroundColourId, Colours::panel);
    setColour (ScrollBar::thumbColourId, Colours::raised);
    setColour (TabbedComponent::backgroundColourId, Colours::panel);
    setColour (TabbedComponent::outlineColourId, Colours::outline);
    setColour (TabbedButtonBar::tabTextColourId, Colours::textDim);
    setColour (TabbedButtonBar::frontTextColourId, Colours::text);
    setColour (ToggleButton::textColourId, Colours::text);
    setColour (ToggleButton::tickColourId, Colours::accent);
    setColour (TooltipWindow::backgroundColourId, Colours::panelLight);
    setColour (TooltipWindow::textColourId, Colours::text);
    setColour (AlertWindow::backgroundColourId, Colours::panelLight);
    setColour (AlertWindow::textColourId, Colours::text);
}

void CatLookAndFeel::drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> b, float corner, juce::Colour tint)
{
    // Flat surface with a hairline edge (kept the old name so every panel picks up the new look)
    g.setColour (tint);
    g.fillRoundedRectangle (b, corner);
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    g.drawRoundedRectangle (b.reduced (0.5f), corner, 1.0f);
}

void CatLookAndFeel::drawTriangle (juce::Graphics& g, juce::Rectangle<float> r, bool right, juce::Colour c)
{
    juce::Path p;
    const auto s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const auto cx = r.getCentreX(), cy = r.getCentreY();
    if (right) p.addTriangle (cx - s * 0.5f, cy - s * 0.6f, cx - s * 0.5f, cy + s * 0.6f, cx + s * 0.55f, cy);
    else       p.addTriangle (cx + s * 0.5f, cy - s * 0.6f, cx + s * 0.5f, cy + s * 0.6f, cx - s * 0.55f, cy);
    g.setColour (c);
    g.fillPath (p);
}

void CatLookAndFeel::drawLogo (juce::Graphics& g, juce::Rectangle<float> b, const juce::String& first, const juce::String& second)
{
    auto f1 = catFont (b.getHeight() * 0.72f, true);
    auto f2 = catFont (b.getHeight() * 0.72f, false);
    juce::GlyphArrangement ga;
    ga.addLineOfText (f1, first, 0.0f, 0.0f);
    const float w1 = ga.getBoundingBox (0, -1, true).getWidth();

    g.setFont (f1);
    g.setColour (Colours::text);
    g.drawText (first, b.withWidth (w1 + 4.0f), juce::Justification::centredLeft);
    g.setFont (f2);
    g.setColour (Colours::textDim);
    g.drawText (second, b.withTrimmedLeft (w1 + 2.0f), juce::Justification::centredLeft);
}

void CatLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                       float startAngle, float endAngle, juce::Slider& slider)
{
    // Thin arc + value arc + a small dot: no knob body, no gradients
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float lineW = juce::jmax (2.0f, radius * 0.08f);
    const float arcRadius = radius - lineW;
    const float toAngle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::raised);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                                    : startAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         juce::jmin (zeroAngle, toAngle), juce::jmax (zeroAngle, toAngle), true);
    const auto colour = slider.isEnabled() ? slider.findColour (juce::Slider::rotarySliderFillColourId) : Colours::textDim;
    g.setColour (colour);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Indicator: short line from the centre area towards the arc
    const auto dir = juce::Point<float> (std::sin (toAngle), -std::cos (toAngle));
    g.setColour (Colours::text);
    g.drawLine (juce::Line<float> (centre + dir * (arcRadius * 0.35f), centre + dir * (arcRadius * 0.72f)), juce::jmax (1.5f, lineW * 0.8f));
}

void CatLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                       float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                                       juce::Slider& slider)
{
    auto r = juce::Rectangle<int> (x, y, width, height).toFloat();

    if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
    {
        g.setColour (Colours::panelLight);
        g.fillRoundedRectangle (r, 3.0f);
        auto fill = style == juce::Slider::LinearBar ? r.withRight (sliderPos) : r.withTop (sliderPos);
        g.setColour (slider.findColour (juce::Slider::trackColourId).withAlpha (slider.isEnabled() ? 0.35f : 0.15f));
        g.fillRoundedRectangle (fill, 3.0f);
        return;
    }

    const bool horizontal = slider.isHorizontal();
    const float thickness = 3.0f;
    auto trackRect = horizontal ? juce::Rectangle<float> (r.getX(), r.getCentreY() - thickness * 0.5f, r.getWidth(), thickness)
                                : juce::Rectangle<float> (r.getCentreX() - thickness * 0.5f, r.getY(), thickness, r.getHeight());
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (trackRect, 2.0f);

    auto valueRect = horizontal ? trackRect.withRight (sliderPos) : trackRect.withTop (sliderPos);
    g.setColour (slider.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (valueRect, 2.0f);

    const float thumb = 10.0f;
    auto thumbPos = horizontal ? juce::Point<float> (sliderPos, r.getCentreY()) : juce::Point<float> (r.getCentreX(), sliderPos);
    g.setColour (Colours::text);
    g.fillEllipse (juce::Rectangle<float> (thumb, thumb).withCentre (thumbPos));
    juce::ignoreUnused (minSliderPos, maxSliderPos);
}

juce::Label* CatLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (catFont (12.0f));
    l->setColour (juce::Label::textColourId, Colours::text);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

void CatLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                                           bool over, bool down)
{
    // Flat: off = quiet surface, on = soft tint of the button's "on" colour. No borders.
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto base = on ? b.findColour (juce::TextButton::buttonOnColourId).withAlpha (0.2f) : bg;
    if (down)      base = base.brighter (0.06f);
    else if (over) base = base.brighter (0.1f);

    g.setColour (base);
    g.fillRoundedRectangle (r, 4.0f);
}

void CatLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    const auto colour = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId).brighter (0.35f)
                                           : b.findColour (juce::TextButton::textColourOffId);
    g.setColour (colour.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1, 0.8f);
}

juce::Font CatLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return catFont (juce::jmin (13.5f, (float) h * 0.5f), false);
}

void CatLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);

    // Thin chevron
    juce::Path chevron;
    const float ax = (float) width - 13.0f, ay = (float) height * 0.5f;
    chevron.startNewSubPath (ax - 4.0f, ay - 2.0f);
    chevron.lineTo (ax, ay + 2.0f);
    chevron.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font CatLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return catFont (juce::jmin (14.0f, (float) box.getHeight() * 0.55f));
}

void CatLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font CatLookAndFeel::getPopupMenuFont()   { return catFont (14.0f); }
juce::Font CatLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

void CatLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    // Slider value boxes are created before the editor's look-and-feel is applied,
    // so they would keep the default outline: draw them flat here instead.
    const bool sliderBox = dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr;

    if (! sliderBox)
        g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (! label.isBeingEdited())
    {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const auto font = sliderBox ? catFont (12.0f) : getLabelFont (label);
        g.setColour ((sliderBox ? Colours::text : label.findColour (juce::Label::textColourId)).withMultipliedAlpha (alpha));
        g.setFont (font);
        const auto area = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
        g.drawFittedText (label.getText(), area, label.getJustificationType(),
                          juce::jmax (1, (int) ((float) area.getHeight() / font.getHeight())), label.getMinimumHorizontalScale());
    }

    if (! sliderBox)
    {
        g.setColour (label.findColour (juce::Label::outlineColourId));
        g.drawRect (label.getLocalBounds());
    }
}

int CatLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& b, int depth)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (catFont (13.0f, true), b.getButtonText(), 0.0f, 0.0f);
    return (int) ga.getBoundingBox (0, -1, true).getWidth() + depth + 10;
}

void CatLookAndFeel::drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool front = b.isFrontTab();

    if (front)
    {
        g.setColour (Colours::accent);
        g.fillRect (r.removeFromBottom (2.0f).reduced (10.0f, 0.0f));
    }

    g.setFont (catFont (12.5f, front));
    g.setColour (front ? Colours::text : (over ? Colours::text.withAlpha (0.8f) : Colours::textDim));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void CatLookAndFeel::drawTabAreaBehindFrontButton (juce::TabbedButtonBar& bar, juce::Graphics& g, int w, int h)
{
    juce::ignoreUnused (bar);
    g.setColour (Colours::outline);
    g.fillRect (0, h - 1, w, 1);
}

void CatLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging)
{
    g.setColour (Colours::textDim.withAlpha (over || dragging ? 0.8f : 0.35f));
    for (float i = 0.3f; i < 1.0f; i += 0.3f)
        g.drawLine ((float) w * i, (float) h, (float) w, (float) h * i, 1.0f);
}

void CatLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (Colours::panelLight);
    g.setColour (Colours::outline);
    g.drawRect (0, 0, width, height);
}

void CatLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                                    int thumbStart, int thumbSize, bool over, bool down)
{
    auto thumb = vertical ? juce::Rectangle<int> (x, thumbStart, width, thumbSize)
                          : juce::Rectangle<int> (thumbStart, y, thumbSize, height);
    g.setColour (Colours::raised.brighter (over || down ? 0.25f : 0.0f));
    g.fillRoundedRectangle (thumb.toFloat().reduced (vertical ? 2.5f : 1.0f, vertical ? 1.0f : 2.5f), 2.0f);
    juce::ignoreUnused (height, width);
}

} // namespace cat

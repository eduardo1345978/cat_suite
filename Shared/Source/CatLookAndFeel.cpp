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
    if (theme().uiFont)                    // (Talk-Cat: everything in the terminal font)
        return displayFont (height);
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
    const auto& t = theme();
    if (t.backdrop == Backdrop::Plain)
    {
        // Graphite: flat surface with a hairline edge
        g.setColour (tint);
        g.fillRoundedRectangle (b, corner);
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.drawRoundedRectangle (b.reduced (0.5f), corner, 1.0f);
        return;
    }
    // a little depth: lighter at the top, a light top edge, a dark outline
    const float c = juce::jmax (corner, t.corner * 0.75f);
    g.setGradientFill (juce::ColourGradient (tint.brighter (t.light ? 0.06f : 0.08f), 0.0f, b.getY(), tint.darker (t.light ? 0.04f : 0.12f), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, c);
    g.setColour (t.light ? juce::Colours::black.withAlpha (0.18f) : juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (b.reduced (0.5f), c, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (t.light ? 0.55f : 0.07f));
    g.drawHorizontalLine ((int) b.getY() + 1, b.getX() + c * 0.6f, b.getRight() - c * 0.6f);
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
    const bool themed = theme().fontData != nullptr;
    if (theme().backdrop == Backdrop::ChromeSheen)
    {
        // liquid chrome lettering: "bend·cat"
        const auto word = first.toLowerCase() + juce::String::charToString ((juce::juce_wchar) 0x00b7) + second.trimCharactersAtStart ("-").toLowerCase();
        juce::GlyphArrangement ga;
        ga.addLineOfText (displayFont (b.getHeight() * 0.8f), word, b.getX(), 0.0f);
        juce::Path text;
        ga.createPath (text);
        const auto tb = text.getBounds();
        text.applyTransform (juce::AffineTransform::translation (0.0f, b.getCentreY() - tb.getCentreY()));
        const auto r = text.getBounds();
        juce::ColourGradient chrome (juce::Colours::white, 0.0f, r.getY(), juce::Colour (0xff8c95a6), 0.0f, r.getBottom(), false);
        chrome.addColour (0.38, juce::Colour (0xffb9c2d3));
        chrome.addColour (0.52, juce::Colour (0xff4e5566));
        chrome.addColour (0.70, juce::Colour (0xffdde3ee));
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillPath (text, juce::AffineTransform::translation (0.0f, 2.0f));
        g.setGradientFill (chrome);
        g.fillPath (text);
        return;
    }
    auto f1 = themed ? displayFont (b.getHeight() * 0.62f) : catFont (b.getHeight() * 0.72f, true);
    auto f2 = themed ? displayFont (b.getHeight() * 0.62f) : catFont (b.getHeight() * 0.72f, false);
    juce::GlyphArrangement ga;
    ga.addLineOfText (f1, first, 0.0f, 0.0f);
    const float w1 = ga.getBoundingBox (0, -1, true).getWidth();

    g.setFont (f1);
    g.setColour (Colours::text);
    g.drawText (first, b.withWidth (w1 + 4.0f), juce::Justification::centredLeft);
    g.setFont (f2);
    g.setColour (themed ? Colours::accent : Colours::textDim);
    g.drawText (second, b.withTrimmedLeft (w1 + 2.0f), juce::Justification::centredLeft);
}

void CatLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                       float startAngle, float endAngle, juce::Slider& slider)
{
    const auto& t = theme();
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float lineW = juce::jmax (2.0f, radius * 0.08f);
    const float arcRadius = radius - lineW;
    const float toAngle = startAngle + pos * (endAngle - startAngle);
    const auto dir = juce::Point<float> (std::sin (toAngle), -std::cos (toAngle));
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                                    : startAngle;
    const auto fill = slider.isEnabled() ? slider.findColour (juce::Slider::rotarySliderFillColourId) : Colours::textDim;

    auto arc = [&] (float r, float from, float to, float w, juce::Colour c)
    {
        juce::Path p;
        p.addCentredArc (centre.x, centre.y, r, r, 0.0f, from, to, true);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    auto body = [&] (float r, juce::Colour light, juce::Colour mid, juce::Colour dark)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre.translated (0.0f, r * 0.12f)));
        juce::ColourGradient grad (light, centre.x - r * 0.35f, centre.y - r * 0.45f, dark, centre.x + r * 0.7f, centre.y + r * 0.9f, true);
        grad.addColour (0.45, mid);
        g.setGradientFill (grad);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
    };
    auto ticks = [&] (float r0, float r1, juce::Colour c)
    {
        g.setColour (c);
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
            const auto d = juce::Point<float> (std::sin (a), -std::cos (a));
            g.drawLine (juce::Line<float> (centre + d * r0, centre + d * (i % 5 == 0 ? r1 + 2.0f : r1)), 1.0f);
        }
    };
    auto pointer = [&] (float r0, float r1, float w, juce::Colour c, bool glow)
    {
        const juce::Line<float> l (centre + dir * r0, centre + dir * r1);
        if (glow)
        {
            g.setColour (c.withAlpha (0.25f));
            g.drawLine (l, w * 2.6f);
        }
        g.setColour (c);
        g.drawLine (l, w);
    };

    switch (t.knob)
    {
        case KnobStyle::Arc:
        {
            arc (arcRadius, startAngle, endAngle, lineW, Colours::raised);
            arc (arcRadius, juce::jmin (zeroAngle, toAngle), juce::jmax (zeroAngle, toAngle), lineW, fill);
            g.setColour (Colours::text);
            g.drawLine (juce::Line<float> (centre + dir * (arcRadius * 0.35f), centre + dir * (arcRadius * 0.72f)), juce::jmax (1.5f, lineW * 0.8f));
            break;
        }
        case KnobStyle::Hardware:
        {
            // a black knob in a ring, a bright pointer line; a thin value arc round it
            arc (arcRadius, startAngle, endAngle, lineW * 0.6f, t.light ? juce::Colours::black.withAlpha (0.12f) : juce::Colours::white.withAlpha (0.06f));
            arc (arcRadius, juce::jmin (zeroAngle, toAngle), juce::jmax (zeroAngle, toAngle), lineW * 0.6f, fill.withAlpha (0.8f));
            const float r = arcRadius * 0.78f;
            g.setColour (t.knobRing);
            g.fillEllipse (juce::Rectangle<float> (r * 2.2f, r * 2.2f).withCentre (centre));
            body (r, t.knobBody.brighter (0.55f), t.knobBody.brighter (0.1f), t.knobBody.darker (0.4f));
            pointer (r * 0.3f, r * 0.9f, juce::jmax (2.0f, r * 0.13f), t.knobPointer, t.knobGlow);
            break;
        }
        case KnobStyle::Chrome:
        {
            const float r = arcRadius * 0.9f;
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre.translated (0.0f, r * 0.15f)));
            // brushed metal: a light diagonal sheen over a mid grey, with a dark band
            juce::ColourGradient metal (juce::Colour (0xfff4f6fa), centre.x - r, centre.y - r, juce::Colour (0xff6e7789), centre.x + r, centre.y + r, false);
            metal.addColour (0.35, juce::Colour (0xffb9c2d3));
            metal.addColour (0.5, juce::Colour (0xff545b6a));
            metal.addColour (0.68, juce::Colour (0xffe3e8f0));
            g.setGradientFill (metal);
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre), 1.0f);
            arc (r + lineW * 1.2f, juce::jmin (zeroAngle, toAngle), juce::jmax (zeroAngle, toAngle), lineW * 0.7f, fill);
            g.setColour (t.knobPointer);
            g.fillEllipse (juce::Rectangle<float> (r * 0.24f, r * 0.24f).withCentre (centre + dir * (r * 0.72f)));
            break;
        }
        case KnobStyle::Cream:
        case KnobStyle::Brass:
        {
            const bool brass = t.knob == KnobStyle::Brass;
            const float r = arcRadius * 0.74f;
            ticks (r * 1.18f, r * 1.32f, (t.light ? Colours::text : Colours::textDim).withAlpha (0.7f));
            arc (r * 1.08f, juce::jmin (zeroAngle, toAngle), juce::jmax (zeroAngle, toAngle), lineW * 0.55f, fill);
            if (brass) body (r, juce::Colour (0xfff3d99b), juce::Colour (0xffc79a4c), juce::Colour (0xff7a5a26));
            else       body (r, juce::Colour (0xfffffdf6), juce::Colour (0xffe9e1ce), juce::Colour (0xffa99f88));
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre), 1.0f);
            pointer (r * 0.15f, r * 0.92f, juce::jmax (2.2f, r * 0.15f), t.knobPointer, false);
            break;
        }
        case KnobStyle::Glow:
        {
            arc (arcRadius, startAngle, endAngle, lineW, juce::Colours::white.withAlpha (0.1f));
            arc (arcRadius, juce::jmin (zeroAngle, toAngle), juce::jmax (zeroAngle, toAngle), lineW * 2.6f, fill.withAlpha (0.22f));
            arc (arcRadius, juce::jmin (zeroAngle, toAngle), juce::jmax (zeroAngle, toAngle), lineW, fill);
            const float r = arcRadius * 0.62f;
            g.setColour (t.knobBody);
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre), 1.0f);
            g.setColour (Colours::text);
            g.fillEllipse (juce::Rectangle<float> (r * 0.36f, r * 0.36f).withCentre (centre + dir * (r * 0.62f)));
            break;
        }
        case KnobStyle::Gauge:
        {
            // a round dial with ticks and an amber needle
            const float r = arcRadius;
            g.setColour (t.knobBody);
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
            g.setColour (Colours::outline);
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre), 1.5f);
            ticks (r * 0.72f, r * 0.86f, Colours::accent.withAlpha (0.6f));
            arc (r * 0.92f, juce::jmin (zeroAngle, toAngle), juce::jmax (zeroAngle, toAngle), lineW * 0.5f, fill.withAlpha (0.7f));
            pointer (0.0f, r * 0.8f, juce::jmax (1.8f, r * 0.07f), t.knobPointer, true);
            g.setColour (Colours::text);
            g.fillEllipse (juce::Rectangle<float> (r * 0.22f, r * 0.22f).withCentre (centre));
            break;
        }
    }
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

void CatLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& juceBg,
                                           bool over, bool down)
{
    const auto& t = theme();
    // JUCE hands a toggled-on button its solid "on" colour; every style draws "on" itself, from the off colour
    const auto bg = b.getToggleState() ? b.findColour (juce::TextButton::buttonColourId) : juceBg;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto onColour = b.findColour (juce::TextButton::buttonOnColourId);
    const float c = juce::jmin (t.corner, r.getHeight() * 0.5f);

    switch (t.button)
    {
        case ButtonStyle::Flat:
        {
            // Graphite: off = quiet surface, on = soft tint of the button's "on" colour. No borders.
            auto base = on ? onColour.withAlpha (0.2f) : bg;
            if (down)      base = base.brighter (0.06f);
            else if (over) base = base.brighter (0.1f);
            g.setColour (base);
            g.fillRoundedRectangle (r, 4.0f);
            break;
        }
        case ButtonStyle::Hardware:
        {
            // a raised key with a shadow under it; pressed / on = pushed in
            const bool in = down || on;
            auto key = r.withTrimmedBottom (in ? 1.0f : 3.0f).translated (0.0f, in ? 2.0f : 0.0f);
            g.setColour (juce::Colours::black.withAlpha (t.light ? 0.3f : 0.6f));
            g.fillRoundedRectangle (r.withTrimmedTop (3.0f), c);
            auto top = bg.brighter (t.light ? 0.25f : 0.12f);
            if (on) top = top.interpolatedWith (onColour, t.light ? 0.45f : 0.35f);
            if (over) top = top.brighter (0.06f);
            g.setGradientFill (juce::ColourGradient (top.brighter (0.08f), 0.0f, key.getY(), top.darker (0.1f), 0.0f, key.getBottom(), false));
            g.fillRoundedRectangle (key, c);
            g.setColour (juce::Colours::white.withAlpha (t.light ? 0.6f : 0.08f));
            g.drawHorizontalLine ((int) key.getY() + 1, key.getX() + c, key.getRight() - c);
            break;
        }
        case ButtonStyle::Glow:
        {
            auto base = bg.brighter (over ? 0.08f : 0.0f);
            g.setColour (base);
            g.fillRoundedRectangle (r, c);
            g.setColour (on ? onColour.withAlpha (0.9f) : juce::Colours::white.withAlpha (t.light ? 0.0f : 0.07f));
            g.drawRoundedRectangle (r.reduced (0.5f), c, on ? 1.4f : 1.0f);
            if (on)
            {
                g.setColour (onColour.withAlpha (0.18f));
                g.fillRoundedRectangle (r, c);
                g.setColour (onColour.withAlpha (0.12f));
                g.drawRoundedRectangle (r.expanded (1.5f), c + 1.5f, 3.0f);
            }
            if (down) { g.setColour (juce::Colours::black.withAlpha (0.15f)); g.fillRoundedRectangle (r, c); }
            break;
        }
        case ButtonStyle::Metal:
        {
            if (on)
            {
                juce::ColourGradient metal (juce::Colour (0xffffffff), 0.0f, r.getY(), juce::Colour (0xffd6dce7), 0.0f, r.getBottom(), false);
                metal.addColour (0.5, juce::Colour (0xffaeb7c8));
                metal.addColour (0.52, juce::Colour (0xff6e7789));
                g.setGradientFill (metal);
            }
            else g.setColour (bg.brighter (over ? 0.1f : 0.03f));
            g.fillRoundedRectangle (r, c);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.0f : 0.14f));
            g.drawRoundedRectangle (r.reduced (0.5f), c, 1.0f);
            if (down) { g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillRoundedRectangle (r, c); }
            break;
        }
        case ButtonStyle::Terminal:
        {
            g.setColour (on ? Colours::accent : bg.brighter (over ? 0.15f : 0.0f));
            g.fillRect (r);
            g.setColour (Colours::accent.withAlpha (on ? 1.0f : 0.55f));
            g.drawRect (r, 1.0f);
            break;
        }
    }
}

void CatLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    const auto& t = theme();
    auto onText = b.findColour (juce::TextButton::buttonOnColourId).brighter (0.35f);
    if (t.button == ButtonStyle::Metal || t.button == ButtonStyle::Terminal) onText = juce::Colour (0xff0b0c0f);
    else if (t.light) onText = Colours::text;
    const auto colour = b.getToggleState() ? onText : b.findColour (juce::TextButton::textColourOffId);
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
    const auto& t = theme();
    const float c = juce::jmin (t.corner, r.getHeight() * 0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, t.backdrop == Backdrop::Plain ? 4.0f : c);
    if (t.backdrop != Backdrop::Plain)
    {
        // a recessed field
        g.setColour (juce::Colours::black.withAlpha (t.light ? 0.16f : 0.4f));
        g.drawRoundedRectangle (r.reduced (0.5f), c, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (t.light ? 0.06f : 0.2f));
        g.fillRoundedRectangle (r.withHeight (3.0f), c);
    }

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

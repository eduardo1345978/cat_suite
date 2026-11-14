#include "EnvelopeEditorUI.h"
#include "PluginProcessor.h"
#include "CatLookAndFeel.h"

using namespace bendcat;
namespace C = cat::Colours;

namespace
{
    const juce::Colour curveColour { 0xff8fa3f0 };

    juce::Path envelopePath (const BendEnvelope& env, juce::Rectangle<float> area, int cycles, int resolution = 96)
    {
        juce::Path p;
        const int n = resolution * cycles;
        for (int i = 0; i <= n; ++i)
        {
            const float x = (float) i / (float) n;
            float local = x * cycles;
            local = i == n ? 1.0f : local - std::floor (local);
            const float v = env.evaluate (local);
            const juce::Point<float> pt (area.getX() + x * area.getWidth(), area.getCentreY() - v * area.getHeight() * 0.5f);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        return p;
    }
}

//==============================================================================
ShapePalette::ShapePalette (BendCatProcessor& p) : processor (p)
{
    for (int i = 0; i < getShapeNames().size(); ++i)
        shapes.push_back (makeShape (i));
}

juce::Rectangle<float> ShapePalette::cell (int index) const
{
    auto r = getLocalBounds().toFloat().reduced (6.0f);
    const float w = r.getWidth() / 3.0f, h = r.getHeight() / 9.0f;
    return { r.getX() + (index % 3) * w, r.getY() + (index / 3) * h, w, h };
}

void ShapePalette::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff1b1e25));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);

    for (int i = 0; i < (int) shapes.size(); ++i)
    {
        auto c = cell (i);
        if (i == hover)
        {
            g.setColour (C::raised);
            g.fillRoundedRectangle (c.reduced (1.0f), 3.0f);
        }
        auto area = c.reduced (c.getWidth() * 0.22f, c.getHeight() * 0.28f);
        g.setColour (i == selected ? curveColour : C::textDim);
        g.strokePath (envelopePath (shapes[(size_t) i], area, 1, 24), juce::PathStrokeType (i == selected ? 1.6f : 1.1f));
    }
}

void ShapePalette::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < (int) shapes.size(); ++i)
        if (cell (i).contains (e.position))
        {
            selected = i;
            processor.setEnvelope (shapes[(size_t) i]);
            repaint();
            return;
        }
}

void ShapePalette::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < (int) shapes.size(); ++i)
        if (cell (i).contains (e.position)) h = i;
    if (h != hover)
    {
        hover = h;
        setTooltip (h >= 0 ? getShapeNames()[h] : juce::String());
        repaint();
    }
}

void ShapePalette::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

//==============================================================================
EnvelopeEditor::EnvelopeEditor (BendCatProcessor& p) : processor (p)
{
    refresh();
    startTimerHz (30);
}

void EnvelopeEditor::refresh()
{
    env = processor.getEnvelope();
    shownVersion = processor.dataVersion.load();
    repaint();
}

int EnvelopeEditor::cycles() const
{
    return juce::jlimit (1, 32, juce::roundToInt (processor.apvts.getRawParameterValue ("envCycles")->load()));
}

void EnvelopeEditor::timerCallback()
{
    if (processor.dataVersion.load() != shownVersion && dragNode < 0 && dragSegment < 0)
        refresh();

    const float ph = processor.uiEnvPhase.load();
    static int lastCycles = -1;
    if (std::abs (ph - playhead) > 0.001f || cycles() != lastCycles)
    {
        playhead = ph;
        lastCycles = cycles();
        repaint();
    }
}

juce::Rectangle<float> EnvelopeEditor::plotArea() const
{
    return getLocalBounds().toFloat().reduced (28.0f, 26.0f);
}

juce::Point<float> EnvelopeEditor::toScreen (float t, float v, int cycle) const
{
    const auto a = plotArea();
    const float cw = a.getWidth() / (float) cycles();
    return { a.getX() + (cycle + t) * cw, a.getCentreY() - v * a.getHeight() * 0.5f };
}

std::pair<float, float> EnvelopeEditor::fromScreen (juce::Point<float> p) const
{
    const auto a = plotArea();
    const float cw = a.getWidth() / (float) cycles();
    const float x = juce::jlimit (0.0f, a.getWidth() - 0.001f, p.x - a.getX());
    const float local = (x - std::floor (x / cw) * cw) / cw;
    const float v = juce::jlimit (-1.0f, 1.0f, (a.getCentreY() - p.y) / (a.getHeight() * 0.5f));
    return { local, v };
}

int EnvelopeEditor::hitNode (juce::Point<float> p) const
{
    for (int c = 0; c < cycles(); ++c)
        for (size_t i = 0; i < env.points.size(); ++i)
            if (toScreen (env.points[i].first, env.points[i].second, c).getDistanceFrom (p) < 8.0f)
                return (int) i;
    return -1;
}

int EnvelopeEditor::segmentAt (float t) const
{
    for (size_t i = 0; i + 1 < env.points.size(); ++i)
        if (t >= env.points[i].first && t <= env.points[i + 1].first)
            return (int) i;
    return -1;
}

void EnvelopeEditor::paint (juce::Graphics& g)
{
    // Liquid chrome: a glass panel, semitone grid, a thick chrome curve with a soft blue glow, chrome handles
    auto frame = getLocalBounds().toFloat().reduced (0.5f);
    const float radius = juce::jmin (22.0f, frame.getHeight() * 0.12f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), 0.0f, frame.getY(),
                                             juce::Colours::white.withAlpha (0.01f), 0.0f, frame.getBottom(), false));
    g.fillRoundedRectangle (frame, radius);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (frame, radius);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (frame, radius, 1.0f);

    const auto a = plotArea();
    const int n = cycles();
    const float amount = processor.apvts.getRawParameterValue ("bendAmount")->load();

    // grid: +amount ... -amount in semitones
    g.setFont (cat::catFont (11.0f));
    for (int i = 0; i <= 4; ++i)
    {
        const float y = a.getY() + a.getHeight() * (float) i / 4.0f;
        g.setColour (juce::Colours::white.withAlpha (i == 2 ? 0.12f : 0.06f));
        g.drawHorizontalLine (juce::roundToInt (y), frame.getX() + 2.0f, frame.getRight() - 2.0f);
        const float st = amount * (1.0f - (float) i * 0.5f);
        g.setColour (juce::Colour (0xff8890a0));
        g.drawText ((st > 0.0f ? "+" : "") + juce::String (st, std::abs (st - std::round (st)) < 0.01f ? 0 : 1) + " st",
                    juce::Rectangle<float> (frame.getX() + 12.0f, y - 15.0f, 60.0f, 13.0f), juce::Justification::centredLeft);
    }
    for (int c = 1; c < n; ++c)
    {
        const float x = a.getX() + a.getWidth() * (float) c / (float) n;
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
    }

    const auto path = envelopePath (env, a, n, juce::jmax (48, 192 / n));
    const float thick = juce::jlimit (6.0f, 14.0f, a.getHeight() * 0.045f);
    // glow
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (juce::Colour (0xff9db4ff).withAlpha (0.06f));
        g.strokePath (path, juce::PathStrokeType (thick + (float) i * 7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // chrome body
    {
        juce::ColourGradient chrome (juce::Colours::white, 0.0f, a.getY(), juce::Colour (0xff7e879a), 0.0f, a.getBottom(), false);
        chrome.addColour (0.35, juce::Colour (0xffc3cbda));
        chrome.addColour (0.5, juce::Colour (0xff4a5060));
        chrome.addColour (0.65, juce::Colour (0xffe6ebf3));
        g.setColour (juce::Colour (0xff0b0c0f));
        g.strokePath (path, juce::PathStrokeType (thick + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (chrome);
        g.strokePath (path, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (0.0f, -thick * 0.28f));
    }

    for (int c = 0; c < n; ++c)
        for (size_t i = 0; i < env.points.size(); ++i)
        {
            const auto p = toScreen (env.points[i].first, env.points[i].second, c);
            const float d = (int) i == dragNode ? 26.0f : 20.0f;
            auto dot = juce::Rectangle<float> (d, d).withCentre (p);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillEllipse (dot.translated (0.0f, 3.0f));
            juce::ColourGradient knob (juce::Colours::white, dot.getX(), dot.getY(), juce::Colour (0xff6e7789), dot.getRight(), dot.getBottom(), false);
            knob.addColour (0.45, juce::Colour (0xffd6dce7));
            knob.addColour (0.55, juce::Colour (0xff8d96a8));
            g.setGradientFill (knob);
            g.fillEllipse (dot);
            g.setColour (juce::Colour (0xff0b0c0f));
            g.drawEllipse (dot, 2.0f);
        }

    if (playhead >= 0.0f)
    {
        const float x = a.getX() + juce::jlimit (0.0f, 1.0f, playhead) * a.getWidth();
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawVerticalLine ((int) x, frame.getY() + 4.0f, frame.getBottom() - 4.0f);
    }
}

void EnvelopeEditor::mouseDown (const juce::MouseEvent& e)
{
    dragNode = hitNode (e.position);
    dragSegment = -1;

    if (e.mods.isPopupMenu())
    {
        if (dragNode > 0 && dragNode < (int) env.points.size() - 1)
        {
            env.points.erase (env.points.begin() + dragNode);
            env.curves.erase (env.curves.begin() + juce::jmin (dragNode, (int) env.curves.size() - 1));
            commit();
        }
        dragNode = -1;
        return;
    }

    if (dragNode < 0)
    {
        const auto [t, v] = fromScreen (e.position);
        juce::ignoreUnused (v);
        dragSegment = segmentAt (t);
        if (dragSegment >= 0)
        {
            dragStartCurve = env.curves[(size_t) dragSegment];
            dragStartY = e.position.y;
        }
    }
    repaint();
}

void EnvelopeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode >= 0)
    {
        auto [t, v] = fromScreen (e.position);
        auto& pt = env.points[(size_t) dragNode];
        const bool isEnd = dragNode == 0 || dragNode == (int) env.points.size() - 1;
        if (! isEnd)
        {
            const float lo = env.points[(size_t) dragNode - 1].first;
            const float hi = env.points[(size_t) dragNode + 1].first;
            pt.first = juce::jlimit (lo, hi, t);
        }
        pt.second = std::abs (v) < 0.04f ? 0.0f : v; // snap to centre line
        repaint();
        return;
    }

    if (dragSegment >= 0)
    {
        const float delta = (e.position.y - dragStartY) / 120.0f;
        const auto& a = env.points[(size_t) dragSegment];
        const auto& b = env.points[(size_t) dragSegment + 1];
        const float dir = b.second >= a.second ? 1.0f : -1.0f;
        env.curves[(size_t) dragSegment] = juce::jlimit (-1.0f, 1.0f, dragStartCurve + delta * dir);
        repaint();
    }
}

void EnvelopeEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragNode >= 0 || dragSegment >= 0)
        commit();
    dragNode = dragSegment = -1;
    repaint();
}

void EnvelopeEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = hitNode (e.position);
    if (hit > 0 && hit < (int) env.points.size() - 1)
    {
        env.points.erase (env.points.begin() + hit);
        env.curves.erase (env.curves.begin() + juce::jmin (hit, (int) env.curves.size() - 1));
    }
    else if (hit < 0 && env.points.size() < 64)
    {
        const auto [t, v] = fromScreen (e.position);
        const int seg = segmentAt (t);
        if (seg < 0) return;
        env.points.insert (env.points.begin() + seg + 1, { t, v });
        env.curves.insert (env.curves.begin() + seg + 1, 0.0f);
    }
    commit();
}

void EnvelopeEditor::commit()
{
    processor.setEnvelope (env);
    shownVersion = processor.dataVersion.load();
}

//==============================================================================
BendPatternEditor::BendPatternEditor (BendCatProcessor& p) : processor (p)
{
    cents = processor.getPatternCents();
    startTimerHz (10);
}

void BendPatternEditor::timerCallback()
{
    if (processor.dataVersion.load() != shownVersion && ! isMouseButtonDown())
    {
        cents = processor.getPatternCents();
        shownVersion = processor.dataVersion.load();
        repaint();
    }
}

void BendPatternEditor::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff1b1e25));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
    auto a = getLocalBounds().toFloat().reduced (10.0f, 14.0f);
    const float w = a.getWidth() / 16.0f;

    g.setColour (C::outline);
    g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());

    for (int i = 0; i < 16; ++i)
    {
        const float v = cents[(size_t) i] / 50.0f;
        const float x = a.getX() + i * w + 2.0f;
        const float y0 = a.getCentreY(), y1 = a.getCentreY() - v * a.getHeight() * 0.5f;
        g.setColour (juce::Colour (i % 4 == 0 ? 0xff22262f : 0xff1f222a));
        g.fillRect (x, a.getY(), w - 4.0f, a.getHeight());
        g.setColour (curveColour);
        g.fillRect (x, std::min (y0, y1), w - 4.0f, std::max (2.0f, std::abs (y1 - y0)));
        g.setColour (C::textDim);
        g.setFont (cat::catFont (9.5f));
        g.drawText (juce::String (juce::roundToInt (cents[(size_t) i])), juce::Rectangle<float> (x, a.getBottom() + 1.0f, w - 4.0f, 12.0f),
                    juce::Justification::centred);
    }
    g.setColour (C::textDim);
    g.setFont (cat::catFont (10.5f));
    g.drawText ("+50 ct", a.translated (0, -13.0f).withHeight (12.0f), juce::Justification::centredLeft);
}

void BendPatternEditor::setFromMouse (juce::Point<float> p)
{
    auto a = getLocalBounds().toFloat().reduced (10.0f, 14.0f);
    const int i = juce::jlimit (0, 15, (int) ((p.x - a.getX()) / (a.getWidth() / 16.0f)));
    const float v = juce::jlimit (-1.0f, 1.0f, (a.getCentreY() - p.y) / (a.getHeight() * 0.5f));
    cents[(size_t) i] = std::round (v * 50.0f);
    processor.setPatternCents (cents);
    shownVersion = processor.dataVersion.load();
    repaint();
}

void BendPatternEditor::mouseDown (const juce::MouseEvent& e) { setFromMouse (e.position); }
void BendPatternEditor::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }

void BendPatternEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto a = getLocalBounds().toFloat().reduced (10.0f, 14.0f);
    const int i = juce::jlimit (0, 15, (int) ((e.position.x - a.getX()) / (a.getWidth() / 16.0f)));
    cents[(size_t) i] = 0.0f;
    processor.setPatternCents (cents);
    repaint();
}

//==============================================================================
CurvePreview::CurvePreview (BendCatProcessor& p, Kind k) : processor (p), kind (k)
{
    startTimerHz (10);
}

void CurvePreview::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff1b1e25));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
    auto a = getLocalBounds().toFloat().reduced (12.0f, 14.0f);

    g.setColour (C::outline);
    g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());

    juce::Path p;
    const int n = 200;
    if (kind == Kind::Glide)
    {
        const int curve = juce::roundToInt (processor.apvts.getRawParameterValue ("glideCurve")->load());
        for (int i = 0; i <= n; ++i)
        {
            const float x = (float) i / n;
            const float v = 1.0f - BendCatEngine::shapeGlide (x, curve); // from previous pitch (top) to target (centre)
            const juce::Point<float> pt (a.getX() + x * a.getWidth(), a.getCentreY() - v * a.getHeight() * 0.5f);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour (C::textDim);
        g.setFont (cat::catFont (11.0f));
        g.drawText ("previous chord tone", a.withHeight (14.0f), juce::Justification::topLeft);
        g.drawText ("target pitch", a.withTop (a.getCentreY() + 2.0f).withHeight (14.0f), juce::Justification::topRight);
    }
    else
    {
        const int wave = juce::roundToInt (processor.apvts.getRawParameterValue ("lfoWave")->load());
        const float depth = processor.apvts.getRawParameterValue ("lfoDepth")->load();
        for (int i = 0; i <= n; ++i)
        {
            const double phase = 2.0 * i / n;
            const float v = BendCatEngine::lfoValue (wave, phase, 12345u) * depth;
            const juce::Point<float> pt (a.getX() + (float) i / n * a.getWidth(), a.getCentreY() - v * a.getHeight() * 0.5f);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
    }
    g.setColour (curveColour);
    g.strokePath (p, juce::PathStrokeType (1.8f));
}

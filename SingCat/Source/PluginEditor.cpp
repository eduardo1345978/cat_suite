#include "PluginEditor.h"

namespace C = cat::Colours;
using namespace singcat;

namespace
{
    juce::RangedAudioParameter* param (SingCatProcessor& p, const char* id) { return p.apvts.getParameter (id); }
    float value (SingCatProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

    juce::Colour faceColour (SingerType t)
    {
        switch (t)
        {
            case SingerType::Bass:
            case SingerType::Tenor:      return juce::Colour (0xffb9bfcf);
            case SingerType::Alto:
            case SingerType::Soprano:    return juce::Colour (0xffd6c6d3);
            case SingerType::Child:      return juce::Colour (0xffdcd3bb);
            case SingerType::ElderMan:
            case SingerType::ElderWoman: return juce::Colour (0xffc4c2bd);
            case SingerType::Kitten:     return juce::Colour (0xffe2bd94);
            case SingerType::Cat:        return juce::Colour (0xffbcb3a3);
            case SingerType::Tomcat:     return juce::Colour (0xff9497a4);
        }
        return C::text;
    }
}

//==============================================================================
VowelPad::VowelPad (SingCatProcessor& p) : processor (p)
{
    setTooltip ("Vowel chart: drag the dot to choose the vowel. The ring is the target for Wave / Envelope and for MPE slide (CC74). "
                "In Path mode, draw a line and the choir sings along it.");
    startTimerHz (30);
}

void VowelPad::timerCallback()
{
    pulse += 0.12f;
    repaint();
}

bool VowelPad::pathMode() const
{
    return juce::roundToInt (processor.apvts.getRawParameterValue ("move")->load()) == (int) Move::Path;
}

juce::Rectangle<float> VowelPad::chartArea() const
{
    return getLocalBounds().toFloat().reduced (30.0f, 26.0f).withTrimmedBottom (4.0f);
}

juce::Point<float> VowelPad::toScreen (juce::Point<float> c) const
{
    // IPA-style trapezoid: the front edge slants as the jaw opens
    const auto r = chartArea();
    const float left = r.getX() + 0.24f * r.getWidth() * c.y;
    return { left + c.x * (r.getRight() - left), r.getY() + c.y * r.getHeight() };
}

juce::Point<float> VowelPad::toChart (juce::Point<float> s) const
{
    const auto r = chartArea();
    const float y = juce::jlimit (0.0f, 1.0f, (s.y - r.getY()) / r.getHeight());
    const float left = r.getX() + 0.24f * r.getWidth() * y;
    return { juce::jlimit (0.0f, 1.0f, (s.x - left) / (r.getRight() - left)), y };
}

void VowelPad::setPoint (int which, juce::Point<float> c)
{
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = param (processor, id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set (which == 0 ? "vowelX" : "vowelX2", c.x);
    set (which == 0 ? "vowelY" : "vowelY2", c.y);
}

void VowelPad::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff101116));
    g.fillRoundedRectangle (bounds, 8.0f);

    // Chart outline and guide lines
    juce::Path trapezoid;
    trapezoid.startNewSubPath (toScreen ({ 0, 0 }));
    trapezoid.lineTo (toScreen ({ 1, 0 }));
    trapezoid.lineTo (toScreen ({ 1, 1 }));
    trapezoid.lineTo (toScreen ({ 0, 1 }));
    trapezoid.closeSubPath();
    g.setColour (C::panelLight);
    g.fillPath (trapezoid);
    g.setColour (C::outline);
    g.strokePath (trapezoid, juce::PathStrokeType (1.2f));
    for (float y : { 1.0f / 3.0f, 2.0f / 3.0f })
        g.drawLine (juce::Line<float> (toScreen ({ 0, y }), toScreen ({ 1, y })), 1.0f);
    g.drawLine (juce::Line<float> (toScreen ({ 0.5f, 0 }), toScreen ({ 0.5f, 1 })), 1.0f);

    g.setColour (C::textDim);
    g.setFont (cat::catFont (10.5f));
    const auto r = chartArea();
    g.drawText ("FRONT", juce::Rectangle<float> (r.getX(), 4.0f, 80.0f, 16.0f), juce::Justification::centredLeft);
    g.drawText ("BACK", juce::Rectangle<float> (r.getRight() - 80.0f, 4.0f, 80.0f, 16.0f), juce::Justification::centredRight);
    g.drawText ("CLOSE", juce::Rectangle<float> (2.0f, r.getY() - 6.0f, 40.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText ("OPEN", juce::Rectangle<float> (2.0f, r.getBottom() - 6.0f, 40.0f, 12.0f), juce::Justification::centredLeft);

    const auto move = (Move) juce::roundToInt (value (processor, "move"));
    const juce::Point<float> a (value (processor, "vowelX"), value (processor, "vowelY"));
    const juce::Point<float> b (value (processor, "vowelX2"), value (processor, "vowelY2"));

    // Vowel labels
    for (const auto& an : vowelAnchors())
    {
        const auto s = toScreen ({ an.x, an.y });
        g.setColour (move == Move::Steps ? C::text.withAlpha (0.8f) : C::textDim.withAlpha (0.9f));
        g.setFont (cat::catFont (14.0f, true));
        g.drawText (an.label, juce::Rectangle<float> (40.0f, 18.0f).withCentre (s), juce::Justification::centred);
    }

    // Movement guide
    if (move == Move::Wave || move == Move::Envelope)
    {
        juce::Path line;
        line.startNewSubPath (toScreen (a));
        line.lineTo (toScreen (b));
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, line, dashes, 2);
        g.setColour (C::accent.withAlpha (0.5f));
        g.fillPath (dashed);
    }

    const auto& pathPts = dragging == 2 ? drawing : processor.engine.getPath();
    if (move == Move::Path || dragging == 2)
    {
        juce::Path pp;
        for (size_t i = 0; i < pathPts.size(); ++i)
        {
            const auto s = toScreen (pathPts[i]);
            if (i == 0) pp.startNewSubPath (s); else pp.lineTo (s);
        }
        g.setColour (C::accent.withAlpha (0.75f));
        g.strokePath (pp, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (pathPts.empty())
        {
            g.setColour (C::textDim);
            g.setFont (cat::catFont (12.0f));
            g.drawText ("Draw a path with the mouse", bounds.removeFromBottom (24.0f), juce::Justification::centred);
        }
    }

    // B (target) ring and A (main) dot
    const auto sb = toScreen (b);
    g.setColour (C::accent.withAlpha (move == Move::Wave || move == Move::Envelope ? 0.9f : 0.4f));
    g.drawEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (sb), 2.0f);
    g.setFont (cat::catFont (10.0f, true));
    g.drawText ("B", juce::Rectangle<float> (16.0f, 12.0f).withCentre (sb.translated (13.0f, -11.0f)), juce::Justification::centred);

    const auto sa = toScreen (a);
    if (move != Move::Path && move != Move::Steps && move != Move::Random)
    {
        g.setColour (C::accent);
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (sa));
    }

    // Where the choir is singing now
    const auto now = toScreen ({ processor.engine.uiVowelX.load(), processor.engine.uiVowelY.load() });
    const float level = juce::jlimit (0.0f, 1.0f, processor.engine.uiLevel.load() * 2.5f);
    const float glow = 10.0f + 18.0f * level + 2.0f * std::sin (pulse);
    g.setColour (C::text.withAlpha (0.10f + 0.25f * level));
    g.fillEllipse (juce::Rectangle<float> (glow * 2.0f, glow * 2.0f).withCentre (now));
    g.setColour (C::text.withAlpha (0.85f));
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (now));
}

void VowelPad::mouseDown (const juce::MouseEvent& e)
{
    const juce::Point<float> a (value (processor, "vowelX"), value (processor, "vowelY"));
    const juce::Point<float> b (value (processor, "vowelX2"), value (processor, "vowelY2"));
    const auto pos = e.position;

    const float da = pos.getDistanceFrom (toScreen (a)), db = pos.getDistanceFrom (toScreen (b));
    if (db < 14.0f && db < da)       dragging = 1;
    else if (pathMode() && da > 14.0f)
    {
        dragging = 2;
        drawing.clear();
        drawing.push_back (toChart (pos));
        return;
    }
    else                             dragging = 0;

    for (auto* id : { dragging == 0 ? "vowelX" : "vowelX2", dragging == 0 ? "vowelY" : "vowelY2" })
        if (auto* prm = param (processor, id)) prm->beginChangeGesture();
    setPoint (dragging, toChart (pos));
}

void VowelPad::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == 2)
    {
        const auto c = toChart (e.position);
        if (drawing.empty() || drawing.back().getDistanceFrom (c) > 0.02f)
        {
            if ((int) drawing.size() >= maxPathPoints)
            {
                // Keep the whole stroke: thin it out instead of stopping
                std::vector<juce::Point<float>> thin;
                for (size_t i = 0; i < drawing.size(); i += 2) thin.push_back (drawing[i]);
                drawing = thin;
            }
            drawing.push_back (c);
        }
        repaint();
        return;
    }
    if (dragging >= 0)
        setPoint (dragging, toChart (e.position));
}

void VowelPad::mouseUp (const juce::MouseEvent&)
{
    if (dragging == 2)
    {
        if (drawing.size() >= 2)
            processor.engine.setPath (drawing);
    }
    else if (dragging >= 0)
    {
        for (auto* id : { dragging == 0 ? "vowelX" : "vowelX2", dragging == 0 ? "vowelY" : "vowelY2" })
            if (auto* prm = param (processor, id)) prm->endChangeGesture();
    }
    dragging = -1;
}

//==============================================================================
StageView::StageView (SingCatProcessor& p) : processor (p)
{
    setTooltip ("The stage: drag singers to place them, drag the microphone pair to move where you listen from. "
                "Closer to the mics = louder and brighter; the side a singer stands on is where you hear them.");
    const char* tips[] = { "Whole stage with faces", "Close-up of every singer's lips", "Shaded 3D faces that turn to the microphones" };
    for (int v = 0; v < 3; ++v)
    {
        viewButtons[v].setClickingTogglesState (false);
        viewButtons[v].setTooltip (tips[v]);
        viewButtons[v].onClick = [this, v] { processor.faceView = v; repaint(); };
        addAndMakeVisible (viewButtons[v]);
    }
    startTimerHz (30);
}

void StageView::resized()
{
    auto r = getLocalBounds().reduced (8, 5).removeFromTop (22);
    for (int v = 2; v >= 0; --v)
        viewButtons[v].setBounds (r.removeFromRight (v == 2 ? 40 : 56).reduced (2, 0));
}

juce::Point<float> StageView::stageToScreen (float x, float y, float& scale) const
{
    const auto r = floorArea();
    const float width = 1.0f - 0.32f * juce::jmax (0.0f, y);   // perspective: the back row is narrower
    scale = 1.0f - 0.38f * juce::jmax (0.0f, y);
    return { r.getCentreX() + x * r.getWidth() * 0.44f * width, r.getBottom() - r.getHeight() * (0.16f + 0.7f * y) };
}

juce::Point<float> StageView::screenToStage (juce::Point<float> p) const
{
    const auto r = floorArea();
    const float y = ((r.getBottom() - p.y) / r.getHeight() - 0.16f) / 0.7f;
    const float width = 1.0f - 0.32f * juce::jmax (0.0f, y);
    return { (p.x - r.getCentreX()) / (r.getWidth() * 0.44f * width), y };
}

juce::Point<float> StageView::micPosition() const
{
    float scale = 1.0f;
    return stageToScreen (processor.engine.micX.load(), processor.engine.micY.load(), scale);
}

void StageView::drawMics (juce::Graphics& g) const
{
    // Stand + bar + two capsules (the spacing follows Mic Width)
    float scale = 1.0f;
    const auto c = stageToScreen (processor.engine.micX.load(), processor.engine.micY.load(), scale);
    const float spacingM = 0.2f + 1.8f * value (processor, "micWidth") / 100.0f;
    float s2 = 1.0f;
    const auto right = stageToScreen (processor.engine.micX.load() + spacingM / 5.0f * 0.5f, processor.engine.micY.load(), s2);
    const float half = juce::jmax (7.0f, right.x - c.x);
    g.setColour (C::textDim.withAlpha (0.7f));
    g.drawLine (c.x, c.y, c.x, c.y + 18.0f, 2.0f);
    g.drawLine (c.x - 9.0f, c.y + 18.0f, c.x + 9.0f, c.y + 18.0f, 2.0f);
    g.drawLine (c.x - half, c.y, c.x + half, c.y, 2.0f);
    for (float side : { -1.0f, 1.0f })
    {
        const auto cap = juce::Rectangle<float> (7.0f, 13.0f).withCentre ({ c.x + side * half, c.y - 6.0f });
        g.setColour (C::accent);
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (C::background.withAlpha (0.6f));
        for (int k = 1; k < 4; ++k)
            g.drawHorizontalLine (juce::roundToInt (cap.getY() + (float) k * 3.0f), cap.getX() + 1.0f, cap.getRight() - 1.0f);
    }
    g.setColour (C::textDim);
    g.setFont (cat::catFont (9.5f, true));
    g.drawText ("MICS", juce::Rectangle<float> (40.0f, 12.0f).withCentre ({ c.x, c.y + 27.0f }), juce::Justification::centred);
}

int StageView::numSingers() const
{
    return juce::jlimit (1, maxSingers, juce::roundToInt (value (processor, "singers")));
}

void StageView::timerCallback()
{
    time += 1.0f / 30.0f;
    auto& rnd = juce::Random::getSystemRandom();
    for (auto& b : blink)
    {
        b = juce::jmax (0.0f, b - 0.25f);
        if (rnd.nextFloat() < 0.004f) b = 1.0f;
    }
    repaint();
}

juce::Rectangle<float> StageView::floorArea() const
{
    return getLocalBounds().toFloat().reduced (14.0f).withTrimmedTop (26.0f);
}

juce::Point<float> StageView::facePosition (int i, float& scale) const
{
    return stageToScreen (processor.engine.stageX[(size_t) i].load(), processor.engine.stageY[(size_t) i].load(), scale);
}

void StageView::drawRoom (juce::Graphics& g, juce::Rectangle<float> r) const
{
    const int room = juce::roundToInt (value (processor, "room"));
    g.setColour (C::outline.withAlpha (0.7f));
    const float wallBottom = r.getBottom() - r.getHeight() * 0.86f + r.getHeight() * 0.06f;
    const float backL = r.getX() + r.getWidth() * 0.16f, backR = r.getRight() - r.getWidth() * 0.16f;

    // Floor (perspective) and back wall line
    juce::Path floor;
    floor.startNewSubPath (r.getX(), r.getBottom());
    floor.lineTo (backL, wallBottom);
    floor.lineTo (backR, wallBottom);
    floor.lineTo (r.getRight(), r.getBottom());
    floor.closeSubPath();
    g.setColour (juce::Colour (0xff14151a));
    g.fillPath (floor);
    g.setColour (C::outline.withAlpha (0.8f));
    g.strokePath (floor, juce::PathStrokeType (1.0f));

    const float wallTop = r.getY() - 10.0f;
    const float w = backR - backL;
    g.setColour (C::outline.withAlpha (0.55f));
    if (room == 0)        // studio: acoustic panels
    {
        for (int i = 0; i < 5; ++i)
            g.drawRect (juce::Rectangle<float> (backL + w * (0.06f + 0.19f * (float) i), wallTop + 8.0f, w * 0.14f, (wallBottom - wallTop) * 0.45f), 1.0f);
    }
    else if (room == 1)   // room: two windows
    {
        for (int i = 0; i < 2; ++i)
        {
            auto win = juce::Rectangle<float> (backL + w * (0.2f + 0.42f * (float) i), wallTop + 10.0f, w * 0.18f, (wallBottom - wallTop) * 0.5f);
            g.drawRect (win, 1.0f);
            g.drawLine (win.getCentreX(), win.getY(), win.getCentreX(), win.getBottom(), 1.0f);
        }
    }
    else                  // hall: columns, cathedral: pointed arches
    {
        const int n = room == 2 ? 6 : 5;
        for (int i = 0; i < n; ++i)
        {
            const float cx = backL + w * ((float) i + 0.5f) / (float) n;
            const float hw = w / (float) n * 0.32f;
            juce::Path arch;
            arch.startNewSubPath (cx - hw, wallBottom);
            arch.lineTo (cx - hw, wallTop + 26.0f);
            if (room == 3)
            {
                arch.quadraticTo (cx - hw, wallTop + 8.0f, cx, wallTop);
                arch.quadraticTo (cx + hw, wallTop + 8.0f, cx + hw, wallTop + 26.0f);
            }
            else
                arch.lineTo (cx + hw, wallTop + 26.0f);
            arch.lineTo (cx + hw, wallBottom);
            g.strokePath (arch, juce::PathStrokeType (1.0f));
        }
    }
}

void StageView::drawFace (juce::Graphics& g, int i, juce::Point<float> c, float s) const
{
    const auto& v = processor.engine.views[(size_t) i];
    const auto type = (SingerType) juce::jlimit (0, 9, v.type.load());
    const auto& info = typeInfo (type);
    const bool singing = v.singing.load();
    const float level = v.level.load();
    const float open = juce::jlimit (0.0f, 1.0f, v.open.load());
    const float width = juce::jlimit (0.1f, 1.0f, v.width.load());
    auto skin = faceColour (type);
    if (! singing && level < 0.02f) skin = skin.withMultipliedBrightness (0.72f);

    if (info.child) s *= 0.82f;
    const auto dark = juce::Colour (0xff15161b);

    // Glow while singing
    if (level > 0.01f)
    {
        g.setColour (C::accent.withAlpha (juce::jmin (0.35f, level * 0.6f)));
        g.fillEllipse (juce::Rectangle<float> (s * 3.0f, s * 3.0f).withCentre (c));
    }

    // Shoulders (choir robe)
    g.setColour (skin.withMultipliedBrightness (0.45f));
    g.fillEllipse (juce::Rectangle<float> (s * 2.4f, s * 1.3f).withCentre (c.translated (0.0f, s * 1.45f)));

    if (info.isCat)
    {
        juce::Path head;
        head.addEllipse (c.x - s, c.y - s * 0.85f, s * 2.0f, s * 1.7f);
        head.addTriangle (c.x - s * 0.95f, c.y - s * 0.25f, c.x - s * 0.8f, c.y - s * 1.5f, c.x - s * 0.12f, c.y - s * 0.75f);
        head.addTriangle (c.x + s * 0.95f, c.y - s * 0.25f, c.x + s * 0.8f, c.y - s * 1.5f, c.x + s * 0.12f, c.y - s * 0.75f);
        g.setColour (skin);
        g.fillPath (head);
        if (type == SingerType::Tomcat)
        {
            g.setColour (dark.withAlpha (0.4f));          // torn ear + stripes for the tomcat
            g.fillEllipse (c.x + s * 0.55f, c.y - s * 1.25f, s * 0.3f, s * 0.3f);
        }
        // eyes
        const float eyeH = s * 0.28f * (1.0f - blink[(size_t) i]);
        g.setColour (dark);
        g.fillEllipse (c.x - s * 0.5f - s * 0.13f, c.y - s * 0.2f, s * 0.26f, juce::jmax (1.0f, eyeH));
        g.fillEllipse (c.x + s * 0.5f - s * 0.13f, c.y - s * 0.2f, s * 0.26f, juce::jmax (1.0f, eyeH));
        // nose + mouth
        juce::Path nose;
        nose.addTriangle (c.x - s * 0.1f, c.y + s * 0.12f, c.x + s * 0.1f, c.y + s * 0.12f, c.x, c.y + s * 0.24f);
        g.setColour (juce::Colour (0xffc98f96));
        g.fillPath (nose);
        const float mw = s * (0.22f + 0.3f * width), mh = juce::jmax (s * 0.05f, open * s * 0.5f);
        g.setColour (dark);
        g.fillEllipse (juce::Rectangle<float> (mw, mh).withCentre ({ c.x, c.y + s * 0.3f + mh * 0.5f }));
        // whiskers
        g.setColour (C::text.withAlpha (0.35f));
        for (float side : { -1.0f, 1.0f })
            for (int k = 0; k < 2; ++k)
                g.drawLine (c.x + side * s * 0.35f, c.y + s * 0.25f + (float) k * s * 0.12f,
                            c.x + side * s * 1.2f, c.y + s * 0.15f + (float) k * s * 0.25f, 0.8f);
        return;
    }

    // People: hair behind, face, hair in front
    const auto hair = info.elder ? juce::Colour (0xffe6e6e6) : (info.female ? juce::Colour (0xff3b3440) : juce::Colour (0xff2c2d33));
    if (info.female)
    {
        g.setColour (hair);
        g.fillRoundedRectangle (juce::Rectangle<float> (s * 2.1f, s * 2.4f).withCentre (c.translated (0.0f, s * 0.25f)), s * 0.9f);
    }
    g.setColour (skin);
    g.fillEllipse (juce::Rectangle<float> (s * 1.7f, s * 2.0f).withCentre (c));
    if (! info.female)
    {
        juce::Path cap;
        cap.addCentredArc (c.x, c.y - s * 0.15f, s * 0.86f, s * 0.9f, 0.0f, -1.6f, 1.6f, true);
        cap.closeSubPath();
        g.setColour (hair.withAlpha (info.elder ? 0.9f : 1.0f));
        if (info.elder)   // receding hairline: just the sides
        {
            g.fillEllipse (c.x - s * 0.9f, c.y - s * 0.55f, s * 0.35f, s * 0.6f);
            g.fillEllipse (c.x + s * 0.55f, c.y - s * 0.55f, s * 0.35f, s * 0.6f);
        }
        else
            g.fillPath (cap);
    }
    else
    {
        g.setColour (hair);
        g.fillEllipse (juce::Rectangle<float> (s * 1.75f, s * 0.8f).withCentre (c.translated (0.0f, -s * 0.72f)));
    }

    // eyes (elders get glasses)
    const float eyeH = s * 0.18f * (1.0f - blink[(size_t) i]);
    g.setColour (dark);
    g.fillEllipse (c.x - s * 0.38f - s * 0.09f, c.y - s * 0.18f, s * 0.18f, juce::jmax (1.0f, eyeH));
    g.fillEllipse (c.x + s * 0.38f - s * 0.09f, c.y - s * 0.18f, s * 0.18f, juce::jmax (1.0f, eyeH));
    if (info.elder)
    {
        g.setColour (dark.withAlpha (0.7f));
        g.drawEllipse (c.x - s * 0.62f, c.y - s * 0.3f, s * 0.46f, s * 0.4f, 1.0f);
        g.drawEllipse (c.x + s * 0.16f, c.y - s * 0.3f, s * 0.46f, s * 0.4f, 1.0f);
    }

    // mouth: width from rounding / spreading, height from jaw opening x loudness
    const float mw = s * (0.28f + 0.5f * width), mh = juce::jmax (s * 0.06f, open * s * 0.62f);
    g.setColour (juce::Colour (0xff6a3a44));
    g.fillEllipse (juce::Rectangle<float> (mw, mh).withCentre ({ c.x, c.y + s * 0.5f }));
    if (open > 0.15f)
    {
        g.setColour (dark);
        g.fillEllipse (juce::Rectangle<float> (mw * 0.75f, mh * 0.65f).withCentre ({ c.x, c.y + s * 0.52f }));
    }
}

void StageView::drawFace3D (juce::Graphics& g, int i, juce::Point<float> c, float s) const
{
    // Pseudo-3D: soft lighting from the upper left, features turned towards the microphones
    const auto& v = processor.engine.views[(size_t) i];
    const auto type = (SingerType) juce::jlimit (0, 9, v.type.load());
    const auto& info = typeInfo (type);
    const bool singing = v.singing.load();
    const float level = v.level.load();
    const float open = juce::jlimit (0.0f, 1.0f, v.open.load());
    const float width = juce::jlimit (0.1f, 1.0f, v.width.load());
    const float turn = juce::jlimit (-1.0f, 1.0f, v.facing.load()) * 0.28f * s;
    auto skin = faceColour (type);
    if (! singing && level < 0.02f) skin = skin.withMultipliedBrightness (0.75f);
    if (info.child) s *= 0.85f;
    const auto dark = juce::Colour (0xff120f14);

    // floor shadow and body
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (s * 2.6f, s * 0.6f).withCentre (c.translated (0.0f, s * 2.05f)));
    juce::ColourGradient robe (skin.withMultipliedBrightness (0.6f), c.x - s, c.y + s, skin.withMultipliedBrightness (0.25f), c.x + s, c.y + s * 2.2f, false);
    g.setGradientFill (robe);
    g.fillEllipse (juce::Rectangle<float> (s * 2.5f, s * 1.5f).withCentre (c.translated (0.0f, s * 1.55f)));

    auto shade = [&] (juce::Path& p, juce::Colour col, juce::Rectangle<float> box)
    {
        juce::ColourGradient grad (col.brighter (0.35f), box.getX() + box.getWidth() * 0.32f, box.getY() + box.getHeight() * 0.25f,
                                   col.darker (0.55f), box.getRight(), box.getBottom(), true);
        g.setGradientFill (grad);
        g.fillPath (p);
    };

    if (info.isCat)
    {
        juce::Path head;
        const auto box = juce::Rectangle<float> (s * 2.0f, s * 1.75f).withCentre (c);
        head.addEllipse (box);
        for (float side : { -1.0f, 1.0f })
        {
            juce::Path ear;
            ear.addTriangle (c.x + side * s * 0.95f, c.y - s * 0.25f, c.x + side * s * 0.8f + turn * 0.3f, c.y - s * 1.55f, c.x + side * s * 0.1f, c.y - s * 0.75f);
            shade (ear, skin, box);
            juce::Path inner;
            inner.addTriangle (c.x + side * s * 0.78f, c.y - s * 0.4f, c.x + side * s * 0.74f + turn * 0.3f, c.y - s * 1.25f, c.x + side * s * 0.3f, c.y - s * 0.72f);
            g.setColour (juce::Colour (0xffd59aa4).withAlpha (0.8f));
            g.fillPath (inner);
        }
        shade (head, skin, box);
    }
    else
    {
        const auto hair = info.elder ? juce::Colour (0xffe8e8e8) : (info.female ? juce::Colour (0xff43394a) : juce::Colour (0xff302f37));
        const auto box = juce::Rectangle<float> (s * 1.75f, s * 2.05f).withCentre (c);
        if (info.female)
        {
            juce::Path back;
            back.addRoundedRectangle (juce::Rectangle<float> (s * 2.15f, s * 2.45f).withCentre (c.translated (-turn * 0.4f, s * 0.25f)), s * 0.9f);
            shade (back, hair, box);
        }
        juce::Path head;
        head.addEllipse (box);
        for (float side : { -1.0f, 1.0f })     // ears
        {
            juce::Path ear;
            ear.addEllipse (juce::Rectangle<float> (s * 0.3f, s * 0.48f).withCentre ({ c.x + side * s * 0.86f - turn * 0.5f, c.y + s * 0.02f }));
            shade (ear, skin.darker (0.1f), box);
        }
        shade (head, skin, box);
        juce::Path top;
        if (info.elder)
        {
            top.addEllipse (c.x - s * 0.92f - turn * 0.3f, c.y - s * 0.6f, s * 0.38f, s * 0.62f);
            top.addEllipse (c.x + s * 0.54f - turn * 0.3f, c.y - s * 0.6f, s * 0.38f, s * 0.62f);
        }
        else
            top.addCentredArc (c.x - turn * 0.3f, c.y - s * 0.18f, s * 0.9f, s * 0.92f, 0.0f, -1.6f, 1.6f, true);
        shade (top, hair, box);
    }

    // eyes: whites, irises looking at the mics, lids blink
    const float eyeY = c.y - s * (info.isCat ? 0.12f : 0.16f);
    for (float side : { -1.0f, 1.0f })
    {
        const auto eye = juce::Rectangle<float> (s * 0.34f, s * 0.26f * (1.0f - blink[(size_t) i] * 0.9f)).withCentre ({ c.x + side * s * 0.42f + turn, eyeY });
        g.setColour (info.isCat ? juce::Colour (0xffd8e070) : juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (eye);
        g.setColour (dark);
        const float iw = info.isCat ? s * 0.08f : s * 0.16f;
        g.fillEllipse (juce::Rectangle<float> (iw, juce::jmax (1.0f, eye.getHeight() * 0.9f)).withCentre (eye.getCentre().translated (turn * 0.25f, 0.0f)));
        g.setColour (skin.darker (0.35f));
        g.drawLine (eye.getX(), eye.getY() - 1.0f, eye.getRight(), eye.getY() - 1.0f, 1.0f);
    }

    // nose
    juce::Path nose;
    if (info.isCat)
        nose.addTriangle (c.x - s * 0.1f + turn, c.y + s * 0.14f, c.x + s * 0.1f + turn, c.y + s * 0.14f, c.x + turn, c.y + s * 0.26f);
    else
        nose.addEllipse (juce::Rectangle<float> (s * 0.22f, s * 0.14f).withCentre ({ c.x + turn * 1.2f, c.y + s * 0.18f }));
    g.setColour (info.isCat ? juce::Colour (0xffc98f96) : skin.darker (0.3f));
    g.fillPath (nose);

    // mouth with depth: lips, dark inside, teeth and tongue when open
    const float mw = s * (info.isCat ? 0.22f + 0.3f * width : 0.3f + 0.5f * width);
    const float mh = juce::jmax (s * 0.07f, open * s * (info.isCat ? 0.5f : 0.6f));
    const juce::Point<float> mc (c.x + turn, c.y + s * (info.isCat ? 0.42f : 0.52f));
    const auto mouth = juce::Rectangle<float> (mw, mh).withCentre (mc);
    g.setColour (info.isCat ? dark : juce::Colour (0xff9a5a66));
    g.fillEllipse (mouth.expanded (s * 0.04f));
    juce::ColourGradient inside (juce::Colour (0xff3a1820), mc.x, mc.y - mh * 0.3f, dark, mc.x, mc.y + mh * 0.5f, false);
    g.setGradientFill (inside);
    g.fillEllipse (mouth);
    if (open > 0.25f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillRect (mouth.withHeight (mh * 0.16f).reduced (mw * 0.18f, 0.0f).translated (0.0f, mh * 0.06f));
        g.setColour (juce::Colour (0xffc76e7c));
        g.fillEllipse (juce::Rectangle<float> (mw * 0.55f, mh * 0.35f).withCentre ({ mc.x, mouth.getBottom() - mh * 0.2f }));
    }

    if (level > 0.01f)
    {
        g.setColour (C::accent.withAlpha (juce::jmin (0.5f, level * 0.8f)));
        g.drawEllipse (juce::Rectangle<float> (s * 2.6f, s * 2.6f).withCentre (c), 1.5f);
    }
}

void StageView::drawLips (juce::Graphics& g, int i, juce::Rectangle<float> cell) const
{
    const auto& v = processor.engine.views[(size_t) i];
    const auto type = (SingerType) juce::jlimit (0, 9, v.type.load());
    const auto& info = typeInfo (type);
    const float open = juce::jlimit (0.0f, 1.0f, v.open.load());
    const float width = juce::jlimit (0.1f, 1.0f, v.width.load());
    const bool singing = v.singing.load();
    const auto inner = cell.reduced (5.0f);

    g.setColour (faceColour (type).withMultipliedBrightness (singing ? 0.42f : 0.28f));
    g.fillRoundedRectangle (inner, 8.0f);

    const auto c = inner.getCentre().translated (0.0f, -inner.getHeight() * 0.04f);
    const float size = juce::jmin (inner.getWidth(), inner.getHeight() * 1.2f);
    const float W = size * (0.36f + 0.46f * width), H = size * (0.05f + 0.36f * open);
    const auto lipCol = info.isCat ? juce::Colour (0xff3a2e33) : juce::Colour (0xffc0707e);

    // inside of the mouth
    juce::Path inside;
    inside.addEllipse (juce::Rectangle<float> (W, H).withCentre (c));
    juce::ColourGradient dark (juce::Colour (0xff40141c), c.x, c.y - H * 0.4f, juce::Colour (0xff120a0c), c.x, c.y + H * 0.5f, false);
    g.setGradientFill (dark);
    g.fillPath (inside);
    if (open > 0.2f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (W * 0.62f, H * 0.14f).withCentre (c.translated (0.0f, -H * 0.36f)));
        g.setColour (juce::Colour (0xffd07a88));
        g.fillEllipse (juce::Rectangle<float> (W * 0.5f, H * 0.38f).withCentre (c.translated (0.0f, H * 0.28f)));
    }

    // upper lip with a cupid's bow, lower lip as a fuller curve
    const float t = juce::jmax (3.0f, size * 0.07f);
    juce::Path upper;
    upper.startNewSubPath (c.x - W * 0.55f, c.y);
    upper.cubicTo (c.x - W * 0.35f, c.y - H * 0.55f - t, c.x - W * 0.12f, c.y - H * 0.5f - t * 1.4f, c.x, c.y - H * 0.45f - t * 0.9f);
    upper.cubicTo (c.x + W * 0.12f, c.y - H * 0.5f - t * 1.4f, c.x + W * 0.35f, c.y - H * 0.55f - t, c.x + W * 0.55f, c.y);
    upper.cubicTo (c.x + W * 0.3f, c.y - H * 0.45f, c.x - W * 0.3f, c.y - H * 0.45f, c.x - W * 0.55f, c.y);
    juce::Path lower;
    lower.startNewSubPath (c.x - W * 0.55f, c.y);
    lower.cubicTo (c.x - W * 0.3f, c.y + H * 0.48f, c.x + W * 0.3f, c.y + H * 0.48f, c.x + W * 0.55f, c.y);
    lower.cubicTo (c.x + W * 0.32f, c.y + H * 0.6f + t * 1.6f, c.x - W * 0.32f, c.y + H * 0.6f + t * 1.6f, c.x - W * 0.55f, c.y);
    juce::ColourGradient lipGrad (lipCol.brighter (0.3f), c.x, c.y - H, lipCol.darker (0.4f), c.x, c.y + H + t * 2.0f, false);
    g.setGradientFill (lipGrad);
    g.fillPath (upper);
    g.fillPath (lower);

    g.setColour (C::textDim);
    g.setFont (cat::catFont (juce::jlimit (8.0f, 11.0f, inner.getHeight() * 0.12f)));
    const int note = v.note.load();
    g.drawText (juce::String (info.name) + (note >= 0 && singing ? "  " + juce::MidiMessage::getMidiNoteName (note, true, true, 3) : juce::String()),
                inner.withTrimmedTop (inner.getHeight() - juce::jmin (16.0f, inner.getHeight() * 0.22f)), juce::Justification::centred);
}

void StageView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff101116));
    g.fillRoundedRectangle (bounds, 8.0f);
    const int view = processor.faceView.load();
    for (int v = 0; v < 3; ++v)
        viewButtons[v].setColour (juce::TextButton::buttonColourId, v == view ? C::accent.withAlpha (0.35f) : C::raised);

    const int n = numSingers();
    if (view == 1)
    {
        // Lips: one close-up mouth per singer
        auto area = bounds.reduced (12.0f).withTrimmedTop (26.0f);
        const int cols = juce::jmax (1, (int) std::ceil (std::sqrt ((double) n * area.getWidth() / juce::jmax (1.0f, area.getHeight()))));
        const int rows = (n + cols - 1) / cols;
        const float cw = area.getWidth() / (float) cols, ch = area.getHeight() / (float) juce::jmax (1, rows);
        for (int i = 0; i < n; ++i)
            drawLips (g, i, { area.getX() + cw * (float) (i % cols), area.getY() + ch * (float) (i / cols), cw, ch });
        g.setColour (C::textDim);
        g.setFont (cat::catFont (11.0f, true));
        g.drawText ("LIPS  -  " + juce::String (n) + (n == 1 ? " SINGER" : " SINGERS"), bounds.reduced (12.0f, 6.0f).removeFromTop (16.0f),
                    juce::Justification::centredLeft);
        return;
    }
    drawRoom (g, floorArea());

    std::vector<int> order ((size_t) n);
    for (int i = 0; i < n; ++i) order[(size_t) i] = i;
    std::sort (order.begin(), order.end(), [this] (int a, int b)
    {
        return processor.engine.stageY[(size_t) a].load() > processor.engine.stageY[(size_t) b].load();
    });

    const auto r = floorArea();
    const float base = juce::jmin (r.getWidth(), r.getHeight()) * (n > 32 ? 0.045f : (n > 16 ? 0.058f : (n > 6 ? 0.075f : 0.1f)));
    for (int i : order)
    {
        float scale = 1.0f;
        const auto pos = facePosition (i, scale);
        if (view == 2) drawFace3D (g, i, pos, base * scale * 1.15f);
        else           drawFace (g, i, pos, base * scale);

        const int note = processor.engine.views[(size_t) i].note.load();
        if (n <= 8 && note >= 0 && processor.engine.views[(size_t) i].singing.load())
        {
            g.setColour (C::textDim);
            g.setFont (cat::catFont (10.5f, true));
            g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3),
                        juce::Rectangle<float> (50.0f, 14.0f).withCentre (pos.translated (0.0f, -base * scale * 1.9f)), juce::Justification::centred);
        }
    }

    drawMics (g);

    // Caption
    g.setColour (C::textDim);
    g.setFont (cat::catFont (11.0f, true));
    const int cast = juce::roundToInt (value (processor, "cast"));
    const int room = juce::roundToInt (value (processor, "room"));
    g.drawText ("STAGE  -  " + castNames()[cast].toUpperCase() + "  -  " + roomNames()[room].toUpperCase() + "  -  " + juce::String (n)
                    + (n == 1 ? " SINGER" : " SINGERS"),
                bounds.reduced (12.0f, 6.0f).removeFromTop (16.0f), juce::Justification::centredLeft);
}

void StageView::mouseDown (const juce::MouseEvent& e)
{
    dragging = -1;
    if (processor.faceView.load() == 1) return;
    if (e.position.getDistanceFrom (micPosition()) < 20.0f)
    {
        dragging = -2;    // the microphone pair
        return;
    }
    float best = 1.0e9f;
    for (int i = 0; i < numSingers(); ++i)
    {
        float scale = 1.0f;
        const float d = e.position.getDistanceFrom (facePosition (i, scale));
        if (d < best && d < 40.0f * scale) { best = d; dragging = i; }
    }
}

void StageView::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = screenToStage (e.position);
    if (dragging == -2)
    {
        processor.engine.micX = juce::jlimit (-1.0f, 1.0f, p.x);
        processor.engine.micY = juce::jlimit (-0.22f, 1.0f, p.y);
        return;
    }
    if (dragging < 0) return;
    processor.engine.stageX[(size_t) dragging] = juce::jlimit (-1.0f, 1.0f, p.x);
    processor.engine.stageY[(size_t) dragging] = juce::jlimit (0.0f, 1.0f, p.y);
}

void StageView::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
}

//==============================================================================
LyricStrip::LyricStrip (SingCatProcessor& p) : processor (p)
{
    setTooltip ("The lyric as sung syllables - each new note sings the next one. Click to start again from the first syllable.");
    refresh();
    startTimerHz (20);
}

void LyricStrip::refresh()
{
    syllables.clear();
    for (const auto& s : processor.engine.getSyllables())
        syllables.push_back (juce::String::fromUTF8 (s.text));
    repaint();
}

void LyricStrip::timerCallback()
{
    const int now = processor.engine.uiSyllable.load();
    if (now != shown)
    {
        shown = now;
        repaint();
    }
}

void LyricStrip::mouseDown (const juce::MouseEvent&)
{
    processor.engine.restartLyrics();
}

void LyricStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff101116));
    g.fillRoundedRectangle (r, 5.0f);
    r.reduce (6.0f, 2.0f);

    const bool on = value (processor, "words") > 0.5f;
    g.setFont (cat::catFont (12.0f, true));
    if (syllables.empty())
    {
        g.setColour (C::textDim);
        g.drawText ("Type a lyric below", r, juce::Justification::centredLeft);
        return;
    }

    // Keep the current syllable in view: start a few syllables before it
    const int current = shown;
    int first = juce::jmax (0, current - 3);
    float x = r.getX();
    for (int i = first; i < (int) syllables.size() && x < r.getRight(); ++i)
    {
        const auto& text = syllables[(size_t) i];
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 12.0f;
        const auto cell = juce::Rectangle<float> (x, r.getY(), w, r.getHeight());
        if (i == current && on)
        {
            g.setColour (C::accent.withAlpha (0.25f));
            g.fillRoundedRectangle (cell.reduced (0.0f, 1.0f), 4.0f);
            g.setColour (C::accent);
        }
        else
            g.setColour (on ? C::text.withAlpha (0.75f) : C::textDim);
        g.drawText (text, cell, juce::Justification::centred);
        x += w + 2.0f;
    }
}

//==============================================================================
SingCatEditor::SingCatEditor (SingCatProcessor& p)
    : processor (p),
      mode (p.apvts, "mode", "Mode", &p.midiLearn, 40),
      cast (p.apvts, "cast", "Cast", &p.midiLearn, 36),
      style (p.apvts, "style", "Style", &p.midiLearn, 38),
      onset (p.apvts, "onset", "Onset", &p.midiLearn, 42),
      move (p.apvts, "move", "Move", &p.midiLearn, 40),
      room (p.apvts, "room", "Room", &p.midiLearn, 40),
      quality (p.apvts, "quality", "Quality", &p.midiLearn, 50),
      moveRate (p.apvts, "moveRate", "Rate", &p.midiLearn, 36),
      spread (p.apvts, "spread", "Spread", &p.midiLearn, 46),
      roomMix (p.apvts, "roomMix", "Mix", &p.midiLearn, 30),
      micWidth (p.apvts, "micWidth", "Width", &p.midiLearn, 42),
      micDist (p.apvts, "micDist", "Distance", &p.midiLearn, 58),
      attack (p.apvts, "attack", "Attack", &p.midiLearn, 46),
      release (p.apvts, "release", "Release", &p.midiLearn, 52),
      vibrato (p.apvts, "vibrato", "Vibrato", &p.midiLearn, 52),
      breath (p.apvts, "breath", "Breath", &p.midiLearn, 48),
      rasp (p.apvts, "rasp", "Rasp", &p.midiLearn, 36),
      glide (p.apvts, "glide", "Glide", &p.midiLearn, 38),
      drift (p.apvts, "drift", "Drift", &p.midiLearn, 36),
      human (p.apvts, "human", "Loose", &p.midiLearn, 40),
      purr (p.apvts, "purr", "Purr", &p.midiLearn, 34),
      neck (p.apvts, "neck", "Neck", &p.midiLearn, 36),
      jaw (p.apvts, "jaw", "Jaw", &p.midiLearn, 30),
      lips (p.apvts, "lips", "Lips", &p.midiLearn, 32),
      nose (p.apvts, "nose", "Nose", &p.midiLearn, 36),
      chest (p.apvts, "chest", "Chest", &p.midiLearn, 40),
      volume (p.apvts, "volume", "Volume", &p.midiLearn, 50),
      mpe (p.apvts, "mpe", "MPE", &p.midiLearn),
      words (p.apvts, "words", "Words", &p.midiLearn),
      diction (p.apvts, "diction", "Diction", &p.midiLearn, 54),
      lyricStrip (p),
      pad (p),
      stage (p),
      catView (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, cat::CatStyle::Sing),
      surpriseControls (p, p.surpriseUndo, [&p] { p.surprise(); }),
      bootScreen (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); },
                  "SING", "-cat", "CAT & HUMAN VOICES", cat::CatStyle::Sing)
{
    setLookAndFeel (&lnf);

    presetStepper = std::make_unique<cat::PresetBar> (p.presetManager);
    singers = cat::Stepper::forParameter (p.apvts, "singers", "Singers");
    unison = cat::Stepper::forParameter (p.apvts, "unison", "Unison");
    bendRange = cat::Stepper::forParameter (p.apvts, "bendRange", "Bend");

    mode.getCombo().setTooltip ("Solo: one singer, legato glides.  Choir: each note gets its own singer(s).  Tutti: everybody sings, "
                                "split over the held notes, each in their own octave.");
    cast.getCombo().setTooltip ("Who is on stage: people of all ages and/or cats");
    style.getCombo().setTooltip ("Singing style: tension, vibrato, scoops, breath, rasp and nasality");
    onset.getCombo().setTooltip ("How notes start. Auto: cats meow, people open on the vowel");
    move.getCombo().setTooltip ("Vowel movement: Wave (A<->B), Envelope (A to B per note), Steps (random vowels), Path (drawn), Random (per note)");
    unison->setTooltip ("Choir mode: how many singers take each note");
    spread.getSlider().setTooltip ("How differently the singers move through the vowels");
    human.getSlider().setTooltip ("Singers come in slightly apart, like real people");
    drift.getSlider().setTooltip ("Slow pitch drift of each singer");
    purr.getSlider().setTooltip ("Purring: closed mouth, ~25 pulses per second (strongest for cats)");
    neck.getSlider().setTooltip ("Longer / shorter vocal tract (darker / brighter voice)");
    jaw.getSlider().setTooltip ("Jaw opening");
    lips.getSlider().setTooltip ("Lip rounding (+) or spreading (-)");
    nose.getSlider().setTooltip ("Nasality (velum opening)");
    chest.getSlider().setTooltip ("Light (-) or full, pressed (+) voice");
    mpe.setTooltip ("MPE: per-note pitch bend, pressure (loudness) and slide (CC74, moves the vowel towards B)");
    quality.getCombo().setTooltip ("High runs the vocal tracts at twice the sample rate (cleaner high voices, more CPU)");
    dice.setTooltip ("Roll new singers: new individual voices and shuffled stage positions");
    arrange.setTooltip ("Line the singers up in choir rows again");
    dice.onClick = [this] { processor.rollDice(); };

    words.setTooltip ("Words: the singers sing the lyric below, one syllable per note (the vowel chart is not used)");
    diction.getSlider().setTooltip ("How crisp and strong the consonants are");
    lyricsBox.setText (processor.getLyricsText(), juce::dontSendNotification);
    lyricsBox.setTextToShowWhenEmpty ("lyrics, e.g. hel-lo my dar-ling  (use - between syllables, + to hold a note)", C::textDim);
    lyricsBox.setTooltip ("Type a lyric. Spaces or - split syllables, + holds the previous syllable over the next note. "
                          "Each new note sings the next syllable; the lyric starts again when the song starts.");
    lyricsBox.setFont (cat::catFont (13.0f));
    lyricsBox.setIndents (6, 6);
    lyricsBox.onTextChange = [this]
    {
        processor.setLyricsText (lyricsBox.getText());
        lyricStrip.refresh();
    };
    arrange.onClick = [this] { processor.resetStage(); };

    for (auto* c : std::initializer_list<juce::Component*> {
             presetStepper.get(), singers.get(), unison.get(), bendRange.get(), &mode, &cast, &style, &onset, &move, &room, &quality,
             &moveRate, &spread, &roomMix, &micWidth, &micDist, &attack, &release, &vibrato, &breath, &rasp, &glide, &drift,
             &human, &purr, &neck, &jaw, &lips, &nose, &chest, &volume, &mpe, &dice, &arrange, &pad, &stage,
             &words, &diction, &lyricsBox, &lyricStrip })
        addAndMakeVisible (c);

    addAndMakeVisible (catView);
    addAndMakeVisible (surpriseControls);
    addAndMakeVisible (bootScreen);
    setSize (1080, 760);
}

SingCatEditor::~SingCatEditor()
{
    setLookAndFeel (nullptr);
}

void SingCatEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::background);
    cat::CatLookAndFeel::drawLogo (g, juce::Rectangle<float> (14.0f, 8.0f, 160.0f, 32.0f), "SING", "-cat");

    g.setColour (C::textDim);
    g.setFont (cat::catFont (11.0f, true));
    g.drawText ("VOICE", juce::Rectangle<int> (12, cast.getY() - 17, 200, 14), juce::Justification::centredLeft);
    g.drawText ("CHARACTER & PLAY", juce::Rectangle<int> (12, breath.getY() - 17, 200, 14), juce::Justification::centredLeft);
    g.drawText ("BODY", juce::Rectangle<int> (12, neck.getY() - 17, 200, 14), juce::Justification::centredLeft);
}

void SingCatEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto item = [] (juce::Component& c, float w, float h = 30.0f) { return juce::FlexItem (c).withWidth (w).withHeight (h).withMargin ({ 0, 3, 0, 3 }); };
    auto row = [&] (juce::Rectangle<int> area, std::initializer_list<juce::FlexItem> items)
    {
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        for (auto& it : items) fb.items.add (it);
        fb.performLayout (area);
    };

    {
        auto top = r.removeFromTop (40);
        auto logo = top.removeFromLeft (160);
        catView.setBounds (logo.removeFromRight (40).expanded (0, 6));
        row (top, { juce::FlexItem().withFlex (1.0f), item (*presetStepper, 330), juce::FlexItem().withFlex (1.0f),
                    item (mode, 150), item (*singers, 112), item (surpriseControls, 122) });
    }
    r.removeFromTop (8);

    // Bottom: three rows of controls
    auto bodyRow = r.removeFromBottom (30);
    r.removeFromBottom (20);
    auto charRow = r.removeFromBottom (30);
    r.removeFromBottom (20);
    auto voiceRow = r.removeFromBottom (30);
    r.removeFromBottom (24);

    row (voiceRow, { item (cast, 180), item (style, 160), item (*unison, 116),
                     item (attack, 100).withFlex (1.0f), item (release, 100).withFlex (1.0f), item (glide, 90).withFlex (1.0f),
                     item (vibrato, 100).withFlex (1.0f) });
    row (charRow, { item (breath, 100).withFlex (1.0f), item (rasp, 90).withFlex (1.0f), item (drift, 90).withFlex (1.0f),
                    item (human, 90).withFlex (1.0f), item (purr, 90).withFlex (1.0f), item (mpe, 54), item (*bendRange, 92),
                    item (quality, 128), item (volume, 110).withFlex (1.0f) });
    row (bodyRow, { item (neck, 100).withFlex (1.0f), item (jaw, 100).withFlex (1.0f), item (lips, 100).withFlex (1.0f),
                    item (nose, 100).withFlex (1.0f), item (chest, 100).withFlex (1.0f) });

    // Main: vowel chart on the left, stage on the right
    auto left = r.removeFromLeft (400);
    r.removeFromLeft (10);
    auto wordsRow = left.removeFromBottom (30);
    left.removeFromBottom (6);
    auto lyricRow = left.removeFromBottom (24);
    left.removeFromBottom (6);
    auto moveRow2 = left.removeFromBottom (30);
    left.removeFromBottom (6);
    auto moveRow1 = left.removeFromBottom (30);
    left.removeFromBottom (8);
    pad.setBounds (left);
    lyricStrip.setBounds (lyricRow);
    row (wordsRow, { item (words, 64), item (lyricsBox, 200).withFlex (1.0f), item (diction, 110) });
    row (moveRow1, { item (move, 160).withFlex (1.0f), item (moveRate, 140).withFlex (1.0f) });
    row (moveRow2, { item (spread, 160).withFlex (1.0f), item (onset, 160).withFlex (1.0f) });

    auto stageRow = r.removeFromBottom (30);
    r.removeFromBottom (8);
    stage.setBounds (r);
    row (stageRow, { item (room, 150), item (roomMix, 90).withFlex (1.0f), item (micWidth, 100).withFlex (1.0f),
                     item (micDist, 120).withFlex (1.0f), item (dice, 64), item (arrange, 80) });

    bootScreen.setBounds (getLocalBounds());
}

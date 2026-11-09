#include "PluginEditor.h"
#include "CatTheme.h"

using namespace juce;

namespace
{
constexpr int designW = 1000, designH = 620, stripY = 522;
const Colour bgCol (0xff05050b), textCol (0xffe9e6f5);
const char* cornerNames[] = { "DREAM", "PULSE", "DARK", "WILD" };
}

//==============================================================================
Colour NebulaView::cornerColour (int corner, bool dustCol)
{
    static const Colour cloud[] = { Colour (0xff7b5cff), Colour (0xff00c8ff), Colour (0xffe0204e), Colour (0xffff9a1f) };
    static const Colour grain[] = { Colour (0xffb9a6ff), Colour (0xff5fe1ff), Colour (0xffff5e7a), Colour (0xffffc266) };
    return (dustCol ? grain : cloud)[jlimit (0, 3, corner)];
}

Colour NebulaView::colourAt (float x, float y, bool dustCol)
{
    x = jlimit (0.0f, 1.0f, x); y = jlimit (0.0f, 1.0f, y);
    const float w[4] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };
    float r = 0, g = 0, b = 0;
    for (int k = 0; k < 4; ++k)
    {
        const auto c = cornerColour (k, dustCol);
        r += w[k] * c.getFloatRed(); g += w[k] * c.getFloatGreen(); b += w[k] * c.getFloatBlue();
    }
    return Colour::fromFloatRGBA (r, g, b, 1.0f);
}

NebulaView::NebulaView (SwarmProcessor& p)
    : processor (p), px (*p.apvts.getParameter ("x")), py (*p.apvts.getParameter ("y"))
{
    setTooltip ("Drag the point: DREAM (top left), PULSE (top right, in time with Cubase), DARK (bottom left), WILD (bottom right). "
                "The small light is where LIFE has taken it. Double-click for the middle.");
    setMouseCursor (MouseCursor::CrosshairCursor);
    sparks.reserve (1200);
}

Rectangle<float> NebulaView::field() const
{
    // where the point can go (the strip at the bottom and the title stay clear)
    return getLocalBounds().toFloat().withTrimmedTop (96.0f).withTrimmedBottom (110.0f).reduced (40.0f, 0.0f);
}

void NebulaView::resized()
{
    const int w = jmax (1, getWidth()), h = jmax (1, getHeight());
    stars = Image (Image::ARGB, w, h, true);
    dust = Image (Image::ARGB, w, h, true);
    Random r (11);
    {
        Graphics g (stars);
        for (int i = 0; i < 260; ++i)
        {
            const float s = r.nextFloat() < 0.15f ? 2.2f : (r.nextFloat() < 0.5f ? 1.6f : 1.1f);
            g.setColour (Colours::white.withAlpha (0.2f + 0.6f * r.nextFloat()));
            g.fillEllipse (r.nextFloat() * (float) w, r.nextFloat() * (float) h, s, s);
        }
    }
    {
        Graphics g (dust);
        const float W = (float) w, H = (float) h;
        struct Cl { float cx, cy, sx, sy; int corner; };
        const Cl cls[] = { { 0.25f * W, 0.36f * H, 0.12f * W, 0.14f * H, 0 }, { 0.76f * W, 0.33f * H, 0.11f * W, 0.13f * H, 1 },
                           { 0.28f * W, 0.76f * H, 0.15f * W, 0.11f * H, 2 }, { 0.74f * W, 0.74f * H, 0.12f * W, 0.12f * H, 3 } };
        auto gauss = [&r] { float s = 0; for (int k = 0; k < 4; ++k) s += r.nextFloat(); return (s - 2.0f) * 1.73f; };
        for (int i = 0; i < 5200; ++i)
        {
            float x, y; Colour c;
            if (r.nextFloat() < 0.22f)
            {
                x = r.nextFloat() * W; y = r.nextFloat() * H * 0.95f;
                c = Colours::white;
            }
            else
            {
                const auto& cl = cls[r.nextInt (4)];
                x = cl.cx + gauss() * cl.sx; y = cl.cy + gauss() * cl.sy;
                c = r.nextFloat() < 0.25f ? Colours::white : cornerColour (cl.corner, true);
            }
            const float s = 0.7f + std::pow (r.nextFloat(), 3.0f) * 2.4f;
            g.setColour (c.withAlpha (0.25f + 0.7f * r.nextFloat()));
            g.fillEllipse (x, y, s, s);
        }
    }
}

void NebulaView::tick (float dt)
{
    time += dt;
    const float e = processor.engine.energy.load();
    energySmooth += (e - energySmooth) * jmin (1.0f, dt * 6.0f);

    // a spark for every grain
    const auto f = field();
    const int n = processor.engine.popGrainEvents (evBuf.data(), (int) evBuf.size());
    for (int i = 0; i < n && sparks.size() < 1100; ++i)
    {
        const auto& ev = evBuf[(size_t) i];
        Spark s;
        s.x = f.getX() + ev.x * f.getWidth() + (rng.nextFloat() - 0.5f) * 30.0f;
        s.y = f.getY() + ev.y * f.getHeight() + (rng.nextFloat() - 0.5f) * 30.0f;
        const float a = rng.nextFloat() * MathConstants<float>::twoPi;
        const float sp = 6.0f + 30.0f * rng.nextFloat();
        s.vx = std::cos (a) * sp + ev.pan * 18.0f;
        s.vy = std::sin (a) * sp - ev.semis * 1.2f;
        if (ev.reverse) { s.vx = -s.vx; s.vy = -s.vy; }
        s.age = 0.0f;
        s.life = jlimit (0.35f, 2.8f, ev.seconds * 2.0f + 0.3f);
        s.size = jlimit (1.2f, 4.2f, 1.2f + ev.level * 3.5f);
        s.col = colourAt (ev.x, ev.y, true).interpolatedWith (Colours::white, ev.semis > 0.0f ? 0.45f : 0.15f);
        sparks.push_back (s);
    }
    for (auto& s : sparks)
    {
        s.age += dt;
        s.x += s.vx * dt; s.y += s.vy * dt;
        s.vx *= 0.985f; s.vy *= 0.985f;
    }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const Spark& s) { return s.age >= s.life; }), sparks.end());

    trail.push_back ({ processor.engine.wanderX.load(), processor.engine.wanderY.load() });
    if (trail.size() > 36) trail.erase (trail.begin());
    repaint();
}

void NebulaView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float W = b.getWidth(), H = b.getHeight();
    g.fillAll (bgCol);
    g.drawImageAt (stars, 0, 0);

    // the four clouds breathe, the one the swarm is in glows
    const float wx = processor.engine.wanderX.load(), wy = processor.engine.wanderY.load();
    const float wts[4] = { (1 - wx) * (1 - wy), wx * (1 - wy), (1 - wx) * wy, wx * wy };
    const Point<float> centres[4] = { { 0.25f * W, 0.34f * H }, { 0.77f * W, 0.31f * H }, { 0.27f * W, 0.77f * H }, { 0.75f * W, 0.75f * H } };
    const Point<float> radii[4] = { { 0.30f * W, 0.36f * H }, { 0.28f * W, 0.33f * H }, { 0.32f * W, 0.29f * H }, { 0.29f * W, 0.31f * H } };
    for (int k = 0; k < 4; ++k)
    {
        const float breathe = 1.0f + 0.04f * std::sin (time * (0.35f + 0.1f * (float) k) + (float) k * 1.7f);
        const auto c = centres[k].translated (std::sin (time * 0.11f + (float) k) * 10.0f, std::cos (time * 0.09f + (float) k * 2.0f) * 8.0f);
        const float rx = radii[k].x * breathe, ry = radii[k].y * breathe;
        const float alpha = jlimit (0.0f, 0.95f, 0.30f + 0.35f * wts[k] + 0.25f * energySmooth * wts[k]);
        const auto col = cornerColour (k, false);
        ColourGradient grad (col.withAlpha (alpha), c.x, c.y, col.withAlpha (0.0f), c.x + rx, c.y, true);
        grad.addColour (0.45, col.withAlpha (alpha * 0.45f));
        Graphics::ScopedSaveState save (g);
        g.addTransform (AffineTransform::scale (1.0f, ry / rx, c.x, c.y));     // an elliptical glow that fades out at every edge
        g.setGradientFill (grad);
        g.fillEllipse (c.x - rx, c.y - rx, rx * 2.0f, rx * 2.0f);
    }
    g.drawImageAt (dust, 0, 0);

    // the grains
    for (const auto& s : sparks)
    {
        const float t = s.age / s.life;
        const float a = t < 0.1f ? t / 0.1f : 1.0f - (t - 0.1f) / 0.9f;
        g.setColour (s.col.withAlpha (jlimit (0.0f, 1.0f, a)));
        g.fillEllipse (s.x - s.size * 0.5f, s.y - s.size * 0.5f, s.size, s.size);
        if (s.size > 2.6f)
        {
            g.setColour (s.col.withAlpha (a * 0.09f));
            g.fillEllipse (s.x - s.size * 2.0f, s.y - s.size * 2.0f, s.size * 4.0f, s.size * 4.0f);
        }
    }

    // fade into the strip at the bottom
    g.setGradientFill (ColourGradient (bgCol.withAlpha (0.0f), 0.0f, H * 0.78f, bgCol, 0.0f, H, false));
    g.fillRect (b.withTop (H * 0.78f));

    // labels
    g.setFont (cat::displayFont (13.0f).withExtraKerningFactor (0.38f));
    const auto f = field();
    g.setColour (Colours::white.withAlpha (0.72f));
    g.drawText (cornerNames[0], Rectangle<float> (f.getX(), f.getY() + 4.0f, 200.0f, 20.0f), Justification::centredLeft);
    g.drawText (cornerNames[1], Rectangle<float> (f.getRight() - 200.0f, f.getY() + 4.0f, 200.0f, 20.0f), Justification::centredRight);
    g.drawText (cornerNames[2], Rectangle<float> (f.getX(), f.getBottom() - 24.0f, 200.0f, 20.0f), Justification::centredLeft);
    g.drawText (cornerNames[3], Rectangle<float> (f.getRight() - 200.0f, f.getBottom() - 24.0f, 200.0f, 20.0f), Justification::centredRight);

    g.setColour (textCol);
    g.setFont (cat::displayFont (40.0f));
    g.drawText ("swarm", Rectangle<float> (40.0f, 26.0f, 300.0f, 46.0f), Justification::centredLeft);
    g.setFont (cat::catFont (11.5f));
    g.setColour (textCol.withAlpha (0.5f));
    g.drawText ("drag the dust " + String::fromUTF8 ("\xc2\xb7") + " it lives on its own", Rectangle<float> (40.0f, 72.0f, 320.0f, 16.0f), Justification::centredLeft);

    // where LIFE has taken it (a fading trail) and your point
    auto toScreen = [&f] (Point<float> p) { return Point<float> (f.getX() + p.x * f.getWidth(), f.getY() + p.y * f.getHeight()); };
    for (size_t i = 1; i < trail.size(); ++i)
    {
        g.setColour (colourAt (wx, wy, true).withAlpha (0.3f * (float) i / (float) trail.size()));
        g.drawLine (Line<float> (toScreen (trail[i - 1]), toScreen (trail[i])), 1.0f);
    }
    const auto me = toScreen ({ px.convertFrom0to1 (px.getValue()), py.convertFrom0to1 (py.getValue()) });
    const auto lifeAt = toScreen ({ wx, wy });
    {
        Path link;
        link.startNewSubPath (me);
        link.quadraticTo (me.translated ((lifeAt.x - me.x) * 0.5f, -30.0f), lifeAt);
        Path dashed;
        const float dashes[] = { 2.0f, 5.0f };
        PathStrokeType (1.0f).createDashedStroke (dashed, link, dashes, 2);
        g.setColour (Colours::white.withAlpha (0.4f));
        g.fillPath (dashed);
    }
    const float pulse = 1.0f + 0.08f * energySmooth + 0.03f * std::sin (time * 2.0f);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawEllipse (Rectangle<float> (108.0f * pulse, 108.0f * pulse).withCentre (me), 1.0f);
    g.setColour (Colours::white.withAlpha (0.08f + 0.1f * energySmooth));
    g.fillEllipse (Rectangle<float> (60.0f, 60.0f).withCentre (me));
    g.setColour (Colours::white);
    g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (me));
    g.setColour (colourAt (wx, wy, true));
    g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (lifeAt));
    g.setColour (colourAt (wx, wy, true).withAlpha (0.25f));
    g.fillEllipse (Rectangle<float> (20.0f, 20.0f).withCentre (lifeAt));

    if (processor.engine.frozen.load())
    {
        g.setColour (Colour (0xffbfe9ff).withAlpha (0.85f));
        g.setFont (cat::displayFont (13.0f).withExtraKerningFactor (0.38f));
        g.drawText ("FROZEN", Rectangle<float> (0.0f, f.getY() + 4.0f, W, 20.0f), Justification::centredTop);
    }
}

void NebulaView::setPoint (Point<float> pos)
{
    const auto f = field();
    const float x = jlimit (0.0f, 1.0f, (pos.x - f.getX()) / f.getWidth());
    const float y = jlimit (0.0f, 1.0f, (pos.y - f.getY()) / f.getHeight());
    px.setValueNotifyingHost (px.convertTo0to1 (x));
    py.setValueNotifyingHost (py.convertTo0to1 (y));
}

void NebulaView::mouseDown (const MouseEvent& e)
{
    px.beginChangeGesture(); py.beginChangeGesture();
    dragging = true;
    setPoint (e.position);
}

void NebulaView::mouseDrag (const MouseEvent& e) { if (dragging) setPoint (e.position); }

void NebulaView::mouseUp (const MouseEvent&)
{
    if (dragging) { px.endChangeGesture(); py.endChangeGesture(); }
    dragging = false;
}

void NebulaView::mouseDoubleClick (const MouseEvent&)
{
    px.beginChangeGesture(); py.beginChangeGesture();
    px.setValueNotifyingHost (px.convertTo0to1 (0.5f));
    py.setValueNotifyingHost (py.convertTo0to1 (0.5f));
    px.endChangeGesture(); py.endChangeGesture();
}

//==============================================================================
DustSlider::DustSlider (AudioProcessorValueTreeState& s, const String& id, const String& cap)
    : juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox), caption (cap), attachment (s, id, *this)
{
    setSliderSnapsToMousePosition (false);
    setVelocityBasedMode (false);
}

void DustSlider::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setFont (cat::catFont (10.5f).withExtraKerningFactor (0.12f));
    g.setColour (textCol.withAlpha (0.55f));
    g.drawText (caption, b.withHeight (14.0f), Justification::centredLeft);
    const float y = 22.0f, w = b.getWidth() - 6.0f;
    const float p = (float) valueToProportionOfLength (getValue());
    g.setColour (Colours::white.withAlpha (0.15f));
    g.fillRect (3.0f, y, w, 2.0f);
    g.setColour (Colours::white.withAlpha (isMouseOverOrDragging() ? 1.0f : 0.9f));
    g.fillRect (3.0f, y, w * p, 2.0f);
    g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre ({ 3.0f + w * p, y + 1.0f }));
    g.setColour (textCol.withAlpha (0.88f));
    g.setFont (cat::catFont (11.0f));
    g.drawText (getTextFromValue (getValue()) + suffix, b.withTop (32.0f).withHeight (16.0f), Justification::centredLeft);
}

//==============================================================================
MorePanel::MorePanel (SwarmProcessor& p)
    : echoTime (p.apvts, "echotime", "echo", nullptr, 40), split (p.apvts, "split", "dry lows", nullptr, 58)
{
    const std::tuple<const char*, const char*, const char*> list[] = {
        { "shape", "SHAPE", "Soft (round grains) or chopped (flat with short fades). The middle: what the nebula does" },
        { "reverse", "REVERSE", "How many grains play backwards (the middle: what the nebula does)" },
        { "updown", "UP / DOWN", "Pitch jumps: only down, both, or only up" },
        { "detune", "DETUNE", "Every grain a little out of tune (up to 50 cents): wide, choir-like" },
        { "width", "WIDTH", "How wide the swarm flies in stereo (0 = mono, 150 = wider than the speakers)" },
        { "reach", "REACH", "How far back in time the grains are taken from (and how often it digs up old bits)" },
        { "lifespeed", "SPEED", "LIFE SPEED: how fast LIFE wanders and breathes" },
        { "echo", "ECHO", "The tempo echo after the grains (the middle: what the nebula does)" },
        { "size", "SIZE", "How long the reverb is" },
        { "duck", "DUCK", "The swarm steps back while the sound plays and blooms in the gaps (keeps vocals clear)" } };
    for (auto& [id, cap, tip] : list)
    {
        auto sl = std::make_unique<DustSlider> (p.apvts, id, cap);
        sl->setTooltip (tip);
        sl->setDoubleClickReturnValue (true, (double) p.apvts.getParameter (id)->convertFrom0to1 (p.apvts.getParameter (id)->getDefaultValue()));
        addAndMakeVisible (*sl);
        sliders.push_back (std::move (sl));
    }
    echoTime.getCombo().setTooltip ("How far apart the echoes are (in time with Cubase)");
    split.getCombo().setTooltip ("Everything below this stays dry and solid (bass, kick)");
    addAndMakeVisible (echoTime);
    addAndMakeVisible (split);
}

void MorePanel::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colour (0xff0b0b16).withAlpha (0.86f));
    g.fillRoundedRectangle (b, 14.0f);
    g.setColour (Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (b, 14.0f, 1.0f);
    g.setFont (cat::displayFont (11.0f).withExtraKerningFactor (0.4f));
    g.setColour (Colours::white.withAlpha (0.45f));
    g.drawText ("MORE", b.reduced (16.0f, 8.0f).withHeight (14.0f), Justification::centredLeft);
    g.setFont (cat::catFont (10.5f));
    g.setColour (Colours::white.withAlpha (0.38f));
    g.drawText ("double-click a slider: back to auto", b.reduced (16.0f, 8.0f).withHeight (14.0f), Justification::centredRight);
}

void MorePanel::resized()
{
    for (size_t i = 0; i < sliders.size(); ++i) sliders[i]->setBounds (16 + (int) i * 76, 30, 64, 50);
    echoTime.setBounds (getWidth() - 186, 30, 170, 24);
    split.setBounds (getWidth() - 186, 58, 170, 24);
}

//==============================================================================
void SwarmEditor::Pill::paintButton (Graphics& g, bool over, bool)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    if (on)
    {
        g.setGradientFill (ColourGradient (Colour (0xffbfe9ff).withAlpha (0.35f), b.getCentreX(), b.getCentreY(),
                                           Colour (0xffbfe9ff).withAlpha (0.08f), b.getRight(), b.getCentreY(), true));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    }
    else
    {
        g.setColour (Colours::white.withAlpha (over ? 0.12f : 0.07f));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    }
    g.setColour (Colours::white.withAlpha (on ? 0.95f : 0.5f));
    g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);
    g.setColour (Colours::white);
    g.setFont (cat::displayFont (15.0f).withExtraKerningFactor (0.42f));
    g.drawText (getButtonText(), b, Justification::centred);
}

void SwarmEditor::Dice::paintButton (Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().withSizeKeepingCentre (24.0f, 24.0f);
    if (down) b = b.reduced (1.0f);
    g.setColour (Colours::white.withAlpha (over ? 1.0f : 0.8f));
    g.drawRoundedRectangle (b, 5.0f, 1.4f);
    const float d = 3.4f;
    for (auto p : { Point<float> (0.28f, 0.28f), Point<float> (0.72f, 0.28f), Point<float> (0.5f, 0.5f), Point<float> (0.28f, 0.72f), Point<float> (0.72f, 0.72f) })
        g.fillEllipse (Rectangle<float> (d, d).withCentre ({ b.getX() + p.x * b.getWidth(), b.getY() + p.y * b.getHeight() }));
}

SwarmEditor::SwarmEditor (SwarmProcessor& p) : processor (p)
{
    addAndMakeVisible (nebula);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (dice);
    dice.setTooltip ("A new sound (keeps MIX, the grid, the notes and FREEZE)");
    dice.onClick = [this] { processor.randomise(); };
    addAndMakeVisible (grid);
    addAndMakeVisible (notes);
    grid.getCombo().setTooltip ("PULSE plays on this grid, in time with Cubase");
    notes.getCombo().setTooltip ("Which notes the grains may jump to (from the note they came from)");

    const std::pair<const char*, const char*> list[] = { { "grain", "GRAIN" }, { "density", "DENSITY" }, { "pitch", "PITCH" }, { "scatter", "SCATTER" },
                                                         { "life", "LIFE" }, { "space", "SPACE" }, { "tone", "TONE" }, { "dirt", "DIRT" }, { "mix", "MIX" } };
    const char* tips[] = { "How long each grain is (15 ms crackle ... 0.6 s smear)",
                           "How many grains at once",
                           "How often grains jump to other notes (octaves, fifths, the NOTES you picked)",
                           "How far apart in time and space the grains are taken and thrown",
                           "How much it lives on its own: it wanders round your point, breathes with the sound, digs up bits from seconds ago",
                           "Echo and a big room after the grains",
                           "Darker / brighter",
                           "Warm dirt on the grains",
                           "How much swarm you hear (the lows under 120 Hz always stay dry and solid)" };
    int i = 0;
    for (auto& [id, cap] : list)
    {
        auto s = std::make_unique<DustSlider> (processor.apvts, id, cap);
        s->setTooltip (tips[i++]);
        s->setDoubleClickReturnValue (true, (double) processor.apvts.getParameter (id)->convertFrom0to1 (processor.apvts.getParameter (id)->getDefaultValue()));
        addAndMakeVisible (*s);
        sliders.push_back (std::move (s));
    }
    freeze.setClickingTogglesState (true);
    freeze.setTooltip ("Hold the last seconds and let the swarm play them forever");
    addAndMakeVisible (freeze);
    more.setTooltip ("More controls: shape, reverse, up / down, detune, width, reach, life speed, echo, reverb size, duck, dry lows");
    more.setColour (TextButton::buttonColourId, Colours::transparentBlack);
    more.setColour (TextButton::buttonOnColourId, Colours::white.withAlpha (0.12f));
    more.setColour (TextButton::textColourOffId, Colours::white.withAlpha (0.6f));
    more.setColour (TextButton::textColourOnId, Colours::white);
    more.setClickingTogglesState (true);
    more.onClick = [this] { showMore (more.getToggleState()); };
    addAndMakeVisible (more);
    addChildComponent (morePanel);
    more.setToggleState (processor.moreOpen, dontSendNotification);
    morePanel.setVisible (processor.moreOpen);
    setSize (designW, designH);
    startTimerHz (30);
}

SwarmEditor::~SwarmEditor() { stopTimer(); }

void SwarmEditor::showMore (bool on)
{
    processor.moreOpen = on;
    morePanel.setVisible (on);
    if (on) morePanel.toFront (false);
}

void SwarmEditor::paint (Graphics& g)
{
    g.fillAll (bgCol);
}

void SwarmEditor::resized()
{
    nebula.setBounds (getLocalBounds().withHeight (stripY + 8));
    presetBar.setBounds (getWidth() - 360, 30, 290, 34);
    dice.setBounds (getWidth() - 64, 30, 34, 34);
    grid.setBounds (330, 34, 112, 26);
    notes.setBounds (452, 34, 168, 26);
    for (size_t i = 0; i < sliders.size(); ++i) sliders[i]->setBounds (34 + (int) i * 82, stripY + 10, 70, 50);
    freeze.setBounds (790, stripY + 8, 170, 46);
    more.setBounds (790, stripY + 60, 170, 24);
    morePanel.setBounds (20, stripY - 100, designW - 40, 92);
}

void SwarmEditor::timerCallback()
{
    const auto now = Time::getMillisecondCounter();
    const float dt = lastTick == 0 ? 1.0f / 30.0f : jlimit (0.005f, 0.1f, (float) (now - lastTick) / 1000.0f);
    lastTick = now;
    // PITCH shows which notes it jumps to
    if (sliders.size() > 2)
    {
        const auto n = swarm::scaleNames()[roundToInt (processor.apvts.getRawParameterValue ("scale")->load())];
        const auto suf = String (" ") + String::fromUTF8 ("\xc2\xb7") + " " + n.upToFirstOccurrenceOf (" ", false, false).toLowerCase();
        if (sliders[2]->suffix != suf) { sliders[2]->suffix = suf; sliders[2]->repaint(); }
    }
    nebula.tick (dt);
}

#include "PluginEditor.h"
#include "CatTheme.h"

using namespace juce;

namespace
{
constexpr int designW = 1000, designH = 620;
const Colour ink (0xffe6f4ff), dim (0xff8a98b8), cyan (0xff5ef0ff), magenta (0xffff4fd8), violet (0xff7b6cff);
constexpr int ringCount = 7;            // the tunnel shows 7 octaves

float wrapf (float x, float span) { x = std::fmod (x, span); return x < 0.0f ? x + span : x; }

/** 0 = the middle of the tunnel, 1 = its rim (a little perspective). */
float radiusOf (float u) { return 0.05f + 0.95f * std::pow (u, 1.7f); }

Colour hue (float u) { return u < 0.5f ? violet.interpolatedWith (cyan, u * 2.0f) : cyan.interpolatedWith (magenta, (u - 0.5f) * 2.0f); }
}

//==============================================================================
VortexView::VortexView (VortexProcessor& p) : processor (p), spin (*p.apvts.getParameter ("spin"))
{
    setTooltip ("SPIN: drag up to rise faster, down to fall. Double-click: the default");
    setMouseCursor (MouseCursor::UpDownResizeCursor);
    Random r (7);
    for (int i = 0; i < 260; ++i)
        stars.push_back ({ r.nextFloat() * MathConstants<float>::twoPi, r.nextFloat(), 0.6f + 0.8f * r.nextFloat(), 0.6f + 1.6f * r.nextFloat() });
}

void VortexView::tick (float dt)
{
    // follow the engine's spiral (it repeats every octave, so take the short way round)
    float target = processor.engine.screenPos.load();
    float d = target - wrapf (shownPos, 1.0f);
    d -= std::round (d);
    shownPos += d;
    if (std::abs (shownPos) > 1000.0f) shownPos = wrapf (shownPos, (float) ringCount);

    const float rate = processor.engine.rateNow.load();
    level += (jlimit (0.0f, 1.0f, std::sqrt (processor.engine.outLevel.load()) * 1.2f) - level) * jmin (1.0f, dt * 8.0f);
    swirl += dt * (0.25f + 1.2f * std::abs (rate)) * (rate < 0.0f ? -1.0f : 1.0f);
    for (auto& s : stars)
    {
        s.u += rate / ringCount * dt * s.speed;
        s.angle += dt * (0.2f + 0.6f * (1.0f - s.u)) * s.speed * (rate < 0.0f ? -1.0f : 1.0f);
        if (s.u >= 1.0f) { s.u -= 1.0f; s.angle += 2.4f; }
        if (s.u < 0.0f)  { s.u += 1.0f; s.angle -= 2.4f; }
    }
    repaint();
}

void VortexView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    Path clip;
    clip.addRoundedRectangle (b, 18.0f);
    g.reduceClipRegion (clip);

    const auto c = b.getCentre();
    const float R = std::hypot (b.getWidth(), b.getHeight()) * 0.52f;
    ColourGradient bg (Colour (0xff121a40).interpolatedWith (Colour (0xff1b2a66), level * 0.5f), c.x, c.y, Colour (0xff04050c), c.x + R * 0.8f, c.y, true);
    bg.addColour (0.35, Colour (0xff0a0f26));
    g.setGradientFill (bg);
    g.fillRect (b);

    const float pos = wrapf (shownPos, (float) ringCount);
    auto at = [&] (float u, float angle)
    {
        const float r = R * radiusOf (u);
        // the tunnel bends a little: deeper rings sit higher
        return Point<float> (c.x + r * std::cos (angle), c.y + r * std::sin (angle) * 0.82f - (1.0f - u) * 26.0f);
    };

    // spiral arms (turning them = zooming, like the rings)
    constexpr int arms = 5;
    for (int a = 0; a < arms; ++a)
    {
        const float base = (float) a * MathConstants<float>::twoPi / arms + swirl * 0.35f;
        Point<float> last = at (0.0f, base - pos * 0.9f);
        for (int k = 1; k <= 60; ++k)
        {
            const float u = (float) k / 60.0f;
            const auto pt = at (u, base + u * 3.4f - pos * 0.9f);
            g.setColour (hue (u).withAlpha (0.05f + 0.22f * vortex::window (u, 0.8f) * (0.5f + level)));
            g.drawLine ({ last, pt }, 1.0f + 5.0f * u);
            last = pt;
        }
    }

    // the rings: every octave one, flying out (rising) or in (falling) forever
    for (int j = 0; j < ringCount * 2; ++j)
    {
        const bool half = (j & 1) != 0;
        const float u = wrapf ((float) j * 0.5f + pos, (float) ringCount) / ringCount;
        const float w = vortex::window (u, 1.0f);
        if (w < 0.01f) continue;
        const float r = R * radiusOf (u);
        Path ring;
        ring.addEllipse (c.x - r, c.y - r * 0.82f - (1.0f - u) * 26.0f, r * 2.0f, r * 1.64f);
        const auto col = hue (u);
        if (! half)
        {
            g.setColour (col.withAlpha (0.10f * w * (0.4f + level)));
            g.strokePath (ring, PathStrokeType (10.0f + 30.0f * u));
        }
        g.setColour (col.withAlpha ((half ? 0.18f : 0.55f) * w * (0.45f + 0.55f * level)));
        g.strokePath (ring, PathStrokeType (half ? 0.8f : 1.2f + 3.0f * u));
    }

    // star dust carried along
    for (auto& s : stars)
    {
        const float w = vortex::window (s.u, 1.0f);
        if (w < 0.02f) continue;
        const auto pt = at (s.u, s.angle);
        const float sz = s.size * (0.4f + 2.0f * s.u);
        g.setColour (hue (s.u).brighter (0.4f).withAlpha (0.25f + 0.7f * w * (0.4f + level)));
        g.fillEllipse (pt.x - sz * 0.5f, pt.y - sz * 0.5f, sz, sz);
    }

    // the eye in the middle
    const Point<float> eye (c.x, c.y - 26.0f);          // where the deepest ring is
    const float er = R * 0.045f;
    g.setGradientFill (ColourGradient (cyan.withAlpha (0.55f * (0.3f + level)), eye.x, eye.y, cyan.withAlpha (0.0f), eye.x + er * 4.0f, eye.y, true));
    g.fillEllipse (eye.x - er * 4.0f, eye.y - er * 4.0f, er * 8.0f, er * 8.0f);
    g.setColour (Colour (0xff020308));
    g.fillEllipse (eye.x - er, eye.y - er, er * 2.0f, er * 2.0f);
    g.setColour (ink.withAlpha (0.8f));
    g.drawEllipse (eye.x - er, eye.y - er, er * 2.0f, er * 2.0f, 1.2f);

    // what it does now
    g.setFont (cat::catFont (13.0f));
    g.setColour (ink.withAlpha (0.9f));
    g.drawText (processor.spinText(), b.withTrimmedTop (b.getHeight() - 42.0f).reduced (16.0f, 8.0f), Justification::centred);
    g.setColour (dim.withAlpha (0.55f));
    g.setFont (cat::catFont (10.5f));
    g.drawText ("drag up / down", b.reduced (16.0f, 12.0f), Justification::topRight);

    g.setColour (Colour (0xff242a48));
    g.drawRoundedRectangle (b.reduced (0.5f), 18.0f, 1.0f);
}

void VortexView::mouseDown (const MouseEvent&)
{
    dragStart = spin.getValue();
    spin.beginChangeGesture();
}

void VortexView::mouseDrag (const MouseEvent& e)
{
    spin.setValueNotifyingHost (jlimit (0.0f, 1.0f, dragStart - (float) e.getDistanceFromDragStartY() / 400.0f));
}

void VortexView::mouseUp (const MouseEvent&) { spin.endChangeGesture(); }

void VortexView::mouseDoubleClick (const MouseEvent&)
{
    spin.beginChangeGesture();
    spin.setValueNotifyingHost (spin.getDefaultValue());
    spin.endChangeGesture();
}

//==============================================================================
void LadderView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const int modeIndex = roundToInt (processor.apvts.getRawParameterValue ("mode")->load());
    const float focus = processor.apvts.getRawParameterValue ("focus")->load();
    const double pos = processor.engine.screenPos.load();
    const auto area = b.withTrimmedTop (22.0f).withTrimmedBottom (8.0f);

    g.setFont (cat::catFont (10.5f));
    g.setColour (dim);
    g.drawText (modeIndex == 1 ? "FILTERS" : "COPIES", b.withHeight (16.0f), Justification::centredLeft);

    g.setColour (Colour (0xff242a48));
    g.drawLine (area.getX() + 6.0f, area.getY(), area.getX() + 6.0f, area.getBottom(), 1.0f);

    if (modeIndex == 1)
    {
        // 10 octave bands on a 20 Hz .. 20 kHz ladder
        const float pw = vortex::filterPower (focus);
        for (int j = 0; j < vortex::filterBands; ++j)
        {
            const double t = std::log2 (vortex::bandHz (j, pos) / vortex::filterLowHz) / vortex::filterBands;
            const float w = vortex::window (t, pw);
            const float y = area.getBottom() - (float) t * area.getHeight();
            g.setColour (hue ((float) t).withAlpha (0.25f + 0.75f * w));
            g.fillRoundedRectangle (area.getX() + 10.0f, y - 2.0f, 4.0f + w * (area.getWidth() - 14.0f), 4.0f, 2.0f);
        }
        g.setColour (dim.withAlpha (0.7f));
        for (auto [hz, label] : { std::pair { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
        {
            const float y = area.getBottom() - (float) (std::log2 (hz / vortex::filterLowHz) / vortex::filterBands) * area.getHeight();
            g.drawText (label, (int) area.getRight() - 30, (int) y - 7, 30, 14, Justification::centredRight);
        }
        return;
    }

    // 4 copies on an octave ladder (-2.5 .. +1.5); the line is the sound as it came in
    const float pw = vortex::pitchPower (focus);
    auto yOf = [&] (double o) { return area.getBottom() - (float) ((o - vortex::pitchLow) / vortex::pitchSpan) * area.getHeight(); };
    g.setColour (dim.withAlpha (0.45f));
    g.drawLine (area.getX(), yOf (0.0), area.getRight(), yOf (0.0), 1.0f);
    g.drawText ("0", (int) area.getRight() - 20, (int) yOf (0.0) - 15, 20, 14, Justification::centredRight);
    for (int i = 0; i < vortex::pitchLayers; ++i)
    {
        const double o = vortex::layerOctave (i, pos);
        const double t = (o - vortex::pitchLow) / vortex::pitchSpan;
        const float w = vortex::window (t, pw);
        const float y = yOf (o);
        g.setColour (hue ((float) t).withAlpha (0.25f + 0.75f * w));
        g.fillRoundedRectangle (area.getX() + 10.0f, y - 3.0f, 4.0f + w * (area.getWidth() - 14.0f), 6.0f, 3.0f);
    }
}

//==============================================================================
VortexEditor::VortexEditor (VortexProcessor& p) : processor (p)
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (vortex);
    addAndMakeVisible (ladder);
    for (auto* s : { &spin, &focus, &feedback, &width, &tone, &gen, &mix })
    {
        s->setColours (violet, cyan);
        addAndMakeVisible (*s);
    }
    spin.getSlider().setTooltip ("SPIN: how fast it rises (right) or falls (left); in the middle it stands still");
    focus.getSlider().setTooltip ("FOCUS: fewer, clearer copies (high) or a thick cloud of them (low)");
    feedback.getSlider().setTooltip ("FEEDBACK: what comes out goes round again an 1/8 note later - cascades and waterfalls");
    width.getSlider().setTooltip ("WIDTH: spreads the spiral left and right");
    tone.getSlider().setTooltip ("TONE: darker to the left, brighter to the right");
    gen.getSlider().setTooltip ("GEN: an endless Shepard tone of its own - spins even when nothing comes in");
    mix.getSlider().setTooltip ("MIX: how much of the spiral you hear");
    addAndMakeVisible (mode);
    addAndMakeVisible (sync);
    mode.getCombo().setTooltip ("PITCH: copies of the sound glide up an octave apart. FILTER: filters sweep through it. BOTH: one after the other");
    sync.getCombo().setTooltip ("Free: SPIN sets the speed. Or one octave every 1 .. 16 bars, locked to Cubase (SPIN left = falling)");
    setSize (designW, designH);
    startTimerHz (40);
}

VortexEditor::~VortexEditor() { stopTimer(); }

void VortexEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    ColourGradient bg (Colour (0xff0c1030), b.getWidth() * 0.5f, b.getHeight() * 0.38f, Colour (0xff03040a), b.getWidth() * 0.5f + b.getWidth() * 0.62f, b.getHeight() * 0.38f, true);
    g.setGradientFill (bg);
    g.fillAll();

    g.setColour (ink);
    g.setFont (cat::displayFont (26.0f));
    g.drawText ("VORTEX", 40, 26, 300, 40, Justification::centredLeft);
    g.setColour (dim);
    g.setFont (cat::catFont (11.0f));
    g.drawText ("endless rise and fall", 42, 62, 300, 16, Justification::centredLeft);

    g.setColour (dim.withAlpha (0.7f));
    const float ag = processor.engine.autoGainDb.load();
    g.drawText ("auto gain " + String (ag >= 0.0f ? "+" : "") + String (ag, 1) + " dB", getWidth() - 220, 586, 180, 18, Justification::centredRight);
}

void VortexEditor::resized()
{
    presetBar.setBounds (getWidth() - 360, 30, 320, 34);
    vortex.setBounds (180, 84, 640, 420);
    ladder.setBounds (60, 110, 96, 370);
    const int y = 528, w = 108, gap = 20;
    int x = (getWidth() - (7 * w + 6 * gap)) / 2;
    for (auto* s : { &spin, &focus, &feedback, &width, &tone, &gen, &mix }) { s->setBounds (x, y, w, 40); x += w + gap; }
    mode.setBounds (40, 580, 150, 26);
    sync.setBounds (204, 580, 150, 26);
}

void VortexEditor::timerCallback()
{
    const auto now = Time::getMillisecondCounter();
    const float dt = lastTick == 0 ? 1.0f / 40.0f : jlimit (0.005f, 0.1f, (float) (now - lastTick) / 1000.0f);
    lastTick = now;
    vortex.tick (dt);
    ladder.repaint();
    repaint (getWidth() - 230, 580, 200, 30);
}

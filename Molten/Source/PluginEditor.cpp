#include "PluginEditor.h"
#include "CatTheme.h"

using namespace juce;

namespace
{
constexpr int designW = 1000, designH = 620;
const Colour cream (0xffffe7d6), dim (0xffb9a8a0), hot1 (0xffffb15c), hot2 (0xffff3d12);
}

//==============================================================================
LavaView::LavaView (MoltenProcessor& p) : processor (p), heat (*p.apvts.getParameter ("heat"))
{
    setTooltip ("HEAT: drag up / down on the lava. Click to poke it, double-click for the middle");
    setMouseCursor (MouseCursor::UpDownResizeCursor);
}

LavaView::~LavaView() = default;

Rectangle<float> LavaView::fieldArea() const
{
    // the simulation's field, scaled to fill the view (2 screen pixels per field pixel at the design size)
    return getLocalBounds().toFloat();
}

void LavaView::resized()
{
    sim.setSize (jmax (60, getWidth() / 2), jmax (60, getHeight() / 2));
}

void LavaView::tick (float dt)
{
    LavaSim::Input in;
    in.heat = heat.convertFrom0to1 (heat.getValue());
    in.drip = processor.apvts.getRawParameterValue ("drip")->load();
    in.wobble = processor.apvts.getRawParameterValue ("wobble")->load();
    in.level = jlimit (0.0f, 1.0f, std::sqrt (processor.engine.inLevel.load()) * 1.1f);
    in.dripCount = processor.engine.drips.load();
    in.dt = dt;
    sim.step (in);
    sim.render();
    repaint();
}

void LavaView::paint (Graphics& g)
{
    const auto area = fieldArea();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    // the glow behind, bigger and soft
    g.setOpacity (0.85f);
    g.drawImage (sim.glow(), area.expanded (area.getWidth() * 0.06f, area.getHeight() * 0.06f));
    g.setOpacity (1.0f);
    g.drawImage (sim.image(), area);

    // the temperature, written on the lava
    const auto c = sim.centre();
    const Point<float> at (area.getX() + c.x / (float) sim.width() * area.getWidth(), area.getY() + c.y / (float) sim.height() * area.getHeight());
    const float v = heat.getValue();
    const auto num = String (roundToInt (v * 1200.0f)) + String::fromUTF8 ("\xc2\xb0");
    const auto box = Rectangle<float> (300.0f, 80.0f).withCentre (at);
    g.setFont (cat::displayFont (72.0f));
    // a soft dark halo so it reads on glowing lava and on dark crust alike
    g.setColour (Colours::black.withAlpha (0.16f));
    for (auto o : { Point<float> (-2, 0), Point<float> (2, 0), Point<float> (0, -2), Point<float> (0, 3), Point<float> (2, 3), Point<float> (-2, 3) })
        g.drawText (num, box.translated (o.x, o.y), Justification::centred);
    g.setColour (cream);
    g.drawText (num, box, Justification::centred);

    g.setFont (cat::catFont (12.0f).withExtraKerningFactor (0.35f));
    g.setColour (dim.withAlpha (0.75f));
    g.drawText (String::fromUTF8 ("HEAT \xc2\xb7 drag the lava up or down"), getLocalBounds().toFloat().removeFromBottom (22.0f), Justification::centred);
}

void LavaView::mouseDown (const MouseEvent& e)
{
    dragStartHeat = heat.getValue();
    dragging = false;
    const auto a = fieldArea();
    sim.poke ({ (e.position.x - a.getX()) / a.getWidth() * (float) sim.width(), (e.position.y - a.getY()) / a.getHeight() * (float) sim.height() }, 2.5f);
}

void LavaView::mouseDrag (const MouseEvent& e)
{
    if (! dragging && std::abs (e.getDistanceFromDragStartY()) > 2) { dragging = true; heat.beginChangeGesture(); }
    if (dragging)
    {
        const float sens = e.mods.isShiftDown() ? 0.0008f : 0.004f;
        heat.setValueNotifyingHost (jlimit (0.0f, 1.0f, dragStartHeat - (float) e.getDistanceFromDragStartY() * sens));
    }
}

void LavaView::mouseUp (const MouseEvent&)
{
    if (dragging) heat.endChangeGesture();
    dragging = false;
}

void LavaView::mouseDoubleClick (const MouseEvent&)
{
    heat.beginChangeGesture();
    heat.setValueNotifyingHost (heat.getDefaultValue());
    heat.endChangeGesture();
}

//==============================================================================
MoltenEditor::MoltenEditor (MoltenProcessor& p) : processor (p)
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (lava);
    for (auto* s : { &drip, &wobble, &body, &mix })
    {
        s->setColours (hot1, hot2);
        addAndMakeVisible (*s);
    }
    drip.getSlider().setTooltip ("DRIP: bits of the sound drip off and fall - echoes that get lower and darker each time");
    wobble.getSlider().setTooltip ("WOBBLE: the pitch sags and wobbles like warm tape (more when it's loud and hot)");
    body.getSlider().setTooltip ("BODY: low end and weight (the bass stays solid and in the middle)");
    mix.getSlider().setTooltip ("MIX: how much of the melted sound you hear");
    addAndMakeVisible (dripTime);
    addAndMakeVisible (monoBass);
    dripTime.getCombo().setTooltip ("How far apart the drips fall (in time with Cubase)");
    monoBass.setTooltip ("Keeps everything under ~120 Hz in the middle (solid 808s and kicks)");
    setSize (designW, designH);
    startTimerHz (30);
}

MoltenEditor::~MoltenEditor() { stopTimer(); }

void MoltenEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    ColourGradient bg (Colour (0xff2a0d06), b.getWidth() * 0.5f, b.getHeight() * 0.40f, Colour (0xff070303), b.getWidth() * 0.5f + b.getWidth() * 0.6f, b.getHeight() * 0.40f, true);
    bg.addColour (0.55, Colour (0xff120705));
    g.setGradientFill (bg);
    g.fillAll();

    g.setColour (cream);
    g.setFont (cat::displayFont (28.0f));
    g.drawText ("MOLTEN", 40, 28, 300, 40, Justification::centredLeft);

    // a quiet meter of what auto gain is doing
    g.setFont (cat::catFont (11.0f));
    g.setColour (dim.withAlpha (0.7f));
    const float ag = processor.engine.autoGainDb.load();
    g.drawText ("auto gain " + String (ag >= 0.0f ? "+" : "") + String (ag, 1) + " dB", getWidth() - 220, 586, 180, 18, Justification::centredRight);
}

void MoltenEditor::resized()
{
    presetBar.setBounds (getWidth() - 340, 30, 300, 34);
    lava.setBounds (180, 64, 640, 440);
    const int y = 532, w = 150, gap = 80;
    const int x0 = (getWidth() - (4 * w + 3 * gap)) / 2;
    int x = x0;
    for (auto* s : { &drip, &wobble, &body, &mix }) { s->setBounds (x, y, w, 40); x += w + gap; }
    dripTime.setBounds (40, 580, 150, 26);
    monoBass.setBounds (204, 580, 110, 26);
}

void MoltenEditor::timerCallback()
{
    const auto now = Time::getMillisecondCounter();
    const float dt = lastTick == 0 ? 1.0f / 30.0f : jlimit (0.005f, 0.1f, (float) (now - lastTick) / 1000.0f);
    lastTick = now;
    lava.tick (dt);
    repaint (getWidth() - 230, 580, 200, 30);
}

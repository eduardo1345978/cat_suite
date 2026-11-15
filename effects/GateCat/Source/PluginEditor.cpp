#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr int designW = 1000, designH = 600;
// a path of torii gates at night: ink black, warm paper text, one vermilion
const Colour ink (0xff121116), panel (0xff17151c), side (0xff0f0e12), top (0xff0c0b0f), line (0xff26232c), edge (0xff2f2b36),
             paper (0xffece6dc), mute (0xff7d7688), soft (0xff9a93a6), vermilion (0xffc93d27), glow (0xffff6a4a), unlit (0xff2c2933);

Font mono (float h) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::plain)); }
Font label (float h, bool bold = false) { return cat::catFont (h, bold); }

/** flat buttons: dark, a thin edge, vermilion when on */
struct FlatLaf : LookAndFeel_V4
{
    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? vermilion.withAlpha (0.18f) : (down ? edge : over ? Colour (0xff1d1b22) : panel));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? vermilion : edge);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }
    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        g.setColour (b.getToggleState() ? paper : soft);
        g.setFont (label (12.0f, b.getToggleState()));
        g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
    }
};
FlatLaf& flat() { static FlatLaf l; return l; }
}

//==============================================================================
ToriiPath::ToriiPath (GateCatProcessor& p) : processor (p)
{
    for (int k = 0; k < gate::maxSteps; ++k) params[(size_t) k] = processor.apvts.getParameter (GateCatProcessor::stepId (k));
    setTooltip ("Drag a gate up or down to set how loud that step is. Double-click: on / off. Shift-drag: draw a straight line");
}

Rectangle<float> ToriiPath::area() const { return getLocalBounds().toFloat().reduced (16.0f, 0.0f).withTrimmedTop (18.0f).withTrimmedBottom (26.0f); }

int ToriiPath::stepAtX (float x) const
{
    const int n = roundToInt (processor.apvts.getRawParameterValue ("steps")->load());
    const auto a = area();
    return jlimit (0, n - 1, (int) ((x - a.getX()) / a.getWidth() * (float) n));
}

float ToriiPath::levelAtY (float y) const
{
    const auto a = area();
    return jlimit (0.0f, 1.0f, (a.getBottom() - y) / a.getHeight());
}

void ToriiPath::setStep (int k, float v)
{
    auto* prm = params[(size_t) k];
    if (! grabbed[(size_t) k]) { prm->beginChangeGesture(); grabbed[(size_t) k] = true; }
    prm->setValueNotifyingHost (jlimit (0.0f, 1.0f, v));
}

void ToriiPath::mouseDown (const MouseEvent& e)
{
    last = lineStart = e.position;
    lineMode = e.mods.isShiftDown();
    if (! lineMode) setStep (stepAtX (e.position.x), levelAtY (e.position.y));
}

void ToriiPath::mouseDrag (const MouseEvent& e)
{
    // a line from where the drag started (shift) or from the last point (so a fast drag skips nothing)
    const auto from = lineMode ? lineStart : last;
    const int a = stepAtX (from.x), b = stepAtX (e.position.x);
    for (int k = jmin (a, b); k <= jmax (a, b); ++k)
    {
        const float t = a == b ? 1.0f : (float) (k - a) / (float) (b - a);
        setStep (k, levelAtY (from.y + (e.position.y - from.y) * t));
    }
    last = e.position;
}

void ToriiPath::mouseUp (const MouseEvent&)
{
    for (int k = 0; k < gate::maxSteps; ++k)
        if (grabbed[(size_t) k]) { params[(size_t) k]->endChangeGesture(); grabbed[(size_t) k] = false; }
}

void ToriiPath::mouseDoubleClick (const MouseEvent& e)
{
    const int k = stepAtX (e.position.x);
    setStep (k, params[(size_t) k]->getValue() > 0.5f ? 0.0f : 1.0f);
    mouseUp (e);
}

void ToriiPath::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (b, 8.0f);
    // a faint warm glow at the ground
    g.setGradientFill (ColourGradient (vermilion.withAlpha (0.0f), 0.0f, b.getHeight() * 0.7f, vermilion.withAlpha (0.07f), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (line);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    const auto a = area();
    const int n = roundToInt (processor.apvts.getRawParameterValue ("steps")->load());
    const int now = processor.engine.stepNow.load();
    const float w = a.getWidth() / (float) n;
    for (int k = 0; k < n; ++k)
    {
        const float v = params[(size_t) k]->getValue();
        const bool on = v > 0.01f, lit = k == now && on;
        const Colour c = lit ? glow : on ? vermilion : unlit;
        const float cx = a.getX() + w * ((float) k + 0.5f);
        const float gw = jmin (w * 0.86f, 44.0f);
        const float h = on ? 26.0f + v * (a.getHeight() - 26.0f) : 22.0f;
        const float topY = a.getBottom() - h;
        if (lit)
        {
            g.setColour (glow.withAlpha (0.18f));
            g.fillEllipse (cx - gw * 0.9f, topY - 14.0f, gw * 1.8f, 30.0f);
        }
        g.setColour (c);
        // kasagi: the top beam, a little curved up at the ends
        Path beam;
        beam.startNewSubPath (cx - gw * 0.5f, topY + 1.0f);
        beam.quadraticTo (cx, topY + 4.0f, cx + gw * 0.5f, topY + 1.0f);
        beam.lineTo (cx + gw * 0.46f, topY + 7.0f);
        beam.quadraticTo (cx, topY + 9.0f, cx - gw * 0.46f, topY + 7.0f);
        beam.closeSubPath();
        g.fillPath (beam);
        // nuki: the second beam, and the two posts
        g.fillRect (cx - gw * 0.38f, topY + 13.0f, gw * 0.76f, 3.5f);
        const float postW = jmax (3.0f, gw * 0.11f);
        g.fillRect (cx - gw * 0.30f, topY + 8.0f, postW, h - 8.0f);
        g.fillRect (cx + gw * 0.30f - postW, topY + 8.0f, postW, h - 8.0f);
        g.setColour (lit ? glow : line);
        g.fillRect (cx - gw * 0.4f, a.getBottom(), gw * 0.8f, 2.5f);
        g.setColour (lit ? glow : Colour (0xff5d5768));
        g.setFont (mono (10.0f));
        g.drawText (String (k + 1).paddedLeft ('0', 2), Rectangle<float> (cx - 20.0f, a.getBottom() + 6.0f, 40.0f, 14.0f), Justification::centred);
    }
}

//==============================================================================
ToriiKnob::ToriiKnob (AudioProcessorValueTreeState& state, const String& id, const String& n) : param (state.getParameter (id)), name (n)
{
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (180);
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
    slider.onValueChange = [this] { repaint(); };
}

void ToriiKnob::resized() { slider.setBounds (getLocalBounds().removeFromTop (60).withSizeKeepingCentre (60, 60)); }

void ToriiKnob::paint (Graphics& g)
{
    const auto r = getLocalBounds().removeFromTop (60).toFloat().withSizeKeepingCentre (54.0f, 54.0f);
    const float v = param != nullptr ? param->getValue() : 0.0f;
    const float a0 = MathConstants<float>::pi * 1.25f, a1 = MathConstants<float>::pi * 2.75f, av = a0 + v * (a1 - a0);
    const auto c = r.getCentre();
    const float rad = r.getWidth() * 0.5f - 2.0f;
    Path track, val;
    track.addCentredArc (c.x, c.y, rad, rad, 0.0f, a0, a1, true);
    val.addCentredArc (c.x, c.y, rad, rad, 0.0f, a0, av, true);
    g.setColour (Colour (0xff2a2731));
    g.strokePath (track, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (vermilion);
    g.strokePath (val, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
    const auto body = r.reduced (8.0f);
    g.setColour (Colour (0xff1b1920));
    g.fillEllipse (body);
    g.setColour (Colour (0xff34303c));
    g.drawEllipse (body, 1.0f);
    const Point<float> tip (c.x + std::sin (av) * (body.getWidth() * 0.5f - 4.0f), c.y - std::cos (av) * (body.getWidth() * 0.5f - 4.0f));
    const Point<float> base (c.x + std::sin (av) * 6.0f, c.y - std::cos (av) * 6.0f);
    g.setColour (paper);
    g.drawLine (Line<float> (base, tip), 2.0f);

    auto t = getLocalBounds().withTrimmedTop (62);
    g.setColour (mute);
    g.setFont (label (10.5f, true));
    g.drawText (name.toUpperCase(), t.removeFromTop (14), Justification::centred);
    g.setColour (paper);
    g.setFont (mono (11.0f));
    g.drawText (param != nullptr ? param->getCurrentValueAsText() : String(), t.removeFromTop (16), Justification::centred);
}

//==============================================================================
GateCatEditor::GateCatEditor (GateCatProcessor& p) : processor (p)
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (path);

    struct K { const char* id; const char* name; const char* tip; };
    const K ks[] = {
        { "swing", "Swing", "SWING: every second step comes later - a groove instead of a machine" },
        { "length", "Length", "LENGTH: how much of each step stays open (short = choppy, long = smooth)" },
        { "attack", "Attack", "ATTACK: how fast a step opens (slow = soft swells)" },
        { "release", "Release", "RELEASE: how fast a step closes (slow = it breathes)" },
        { "depth", "Depth", "DEPTH: how deep the holes are (0 = nothing happens, 100% = silence between steps)" },
        { "filter", "Filter", "FILTER: quiet steps also get darker - a filter opening in the rhythm" },
        { "mix", "Mix", "MIX: the chopped sound against the original" },
        { "output", "Output", "OUTPUT: the level that comes out" },
    };
    for (auto& k : ks)
    {
        knobs.push_back (std::make_unique<ToriiKnob> (processor.apvts, k.id, k.name));
        knobs.back()->getSlider().setTooltip (k.tip);
        addAndMakeVisible (*knobs.back());
    }

    clearB.onClick = [this] { setAll ([] (int) { return 0.0f; }); };
    fillB.onClick = [this] { setAll ([] (int) { return 1.0f; }); };
    randomB.onClick = [this]
    {
        auto& rnd = Random::getSystemRandom();
        setAll ([&rnd] (int k) { return k % 4 == 0 || rnd.nextFloat() < 0.45f ? 0.4f + 0.6f * rnd.nextFloat() : 0.0f; });
    };
    shiftB.onClick = [this]
    {
        const int n = roundToInt (processor.apvts.getRawParameterValue ("steps")->load());
        std::array<float, gate::maxSteps> v {};
        for (int k = 0; k < gate::maxSteps; ++k) v[(size_t) k] = processor.apvts.getRawParameterValue (GateCatProcessor::stepId (k))->load();
        setAll ([v, n] (int k) { return k < n ? v[(size_t) ((k - 1 + n) % n)] : v[(size_t) k]; });
    };
    clearB.setTooltip ("All gates closed");
    fillB.setTooltip ("All gates fully open");
    randomB.setTooltip ("A new random rhythm (always on the beats)");
    shiftB.setTooltip ("Move the whole pattern one step to the right");
    aB.setTooltip ("A / B: two settings to compare. Change B, click A to hear the first again");
    bB.setTooltip (aB.getTooltip());
    aB.onClick = [this] { processor.switchAB (0); };
    bB.onClick = [this] { processor.switchAB (1); };

    const auto rates = gate::rateNames();
    for (int i = 0; i < 6; ++i)
    {
        rateB[(size_t) i].setButtonText (rates[i]);
        rateB[(size_t) i].setTooltip ("How long one step is: 1/16 = four steps per beat. T = triplets");
        rateB[(size_t) i].onClick = [this, i] { setParam ("rate", (float) i); };
    }
    stepsDown.onClick = [this] { setParam ("steps", processor.apvts.getRawParameterValue ("steps")->load() - 1.0f); };
    stepsUp.onClick = [this] { setParam ("steps", processor.apvts.getRawParameterValue ("steps")->load() + 1.0f); };
    stepsDown.setTooltip ("A shorter pattern (12 for triplets)");
    stepsUp.setTooltip ("A longer pattern (16 = one bar of 1/16)");

    for (auto* b : { &clearB, &fillB, &randomB, &shiftB, &aB, &bB, &stepsDown, &stepsUp }) { b->setLookAndFeel (&flat()); addAndMakeVisible (*b); }
    for (auto& b : rateB) { b.setLookAndFeel (&flat()); addAndMakeVisible (b); }

    setSize (designW, designH);
    startTimerHz (30);
}

GateCatEditor::~GateCatEditor()
{
    stopTimer();
    for (auto* b : { &clearB, &fillB, &randomB, &shiftB, &aB, &bB, &stepsDown, &stepsUp }) b->setLookAndFeel (nullptr);
    for (auto& b : rateB) b.setLookAndFeel (nullptr);
}

void GateCatEditor::setParam (const String& id, float value)
{
    if (auto* prm = processor.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (value)));
        prm->endChangeGesture();
    }
}

void GateCatEditor::setAll (std::function<float (int)> f)
{
    for (int k = 0; k < gate::maxSteps; ++k)
        if (auto* prm = processor.apvts.getParameter (GateCatProcessor::stepId (k)))
        { prm->beginChangeGesture(); prm->setValueNotifyingHost (jlimit (0.0f, 1.0f, f (k))); prm->endChangeGesture(); }
}

void GateCatEditor::paint (Graphics& g)
{
    g.fillAll (ink);
    // the top bar
    g.setColour (top);
    g.fillRect (0, 0, getWidth(), 56);
    g.setColour (line);
    g.fillRect (0, 56, getWidth(), 1);
    g.setColour (paper);
    g.setFont (cat::displayFont (24.0f));
    g.drawText ("GATE-CAT", 24, 0, 160, 56, Justification::centredLeft);
    Path diamond;
    diamond.addRectangle (-4.0f, -4.0f, 8.0f, 8.0f);
    diamond.applyTransform (AffineTransform::rotation (MathConstants<float>::pi / 4.0f).translated (146.0f, 29.0f));
    g.setColour (vermilion);
    g.fillPath (diamond);

    g.setColour (mute);
    g.setFont (label (11.0f, true));
    g.drawText ("PATTERN", 24, 70, 120, 26, Justification::centredLeft);
    g.setFont (label (11.0f));
    g.drawText ("drag a gate up / down     double-click: on / off     shift-drag: a straight line", 24, knobArea.getY() - 26, 600, 18, Justification::centredLeft);

    // the side panel: timing and output
    g.setColour (side);
    g.fillRect (sideArea);
    g.setColour (line);
    g.fillRect (sideArea.getX(), sideArea.getY(), 1, sideArea.getHeight());
    g.setColour (mute);
    g.setFont (label (11.0f, true));
    g.drawText ("TIMING", sideArea.getX() + 18, sideArea.getY() + 14, 150, 16, Justification::centredLeft);
    g.drawText ("OUTPUT", sideArea.getX() + 18, meterArea.getY() - 26, 150, 16, Justification::centredLeft);
    g.setColour (soft);
    g.setFont (label (12.0f));
    g.drawText ("Steps", sideArea.getX() + 18, stepsDown.getY(), 60, stepsDown.getHeight(), Justification::centredLeft);
    g.setColour (paper);
    g.setFont (mono (13.0f));
    g.drawText (String (roundToInt (processor.apvts.getRawParameterValue ("steps")->load())), stepsDown.getRight(), stepsDown.getY(), stepsUp.getX() - stepsDown.getRight(), stepsDown.getHeight(), Justification::centred);

    // the meters (L, R) and the gate
    auto m = meterArea;
    auto bars = m.removeFromLeft (34);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = bars.removeFromLeft (12).toFloat();
        bars.removeFromLeft (4);
        g.setColour (Colour (0xff1d1b22));
        g.fillRoundedRectangle (bar, 2.0f);
        const float db = Decibels::gainToDecibels (ch == 0 ? meterL : meterR, -60.0f);
        const float frac = jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = bar.withTop (bar.getBottom() - bar.getHeight() * frac);
        g.setGradientFill (ColourGradient (Colour (0xff3fae7a), 0.0f, bar.getBottom(), Colour (0xffe3482f), 0.0f, bar.getY(), false));
        g.fillRoundedRectangle (fill, 2.0f);
    }
    m.removeFromLeft (10);
    g.setFont (mono (11.0f));
    g.setColour (soft);
    const float db = Decibels::gainToDecibels (jmax (meterL, meterR), -60.0f);
    g.drawText (db <= -59.9f ? String ("-inf dB") : String (db, 1) + " dB", m.removeFromTop (18), Justification::centredRight);
    g.drawText ("gate " + String (roundToInt (gateShown * 100.0f)) + "%", m.removeFromTop (18), Justification::centredRight);

    // the knob row
    g.setColour (side);
    g.fillRect (knobArea);
    g.setColour (line);
    g.fillRect (knobArea.getX(), knobArea.getY(), knobArea.getWidth(), 1);
}

void GateCatEditor::resized()
{
    presetBar.setBounds ((getWidth() - 320) / 2, 11, 320, 34);
    aB.setBounds (getWidth() - 24 - 78, 12, 36, 32);
    bB.setBounds (aB.getRight() + 6, 12, 36, 32);

    knobArea = { 0, getHeight() - 116, getWidth(), 116 };
    sideArea = { getWidth() - 210, 57, 210, knobArea.getY() - 57 };

    auto tools = Rectangle<int> (24, 70, sideArea.getX() - 48, 26);
    for (auto* b : { &shiftB, &randomB, &fillB, &clearB }) { b->setBounds (tools.removeFromRight (64)); tools.removeFromRight (6); }
    path.setBounds (24, 104, sideArea.getX() - 48, knobArea.getY() - 104 - 34);

    auto s = sideArea.reduced (18, 0).withTrimmedTop (40);
    auto r1 = s.removeFromTop (28), r2 = (s.removeFromTop (4), s.removeFromTop (28));
    for (int i = 0; i < 6; ++i)
    {
        auto& row = i < 3 ? r1 : r2;
        rateB[(size_t) i].setBounds (row.removeFromLeft ((sideArea.getWidth() - 36 - 8) / 3));
        row.removeFromLeft (4);
    }
    s.removeFromTop (16);
    auto stepsRow = s.removeFromTop (28);
    stepsUp.setBounds (stepsRow.removeFromRight (28));
    stepsRow.removeFromRight (40);
    stepsDown.setBounds (stepsRow.removeFromRight (28));
    s.removeFromTop (44);
    meterArea = s.removeFromTop (jmin (150, s.getHeight() - 10));

    const int w = getWidth() / 8;
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds ((int) i * w, knobArea.getY() + 10, w, 96);
}

void GateCatEditor::timerCallback()
{
    // meters fall back slowly, jump up at once
    const float l = processor.engine.peakL.load(), r = processor.engine.peakR.load();
    meterL = jmax (l, meterL * 0.85f);
    meterR = jmax (r, meterR * 0.85f);
    gateShown = processor.engine.gainNow.load();
    const int rate = roundToInt (processor.apvts.getRawParameterValue ("rate")->load());
    for (int i = 0; i < 6; ++i) rateB[(size_t) i].setToggleState (i == rate, dontSendNotification);
    aB.setToggleState (processor.abSlot == 0, dontSendNotification);
    bB.setToggleState (processor.abSlot == 1, dontSendNotification);
    path.repaint();
    repaint (sideArea);
}

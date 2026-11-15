#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr int designW = 1000, designH = 600;
// a club at night: deep plum, hot pink LEDs that light up, a lime playhead
const Colour bgTop (0xff1d0f2a), bgBottom (0xff08040e), ink (0xfff6ecff), dim (0xff9d86b3),
             pink (0xffff3d9a), lime (0xffb8ff3d), off (0xff2e1d40);
}

//==============================================================================
StepGrid::StepGrid (GateCatProcessor& p) : processor (p)
{
    for (int k = 0; k < gate::maxSteps; ++k) params[(size_t) k] = processor.apvts.getParameter (GateCatProcessor::stepId (k));
    setTooltip ("Drag up and down over the steps to draw how loud each one is. Double-click a step: fully on / off");
}

int StepGrid::stepAtX (float x) const
{
    const int n = roundToInt (processor.apvts.getRawParameterValue ("steps")->load());
    return jlimit (0, n - 1, (int) (x / (float) getWidth() * (float) n));
}

void StepGrid::setStep (int k, float v)
{
    auto* prm = params[(size_t) k];
    if (! grabbed[(size_t) k]) { prm->beginChangeGesture(); grabbed[(size_t) k] = true; }
    prm->setValueNotifyingHost (jlimit (0.0f, 1.0f, v));
}

void StepGrid::mouseDown (const MouseEvent& e)
{
    last = e.position;
    setStep (stepAtX (e.position.x), 1.0f - e.position.y / (float) (getHeight() - 18));
}

void StepGrid::mouseDrag (const MouseEvent& e)
{
    // draw a line from the last point, so a fast drag doesn't skip steps
    const int a = stepAtX (last.x), b = stepAtX (e.position.x);
    for (int k = jmin (a, b); k <= jmax (a, b); ++k)
    {
        const float t = a == b ? 1.0f : (float) (k - a) / (float) (b - a);
        const float y = last.y + (e.position.y - last.y) * t;
        setStep (k, 1.0f - y / (float) (getHeight() - 18));
    }
    last = e.position;
}

void StepGrid::mouseUp (const MouseEvent&)
{
    for (int k = 0; k < gate::maxSteps; ++k)
        if (grabbed[(size_t) k]) { params[(size_t) k]->endChangeGesture(); grabbed[(size_t) k] = false; }
}

void StepGrid::mouseDoubleClick (const MouseEvent& e)
{
    const int k = stepAtX (e.position.x);
    setStep (k, params[(size_t) k]->getValue() > 0.5f ? 0.0f : 1.0f);
    mouseUp (e);
}

void StepGrid::paint (Graphics& g)
{
    const int n = roundToInt (processor.apvts.getRawParameterValue ("steps")->load());
    const int now = processor.engine.stepNow.load();
    const float w = (float) getWidth() / (float) n, h = (float) getHeight() - 18.0f;
    for (int k = 0; k < n; ++k)
    {
        auto cell = Rectangle<float> ((float) k * w, 0.0f, w, h).reduced (jmin (5.0f, w * 0.12f), 0.0f);
        g.setColour (off.withAlpha (k % 4 == 0 ? 1.0f : 0.7f));
        g.fillRoundedRectangle (cell, 5.0f);
        const float v = params[(size_t) k]->getValue();
        const bool lit = k == now;
        // the level: a stack of LED segments
        const int segs = 12;
        const float segH = h / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            if ((float) (i + 1) / (float) segs > v + 0.04f) break;
            auto seg = Rectangle<float> (cell.getX() + 4.0f, h - (float) (i + 1) * segH + 2.0f, cell.getWidth() - 8.0f, segH - 4.0f);
            g.setColour ((lit ? lime : pink).withAlpha (0.55f + 0.45f * (float) i / (float) segs));
            g.fillRoundedRectangle (seg, 2.0f);
        }
        if (lit)
        {
            g.setColour (lime.withAlpha (0.8f));
            g.drawRoundedRectangle (cell, 5.0f, 2.0f);
        }
        g.setColour (dim);
        g.setFont (cat::catFont (11.0f));
        g.drawText (String (k + 1), cell.withTop (h + 4.0f).withHeight (14.0f), Justification::centred);
    }
}

//==============================================================================
GateCatEditor::GateCatEditor (GateCatProcessor& p) : processor (p)
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (grid);
    for (auto* s : { &swing, &length, &attack, &release, &depth, &filter, &mix, &steps })
    {
        s->setColours (pink, lime);
        addAndMakeVisible (*s);
    }
    swing.getSlider().setTooltip ("SWING: every second step comes later - a groove instead of a machine");
    length.getSlider().setTooltip ("LENGTH: how much of each step stays open (short = choppy, long = smooth)");
    attack.getSlider().setTooltip ("ATTACK: how fast a step opens (slow = soft swells)");
    release.getSlider().setTooltip ("RELEASE: how fast a step closes (slow = it breathes)");
    depth.getSlider().setTooltip ("DEPTH: how deep the holes are (0 = nothing happens, 100% = silence between steps)");
    filter.getSlider().setTooltip ("FILTER: quiet steps also get darker - like a filter opening in the rhythm");
    mix.getSlider().setTooltip ("MIX: the chopped sound against the original");
    steps.getSlider().setTooltip ("STEPS: how long the pattern is (12 for triplets, 16 for one bar of 1/16)");
    addAndMakeVisible (rate);
    rate.getCombo().setTooltip ("How long one step is: 1/16 = four steps per beat. T = triplets");

    auto setAll = [this] (std::function<float (int)> f)
    {
        for (int k = 0; k < gate::maxSteps; ++k)
            if (auto* prm = processor.apvts.getParameter (GateCatProcessor::stepId (k)))
            { prm->beginChangeGesture(); prm->setValueNotifyingHost (jlimit (0.0f, 1.0f, f (k))); prm->endChangeGesture(); }
    };
    clearB.onClick = [setAll] { setAll ([] (int) { return 0.0f; }); };
    fillB.onClick = [setAll] { setAll ([] (int) { return 1.0f; }); };
    randomB.onClick = [setAll]
    {
        auto& rnd = Random::getSystemRandom();
        setAll ([&rnd] (int k) { return k % 4 == 0 || rnd.nextFloat() < 0.45f ? 0.4f + 0.6f * rnd.nextFloat() : 0.0f; });
    };
    shiftB.onClick = [this, setAll]
    {
        const int n = roundToInt (processor.apvts.getRawParameterValue ("steps")->load());
        std::array<float, gate::maxSteps> v {};
        for (int k = 0; k < gate::maxSteps; ++k) v[(size_t) k] = processor.apvts.getRawParameterValue (GateCatProcessor::stepId (k))->load();
        setAll ([v, n] (int k) { return k < n ? v[(size_t) ((k - 1 + n) % n)] : v[(size_t) k]; });
    };
    clearB.setTooltip ("All steps off");
    fillB.setTooltip ("All steps fully on");
    randomB.setTooltip ("A new random rhythm (always on the beats)");
    shiftB.setTooltip ("Move the whole pattern one step to the right");
    for (auto* b : { &clearB, &fillB, &randomB, &shiftB }) addAndMakeVisible (*b);

    setSize (designW, designH);
    startTimerHz (30);
}

GateCatEditor::~GateCatEditor() { stopTimer(); }

void GateCatEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, b.getHeight(), false));
    g.fillAll();

    g.setColour (pink);
    g.setFont (cat::displayFont (28.0f));
    g.drawText ("GATE-CAT", 40, 24, 320, 40, Justification::centredLeft);
    g.setColour (dim);
    g.setFont (cat::catFont (11.0f));
    g.drawText ("chops any sound into a rhythm", 42, 62, 320, 16, Justification::centredLeft);

    // the gate light: how open it is right now
    const auto lamp = Rectangle<float> (360.0f, 34.0f, 22.0f, 22.0f);
    g.setColour (lime.withAlpha (0.15f + 0.85f * shownGain));
    g.fillEllipse (lamp);
    g.setColour (lime.withAlpha (0.4f));
    g.drawEllipse (lamp, 1.5f);
    g.setColour (dim);
    g.drawText ("GATE", 390, 36, 60, 18, Justification::centredLeft);
    g.drawText ("drag over the steps to draw the rhythm  -  double-click a step: on / off", 40, 566, 600, 16, Justification::centredLeft);
}

void GateCatEditor::resized()
{
    presetBar.setBounds (getWidth() - 360, 30, 320, 34);
    grid.setBounds (40, 100, getWidth() - 80, 318);
    int x = 40;
    for (auto* bt : { &clearB, &fillB, &randomB, &shiftB }) { bt->setBounds (x, 438, 90, 28); x += 100; }
    rate.setBounds (getWidth() - 220, 438, 180, 28);
    const int w = 100, gap = 16, y = 500;
    x = (getWidth() - (8 * w + 7 * gap)) / 2;
    for (auto* s : { &steps, &swing, &length, &attack, &release, &depth, &filter, &mix }) { s->setBounds (x, y, w, 40); x += w + gap; }
}

void GateCatEditor::timerCallback()
{
    shownGain = processor.engine.gainNow.load();
    grid.repaint();
    repaint (355, 30, 32, 32);
}

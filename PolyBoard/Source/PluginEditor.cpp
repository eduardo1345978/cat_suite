#include "PluginEditor.h"
#include "CatTheme.h"

using namespace juce;
using namespace board;

String knobCaption (int pedal, int knob);    // PluginProcessor.cpp

namespace
{
constexpr int designW = 1180, designH = 640;
const Colour bg (0xff1a1612), cream (0xfffff8e8), dim (0xff8a8072), ink (0xff100d0a);
const Colour pedalColours[numPedals] = { Colour (0xffc0392b), Colour (0xff16a085), Colour (0xffd35400), Colour (0xff6c3483),
                                         Colour (0xff2471a3), Colour (0xff7e5109), Colour (0xff1c2833) };
const char* pedalTips[numPedals] = {
    "DRIVE: the sound is pushed into a soft clipper - the round wave gets flat tops = warm overtones (GAIN = how hard, TONE = how bright)",
    "COMPRESSOR: turns loud notes down, so everything sounds even and longer (SQUASH = how much, LEVEL = louder after it)",
    "FILTER: an auto-wah - a filter that opens and closes in time with the song (SWEEP = how far, SPEED = how fast)",
    "PHASER: phase-shifted copies cancel a few frequencies that sweep up and down - the 70s swoosh (RATE, DEPTH)",
    "CHORUS: two copies, a tiny bit late and wobbling - wider and thicker, like many synths at once (RATE, MIX)",
    "DELAY: echoes in time with the song, left - right, each one darker (TIME = the note value, REPEATS)",
    "REVERB: a room around the sound (SIZE = small room .. huge hall, MIX)" };
constexpr int pedalW = 138, pedalH = 360, gap = 18;

Font script (float h) { return cat::displayFont (h); }
Font small (float h) { return Font (FontOptions (h, Font::bold)); }
String noteName (float hz, float& cents)
{
    const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    const int nearest = roundToInt (midi);
    cents = (midi - (float) nearest) * 100.0f;
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return String (names[((nearest % 12) + 12) % 12]) + String (nearest / 12 - 1);
}
}

//==============================================================================
BoardLaf::BoardLaf()
{
    setColour (TooltipWindow::backgroundColourId, Colour (0xff241f1a));
    setColour (TooltipWindow::textColourId, cream);
    setColour (TooltipWindow::outlineColourId, Colour (0xff3a332b));
    setColour (BubbleComponent::backgroundColourId, ink);
    setColour (BubbleComponent::outlineColourId, Colour (0xff3a332b));
}

void BoardLaf::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    if (s.getProperties().contains ("plain"))
    {
        // the small input / output knobs at the bottom
        Path track, value;
        track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, start, end, true);
        value.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, start, start + (end - start) * pos, true);
        g.setColour (Colour (0xff3a332b)); g.strokePath (track, PathStrokeType (3.0f));
        g.setColour (cream); g.strokePath (value, PathStrokeType (3.0f));
        return;
    }
    // a shadow, the cream knob with a gradient, a skirt and a pointer line
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - r + 1.0f, c.y - r + 3.0f, r * 2.0f, r * 2.0f);
    ColourGradient knob (cream, c.x - r * 0.4f, c.y - r * 0.5f, Colour (0xffbdb29a), c.x + r * 0.5f, c.y + r * 0.7f, true);
    g.setGradientFill (knob);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (Colour (0xff8f8570));
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
    for (int k = 0; k < 24; ++k)       // knurling
    {
        const float a = (float) k / 24.0f * MathConstants<float>::twoPi;
        g.setColour (Colour (0x22000000));
        g.drawLine (c.x + std::sin (a) * r * 0.86f, c.y - std::cos (a) * r * 0.86f, c.x + std::sin (a) * r, c.y - std::cos (a) * r, 1.0f);
    }
    const float a = start + (end - start) * pos;
    g.setColour (Colour (0xff2a2622));
    g.drawLine (c.x + std::sin (a) * r * 0.2f, c.y - std::cos (a) * r * 0.2f, c.x + std::sin (a) * r * 0.82f, c.y - std::cos (a) * r * 0.82f, 2.6f);
}

void BoardLaf::drawBubble (Graphics& g, BubbleComponent&, const Point<float>&, const Rectangle<float>& body)
{
    g.setColour (ink.withAlpha (0.92f));
    g.fillRoundedRectangle (body, 6.0f);
    g.setColour (Colour (0xff3a332b));
    g.drawRoundedRectangle (body, 6.0f, 1.0f);
}

Font BoardLaf::getSliderPopupFont (Slider&) { return small (13.0f); }

//==============================================================================
PedalView::PedalView (PolyBoardProcessor& p, int pd, std::function<void (PedalView&, const MouseEvent&, int)> drag)
    : pedal (pd), processor (p), onDrag (std::move (drag))
{
    for (auto* k : { &knobA, &knobB })
    {
        k->setSliderStyle (Slider::RotaryVerticalDrag);
        k->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        k->setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        k->setPopupDisplayEnabled (true, true, nullptr, 1200);
        k->setMouseDragSensitivity (160);
        addAndMakeVisible (*k);
    }
    attA = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, PolyBoardProcessor::paramId (pd, "a"), knobA);
    attB = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, PolyBoardProcessor::paramId (pd, "b"), knobB);
    for (auto [k, id] : { std::pair { &knobA, "a" }, { &knobB, "b" } })
        if (auto* prm = p.apvts.getParameter (PolyBoardProcessor::paramId (pd, id)))
            k->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    knobA.setTooltip (knobCaption (pd, 0) + " - drag up / down, double-click = back to normal");
    knobB.setTooltip (knobCaption (pd, 1) + " - drag up / down, double-click = back to normal");
    footswitch.setClickingTogglesState (true);
    footswitch.setTooltip ("Switch the pedal on / off (a MIDI footswitch: CC " + String (80) + ".." + String (86) + " by position)");
    attOn = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, PolyBoardProcessor::paramId (pd, "on"), footswitch);
    addAndMakeVisible (footswitch);
    footswitch.onStateChange = [this] { repaint(); };
    setTooltip (String (pedalTips[pd]) + "\nDrag the pedal sideways to change the order.");
    setMouseCursor (MouseCursor::DraggingHandCursor);
}

void PedalView::resized()
{
    knobA.setBounds (20, 18, 44, 44);
    knobB.setBounds (getWidth() - 64, 18, 44, 44);
    footswitch.setBounds (getWidth() / 2 - 28, getHeight() - 74, 56, 56);
}

void PedalView::mouseDown (const MouseEvent& e) { onDrag (*this, e, 0); }
void PedalView::mouseDrag (const MouseEvent& e) { onDrag (*this, e, 1); }
void PedalView::mouseUp (const MouseEvent& e) { onDrag (*this, e, 2); }

void PedalView::Footswitch::paintButton (Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat().reduced (4.0f).translated (0.0f, down ? 1.5f : 0.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (b.translated (0.0f, 3.0f));
    ColourGradient chrome (Colours::white, b.getX() + b.getWidth() * 0.35f, b.getY() + b.getHeight() * 0.3f,
                           Colour (0xff7d7d7d), b.getRight(), b.getBottom(), true);
    g.setGradientFill (chrome);
    g.fillEllipse (b);
    g.setColour (Colour (0xff5a5a5a));
    g.drawEllipse (b, 1.2f);
    g.setColour (Colours::white.withAlpha (over ? 0.35f : 0.18f));
    g.drawEllipse (b.reduced (b.getWidth() * 0.22f), 1.2f);
}

void PedalView::drawPicture (Graphics& g, Rectangle<float> a, Colour c) const
{
    // each pedal's picture moves with what the pedal is doing right now
    const bool on = footswitch.getToggleState();
    g.setColour (c);
    const float w = a.getWidth(), h = a.getHeight(), x0 = a.getX(), y0 = a.getY();
    const PathStrokeType thick (3.5f, PathStrokeType::curved, PathStrokeType::rounded);
    auto& m = processor.engine.meters;
    switch (pedal)
    {
        case Drive:
        {
            // a zigzag: taller and sharper the harder it drives
            const float amp = 0.25f + 0.55f * (on ? work : 0.2f) + 0.2f * processor.apvts.getRawParameterValue ("drive_a")->load();
            Path z;
            z.startNewSubPath (x0, y0 + h * 0.85f);
            for (int i = 0; i < 6; ++i)
                z.lineTo (x0 + w * (0.1f + 0.17f * (float) i), y0 + h * (i % 2 == 0 ? 0.5f - 0.45f * amp : 0.5f + 0.35f * amp));
            z.lineTo (x0 + w, y0 + h * 0.15f);
            g.strokePath (z, thick);
            break;
        }
        case Comp:
        {
            // two walls squeezing the sound in the middle
            const float squeeze = on ? jlimit (0.0f, 1.0f, 1.0f - m.gainReduction.load()) : 0.0f;
            const float gapW = w * (0.32f - 0.2f * squeeze);
            g.fillRect (a.getCentreX() - gapW - 4.0f, y0 + h * 0.1f, 4.0f, h * 0.8f);
            g.fillRect (a.getCentreX() + gapW, y0 + h * 0.1f, 4.0f, h * 0.8f);
            g.drawLine (x0, a.getCentreY(), a.getCentreX() - gapW - 6.0f, a.getCentreY(), 3.0f);
            g.drawLine (a.getCentreX() + gapW + 6.0f, a.getCentreY(), x0 + w, a.getCentreY(), 3.0f);
            Path arr;
            arr.addTriangle (a.getCentreX() - gapW + 2.0f, a.getCentreY() - 7.0f, a.getCentreX() - gapW + 2.0f, a.getCentreY() + 7.0f, a.getCentreX() - 2.0f, a.getCentreY());
            arr.addTriangle (a.getCentreX() + gapW - 2.0f, a.getCentreY() - 7.0f, a.getCentreX() + gapW - 2.0f, a.getCentreY() + 7.0f, a.getCentreX() + 2.0f, a.getCentreY());
            g.fillPath (arr);
            break;
        }
        case Filter:
        {
            // the filter's hump, sliding to where the cutoff is
            const float at = on ? m.lfo.load() : 0.5f;
            const float peakX = x0 + w * (0.2f + 0.6f * at);
            Path f;
            f.startNewSubPath (x0, y0 + h * 0.9f);
            f.cubicTo (peakX - w * 0.2f, y0 + h * 0.9f, peakX - w * 0.1f, y0 + h * 0.05f, peakX, y0 + h * 0.08f);
            f.cubicTo (peakX + w * 0.1f, y0 + h * 0.1f, peakX + w * 0.12f, y0 + h * 0.95f, x0 + w, y0 + h * 0.95f);
            g.strokePath (f, thick);
            break;
        }
        case Phaser:
        {
            // a dashed ring that turns
            const float r = jmin (w, h) * 0.42f;
            g.drawEllipse (a.getCentreX() - r, a.getCentreY() - r, r * 2.0f, r * 2.0f, 3.0f);
            const float spin = on ? time * (0.5f + 4.0f * processor.apvts.getRawParameterValue ("phaser_a")->load()) : 0.0f;
            for (int k = 0; k < 8; ++k)
            {
                const float ang = spin + (float) k / 8.0f * MathConstants<float>::twoPi;
                const float rr = r * 0.62f;
                g.fillEllipse (a.getCentreX() + std::cos (ang) * rr - 3.0f, a.getCentreY() + std::sin (ang) * rr - 3.0f, 6.0f, 6.0f);
            }
            g.fillEllipse (a.getCentreX() - 5.0f, a.getCentreY() - 5.0f, 10.0f, 10.0f);
            break;
        }
        case Chorus:
        {
            // two waves drifting against each other
            for (int k = 0; k < 2; ++k)
            {
                Path wv;
                const float shift = on ? time * (1.0f + (float) k * 0.7f) : (float) k;
                for (int i = 0; i <= 40; ++i)
                {
                    const float t = (float) i / 40.0f;
                    const Point<float> pt (x0 + t * w, a.getCentreY() + (float) (k * 10 - 5) + std::sin (t * 9.0f + shift) * h * 0.25f);
                    if (i == 0) wv.startNewSubPath (pt); else wv.lineTo (pt);
                }
                g.setColour (c.withAlpha (k == 0 ? 1.0f : 0.5f));
                g.strokePath (wv, PathStrokeType (3.0f));
            }
            break;
        }
        case Delay:
        {
            // echoes: each bar smaller; they light up as the echoes come back
            const float rep = processor.apvts.getRawParameterValue ("delay_b")->load();
            for (int k = 0; k < 5; ++k)
            {
                const float hh = h * std::pow (0.35f + 0.6f * rep, (float) k);
                g.setColour (c.withAlpha ((k == 0 ? 1.0f : 0.75f) * (0.55f + 0.45f * (on ? work : 0.3f))));
                g.fillRect (x0 + w * (0.06f + 0.19f * (float) k), y0 + h - hh, w * 0.12f, hh);
            }
            break;
        }
        case board::Reverb:
        {
            // arches of a hall, glowing with the reverb
            const float glow = on ? work : 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                const float s = 1.0f - 0.3f * (float) k;
                Path arch;
                const float aw = w * 0.9f * s, ah = h * 0.95f * s;
                const float ax = a.getCentreX() - aw * 0.5f, ay = y0 + h - ah;
                arch.startNewSubPath (ax, y0 + h);
                arch.lineTo (ax, ay + ah * 0.45f);
                arch.quadraticTo (a.getCentreX(), ay - ah * 0.3f, ax + aw, ay + ah * 0.45f);
                arch.lineTo (ax + aw, y0 + h);
                g.setColour (c.withAlpha ((1.0f - 0.3f * (float) k) * (0.7f + 0.3f * glow)));
                g.strokePath (arch, PathStrokeType (3.5f - (float) k));
            }
            break;
        }
        default: break;
    }
}

void PedalView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (2.0f);
    const auto col = pedalColours[pedal];
    const bool on = footswitch.getToggleState();

    // the shadow under the pedal
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (b.translated (0.0f, 6.0f), 9.0f);

    // hammered paint: the colour, a light from the top left, and little rings
    g.setColour (col);
    g.fillRoundedRectangle (b, 9.0f);
    {
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addRoundedRectangle (b, 9.0f);
        g.reduceClipRegion (clip);
        Random rnd (pedal * 31 + 7);
        for (int i = 0; i < 140; ++i)
        {
            const float x = b.getX() + rnd.nextFloat() * b.getWidth(), y = b.getY() + rnd.nextFloat() * b.getHeight();
            const float r = 3.0f + rnd.nextFloat() * 7.0f;
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.05f));
            g.drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 1.0f);
        }
        ColourGradient light (Colours::white.withAlpha (0.28f), b.getX() + b.getWidth() * 0.25f, b.getY() + b.getHeight() * 0.12f,
                              Colours::transparentWhite, b.getX() + b.getWidth() * 0.9f, b.getY() + b.getHeight() * 0.7f, true);
        g.setGradientFill (light);
        g.fillRect (b);
        if (! on) { g.setColour (Colours::black.withAlpha (0.45f)); g.fillRect (b); }
    }
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawRoundedRectangle (b, 9.0f, 1.5f);

    // screws in the corners
    for (auto pt : { b.getTopLeft() + Point<float> (8.0f, 8.0f), b.getTopRight() + Point<float> (-8.0f, 8.0f),
                     b.getBottomLeft() + Point<float> (8.0f, -8.0f), b.getBottomRight() + Point<float> (-8.0f, -8.0f) })
    {
        g.setColour (Colour (0xffbfbfbf));
        g.fillEllipse (pt.x - 3.5f, pt.y - 3.5f, 7.0f, 7.0f);
        g.setColour (Colour (0xff555555));
        g.drawLine (pt.x - 2.5f, pt.y, pt.x + 2.5f, pt.y, 1.0f);
    }

    // knob captions
    g.setColour (cream.withAlpha (on ? 0.9f : 0.5f));
    g.setFont (small (9.5f));
    g.drawText (knobCaption (pedal, 0).toUpperCase(), Rectangle<float> (8.0f, 64.0f, 68.0f, 14.0f), Justification::centred);
    g.drawText (knobCaption (pedal, 1).toUpperCase(), Rectangle<float> (b.getWidth() - 72.0f, 64.0f, 68.0f, 14.0f), Justification::centred);

    // the picture
    drawPicture (g, Rectangle<float> (24.0f, 96.0f, b.getWidth() - 44.0f, 74.0f), cream.withAlpha (on ? 0.95f : 0.45f));

    // the name in script, the kind under it
    g.setColour (Colours::black.withAlpha (0.25f));
    g.setFont (script (pedal == Comp ? 25.0f : 31.0f));
    const String name = pedalName (pedal);
    g.drawText (name, Rectangle<float> (0.0f, 194.0f, b.getWidth() + 4.0f, 40.0f), Justification::centred);
    g.setColour (cream.withAlpha (on ? 1.0f : 0.55f));
    g.drawText (name, Rectangle<float> (0.0f, 192.0f, b.getWidth() + 4.0f, 40.0f), Justification::centred);
    g.setFont (small (10.0f));
    g.setColour (cream.withAlpha (on ? 0.75f : 0.4f));
    g.drawText (String (pedalKind (pedal)).toUpperCase(), Rectangle<float> (0.0f, 232.0f, b.getWidth() + 4.0f, 14.0f), Justification::centred);

    // the LED
    const Point<float> led (b.getCentreX(), b.getBottom() - 92.0f);
    if (on)
    {
        g.setColour (Colour (0xffff3b30).withAlpha (0.35f));
        g.fillEllipse (led.x - 9.0f, led.y - 9.0f, 18.0f, 18.0f);
    }
    g.setColour (on ? Colour (0xffff4a3d) : Colour (0xff3a1210));
    g.fillEllipse (led.x - 4.5f, led.y - 4.5f, 9.0f, 9.0f);
}

//==============================================================================
PolyBoardEditor::PolyBoardEditor (PolyBoardProcessor& p) : processor (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (presetBar);
    for (int pd = 0; pd < numPedals; ++pd)
    {
        auto* v = pedals.add (new PedalView (processor, pd, [this] (PedalView& pv, const MouseEvent& e, int phase) { dragPedal (pv, e, phase); }));
        addAndMakeVisible (v);
    }
    for (auto [s, id, tip] : { std::tuple { &input, "input", "INPUT: how loud the synth comes in (turn up if the Poly D is quiet)" },
                               std::tuple { &output, "output", "OUTPUT: the volume after all the pedals" } })
    {
        s->setSliderStyle (Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s->setPopupDisplayEnabled (true, true, nullptr, 1200);
        s->getProperties().set ("plain", true);
        s->setDoubleClickReturnValue (true, 0.0);
        s->setTooltip (tip);
        addAndMakeVisible (*s);
    }
    inA = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, "input", input);
    outA = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, "output", output);
    setSize (designW, designH);
    startTimerHz (30);
}

PolyBoardEditor::~PolyBoardEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

Rectangle<int> PolyBoardEditor::slotBounds (int slot) const
{
    const int total = numPedals * pedalW + (numPedals - 1) * gap;
    const int x0 = (getWidth() - total) / 2;
    return { x0 + slot * (pedalW + gap), 128, pedalW, pedalH };
}

void PolyBoardEditor::layoutPedals (bool animate)
{
    const auto o = processor.getOrder();
    shownOrder = o;
    for (int slot = 0; slot < numPedals; ++slot)
    {
        auto* v = pedals[o[(size_t) slot]];
        if (v == dragging) continue;
        if (animate) Desktop::getInstance().getAnimator().animateComponent (v, slotBounds (slot), 1.0f, 140, false, 1.0, 1.0);
        else v->setBounds (slotBounds (slot));
    }
}

void PolyBoardEditor::dragPedal (PedalView& v, const MouseEvent& e, int phase)
{
    const auto o = processor.getOrder();
    if (phase == 0)
    {
        dragging = &v;
        dragFrom = (int) (std::find (o.begin(), o.end(), v.pedal) - o.begin());
        v.toFront (false);
        return;
    }
    if (dragging != &v) return;
    const auto home = slotBounds (dragFrom);
    if (phase == 1)
    {
        v.setTopLeftPosition (home.getX() + e.getDistanceFromDragStartX(), home.getY() - 10);
        // where would it land? move the others out of the way while dragging
        const int to = jlimit (0, numPedals - 1, (v.getBounds().getCentreX() - slotBounds (0).getX()) / (pedalW + gap));
        const int from = (int) (std::find (o.begin(), o.end(), v.pedal) - o.begin());
        if (to != from) { processor.movePedal (from, to); dragFrom = to; layoutPedals (true); }
        return;
    }
    dragging = nullptr;
    Desktop::getInstance().getAnimator().animateComponent (&v, slotBounds (dragFrom), 1.0f, 120, false, 1.0, 1.0);
    repaint();
}

void PolyBoardEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.fillAll (bg);
    // the board: a dark carpet under the pedals
    const auto board = Rectangle<float> (slotBounds (0).getX() - 22.0f, 108.0f, (float) (numPedals * pedalW + (numPedals - 1) * gap) + 44.0f, (float) pedalH + 52.0f);
    g.setColour (Colour (0xff120f0c));
    g.fillRoundedRectangle (board, 14.0f);
    g.setColour (Colour (0xff2a241e));
    g.drawRoundedRectangle (board, 14.0f, 1.0f);

    // the cables between the pedals (behind them)
    for (int slot = 0; slot + 1 < numPedals; ++slot)
    {
        const auto a = slotBounds (slot), c = slotBounds (slot + 1);
        const float y = (float) a.getY() + 150.0f;
        g.setColour (Colour (0xff0a0806));
        g.fillRoundedRectangle ((float) a.getRight() - 4.0f, y - 3.0f, (float) (c.getX() - a.getRight()) + 8.0f, 6.0f, 3.0f);
        g.setColour (Colour (0xffb0a68f));
        g.fillRect ((float) a.getRight() - 1.0f, y - 6.0f, 4.0f, 12.0f);
        g.fillRect ((float) c.getX() - 3.0f, y - 6.0f, 4.0f, 12.0f);
    }

    // the top bar: minimal
    g.setColour (cream);
    g.setFont (small (15.0f));
    g.drawText ("P O L Y   B O A R D", 36, 30, 260, 22, Justification::centredLeft);
    g.setColour (dim);
    g.setFont (small (12.0f));
    g.drawText ("drag a pedal to move it  .  click the footswitch to switch it", 36, 54, 420, 16, Justification::centredLeft);

    String tuner = "tuner: play a note";
    Colour tc = dim;
    if (tunerHz > 0.0f)
    {
        float cents = 0.0f;
        const auto name = noteName (tunerHz, cents);
        tuner = name + "  " + (cents >= 0 ? "+" : "") + String (roundToInt (cents)) + " c";
        tc = std::abs (cents) < 5.0f ? Colour (0xff6fd08c) : std::abs (cents) < 20.0f ? Colour (0xffffc861) : Colour (0xffff6b5b);
    }
    g.setFont (small (13.0f));
    g.setColour (dim);
    g.drawText (String (roundToInt (processor.bpm.load())) + " BPM", getWidth() - 490, 32, 70, 20, Justification::centredRight);
    g.setColour (tc);
    g.drawText (tuner, getWidth() - 390, 32, 130, 20, Justification::centredRight);

    // the bottom: in and out, thin
    const float y = (float) getHeight() - 52.0f;
    auto bar = [&] (float x, float w, float level, const char* label)
    {
        g.setColour (dim);
        g.setFont (small (11.0f));
        g.drawText (label, (int) x, (int) y - 8, 40, 16, Justification::centredLeft);
        g.setColour (Colour (0xff2a241e));
        g.fillRoundedRectangle (x + 40.0f, y - 2.0f, w, 4.0f, 2.0f);
        const float db = Decibels::gainToDecibels (level, -60.0f);
        const float f = jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (level > 0.99f ? Colour (0xffff6b5b) : cream);
        g.fillRoundedRectangle (x + 40.0f, y - 2.0f, w * f, 4.0f, 2.0f);
    };
    bar (36.0f, b.getWidth() * 0.5f - 170.0f, inLevel, "IN");
    bar (b.getWidth() * 0.5f + 40.0f, b.getWidth() * 0.5f - 170.0f, outLevel, "OUT");
}

void PolyBoardEditor::resized()
{
    presetBar.setBounds (getWidth() - 250, 26, 214, 32);
    input.setBounds (getWidth() / 2 - 80, getHeight() - 70, 36, 36);
    output.setBounds (getWidth() - 80, getHeight() - 70, 36, 36);
    layoutPedals (false);
}

void PolyBoardEditor::timerCallback()
{
    const auto now = Time::getMillisecondCounter();
    const float dt = lastTick == 0 ? 1.0f / 30.0f : jlimit (0.005f, 0.1f, (float) (now - lastTick) / 1000.0f);
    lastTick = now;
    time += dt;
    auto& m = processor.engine.meters;
    inLevel = jmax (m.inLevel.load(), inLevel * 0.85f);
    outLevel = jmax (m.outLevel.load(), outLevel * 0.85f);
    for (auto* v : pedals)
    {
        v->time = time;
        v->work += (m.work[(size_t) v->pedal].load() - v->work) * jmin (1.0f, dt * 12.0f);
        v->repaint (0, 90, v->getWidth(), 90);
    }
    // a preset (or the host) changed the order: move the pedals there
    if (dragging == nullptr && processor.getOrder() != shownOrder) { shownOrder = processor.getOrder(); layoutPedals (true); }
    if (--tunerCountdown <= 0)
    {
        tunerCountdown = 4;      // about 7 times a second
        const float hz = processor.detectPitch();
        tunerHz = hz > 0.0f ? (tunerHz > 0.0f && std::abs (hz / tunerHz - 1.0f) < 0.03f ? tunerHz * 0.6f + hz * 0.4f : hz) : 0.0f;
    }
    repaint (0, 0, getWidth(), 100);
    repaint (0, getHeight() - 80, getWidth(), 80);
}

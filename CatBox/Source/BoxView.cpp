#include "BoxView.h"

namespace catbox
{
namespace
{
namespace C
{
    const juce::Colour bg       { 0xff0b0d12 };
    const juce::Colour screen   { 0xff10131a };
    const juce::Colour cell     { 0xff161a23 };
    const juce::Colour body     { 0xff1b1e26 };
    const juce::Colour bodyEdge { 0xff2a2e38 };
    const juce::Colour text     { 0xffe8eaf0 };
    const juce::Colour dim      { 0xff7d8494 };
    const juce::Colour faint    { 0xff2a2e38 };
    const juce::Colour padOff   { 0xff242832 };
    const juce::Colour stepOn   { 0xff6fcf7f };
    const juce::Colour stepTie  { 0xff5d8fe0 };
    const juce::Colour stepAcc  { 0xffffa64d };
    const juce::Colour rec      { 0xffe5534b };
    const juce::Colour global   { 0xffd8dce6 };
}

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

const char padKeys[numPads] = { '1', '2', '3', '4', 'Q', 'W', 'E', 'R', 'A', 'S', 'D', 'F', 'Z', 'X', 'C', 'V' };

bool keyDown (int c)
{
    return juce::KeyPress::isKeyCurrentlyDown (c)
        || (c >= 'A' && c <= 'Z' && juce::KeyPress::isKeyCurrentlyDown (c + ('a' - 'A')));
}

bool padKeyDown (int pad)
{
    const int c = padKeys[pad];
    return keyDown (c) || (c == 'Z' && keyDown ('Y'));   // Slovak / German keyboards swap Z and Y
}

int buttonKey (int b)
{
    switch (b)
    {
        case BtnPlay:       return juce::KeyPress::spaceKey;
        case BtnHold:       return 'H';
        case BtnTap:        return 'T';
        case BtnInstrument: return juce::KeyPress::tabKey;
        case BtnOctDown:    return ',';
        case BtnOctUp:      return '.';
        case BtnRec:        return juce::KeyPress::returnKey;
        default:            return 0;
    }
}

void drawArc (juce::Graphics& g, juce::Point<float> c, float r, float from, float to, float thickness, juce::Colour col)
{
    juce::Path p;
    p.addCentredArc (c.x, c.y, r, r, 0.0f, from, to, true);
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

juce::Colour BoxView::instrumentColour (int i)
{
    return i == Drums ? juce::Colour (0xffeb9a5c) : i == Arp ? juce::Colour (0xff5fd3b8) : juce::Colour (0xffab8ef2);
}

BoxView::BoxView (BoxController& b, bool kioskMode) : box (b), kiosk (kioskMode)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    if (kiosk) setMouseCursor (juce::MouseCursor::NoCursor);
    startTimerHz (30);
}

BoxView::~BoxView() { stopTimer(); }

void BoxView::resized()
{
    const auto lb = logicalBounds().toFloat();
    scale = juce::jmin ((float) getWidth() / lb.getWidth(), (float) getHeight() / lb.getHeight());
    offset = { ((float) getWidth() - lb.getWidth() * scale) * 0.5f, ((float) getHeight() - lb.getHeight() * scale) * 0.5f };
}

juce::Point<float> BoxView::toLogical (juce::Point<float> p) const
{
    return (p - offset) / scale;
}

void BoxView::timerCallback()
{
    const uint32_t f = box.getAudio().drumFlash.exchange (0);
    for (int p = 0; p < numPads; ++p)
    {
        if ((f >> p) & 1u) flashLevel[(size_t) p] = 1.0f;
        else flashLevel[(size_t) p] *= 0.78f;
    }
    if (! hasKeyboardFocus (true) && isShowing() && getPeer() != nullptr && getPeer()->isFocused()) grabKeyboardFocus();
    repaint();
}

//==============================================================================
juce::Colour BoxView::padColour (int pad, bool& lit, bool& outlined) const
{
    lit = outlined = false;
    const int page = box.getPage();
    const auto col = instrumentColour (page);
    const auto& data = box.getData();
    const bool pressed = ((pressedPads >> pad) & 1u) != 0;

    if (box.isShiftHeld())
    {
        outlined = data.loadedPreset[(size_t) page] == pad;
        if (box.presetExists (page, pad)) { lit = outlined; return outlined ? col : col.withMultipliedBrightness (0.55f); }
        return C::padOff;
    }

    switch (page)
    {
        case Drums:
        {
            const float lvl = juce::jmax (flashLevel[(size_t) pad], pressed ? 1.0f : 0.0f);
            lit = lvl > 0.3f;
            return C::padOff.interpolatedWith (col, 0.22f + 0.78f * lvl);
        }
        case Arp:
            if (box.isStepView())
            {
                const auto& a = data.inst[Arp].values;
                const int len = juce::roundToInt (a[slot::ALength]);
                outlined = box.getAudio().arpStep.load() == pad;
                const auto st = data.inst[Arp].arpSteps[(size_t) pad];
                juce::Colour c = st == StepOn ? C::stepOn : st == StepTie ? C::stepTie : st == StepAccent ? C::stepAcc : C::padOff;
                if (pad >= len) c = c.withMultipliedBrightness (0.3f);
                return c;
            }
            else
            {
                const int note = box.arpPadNote (pad);
                const uint64_t bits = note < 64 ? box.getAudio().arpNotesLo.load() : box.getAudio().arpNotesHi.load();
                lit = pressed || ((bits >> (note & 63)) & 1u) != 0;
                const bool root = pad % (int) scaleIntervals (juce::roundToInt (data.globals[GScale])).size() == 0;
                return lit ? col : C::padOff.interpolatedWith (col, root ? 0.38f : 0.16f);
            }
        case Bass:
        default:
        {
            lit = pressed || data.bassChordDegree == pad;
            const bool root = pad % (int) scaleIntervals (juce::roundToInt (data.globals[GScale])).size() == 0;
            return lit ? col : C::padOff.interpolatedWith (col, root ? 0.38f : 0.16f);
        }
    }
}

void BoxView::paintPad (juce::Graphics& g, juce::Rectangle<float> r, int pad, bool big)
{
    bool lit = false, outlined = false;
    const auto c = padColour (pad, lit, outlined);
    if (lit && big)
    {
        g.setColour (c.withAlpha (0.35f));
        g.fillRoundedRectangle (r.expanded (3.0f), 9.0f);
    }
    g.setColour (c);
    g.fillRoundedRectangle (r, big ? 7.0f : 4.0f);
    if (outlined)
    {
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (r.reduced (1.0f), big ? 7.0f : 4.0f, big ? 2.5f : 1.6f);
    }
    if (! big) return;

    // what the pad does right now
    juce::String label;
    const int page = box.getPage();
    if (box.isShiftHeld()) label = juce::String (pad + 1);
    else if (page == Drums) label = drumPadName (pad);
    else if (page == Arp && box.isStepView())
    {
        const auto st = box.getData().inst[Arp].arpSteps[(size_t) pad];
        label = juce::String (pad + 1) + (st == StepRest ? " -" : st == StepTie ? " ~" : st == StepAccent ? " !" : "");
    }
    else if (page == Arp) label = juce::MidiMessage::getMidiNoteName (box.arpPadNote (pad), true, true, 4);
    else label = degreeChordName (juce::roundToInt (box.getData().globals[GKey]),
                                  juce::roundToInt (box.getData().globals[GScale]), pad);
    g.setColour (lit ? juce::Colours::black.withAlpha (0.75f) : C::text.withAlpha (0.7f));
    g.setFont (font (12.5f, true));
    g.drawText (label, r.reduced (4.0f), juce::Justification::centredBottom);
    g.setFont (font (10.0f));
    g.setColour (C::dim.withAlpha (lit ? 0.0f : 0.8f));
    g.drawText (juce::String::charToString ((juce::juce_wchar) padKeys[pad]), r.reduced (5.0f, 3.0f), juce::Justification::topLeft);
}

//==============================================================================
void BoxView::paint (juce::Graphics& g)
{
    g.fillAll (C::bg);
    g.addTransform (juce::AffineTransform::scale (scale).translated (offset));
    paintScreen (g, { 0.0f, 0.0f, (float) screenW, (float) screenH });
    if (! kiosk) paintPanel (g, { 0.0f, (float) screenH, (float) screenW, (float) panelH });
}

void BoxView::paintScreen (juce::Graphics& g, juce::Rectangle<float> area)
{
    const int page = box.getPage();
    const auto col = instrumentColour (page);
    const auto& data = box.getData();
    auto& audio = box.getAudio();

    g.setColour (C::screen);
    g.fillRect (area);

    // header: instrument, the three pages, tempo, key
    auto header = area.removeFromTop (64.0f).reduced (22.0f, 0.0f);
    g.setColour (col);
    g.setFont (font (38.0f, true));
    g.drawText (instrumentName (page).toUpperCase(), header.removeFromLeft (170.0f), juce::Justification::centredLeft);

    auto tabs = header.removeFromLeft (230.0f);
    for (int i = 0; i < NumInstruments; ++i)
    {
        auto t = tabs.removeFromLeft (76.0f);
        const bool active = i == page;
        g.setColour (data.muted[(size_t) i] ? C::rec.withAlpha (0.7f) : active ? instrumentColour (i) : C::dim);
        g.setFont (font (15.0f, active));
        g.drawText (instrumentName (i) + (data.muted[(size_t) i] ? " M" : ""), t, juce::Justification::centred);
        if (active) { g.setColour (instrumentColour (i)); g.fillRect (t.getCentreX() - 18.0f, t.getBottom() - 16.0f, 36.0f, 3.0f); }
    }

    // play state + tempo + key on the right
    {
        auto right = header;
        const float cy = right.getCentreY();
        g.setFont (font (22.0f, true));
        g.setColour (C::text);
        const auto keyText = keyNames()[juce::roundToInt (data.globals[GKey])] + " "
                           + scaleNames()[juce::roundToInt (data.globals[GScale])].toLowerCase();
        g.setFont (font (16.0f));
        g.setColour (C::dim);
        g.drawText (keyText, right.removeFromRight (150.0f), juce::Justification::centredRight);
        g.setFont (font (22.0f, true));
        g.setColour (C::text);
        g.drawText (juce::String (juce::roundToInt (data.globals[GTempo])) + " BPM", right.removeFromRight (110.0f),
                    juce::Justification::centredRight);
        const float x = right.getRight() - 30.0f;
        juce::Path p;
        if (audio.isPlaying()) p.addTriangle (x, cy - 9.0f, x, cy + 9.0f, x + 15.0f, cy);
        else p.addRectangle (x, cy - 7.0f, 14.0f, 14.0f);
        g.setColour (audio.isPlaying() ? C::stepOn : C::dim);
        g.fillPath (p);
    }

    // status chips
    auto chips = area.removeFromTop (28.0f).reduced (22.0f, 3.0f);
    auto chip = [&g, &chips] (const juce::String& s, juce::Colour c)
    {
        const float w = 18.0f + s.length() * 8.5f;
        auto r = chips.removeFromLeft (w);
        chips.removeFromLeft (8.0f);
        g.setColour (c.withAlpha (0.2f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (c);
        g.setFont (font (13.0f, true));
        g.drawText (s, r, juce::Justification::centred);
    };
    if (box.isShiftHeld()) chip ("SHIFT", C::global);
    if (box.isLatched()) chip ("HOLD", instrumentColour (Arp));
    if (audio.isRecording()) chip ("REC", C::rec);
    if (box.isStepView()) chip ("KROKY", C::stepOn);
    if (data.muted[(size_t) page]) chip (sk ("STLMENÉ"), C::rec);
    if (data.loadedPreset[(size_t) page] >= 0) chip ("PRESET " + juce::String (data.loadedPreset[(size_t) page] + 1), col);

    // the 12 knobs (the Shift layer while Shift is held)
    auto knobs = area.removeFromTop (240.0f).reduced (10.0f, 0.0f);
    const int layer = box.isShiftHeld() ? numKnobs : 0;
    for (int row = 0; row < 2; ++row)
    {
        auto r = knobs.removeFromTop (120.0f);
        const float w = r.getWidth() / 6.0f;
        for (int c = 0; c < 6; ++c)
            paintKnobCell (g, r.removeFromLeft (w).reduced (4.0f), layer + row * 6 + c);
    }

    paintBottom (g, area.reduced (10.0f, 6.0f));
}

void BoxView::paintKnobCell (juce::Graphics& g, juce::Rectangle<float> r, int slotIndex)
{
    const int page = box.getPage();
    const auto& d = paramDesc (page, slotIndex);
    g.setColour (C::cell);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (C::dim.withAlpha (0.6f));
    g.setFont (font (11.0f));
    g.drawText (juce::String (slotIndex % numKnobs + 1), r.reduced (7.0f, 5.0f), juce::Justification::topLeft);
    if (! d.used())
    {
        g.setColour (C::faint);
        g.setFont (font (22.0f));
        g.drawText ("-", r, juce::Justification::centred);
        return;
    }

    const float v = box.getData().get (page, slotIndex);
    const float frac = d.max > d.min ? (v - d.min) / (d.max - d.min) : 0.0f;
    const auto col = d.global >= 0 ? C::global : instrumentColour (page);
    const juce::Point<float> c { r.getCentreX(), r.getY() + 38.0f };
    constexpr float a0 = -2.35f, a1 = 2.35f;
    drawArc (g, c, 24.0f, a0, a1, 5.0f, C::faint);
    if (frac > 0.001f) drawArc (g, c, 24.0f, a0, a0 + (a1 - a0) * frac, 5.0f, col);

    g.setColour (C::dim);
    g.setFont (font (14.0f));
    g.drawText (d.name, r.withTrimmedTop (66.0f).withHeight (18.0f), juce::Justification::centred);
    g.setColour (C::text);
    g.setFont (font (17.0f, true));
    g.drawFittedText (d.format (v), r.withTrimmedTop (84.0f).withHeight (24.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void BoxView::paintBottom (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int page = box.getPage();
    const auto col = instrumentColour (page);
    const auto& data = box.getData();
    auto& audio = box.getAudio();

    // mini pad map
    auto mini = r.removeFromLeft (140.0f).reduced (8.0f, 2.0f);
    const float cell = juce::jmin (mini.getWidth(), mini.getHeight()) / 4.0f;
    for (int p = 0; p < numPads; ++p)
        paintPad (g, { mini.getX() + (p % 4) * cell, mini.getY() + (p / 4) * cell, cell - 4.0f, cell - 4.0f }, p, false);

    // level meter on the right edge
    auto meter = r.removeFromRight (10.0f).reduced (2.0f, 4.0f);
    g.setColour (C::faint);
    g.fillRoundedRectangle (meter, 2.0f);
    const float pk = juce::jlimit (0.0f, 1.0f, audio.peak.load());
    g.setColour (pk > 0.85f ? C::rec : C::stepOn);
    g.fillRoundedRectangle (meter.withTrimmedTop (meter.getHeight() * (1.0f - pk)), 2.0f);

    auto info = r.reduced (10.0f, 4.0f);
    auto line1 = info.removeFromTop (34.0f);
    g.setFont (font (19.0f, true));
    g.setColour (C::text);
    const auto& iv = data.inst[(size_t) page].values;

    if (page == Drums)
    {
        g.drawText (paramDesc (Drums, slot::DStyle).format (iv[slot::DStyle]) + "  |  " + paramDesc (Drums, slot::DKit).format (iv[slot::DKit])
                      + sk ("  |  Variácia ") + juce::String (juce::roundToInt (iv[slot::DSeed])),
                    line1, juce::Justification::centredLeft);
        const int steps = barsFromChoice (iv[slot::DBars]) * 16;
        const int cur = audio.drumStep.load();
        auto bar = info.removeFromTop (34.0f).reduced (0.0f, 6.0f);
        const float w = bar.getWidth() / steps;
        for (int s = 0; s < steps; ++s)
        {
            auto b = bar.withX (bar.getX() + s * w).withWidth (w - (steps > 32 ? 1.0f : 2.0f));
            g.setColour (s == cur ? col : (s % 4 == 0 ? C::faint.brighter (0.3f) : C::faint));
            g.fillRoundedRectangle (b, 2.0f);
        }
        g.setFont (font (14.0f));
        g.setColour (C::dim);
        g.drawText (juce::String ((int) data.inst[Drums].drumHits.size()) + sk (" nahratých úderov  |  Rec = nahrávať, Shift+Tap = zmazať"),
                    info.removeFromTop (26.0f), juce::Justification::centredLeft);
    }
    else if (page == Arp)
    {
        g.drawText (paramDesc (Arp, slot::AMode).format (iv[slot::AMode]) + "  |  " + paramDesc (Arp, slot::ADivision).format (iv[slot::ADivision])
                      + sk ("  |  oktávy ") + juce::String (juce::roundToInt (iv[slot::AOctaves]))
                      + "  |  " + paramDesc (Arp, slot::ASound).format (iv[slot::ASound]),
                    line1, juce::Justification::centredLeft);
        const int len = juce::roundToInt (iv[slot::ALength]);
        const int cur = audio.arpStep.load();
        auto bar = info.removeFromTop (40.0f).reduced (0.0f, 4.0f);
        const float w = bar.getWidth() / 16.0f;
        for (int s = 0; s < 16; ++s)
        {
            const auto st = data.inst[Arp].arpSteps[(size_t) s];
            auto c = st == StepOn ? C::stepOn : st == StepTie ? C::stepTie : st == StepAccent ? C::stepAcc : C::faint;
            if (s >= len) c = c.withMultipliedBrightness (0.3f);
            auto b = bar.withX (bar.getX() + s * w).withWidth (w - 3.0f);
            g.setColour (c);
            g.fillRoundedRectangle (b, 3.0f);
            if (s == cur) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (b, 3.0f, 2.0f); }
        }
        g.setFont (font (14.0f));
        g.setColour (C::dim);
        g.drawText (sk ("zelená hrá  |  modrá ťahá  |  oranžová akcent  |  Rec = upraviť kroky"),
                    info.removeFromTop (24.0f), juce::Justification::centredLeft);
    }
    else
    {
        auto big = info.removeFromLeft (130.0f);
        g.setColour (col);
        g.setFont (font (46.0f, true));
        g.drawText (box.bassChordName(), big, juce::Justification::centredLeft);
        g.setColour (C::text);
        g.setFont (font (19.0f, true));
        g.drawText (paramDesc (Bass, slot::BStyle).format (iv[slot::BStyle]) + "  |  " + paramDesc (Bass, slot::BSound).format (iv[slot::BSound]),
                    info.removeFromTop (40.0f), juce::Justification::centredLeft);
        g.setFont (font (14.0f));
        g.setColour (C::dim);
        g.drawText (sk ("Pady vyberajú akord, bass hrá linku k nemu"), info.removeFromTop (26.0f), juce::Justification::centredLeft);
    }

    // short messages ("Saved: Arp 5")
    const auto flash = box.getFlash();
    if (flash.isNotEmpty())
    {
        auto f = r.reduced (20.0f, 30.0f).withTrimmedLeft (0.0f);
        g.setColour (C::bg.withAlpha (0.92f));
        g.fillRoundedRectangle (f, 10.0f);
        g.setColour (col);
        g.drawRoundedRectangle (f, 10.0f, 2.0f);
        g.setColour (C::text);
        g.setFont (font (20.0f, true));
        g.drawFittedText (flash, f.reduced (12.0f, 4.0f).toNearestInt(), juce::Justification::centred, 2);
    }
}

//==============================================================================
juce::Rectangle<float> BoxView::knobArea (int knob) const
{
    const float w = 760.0f / 6.0f;
    return { 20.0f + (knob % 6) * w, (float) screenH + 14.0f + (knob / 6) * 82.0f, w, 80.0f };
}

juce::Rectangle<float> BoxView::padArea (int pad) const
{
    const float w = 330.0f / 4.0f, h = 58.0f;
    return juce::Rectangle<float> { 20.0f + (pad % 4) * w, (float) screenH + 186.0f + (pad / 4) * h, w, h }.reduced (4.0f);
}

juce::Rectangle<float> BoxView::buttonArea (int b) const
{
    const float w = 420.0f / 4.0f;
    return juce::Rectangle<float> { 365.0f + (b % 4) * w, (float) screenH + 210.0f + (b / 4) * 95.0f, w, 76.0f }.reduced (6.0f);
}

juce::String BoxView::buttonName (int b)
{
    switch (b)
    {
        case BtnPlay:       return "Play";
        case BtnHold:       return "Hold";
        case BtnShift:      return "Shift";
        case BtnTap:        return "Tap";
        case BtnInstrument: return sk ("Nástroj");
        case BtnOctDown:    return sk ("Okt -");
        case BtnOctUp:      return sk ("Okt +");
        case BtnRec:        return "Rec";
        default:            return {};
    }
}

void BoxView::paintPanel (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (C::body);
    g.fillRect (area);
    g.setColour (C::bodyEdge);
    g.drawLine (area.getX(), area.getY() + 1.0f, area.getRight(), area.getY() + 1.0f, 2.0f);

    const int page = box.getPage();
    const auto col = instrumentColour (page);
    const int layer = box.isShiftHeld() ? numKnobs : 0;

    for (int k = 0; k < numKnobs; ++k)
    {
        auto r = knobArea (k);
        const juce::Point<float> c { r.getCentreX(), r.getY() + 28.0f };
        g.setColour (juce::Colour (0xff0f1116));
        g.fillEllipse (c.x - 25.0f, c.y - 25.0f, 50.0f, 50.0f);
        g.setColour (C::bodyEdge.brighter (0.4f));
        g.drawEllipse (c.x - 25.0f, c.y - 25.0f, 50.0f, 50.0f, 1.5f);
        const auto& d = paramDesc (page, layer + k);
        if (d.used())
        {
            const float v = box.getData().get (page, layer + k);
            const float frac = d.max > d.min ? (v - d.min) / (d.max - d.min) : 0.0f;
            const float ang = -2.35f + 4.7f * frac;
            g.setColour (d.global >= 0 ? C::global : col);
            g.drawLine (c.x + std::sin (ang) * 8.0f, c.y - std::cos (ang) * 8.0f,
                        c.x + std::sin (ang) * 21.0f, c.y - std::cos (ang) * 21.0f, 3.0f);
        }
        g.setColour (C::dim);
        g.setFont (font (12.5f));
        g.drawText (d.used() ? d.name : juce::String ("-"), r.withTrimmedTop (56.0f), juce::Justification::centredTop);
    }

    for (int p = 0; p < numPads; ++p) paintPad (g, padArea (p), p, true);

    for (int b = 0; b < NumButtons; ++b)
    {
        auto r = buttonArea (b);
        const bool lit = box.isButtonLit (b) || (b == BtnShift && mouseShiftLatched);
        const auto litCol = b == BtnRec ? C::rec : b == BtnPlay ? C::stepOn : b == BtnInstrument ? col : C::global;
        g.setColour (lit ? litCol : juce::Colour (0xff2b2f39));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (lit ? juce::Colours::black : C::text);
        g.setFont (font (16.0f, true));
        g.drawText (buttonName (b), r.withTrimmedBottom (12.0f), juce::Justification::centred);
        g.setFont (font (11.0f));
        g.setColour (lit ? juce::Colours::black.withAlpha (0.6f) : C::dim);
        const char* keyHint[NumButtons] = { "Space", "H", "Shift", "T", "Tab", ",", ".", "Enter" };
        g.drawText (keyHint[b], r.withTrimmedTop (34.0f), juce::Justification::centred);
    }

    g.setColour (C::dim.withAlpha (0.7f));
    g.setFont (font (12.0f));
    g.drawText (sk ("Gombíky: ťahaj myšou alebo koliesko  |  Shift + pad = načítať, Shift + Rec + pad = uložiť  |  pravý klik = zvuková karta"),
                juce::Rectangle<float> (20.0f, area.getBottom() - 24.0f, 760.0f, 20.0f), juce::Justification::centred);
}

//==============================================================================
void BoxView::mouseDown (const juce::MouseEvent& e)
{
    const auto p = toLogical (e.position);
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, sk ("Zvuková karta a MIDI..."));
        m.addItem (2, sk ("Celá obrazovka (F11)"));
        m.addItem (3, sk ("Otvoriť priečinok s presetmi"));
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
        {
            if (r == 1 && onSettings) onSettings();
            if (r == 2 && onToggleFullScreen) onToggleFullScreen();
            if (r == 3)
            {
                auto dir = box.getDataDirectory().getChildFile ("Presets");
                dir.createDirectory();
                dir.startAsProcess();
            }
        });
        return;
    }
    if (kiosk) return;

    for (int k = 0; k < numKnobs; ++k)
        if (knobArea (k).contains (p)) { drag = Drag::Knob; dragIndex = k; lastDragY = p.y; return; }
    for (int pad = 0; pad < numPads; ++pad)
        if (padArea (pad).contains (p))
        {
            drag = Drag::Pad; dragIndex = pad;
            pressedPads |= 1u << pad;
            box.padDown (pad, 100);
            return;
        }
    for (int b = 0; b < NumButtons; ++b)
        if (buttonArea (b).contains (p))
        {
            if (b == BtnShift)   // a mouse can't hold Shift while clicking a pad: it toggles instead
            {
                mouseShiftLatched = ! mouseShiftLatched;
                mouseShiftLatched ? box.buttonDown (BtnShift) : box.buttonUp (BtnShift);
                return;
            }
            drag = Drag::Button; dragIndex = b;
            box.buttonDown (b);
            return;
        }
}

void BoxView::mouseDrag (const juce::MouseEvent& e)
{
    if (drag != Drag::Knob) return;
    const auto p = toLogical (e.position);
    const float dy = lastDragY - p.y;
    lastDragY = p.y;
    box.turnKnob (dragIndex, dy / (e.mods.isShiftDown() ? 24.0f : 6.0f));
}

void BoxView::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::Pad)
    {
        pressedPads &= ~(1u << dragIndex);
        box.padUp (dragIndex);
        // a mouse-latched Shift is used up by one preset pad press
        if (mouseShiftLatched) { mouseShiftLatched = false; box.buttonUp (BtnShift); }
    }
    if (drag == Drag::Button) box.buttonUp (dragIndex);
    drag = Drag::None;
    dragIndex = -1;
}

void BoxView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (kiosk) return;
    const auto p = toLogical (e.position);
    for (int k = 0; k < numKnobs; ++k)
        if (knobArea (k).contains (p))
        {
            const float dir = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) > 0.0f ? 1.0f : -1.0f;
            box.turnKnob (k, dir);
            return;
        }
}

//==============================================================================
bool BoxView::keyPressed (const juce::KeyPress& k)
{
    if (k.getKeyCode() == juce::KeyPress::F11Key) { if (onToggleFullScreen) onToggleFullScreen(); return true; }
    return true;   // every other key is read in keyStateChanged (and Tab must not move the focus)
}

bool BoxView::keyStateChanged (bool)
{
    for (int p = 0; p < numPads; ++p)
    {
        const bool down = padKeyDown (p);
        if (down == keyPadDown[(size_t) p]) continue;
        keyPadDown[(size_t) p] = down;
        if (down) { pressedPads |= 1u << p; box.padDown (p, 100); }
        else      { pressedPads &= ~(1u << p); box.padUp (p); }
    }
    for (int b = 0; b < NumButtons; ++b)
    {
        const int key = buttonKey (b);
        if (key == 0) continue;
        const bool down = keyDown (key);
        if (down == keyButtonDown[(size_t) b]) continue;
        keyButtonDown[(size_t) b] = down;
        down ? box.buttonDown (b) : box.buttonUp (b);
    }
    return true;
}

void BoxView::modifierKeysChanged (const juce::ModifierKeys& m)
{
    if (m.isShiftDown() != box.isShiftHeld() && ! mouseShiftLatched)
        m.isShiftDown() ? box.buttonDown (BtnShift) : box.buttonUp (BtnShift);
}

} // namespace catbox

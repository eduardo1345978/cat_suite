#include "BoxView.h"
#include <cmath>

namespace catbox
{
namespace
{
const BoxView::Palette darkScreen  { juce::Colour (0xff0b0d12), juce::Colour (0xff10131a), juce::Colour (0xff161a23), juce::Colour (0xffe8eaf0),
                                     juce::Colour (0xff7d8494), juce::Colour (0xff2a2e38), juce::Colour (0xff242832) };
const BoxView::Palette lightScreen { juce::Colour (0xff0b0d12), juce::Colour (0xfff4f5f8), juce::Colour (0xffffffff), juce::Colour (0xff1b1e26),
                                     juce::Colour (0xff6a7184), juce::Colour (0xffd9dce4), juce::Colour (0xffcfd3dc) };

namespace C
{
    // the front panel is the black 3D-printed case: always dark
    const juce::Colour body     { 0xff17191f };
    const juce::Colour bodyEdge { 0xff2a2e38 };
    const juce::Colour panelOff { 0xff262a33 };
    const juce::Colour text     { 0xffe8eaf0 };
    const juce::Colour dim      { 0xff7d8494 };
    const juce::Colour stepOn   { 0xff4fbf6a };
    const juce::Colour stepTie  { 0xff4f84d8 };
    const juce::Colour stepAcc  { 0xfff09a3e };
    const juce::Colour rec      { 0xffe0504a };
    const juce::Colour global   { 0xffc9cedb };
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
        case BtnSteps:      return 'B';
        case BtnTap:        return 'T';
        case BtnInstrument: return juce::KeyPress::tabKey;
        case BtnOctDown:    return ',';
        case BtnOctUp:      return '.';
        case BtnRec:        return juce::KeyPress::returnKey;
        default:            return 0;   // Shift: modifierKeysChanged
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
    return i == Drums ? juce::Colour (0xffe8894a) : i == Arp ? juce::Colour (0xff33c2a2) : juce::Colour (0xff9b7cec);
}

BoxView::BoxView (BoxController& b, bool kioskMode) : box (b), kiosk (kioskMode)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    if (kiosk) setMouseCursor (juce::MouseCursor::NoCursor);
    startTimerHz (30);
}

BoxView::~BoxView() { stopTimer(); }

const BoxView::Palette& BoxView::pal() const { return box.isLightScreen() ? lightScreen : darkScreen; }

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
juce::Colour BoxView::padColour (int pad, bool& lit, bool& outlined, bool& thick) const
{
    lit = outlined = thick = false;
    const int page = box.getPage();
    const auto col = instrumentColour (page);
    const auto& data = box.getData();
    const auto off = C::panelOff;
    const bool pressed = ((pressedPads >> pad) & 1u) != 0;
    auto& audio = box.getAudio();

    if (box.isShiftHeld())
    {
        outlined = data.loadedPreset[(size_t) page] == pad;
        if (box.presetExists (page, pad)) { lit = outlined; return outlined ? col : col.withMultipliedBrightness (0.55f); }
        return off;
    }

    if (box.isStepView())
    {
        const int step = (page == Arp ? 0 : juce::jmin (box.stepPage (page), box.numStepPages (page) - 1) * numPads) + pad;
        if (page == Drums)
        {
            const int len = data.drumLength();
            outlined = audio.isPlaying() && audio.drumStep.load() == step;
            if (step >= len) return off.withMultipliedBrightness (0.6f);
            const int v = data.inst[Drums].drumSteps[(size_t) data.selectedDrum][(size_t) step];
            lit = v > 0;
            return v == 0 ? (step % 4 == 0 ? off.brighter (0.15f) : off) : v >= 120 ? col.brighter (0.35f) : col.withMultipliedBrightness (0.85f);
        }
        if (page == Arp)
        {
            const int len = juce::roundToInt (data.inst[Arp].values[slot::ALength]);
            outlined = audio.arpStep.load() == pad;
            const auto st = data.inst[Arp].arpSteps[(size_t) pad];
            juce::Colour c = st == StepOn ? C::stepOn : st == StepTie ? C::stepTie : st == StepAccent ? C::stepAcc : off;
            return pad >= len ? c.withMultipliedBrightness (0.3f) : c;
        }
        const int len = data.bassLength();
        thick = box.selectedBassStep() == step;
        outlined = thick || (audio.isPlaying() && audio.bassStep.load() == step);
        if (step >= len) return off.withMultipliedBrightness (0.6f);
        const auto& s = data.inst[Bass].bassSteps[(size_t) step];
        lit = s.note >= 0;
        return s.note < 0 ? (step % 4 == 0 ? off.brighter (0.15f) : off) : s.accent ? C::stepAcc : col;
    }

    switch (page)
    {
        case Drums:
        {
            outlined = data.selectedDrum == pad;
            const float lvl = juce::jmax (flashLevel[(size_t) pad], pressed ? 1.0f : 0.0f);
            lit = lvl > 0.3f;
            return off.interpolatedWith (col, 0.22f + 0.78f * lvl);
        }
        case Arp:
        {
            const int note = box.arpPadNote (pad);
            const uint64_t bits = note < 64 ? audio.arpNotesLo.load() : audio.arpNotesHi.load();
            lit = pressed || ((bits >> (note & 63)) & 1u) != 0;
            const bool root = pad % (int) scaleIntervals (juce::roundToInt (data.globals[GScale])).size() == 0;
            return lit ? col : off.interpolatedWith (col, root ? 0.38f : 0.16f);
        }
        case Bass:
        default:
        {
            const int note = box.bassPadNote (pad) + 12 * juce::roundToInt (data.inst[Bass].values[slot::BOctave]);
            lit = pressed || audio.bassNoteNow.load() == note;
            const bool root = pad % (int) scaleIntervals (juce::roundToInt (data.globals[GScale])).size() == 0;
            return lit ? col : off.interpolatedWith (col, root ? 0.38f : 0.16f);
        }
    }
}

juce::String BoxView::padLabel (int pad) const
{
    const int page = box.getPage();
    const auto& data = box.getData();
    if (box.isShiftHeld()) return juce::String (pad + 1);
    if (box.isStepView())
    {
        if (page == Arp)
        {
            const auto st = data.inst[Arp].arpSteps[(size_t) pad];
            return juce::String (pad + 1) + (st == StepRest ? " -" : st == StepTie ? " ~" : st == StepAccent ? " !" : "");
        }
        const int step = juce::jmin (box.stepPage (page), box.numStepPages (page) - 1) * numPads + pad;
        if (page == Drums) return juce::String (step + 1);
        const auto& s = data.inst[Bass].bassSteps[(size_t) juce::jmin (step, maxSteps - 1)];
        if (s.note < 0) return juce::String (step + 1);
        return noteName (s.note) + (s.slide ? " ~" : "") + (s.accent ? " !" : "");
    }
    if (page == Drums) return drumPadName (pad);
    if (page == Arp) return noteName (box.arpPadNote (pad));
    return noteName (box.bassPadNote (pad));
}

void BoxView::paintPad (juce::Graphics& g, juce::Rectangle<float> r, int pad, bool big)
{
    bool lit = false, outlined = false, thick = false;
    const auto c = padColour (pad, lit, outlined, thick);
    if (lit && big)
    {
        g.setColour (c.withAlpha (0.3f));
        g.fillRoundedRectangle (r.expanded (3.0f), 9.0f);
    }
    g.setColour (c);
    g.fillRoundedRectangle (r, big ? 7.0f : 4.0f);
    if (outlined)
    {
        g.setColour (juce::Colours::white.withAlpha (thick ? 1.0f : 0.8f));
        g.drawRoundedRectangle (r.reduced (1.0f), big ? 7.0f : 4.0f, (big ? 2.0f : 1.4f) * (thick ? 1.8f : 1.0f));
    }
    if (! big) return;

    g.setColour (lit ? juce::Colours::black.withAlpha (0.75f) : C::text.withAlpha (0.75f));
    g.setFont (font (12.5f, true));
    g.drawText (padLabel (pad), r.reduced (4.0f), juce::Justification::centredBottom);
    g.setFont (font (10.0f));
    g.setColour (C::dim.withAlpha (lit ? 0.0f : 0.8f));
    g.drawText (juce::String::charToString ((juce::juce_wchar) padKeys[pad]), r.reduced (5.0f, 3.0f), juce::Justification::topLeft);
}

//==============================================================================
void BoxView::paint (juce::Graphics& g)
{
    g.fillAll (pal().bg);
    g.addTransform (juce::AffineTransform::scale (scale).translated (offset));
    paintScreen (g, { 0.0f, 0.0f, (float) screenW, (float) screenH });
    if (! kiosk) paintPanel (g, { 0.0f, (float) screenH, (float) screenW, (float) panelH });
}

void BoxView::paintScreen (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& P = pal();
    const int page = box.getPage();
    const auto col = instrumentColour (page);
    const auto& data = box.getData();
    auto& audio = box.getAudio();

    g.setColour (P.screen);
    g.fillRect (area);

    // header: instrument, the three pages, play state, tempo, key
    auto header = area.removeFromTop (64.0f).reduced (22.0f, 0.0f);
    g.setColour (col);
    g.setFont (font (38.0f, true));
    g.drawText (instrumentName (page).toUpperCase(), header.removeFromLeft (170.0f), juce::Justification::centredLeft);

    auto tabs = header.removeFromLeft (230.0f);
    for (int i = 0; i < NumInstruments; ++i)
    {
        auto t = tabs.removeFromLeft (76.0f);
        const bool active = i == page;
        g.setColour (data.muted[(size_t) i] ? C::rec : active ? instrumentColour (i) : P.dim);
        g.setFont (font (15.0f, active));
        g.drawText (instrumentName (i) + (data.muted[(size_t) i] ? " M" : ""), t, juce::Justification::centred);
        if (active) { g.setColour (instrumentColour (i)); g.fillRect (t.getCentreX() - 18.0f, t.getBottom() - 16.0f, 36.0f, 3.0f); }
    }

    {
        auto right = header;
        const float cy = right.getCentreY();
        g.setFont (font (16.0f));
        g.setColour (P.dim);
        g.drawText (keyNames()[juce::roundToInt (data.globals[GKey])] + " " + scaleNames()[juce::roundToInt (data.globals[GScale])].toLowerCase(),
                    right.removeFromRight (150.0f), juce::Justification::centredRight);
        g.setFont (font (22.0f, true));
        g.setColour (P.text);
        g.drawText (juce::String (juce::roundToInt (data.globals[GTempo])) + " BPM", right.removeFromRight (110.0f),
                    juce::Justification::centredRight);
        const float x = right.getRight() - 30.0f;
        juce::Path p;
        if (audio.isPlaying()) p.addTriangle (x, cy - 9.0f, x, cy + 9.0f, x + 15.0f, cy);
        else p.addRectangle (x, cy - 7.0f, 14.0f, 14.0f);
        g.setColour (audio.isPlaying() ? C::stepOn : P.dim);
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
    if (box.isShiftHeld()) chip ("SHIFT", P.dim);
    if (box.isStepView()) chip ("STEPS", C::stepOn);
    if (audio.isRecording() && page != Arp) chip ("REC", C::rec);
    if (box.isLatched() && page == Arp) chip ("HOLD", instrumentColour (Arp));
    if (data.muted[(size_t) page]) chip ("MUTED", C::rec);
    if (data.loadedPreset[(size_t) page] >= 0) chip ("PRESET " + juce::String (data.loadedPreset[(size_t) page] + 1), col);
    if (page == Drums) chip (drumPadName (data.selectedDrum).toUpperCase(), col);

    // the 12 knobs (the Shift layer while Shift is held)
    auto knobs = area.removeFromTop (240.0f).reduced (10.0f, 0.0f);
    for (int row = 0; row < 2; ++row)
    {
        auto r = knobs.removeFromTop (120.0f);
        const float w = r.getWidth() / 6.0f;
        for (int c = 0; c < 6; ++c)
            paintKnobCell (g, r.removeFromLeft (w).reduced (4.0f), row * 6 + c);
    }

    paintBottom (g, area.reduced (10.0f, 6.0f));
}

void BoxView::paintKnobCell (juce::Graphics& g, juce::Rectangle<float> r, int knob)
{
    const auto& P = pal();
    const int page = box.getPage();
    const auto v = box.knobView (knob);
    g.setColour (v.stepEdit ? P.cell.interpolatedWith (instrumentColour (page), 0.12f) : P.cell);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (P.dim.withAlpha (0.6f));
    g.setFont (font (11.0f));
    g.drawText (juce::String (knob + 1), r.reduced (7.0f, 5.0f), juce::Justification::topLeft);
    if (! v.used)
    {
        g.setColour (P.faint);
        g.setFont (font (22.0f));
        g.drawText ("-", r, juce::Justification::centred);
        return;
    }

    const auto col = v.global ? P.dim : instrumentColour (page);
    const juce::Point<float> c { r.getCentreX(), r.getY() + 38.0f };
    constexpr float a0 = -2.35f, a1 = 2.35f;
    drawArc (g, c, 24.0f, a0, a1, 5.0f, P.faint);
    const float frac = juce::jlimit (0.0f, 1.0f, v.fraction);
    if (frac > 0.001f) drawArc (g, c, 24.0f, a0, a0 + (a1 - a0) * frac, 5.0f, col);

    g.setColour (P.dim);
    g.setFont (font (14.0f));
    g.drawFittedText (v.name, r.withTrimmedTop (66.0f).withHeight (18.0f).toNearestInt(), juce::Justification::centred, 1, 0.75f);
    g.setColour (P.text);
    g.setFont (font (17.0f, true));
    g.drawFittedText (v.value, r.withTrimmedTop (84.0f).withHeight (24.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void BoxView::paintStepRow (juce::Graphics& g, juce::Rectangle<float> bar, int instrument)
{
    const auto& P = pal();
    const auto& data = box.getData();
    auto& audio = box.getAudio();
    const auto col = instrumentColour (instrument);
    const int len = instrument == Drums ? data.drumLength() : data.bassLength();
    const int shown = instrument == Drums ? len : juce::jmax (16, ((len + 15) / 16) * 16);
    const int cur = audio.isPlaying() ? (instrument == Drums ? audio.drumStep.load() : audio.bassStep.load()) : -1;
    const int page = juce::jmin (box.stepPage (instrument), box.numStepPages (instrument) - 1);
    const float w = bar.getWidth() / shown;

    if (box.isStepView() && box.getPage() == instrument)   // the 16 steps the pads show now
    {
        g.setColour (col.withAlpha (0.18f));
        g.fillRoundedRectangle (bar.withX (bar.getX() + page * 16 * w).withWidth (16 * w).expanded (0.0f, 3.0f), 4.0f);
    }
    for (int s = 0; s < shown; ++s)
    {
        auto b = bar.withX (bar.getX() + s * w).withWidth (w - (shown > 32 ? 1.0f : 2.0f));
        juce::Colour c = P.faint;
        if (s >= len) c = P.faint.withAlpha (0.35f);
        else if (instrument == Drums)
        {
            const int v = data.inst[Drums].drumSteps[(size_t) data.selectedDrum][(size_t) s];
            if (v > 0) c = v >= 120 ? col.brighter (0.3f) : col;
        }
        else
        {
            const auto& st = data.inst[Bass].bassSteps[(size_t) s];
            if (st.note >= 0)
            {
                c = st.accent ? C::stepAcc : col;
                // note height inside the box: a tiny melody line
                const float hgt = juce::jmap ((float) juce::jlimit (24, 72, st.note), 24.0f, 72.0f, 0.15f, 1.0f);
                g.setColour (c.withAlpha (0.35f));
                g.fillRoundedRectangle (b, 2.0f);
                b = b.withTrimmedTop (b.getHeight() * (1.0f - hgt));
            }
        }
        g.setColour (c);
        g.fillRoundedRectangle (b, 2.0f);
        if (instrument == Bass && s < len && data.inst[Bass].bassSteps[(size_t) s].slide)
        {
            g.setColour (C::stepTie);
            g.fillRect (b.getRight() - 2.0f, bar.getY() - 4.0f, w, 3.0f);
        }
        if (s == cur)
        {
            g.setColour (P.text);
            g.drawRoundedRectangle (bar.withX (bar.getX() + s * w).withWidth (w - 1.0f), 2.0f, 1.6f);
        }
    }
}

void BoxView::paintScope (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& P = pal();
    auto& audio = box.getAudio();
    g.setColour (P.cell);
    g.fillRoundedRectangle (r, 6.0f);
    juce::Path p;
    const int n = (int) audio.scope.size();
    for (int i = 0; i < n; ++i)
    {
        const float v = juce::jlimit (-1.0f, 1.0f, audio.scope[(size_t) i].load (std::memory_order_relaxed) * 2.0f);
        const float x = r.getX() + 4.0f + (r.getWidth() - 8.0f) * i / (n - 1);
        const float y = r.getCentreY() - v * (r.getHeight() * 0.42f);
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (instrumentColour (box.getPage()));
    g.strokePath (p, juce::PathStrokeType (1.6f));
    const float pk = juce::jlimit (0.0f, 1.0f, audio.peak.load());
    g.setColour (pk > 0.85f ? C::rec : C::stepOn);
    g.fillRect (r.getX() + 4.0f, r.getBottom() - 5.0f, (r.getWidth() - 8.0f) * pk, 2.5f);
}

void BoxView::paintBottom (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& P = pal();
    const int page = box.getPage();
    const auto col = instrumentColour (page);
    const auto& data = box.getData();
    auto& audio = box.getAudio();

    // mini pad map
    auto mini = r.removeFromLeft (140.0f).reduced (8.0f, 2.0f);
    const float cell = juce::jmin (mini.getWidth(), mini.getHeight()) / 4.0f;
    for (int p = 0; p < numPads; ++p)
        paintPad (g, { mini.getX() + (p % 4) * cell, mini.getY() + (p / 4) * cell, cell - 4.0f, cell - 4.0f }, p, false);

    // sound picture
    paintScope (g, r.removeFromRight (150.0f).reduced (4.0f, 8.0f));

    auto info = r.reduced (10.0f, 4.0f);
    auto line1 = info.removeFromTop (34.0f);
    g.setFont (font (19.0f, true));
    g.setColour (P.text);
    const auto& iv = data.inst[(size_t) page].values;
    auto hint = [&g, &P] (juce::Rectangle<float> rr, const juce::String& s)
    {
        g.setFont (font (14.0f));
        g.setColour (P.dim);
        g.drawFittedText (s, rr.toNearestInt(), juce::Justification::centredLeft, 2, 0.8f);
    };

    if (page == Drums)
    {
        g.drawText (drumPadName (data.selectedDrum) + "  |  " + paramDesc (Drums, slot::DKit).format (iv[slot::DKit])
                      + "  |  " + juce::String (data.drumLength()) + " steps",
                    line1, juce::Justification::centredLeft);
        paintStepRow (g, info.removeFromTop (36.0f).reduced (0.0f, 7.0f), Drums);
        hint (info, box.isStepView() ? "Pads = steps of " + drumPadName (data.selectedDrum) + " (tap: on > accent > off). Drum -/+ picks the drum."
                                     : juce::String ("Pads play. Steps = draw the beat, Rec = record what you play."));
    }
    else if (page == Arp)
    {
        g.drawText (paramDesc (Arp, slot::AMode).format (iv[slot::AMode]) + "  |  " + paramDesc (Arp, slot::ADivision).format (iv[slot::ADivision])
                      + "  |  " + juce::String (juce::roundToInt (iv[slot::AOctaves])) + " oct  |  "
                      + paramDesc (Arp, slot::ASound).format (iv[slot::ASound]),
                    line1, juce::Justification::centredLeft);
        const int len = juce::roundToInt (iv[slot::ALength]);
        const int cur = audio.arpStep.load();
        auto bar = info.removeFromTop (36.0f).reduced (0.0f, 5.0f);
        const float w = bar.getWidth() / 16.0f;
        for (int s = 0; s < 16; ++s)
        {
            const auto st = data.inst[Arp].arpSteps[(size_t) s];
            auto c = st == StepOn ? C::stepOn : st == StepTie ? C::stepTie : st == StepAccent ? C::stepAcc : P.faint;
            if (s >= len) c = c.withAlpha (0.3f);
            auto b = bar.withX (bar.getX() + s * w).withWidth (w - 3.0f);
            g.setColour (c);
            g.fillRoundedRectangle (b, 3.0f);
            if (s == cur) { g.setColour (P.text); g.drawRoundedRectangle (b, 3.0f, 2.0f); }
        }
        hint (info, box.isStepView() ? "Pads = the 16 steps: green plays, blue ties, orange accents, dark rests."
                                     : juce::String ("Hold notes on the pads, the arp plays them. Rec = Hold."));
    }
    else
    {
        const int now = audio.bassNoteNow.load();
        g.drawText (paramDesc (Bass, slot::BSound).format (iv[slot::BSound]) + "  |  " + juce::String (data.bassLength()) + " steps"
                      + (now >= 0 ? "  |  " + noteName (now) : juce::String()),
                    line1, juce::Justification::centredLeft);
        paintStepRow (g, info.removeFromTop (36.0f).reduced (0.0f, 7.0f), Bass);
        hint (info, box.isStepView() ? juce::String ("Tap a step to set it, tap again to pick, once more to clear. Knobs 2-4: note, slide, accent.")
                                     : juce::String ("Pads play the scale. Rec records your line, hold two pads for a slide."));
    }

    const auto flash = box.getFlash();
    if (flash.isNotEmpty())
    {
        auto f = r.reduced (16.0f, 30.0f);
        g.setColour (P.screen.withAlpha (0.95f));
        g.fillRoundedRectangle (f, 10.0f);
        g.setColour (col);
        g.drawRoundedRectangle (f, 10.0f, 2.0f);
        g.setColour (P.text);
        g.setFont (font (19.0f, true));
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

void BoxView::paintPanel (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (C::body);
    g.fillRect (area);
    g.setColour (C::bodyEdge);
    g.drawLine (area.getX(), area.getY() + 1.0f, area.getRight(), area.getY() + 1.0f, 2.0f);

    const int page = box.getPage();
    const auto col = instrumentColour (page);

    for (int k = 0; k < numKnobs; ++k)
    {
        auto r = knobArea (k);
        const juce::Point<float> c { r.getCentreX(), r.getY() + 28.0f };
        g.setColour (juce::Colour (0xff0e1014));
        g.fillEllipse (c.x - 25.0f, c.y - 25.0f, 50.0f, 50.0f);
        g.setColour (C::bodyEdge.brighter (0.4f));
        g.drawEllipse (c.x - 25.0f, c.y - 25.0f, 50.0f, 50.0f, 1.5f);
        const auto v = box.knobView (k);
        if (v.used)
        {
            const float ang = -2.35f + 4.7f * juce::jlimit (0.0f, 1.0f, v.fraction);
            g.setColour (v.global ? C::global : col);
            g.drawLine (c.x + std::sin (ang) * 8.0f, c.y - std::cos (ang) * 8.0f,
                        c.x + std::sin (ang) * 21.0f, c.y - std::cos (ang) * 21.0f, 3.0f);
        }
        g.setColour (C::dim);
        g.setFont (font (12.5f));
        g.drawFittedText (v.used ? v.name : juce::String ("-"), r.withTrimmedTop (56.0f).toNearestInt(), juce::Justification::centredTop, 1, 0.75f);
    }

    for (int p = 0; p < numPads; ++p) paintPad (g, padArea (p), p, true);

    const char* keyHint[NumButtons] = { "Space", "B", "Shift", "T", "Tab", ",", ".", "Enter" };
    for (int b = 0; b < NumButtons; ++b)
    {
        auto r = buttonArea (b);
        const bool lit = box.isButtonLit (b) || (b == BtnShift && mouseShiftLatched);
        const auto litCol = b == BtnRec ? (page == Arp ? col : C::rec) : b == BtnPlay ? C::stepOn : b == BtnSteps ? C::stepOn : C::global;
        g.setColour (lit ? litCol : juce::Colour (0xff2b2f39));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (lit ? juce::Colours::black : C::text);
        g.setFont (font (16.0f, true));
        g.drawText (box.buttonLabel (b), r.withTrimmedBottom (12.0f), juce::Justification::centred);
        g.setFont (font (11.0f));
        g.setColour (lit ? juce::Colours::black.withAlpha (0.6f) : C::dim);
        g.drawText (keyHint[b], r.withTrimmedTop (34.0f), juce::Justification::centred);
    }

    g.setColour (C::dim.withAlpha (0.7f));
    g.setFont (font (12.0f));
    g.drawText ("Knobs: drag or scroll  |  Shift + pad = load, Shift + Rec + pad = save  |  right-click = settings",
                juce::Rectangle<float> (20.0f, area.getBottom() - 24.0f, 760.0f, 20.0f), juce::Justification::centred);
}

//==============================================================================
void BoxView::mouseDown (const juce::MouseEvent& e)
{
    const auto p = toLogical (e.position);
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Sound card and MIDI...");
        m.addItem (2, "Full screen (F11)");
        m.addItem (4, "Light screen", true, box.isLightScreen());
        m.addItem (3, "Open the presets folder");
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
        {
            if (r == 1 && onSettings) onSettings();
            if (r == 2 && onToggleFullScreen) onToggleFullScreen();
            if (r == 4) box.setLightScreen (! box.isLightScreen());
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
    if (drag == Drag::Button)
    {
        box.buttonUp (dragIndex);
        if (mouseShiftLatched && dragIndex != BtnRec) { mouseShiftLatched = false; box.buttonUp (BtnShift); }
    }
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

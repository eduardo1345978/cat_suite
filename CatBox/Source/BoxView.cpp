#include "BoxView.h"
#include "MasterFx.h"
#include <cmath>

namespace catbox
{
namespace
{
const BoxView::Palette darkScreen  { juce::Colour (0xff0b0d12), juce::Colour (0xff10131a), juce::Colour (0xff171b24),
                                     juce::Colour (0xffe8eaf0), juce::Colour (0xff858b9b), juce::Colour (0xff2a2e38) };
const BoxView::Palette lightScreen { juce::Colour (0xff0b0d12), juce::Colour (0xfff4f5f8), juce::Colour (0xffffffff),
                                     juce::Colour (0xff1b1e26), juce::Colour (0xff666d80), juce::Colour (0xffd9dce4) };

namespace C
{
    // the front panel is the black 3D-printed case: always dark
    const juce::Colour body     { 0xff17191f };
    const juce::Colour bodyEdge { 0xff2a2e38 };
    const juce::Colour padOff   { 0xff262a33 };
    const juce::Colour text     { 0xffe8eaf0 };
    const juce::Colour dim      { 0xff858b9b };
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

// pads in two rows of 8: top row 1-8, bottom row Q-I
const char padKeys[numPads] = { '1', '2', '3', '4', '5', '6', '7', '8', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I' };

bool keyDown (int c)
{
    return juce::KeyPress::isKeyCurrentlyDown (c)
        || (c >= 'A' && c <= 'Z' && juce::KeyPress::isKeyCurrentlyDown (c + ('a' - 'A')));
}

bool padKeyDown (int pad)
{
    const int c = padKeys[pad];
    return keyDown (c) || (c == 'Y' && keyDown ('Z'));   // Slovak / German keyboards swap Z and Y
}

int buttonKey (int b)
{
    switch (b)
    {
        case BtnPlay:       return juce::KeyPress::spaceKey;
        case BtnSteps:      return 'B';
        case BtnScene:      return 'N';
        case BtnInstrument: return juce::KeyPress::tabKey;
        case BtnOctDown:    return ',';
        case BtnOctUp:      return '.';
        case BtnRec:        return juce::KeyPress::returnKey;
        case BtnChaos:      return 'C';
        default:            return 0;   // Shift: modifierKeysChanged
    }
}

const char* buttonKeyHint (int b)
{
    static const char* hints[NumButtons] = { "Space", "B", "Shift", "N", "Tab", ",", ".", "Enter", "C" };
    return hints[juce::jlimit (0, NumButtons - 1, b)];
}

void drawArc (juce::Graphics& g, juce::Point<float> c, float r, float from, float to, float thickness, juce::Colour col)
{
    juce::Path p;
    p.addCentredArc (c.x, c.y, r, r, 0.0f, from, to, true);
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

juce::Colour BoxView::pageColour (int i)
{
    switch (i)
    {
        case Drums: return juce::Colour (0xffe8894a);
        case Arp:   return juce::Colour (0xff33c2a2);
        case Bass:  return juce::Colour (0xff9b7cec);
        default:    return juce::Colour (0xff4fa3e8);
    }
}

BoxView::BoxView (BoxController& b, bool kioskMode) : box (b), kiosk (kioskMode)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    if (kiosk) setMouseCursor (juce::MouseCursor::NoCursor);
    fftData.assign ((size_t) (2 << fftOrder), 0.0f);
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

void BoxView::updateSpectrum()
{
    auto& audio = box.getAudio();
    const int size = 1 << fftOrder;
    const int w = audio.spectrumWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < size; ++i)
    {
        const float win = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) (size - 1));
        fftData[(size_t) i] = audio.spectrumRing[(size_t) ((w + i) % BoxAudio::spectrumSize)].load (std::memory_order_relaxed) * win;
    }
    std::fill (fftData.begin() + size, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    const double rate = audio.getSampleRate() * 0.5;   // the ring holds every second sample
    const double binHz = rate / size;
    for (int b = 0; b < numBands; ++b)
    {
        const double f0 = 40.0 * std::pow (11000.0 / 40.0, (double) b / numBands);
        const double f1 = 40.0 * std::pow (11000.0 / 40.0, (double) (b + 1) / numBands);
        const int i0 = juce::jlimit (1, size / 2 - 1, (int) (f0 / binHz)), i1 = juce::jlimit (i0 + 1, size / 2, (int) (f1 / binHz) + 1);
        float m = 0.0f;
        for (int i = i0; i < i1; ++i) m = juce::jmax (m, fftData[(size_t) i]);
        const float db = juce::Decibels::gainToDecibels (m / (float) size * 4.0f, -80.0f);
        const float v = juce::jlimit (0.0f, 1.0f, (db + 72.0f) / 66.0f);
        bands[(size_t) b] = v > bands[(size_t) b] ? v : bands[(size_t) b] * 0.86f + v * 0.14f;
    }
}

void BoxView::timerCallback()
{
    const uint32_t f = box.getAudio().drumFlash.exchange (0);
    for (int p = 0; p < numPads; ++p)
    {
        if ((f >> p) & 1u) flashLevel[(size_t) p] = 1.0f;
        else flashLevel[(size_t) p] *= 0.78f;
    }
    updateSpectrum();
    if (! hasKeyboardFocus (true) && isShowing() && getPeer() != nullptr && getPeer()->isFocused()) grabKeyboardFocus();
    repaint();
}

//==============================================================================
juce::Colour BoxView::padColour (int pad, bool& lit, bool& outlined, bool& thick) const
{
    lit = outlined = thick = false;
    const int page = box.getPage();
    const auto col = pageColour (page);
    const auto& data = box.getData();
    const auto off = C::padOff;
    const bool pressed = ((pressedPads >> pad) & 1u) != 0;
    auto& audio = box.getAudio();

    if (box.isSceneMode())
    {
        const auto sc = juce::Colour (0xfff0c94a);
        outlined = data.loadedScene == pad;
        if (box.sceneExists (pad)) { lit = outlined; return outlined ? sc : sc.withMultipliedBrightness (0.5f); }
        return off;
    }
    if (box.isShiftHeld())
    {
        outlined = data.loadedPreset[(size_t) page] == pad;
        if (box.presetExists (page, pad)) { lit = outlined; return outlined ? col : col.withMultipliedBrightness (0.55f); }
        return off;
    }
    if (page == PageFx)
    {
        lit = pressed || ((audio.perfHeld.load() >> pad) & 1u) != 0;
        return lit ? col : off.interpolatedWith (col, pad < 4 || pad == PerfRepeat2 ? 0.3f : 0.16f);
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
    if (box.isSceneMode()) return "Scene " + juce::String (pad + 1);
    if (box.isShiftHeld()) return juce::String (pad + 1);
    if (page == PageFx) return perfName (pad);
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

juce::String BoxView::presetTitle() const
{
    const auto& data = box.getData();
    const int page = box.getPage();
    if (box.isSceneMode()) return data.loadedScene >= 0 ? "Scene " + juce::String (data.loadedScene + 1) : juce::String ("Scenes");
    if (data.loadedPreset[(size_t) page] >= 0) return "Preset " + juce::String (data.loadedPreset[(size_t) page] + 1);
    if (data.loadedScene >= 0) return "Scene " + juce::String (data.loadedScene + 1);
    return {};
}

void BoxView::paintPad (juce::Graphics& g, juce::Rectangle<float> r, int pad)
{
    bool lit = false, outlined = false, thick = false;
    const auto c = padColour (pad, lit, outlined, thick);
    if (lit)
    {
        g.setColour (c.withAlpha (0.28f));
        g.fillRoundedRectangle (r.expanded (3.0f), 10.0f);
    }
    g.setColour (c);
    g.fillRoundedRectangle (r, 8.0f);
    if (outlined)
    {
        g.setColour (juce::Colours::white.withAlpha (thick ? 1.0f : 0.8f));
        g.drawRoundedRectangle (r.reduced (1.0f), 8.0f, thick ? 3.6f : 2.0f);
    }
    g.setColour (lit ? juce::Colours::black.withAlpha (0.75f) : C::text.withAlpha (0.75f));
    g.setFont (font (12.5f, true));
    g.drawFittedText (padLabel (pad), r.reduced (5.0f).toNearestInt(), juce::Justification::centredBottom, 2, 0.8f);
    g.setFont (font (10.0f));
    g.setColour (C::dim.withAlpha (lit ? 0.0f : 0.8f));
    g.drawText (juce::String::charToString ((juce::juce_wchar) padKeys[pad]), r.reduced (6.0f, 4.0f), juce::Justification::topLeft);
}

//==============================================================================
void BoxView::paint (juce::Graphics& g)
{
    g.fillAll (pal().bg);
    g.addTransform (juce::AffineTransform::scale (scale).translated (offset));
    if (isBoxScreen()) paintBoxScreen (g, { 0.0f, 0.0f, (float) screenW, (float) screenH });
    else paintScreen (g, { 0.0f, 0.0f, (float) screenW, (float) screenH });
    if (! kiosk) paintPanel (g, { 0.0f, (float) screenH, (float) screenW, (float) panelH });
}

juce::Rectangle<float> BoxView::tabArea (int p) const
{
    return { 170.0f + p * 62.0f, 12.0f, 60.0f, 40.0f };
}

void BoxView::paintScreen (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& P = pal();
    const int page = box.getPage();
    const auto col = pageColour (page);
    const auto& data = box.getData();
    auto& audio = box.getAudio();

    g.setColour (P.screen);
    g.fillRect (area);

    // header: page name, the four pages, preset / scene, play + tempo
    g.setColour (col);
    g.setFont (font (34.0f, true));
    g.drawText (instrumentName (page).toUpperCase(), juce::Rectangle<float> (20.0f, 0.0f, 150.0f, 64.0f), juce::Justification::centredLeft);
    for (int i = 0; i < NumPages; ++i)
    {
        const auto t = tabArea (i);
        const bool active = i == page;
        const bool muted = i < NumInstruments && data.muted[(size_t) i];
        if (active) { g.setColour (pageColour (i).withAlpha (0.16f)); g.fillRoundedRectangle (t, 8.0f); }
        g.setColour (muted ? C::rec : active ? pageColour (i) : P.dim);
        g.setFont (font (15.0f, active));
        g.drawText (instrumentName (i), t, juce::Justification::centred);
    }
    g.setColour (P.text);
    g.setFont (font (22.0f, true));
    g.drawFittedText (presetTitle(), juce::Rectangle<int> (430, 0, 190, 64), juce::Justification::centred, 1, 0.8f);
    {
        const float cy = 32.0f, x = 640.0f;
        juce::Path p;
        if (audio.isPlaying()) p.addTriangle (x, cy - 10.0f, x, cy + 10.0f, x + 17.0f, cy);
        else p.addRectangle (x, cy - 8.0f, 16.0f, 16.0f);
        g.setColour (audio.isPlaying() ? C::stepOn : P.dim);
        g.fillPath (p);
        g.setColour (P.text);
        g.setFont (font (26.0f, true));
        g.drawText (juce::String (juce::roundToInt (data.globals[GTempo])), juce::Rectangle<float> (662.0f, 4.0f, 70.0f, 40.0f), juce::Justification::centredRight);
        g.setFont (font (12.0f));
        g.setColour (P.dim);
        g.drawText ("BPM", juce::Rectangle<float> (736.0f, 12.0f, 44.0f, 28.0f), juce::Justification::centredLeft);
        g.drawText (keyNames()[juce::roundToInt (data.globals[GKey])] + " " + scaleNames()[juce::roundToInt (data.globals[GScale])].toLowerCase(),
                    juce::Rectangle<float> (640.0f, 42.0f, 140.0f, 18.0f), juce::Justification::centredLeft);
    }

    // status: only what is switched on
    auto chips = juce::Rectangle<float> (20.0f, 66.0f, 760.0f, 22.0f);
    auto chip = [&g, &chips] (const juce::String& s, juce::Colour c)
    {
        const float w = 16.0f + s.length() * 8.0f;
        auto r = chips.removeFromLeft (w);
        chips.removeFromLeft (8.0f);
        g.setColour (c.withAlpha (0.2f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (c);
        g.setFont (font (12.5f, true));
        g.drawText (s, r, juce::Justification::centred);
    };
    if (box.isShiftHeld()) chip ("SHIFT", P.dim);
    if (box.isSceneMode()) chip ("SCENES", juce::Colour (0xfff0c94a));
    if (audio.sceneWaiting.load()) chip ("NEXT BAR", juce::Colour (0xfff0c94a));
    if (box.isStepView()) chip ("STEPS", C::stepOn);
    if (audio.isRecording() && page < NumInstruments && page != Arp) chip ("REC", C::rec);
    if (box.isLatched() && page == Arp) chip ("HOLD", pageColour (Arp));
    if (page < NumInstruments && data.muted[(size_t) page]) chip ("MUTED", C::rec);
    if (page == Drums) chip (drumPadName (data.selectedDrum).toUpperCase() + " v", col);

    // the 12 knobs
    for (int k = 0; k < numKnobs; ++k)
        paintKnobCell (g, juce::Rectangle<float> (14.0f + (k % 6) * 129.0f, 94.0f + (k / 6) * 118.0f, 129.0f, 118.0f).reduced (4.0f), k);

    paintBottom (g, { 14.0f, 336.0f, 772.0f, 136.0f });
}

juce::String BoxView::loadedName() const
{
    const auto& data = box.getData();
    const int page = box.getPage();
    if (box.isSceneMode() && data.loadedScene >= 0) return box.presetName (-1, data.loadedScene);
    if (data.loadedPreset[(size_t) page] >= 0)
    {
        const auto n = box.presetName (page, data.loadedPreset[(size_t) page]);
        if (n.isNotEmpty()) return n;
    }
    if (data.loadedScene >= 0) return box.presetName (-1, data.loadedScene);
    return {};
}

void BoxView::paintBoxScreen (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& P = pal();
    const int page = box.getPage();
    const auto col = pageColour (page);
    const auto& data = box.getData();
    auto& audio = box.getAudio();

    g.setColour (P.screen);
    g.fillRect (area);

    // top: what plays, which preset, tempo
    g.setColour (col);
    g.setFont (font (34.0f, true));
    g.drawText (instrumentName (page).toUpperCase(), juce::Rectangle<float> (24.0f, 6.0f, 200.0f, 56.0f), juce::Justification::centredLeft);
    const auto name = loadedName();
    g.setColour (name.isEmpty() ? P.dim : P.text);
    g.setFont (font (26.0f, true));
    g.drawFittedText (name.isEmpty() ? juce::String ("-") : name, juce::Rectangle<int> (220, 6, 380, 56), juce::Justification::centred, 1, 0.7f);
    {
        const float cy = 34.0f, x = 652.0f;
        juce::Path p;
        if (audio.isPlaying()) p.addTriangle (x, cy - 11.0f, x, cy + 11.0f, x + 19.0f, cy);
        else p.addRectangle (x, cy - 9.0f, 18.0f, 18.0f);
        g.setColour (audio.isPlaying() ? C::stepOn : P.dim);
        g.fillPath (p);
        g.setColour (P.text);
        g.setFont (font (30.0f, true));
        g.drawText (juce::String (juce::roundToInt (data.globals[GTempo])), juce::Rectangle<float> (676.0f, 10.0f, 70.0f, 48.0f), juce::Justification::centredRight);
        g.setFont (font (13.0f));
        g.setColour (P.dim);
        g.drawText ("BPM", juce::Rectangle<float> (750.0f, 22.0f, 40.0f, 24.0f), juce::Justification::centredLeft);
    }

    // a thin line of what is switched on
    juce::StringArray on;
    if (box.isShiftHeld()) on.add ("SHIFT");
    if (box.isStepView()) on.add ("STEPS");
    if (audio.isRecording() && page < NumInstruments && page != Arp) on.add ("REC");
    if (box.isLatched() && page == Arp) on.add ("HOLD");
    if (audio.sceneWaiting.load()) on.add ("NEXT BAR");
    if (page < NumInstruments && data.muted[(size_t) page]) on.add ("MUTED");
    if (page == Drums) on.add (drumPadName (data.selectedDrum).toUpperCase());
    g.setFont (font (14.0f, true));
    g.setColour (P.dim);
    g.drawText (on.joinIntoString ("   "), juce::Rectangle<float> (24.0f, 62.0f, 752.0f, 22.0f), juce::Justification::centredLeft);

    // the middle: presets / scenes while choosing, else the sound itself
    auto mid = juce::Rectangle<float> (16.0f, 90.0f, 768.0f, 300.0f);
    if (box.isShiftHeld() || box.isSceneMode()) paintSlotGrid (g, mid);
    else paintScope (g, mid);

    // a knob was turned: its name, a big number and a bar
    const bool recent = box.lastKnobPage() == page && box.lastKnobSlot() >= 0
                        && juce::Time::getMillisecondCounter() - box.lastKnobTime() < 1600;
    if (recent && ! box.isSceneMode())
    {
        const int knob = box.lastKnobSlot() % numKnobs;
        const bool stepKnob = page == Bass && box.isStepView() && box.lastKnobSlot() < numKnobs && knob >= 1 && knob <= 3;
        const auto v = stepKnob ? box.knobView (knob) : box.slotView (page, box.lastKnobSlot());
        if (v.used)
        {
            auto card = juce::Rectangle<float> (120.0f, 120.0f, 560.0f, 230.0f);
            g.setColour (P.cell.withAlpha (0.97f));
            g.fillRoundedRectangle (card, 16.0f);
            g.setColour (col.withAlpha (0.6f));
            g.drawRoundedRectangle (card, 16.0f, 2.0f);
            auto in = card.reduced (28.0f, 18.0f);
            g.setColour (P.dim);
            g.setFont (font (22.0f));
            g.drawText (v.name + "   (knob " + juce::String (knob + 1) + (box.lastKnobSlot() >= numKnobs ? " + Shift" : "") + ")",
                        in.removeFromTop (34.0f), juce::Justification::centredLeft);
            g.setColour (P.text);
            g.setFont (font (76.0f, true));
            g.drawFittedText (v.value, in.removeFromTop (100.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
            auto bar = in.removeFromBottom (22.0f);
            g.setColour (P.faint);
            g.fillRoundedRectangle (bar, 11.0f);
            g.setColour (v.global ? P.dim : col);
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (22.0f, bar.getWidth() * juce::jlimit (0.0f, 1.0f, v.fraction))), 11.0f);
        }
    }

    paintStrip (g, { 16.0f, 400.0f, 768.0f, 66.0f });

    const auto flash = box.getFlash();
    if (flash.isNotEmpty() && ! recent)
    {
        g.setFont (font (20.0f, true));
        const float w = juce::jmin (740.0f, 60.0f + flash.length() * 10.5f);
        auto f = juce::Rectangle<float> (400.0f - w * 0.5f, 100.0f, w, 46.0f);
        g.setColour (P.cell.withAlpha (0.97f));
        g.fillRoundedRectangle (f, 23.0f);
        g.setColour (col);
        g.drawRoundedRectangle (f, 23.0f, 2.0f);
        g.setColour (P.text);
        g.drawFittedText (flash, f.reduced (16.0f, 2.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

void BoxView::paintScope (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& P = pal();
    auto& audio = box.getAudio();
    g.setColour (P.cell);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (P.faint);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 12.0f, r.getRight() - 12.0f);

    // the last ~40 ms of sound, started where the wave goes up through zero, so it stands still
    constexpr int n = BoxAudio::spectrumSize, shown = 420;
    std::array<float, n> x {};
    const int w = audio.spectrumWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) x[(size_t) i] = audio.spectrumRing[(size_t) ((w + i) % n)].load (std::memory_order_relaxed);
    int start = n - shown - 1;
    for (int i = n - shown - 2; i > 40; --i)
        if (x[(size_t) i] <= 0.0f && x[(size_t) i + 1] > 0.0f) { start = i; break; }
    float peakAbs = 0.02f;
    for (int i = 0; i < shown; ++i) peakAbs = juce::jmax (peakAbs, std::abs (x[(size_t) (start + i)]));
    const float gain = 0.42f * r.getHeight() / peakAbs;

    juce::Path wave;
    for (int i = 0; i < shown; ++i)
    {
        const float px = r.getX() + 14.0f + (r.getWidth() - 28.0f) * (float) i / (shown - 1);
        const float py = r.getCentreY() - x[(size_t) (start + i)] * gain;
        if (i == 0) wave.startNewSubPath (px, py); else wave.lineTo (px, py);
    }
    const auto col = pageColour (box.getPage());
    g.setColour (col.withAlpha (0.25f));
    g.strokePath (wave, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col);
    g.strokePath (wave, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // the level, as a thin bar on the right
    const float pk = juce::jlimit (0.0f, 1.0f, audio.peak.load());
    auto meter = juce::Rectangle<float> (r.getRight() - 12.0f, r.getY() + 14.0f, 4.0f, r.getHeight() - 28.0f);
    g.setColour (P.faint);
    g.fillRoundedRectangle (meter, 2.0f);
    g.setColour (pk > 0.9f ? C::rec : col);
    g.fillRoundedRectangle (meter.withTrimmedTop (meter.getHeight() * (1.0f - pk)), 2.0f);
}

void BoxView::paintSlotGrid (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& P = pal();
    const bool scenes = box.isSceneMode();
    const int page = box.getPage();
    const auto& data = box.getData();
    const auto col = scenes ? juce::Colour (0xfff0c94a) : pageColour (page);
    const int loaded = scenes ? data.loadedScene : data.loadedPreset[(size_t) page];

    g.setColour (P.text);
    g.setFont (font (18.0f, true));
    auto head = r.removeFromTop (30.0f);
    g.drawText (scenes ? juce::String ("Scenes: pad = play it, Rec + pad = save")
                       : "Presets " + instrumentName (page) + ": pad = load, Rec + pad = save",
                head, juce::Justification::centredLeft);
    if (linkInfo && ! scenes)
    {
        g.setFont (font (14.0f));
        g.setColour (P.dim);
        g.drawText (linkInfo(), head, juce::Justification::centredRight);
    }
    const float cw = r.getWidth() / 8.0f, ch = (r.getHeight() - 6.0f) / 2.0f;
    for (int i = 0; i < numPresetSlots; ++i)
    {
        auto c = juce::Rectangle<float> (r.getX() + (i % 8) * cw, r.getY() + 6.0f + (i / 8) * ch, cw, ch).reduced (4.0f);
        const auto n = box.presetName (scenes ? -1 : page, i);
        const bool isLoaded = i == loaded;
        g.setColour (isLoaded ? col : n.isNotEmpty() ? P.cell.interpolatedWith (col, 0.18f) : P.cell);
        g.fillRoundedRectangle (c, 10.0f);
        g.setColour (isLoaded ? juce::Colours::black.withAlpha (0.6f) : P.dim);
        g.setFont (font (14.0f, true));
        g.drawText (juce::String (i + 1), c.reduced (9.0f, 6.0f), juce::Justification::topLeft);
        g.setColour (isLoaded ? juce::Colours::black : n.isNotEmpty() ? P.text : P.faint);
        g.setFont (font (16.0f, n.isNotEmpty()));
        g.drawFittedText (n.isNotEmpty() ? n : juce::String ("empty"), c.reduced (8.0f, 10.0f).toNearestInt(), juce::Justification::centredBottom, 3, 0.75f);
    }
}

void BoxView::paintStrip (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& P = pal();
    const int page = box.getPage();
    const auto col = pageColour (page);
    const auto& data = box.getData();
    auto& audio = box.getAudio();

    if (page == PageFx)
    {
        juce::StringArray held;
        const uint32_t bits = audio.perfHeld.load();
        for (int p = 0; p < NumPerf; ++p) if ((bits >> p) & 1u) held.add (perfName (p));
        g.setColour (held.isEmpty() ? P.dim : col);
        g.setFont (font (22.0f, held.size() > 0));
        g.drawFittedText (held.isEmpty() ? juce::String ("hold a pad for an effect") : held.joinIntoString ("  +  "),
                          r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        return;
    }

    int len = 16, cur = -1;
    auto stepColour = [&] (int s) -> juce::Colour
    {
        if (page == Drums)
        {
            const int v = data.inst[Drums].drumSteps[(size_t) data.selectedDrum][(size_t) s];
            return v == 0 ? P.faint : v >= 120 ? col.brighter (0.3f) : col;
        }
        if (page == Bass)
        {
            const auto& st = data.inst[Bass].bassSteps[(size_t) s];
            return st.note < 0 ? P.faint : st.accent ? C::stepAcc : col;
        }
        const auto st = data.inst[Arp].arpSteps[(size_t) s];
        return st == StepOn ? C::stepOn : st == StepTie ? C::stepTie : st == StepAccent ? C::stepAcc : P.faint;
    };
    if (page == Drums) { len = data.drumLength(); cur = audio.isPlaying() ? audio.drumStep.load() : -1; }
    else if (page == Bass) { len = data.bassLength(); cur = audio.isPlaying() ? audio.bassStep.load() : -1; }
    else { len = juce::roundToInt (data.inst[Arp].values[slot::ALength]); cur = audio.arpStep.load(); }
    len = juce::jlimit (1, maxSteps, len);
    const float w = r.getWidth() / len;
    for (int s = 0; s < len; ++s)
    {
        auto b = r.withX (r.getX() + s * w).withWidth (w).reduced (len > 32 ? 0.6f : 2.0f, 10.0f);
        g.setColour (stepColour (s));
        g.fillRoundedRectangle (b, 3.0f);
        if (s == cur) { g.setColour (P.text); g.drawRoundedRectangle (b.expanded (1.0f, 4.0f), 3.0f, 2.0f); }
    }
}

void BoxView::paintKnobCell (juce::Graphics& g, juce::Rectangle<float> r, int knob)
{
    const auto& P = pal();
    const int page = box.getPage();
    const auto v = box.knobView (knob);
    g.setColour (v.stepEdit ? P.cell.interpolatedWith (pageColour (page), 0.14f) : P.cell);
    g.fillRoundedRectangle (r, 10.0f);
    if (! v.used) return;

    const auto col = v.global ? P.dim : pageColour (page);
    const juce::Point<float> c { r.getCentreX(), r.getY() + 40.0f };
    constexpr float a0 = -2.35f, a1 = 2.35f;
    drawArc (g, c, 25.0f, a0, a1, 6.0f, P.faint);
    const float frac = juce::jlimit (0.0f, 1.0f, v.fraction);
    if (frac > 0.001f) drawArc (g, c, 25.0f, a0, a0 + (a1 - a0) * frac, 6.0f, col);

    g.setColour (P.dim);
    g.setFont (font (13.5f));
    g.drawFittedText (v.name, r.withTrimmedTop (70.0f).withHeight (18.0f).toNearestInt(), juce::Justification::centred, 1, 0.75f);
    g.setColour (P.text);
    g.setFont (font (18.0f, true));
    g.drawFittedText (v.value, r.withTrimmedTop (87.0f).withHeight (24.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void BoxView::paintSteps (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& P = pal();
    const int page = box.getPage();
    const auto col = pageColour (page);
    const auto& data = box.getData();
    auto& audio = box.getAudio();
    const auto& iv = data.inst[(size_t) page].values;

    // one line saying what plays
    juce::String line;
    if (page == Drums)
        line = paramDesc (Drums, slot::DKit).format (iv[slot::DKit]) + "  |  " + juce::String (data.drumLength()) + " steps";
    else if (page == Arp)
        line = paramDesc (Arp, slot::ASound).format (iv[slot::ASound]) + "  |  "
             + (juce::roundToInt (iv[slot::AArpOn]) == 0 ? paramDesc (Arp, slot::AMode).format (iv[slot::AMode]) + " " + paramDesc (Arp, slot::ADivision).format (iv[slot::ADivision])
                                                           : juce::String ("synth (arp off)"));
    else
    {
        const int now = audio.bassNoteNow.load();
        line = paramDesc (Bass, slot::BSound).format (iv[slot::BSound]) + "  |  " + juce::String (data.bassLength()) + " steps"
             + (now >= 0 ? "  |  " + noteName (now) : juce::String());
    }
    g.setColour (P.text);
    g.setFont (font (17.0f, true));
    g.drawText (line, r.removeFromTop (28.0f), juce::Justification::centredLeft);

    if (page == Arp)
    {
        const int len = juce::roundToInt (iv[slot::ALength]);
        const int cur = audio.arpStep.load();
        auto bar = r.removeFromTop (44.0f).reduced (0.0f, 6.0f);
        const float w = bar.getWidth() / 16.0f;
        for (int s = 0; s < 16; ++s)
        {
            const auto st = data.inst[Arp].arpSteps[(size_t) s];
            auto c = st == StepOn ? C::stepOn : st == StepTie ? C::stepTie : st == StepAccent ? C::stepAcc : P.faint;
            if (s >= len) c = c.withAlpha (0.3f);
            auto b = bar.withX (bar.getX() + s * w).withWidth (w - 4.0f);
            g.setColour (c);
            g.fillRoundedRectangle (b, 4.0f);
            if (s == cur) { g.setColour (P.text); g.drawRoundedRectangle (b, 4.0f, 2.0f); }
        }
        // the notes the arp is playing
        juce::String held;
        const uint64_t lo = audio.arpNotesLo.load(), hi = audio.arpNotesHi.load();
        for (int n = 0; n < 128; ++n)
            if (((n < 64 ? lo >> n : hi >> (n - 64)) & 1u) != 0) held << noteName (n) << "  ";
        g.setColour (held.isEmpty() ? P.dim : col);
        g.setFont (font (15.0f, held.isNotEmpty()));
        g.drawText (held.isEmpty() ? juce::String ("hold notes on the pads") : held.trim(), r.removeFromTop (30.0f), juce::Justification::centredLeft);
        return;
    }

    const int len = page == Drums ? data.drumLength() : data.bassLength();
    const int shown = page == Drums ? len : juce::jmax (16, ((len + 15) / 16) * 16);
    const int cur = audio.isPlaying() ? (page == Drums ? audio.drumStep.load() : audio.bassStep.load()) : -1;
    const int sp = juce::jmin (box.stepPage (page), box.numStepPages (page) - 1);
    auto bar = r.removeFromTop (page == Drums ? 34.0f : 58.0f).reduced (0.0f, 4.0f);
    const float w = bar.getWidth() / shown;
    if (box.isStepView())   // the 16 steps the pads show now
    {
        g.setColour (col.withAlpha (0.16f));
        g.fillRoundedRectangle (bar.withX (bar.getX() + sp * 16 * w).withWidth (16 * w).expanded (0.0f, 3.0f), 4.0f);
    }
    for (int s = 0; s < shown; ++s)
    {
        auto b = bar.withX (bar.getX() + s * w).withWidth (w - (shown > 32 ? 1.0f : 2.5f));
        juce::Colour c = s >= len ? P.faint.withAlpha (0.35f) : P.faint;
        if (s < len && page == Drums)
        {
            const int v = data.inst[Drums].drumSteps[(size_t) data.selectedDrum][(size_t) s];
            if (v > 0) c = v >= 120 ? col.brighter (0.3f) : col;
        }
        else if (s < len)
        {
            const auto& st = data.inst[Bass].bassSteps[(size_t) s];
            if (st.note >= 0)
            {
                c = st.accent ? C::stepAcc : col;
                const float hgt = juce::jmap ((float) juce::jlimit (24, 72, st.note), 24.0f, 72.0f, 0.15f, 1.0f);
                g.setColour (c.withAlpha (0.25f));
                g.fillRoundedRectangle (b, 2.0f);
                b = b.withTrimmedTop (b.getHeight() * (1.0f - hgt));
                if (st.slide) { g.setColour (C::stepTie); g.fillRect (b.getRight() - 2.0f, b.getY(), w, 3.0f); }
            }
        }
        g.setColour (c);
        g.fillRoundedRectangle (b, 2.0f);
        if (s == cur)
        {
            g.setColour (P.text);
            g.drawRoundedRectangle (bar.withX (bar.getX() + s * w).withWidth (w - 1.0f), 2.0f, 1.6f);
        }
    }

    if (page == Drums)   // every drum at a glance: one thin line each
    {
        auto grid = r.reduced (0.0f, 4.0f);
        const float lh = grid.getHeight() / numPads;
        for (int p = 0; p < numPads; ++p)
            for (int s = 0; s < len; ++s)
                if (data.inst[Drums].drumSteps[(size_t) p][(size_t) s] > 0)
                {
                    g.setColour (p == data.selectedDrum ? col : P.dim.withAlpha (0.7f));
                    g.fillRect (grid.getX() + s * w, grid.getY() + p * lh, juce::jmax (1.0f, w - 1.0f), juce::jmax (1.0f, lh - 1.0f));
                }
    }
}

void BoxView::paintPerfGrid (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& P = pal();
    const auto col = pageColour (PageFx);
    const uint32_t held = box.getAudio().perfHeld.load();
    g.setColour (P.text);
    g.setFont (font (17.0f, true));
    g.drawText ("Hold a pad for an effect", r.removeFromTop (28.0f), juce::Justification::centredLeft);
    const float w = r.getWidth() / 8.0f, h = (r.getHeight() - 4.0f) / 2.0f;
    for (int p = 0; p < NumPerf; ++p)
    {
        auto b = juce::Rectangle<float> (r.getX() + (p % 8) * w, r.getY() + (p / 8) * h, w, h).reduced (2.5f);
        const bool on = ((held >> p) & 1u) != 0;
        g.setColour (on ? col : P.cell);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (on ? juce::Colours::black : P.dim);
        g.setFont (font (11.5f, on));
        g.drawFittedText (perfName (p), b.reduced (3.0f).toNearestInt(), juce::Justification::centred, 2, 0.8f);
    }
}

void BoxView::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& P = pal();
    g.setColour (P.cell);
    g.fillRoundedRectangle (r, 10.0f);
    auto inner = r.reduced (10.0f, 10.0f);
    const float w = inner.getWidth() / numBands;
    const auto col = pageColour (box.getPage());
    for (int b = 0; b < numBands; ++b)
    {
        const float hgt = juce::jmax (2.0f, inner.getHeight() * bands[(size_t) b]);
        g.setColour (col.withAlpha (0.35f + 0.65f * bands[(size_t) b]));
        g.fillRoundedRectangle (inner.getX() + b * w, inner.getBottom() - hgt, w - 1.5f, hgt, 1.5f);
    }
    const float pk = juce::jlimit (0.0f, 1.0f, box.getAudio().peak.load());
    if (pk > 0.9f) { g.setColour (C::rec); g.fillRoundedRectangle (r.getRight() - 14.0f, r.getY() + 6.0f, 8.0f, 8.0f, 4.0f); }
}

void BoxView::paintBottom (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& P = pal();
    auto left = r.removeFromLeft (510.0f);
    r.removeFromLeft (12.0f);
    paintSpectrum (g, r);

    if (box.getPage() == PageFx) paintPerfGrid (g, left);
    else paintSteps (g, left);

    const auto flash = box.getFlash();
    if (flash.isNotEmpty())
    {
        auto f = left.reduced (0.0f, 30.0f);
        g.setColour (P.screen.withAlpha (0.96f));
        g.fillRoundedRectangle (f, 10.0f);
        g.setColour (pageColour (box.getPage()));
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
    return { 20.0f + (knob % 6) * w, (float) screenH + 12.0f + (knob / 6) * 80.0f, w, 78.0f };
}

juce::Rectangle<float> BoxView::padArea (int pad) const
{
    const float w = 760.0f / 8.0f, h = 74.0f;
    return juce::Rectangle<float> { 20.0f + (pad % 8) * w, (float) screenH + 182.0f + (pad / 8) * h, w, h }.reduced (5.0f);
}

juce::Rectangle<float> BoxView::buttonArea (int b) const
{
    const float w = 760.0f / (float) NumButtons;
    return juce::Rectangle<float> { 20.0f + b * w, (float) screenH + 338.0f, w, 62.0f }.reduced (4.0f);
}

void BoxView::paintPanel (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (C::body);
    g.fillRect (area);
    g.setColour (C::bodyEdge);
    g.drawLine (area.getX(), area.getY() + 1.0f, area.getRight(), area.getY() + 1.0f, 2.0f);

    const int page = box.getPage();
    const auto col = pageColour (page);

    for (int k = 0; k < numKnobs; ++k)
    {
        auto r = knobArea (k);
        const juce::Point<float> c { r.getCentreX(), r.getY() + 27.0f };
        g.setColour (juce::Colour (0xff0e1014));
        g.fillEllipse (c.x - 24.0f, c.y - 24.0f, 48.0f, 48.0f);
        g.setColour (C::bodyEdge.brighter (0.4f));
        g.drawEllipse (c.x - 24.0f, c.y - 24.0f, 48.0f, 48.0f, 1.5f);
        const auto v = box.knobView (k);
        if (v.used)
        {
            const float ang = -2.35f + 4.7f * juce::jlimit (0.0f, 1.0f, v.fraction);
            g.setColour (v.global ? C::global : col);
            g.drawLine (c.x + std::sin (ang) * 8.0f, c.y - std::cos (ang) * 8.0f,
                        c.x + std::sin (ang) * 20.0f, c.y - std::cos (ang) * 20.0f, 3.0f);
        }
        g.setColour (C::dim);
        g.setFont (font (12.0f));
        g.drawFittedText (v.used ? v.name : juce::String ("-"), r.withTrimmedTop (54.0f).toNearestInt(), juce::Justification::centredTop, 1, 0.75f);
    }

    for (int p = 0; p < numPads; ++p) paintPad (g, padArea (p), p);

    for (int b = 0; b < NumButtons; ++b)
    {
        auto r = buttonArea (b);
        const bool lit = box.isButtonLit (b) || (b == BtnShift && mouseShiftLatched);
        const auto litCol = b == BtnRec ? (page == Arp ? col : C::rec) : b == BtnPlay || b == BtnSteps ? C::stepOn
                          : b == BtnScene ? juce::Colour (0xfff0c94a) : b == BtnChaos ? juce::Colour (0xffe0508a) : C::global;
        g.setColour (lit ? litCol : juce::Colour (0xff2b2f39));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (lit ? juce::Colours::black : C::text);
        g.setFont (font (15.0f, true));
        g.drawText (box.buttonLabel (b), r.withTrimmedBottom (14.0f), juce::Justification::centred);
        g.setFont (font (10.5f));
        g.setColour (lit ? juce::Colours::black.withAlpha (0.6f) : C::dim);
        g.drawText (buttonKeyHint (b), r.withTrimmedTop (30.0f), juce::Justification::centred);
    }

    g.setColour (C::dim.withAlpha (0.7f));
    g.setFont (font (12.0f));
    g.drawText ("Shift + pad = load preset, Shift + Rec + pad = save  |  Shift + Play = tap tempo  |  right-click = settings",
                juce::Rectangle<float> (20.0f, area.getBottom() - 30.0f, 760.0f, 20.0f), juce::Justification::centred);
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
        if (! kiosk) m.addItem (5, "Show the box screen (5\")", true, boxPreview);
        m.addItem (3, "Open the presets folder");
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
        {
            if (r == 1 && onSettings) onSettings();
            if (r == 2 && onToggleFullScreen) onToggleFullScreen();
            if (r == 4) box.setLightScreen (! box.isLightScreen());
            if (r == 5) setBoxScreen (! boxPreview);
            if (r == 3)
            {
                auto dir = box.getDataDirectory().getChildFile ("Presets");
                dir.createDirectory();
                dir.startAsProcess();
            }
        });
        return;
    }

    // the screen: the page names switch the page, the drum chip picks the drum
    for (int i = 0; i < NumPages; ++i)
        if (! isBoxScreen() && tabArea (i).contains (p)) { box.setPage (i); return; }
    if (! isBoxScreen() && box.getPage() == Drums && juce::Rectangle<float> (14.0f, 64.0f, 772.0f, 26.0f).contains (p))
    {
        juce::PopupMenu m;
        for (int d = 0; d < numPads; ++d) m.addItem (d + 1, drumPadName (d), true, d == box.selectedDrum());
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r) { if (r > 0) box.pickDrum (r - 1); });
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

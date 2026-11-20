#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr int designW = 1000, designH = 680;
const Colour bezelA (0xff26252b), bezelB (0xff111014), screenBg (0xff07081a), osd (0xfff2f2f2), dimOsd (0xff7f8399), vhsBlue (0xff2133d6);
const Rectangle<int> screenArea (24, 20, 952, 612);

/** text the way a VCR draws it: white with a hard black shadow; with "split" the colours come apart (a bad tape) */
void osdText (Graphics& g, const String& t, Rectangle<int> r, Justification j, float split = 0.0f, Colour c = osd)
{
    if (split > 0.5f)
    {
        g.setColour (Colour (0xffff2a4a).withAlpha (0.8f)); g.drawText (t, r.translated (-(int) split, 0), j);
        g.setColour (Colour (0xff2af0ff).withAlpha (0.8f)); g.drawText (t, r.translated ((int) split, 0), j);
    }
    g.setColour (Colours::black.withAlpha (0.8f)); g.drawText (t, r.translated (2, 2), j);
    g.setColour (c); g.drawText (t, r, j);
}
}

Colour chopui::fxColour (int fx)
{
    static const Colour c[] = { Colour (0xff3a3c50), Colour (0xffff3b6b), Colour (0xffff6b3b), Colour (0xffffb03b), Colour (0xffffe13b), Colour (0xffb6ff3b),
                                Colour (0xff3bffb0), Colour (0xff3be1ff), Colour (0xff3b8bff), Colour (0xff7a5bff), Colour (0xffd13bff), Colour (0xffbff8ff), Colour (0xff8a8a99) };
    return c[jlimit (0, 12, fx)];
}
using chopui::fxColour;

//==============================================================================
void Monitor::tick()
{
    ++frame;
    const int fx = processor.engine.nowFx.load();
    if (fx != chop::None) { labelFx = fx; labelFrames = 14; }
    else if (labelFrames > 0) --labelFrames;
    repaint();
}

void Monitor::paintContent (Graphics& g)
{
    const auto b = getLocalBounds();
    g.fillAll (screenBg);
    auto& e = processor.engine;

    // this bar: 16 columns, the current one lit, what played on each at the bottom
    const float cw = (float) b.getWidth() / 16.0f;
    const int now = e.step.load();
    for (int k = 0; k < 16; ++k)
    {
        const int x = (int) ((float) k * cw);
        if (k == now) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillRect (x, 0, (int) cw, b.getHeight()); }
        g.setColour (Colours::white.withAlpha (k % 4 == 0 ? 0.14f : 0.05f));
        g.fillRect (x, 0, 1, b.getHeight());
        const int v = e.barFx[(size_t) k].load();
        if ((v & 0xff) != chop::None)
        {
            g.setColour (fxColour (v & 0xff).withAlpha ((v >> 8) == chop::SrcLive ? 1.0f : 0.75f));
            g.fillRect (x + 2, b.getBottom() - 10, (int) cw - 4, 6);
        }
    }

    // the sound, newest on the right, coloured by the glitch that made it
    const int pos = e.peakPos.load();
    const int count = jmin (chop::Engine::peakCount, b.getWidth() / 2);
    const float mid = (float) b.getHeight() * 0.47f, hh = (float) b.getHeight() * 0.42f;
    for (int i = 0; i < count; ++i)
    {
        const int idx = ((pos - count + i) % chop::Engine::peakCount + chop::Engine::peakCount) % chop::Engine::peakCount;
        const float pk = jmin (1.0f, std::sqrt (e.peaks[(size_t) idx].load()));      // (square root: quiet parts still show)
        const int fx = e.peakFx[(size_t) idx].load() & 0xff;
        const float x = (float) b.getWidth() - (float) (count - i) * 2.0f;
        g.setColour (fx == chop::None ? Colour (0xffd8dcf0).withAlpha (0.75f) : fxColour (fx));
        g.fillRect (x, mid - pk * hh, 1.6f, jmax (1.0f, pk * hh * 2.0f));
    }

    // the big on-screen label: what glitch plays now
    if (labelFrames > 0)
    {
        static const char* labels[] = { "", "x 1/8", "x 1/16", "x 1/32", "x 1/64", ">>> ROLL", "<< REV", "[] STOP", "+12 PITCH", "-12 PITCH", "?? SCATTER", "** FREEZE", "-- MUTE" };
        g.setFont (cat::lcdFont (58.0f));
        const float split = labelFrames > 10 ? 4.0f : 2.0f;
        osdText (g, labels[jlimit (0, 12, labelFx)], b.withTrimmedBottom (14), Justification::centred, split, fxColour (labelFx).interpolatedWith (Colours::white, 0.45f));
    }
    g.setFont (cat::lcdFont (18.0f));
    const int src = e.nowSource.load();
    const char* srcName[] = { "", "SEQ", "CHAOS", "LIVE" };
    osdText (g, String ("CH ") + (src == 0 ? "--" : srcName[src]), b.reduced (8, 4), Justification::topLeft, 0.0f, dimOsd);
}

void Monitor::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    if (b.isEmpty()) return;
    if (img.getWidth() != b.getWidth() || img.getHeight() != b.getHeight()) img = Image (Image::RGB, b.getWidth(), b.getHeight(), true);
    {
        Graphics ig (img);
        paintContent (ig);
    }
    // a glitch tears the picture: bands of it slide sideways (like a VHS losing its tracking)
    const bool tearing = processor.engine.nowFx.load() != chop::None;
    if (! tearing) { g.drawImageAt (img, 0, 0); return; }
    int y = 0;
    while (y < b.getHeight())
    {
        const int h = 4 + rng.nextInt (26);
        const int dx = rng.nextInt (5) == 0 ? rng.nextInt (31) - 15 : rng.nextInt (5) - 2;
        g.drawImage (img, dx, y, b.getWidth(), h, 0, y, b.getWidth(), h);
        y += h;
    }
}

//==============================================================================
int SeqView::cellAt (Point<float> p) const
{
    return jlimit (0, 15, (int) (p.x / ((float) getWidth() / 16.0f)));
}

void SeqView::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    const float cw = (float) b.getWidth() / 16.0f;
    const bool on = processor.apvts.getRawParameterValue ("seqOn")->load() > 0.5f;
    const int now = processor.engine.step.load();
    g.setFont (cat::lcdFont (17.0f));
    for (int k = 0; k < 16; ++k)
    {
        auto r = Rectangle<float> ((float) k * cw + 2.0f, 16.0f, cw - 4.0f, (float) b.getHeight() - 18.0f);
        if (k % 4 == 0) osdText (g, String (k / 4 + 1), Rectangle<int> ((int) r.getX(), 0, 20, 14), Justification::centredLeft, 0.0f, dimOsd);
        const int fx = processor.getStep (k);
        g.setColour (fx == chop::None ? Colour (0xff15172e) : fxColour (fx).withAlpha (on ? 0.85f : 0.3f));
        g.fillRect (r);
        if (fx != chop::None)
        {
            g.setColour (Colours::black.withAlpha (0.85f));
            g.drawText (chop::fxShort()[fx], r.toNearestInt(), Justification::centred);
        }
        if (k == now) { g.setColour (Colours::white); g.drawRect (r.expanded (1.0f), 2.0f); }
        else { g.setColour (Colours::white.withAlpha (0.12f)); g.drawRect (r, 1.0f); }
    }
}

void SeqView::mouseDown (const MouseEvent& e)
{
    const int k = cellAt (e.position);
    if (e.mods.isPopupMenu())
    {
        PopupMenu m;
        for (int f = 0; f < chop::numFx; ++f) m.addItem (f + 1, chop::fxNames()[f], true, processor.getStep (k) == f);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this, k] (int r) { if (r > 0) { processor.setStep (k, r - 1); repaint(); } });
        return;
    }
    paintWith = processor.getStep (k) == brush ? chop::None : brush;    // clicking the same glitch again erases it
    processor.setStep (k, paintWith);
    processor.currentPreset = -1;
    repaint();
}

void SeqView::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const int k = cellAt (e.position);
    if (processor.getStep (k) != paintWith) { processor.setStep (k, paintWith); repaint(); }
}

//==============================================================================
void BrushBar::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    const float cw = (float) b.getWidth() / (float) chop::numFx;
    g.setFont (cat::lcdFont (15.0f));
    for (int f = 0; f < chop::numFx; ++f)
    {
        auto r = Rectangle<float> ((float) f * cw + 2.0f, 2.0f, cw - 4.0f, (float) b.getHeight() - 4.0f);
        g.setColour (f == 0 ? Colour (0xff15172e) : fxColour (f).withAlpha (brush == f ? 1.0f : 0.45f));
        g.fillRect (r);
        g.setColour (f == 0 ? osd : Colours::black);
        g.drawText (f == 0 ? "ERASE" : chop::fxShort()[f], r.toNearestInt(), Justification::centred);
        if (brush == f) { g.setColour (Colours::white); g.drawRect (r.expanded (1.5f), 2.0f); }
    }
}

void BrushBar::mouseDown (const MouseEvent& e)
{
    brush = jlimit (0, chop::numFx - 1, (int) (e.position.x / ((float) getWidth() / (float) chop::numFx)));
    repaint();
    if (onChange) onChange();
}

//==============================================================================
Rectangle<int> PadGrid::padRect (int i) const
{
    const int cols = 6, gap = 8;
    const int w = (getWidth() - gap * (cols - 1)) / cols, h = (getHeight() - gap) / 2;
    return { (i % cols) * (w + gap), (i / cols) * (h + gap), w, h };
}

int PadGrid::padAt (Point<float> p) const
{
    for (int i = 0; i < chop::numPads; ++i) if (padRect (i).toFloat().contains (p)) return i;
    return -1;
}

void PadGrid::paint (Graphics& g)
{
    static const char* keys[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < chop::numPads; ++i)
    {
        const auto r = padRect (i).toFloat();
        const bool lit = processor.engine.padLit[(size_t) i].load();
        const auto c = fxColour (i + 1);
        // the rubber pad: dark when off, glowing in its colour when held
        if (lit) { g.setColour (c.withAlpha (0.35f)); g.fillRoundedRectangle (r.expanded (4.0f), 10.0f); }
        g.setGradientFill (ColourGradient (lit ? c : Colour (0xff1d1f36), r.getX(), r.getY(), lit ? c.darker (0.5f) : Colour (0xff11122a), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (c.withAlpha (lit ? 1.0f : 0.7f));
        g.drawRoundedRectangle (r.reduced (1.0f), 8.0f, 2.0f);
        g.setFont (cat::lcdFont (24.0f));
        g.setColour (lit ? Colours::black : osd);
        g.drawText (chop::fxShort()[i + 1], r.toNearestInt().withTrimmedBottom (14), Justification::centred);
        g.setFont (cat::lcdFont (15.0f));
        g.setColour (lit ? Colours::black.withAlpha (0.7f) : dimOsd);
        g.drawText ("KEY " + String (keys[i]), r.toNearestInt().reduced (6, 4), Justification::centredBottom);
    }
}

void PadGrid::mouseDown (const MouseEvent& e)
{
    held = padAt (e.position);
    if (held >= 0) processor.engine.padMouse[(size_t) held] = true;
}

void PadGrid::mouseDrag (const MouseEvent& e)
{
    // sliding to another pad switches to it (like on a real sampler)
    const int p = padAt (e.position);
    if (p != held)
    {
        if (held >= 0) processor.engine.padMouse[(size_t) held] = false;
        held = p;
        if (held >= 0) processor.engine.padMouse[(size_t) held] = true;
    }
}

void PadGrid::mouseUp (const MouseEvent&)
{
    if (held >= 0) processor.engine.padMouse[(size_t) held] = false;
    held = -1;
}

//==============================================================================
void OsdButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool inv = down;
    g.setColour (inv ? accent : Colours::black.withAlpha (over ? 0.55f : 0.35f));
    g.fillRect (r);
    g.setColour (isEnabled() ? accent : dimOsd);
    g.drawRect (r, 2.0f);
    g.setFont (cat::lcdFont (size));
    if (inv) { g.setColour (Colours::black); g.drawText (getName(), r.toNearestInt(), Justification::centred); }
    else osdText (g, getName(), r.toNearestInt(), Justification::centred, 0.0f, isEnabled() ? accent : dimOsd);
}

//==============================================================================
void TvScreen::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Colour (0xff0d1030), b.getCentreX(), b.getCentreY(), Colour (0xff04050f), 0.0f, 0.0f, true));
    g.fillRoundedRectangle (b, 22.0f);
}

void TvScreen::paintOverChildren (Graphics& g)
{
    const auto b = getLocalBounds();
    // the VCR's on-screen display
    g.setFont (cat::lcdFont (28.0f));
    if ((frame / 20) % 2 == 0 || glitch > 0.1f) osdText (g, glitch > 0.1f ? "PLAY >>" : "PLAY >", { 26, 14, 160, 30 }, Justification::centredLeft, glitch * 3.0f);
    const int secs = frame / 30;
    osdText (g, "SP  " + String (secs / 3600).paddedLeft ('0', 1) + ":" + String ((secs / 60) % 60).paddedLeft ('0', 2) + ":" + String (secs % 60).paddedLeft ('0', 2),
             { b.getWidth() - 220, 14, 196, 30 }, Justification::centredRight, glitch * 3.0f);

    // the tape's tracking band rolls slowly down the picture
    const int band = (frame * 3) % (b.getHeight() + 120) - 60;
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.0f), 0.0f, (float) band - 30.0f, Colours::white.withAlpha (0.0f), 0.0f, (float) band + 30.0f, false));
    {
        ColourGradient cg (Colours::white.withAlpha (0.0f), 0.0f, (float) band - 30.0f, Colours::white.withAlpha (0.0f), 0.0f, (float) band + 30.0f, false);
        cg.addColour (0.5, Colours::white.withAlpha (0.045f));
        g.setGradientFill (cg);
        g.fillRect (0, band - 30, b.getWidth(), 60);
    }
    // noise: a few thin white streaks always, many when it glitches
    const int streaks = 2 + (int) (glitch * 24.0f);
    for (int i = 0; i < streaks; ++i)
    {
        const int y = rng.nextInt (b.getHeight());
        const int x = rng.nextInt (b.getWidth());
        g.setColour ((rng.nextBool() ? Colours::white : Colour (0xff9fb4ff)).withAlpha (0.08f + 0.3f * rng.nextFloat() * (0.3f + glitch)));
        g.fillRect (x, y, 20 + rng.nextInt (200), 1 + rng.nextInt (2));
    }
    // colour fringes at the edges of the picture while it glitches
    if (glitch > 0.05f)
    {
        g.setColour (Colour (0xffff2a4a).withAlpha (0.10f * glitch)); g.fillRect (b.withWidth (6).translated (2, 0));
        g.setColour (Colour (0xff2af0ff).withAlpha (0.10f * glitch)); g.fillRect (b.withLeft (b.getRight() - 8));
    }
    // scanlines and the curved glass's dark corners
    g.setColour (Colours::black.withAlpha (0.22f));
    for (int y = 0; y < b.getHeight(); y += 3) g.fillRect (0, y, b.getWidth(), 1);
    ColourGradient vg (Colours::transparentBlack, (float) b.getCentreX(), (float) b.getCentreY(), Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
    vg.addColour (0.7, Colours::transparentBlack);
    g.setGradientFill (vg);
    g.fillRoundedRectangle (b.toFloat(), 22.0f);
}

//==============================================================================
ChopCatEditor::ChopCatEditor (ChopCatProcessor& p) : processor (p)
{
    addAndMakeVisible (screen);
    for (Component* c : std::initializer_list<Component*> { &presetBar, &monitor, &seq, &brushes, &pads, &craft, &undo, &dice, &clear, &seqOn,
                                                             &chaos, &fill, &seed, &smooth, &mix, &output, &lock, &quantize })
        screen.addAndMakeVisible (*c);

    craft.accent = Colour (0xffffe13b);
    craft.setTooltip ("CRAFT: a whole new glitch style - steps, chaos, fill, lock (UNDO goes back)");
    craft.onClick = [this] { processor.craft (rng); seq.repaint(); screen.glitch = 1.0f; };
    undo.setTooltip ("UNDO: back to the style before the last CRAFT");
    undo.onClick = [this] { processor.undoCraft(); seq.repaint(); };
    dice.setTooltip ("DICE: CHAOS throws other glitches (a new seed)");
    dice.onClick = [this] { if (auto* prm = processor.apvts.getParameter ("seed")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (1 + rng.nextInt (999)))); };
    clear.setTooltip ("CLEAR: empty the sequencer");
    clear.onClick = [this] { for (int k = 0; k < chop::numSteps; ++k) processor.setStep (k, chop::None); seq.repaint(); };
    brushes.onChange = [this] { seq.repaint(); };

    seqOn.setTooltip ("SEQUENCER on / off (the steps)");
    chaos.getSlider().setTooltip ("CHAOS: glitches at random, always in time with the beat");
    fill.getSlider().setTooltip ("FILL: CHAOS puts more glitches at the end of the bar - like a drummer's fill");
    seed.getSlider().setTooltip ("SEED: which random glitches CHAOS throws (DICE picks one)");
    smooth.getSlider().setTooltip ("SMOOTH: how softly a glitch starts and stops (low = hard clicks, high = soft)");
    mix.getSlider().setTooltip ("MIX: how much of the glitch you hear");
    output.getSlider().setTooltip ("OUTPUT: the level that comes out");
    lock.getCombo().setTooltip ("LOCK: CHAOS repeats the same glitches every 1, 2 or 4 bars (it grooves) - or Free (always new)");
    quantize.getCombo().setTooltip ("PADS: a pad (or MIDI key) starts on the next 1/16 or 1/8 - or at once (Off)");
    for (auto* s : { &chaos, &fill, &seed, &smooth, &mix, &output }) s->setColours (Colour (0xff3a3c60), Colour (0xffff3b6b));
    pads.setTooltip ("Hold a pad: that glitch plays. MIDI keys C .. B (any octave) hold the same pads.");
    seq.setTooltip ("Click: paint the brush's glitch (click it again to erase). Drag: paint many. Right-click: pick from a list.");

    setSize (designW, designH);
    startTimerHz (30);
}

ChopCatEditor::~ChopCatEditor() { stopTimer(); }

void ChopCatEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    // the TV's plastic
    g.setGradientFill (ColourGradient (bezelA, 0.0f, 0.0f, bezelB, 0.0f, (float) b.getHeight(), false));
    g.fillAll();
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (screenArea.toFloat().expanded (6.0f), 26.0f, 2.0f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (screenArea.toFloat().expanded (2.0f), 24.0f, 4.0f);
    // the badge, the dial and the power light under the screen
    g.setFont (cat::catFont (15.0f, true));
    g.setColour (Colour (0xffb9b6c4));
    g.drawText ("CHOP-CAT", 40, 642, 200, 30, Justification::centredLeft);
    g.setFont (cat::catFont (11.0f, false));
    g.setColour (Colour (0xff77748a));
    g.drawText ("VHS  HQ  -  DRUM GLITCH TV", 140, 642, 260, 30, Justification::centredLeft);
    for (int i = 0; i < 18; ++i) { g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (520.0f + (float) i * 14.0f, 648.0f, 8.0f, 20.0f, 3.0f); }
    g.setColour (processor.engine.nowFx.load() != chop::None ? Colour (0xffff3b3b) : Colour (0xff6bff6b));
    g.fillEllipse (938.0f, 652.0f, 12.0f, 12.0f);
    g.setColour (Colours::white.withAlpha (0.4f));
    g.fillEllipse (941.0f, 654.0f, 4.0f, 4.0f);
}

void ChopCatEditor::resized()
{
    screen.setBounds (screenArea);
    // inside the screen
    presetBar.setBounds (200, 14, 330, 32);
    craft.setBounds (650, 56, 150, 60);
    undo.setBounds (808, 56, 120, 60);
    monitor.setBounds (24, 56, 610, 196);
    seq.setBounds (24, 268, 610, 84);
    brushes.setBounds (24, 358, 610, 30);
    pads.setBounds (24, 402, 610, 186);

    const int x = 650, w = 278;
    seqOn.setBounds (x, 132, 150, 28);
    clear.setBounds (x + 158, 128, 120, 36);
    chaos.setBounds (x, 176, w, 40);
    fill.setBounds (x, 222, w, 40);
    lock.setBounds (x, 270, w, 28);
    seed.setBounds (x, 308, w - 96, 40);
    dice.setBounds (x + w - 88, 310, 88, 36);
    quantize.setBounds (x, 360, w, 28);
    smooth.setBounds (x, 400, w, 40);
    mix.setBounds (x, 446, w, 40);
    output.setBounds (x, 492, w, 40);
}

void ChopCatEditor::timerCallback()
{
    ++screen.frame;
    const int fx = processor.engine.nowFx.load();
    screen.glitch = fx != chop::None ? 1.0f : screen.glitch * 0.8f;
    // the whole picture jumps a little while it glitches (more for the wild ones)
    if (fx != chop::None)
    {
        const int amp = (fx == chop::Scatter || fx == chop::Reverse || fx == chop::Roll) ? 4 : 2;
        screen.setTransform (AffineTransform::translation ((float) (rng.nextInt (2 * amp + 1) - amp), (float) (rng.nextInt (3) - 1)));
    }
    else if (! screen.getTransform().isIdentity()) screen.setTransform ({});
    undo.setEnabled (processor.canUndo());
    monitor.tick();
    seq.repaint();
    pads.repaint();
    screen.repaint();
    repaint (930, 645, 30, 30);
}

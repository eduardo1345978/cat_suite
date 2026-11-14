#include "LoopView.h"
#include "CatFonts.h"
#include <algorithm>

using namespace juce;

namespace looper
{
namespace
{
const Colour ink (0xffe8ebef), soft (0xffc9ced6), dim (0xffa3aab4), dimmer (0xff7a818c), faint (0xff5a606b), groove (0xff0c0d0f);
const Colour amber (0xffffd6aa);
const Colour cPlay (0xff5cf2a6), cDub (0xffffb84d), cRec (0xffff5050), cStop (0xff8a919c), cEmpty (0xff5a606b);

Colour stateColour (State s)
{
    switch (s)
    {
        case State::playing:     return cPlay;
        case State::overdubbing: return cDub;
        case State::recording:   return cRec;
        case State::armed:       return cRec;
        case State::stopped:     return cStop;
        case State::empty:       return cEmpty;
    }
    return cEmpty;
}

Font title (float h)
{
    static Typeface::Ptr tf = Typeface::createSystemTypefaceFor (CatFonts::SoraBold_ttf, (size_t) CatFonts::SoraBold_ttfSize);
    return Font (FontOptions (tf).withHeight (h));
}
Font sans (float h, float kerning = 0.0f) { return Font (FontOptions (h)).withExtraKerningFactor (kerning); }
Font mono (float h, float kerning = 0.0f) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::plain)).withExtraKerningFactor (kerning); }

String lengthText (const Engine& e, int t)
{
    const auto st = e.getState (t);
    if (st == State::empty) return String::fromUTF8 ("\xe2\x80\x94");
    if (st == State::armed) return e.getBarPosition() < 0.0 ? "IN " + String ((int) std::ceil (-e.getBarPosition() * e.beatsPerBar.load())) : "WAIT";
    if (st == State::recording)
    {
        const float s = e.getPosition (t);
        return String ((int) s / 60) + ":" + String (std::fmod (s, 60.0f), 1).paddedLeft ('0', 4);
    }
    const int bars = e.getBars (t);
    if (bars > 0) return String (bars) + (bars == 1 ? " BAR" : " BARS");
    return String (e.getLength (t) / e.getSampleRate(), 1) + " S";
}
}

String actionName (int a)
{
    if (a < actStop)  return "Track " + String (a + 1) + " REC / PLAY / DUB";
    if (a < actUndo)  return "Track " + String (a - actStop + 1) + " STOP";
    if (a < actClear) return "Track " + String (a - actUndo + 1) + " UNDO";
    if (a < actPlayAll) return "Track " + String (a - actClear + 1) + " CLEAR";
    switch (a)
    {
        case actPlayAll:   return "PLAY ALL";
        case actStopAll:   return "STOP ALL";
        case actToggleAll: return "PLAY / STOP ALL";
        case actBeat:      return "BEAT on / off";
        case actTap:       return "TAP tempo";
        case actUndoLast:  return "UNDO (last track)";
        default: break;
    }
    return {};
}

std::vector<Binding> LoopView::defaultBindings()
{
    // the 8 pads of a controller like the Oxygen Pro Mini (notes 36-43), and its second bank (44-48) for STOP
    std::vector<Binding> b;
    for (int t = 0; t < numTracks; ++t) b.push_back ({ false, 36 + t, actPress + t });
    b.push_back ({ false, 41, actToggleAll });
    b.push_back ({ false, 42, actUndoLast });
    b.push_back ({ false, 43, actBeat });
    for (int t = 0; t < numTracks; ++t) b.push_back ({ false, 44 + t, actStop + t });
    return b;
}

//==============================================================================
class LoopView::LedButton : public Button
{
public:
    LedButton (const String& label, Colour led, float fontH = 8.5f) : Button (label), ledColour (led), fh (fontH) {}
    Colour ledColour;
    bool ledOn = false, targeted = false, round = false;
    float fh;

    void paintButton (Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        const float r = round ? 10.0f : 7.0f;
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (b.translated (0.0f, 2.0f), r);
        ColourGradient grad (Colour (down ? 0xff25282d : (over ? 0xff383b42 : 0xff32353b)), 0, b.getY(), Colour (down ? 0xff2c2f34 : 0xff25282d), 0, b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b, r);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawLine (b.getX() + r, b.getY() + 0.5f, b.getRight() - r, b.getY() + 0.5f, 1.0f);
        if (targeted)
        {
            g.setColour (cPlay.withAlpha (0.9f));
            g.drawRoundedRectangle (b, r, 1.5f);
        }
        const bool tall = b.getHeight() > 30.0f && ! round;
        if (tall)
        {
            g.setColour (ledOn ? ledColour : Colour (0xff3a3e45));
            g.fillRoundedRectangle (b.getCentreX() - 7.0f, b.getY() + 9.0f, 14.0f, 2.0f, 1.0f);
            g.setColour (dim);
            g.setFont (sans (fh, 0.2f));
            g.drawText (getButtonText(), b.withTrimmedTop (14.0f), Justification::centred);
        }
        else
        {
            g.setFont (sans (fh, 0.2f));
            const int tw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), getButtonText());
            const bool led = ledColour.getAlpha() > 0;
            const float total = (float) tw + (led ? 16.0f : 0.0f);
            float x = b.getCentreX() - total * 0.5f;
            if (led)
            {
                g.setColour (ledOn ? ledColour : Colour (0xff3a3e45));
                g.fillRoundedRectangle (x, b.getCentreY() - 4.0f, 8.0f, 8.0f, round ? 1.0f : 4.0f);
                if (ledOn) { g.setColour (ledColour.withAlpha (0.25f)); g.fillEllipse (x - 3.0f, b.getCentreY() - 7.0f, 14.0f, 14.0f); }
                x += 16.0f;
            }
            g.setColour (ledColour.getAlpha() > 0 && ! led ? ledColour : (round ? Colour (0xffd6dae0) : dim));
            g.drawText (getButtonText(), Rectangle<float> (x, b.getY(), (float) tw + 4.0f, b.getHeight()), Justification::centredLeft);
        }
    }
};

//==============================================================================
class LoopView::Knob : public Component, public SettableTooltipClient
{
public:
    Knob (const String& l, float defaultValue) : label (l), def (defaultValue), value (defaultValue) { setRepaintsOnMouseActivity (true); }
    String label;
    std::function<String (float)> text;
    std::function<void (float)> onChange;
    float def, value;

    void setValue (float v, bool notify)
    {
        v = jlimit (0.0f, 1.0f, v);
        if (std::abs (v - value) < 1.0e-6f) return;
        value = v;
        if (notify && onChange) onChange (v);
        repaint();
    }

    void paint (Graphics& g) override
    {
        const float size = 58.0f, cx = getWidth() * 0.5f, cy = size * 0.5f + 8.0f;
        for (int k = 0; k <= 10; ++k)
        {
            const float a = degreesToRadians (-135.0f + 27.0f * (float) k);
            const float r1 = size * 0.5f + 5.0f, r2 = size * 0.5f + 10.0f;
            g.setColour ((float) k / 10.0f <= value + 1.0e-3f ? soft : Colour (0xff3a3e45));
            g.drawLine (cx + r1 * std::sin (a), cy - r1 * std::cos (a), cx + r2 * std::sin (a), cy - r2 * std::cos (a), 1.0f);
        }
        const Rectangle<float> body (cx - size * 0.5f, cy - size * 0.5f, size, size);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillEllipse (body.translated (0.0f, 4.0f).expanded (1.0f));
        // knurled edge
        for (int k = 0; k < 90; ++k)
        {
            Path p;
            const float a0 = MathConstants<float>::twoPi * (float) k / 90.0f, a1 = a0 + MathConstants<float>::twoPi / 180.0f;
            p.addPieSegment (body, a0, a1, 0.0f);
            g.setColour (Colour (0xff3b3f46));
            g.fillPath (p);
        }
        g.setColour (Colour (0xff2e3137).withAlpha (0.55f));
        g.drawEllipse (body.reduced (1.0f), 1.0f);
        const auto cap = body.reduced (7.0f);
        g.setGradientFill (ColourGradient (Colour (0xff4a4e56), cap.getX() + cap.getWidth() * 0.35f, cap.getY() + cap.getHeight() * 0.3f,
                                           Colour (0xff22252a), cap.getRight(), cap.getBottom(), true));
        g.fillEllipse (cap);
        g.setColour (Colours::white.withAlpha (isMouseOverOrDragging() ? 0.2f : 0.12f));
        g.drawEllipse (cap, 1.0f);
        const float a = degreesToRadians (-135.0f + 270.0f * value);
        const float r0 = 4.0f, r1 = cap.getWidth() * 0.5f - 3.0f;
        g.setColour (ink);
        g.drawLine (cx + r0 * std::sin (a), cy - r0 * std::cos (a), cx + r1 * std::sin (a), cy - r1 * std::cos (a), 2.0f);

        g.setColour (Colour (0xff8a919c));
        g.setFont (sans (9.0f, 0.25f));
        g.drawText (label, 0, (int) (cy + size * 0.5f + 12.0f), getWidth(), 12, Justification::centred);
        g.setColour (soft);
        g.setFont (mono (10.0f));
        g.drawText (text ? text (value) : String (roundToInt (value * 100.0f)) + "%", 0, (int) (cy + size * 0.5f + 25.0f), getWidth(), 13, Justification::centred);
    }
    void mouseDown (const MouseEvent&) override { start = value; }
    void mouseDrag (const MouseEvent& e) override { setValue (start - (float) e.getDistanceFromDragStartY() / 180.0f, true); }
    void mouseDoubleClick (const MouseEvent&) override { setValue (def, true); }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override { setValue (value + w.deltaY * 0.15f, true); }

private:
    float start = 0.0f;
};

//==============================================================================
class LoopView::Strip : public Component, public SettableTooltipClient
{
public:
    Strip (LoopView& v, int index) : view (v), t (index)
    {
        for (auto* b : { &stopB, &undoB, &clearB }) addAndMakeVisible (*b);
        stopB.onClick  = [this] { view.target (actStop + t); };
        undoB.onClick  = [this] { view.target (actUndo + t); };
        clearB.onClick = [this] { view.target (actClear + t); };
        stopB.setTooltip ("Stop this track (key " + String::charToString ("QWERT"[t]) + ")");
        undoB.setTooltip ("Take the last overdub back - again brings it back (key " + String::charToString ("ASDFG"[t]) + ")");
        clearB.setTooltip ("Empty this track (key " + String::charToString ("ZXCVB"[t]) + ")");
        setTooltip ("The big button (key " + String (t + 1) + "): REC -> PLAY -> DUB -> PLAY ... Right-click: more");
    }

    void resized() override
    {
        stopB.setBounds (18, 318, 52, 34);
        undoB.setBounds (74, 318, 52, 34);
        clearB.setBounds (130, 318, 52, 34);
    }

    void refresh()
    {
        auto& e = view.engine;
        const auto st = e.getState (t);
        undoB.ledOn = e.canUndo (t);
        undoB.ledColour = e.isUndone (t) ? cPlay : cDub;
        undoB.setButtonText (e.isUndone (t) ? "REDO" : "UNDO");
        stopB.ledOn = st == State::stopped;
        stopB.ledColour = cStop;
        for (auto* b : { &stopB, &undoB, &clearB }) b->targeted = false;
        if (view.learning)
        {
            stopB.targeted = view.learnTarget == actStop + t;
            undoB.targeted = view.learnTarget == actUndo + t;
            clearB.targeted = view.learnTarget == actClear + t;
        }
        for (auto* b : { &stopB, &undoB, &clearB }) b->repaint();
        if (++frame % 12 == 0 || e.getLength (t) != lastLen || e.isUndone (t) != lastUndone)
        {
            lastLen = e.getLength (t);
            lastUndone = e.isUndone (t);
            e.overview (t, peaks.data(), (int) peaks.size());
            float m = 1.0e-4f;
            for (auto p : peaks) m = jmax (m, p);
            for (auto& p : peaks) p = std::sqrt (p / m);
        }
        repaint();
    }

    void paint (Graphics& g) override
    {
        auto& e = view.engine;
        const auto st = e.getState (t);
        const auto col = stateColour (st);
        const bool lit = st == State::playing || st == State::overdubbing || st == State::recording;
        const bool blink = st == State::armed && (Time::getMillisecondCounter() / 250) % 2 == 0;
        const auto b = getLocalBounds().toFloat();

        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.025f), 0, 0, Colours::black.withAlpha (0.08f), 0, b.getBottom(), false));
        g.fillRoundedRectangle (b, 14.0f);
        g.setColour (Colours::white.withAlpha (view.learning && view.learnTarget == actPress + t ? 0.0f : 0.04f));
        g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);
        if (view.learning && view.learnTarget == actPress + t) { g.setColour (cPlay); g.drawRoundedRectangle (b.reduced (1.0f), 14.0f, 1.5f); }

        g.setColour (ink);
        g.setFont (title (22.0f));
        g.drawText (String (t + 1), 18, 10, 40, 30, Justification::centredLeft);
        const String stText = stateName (st);
        g.setFont (sans (9.5f, 0.25f));
        const int tw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), stText);
        g.setColour (st == State::empty ? cEmpty : col);
        g.drawText (stText, getWidth() - 18 - tw - 2, 18, tw + 4, 14, Justification::centredLeft);
        g.setColour (lit || blink ? col : Colour (0xff2e3137));
        g.fillEllipse ((float) (getWidth() - 18 - tw - 13), 22.0f, 6.0f, 6.0f);

        // LEVEL: the fader and the meter
        g.setColour (Colour (0xff6b717c));
        g.setFont (sans (9.0f, 0.2f));
        g.drawText ("LEVEL", 18, 56, 80, 12, Justification::centredLeft);
        const float v = e.volume (t).load();
        const Rectangle<float> track (18.0f, 81.0f, 164.0f, 4.0f);
        g.setColour (groove);
        g.fillRoundedRectangle (track, 2.0f);
        g.setGradientFill (ColourGradient (Colour (0xff3a3e45), track.getX(), 0, st == State::empty ? Colour (0xff3a3e45) : col, track.getX() + 164.0f * v, 0, false));
        g.fillRoundedRectangle (track.withWidth (jmax (4.0f, 164.0f * v)), 2.0f);
        const Rectangle<float> cap (18.0f + 164.0f * v - 9.0f, 71.0f, 18.0f, 24.0f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (cap.translated (0, 3), 4.0f);
        g.setGradientFill (ColourGradient (Colour (0xffd9dde3), 0, cap.getY(), Colour (0xff8b919a), 0, cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 4.0f);
        g.setColour (Colour (0xff4a4e56));
        g.drawLine (cap.getX() + 3.0f, cap.getCentreY(), cap.getRight() - 3.0f, cap.getCentreY(), 1.0f);
        const float level = jlimit (0.0f, 1.0f, std::sqrt (e.getLevel (t)) * 1.15f);
        for (int k = 0; k < 20; ++k)
        {
            g.setColour ((float) k / 20.0f < level ? (k < 14 ? cPlay : (k < 18 ? cDub : cRec)) : Colour (0xff24272c));
            g.fillRoundedRectangle (18.0f + (float) k * 8.2f, 102.0f, 6.6f, 3.0f, 1.0f);
        }

        // the LED ring
        const Point<float> c (100.0f, 208.0f);
        constexpr int n = 40;
        float fill = 0.0f;
        if (st == State::playing || st == State::overdubbing) fill = e.getPosition (t);
        else if (st == State::recording || blink) fill = 1.0f;
        for (int k = 0; k < n; ++k)
        {
            const bool on = (lit || blink) && (float) k / n < fill + 1.0e-4f;
            const float a = MathConstants<float>::twoPi * (float) k / n;
            const auto p = c + Point<float> (std::sin (a), -std::cos (a)) * 79.0f;
            Path seg;
            seg.addRoundedRectangle (-2.0f, -6.5f, 4.0f, 13.0f, 2.0f);
            seg.applyTransform (AffineTransform::rotation (a).translated (p));
            if (on)
            {
                g.setColour (col.withAlpha (0.22f));
                g.strokePath (seg, PathStrokeType (5.0f));
            }
            g.setColour (on ? col : Colours::white.withAlpha (0.07f));
            g.fillPath (seg);
        }
        // the footswitch: a machined disc
        const float R = 62.0f, r = 54.0f;
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillEllipse (c.x - R, c.y - R + 8.0f, R * 2.0f, R * 2.0f);
        g.setGradientFill (ColourGradient (Colour (pressed ? 0xff3e4249 : 0xff4c5058), c.x - R, c.y - R, Colour (0xff1c1e22), c.x + R, c.y + R, false));
        g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
        g.setColour (Colour (0xff0b0c0e));
        g.drawEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f, 1.0f);
        g.setGradientFill (ColourGradient (Colour (0xff50545c), c.x - r * 0.3f, c.y - r * 0.4f, Colour (0xff2a2d32), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        for (float rr = 4.0f; rr < r; rr += 3.0f)
        {
            g.setColour (Colours::white.withAlpha (0.03f));
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 1.0f);
        }
        g.setColour (Colours::white.withAlpha (0.14f));
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
        g.setColour (soft);
        g.setFont (mono (11.0f, 0.08f));
        g.drawText (lengthText (e, t), Rectangle<float> (c.x - r, c.y - 8.0f, r * 2.0f, 16.0f), Justification::centred);

        // the loop, small, with where it plays
        const Rectangle<float> wave (18.0f, 366.0f, 164.0f, 34.0f);
        g.setColour (groove.withAlpha (0.7f));
        g.fillRoundedRectangle (wave, 5.0f);
        if (e.getLength (t) > 0 && st != State::recording)
        {
            const float bw = wave.getWidth() / (float) peaks.size();
            for (size_t k = 0; k < peaks.size(); ++k)
            {
                const float h = jmax (1.0f, peaks[k] * (wave.getHeight() - 8.0f));
                const bool played = (st == State::playing || st == State::overdubbing) && (float) k / (float) peaks.size() < e.getPosition (t);
                g.setColour ((st == State::stopped ? cStop : col).withAlpha (played ? 0.85f : 0.35f));
                g.fillRect (wave.getX() + (float) k * bw, wave.getCentreY() - h * 0.5f, jmax (1.0f, bw - 0.6f), h);
            }
        }
        else
        {
            g.setColour (faint.withAlpha (0.6f));
            g.setFont (mono (9.0f));
            g.drawText (st == State::recording ? "RECORDING ..." : (st == State::armed ? "WAITING FOR THE BAR" : "PRESS TO RECORD"), wave, Justification::centred);
        }

        g.setColour (faint);
        g.setFont (mono (9.5f));
        g.drawText ("KEY " + String (t + 1) + String::fromUTF8 (" \xc2\xb7 ") + "PAD " + String (36 + t), 0, getHeight() - 30, getWidth(), 14, Justification::centred);
    }

    bool onFootswitch (Point<float> p) const { return p.getDistanceFrom ({ 100.0f, 208.0f }) < 86.0f; }
    bool onFader (Point<float> p) const      { return p.y > 64.0f && p.y < 98.0f && p.x > 8.0f && p.x < 192.0f; }

    void mouseDown (const MouseEvent& ev) override
    {
        const auto p = ev.position;
        if (onFader (p)) { dragFader = true; setFader (p.x); return; }
        if (! onFootswitch (p)) return;
        if (ev.mods.isPopupMenu())
        {
            PopupMenu m;
            m.addSectionHeader ("Track " + String (t + 1));
            m.addItem (1, "Stop");
            m.addItem (2, view.engine.isUndone (t) ? "Redo" : "Undo", view.engine.canUndo (t));
            m.addItem (3, "Clear");
            m.addSeparator();
            m.addItem (4, "Save this loop as .wav ...", view.engine.getLength (t) > 0);
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this] (int r)
            {
                if (r == 1) view.target (actStop + t);
                if (r == 2) view.target (actUndo + t);
                if (r == 3) view.target (actClear + t);
                if (r == 4) saveLoop();
            });
            return;
        }
        pressed = true;
        view.target (actPress + t);
        repaint();
    }
    void mouseDrag (const MouseEvent& ev) override { if (dragFader) setFader (ev.position.x); }
    void mouseUp (const MouseEvent&) override { pressed = false; dragFader = false; repaint(); }
    void mouseDoubleClick (const MouseEvent& ev) override { if (onFader (ev.position)) { view.engine.volume (t) = 0.8f; repaint(); } }
    void setFader (float x) { view.engine.volume (t) = jlimit (0.0f, 1.0f, (x - 18.0f) / 164.0f); repaint(); }

    void saveLoop()
    {
        auto loop = view.engine.getLoop (t);
        if (loop.getNumSamples() <= 0) return;
        view.chooser = std::make_unique<FileChooser> ("Save the loop", File::getSpecialLocation (File::userMusicDirectory).getChildFile ("Cat Loop track " + String (t + 1) + ".wav"), "*.wav");
        view.chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
            [this, loop] (const FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == File()) return;
                f.deleteFile();
                WavAudioFormat wav;
                if (auto w = std::unique_ptr<AudioFormatWriter> (wav.createWriterFor (new FileOutputStream (f), view.engine.getSampleRate(), 2, 24, {}, 0)))
                {
                    w->writeFromAudioSampleBuffer (loop, 0, loop.getNumSamples());
                    view.showMessage ("SAVED " + f.getFileName().toUpperCase());
                }
            });
    }

    LoopView& view;
    const int t;
    LedButton stopB { "STOP", cStop }, undoB { "UNDO", cDub }, clearB { "CLEAR", cRec };
    bool pressed = false, dragFader = false;
    std::array<float, 82> peaks {};
    int frame = 0, lastLen = -1;
    bool lastUndone = false;
};

//==============================================================================
class LoopView::Display : public Component
{
public:
    explicit Display (LoopView& v) : view (v) {}

    void paint (Graphics& g) override
    {
        auto& e = view.engine;
        const auto b = getLocalBounds().toFloat();
        g.setColour (Colour (0xff2b2e33));
        g.fillRoundedRectangle (b, 14.0f);
        const auto glass = b.reduced (4.0f);
        g.setColour (Colours::black);
        g.fillRoundedRectangle (glass.expanded (1.0f), 12.0f);
        g.setColour (Colour (0xff0b0b0c));
        g.fillRoundedRectangle (glass, 11.0f);
        const auto in = glass.reduced (16.0f, 12.0f);

        // the top line: the settings, or a message
        g.setFont (mono (9.5f, 0.12f));
        if (view.message.isNotEmpty())
        {
            g.setColour (amber);
            g.drawText (view.message, in.withHeight (14.0f), Justification::centredLeft);
        }
        else
        {
            g.setColour (amber.withAlpha (0.6f));
            const auto third = in.getWidth() / 3.0f;
            g.drawText ("QUANTIZE " + quantizeNames()[e.quantize.load()].toUpperCase().upToFirstOccurrenceOf (" ", false, false),
                        in.withHeight (14.0f).withWidth (third), Justification::centredLeft);
            g.drawText ("CLICK " + String::fromUTF8 ("\xc2\xb7 ") + clickNames()[e.click.load()].toUpperCase(),
                        in.withHeight (14.0f).withX (in.getX() + third).withWidth (third), Justification::centred);
            g.drawText ("IN " + String::fromUTF8 ("\xc2\xb7 ") + inputNames()[e.input.load()].toUpperCase().upToFirstOccurrenceOf (" (", false, false),
                        in.withHeight (14.0f).withX (in.getX() + third * 2.0f).withWidth (third), Justification::centredRight);
        }

        // tempo
        const bool known = e.isTempoKnown();
        g.setColour (amber.withAlpha (known ? 1.0f : 0.45f));
        g.setFont (title (50.0f).withExtraKerningFactor (-0.02f));
        const String bpm = known ? String (e.getBpm(), 1) : "--.-";
        g.drawText (bpm, (int) in.getX(), (int) in.getY() + 16, 170, 62, Justification::centredLeft);
        const int bw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), bpm);
        g.setFont (mono (11.0f));
        g.setColour (amber.withAlpha (0.7f));
        g.drawText ("BPM", (int) in.getX() + bw + 12, (int) in.getY() + 40, 60, 14, Justification::centredLeft);
        g.drawText (known ? String (e.beatsPerBar.load()) + " / 4" : "FIRST LOOP", (int) in.getX() + bw + 12, (int) in.getY() + 54, 90, 14, Justification::centredLeft);

        // the bar: one dot per beat
        const int bpb = e.beatsPerBar.load();
        const double barPos = e.getBarPosition();
        const bool running = e.isRunning();
        const double beatPos = running ? (barPos - std::floor (barPos)) * bpb : -1.0;
        const int beat = running ? (int) std::floor (beatPos) : -1;
        const float dot = 12.0f, gap = 6.0f;
        float x = in.getRight() - (float) bpb * (dot + gap) + gap;
        const float y = in.getY() + 44.0f;
        g.setFont (mono (10.0f));
        g.setColour (amber.withAlpha (0.6f));
        const String barText = ! running ? "STOPPED" : (barPos < 0.0 ? "COUNT-IN" : "BAR " + String ((int) std::floor (barPos) + 1));
        g.drawText (barText, (int) x - 80, (int) y - 1, 72, 14, Justification::centredRight);
        for (int k = 0; k < bpb; ++k)
        {
            const Rectangle<float> d (x, y, dot, dot);
            g.setColour (amber.withAlpha (0.5f));
            g.drawEllipse (d.reduced (0.5f), 1.0f);
            if (k == beat)
            {
                g.setColour (Colour (0xffffb070).withAlpha (0.35f));
                g.fillEllipse (d.expanded (4.0f));
                g.setColour (amber);
                g.fillEllipse (d);
            }
            else if (k < beat)
            {
                g.setColour (amber.withAlpha (0.35f));
                g.fillEllipse (d);
            }
            x += dot + gap;
        }

        // the 5 tracks at a glance
        const float tw = (in.getWidth() - 4.0f * 10.0f) / 5.0f;
        for (int t = 0; t < numTracks; ++t)
        {
            const float tx = in.getX() + (float) t * (tw + 10.0f), ty = in.getBottom() - 34.0f;
            const auto st = e.getState (t);
            g.setFont (mono (9.0f));
            g.setColour (amber.withAlpha (0.6f));
            g.drawText (String (t + 1), (int) tx, (int) ty, 20, 11, Justification::centredLeft);
            g.setColour (amber.withAlpha (0.12f));
            g.fillRoundedRectangle (tx, ty + 14.0f, tw, 5.0f, 1.0f);
            float f = 0.0f;
            if (st == State::playing || st == State::overdubbing) f = e.getPosition (t);
            else if (st == State::recording) f = 1.0f;
            g.setColour (st == State::recording ? Colour (0xffff8a70) : amber);
            if (f > 0.0f) g.fillRoundedRectangle (tx, ty + 14.0f, tw * f, 5.0f, 1.0f);
            g.setColour (amber.withAlpha (0.75f));
            g.drawText (stateName (st), (int) tx, (int) ty + 22, (int) tw, 11, Justification::centredLeft);
        }
    }

    // clicks: the top line picks QUANTIZE / CLICK / INPUT, the tempo can be dragged (or clicked for its menu)
    void mouseDown (const MouseEvent& ev) override
    {
        dragged = false;
        startBpm = view.engine.getBpm();
        startPoint = ev.position;
    }
    void mouseDrag (const MouseEvent& ev) override
    {
        if (startPoint.y < 30.0f || startPoint.x > 230.0f || startPoint.y > 105.0f) return;
        if (std::abs (ev.getDistanceFromDragStartY()) < 3 && ! dragged) return;
        dragged = true;
        if (! view.engine.setBpm (std::round ((startBpm - ev.getDistanceFromDragStartY() * 0.25) * 2.0) / 2.0))
            view.showMessage ("THE TEMPO IS SET BY THE LOOPS (CLEAR ALL TO CHANGE IT)");
    }
    void mouseUp (const MouseEvent& ev) override
    {
        if (dragged) { view.saveSettings(); return; }
        auto& e = view.engine;
        const auto p = ev.position;
        if (p.y < 30.0f)
        {
            const float third = (getWidth() - 40.0f) / 3.0f;
            const int which = jlimit (0, 2, (int) ((p.x - 20.0f) / third));
            const auto names = which == 0 ? quantizeNames() : (which == 1 ? clickNames() : inputNames());
            auto& setting = which == 0 ? e.quantize : (which == 1 ? e.click : e.input);
            PopupMenu m;
            m.addSectionHeader (which == 0 ? "Quantize: loops start and end on" : (which == 1 ? "Click (metronome)" : "Record from"));
            for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, setting.load() == i);
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this, &setting] (int r)
            {
                if (r > 0) { setting = r - 1; view.saveSettings(); repaint(); }
            });
        }
        else if (p.x < 230.0f && p.y < 105.0f) view.openTempoMenu();
    }

    LoopView& view;
    bool dragged = false;
    double startBpm = 120.0;
    Point<float> startPoint;
};

//==============================================================================
LoopView::LoopView (Engine& e, AudioDeviceManager* dm, PropertiesFile* p) : engine (e), devices (dm), props (p)
{
    setWantsKeyboardFocus (true);
    bindings = defaultBindings();

    for (int t = 0; t < numTracks; ++t)
    {
        auto* s = strips.add (new Strip (*this, t));
        addAndMakeVisible (s);
    }
    display = std::make_unique<Display> (*this);
    addAndMakeVisible (*display);

    inputKnob = std::make_unique<Knob> ("INPUT", 0.0f);
    inputKnob->text = [] (float v) { return v < 0.01f ? String ("OFF") : String (Decibels::gainToDecibels (v * v * 1.4125f), 0) + " dB"; };
    inputKnob->onChange = [this] (float v) { engine.monitor = v * v * 1.4125f; };
    inputKnob->setTooltip ("INPUT: hear what comes in (OFF if your sound card already lets you hear it - or with a microphone and speakers)");
    rhythmKnob = std::make_unique<Knob> ("RHYTHM", 0.6f);
    rhythmKnob->text = [] (float v) { return String (roundToInt (v * 100.0f)) + "%"; };
    rhythmKnob->onChange = [this] (float v) { engine.beatLevel = v * v * 1.6f; engine.clickLevel = 0.3f + v * 0.7f; };
    rhythmKnob->setTooltip ("RHYTHM: how loud the beat and the click are");
    masterKnob = std::make_unique<Knob> ("MASTER", 0.75f);
    masterKnob->text = [] (float v) { return v < 0.01f ? String ("-inf") : String (Decibels::gainToDecibels (v * v * 1.78f), 1) + " dB"; };
    masterKnob->onChange = [this] (float v) { engine.master = v * v * 1.78f; };
    masterKnob->setTooltip ("MASTER: everything that comes out");
    for (auto* k : { inputKnob.get(), rhythmKnob.get(), masterKnob.get() }) addAndMakeVisible (*k);

    auto make = [this] (std::unique_ptr<LedButton>& b, const String& text, Colour led, float fh = 8.5f)
    {
        b = std::make_unique<LedButton> (text, led, fh);
        addAndMakeVisible (*b);
    };
    make (saveB, "SAVE", Colours::transparentBlack);
    make (loadB, "LOAD", Colours::transparentBlack);
    make (exportB, "EXPORT", Colours::transparentBlack);
    make (midiB, "MIDI", cPlay.withAlpha (0.0f));
    make (setupB, "SETUP", Colours::transparentBlack);
    make (playAllB, "PLAY ALL", cPlay, 10.0f);
    make (stopAllB, "STOP ALL", cRec, 10.0f);
    make (beatB, "BEAT", cDub, 9.0f);
    playAllB->round = stopAllB->round = beatB->round = true;
    playAllB->ledOn = stopAllB->ledOn = true;
    midiB->ledColour = Colour (0x0);

    saveB->onClick = [this] { doSave(); };
    loadB->onClick = [this] { doLoad(); };
    exportB->onClick = [this] { doExport(); };
    midiB->onClick = [this] { toggleLearn(); };
    setupB->onClick = [this] { openSetup(); };
    playAllB->onClick = [this] { target (actPlayAll); };
    stopAllB->onClick = [this] { target (actStopAll); };
    beatB->onClick = [this]
    {
        if (ModifierKeys::currentModifiers.isPopupMenu()) openBeatMenu();
        else target (actBeat);
    };
    saveB->setTooltip ("Save the set (all loops, the tempo, the beat) as a .catloop file");
    loadB->setTooltip ("Open a saved set");
    exportB->setTooltip ("Export the mix, or every track as its own .wav (stems) for your DAW");
    midiB->setTooltip ("MIDI learn: click a button here, then press a pad / key / button on your controller");
    setupB->setTooltip ("Sound card, MIDI, latency, what happens after REC");
    beatB->setTooltip ("BEAT on / off (key B). Right-click: the style, the kit, a new pattern");

    loadSettings();
    startTimerHz (30);
}

LoopView::~LoopView()
{
    stopTimer();
    saveSettings();
}

void LoopView::resized()
{
    saveB->setBounds (46, 108, 52, 26);
    loadB->setBounds (104, 108, 52, 26);
    exportB->setBounds (162, 108, 66, 26);
    midiB->setBounds (234, 108, 50, 26);
    setupB->setBounds (46, 140, 62, 26);
    display->setBounds (356, 26, 468, 158);
    inputKnob->setBounds (846, 30, 96, 116);
    rhythmKnob->setBounds (942, 30, 96, 116);
    masterKnob->setBounds (1038, 30, 96, 116);
    beatB->setBounds (862, 152, 262, 28);
    for (int t = 0; t < numTracks; ++t) strips[t]->setBounds (46 + t * 220, 214, 200, 440);
    playAllB->setBounds (824, 664, 150, 42);
    stopAllB->setBounds (984, 664, 150, 42);
}

void LoopView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Colour (0xff26292e), 0, 0, Colour (0xff16181b), 0, b.getBottom(), false));
    g.fillAll();
    g.setColour (Colours::white.withAlpha (0.012f));
    for (int y = 0; y < getHeight(); y += 2) g.drawHorizontalLine (y, 0.0f, b.getRight());
    // screws
    for (auto p : { Point<float> (19, 19), Point<float> (b.getRight() - 19, 19), Point<float> (19, b.getBottom() - 19), Point<float> (b.getRight() - 19, b.getBottom() - 19) })
    {
        g.setGradientFill (ColourGradient (Colour (0xff5a5e66), p.x - 2, p.y - 2, Colour (0xff1e2024), p.x + 5, p.y + 5, true));
        g.fillEllipse (p.x - 5, p.y - 5, 10, 10);
        g.setColour (Colour (0xff15171a));
        g.drawLine (p.x - 3, p.y - 1, p.x + 3, p.y + 1, 1.0f);
    }
    g.setColour (ink);
    g.setFont (title (26.0f).withExtraKerningFactor (0.18f));
    g.drawText ("CAT LOOP", 46, 36, 300, 34, Justification::centredLeft);
    g.setColour (dimmer);
    g.setFont (sans (9.5f, 0.3f));
    g.drawText ("FIVE-TRACK LIVE LOOPER", 47, 72, 300, 14, Justification::centredLeft);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawHorizontalLine (198, 46.0f, b.getRight() - 46.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (199, 46.0f, b.getRight() - 46.0f);
    g.setColour (faint);
    g.setFont (mono (9.5f, 0.05f));
    g.drawText (learning ? "MIDI LEARN: click a button, then press a pad / key on your controller   (MIDI again = done)"
                         : "1-5 BIG BUTTONS  Q-T STOP  A-G UNDO  Z-B CLEAR  SPACE ALL  ENTER TAP  B BEAT",
                46, 676, 760, 16, Justification::centredLeft);
}

void LoopView::timerCallback()
{
    if (message.isNotEmpty() && Time::getMillisecondCounterHiRes() > messageUntil) message = {};
    for (auto* s : strips) s->refresh();
    display->repaint();
    beatB->ledOn = engine.isBeatOn();
    beatB->setButtonText ("BEAT " + String (engine.isBeatOn() ? "ON" : "OFF") + String::fromUTF8 ("  \xc2\xb7  ")
                          + beatNames()[engine.getBeatStyle()].toUpperCase() + String::fromUTF8 ("  \xc2\xb7  ") + kitNames()[engine.kit.load()].toUpperCase());
    midiB->ledColour = learning ? cPlay : Colour (0x0);
    for (auto* b : { playAllB.get(), stopAllB.get(), beatB.get() }) b->targeted = false;
    if (learning)
    {
        playAllB->targeted = learnTarget == actPlayAll;
        stopAllB->targeted = learnTarget == actStopAll;
        beatB->targeted = learnTarget == actBeat;
    }
    for (auto* b : { playAllB.get(), stopAllB.get(), beatB.get(), midiB.get() }) b->repaint();

    if (autoLatency && devices != nullptr)
        if (auto* d = devices->getCurrentAudioDevice())
            engine.latency = d->getInputLatencyInSamples() + d->getOutputLatencyInSamples();
}

void LoopView::showMessage (const String& text)
{
    message = text;
    messageUntil = Time::getMillisecondCounterHiRes() + 2500.0;
    display->repaint();
}

//==== actions ====================================================================================================
void LoopView::target (int action)
{
    if (learning)
    {
        learnTarget = action;
        showMessage ("NOW PRESS A PAD / KEY FOR: " + actionName (action).toUpperCase());
        messageUntil = Time::getMillisecondCounterHiRes() + 60000.0;
        return;
    }
    doAction (action);
}

void LoopView::doAction (int a)
{
    if (a < actStop)          engine.press (a);
    else if (a < actUndo)     engine.stop (a - actStop);
    else if (a < actClear)    { if (! engine.undo (a - actUndo)) showMessage ("NOTHING TO UNDO ON TRACK " + String (a - actUndo + 1)); }
    else if (a < actPlayAll)  engine.clear (a - actClear);
    else switch (a)
    {
        case actPlayAll:   engine.playAll(); break;
        case actStopAll:   engine.stopAll(); break;
        case actToggleAll: engine.togglePlayAll(); break;
        case actBeat:      engine.setBeat (! engine.isBeatOn()); break;
        case actTap:
            if (engine.anyLoop()) showMessage ("THE TEMPO IS SET BY THE LOOPS");
            else { engine.tapTempo(); saveSettings(); }
            break;
        case actUndoLast:  if (! engine.undo (engine.lastTouched())) showMessage ("NOTHING TO UNDO"); break;
        default: break;
    }
}

bool LoopView::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::spaceKey)     { doAction (actToggleAll); return true; }
    if (k == KeyPress::backspaceKey) { doAction (actUndoLast); return true; }
    if (k == KeyPress::returnKey)    { doAction (actTap); return true; }
    const auto c = (juce_wchar) CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (c >= '1' && c <= '5') { doAction (actPress + (int) (c - '1')); return true; }
    const String rows[] { "qwert", "asdfg", "zxcvb" };
    const int base[] { actStop, actUndo, actClear };
    for (int r = 0; r < 3; ++r)
        if (const int i = rows[r].indexOfChar (c); i >= 0) { doAction (base[r] + i); return true; }
    if (c == 'b') { doAction (actBeat); return true; }
    return false;
}

void LoopView::handleMidi (const MidiMessage& m)
{
    bool isCc = false;
    int number = -1;
    if (m.isNoteOn()) number = m.getNoteNumber();
    else if (m.isController())
    {
        const int n = m.getControllerNumber(), v = m.getControllerValue();
        const bool rising = v >= 64 && lastCc[(size_t) n] < 64;
        lastCc[(size_t) n] = v;
        if (rising) { number = n; isCc = true; }
    }
    if (number < 0) return;

    if (learning && learnTarget >= 0)
    {
        bindings.erase (std::remove_if (bindings.begin(), bindings.end(), [&] (const Binding& b) { return b.cc == isCc && b.number == number; }), bindings.end());
        bindings.push_back ({ isCc, number, learnTarget });
        showMessage ((isCc ? "CC " : "NOTE ") + String (number) + String::fromUTF8 (" \xe2\x86\x92 ") + actionName (learnTarget).toUpperCase());
        learnTarget = -1;
        saveSettings();
        return;
    }
    for (auto& b : bindings)
        if (b.cc == isCc && b.number == number) doAction (b.action);
}

void LoopView::toggleLearn()
{
    learning = ! learning;
    learnTarget = -1;
    showMessage (learning ? "MIDI LEARN: CLICK A BUTTON, THEN PRESS A PAD" : "MIDI LEARN DONE");
    repaint();
}

//==== menus ======================================================================================================
void LoopView::openBeatMenu()
{
    PopupMenu m, styles, kits;
    for (int i = 0; i < beatNames().size(); ++i) styles.addItem (100 + i, beatNames()[i], true, engine.getBeatStyle() == i);
    for (int i = 0; i < kitNames().size(); ++i) kits.addItem (200 + i, kitNames()[i], true, engine.kit.load() == i);
    m.addItem (1, engine.isBeatOn() ? "Beat off" : "Beat on");
    m.addItem (2, "A new pattern (same style)");
    m.addSubMenu ("Style", styles);
    m.addSubMenu ("Kit", kits);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (beatB.get()), [this] (int r)
    {
        if (r == 1) engine.setBeat (! engine.isBeatOn());
        if (r == 2) engine.newBeat();
        if (r >= 100 && r < 200) engine.setBeatStyle (r - 100);
        if (r >= 200) engine.kit = r - 200;
        if (r > 0) saveSettings();
    });
}

void LoopView::openTempoMenu()
{
    PopupMenu m, beats;
    const bool locked = engine.anyLoop();
    for (int b : { 2, 3, 4, 5, 6, 7 }) beats.addItem (10 + b, String (b) + " / 4", ! locked, engine.beatsPerBar.load() == b);
    m.addSectionHeader (locked ? "Tempo (set by the loops - CLEAR ALL to change it)" : "Tempo");
    m.addItem (1, "Tap tempo (Enter)", ! locked);
    m.addItem (2, "Type a tempo ...", ! locked);
    m.addItem (3, "The first loop sets the tempo", ! locked, ! engine.isTempoKnown());
    m.addSubMenu ("Beats in a bar", beats, ! locked);
    m.addSeparator();
    m.addItem (4, "CLEAR ALL tracks", engine.anyLoop());
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (display.get()), [this] (int r)
    {
        if (r == 1) doAction (actTap);
        if (r == 2)
        {
            auto* w = new AlertWindow ("Tempo", "BPM (40 - 240):", MessageBoxIconType::NoIcon);
            w->addTextEditor ("bpm", String (engine.getBpm(), 1));
            w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
            w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
            w->enterModalState (true, ModalCallbackFunction::create ([this, w] (int ok)
            {
                if (ok == 1) { engine.setBpm (w->getTextEditorContents ("bpm").getDoubleValue()); saveSettings(); }
            }), true);
        }
        if (r == 3) { engine.resetTempo(); saveSettings(); }
        if (r == 4) engine.clearAll();
        if (r > 10 && r < 20)
        {
            engine.beatsPerBar = r - 10;
            if (engine.isTempoKnown()) engine.setBpm (engine.getBpm());
            saveSettings();
        }
    });
}

void LoopView::openSetup()
{
    PopupMenu m, lat;
    const int devMs = [this]
    {
        if (devices != nullptr)
            if (auto* d = devices->getCurrentAudioDevice())
                return roundToInt ((d->getInputLatencyInSamples() + d->getOutputLatencyInSamples()) * 1000.0 / jmax (1.0, d->getCurrentSampleRate()));
        return 0;
    }();
    lat.addItem (100, "Auto (the sound card says " + String (devMs) + " ms)", true, autoLatency);
    for (int ms : { 0, 5, 10, 15, 20, 30, 40, 60, 80 }) lat.addItem (200 + ms, String (ms) + " ms", true, ! autoLatency && manualLatencyMs == ms);
    m.addItem (1, "Sound card & MIDI ...", devices != nullptr);
    m.addSubMenu ("Latency (overdubs land where you heard them)", lat);
    m.addSeparator();
    m.addItem (2, "After REC: play", true, ! engine.recThenDub.load());
    m.addItem (3, "After REC: overdub", true, engine.recThenDub.load());
    m.addSeparator();
    m.addItem (4, "MIDI pads back to the default (36-48)");
    m.addItem (5, "Keys and pads ...");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (setupB.get()), [this] (int r)
    {
        if (r == 1 && devices != nullptr)
        {
            auto* sel = new AudioDeviceSelectorComponent (*devices, 0, 2, 1, 2, true, false, true, false);
            sel->setSize (520, 460);
            DialogWindow::LaunchOptions o;
            o.content.setOwned (sel);
            o.dialogTitle = "Cat Loop - sound card & MIDI";
            o.dialogBackgroundColour = Colour (0xff1b1d21);
            o.escapeKeyTriggersCloseButton = true;
            o.useNativeTitleBar = true;
            o.resizable = false;
            o.launchAsync();
        }
        if (r == 100) autoLatency = true;
        if (r >= 200) { autoLatency = false; manualLatencyMs = r - 200; engine.latency = roundToInt (manualLatencyMs * engine.getSampleRate() / 1000.0); }
        if (r == 2) engine.recThenDub = false;
        if (r == 3) engine.recThenDub = true;
        if (r == 4) { bindings = defaultBindings(); showMessage ("MIDI PADS: 36-40 TRACKS, 41 ALL, 42 UNDO, 43 BEAT, 44-48 STOP"); }
        if (r == 5)
        {
            String help;
            help << "Keys\n"
                 << "  1 - 5   the big buttons (REC > PLAY > DUB > PLAY)\n"
                 << "  Q W E R T   stop a track\n"
                 << "  A S D F G   undo / redo a track\n"
                 << "  Z X C V B   clear a track\n"
                 << "  Space   play / stop all     Backspace   undo (the last track)\n"
                 << "  Enter   tap the tempo        B   beat on / off\n\nMIDI\n";
            for (auto& b : bindings) help << "  " << (b.cc ? "CC " : "Note ") << b.number << "   " << actionName (b.action) << "\n";
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::NoIcon, "Cat Loop - keys and pads", help);
        }
        if (r > 0) saveSettings();
    });
}

//==== settings ===================================================================================================
void LoopView::saveSettings()
{
    if (props == nullptr) return;
    props->setValue ("quantize", engine.quantize.load());
    props->setValue ("click", engine.click.load());
    props->setValue ("input", engine.input.load());
    props->setValue ("kit", engine.kit.load());
    props->setValue ("beatStyle", engine.getBeatStyle());
    props->setValue ("beatSeed", (int) engine.getBeatSeed());
    props->setValue ("beatsPerBar", engine.beatsPerBar.load());
    props->setValue ("recThenDub", engine.recThenDub.load());
    props->setValue ("autoLatency", autoLatency);
    props->setValue ("latencyMs", manualLatencyMs);
    props->setValue ("inputKnob", inputKnob->value);
    props->setValue ("rhythmKnob", rhythmKnob->value);
    props->setValue ("masterKnob", masterKnob->value);
    props->setValue ("bpm", engine.getBpm());
    for (int t = 0; t < numTracks; ++t) props->setValue ("volume" + String (t), engine.volume (t).load());
    StringArray b;
    for (auto& x : bindings) b.add (String (x.cc ? "c" : "n") + String (x.number) + ":" + String (x.action));
    props->setValue ("bindings", b.joinIntoString (";"));
    props->saveIfNeeded();
}

void LoopView::loadSettings()
{
    if (props != nullptr)
    {
        engine.quantize = jlimit (0, 2, props->getIntValue ("quantize", 0));
        engine.click = jlimit (0, 2, props->getIntValue ("click", 1));
        engine.input = jlimit (0, 2, props->getIntValue ("input", 0));
        engine.kit = jlimit (0, kitNames().size() - 1, props->getIntValue ("kit", 1));
        engine.beatsPerBar = jlimit (2, 7, props->getIntValue ("beatsPerBar", 4));
        engine.setBeatPattern (props->getIntValue ("beatStyle", 0), props->getIntValue ("beatSeed", 1));
        engine.recThenDub = props->getBoolValue ("recThenDub", false);
        autoLatency = props->getBoolValue ("autoLatency", true);
        manualLatencyMs = props->getIntValue ("latencyMs", 0);
        for (int t = 0; t < numTracks; ++t) engine.volume (t) = (float) props->getDoubleValue ("volume" + String (t), 0.8);
        inputKnob->setValue ((float) props->getDoubleValue ("inputKnob", 0.0), false);
        rhythmKnob->setValue ((float) props->getDoubleValue ("rhythmKnob", 0.6), false);
        masterKnob->setValue ((float) props->getDoubleValue ("masterKnob", 0.75), false);
        const auto saved = props->getValue ("bindings");
        if (saved.isNotEmpty())
        {
            bindings.clear();
            for (auto& s : StringArray::fromTokens (saved, ";", {}))
                if (s.length() > 2) bindings.push_back ({ s[0] == 'c', s.substring (1).getIntValue(), jlimit (0, numActions - 1, s.fromFirstOccurrenceOf (":", false, false).getIntValue()) });
        }
    }
    inputKnob->onChange (inputKnob->value);
    rhythmKnob->onChange (rhythmKnob->value);
    masterKnob->onChange (masterKnob->value);
    if (! autoLatency) engine.latency = roundToInt (manualLatencyMs * engine.getSampleRate() / 1000.0);
}

//==== sessions ===================================================================================================
static MemoryBlock wavBytes (const AudioBuffer<float>& b, double sr, int bits)
{
    MemoryBlock mb;
    WavAudioFormat wav;
    if (auto w = std::unique_ptr<AudioFormatWriter> (wav.createWriterFor (new MemoryOutputStream (mb, false), sr, (unsigned int) b.getNumChannels(), bits, {}, 0)))
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    return mb;
}

bool LoopView::saveSession (const File& file)
{
    ZipFile::Builder zip;
    XmlElement xml ("CATLOOP");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("sampleRate", engine.getSampleRate());
    xml.setAttribute ("barLength", engine.getBarLength());
    xml.setAttribute ("beatsPerBar", engine.beatsPerBar.load());
    xml.setAttribute ("tempoKnown", engine.isTempoKnown());
    xml.setAttribute ("beatStyle", engine.getBeatStyle());
    xml.setAttribute ("beatSeed", (int) engine.getBeatSeed());
    xml.setAttribute ("kit", engine.kit.load());
    xml.setAttribute ("beat", engine.isBeatOn());
    for (int t = 0; t < numTracks; ++t)
    {
        auto* tr = xml.createNewChildElement ("TRACK");
        tr->setAttribute ("index", t);
        tr->setAttribute ("volume", engine.volume (t).load());
        auto loop = engine.getLoop (t);
        if (loop.getNumSamples() > 0)
        {
            const String name = "track" + String (t + 1) + ".wav";
            tr->setAttribute ("file", name);
            zip.addEntry (std::make_unique<MemoryInputStream> (wavBytes (loop, engine.getSampleRate(), 32), true), 6, name, Time::getCurrentTime());
        }
    }
    zip.addEntry (std::make_unique<MemoryInputStream> (MemoryBlock (xml.toString().toRawUTF8(), xml.toString().getNumBytesAsUTF8()), true), 9, "session.xml", Time::getCurrentTime());
    file.deleteFile();
    FileOutputStream out (file);
    return out.openedOk() && zip.writeToStream (out, nullptr);
}

bool LoopView::loadSession (const File& file)
{
    ZipFile zip (file);
    const int xi = zip.getIndexOfFileName ("session.xml");
    if (xi < 0) return false;
    std::unique_ptr<InputStream> xs (zip.createStreamForEntry (xi));
    if (xs == nullptr) return false;
    auto xml = parseXML (xs->readEntireStreamAsString());
    if (xml == nullptr || ! xml->hasTagName ("CATLOOP")) return false;
    if (engine.anyLoop()) return false;                  // the caller clears first

    const double fileSr = xml->getDoubleAttribute ("sampleRate", engine.getSampleRate());
    const double ratio = engine.getSampleRate() / fileSr;
    engine.beatsPerBar = jlimit (2, 7, xml->getIntAttribute ("beatsPerBar", 4));
    if (xml->getBoolAttribute ("tempoKnown")) engine.setTempoFromSession (xml->getDoubleAttribute ("barLength") * ratio, engine.beatsPerBar.load());
    engine.setBeatPattern (xml->getIntAttribute ("beatStyle"), xml->getIntAttribute ("beatSeed", 1));
    engine.kit = xml->getIntAttribute ("kit", 1);
    WavAudioFormat wav;
    for (auto* tr : xml->getChildWithTagNameIterator ("TRACK"))
    {
        const int t = tr->getIntAttribute ("index");
        if (! isPositiveAndBelow (t, numTracks)) continue;
        engine.volume (t) = (float) tr->getDoubleAttribute ("volume", 0.8);
        const int wi = zip.getIndexOfFileName (tr->getStringAttribute ("file"));
        if (wi < 0) continue;
        std::unique_ptr<AudioFormatReader> reader (wav.createReaderFor (zip.createStreamForEntry (wi), true));
        if (reader == nullptr) continue;
        AudioBuffer<float> b (2, (int) reader->lengthInSamples);
        reader->read (&b, 0, b.getNumSamples(), 0, true, true);
        if (std::abs (ratio - 1.0) > 1.0e-9)
        {
            // another sample rate: resample, so the loops play at the right speed
            AudioBuffer<float> r (2, (int) std::llround (b.getNumSamples() * ratio));
            for (int ch = 0; ch < 2; ++ch)
            {
                LagrangeInterpolator li;
                li.process (1.0 / ratio, b.getReadPointer (ch), r.getWritePointer (ch), r.getNumSamples(), b.getNumSamples(), 0);
            }
            b = std::move (r);
        }
        engine.setLoop (t, b);
    }
    if (xml->getBoolAttribute ("beat")) engine.setBeat (true);
    return true;
}

bool LoopView::exportMix (const File& f)
{
    auto mix = engine.mixdown (engine.isBeatOn());
    if (mix.getNumSamples() <= 1) return false;
    f.deleteFile();
    auto bytes = wavBytes (mix, engine.getSampleRate(), 24);
    return f.replaceWithData (bytes.getData(), bytes.getSize());
}

bool LoopView::exportStems (const File& folder)
{
    folder.createDirectory();
    int n = 0;
    for (int t = 0; t < numTracks; ++t)
    {
        auto s = engine.stem (t);
        if (s.getNumSamples() <= 0) continue;
        auto bytes = wavBytes (s, engine.getSampleRate(), 24);
        n += folder.getChildFile ("Track " + String (t + 1) + ".wav").replaceWithData (bytes.getData(), bytes.getSize()) ? 1 : 0;
    }
    if (engine.isBeatOn())
    {
        auto bytes = wavBytes (engine.beatStem(), engine.getSampleRate(), 24);
        n += folder.getChildFile ("Beat.wav").replaceWithData (bytes.getData(), bytes.getSize()) ? 1 : 0;
    }
    return n > 0;
}

void LoopView::doSave()
{
    if (! engine.anyLoop()) { showMessage ("NOTHING TO SAVE YET"); return; }
    chooser = std::make_unique<FileChooser> ("Save the set", File::getSpecialLocation (File::userMusicDirectory).getChildFile ("My set.catloop"), "*.catloop");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting, [this] (const FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == File()) return;
        if (! f.hasFileExtension ("catloop")) f = f.withFileExtension ("catloop");
        showMessage (saveSession (f) ? "SAVED " + f.getFileName().toUpperCase() : "COULD NOT SAVE");
    });
}

void LoopView::doLoad()
{
    chooser = std::make_unique<FileChooser> ("Open a set", File::getSpecialLocation (File::userMusicDirectory), "*.catloop");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == File()) return;
        engine.stopAll();
        engine.clearAll();
        // the audio thread empties the tracks; then the set goes in
        auto tries = std::make_shared<int> (0);
        auto attempt = std::make_shared<std::function<void()>>();
        Component::SafePointer<LoopView> safe (this);
        *attempt = [safe, f, tries, attempt]
        {
            if (safe == nullptr) return;
            if (safe->engine.anyLoop() && ++*tries < 40) { Timer::callAfterDelay (50, *attempt); return; }
            safe->showMessage (safe->loadSession (f) ? "LOADED " + f.getFileNameWithoutExtension().toUpperCase() + String::fromUTF8 (" \xc2\xb7 PRESS PLAY ALL") : "COULD NOT OPEN " + f.getFileName().toUpperCase());
        };
        Timer::callAfterDelay (50, *attempt);
    });
}

void LoopView::doExport()
{
    if (! engine.anyLoop()) { showMessage ("NOTHING TO EXPORT YET"); return; }
    PopupMenu m;
    m.addItem (1, "The mix (.wav)");
    m.addItem (2, "Every track as its own .wav (stems, for your DAW)");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (exportB.get()), [this] (int r)
    {
        if (r == 1)
        {
            chooser = std::make_unique<FileChooser> ("Export the mix", File::getSpecialLocation (File::userMusicDirectory).getChildFile ("Cat Loop mix.wav"), "*.wav");
            chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting, [this] (const FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == File()) return;
                if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
                showMessage (exportMix (f) ? "EXPORTED " + f.getFileName().toUpperCase() : "COULD NOT EXPORT");
            });
        }
        if (r == 2)
        {
            chooser = std::make_unique<FileChooser> ("A folder for the stems", File::getSpecialLocation (File::userMusicDirectory));
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
            {
                const auto d = fc.getResult();
                if (d == File()) return;
                showMessage (exportStems (d) ? "STEMS SAVED IN " + d.getFileName().toUpperCase() : "COULD NOT EXPORT");
            });
        }
    });
}

//==== the UI stress test ==========================================================================================
void LoopView::randomPoke (Random& r)
{
    const int what = r.nextInt (100);
    if (what < 55) doAction (r.nextInt (numActions));
    else if (what < 62) strips[r.nextInt (numTracks)]->setFader (r.nextFloat() * 200.0f);
    else if (what < 68) { auto* k = std::array<Knob*, 3> { inputKnob.get(), rhythmKnob.get(), masterKnob.get() }[(size_t) r.nextInt (3)]; k->setValue (r.nextFloat(), true); }
    else if (what < 72) engine.quantize = r.nextInt (3);
    else if (what < 75) engine.click = r.nextInt (3);
    else if (what < 78) engine.input = r.nextInt (3);
    else if (what < 81) { engine.kit = r.nextInt (kitNames().size()); engine.setBeatStyle (r.nextInt (beatNames().size())); }
    else if (what < 84) keyPressed (KeyPress ((int) String ("12345qwertasdfgzxcvbB")[r.nextInt (21)]));
    else if (what < 86) { learning = ! learning; learnTarget = r.nextInt (numActions); }
    else if (what < 90) handleMidi (MidiMessage::noteOn (1, 36 + r.nextInt (13), (uint8) 100));
    else if (what < 93) handleMidi (MidiMessage::controllerEvent (1, 20 + r.nextInt (4), r.nextInt (128)));
    else if (what < 95) engine.setBpm (60.0 + r.nextInt (120));
    else if (what < 97)
    {
        const auto f = File::getSpecialLocation (File::tempDirectory).getChildFile ("catloop-stress.catloop");
        saveSession (f);
        exportMix (f.withFileExtension ("wav"));
    }
    else engine.clearAll();
    if (learning && r.nextInt (6) == 0) learning = false;
}
} // namespace looper

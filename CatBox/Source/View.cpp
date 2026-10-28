#include "View.h"
#include "Music.h"
#include "Samples.h"
#include "Synth.h"
#include <cmath>
#include <complex>

namespace catbox
{
namespace
{
// calm: greys, one accent; red only for Rec. The theme (PROJECT > LOOK) swaps them.
juce::Colour bg       { 0xff0f1012 };
juce::Colour panel    { 0xff18191c };
juce::Colour cell     { 0xff1f2024 };
juce::Colour line     { 0xff2c2e33 };
juce::Colour text     { 0xffeeeeee };
juce::Colour dim      { 0xff8b8e95 };
juce::Colour faint    { 0xff4a4d54 };
juce::Colour accent   { 0xffff8a3d };
const juce::Colour recRed   { 0xffff453a };
const juce::Colour hwBody   { 0xff2a2b2f };
const juce::Colour hwKey    { 0xff3a3c41 };
float fontScale = 1.0f;
bool trackColours = false, rings = false;

struct Theme { juce::uint32 bg, panel, cell, line, text, dim, faint; };
const Theme themes[] = {
    { 0xff0f1012, 0xff18191c, 0xff1f2024, 0xff2c2e33, 0xffeeeeee, 0xff8b8e95, 0xff4a4d54 },   // Graphite
    { 0xfff2efe9, 0xffe6e2da, 0xffdcd7cd, 0xffc9c3b7, 0xff1d1c1a, 0xff6b665e, 0xffa39d92 },   // Paper
    { 0xff0b1020, 0xff121a30, 0xff18223d, 0xff26314f, 0xffe8ecf7, 0xff8b95b3, 0xff47506b },   // Midnight
    { 0xff0d1410, 0xff142019, 0xff1a2a21, 0xff2a3d31, 0xffe7f0e9, 0xff8aa392, 0xff4a5e51 },   // Forest
    { 0xff000000, 0xff0a0f0a, 0xff0f170f, 0xff1c2a1c, 0xffb8ffb8, 0xff5fa35f, 0xff2f5a2f },   // Terminal
    { 0xff000000, 0xff101010, 0xff1a1a1a, 0xff555555, 0xffffffff, 0xffd0d0d0, 0xff888888 },   // High contrast
    { 0xff150f17, 0xff1f1622, 0xff281d2c, 0xff3a2b3f, 0xfff3eaf5, 0xffa291a8, 0xff5d4e62 },   // Plum
    { 0xff1a1612, 0xff241f19, 0xff2d271f, 0xff3d352a, 0xfff4ecdf, 0xffa99b86, 0xff63594a },   // Sand
};
const juce::uint32 accents[] = { 0xffff8a3d, 0xffffc93d, 0xff4cd97b, 0xff3dd6e0, 0xff4d8dff, 0xffa77bff, 0xffff6bb5, 0xffff5a4f, 0xfff0f0f0 };
const juce::uint32 trackHues[] = { 0xffff6b5a, 0xffffa23d, 0xffffd93d, 0xff6fdc6f, 0xff3dd6c6, 0xff4d9dff, 0xffa77bff, 0xffff6bc1 };

void applyLook (const Look& l)
{
    const auto& t = themes[juce::jlimit (0, 7, l.theme)];
    bg = juce::Colour (t.bg); panel = juce::Colour (t.panel); cell = juce::Colour (t.cell); line = juce::Colour (t.line);
    text = juce::Colour (t.text); dim = juce::Colour (t.dim); faint = juce::Colour (t.faint);
    accent = juce::Colour (accents[juce::jlimit (0, 8, l.accent)]);
    if (l.theme == 1 && l.accent == 8) accent = juce::Colour (0xff1d1c1a);   // white on paper: ink instead
    fontScale = l.textSize > 0 ? 1.12f : 1.0f;
    trackColours = l.trackColours;
    rings = l.knobStyle == 1;
}

juce::Colour trackColour (int t) { return juce::Colour (trackHues[juce::jlimit (0, 7, t)]); }

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size * fontScale, bold ? juce::Font::bold : juce::Font::plain));
}

void text1 (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, float size, juce::Colour c,
            juce::Justification j = juce::Justification::centredLeft, bool bold = false)
{
    g.setColour (c);
    g.setFont (font (size, bold));
    g.drawFittedText (s, r, j, 1, 0.8f);
}

const char* shortEngine (int e)
{
    static const char* n[] = { "ANLG", "ACID", "DRUM", "KEYS", "GLDE", "FM", "WAVE", "CHOR", "SMPL", "GRAN", "KIT", "SYN" };
    return n[juce::jlimit (0, NumEngineTypes - 1, e)];
}

const int modeOrder[] = { ModeSound, ModeSeq, ModeFx, ModeMix, ModeProject };
const juce::String sep = juce::String::fromUTF8 ("  \xc2\xb7  ");
} // namespace

//==============================================================================
View::View (Controller& c, bool k) : box (c), kiosk (k)
{
    setWantsKeyboardFocus (true);
    setOpaque (true);
    sliderPos = box.getProject().m.djFilter;
    startTimerHz (30);
}

View::~View() { stopTimer(); }

void View::timerCallback()
{
    phase += 1.0f / 30.0f;
    auto& a = box.getAudio();
    for (int t = 0; t < numTracks; ++t)
    {
        const float lv = box.isRemote() ? a.level[(size_t) t].load() : a.level[(size_t) t].exchange (0.0f);
        meters[(size_t) t] = juce::jmax (lv, meters[(size_t) t] * 0.85f);
        const uint32_t h = a.hits[(size_t) t].exchange (0u);
        if (h != 0) hitGlow[(size_t) t] = 1.0f;
        else hitGlow[(size_t) t] *= 0.82f;
        if (t == box.getTrack())
            for (int l = 0; l < 16; ++l) laneGlow[(size_t) l] = (h >> l) & 1u ? 1.0f : laneGlow[(size_t) l] * 0.8f;
    }
    repaint();
}

void View::resized()
{
    const auto lb = logicalBounds();
    scale = juce::jmin ((float) getWidth() / (float) lb.getWidth(), (float) getHeight() / (float) lb.getHeight());
    offset = { ((float) getWidth() - lb.getWidth() * scale) * 0.5f, ((float) getHeight() - lb.getHeight() * scale) * 0.5f };
}

juce::Point<float> View::toLogical (juce::Point<float> p) const
{
    return (p - offset) / scale;
}

juce::Rectangle<int> View::screenRect() const
{
    return kiosk ? juce::Rectangle<int> (0, 0, screenW, screenH) : juce::Rectangle<int> (24, 24, screenW, screenH);
}

juce::Image View::snapshotScreen()
{
    juce::Image img (juce::Image::RGB, screenW, screenH, true);
    juce::Graphics g (img);
    drawScreen (g);
    return img;
}

//==============================================================================
void View::paint (juce::Graphics& g)
{
    applyLook (box.getLook());
    g.fillAll (kiosk ? bg : juce::Colour (0xff141518));
    g.addTransform (juce::AffineTransform::scale (scale).translated (offset.x, offset.y));
    if (! kiosk) drawPanel (g);
    const auto sr = screenRect();
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (sr);
    g.addTransform (juce::AffineTransform::translation ((float) sr.getX(), (float) sr.getY()));
    drawScreen (g);
}

void View::drawScreen (juce::Graphics& g)
{
    applyLook (box.getLook());
    g.setColour (bg);
    g.fillRect (0, 0, screenW, screenH);
    auto r = juce::Rectangle<int> (0, 0, screenW, screenH);
    panelHits.clearQuick();
    seqHits.clearQuick();
    actionHits.clearQuick();
    menuHits.clearQuick();
    drawHeader (g, r.removeFromTop (52));
    drawTabs (g, r.removeFromTop (34));
    const int scr = box.getScreen();
    if (scr == Controller::ScreenStudio)
    {
        auto tracks = r.removeFromBottom (32);
        auto knobs = r.removeFromBottom (134);
        knobArea = knobs;
        // one line of help above the knobs: what the buttons do now
        if (box.getLook().hintLine || box.assigning() >= 0)
        {
            auto hintRow = r.removeFromBottom (20);
            text1 (g, box.hint(), hintRow.reduced (12, 0), 12.5f, box.assigning() >= 0 ? accent : dim, juce::Justification::centredLeft, box.assigning() >= 0);
        }
        drawContent (g, r.reduced (10, 4));
        drawKnobs (g, knobs);
        drawTracks (g, tracks);
    }
    else
    {
        // SYNTH and DRUMS: the whole screen for the instrument, no track strip, no knob row
        knobArea = {};
        auto hintRow = r.removeFromBottom (18);
        if (synthPanelShown() && kiosk) { auto knobs = r.removeFromBottom (134); knobArea = knobs; drawSynthLive (g, r); drawKnobs (g, knobs); }
        else if (synthPanelShown()) drawSynthPanel (g, r);
        else if (scr == Controller::ScreenDrums && box.getMode() == ModeSeq) drawDrumMachine (g, r.reduced (8, 4));
        else drawContent (g, r.reduced (10, 4));
        text1 (g, scr == Controller::ScreenSynth ? (kiosk ? juce::String ("The pots play the sound - the 12 knobs below: ") + box.pageName() + juce::String (" (Prev / Next: pages, push + turn: the 2nd layer)")
                                                        : juce::String ("Drag a knob (Ctrl = fine) - wheel - double-click = back to start - click the model to change it - numbers = your 12 knobs"))
                                                 : juce::String ("Click a step to turn it on / off - click a drum name to hear it - Space = play"),
               hintRow.reduced (12, 0), 11.5f, dim, juce::Justification::centredLeft);
    }

    if (box.isLearning()) drawLearn (g, juce::Rectangle<int> (0, 52, screenW, screenH - 52));
    if (box.isHelpShown()) drawHelp (g, juce::Rectangle<int> (0, 0, screenW, screenH));

    // the knob turned last on the box: big on the box's screen, a line on the PC
    drawTouch (g, juce::Rectangle<int> (0, 86, screenW, screenH - 86 - 134), kiosk && box.getScreen() != Controller::ScreenStudio);
    // a short message, on top of the content
    const auto f = box.flashText();
    if (f.isNotEmpty() && ! box.isLearning())
    {
        g.setFont (font (16.0f, true));
        const int w = juce::jmin (screenW - 40, juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), f) + 36);
        const juce::Rectangle<int> pill ((screenW - w) / 2, screenH - 32 - 134 - 22 - 38, w, 32);   // low in the content: the titles stay readable
        g.setColour (juce::Colour (0xf0282a2f));
        g.fillRoundedRectangle (pill.toFloat(), 16.0f);
        g.setColour (line.brighter (0.2f));
        g.drawRoundedRectangle (pill.toFloat(), 16.0f, 1.0f);
        text1 (g, f, pill, 16.0f, text, juce::Justification::centred, true);
    }
}

void View::drawHeader (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& p = box.getProject();
    const int t = box.getTrack();
    const auto& tr = p.tracks[(size_t) t];
    g.setColour (panel);
    g.fillRect (r);
    r = r.reduced (12, 6);

    // the track: number, engine, sound
    auto num = r.removeFromLeft (40);
    g.setColour (trackColours ? trackColour (t) : text);
    g.fillRoundedRectangle (num.toFloat().reduced (0, 2), 6.0f);
    text1 (g, juce::String (t + 1), num, 24.0f, trackColours ? juce::Colour (0xff111111) : bg, juce::Justification::centred, true);
    r.removeFromLeft (10);

    // right side: play, rec, tempo, position
    auto right = r.removeFromRight (250);
    const bool playing = box.getAudio().isPlaying(), rec = box.isRecArmed();
    auto icon = right.removeFromRight (40).toFloat().reduced (8, 8);
    playHit = icon.toNearestInt().expanded (8);
    g.setColour (playing ? accent : dim);
    if (playing)
    {
        juce::Path tri;
        tri.addTriangle (icon.getX(), icon.getY(), icon.getX(), icon.getBottom(), icon.getRight(), icon.getCentreY());
        g.fillPath (tri);
    }
    else g.fillRect (icon.reduced (2));
    auto recR = right.removeFromRight (30).toFloat().withSizeKeepingCentre (14, 14);
    g.setColour (rec ? recRed : faint);
    if (rec) g.fillEllipse (recR); else g.drawEllipse (recR, 1.6f);
    const double beats = box.getAudio().songBeats.load();
    const int barN = (int) (beats / 4.0) + 1, beatN = (int) std::fmod (beats, 4.0) + 1;
    text1 (g, playing ? juce::String (barN) + "." + juce::String (beatN) : juce::String ("-.-"), right.removeFromRight (56), 18.0f, playing ? text : dim, juce::Justification::centred, true);
    text1 (g, juce::String (juce::roundToInt (p.m.tempo)), right.removeFromRight (60), 26.0f, text, juce::Justification::centredRight, true);
    text1 (g, "BPM", right.removeFromRight (40).withTrimmedLeft (4), 12.0f, dim, juce::Justification::centredLeft);
    learnHit = {};

    modelHit = {};
    if (synthPanelShown())   // the SYNTH: the model's name, click = the next model
    {
        const int mdl = juce::roundToInt (tr.params[(size_t) engineDef (EngineSynth).index ("model")]);
        const auto& lk = synthModelLook (mdl);
        auto mr = r.removeFromLeft (150);
        modelHit = mr;
        g.setColour (juce::Colour (lk.accent));
        g.fillRoundedRectangle (mr.toFloat().reduced (0, 2), 6.0f);
        const auto onAcc = juce::Colour (lk.accent).contrasting (0.9f);
        text1 (g, lk.name, mr.withTrimmedBottom (14), 20.0f, onAcc, juce::Justification::centred, true);
        text1 (g, juce::String ("after ") + lk.inspired, mr.withTrimmedTop (22), 9.5f, onAcc.withAlpha (0.8f), juce::Justification::centred);
        r.removeFromLeft (10);
    }
    // engine above, sound name big
    auto top = r.removeFromTop (14);
    text1 (g, engineDef (tr.engine).name.toUpperCase() + (tr.fxType != FxOff ? "   +  " + noteFxNames()[tr.fxType].toUpperCase() : juce::String()),
           top, 12.0f, dim, juce::Justification::centredLeft, true);
    soundNameHit = r;
    text1 (g, box.presetName (t) + (box.isEdited (t) ? " *" : ""), r, 24.0f, text, juce::Justification::centredLeft, true);
    if (! kiosk)   // on the PC: a hint that the name opens the sound browser
    {
        g.setFont (font (24.0f, true));
        const int nw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), box.presetName (t) + (box.isEdited (t) ? " *" : ""));
        text1 (g, box.getMode() == ModeBrowse ? juce::String::fromUTF8 ("\xe2\x96\xb2") : juce::String::fromUTF8 ("\xe2\x96\xbc"),
               r.withTrimmedLeft (juce::jmin (r.getWidth() - 20, nw + 8 * (int) fontScale)).withWidth (20), 12.0f, dim, juce::Justification::centredLeft);
    }
}

void View::drawTabs (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (line);
    g.drawHorizontalLine (r.getBottom() - 1, 0.0f, (float) screenW);
    r = r.reduced (10, 0);
    modeHits.clearQuick();
    pageHits.clearQuick();
    screenHits.clearQuick();
    const int m = box.getMode();
    const int scr = box.getScreen();
    // the three screens: SYNTH, DRUMS, STUDIO (the studio then shows its own modes)
    {
        const char* names[] = { "SYNTH", "DRUMS", "STUDIO" };
        for (int s = 0; s < 3; ++s)
        {
            if (scr == Controller::ScreenStudio && s != 2) continue;
            const juce::String name = scr == Controller::ScreenStudio ? juce::String::fromUTF8 ("\xe2\x97\x80 SYNTH") : juce::String (names[s]);
            g.setFont (font (14.0f, true));
            const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 22;
            auto cellR = r.removeFromLeft (w);
            const bool on = scr == s && scr != Controller::ScreenStudio;
            if (on) { g.setColour (accent); g.fillRect (cellR.removeFromBottom (3).reduced (6, 0)); }
            text1 (g, name, cellR, 14.0f, on ? text : (scr == Controller::ScreenStudio ? accent : dim), juce::Justification::centred, true);
            screenHits.add ({ cellR.withHeight (34).withY (52), scr == Controller::ScreenStudio ? 0 : s });
        }
        if (scr == Controller::ScreenStudio) { g.setColour (line); g.drawVerticalLine (r.getX() + 2, 60.0f, 80.0f); r.removeFromLeft (8); }
    }
    for (int mm : modeOrder)
    {
        if (scr != Controller::ScreenStudio) break;
        const juce::String name = mm == ModeSound && m == ModeBrowse ? "BROWSE" : (mm == ModeProject && m == ModeMenu ? "MENU" : modeName (mm));
        g.setFont (font (14.0f, true));
        const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 18;
        auto cellR = r.removeFromLeft (w);
        const bool on = mm == m || (mm == ModeSound && m == ModeBrowse) || (mm == ModeProject && m == ModeMenu);
        if (on)
        {
            g.setColour (accent);
            g.fillRect (cellR.removeFromBottom (3).reduced (6, 0));
        }
        text1 (g, name, cellR, 14.0f, on ? text : dim, juce::Justification::centred, true);
        modeHits.add (cellR.withHeight (34).withY (52));
    }
    // pages (right): the current one bright, ◀ ▶ around
    const auto pages = box.pageNames();
    if (pages.size() > 1 || m == ModeFx)
    {
        auto pr = r.withTrimmedLeft (20);
        g.setFont (font (13.0f, true));
        int total = 0;
        for (auto& s : pages) total += juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) + 16;
        pr = pr.removeFromRight (juce::jmin (pr.getWidth(), total + 40));
        text1 (g, juce::String::fromUTF8 ("\xe2\x97\x80"), pr.removeFromLeft (18), 11.0f, dim, juce::Justification::centred);
        text1 (g, juce::String::fromUTF8 ("\xe2\x96\xb6"), pr.removeFromRight (18), 11.0f, dim, juce::Justification::centred);
        for (int i = 0; i < pages.size(); ++i)
        {
            const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), pages[i]) + 16;
            auto c = pr.removeFromLeft (w);
            const bool on = i == box.pageIndex();
            if (on)
            {
                g.setColour (cell.brighter (0.15f));
                g.fillRoundedRectangle (c.toFloat().reduced (1, 6), 4.0f);
            }
            text1 (g, pages[i], c, 13.0f, on ? text : faint, juce::Justification::centred, true);
            pageHits.add (c.withY (52).withHeight (34));
        }
    }
    if (box.isShift())
    {
        auto s = juce::Rectangle<int> (screenW / 2 - 40, 58, 80, 22);
        g.setColour (text);
        g.fillRoundedRectangle (s.toFloat(), 4.0f);
        text1 (g, "SHIFT", s, 13.0f, bg, juce::Justification::centred, true);
    }
}

void View::drawKnobs (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (panel);
    g.fillRect (r);
    const int cw = screenW / 6, ch = r.getHeight() / 2;
    for (int k = 0; k < numKnobs; ++k)
    {
        const auto v = box.knob (k);
        auto c = juce::Rectangle<int> (r.getX() + (k % 6) * cw, r.getY() + (k / 6) * ch, cw, ch).reduced (4, 3);
        g.setColour (cell);
        g.fillRoundedRectangle (c.toFloat(), 6.0f);
        if (! v.used) { text1 (g, juce::String (k + 1), c.reduced (8, 4), 12.0f, faint, juce::Justification::topLeft); continue; }
        auto inner = c.reduced (8, 5);
        const float n0 = juce::jlimit (0.0f, 1.0f, v.norm);
        if (rings)   // a ring on the right, like the encoder
        {
            auto ringR = inner.removeFromRight (40).toFloat().withSizeKeepingCentre (34, 34);
            const float a0 = -2.35f, a1 = 2.35f, cx = ringR.getCentreX(), cy = ringR.getCentreY(), rad = 14.0f;
            juce::Path back; back.addCentredArc (cx, cy, rad, rad, 0.0f, a0, a1, true);
            g.setColour (line); g.strokePath (back, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const float from = v.bipolar ? 0.0f : a0, to = a0 + (a1 - a0) * n0;
            juce::Path arc; arc.addCentredArc (cx, cy, rad, rad, 0.0f, juce::jmin (from, to), juce::jmax (from, to) + 0.001f, true);
            g.setColour (box.isShift() ? text : accent); g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            text1 (g, v.name, inner.removeFromTop (20), 13.0f, dim, juce::Justification::centredLeft);
            text1 (g, v.value, inner, 18.0f, text, juce::Justification::centredLeft, true);
            continue;
        }
        // the ring as a short bar: where the value is (from the middle for +/- values)
        auto bar = inner.removeFromBottom (5).toFloat();
        g.setColour (line);
        g.fillRoundedRectangle (bar, 2.5f);
        g.setColour (box.isShift() ? text : accent);
        const float n = juce::jlimit (0.0f, 1.0f, v.norm);
        if (v.bipolar)
        {
            const float mid = bar.getCentreX(), x = bar.getX() + bar.getWidth() * n;
            g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (mid, x), bar.getY(), juce::jmax (2.0f, std::abs (x - mid)), bar.getHeight()), 2.5f);
        }
        else g.fillRoundedRectangle (bar.withWidth (juce::jmax (4.0f, bar.getWidth() * n)), 2.5f);
        inner.removeFromBottom (2);
        text1 (g, v.name, inner.removeFromTop (17), 13.0f, dim, juce::Justification::centredLeft);
        text1 (g, v.value, inner, 19.0f, text, juce::Justification::centredLeft, true);
    }
}

void View::drawTracks (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (bg);
    g.fillRect (r);
    trackHits.clearQuick();
    const auto& p = box.getProject();
    const int w = screenW / numTracks;
    for (int t = 0; t < numTracks; ++t)
    {
        auto c = juce::Rectangle<int> (t * w, r.getY(), w, r.getHeight()).reduced (3, 4);
        trackHits.add (c);
        const auto& tr = p.tracks[(size_t) t];
        const bool sel = t == box.getTrack();
        g.setColour (sel ? text : cell);
        g.fillRoundedRectangle (c.toFloat(), 4.0f);
        if (trackColours)
        {
            g.setColour (trackColour (t).withAlpha (tr.mute ? 0.35f : 1.0f));
            g.fillRoundedRectangle (c.toFloat().removeFromBottom (3.0f).reduced (4.0f, 0.0f), 1.5f);
        }
        const juce::Colour fg = sel ? bg : (tr.mute ? faint : dim);
        text1 (g, juce::String (t + 1), c.withWidth (22), 14.0f, fg, juce::Justification::centred, true);
        text1 (g, shortEngine (tr.engine), c.withTrimmedLeft (22).withTrimmedRight (16), 12.0f, fg, juce::Justification::centredLeft, true);
        // activity: a dot that lights when it plays; M = muted, S = solo
        auto dot = c.removeFromRight (16).toFloat().withSizeKeepingCentre (7, 7);
        if (tr.mute) text1 (g, "M", dot.toNearestInt().expanded (5), 11.0f, fg, juce::Justification::centred, true);
        else if (tr.solo) text1 (g, "S", dot.toNearestInt().expanded (5), 11.0f, sel ? bg : accent, juce::Justification::centred, true);
        else
        {
            const float glow = juce::jmax (hitGlow[(size_t) t], meters[(size_t) t] > 0.01f ? 0.35f : 0.0f);
            g.setColour ((sel ? bg : accent).withAlpha (0.2f + 0.8f * glow));
            g.fillEllipse (dot);
        }
    }
}

void View::drawContent (juce::Graphics& g, juce::Rectangle<int> r)
{
    if (box.isSceneMode()) { drawScenes (g, r); return; }
    switch (box.getMode())
    {
        case ModeSound:   drawSound (g, r); break;
        case ModeBrowse:  drawBrowser (g, r); break;
        case ModeSeq:     drawSeq (g, r); break;
        case ModeFx:      drawFx (g, r); break;
        case ModeMix:     drawMix (g, r); break;
        case ModeProject: drawProject (g, r); break;
        case ModeMenu:    drawMenu (g, r); break;
        default: break;
    }
}

void View::drawScope (juce::Graphics& g, juce::Rectangle<int> r)
{
    auto& a = box.getAudio();
    const int n = Audio::scopeSize;
    const int w = a.scopeWrite.load (std::memory_order_relaxed);
    // start at a rising zero crossing so the wave stands still
    int start = 0;
    for (int i = 1; i < n / 2; ++i)
    {
        const float x0 = a.scope[(size_t) ((w + i - 1) % n)].load (std::memory_order_relaxed), x1 = a.scope[(size_t) ((w + i) % n)].load (std::memory_order_relaxed);
        if (x0 <= 0.0f && x1 > 0.0f) { start = i; break; }
    }
    const int len = n / 2;
    juce::Path path;
    const float mid = (float) r.getCentreY(), h = r.getHeight() * 0.45f;
    for (int i = 0; i < len; ++i)
    {
        const float v = juce::jlimit (-1.0f, 1.0f, a.scope[(size_t) ((w + start + i) % n)].load (std::memory_order_relaxed) * 1.6f);
        const float x = r.getX() + r.getWidth() * (float) i / (float) (len - 1);
        const float y = mid - v * h;
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }
    g.setColour (line);
    g.drawHorizontalLine ((int) mid, (float) r.getX(), (float) r.getRight());
    g.setColour (text.withAlpha (0.85f));
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void View::drawSound (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
    switch (box.soundExtraPage())
    {
        case Controller::ExtraFx:    drawChain (g, r); return;
        case Controller::ExtraMacro: drawMacros (g, r); return;
        case Controller::ExtraMod:   drawMods (g, r); return;
        case Controller::ExtraMy:    drawMy (g, r); return;
        default: break;
    }
    if (tr.engine == EngineSampler || tr.engine == EngineGranular) { drawSampler (g, r); return; }
    const auto& d = engineDef (tr.engine);
    // an envelope picture when the page has one, else the sound itself
    auto findEnv = [&d] (const char* a, const char* dd, const char* s, const char* rr, int& ia, int& id, int& is, int& ir)
    {
        ia = d.index (a); id = d.index (dd); is = d.index (s); ir = d.index (rr);
        return ia >= 0 && id >= 0 && is >= 0 && ir >= 0;
    };
    const auto pname = box.pageName();
    int ia, id, is, ir;
    bool env = false;
    juce::String envName;
    if ((pname == "ENV" || pname == "AMP") && (findEnv ("a", "d", "s", "r", ia, id, is, ir) || findEnv ("l1_attack", "l1_decay", "l1_sustain", "l1_release", ia, id, is, ir))) { env = true; envName = "AMP ENVELOPE"; }
    else if (pname == "FILTER" && findEnv ("fa", "fd", "fs", "fr", ia, id, is, ir)) { env = true; envName = "FILTER ENVELOPE"; }

    auto scopeR = env ? r.removeFromLeft (r.getWidth() / 2 - 10) : r;
    g.setColour (panel);
    g.fillRoundedRectangle (scopeR.toFloat(), 8.0f);
    drawScope (g, scopeR.reduced (14, 18));
    if (! env) return;
    r.removeFromLeft (20);
    g.setColour (panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    auto er = r.reduced (16, 14);
    text1 (g, envName, er.removeFromTop (16), 12.0f, dim, juce::Justification::centredLeft, true);
    er.removeFromTop (6);
    const float a = tr.params[(size_t) ia], dd = tr.params[(size_t) id], s = tr.params[(size_t) is], rel = tr.params[(size_t) ir];
    const float total = a + dd + 0.4f + rel;
    const float W = (float) er.getWidth(), H = (float) er.getHeight();
    auto X = [&] (float tt) { return er.getX() + W * std::sqrt (tt / total); };   // a square-root time axis: short times still visible
    juce::Path p;
    p.startNewSubPath ((float) er.getX(), (float) er.getBottom());
    p.lineTo (X (a), (float) er.getY());
    p.quadraticTo (X (a + dd * 0.3f), er.getY() + H * (1.0f - s) * 0.85f, X (a + dd), er.getY() + H * (1.0f - s));
    p.lineTo (X (a + dd + 0.4f), er.getY() + H * (1.0f - s));
    p.quadraticTo (X (a + dd + 0.4f + rel * 0.3f), (float) er.getBottom(), X (total), (float) er.getBottom());
    g.setColour (accent.withAlpha (0.12f));
    juce::Path fill (p);
    fill.lineTo ((float) er.getX(), (float) er.getBottom());
    fill.closeSubPath();
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void View::drawSampler (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    auto in = r.reduced (14, 10);
    auto& a = box.getAudio();
    if (box.isSampling())
    {
        // recording: the input level and how long
        const float secs = (float) a.captureLength.load() / (float) a.getSampleRate();
        auto top = in.removeFromTop (40);
        g.setColour (recRed);
        g.fillEllipse (top.removeFromLeft (28).toFloat().withSizeKeepingCentre (18, 18));
        text1 (g, "RECORDING  " + juce::String (secs, 1) + " s", top, 24.0f, text, juce::Justification::centredLeft, true);
        in.removeFromTop (20);
        auto meter = in.removeFromTop (26).toFloat();
        g.setColour (line);
        g.fillRoundedRectangle (meter, 4.0f);
        const float lv = juce::jlimit (0.0f, 1.0f, std::sqrt (a.inputLevel.load()));
        g.setColour (lv > 0.95f ? recRed : accent);
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * lv), 4.0f);
        in.removeFromTop (14);
        text1 (g, "Rec again = stop. Silence at the start and end is cut off.", in.removeFromTop (24), 15.0f, dim, juce::Justification::centredLeft);
        return;
    }
    const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
    const auto& d = engineDef (tr.engine);
    auto par = [&] (const char* id) { const int i = d.index (id); return i >= 0 ? tr.params[(size_t) i] : 0.0f; };
    const int slot = juce::roundToInt (par ("sample"));
    const Sample* smp = SampleBank::get().at (slot);
    text1 (g, "SAMPLE  " + SampleBank::get().name (slot) + (smp != nullptr ? "   " + juce::String ((float) smp->length() / (float) smp->sampleRate, 1) + " s" : juce::String()),
           in.removeFromTop (18), 13.0f, dim, juce::Justification::centredLeft, true);
    in.removeFromTop (6);
    if (smp == nullptr || smp->length() == 0)
    {
        text1 (g, "No sample. Press Rec to record one with a microphone.", in, 18.0f, text, juce::Justification::centred, true);
        return;
    }
    // the waveform (min / max per pixel)
    const int w = in.getWidth();
    const float mid = (float) in.getCentreY(), h = in.getHeight() * 0.48f;
    const int len = smp->length();
    g.setColour (text.withAlpha (0.75f));
    for (int x = 0; x < w; ++x)
    {
        const int i0 = (int) ((juce::int64) x * len / w), i1 = juce::jmax (i0 + 1, (int) ((juce::int64) (x + 1) * len / w));
        float lo = 0.0f, hi = 0.0f;
        for (int i = i0; i < i1; i += juce::jmax (1, (i1 - i0) / 64)) { const float v = 0.5f * (smp->l[(size_t) i] + smp->r[(size_t) i]); lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
        g.drawVerticalLine (in.getX() + x, mid - hi * h, mid - lo * h + 1.0f);
    }
    if (tr.engine == EngineGranular)   // where the grains come from
    {
        const float pos = par ("pos"), spray = par ("spray");
        const float x0 = in.getX() + w * juce::jmax (0.0f, pos - spray * 0.5f), x1 = in.getX() + w * juce::jmin (1.0f, pos + spray * 0.5f + 0.004f);
        g.setColour (accent.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (x0, (float) in.getY(), juce::jmax (2.0f, x1 - x0), (float) in.getHeight()));
        g.setColour (accent);
        g.drawVerticalLine ((int) (in.getX() + w * pos), (float) in.getY(), (float) in.getBottom());
        return;
    }
    // start / end (outside dimmed), loop start, slices
    const float st = par ("start"), en = par ("end");
    const float xs = in.getX() + w * st, xe = in.getX() + w * en;
    g.setColour (bg.withAlpha (0.6f));
    g.fillRect (juce::Rectangle<float> ((float) in.getX(), (float) in.getY(), xs - in.getX(), (float) in.getHeight()));
    g.fillRect (juce::Rectangle<float> (xe, (float) in.getY(), (float) in.getRight() - xe, (float) in.getHeight()));
    g.setColour (accent);
    g.drawVerticalLine ((int) xs, (float) in.getY(), (float) in.getBottom());
    g.drawVerticalLine ((int) xe, (float) in.getY(), (float) in.getBottom());
    const int mode = juce::roundToInt (par ("mode"));
    if (mode == 1)
    {
        const int slices = juce::roundToInt (par ("slices"));
        g.setColour (accent.withAlpha (0.5f));
        for (int k = 1; k < slices; ++k) g.drawVerticalLine ((int) (xs + (xe - xs) * k / slices), (float) in.getY(), (float) in.getBottom());
    }
    else if (juce::roundToInt (par ("loop")) > 0)
    {
        const float xl = xs + (xe - xs) * par ("loopstart");
        g.setColour (text.withAlpha (0.6f));
        for (int y = in.getY(); y < in.getBottom(); y += 8) g.drawVerticalLine ((int) xl, (float) y, (float) y + 4.0f);
    }
}

void View::drawBrowser (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto cats = box.browseCategories();
    const auto presets = box.browsePresets();
    const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
    browseHits.clearQuick();
    int column = 0;
    auto col = [&] (juce::Rectangle<int> c, const juce::String& title, const juce::StringArray& items, int sel, float size)
    {
        browseColumns[(size_t) column] = c;
        g.setColour (panel);
        g.fillRoundedRectangle (c.toFloat(), 8.0f);
        c = c.reduced (10, 8);
        text1 (g, title, c.removeFromTop (16), 12.0f, dim, juce::Justification::centredLeft, true);
        c.removeFromTop (4);
        const int rowH = (int) (size + 12);
        const int rows = juce::jmax (1, c.getHeight() / rowH);
        const int first = juce::jlimit (0, juce::jmax (0, items.size() - rows), sel - rows / 2);
        // more above / below: a small arrow
        if (first > 0) text1 (g, juce::String::fromUTF8 ("\xe2\x96\xb2"), c.withHeight (10).withY (c.getY() - 14).withTrimmedRight (2), 9.0f, faint, juce::Justification::centredRight);
        for (int i = first; i < juce::jmin (items.size(), first + rows); ++i)
        {
            auto row = c.removeFromTop (rowH);
            browseHits.add ({ row, column, i });
            if (i == sel)
            {
                g.setColour (text);
                g.fillRoundedRectangle (row.toFloat(), 5.0f);
            }
            else if (column == hoverColumn && i == hoverIndex)   // the mouse is over it
            {
                g.setColour (cell.brighter (0.15f));
                g.fillRoundedRectangle (row.toFloat(), 5.0f);
            }
            text1 (g, items[i], row.reduced (8, 0), size, i == sel ? bg : (column == hoverColumn && i == hoverIndex ? text : dim), juce::Justification::centredLeft, i == sel);
        }
        if (first + rows < items.size()) text1 (g, juce::String::fromUTF8 ("\xe2\x96\xbc"), c.withHeight (10).withTrimmedRight (2), 9.0f, faint, juce::Justification::centredRight);
        ++column;
    };
    const int w = r.getWidth();
    juce::StringArray engines = engineNames();
    // the engine shown is what knob 1 picks
    const int eSel = box.browseEngineIndex();
    juce::ignoreUnused (tr);
    col (r.removeFromLeft (w * 22 / 100), "1  ENGINE", engines, eSel, 16.0f);
    r.removeFromLeft (8);
    col (r.removeFromLeft (w * 28 / 100), "2  CATEGORY", cats, box.browseCategory(), 16.0f);
    r.removeFromLeft (8);
    col (r, "3  SOUND  (" + juce::String (presets.size()) + ")", presets, box.browsePresetIndex(), 18.0f);
}

void View::drawSeq (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
    const auto& pat = tr.pat;
    const int barIdx = box.seqBar();
    const int playing = box.getAudio().stepNow[(size_t) box.getTrack()].load();
    if (tr.isDrums())
    {
        // every drum at once: 16 lanes x the 16 steps of this bar
        const int nameW = 92;
        const float rowH = r.getHeight() / 16.0f;
        const float cw = (r.getWidth() - nameW) / 16.0f;
        const auto& d = engineDef (tr.engine);
        for (int l = 0; l < numLanes; ++l)
        {
            const float y = r.getY() + l * rowH;
            const bool sel = l == box.seqLane();
            const auto name = box.laneName (box.getTrack(), l); juce::ignoreUnused (d);
            seqHits.add ({ juce::Rectangle<float> ((float) r.getX(), y, (float) nameW, rowH).toNearestInt(), l, -1 });
            if (sel) { g.setColour (cell.brighter (0.1f)); g.fillRect (juce::Rectangle<float> ((float) r.getX(), y, (float) r.getWidth(), rowH)); }
            const float glow = laneGlow[(size_t) l];
            text1 (g, name, juce::Rectangle<float> ((float) r.getX() + 4, y, (float) nameW - 6, rowH).toNearestInt(), juce::jmin (13.0f, rowH),
                   sel ? text : dim.interpolatedWith (text, glow), juce::Justification::centredLeft, sel);
            for (int s = 0; s < 16; ++s)
            {
                const int step = barIdx * 16 + s;
                auto c = juce::Rectangle<float> (r.getX() + nameW + s * cw, y, cw, rowH).reduced (1.5f, 1.5f);
                seqHits.add ({ c.toNearestInt(), l, step });
                if (step >= pat.length) { g.setColour (bg); g.fillRect (c); continue; }
                const int v = pat.drums[(size_t) l][(size_t) step];
                const bool held = sel && (box.isStepHeld (step) || step == box.selectedStep());
                g.setColour (v > 0 ? (v >= 120 ? accent : text.withAlpha (0.6f + 0.4f * v / 127.0f)) : (s % 4 == 0 ? line.brighter (0.05f) : line.darker (0.15f)));
                g.fillRoundedRectangle (c, 2.0f);
                if (held) { g.setColour (accent); g.drawRoundedRectangle (c, 2.0f, 2.0f); }
                if (step == playing) { g.setColour (accent.withAlpha (0.6f)); g.fillRoundedRectangle (c.withHeight (2.0f).withY (c.getBottom() - 2.0f), 1.0f); }
            }
            if (l == 0)   // steps with recorded knob moves: a small mark above the grid
                for (int s2 = 0; s2 < 16; ++s2)
                    if (barIdx * 16 + s2 < pat.length && pat.hasLocks (barIdx * 16 + s2))
                    {
                        g.setColour (accent);
                        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ r.getX() + nameW + (s2 + 0.5f) * cw, (float) r.getY() - 1.0f }));
                    }
        }
        return;
    }
    // melodic: two rows of 8 steps (like the pads), each with its note, velocity, length, slide and accent
    const int cw = r.getWidth() / 8, ch = r.getHeight() / 2;
    for (int s = 0; s < 16; ++s)
    {
        const int step = barIdx * 16 + s;
        auto c = juce::Rectangle<int> (r.getX() + (s % 8) * cw, r.getY() + (s / 8) * ch, cw, ch).reduced (3);
        if (step >= pat.length) { g.setColour (bg); g.fillRect (c); continue; }
        seqHits.add ({ c, -1, step });
        const auto& st = pat.steps[(size_t) step];
        const bool held = box.isStepHeld (step) || step == box.selectedStep();
        g.setColour (st.on() ? cell.brighter (0.25f) : cell);
        g.fillRoundedRectangle (c.toFloat(), 6.0f);
        if (step == playing) { g.setColour (accent); g.fillRoundedRectangle (c.toFloat().removeFromTop (4.0f), 2.0f); }
        if (held) { g.setColour (accent); g.drawRoundedRectangle (c.toFloat().reduced (1.0f), 6.0f, 2.5f); }
        auto in = c.reduced (8, 6);
        text1 (g, juce::String (step + 1), in.removeFromTop (14), 11.0f, faint, juce::Justification::centredLeft);
        if (! st.on()) continue;
        juce::String notes;
        for (auto n : st.notes) if (n >= 0) notes << (notes.isEmpty() ? "" : " ") << music::noteName (n);
        text1 (g, notes, in.removeFromTop (26), st.count() > 1 ? 14.0f : 20.0f, text, juce::Justification::centredLeft, true);
        // velocity bar, length line
        auto bars = in.removeFromBottom (14);
        g.setColour (line);
        g.fillRect (bars.removeFromTop (4));
        g.setColour (text.withAlpha (0.7f));
        g.fillRect (bars.withY (bars.getY() - 4).withHeight (4).withWidth (juce::roundToInt (in.getWidth() * st.vel / 127.0f)));
        g.setColour (dim);
        g.fillRect (bars.withTrimmedTop (4).withHeight (3).withWidth (juce::jmin (in.getWidth(), juce::roundToInt (in.getWidth() * st.len / 100.0f))));
        juce::String marks;
        if (pat.hasLocks (step)) marks << "KNOB ";
        if (st.slide) marks << "SLIDE ";
        if (st.accent) marks << "ACC ";
        if (st.chance < 100) marks << st.chance << "% ";
        if (st.ratchet > 1) marks << "x" << (int) st.ratchet << " ";
        text1 (g, marks, in, 11.0f, accent, juce::Justification::centredLeft, true);
    }
}

void View::drawFx (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
    g.setColour (panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    auto in = r.reduced (16, 12);
    if (tr.fxType == FxOff)
    {
        text1 (g, "No note FX on this track", in.removeFromTop (40), 22.0f, text, juce::Justification::centred, true);
        text1 (g, juce::String::fromUTF8 ("\xe2\x97\x80 \xe2\x96\xb6  Arp  \xc2\xb7  Chords  \xc2\xb7  Orbit"), in.removeFromTop (30), 16.0f, dim, juce::Justification::centred);
        text1 (g, "It works on everything the track plays: keys, pads and its steps", in.removeFromTop (30), 14.0f, faint, juce::Justification::centred);
        return;
    }
    if (tr.fxType == FxOrbit)
    {
        // the planets around the sun, the line(s) where they sound
        const auto c = in.getCentre().toFloat();
        const float R = juce::jmin (in.getWidth(), in.getHeight()) * 0.48f;
        const int lines = juce::roundToInt (tr.fx[3]);
        const float angle = tr.fx[4] * juce::MathConstants<float>::pi / 180.0f;
        g.setColour (line);
        for (int k = 0; k < lines; ++k)
        {
            const float a = angle + juce::MathConstants<float>::twoPi * k / juce::jmax (1, lines);
            g.drawLine (c.x, c.y, c.x + R * std::cos (a), c.y - R * std::sin (a), 1.5f);
        }
        g.setColour (text);
        g.fillEllipse (juce::Rectangle<float> (14, 14).withCentre (c));
        for (const auto& pv : box.getAudio().planets)
        {
            if (! pv.active.load()) continue;
            const float rad = juce::jmin (R, pv.radius.load() / 32.0f * R * 1.6f + 14.0f);
            g.setColour (faint);
            g.drawEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c), 1.0f);
            const float th = pv.theta.load();
            const auto pp = juce::Point<float> (c.x + rad * std::cos (th), c.y - rad * std::sin (th));
            g.setColour (accent);
            g.fillEllipse (juce::Rectangle<float> (12, 12).withCentre (pp));
            text1 (g, music::noteName (pv.note.load()), juce::Rectangle<int> (40, 16).withCentre (pp.toInt() + juce::Point<int> (0, 14)), 11.0f, dim, juce::Justification::centred);
        }
        text1 (g, "ORBIT", in.removeFromTop (18), 13.0f, dim, juce::Justification::centredLeft, true);
        text1 (g, "hold keys: each one is a planet", in.removeFromBottom (18), 12.0f, faint, juce::Justification::centredLeft);
        return;
    }
    const auto& d = noteFxDef (tr.fxType);
    text1 (g, d.name.toUpperCase(), in.removeFromTop (18), 13.0f, dim, juce::Justification::centredLeft, true);
    in.removeFromTop (6);
    if (tr.fxType == FxArp)
    {
        // the rhythm: steps / hits / rotate as a row of cells
        const int steps = juce::roundToInt (tr.fx[6]), hitsN = juce::roundToInt (tr.fx[7]), rot = juce::roundToInt (tr.fx[8]);
        auto row = in.removeFromTop (60);
        const float cw = row.getWidth() / 16.0f;
        for (int i = 0; i < 16; ++i)
        {
            auto c = juce::Rectangle<float> (row.getX() + i * cw, (float) row.getY(), cw, (float) row.getHeight()).reduced (3);
            if (i >= steps) { g.setColour (bg); g.fillRoundedRectangle (c, 4.0f); continue; }
            const int pos = ((i + rot) % steps + steps) % steps;
            const bool on = ((pos * hitsN) % steps) < hitsN;
            g.setColour (on ? text.withAlpha (0.85f) : line);
            g.fillRoundedRectangle (c, 4.0f);
        }
        in.removeFromTop (14);
        const juce::String desc = d.params[1].format (tr.fx[1]) + sep + d.params[0].format (tr.fx[0]) + sep
                                + juce::String (juce::roundToInt (tr.fx[2])) + " oct" + (tr.fx[5] > 0.5f ? (sep + "HOLD") : juce::String());
        text1 (g, desc, in.removeFromTop (34), 24.0f, text, juce::Justification::centredLeft, true);
        text1 (g, "hold notes on the keys or pads", in.removeFromTop (22), 14.0f, faint, juce::Justification::centredLeft);
        return;
    }
    // chords: what C (the key's root) would play
    const juce::String desc = d.params[0].format (tr.fx[0]) + sep + d.params[1].format (tr.fx[1]) + (tr.fx[7] > 0.5f ? sep + "in key" : juce::String());
    text1 (g, desc, in.removeFromTop (40), 26.0f, text, juce::Justification::centredLeft, true);
    text1 (g, "one key = one chord of " + juce::String (music::keyName (box.getProject().m.key)) + " " + music::scaleName (box.getProject().m.scale),
           in.removeFromTop (26), 15.0f, dim, juce::Justification::centredLeft);
}

void View::drawMix (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& p = box.getProject();
    if (box.pageIndex() >= 2) { drawReturns (g, r); return; }
    const int w = r.getWidth() / numTracks;
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& tr = p.tracks[(size_t) t];
        auto c = juce::Rectangle<int> (r.getX() + t * w, r.getY(), w, r.getHeight()).reduced (4, 0);
        g.setColour (t == box.getTrack() ? cell.brighter (0.12f) : panel);
        g.fillRoundedRectangle (c.toFloat(), 6.0f);
        auto in = c.reduced (8, 6);
        text1 (g, juce::String (t + 1) + " " + shortEngine (tr.engine), in.removeFromTop (18), 13.0f, tr.mute ? faint : text, juce::Justification::centredLeft, true);
        auto bottom = in.removeFromBottom (18);
        text1 (g, tr.mute ? "MUTE" : (tr.solo ? "SOLO" : ""), bottom, 12.0f, tr.solo ? accent : dim, juce::Justification::centredLeft, true);
        // pan
        auto pan = in.removeFromBottom (12).toFloat().reduced (2, 3);
        g.setColour (line); g.fillRect (pan);
        g.setColour (dim);
        g.fillRect (juce::Rectangle<float> (pan.getCentreX() + pan.getWidth() * 0.5f * tr.pan - 2.0f, pan.getY() - 2, 4.0f, pan.getHeight() + 4));
        in.removeFromBottom (4);
        // sends: two small bars
        auto sends = in.removeFromBottom (16);
        const int sw = sends.getWidth() / 3;
        auto sA = sends.removeFromLeft (sw).reduced (1, 4).toFloat(), sB = sends.removeFromLeft (sw).reduced (1, 4).toFloat(), sC = sends.reduced (1, 4).toFloat();
        g.setColour (line); g.fillRect (sA); g.fillRect (sB); g.fillRect (sC);
        g.setColour (box.pageIndex() == 1 ? accent : dim);
        g.fillRect (sA.withWidth (sA.getWidth() * tr.sendA)); g.fillRect (sB.withWidth (sB.getWidth() * tr.sendB));
        g.setColour (dim); g.fillRect (sC.withWidth (sC.getWidth() * tr.sendC));
        in.removeFromBottom (4);
        // volume (wide) and the level meter (thin) next to it
        auto fader = in.removeFromLeft (in.getWidth() / 2).toFloat().reduced (4, 0);
        g.setColour (line); g.fillRoundedRectangle (fader, 3.0f);
        g.setColour (tr.mute ? faint : text.withAlpha (0.8f));
        g.fillRoundedRectangle (fader.withTop (fader.getBottom() - fader.getHeight() * tr.volume), 3.0f);
        auto meter = in.toFloat().reduced (6, 0).withWidth (8.0f);
        g.setColour (line); g.fillRect (meter);
        const float lv = juce::jlimit (0.0f, 1.0f, std::sqrt (meters[(size_t) t]));
        g.setColour (accent.withAlpha (0.85f));
        g.fillRect (meter.withTop (meter.getBottom() - meter.getHeight() * lv));
    }
}

void View::drawProject (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& m = box.getProject().m;
    if (box.pageIndex() == 1)   // LOOK: the themes and colours to pick from
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        auto in = r.reduced (16, 12);
        const auto& lk = box.getLook();
        text1 (g, "THEME (knob 1)", in.removeFromTop (16), 12.0f, dim, juce::Justification::centredLeft, true);
        auto row = in.removeFromTop (58);
        const auto names = themeNames();
        const int w = row.getWidth() / 8;
        for (int i = 0; i < 8; ++i)
        {
            auto c = juce::Rectangle<int> (row.getX() + i * w, row.getY(), w, row.getHeight()).reduced (3, 2);
            const auto& th = themes[i];
            g.setColour (juce::Colour (th.bg)); g.fillRoundedRectangle (c.toFloat(), 5.0f);
            g.setColour (juce::Colour (th.cell)); g.fillRoundedRectangle (c.toFloat().reduced (5.0f, 18.0f).withTrimmedTop (10.0f), 3.0f);
            g.setColour (juce::Colour (accents[lk.accent])); g.fillRect (c.toFloat().reduced (8.0f, 0.0f).withHeight (3.0f).withY ((float) c.getBottom() - 9.0f));
            text1 (g, names[i], c.removeFromTop (18).reduced (4, 0), 11.0f, juce::Colour (th.text), juce::Justification::centredLeft, true);
            if (i == lk.theme) { g.setColour (accent); g.drawRoundedRectangle (juce::Rectangle<int> (row.getX() + i * w, row.getY(), w, row.getHeight()).reduced (1).toFloat(), 6.0f, 2.0f); }
        }
        in.removeFromTop (10);
        text1 (g, "COLOUR (knob 2)", in.removeFromTop (16), 12.0f, dim, juce::Justification::centredLeft, true);
        auto ar = in.removeFromTop (34);
        const int aw = ar.getWidth() / 9;
        for (int i = 0; i < 9; ++i)
        {
            auto c = juce::Rectangle<int> (ar.getX() + i * aw, ar.getY(), aw, ar.getHeight()).reduced (4, 3).toFloat();
            g.setColour (juce::Colour (accents[i])); g.fillRoundedRectangle (c, 5.0f);
            if (i == lk.accent) { g.setColour (text); g.drawRoundedRectangle (c.expanded (2.0f), 6.0f, 2.0f); }
        }
        in.removeFromTop (8);
        text1 (g, juce::String ("Text ") + (lk.textSize ? "big" : "normal") + "   Knobs " + (lk.knobStyle ? "rings" : "bars") + "   Track colours " + (lk.trackColours ? "on" : "off")
                  + "   Help line " + (lk.hintLine ? "on" : "off"), in.removeFromTop (22), 14.0f, text, juce::Justification::centredLeft);
        return;
    }
    g.setColour (panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    auto in = r.reduced (24, 16);
    auto left = in.removeFromLeft (in.getWidth() / 2);
    text1 (g, "TEMPO", left.removeFromTop (18), 13.0f, dim, juce::Justification::centredLeft, true);
    text1 (g, juce::String (juce::roundToInt (m.tempo)), left.removeFromTop (90), 84.0f, text, juce::Justification::centredLeft, true);
    text1 (g, "swing " + juce::String (juce::roundToInt (m.swing * 100.0f)) + "%" + (m.metronome ? "   click on" : ""), left.removeFromTop (26), 16.0f, dim, juce::Justification::centredLeft);
    text1 (g, "KEY", in.removeFromTop (18), 13.0f, dim, juce::Justification::centredLeft, true);
    text1 (g, juce::String (music::keyName (m.key)) + " " + music::scaleName (m.scale), in.removeFromTop (60), 44.0f, text, juce::Justification::centredLeft, true);
    text1 (g, juce::String ("pads: ") + (box.padsAsScale() ? "scale" : "piano") + ", octave " + juce::String (box.padOctave (box.getTrack())),
           in.removeFromTop (24), 15.0f, dim, juce::Justification::centredLeft);
    if (linkInfo) text1 (g, linkInfo(), in.removeFromBottom (20), 13.0f, faint, juce::Justification::centredLeft);
}

void View::drawChain (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
    const int sel = box.fxSlotNow();
    auto top = r.removeFromTop (18);
    text1 (g, "EFFECTS ON TRACK " + juce::String (box.getTrack() + 1) + "  -  sound > 1 > 2 > 3 > 4 > mixer", top, 12.0f, dim, juce::Justification::centredLeft, true);
    r.removeFromTop (4);
    const int w = r.getWidth() / numInserts;
    for (int k = 0; k < numInserts; ++k)
    {
        const auto& in = tr.mods.inserts[(size_t) k];
        const auto& d = insertDef (in.type);
        auto c = juce::Rectangle<int> (r.getX() + k * w, r.getY(), w, r.getHeight()).reduced (5, 0);
        const bool off = in.type == InsOff;
        g.setColour (k == sel ? cell.brighter (0.1f) : panel);
        g.fillRoundedRectangle (c.toFloat(), 8.0f);
        if (k == sel) { g.setColour (accent); g.drawRoundedRectangle (c.toFloat().reduced (1.0f), 8.0f, 2.0f); }
        if (k < numInserts - 1)   // the arrow to the next
        {
            juce::Path a;
            const float x = (float) c.getRight() + 5.0f, y = (float) c.getCentreY();
            a.addTriangle (x - 4.0f, y - 5.0f, x - 4.0f, y + 5.0f, x + 3.0f, y);
            g.setColour (faint); g.fillPath (a);
        }
        auto inr = c.reduced (10, 8);
        text1 (g, juce::String (k + 1), inr.removeFromTop (16), 13.0f, k == sel ? accent : dim, juce::Justification::centredLeft, true);
        text1 (g, off ? juce::String ("empty") : d.name, inr.removeFromTop (26), off ? 16.0f : 19.0f, off ? faint : (in.on ? text : dim), juce::Justification::centredLeft, true);
        if (off) { text1 (g, "knob 2 picks one", inr.removeFromTop (18), 12.0f, faint, juce::Justification::centredLeft); continue; }
        if (! in.on) text1 (g, "BYPASSED", inr.removeFromTop (16), 12.0f, recRed, juce::Justification::centredLeft, true);
        inr.removeFromTop (4);
        for (size_t q = 0; q < d.params.size() && inr.getHeight() >= 15; ++q)
        {
            auto row = inr.removeFromTop (16);
            const float n = d.params[q].toNorm (in.p[q]);
            auto barR = row.removeFromBottom (3).toFloat();
            g.setColour (line); g.fillRect (barR);
            g.setColour (k == sel ? accent.withAlpha (0.8f) : dim); g.fillRect (barR.withWidth (barR.getWidth() * n));
            text1 (g, d.params[q].name + "  " + d.params[q].format (in.p[q]), row, 11.0f, dim, juce::Justification::centredLeft);
        }
    }
}

void View::drawMacros (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
    const auto& md = tr.mods;
    g.setColour (panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    auto in = r.reduced (12, 8);
    auto left = in.removeFromLeft (in.getWidth() * 5 / 9);
    text1 (g, "MACROS  -  MENU > More > Assign to macro, then turn a knob", left.removeFromTop (16), 12.0f, dim, juce::Justification::centredLeft, true);
    const int w = left.getWidth() / numMacros;
    for (int m = 0; m < numMacros; ++m)
    {
        auto c = juce::Rectangle<int> (left.getX() + m * w, left.getY() + 4, w, left.getHeight() - 4).reduced (4, 0);
        int links = 0;
        for (const auto& sl : md.slots) links += sl.active() && sl.source == SrcMacro1 + m ? 1 : 0;
        auto label = c.removeFromBottom (30);
        text1 (g, "M" + juce::String (m + 1), label.removeFromTop (15), 12.0f, text, juce::Justification::centred, true);
        text1 (g, links > 0 ? juce::String (links) + " link" + (links > 1 ? "s" : "") : juce::String ("-"), label, 10.0f, links > 0 ? accent : faint, juce::Justification::centred);
        auto bar = c.reduced (juce::jmax (2, c.getWidth() / 2 - 7), 2).toFloat();
        g.setColour (line); g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (links > 0 ? accent : dim);
        g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * md.macros[(size_t) m]), 4.0f);
    }
    in.removeFromLeft (14);
    // the two LFOs as pictures, and the envelope
    const auto& sd = modSetDefs();
    auto drawShape = [&] (juce::Rectangle<int> c, int k)
    {
        const int shape = juce::roundToInt (md.set[(size_t) (k == 0 ? MsLfo1Shape : MsLfo2Shape)]);
        const int sync = juce::roundToInt (md.set[(size_t) (k == 0 ? MsLfo1Sync : MsLfo2Sync)]);
        const float rate = md.set[(size_t) (k == 0 ? MsLfo1Rate : MsLfo2Rate)];
        text1 (g, "LFO " + juce::String (k + 1) + "  " + sd[(size_t) (k == 0 ? MsLfo1Shape : MsLfo2Shape)].format ((float) shape) + "  "
                  + (sync > 0 ? syncNames()[sync] : sd[(size_t) MsLfo1Rate].format (rate)), c.removeFromTop (15), 11.0f, dim, juce::Justification::centredLeft, true);
        auto pr = c.reduced (2, 4).toFloat();
        juce::Path p;
        juce::Random rnd (7 + k);
        float hold = rnd.nextFloat() * 2.0f - 1.0f, prev = hold;
        for (int i = 0; i <= 64; ++i)
        {
            const float ph = std::fmod ((float) i / 32.0f, 1.0f);
            if (i % 32 == 0 && i > 0) { prev = hold; hold = rnd.nextFloat() * 2.0f - 1.0f; }
            float v;
            switch (shape)
            {
                case 1: v = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
                case 2: v = 2.0f * ph - 1.0f; break;
                case 3: v = 1.0f - 2.0f * ph; break;
                case 4: v = ph < 0.5f ? 1.0f : -1.0f; break;
                case 5: v = hold; break;
                case 6: v = prev + (hold - prev) * (0.5f - 0.5f * std::cos (dsp::pi * ph)); break;
                default: v = std::sin (dsp::twoPi * ph); break;
            }
            const float x = pr.getX() + pr.getWidth() * (float) i / 64.0f, y = pr.getCentreY() - v * pr.getHeight() * 0.42f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (line); g.drawHorizontalLine ((int) pr.getCentreY(), pr.getX(), pr.getRight());
        g.setColour (accent); g.strokePath (p, juce::PathStrokeType (2.0f));
    };
    const int h = in.getHeight() / 3;
    drawShape (in.removeFromTop (h), 0);
    drawShape (in.removeFromTop (h), 1);
    text1 (g, "ENVELOPE  " + sd[(size_t) MsEnvAttack].format (md.set[MsEnvAttack]) + " / " + sd[(size_t) MsEnvDecay].format (md.set[MsEnvDecay])
              + "     RANDOM  " + syncNames()[juce::roundToInt (md.set[MsRandRate])], in.removeFromTop (16), 11.0f, dim, juce::Justification::centredLeft, true);
}

void View::drawMods (juce::Graphics& g, juce::Rectangle<int> r)
{
    const int t = box.getTrack();
    const auto& md = box.getProject().tracks[(size_t) t].mods;
    const int sel = box.modSlotNow();
    auto top = r.removeFromTop (18);
    text1 (g, "MODULATION  -  source > where, how much (+/-)", top, 12.0f, dim, juce::Justification::centredLeft, true);
    const int colW = r.getWidth() / 2, rowH = r.getHeight() / 8;
    for (int i = 0; i < numModSlots; ++i)
    {
        const auto& sl = md.slots[(size_t) i];
        auto c = juce::Rectangle<int> (r.getX() + (i / 8) * colW, r.getY() + (i % 8) * rowH, colW, rowH).reduced (3, 1);
        g.setColour (i == sel ? cell.brighter (0.12f) : panel);
        g.fillRoundedRectangle (c.toFloat(), 4.0f);
        if (i == sel) { g.setColour (accent); g.drawRoundedRectangle (c.toFloat(), 4.0f, 1.5f); }
        const bool used = sl.source != SrcNone && sl.dest != dest::none;
        auto num = c.removeFromLeft (22);
        text1 (g, juce::String (i + 1), num, 12.0f, i == sel ? accent : faint, juce::Justification::centred, true);
        auto amt = c.removeFromRight (90).reduced (4, juce::jmax (2, c.getHeight() / 2 - 3)).toFloat();
        if (used)
        {
            g.setColour (line); g.fillRect (amt);
            const float mid = amt.getCentreX(), x = mid + amt.getWidth() * 0.5f * sl.amount;
            g.setColour (sl.amount >= 0.0f ? accent : dim);
            g.fillRect (juce::Rectangle<float> (juce::jmin (mid, x), amt.getY(), juce::jmax (1.5f, std::abs (x - mid)), amt.getHeight()));
        }
        text1 (g, used ? modSourceNames()[sl.source] + "  >  " + box.destName (t, sl.dest, true) : juce::String ("-"), c, 13.0f, used ? text : faint, juce::Justification::centredLeft, used);
    }
}

void View::drawMy (juce::Graphics& g, juce::Rectangle<int> r)
{
    const int t = box.getTrack();
    const auto& md = box.getProject().tracks[(size_t) t].mods;
    int used = 0;
    for (auto d : md.my) used += d >= 0 ? 1 : 0;
    g.setColour (panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    auto in = r.reduced (12, 8);
    text1 (g, "MY KNOBS  " + juce::String (used) + " / 24   (Shift: 13-24)   MENU > More > Add to MY knobs", in.removeFromTop (16), 12.0f, dim, juce::Justification::centredLeft, true);
    if (used == 0)
    {
        text1 (g, "Put the knobs you use most on one page:", in.removeFromTop (40).withTrimmedTop (14), 17.0f, text, juce::Justification::centredLeft, true);
        text1 (g, "Project twice (MENU) > More > Add to MY knobs, then turn any knob - sound, FX, macro, mix.", in.removeFromTop (24), 14.0f, dim, juce::Justification::centredLeft);
        return;
    }
    in.removeFromTop (4);
    const int cw = in.getWidth() / 6, rh = in.getHeight() / 4;
    for (int i = 0; i < numMyKnobs; ++i)
    {
        auto c = juce::Rectangle<int> (in.getX() + (i % 6) * cw, in.getY() + (i / 6) * rh, cw, rh).reduced (2);
        const bool layerOn = (i < 12) != box.isShift();
        g.setColour (layerOn ? cell : cell.withAlpha (0.35f));
        g.fillRoundedRectangle (c.toFloat(), 4.0f);
        text1 (g, juce::String (i % 12 + 1) + (i >= 12 ? "s" : "") + "  " + (md.my[(size_t) i] >= 0 ? box.destName (t, md.my[(size_t) i], true) : juce::String ("-")),
               c.reduced (6, 0), 11.0f, md.my[(size_t) i] >= 0 ? (layerOn ? text : dim) : faint, juce::Justification::centredLeft);
    }
}

void View::drawReturns (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& p = box.getProject();
    const auto& m = p.m;
    if (box.pageIndex() == 2)   // FX SEND: how much of each track goes in, and the effect
    {
        auto bars = r.removeFromLeft (r.getWidth() / 2);
        const int w = bars.getWidth() / numTracks;
        for (int t = 0; t < numTracks; ++t)
        {
            auto c = juce::Rectangle<int> (bars.getX() + t * w, bars.getY(), w, bars.getHeight()).reduced (3, 0);
            g.setColour (t == box.getTrack() ? cell.brighter (0.12f) : panel);
            g.fillRoundedRectangle (c.toFloat(), 5.0f);
            text1 (g, juce::String (t + 1), c.removeFromBottom (18), 12.0f, text, juce::Justification::centred, true);
            auto bar = c.reduced (juce::jmax (2, c.getWidth() / 2 - 6), 8).toFloat();
            g.setColour (line); g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (accent); g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * p.tracks[(size_t) t].sendC), 3.0f);
        }
        r.removeFromLeft (10);
        g.setColour (panel); g.fillRoundedRectangle (r.toFloat(), 8.0f);
        auto in = r.reduced (12, 8);
        const auto& d = insertDef (m.fxSend.type);
        text1 (g, "FX SEND", in.removeFromTop (16), 12.0f, dim, juce::Justification::centredLeft, true);
        text1 (g, d.name, in.removeFromTop (30), 24.0f, text, juce::Justification::centredLeft, true);
        text1 (g, "level " + juce::String (juce::roundToInt (m.fxLevel * 100)) + "%   duck " + juce::String (juce::roundToInt (m.fxDuck * 100)) + "%",
               in.removeFromTop (18), 12.0f, dim, juce::Justification::centredLeft);
        for (size_t q = 0; q < d.params.size() && in.getHeight() > 14; ++q)
            text1 (g, d.params[q].name + "  " + d.params[q].format (m.fxSend.p[q]), in.removeFromTop (16), 12.0f, dim, juce::Justification::centredLeft);
        return;
    }
    // RETURNS: reverb, delay and the FX send side by side
    const int w = r.getWidth() / 3;
    auto box3 = [&] (int i, const juce::String& title, const juce::String& big, const juce::StringArray& lines, float level, float duck)
    {
        auto c = juce::Rectangle<int> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (5, 0);
        g.setColour (panel); g.fillRoundedRectangle (c.toFloat(), 8.0f);
        auto in = c.reduced (12, 8);
        text1 (g, title, in.removeFromTop (16), 12.0f, dim, juce::Justification::centredLeft, true);
        text1 (g, big, in.removeFromTop (28), 22.0f, text, juce::Justification::centredLeft, true);
        auto lv = in.removeFromBottom (8).toFloat().reduced (0, 1);
        g.setColour (line); g.fillRoundedRectangle (lv, 3.0f);
        g.setColour (accent); g.fillRoundedRectangle (lv.withWidth (lv.getWidth() * level * (1.0f - 0.5f * duck)), 3.0f);
        for (const auto& l : lines) if (in.getHeight() > 14) text1 (g, l, in.removeFromTop (17), 12.0f, dim, juce::Justification::centredLeft);
    };
    auto pc = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
    box3 (0, "REVERB (send A)", revTypeNames()[m.revType],
          { "size " + pc (m.revSize) + "   damp " + pc (m.revDamp), "width " + pc (m.revWidth) + "   low cut " + pc (m.revLowCut),
            "pre-delay " + juce::String (juce::roundToInt (m.revPre * 250.0f)) + " ms", "duck " + pc (m.revDuck) + "   level " + pc (m.revLevel) }, m.revLevel, m.revDuck);
    box3 (1, "DELAY (send B)", delayTypeNames()[m.delayType] + "  " + delayNames()[m.delayTime],
          { "feedback " + pc (m.delayFb) + "   tone " + pc (m.delayTone), "ping-pong " + pc (m.delayPingPong) + "   wow " + pc (m.delayWow),
            "into reverb " + pc (m.delayToRev), "duck " + pc (m.delayDuck) + "   level " + pc (m.delayLevel) }, m.delayLevel, m.delayDuck);
    box3 (2, "FX SEND (send C)", insertDef (m.fxSend.type).name, { "level " + pc (m.fxLevel), "duck " + pc (m.fxDuck), "the FX SEND page picks it" }, m.fxLevel, m.fxDuck);
}

bool View::synthPanelShown() const
{
    const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
    return box.getScreen() == Controller::ScreenSynth && box.getMode() == ModeSound && tr.engine == EngineSynth && box.soundExtraPage() < 0
        && ! box.isSceneMode();
}

void View::drawSynthPanel (juce::Graphics& g, juce::Rectangle<int> r)
{
    const int t = box.getTrack();
    const auto& tr = box.getProject().tracks[(size_t) t];
    const auto& d = engineDef (EngineSynth);
    const int mdl = juce::roundToInt (tr.params[(size_t) d.index ("model")]);
    const auto& lk = synthModelLook (mdl);
    const juce::Colour pnl (lk.panel), face (lk.face), acc (lk.accent), ink (lk.ink);
    g.setColour (pnl);
    g.fillRect (r);
    // wooden cheeks (the 70s instruments)
    if (lk.side != 0)
    {
        const juce::Colour wood (lk.side);
        for (auto side : { r.withWidth (9), r.withLeft (r.getRight() - 9) })
        {
            g.setGradientFill (juce::ColourGradient (wood.brighter (0.25f), (float) side.getX(), 0.0f, wood.darker (0.35f), (float) side.getRight(), 0.0f, false));
            g.fillRect (side);
            g.setColour (wood.darker (0.6f).withAlpha (0.5f));
            for (int y = side.getY() + 7; y < side.getBottom(); y += 13) g.drawHorizontalLine (y, (float) side.getX(), (float) side.getRight());
        }
        r = r.reduced (11, 0);
    }
    r = r.reduced (4, 4);
    const auto& sections = synthPanel();
    // two rows of sections, like the hardware: sound sources on top, shaping below
    const int split = 7;
    auto rowTop = r.removeFromTop (r.getHeight() / 2);
    auto rowBottom = r;
    auto layoutRow = [&] (juce::Rectangle<int> row, int from, int to)
    {
        int cols = 0;
        for (int s = from; s < to; ++s) cols += ((int) sections[(size_t) s].ids.size() + 1) / 2;
        const float colW = (float) row.getWidth() / (float) juce::jmax (1, cols);
        float x = (float) row.getX();
        for (int s = from; s < to; ++s)
        {
            const auto& sec = sections[(size_t) s];
            const int n = (int) sec.ids.size(), c = (n + 1) / 2;
            auto box2 = juce::Rectangle<float> (x, (float) row.getY(), colW * (float) c, (float) row.getHeight()).reduced (2.0f, 2.0f);
            x += colW * (float) c;
            g.setColour (pnl.contrasting (0.06f));
            g.fillRoundedRectangle (box2, 5.0f);
            g.setColour (acc.withAlpha (0.35f));
            g.drawRoundedRectangle (box2, 5.0f, 1.0f);
            auto inner = box2.toNearestInt();
            text1 (g, sec.title, inner.removeFromTop (14).reduced (5, 0), 9.5f, pnl.getPerceivedBrightness() > 0.5f ? acc.darker (0.3f) : acc, juce::Justification::centredLeft, true);
            const int cellW = inner.getWidth() / c, cellH = inner.getHeight() / 2;
            for (int k = 0; k < n; ++k)
            {
                const int fxCode = Controller::panelFxCode (sec.ids[(size_t) k]);
                const int idx = fxCode >= 0 ? fxCode : d.index (sec.ids[(size_t) k]);
                if (idx < 0) continue;
                // the EFFECTS knobs are the box's send delay and reverb (not the synth's own)
                ParamDef fxDef;
                if (fxCode >= 0) { fxDef.unit = box.panelFxStepped (fxCode) ? ParamDef::Choice : ParamDef::Pct; fxDef.lo = 0.0f; fxDef.hi = 1.0f; }
                const auto& pd = fxCode >= 0 ? fxDef : d.params[(size_t) idx];
                const auto fxv = fxCode >= 0 ? box.panelFxView (fxCode) : KnobView();
                auto cell2 = juce::Rectangle<int> (inner.getX() + (k / 2) * cellW, inner.getY() + (k % 2) * cellH, cellW, cellH);
                panelHits.add ({ cell2, idx });
                const float val = fxCode >= 0 ? fxv.norm : tr.params[(size_t) idx];
                const float nrm = fxCode >= 0 ? fxv.norm : pd.toNorm (val);
                const bool active = idx == panelActive || idx == panelHover;
                auto lab = cell2.removeFromTop (11);
                auto valR = cell2.removeFromBottom (11);
                juce::String name = fxCode >= 0 ? fxv.name : pd.name;
                for (const char* cut : { "Osc 1 ", "Osc 2 ", "Osc 3 ", "LFO > ", "F " }) if (name.startsWith (cut) && name.length() > (int) strlen (cut)) name = name.substring ((int) strlen (cut));
                text1 (g, name, lab.reduced (1, 0), 9.0f, ink.withAlpha (0.85f), juce::Justification::centred);
                text1 (g, fxCode >= 0 ? fxv.value : pd.format (val), valR.reduced (1, 0), 9.0f, active ? acc : ink.withAlpha (0.55f), juce::Justification::centred, active);
                // the knob
                const float dia = (float) juce::jmin (cell2.getWidth() - 6, cell2.getHeight() - 2);
                if (dia < 8.0f) continue;
                const auto kc = cell2.toFloat().withSizeKeepingCentre (dia, dia);
                const bool sw = pd.unit == ParamDef::Toggle;
                if (sw)   // a switch: a lit rectangle
                {
                    auto sr2 = kc.withSizeKeepingCentre (dia * 0.9f, dia * 0.55f);
                    g.setColour (val > 0.5f ? acc : face.contrasting (0.12f));
                    g.fillRoundedRectangle (sr2, 3.0f);
                    text1 (g, val > 0.5f ? "ON" : "OFF", sr2.toNearestInt(), 8.5f, val > 0.5f ? acc.contrasting (0.9f) : face.contrasting (0.7f), juce::Justification::centred, true);
                }
                else
                {
                    const float a0 = -2.4f, a1 = 2.4f, cx = kc.getCentreX(), cy = kc.getCentreY(), rad = dia * 0.5f;
                    g.setColour (face.darker (0.4f));
                    g.fillEllipse (kc.expanded (1.5f));
                    g.setColour (face);
                    g.fillEllipse (kc.reduced (dia * 0.12f));
                    juce::Path back; back.addCentredArc (cx, cy, rad - 1.5f, rad - 1.5f, 0.0f, a0, a1, true);
                    g.setColour (pnl.contrasting (0.18f));
                    g.strokePath (back, juce::PathStrokeType (2.5f));
                    const bool bip = pd.unit == ParamDef::BiPct || ((pd.unit == ParamDef::Semi || pd.unit == ParamDef::Cents) && pd.lo < 0);
                    const float from = bip ? 0.0f : a0, to = a0 + (a1 - a0) * nrm;
                    juce::Path arc; arc.addCentredArc (cx, cy, rad - 1.5f, rad - 1.5f, 0.0f, juce::jmin (from, to), juce::jmax (from, to) + 0.001f, true);
                    g.setColour (active ? acc.brighter (0.3f) : acc);
                    g.strokePath (arc, juce::PathStrokeType (2.5f));
                    const float ang = to - juce::MathConstants<float>::halfPi;
                    g.setColour (face.contrasting (0.85f));
                    g.drawLine (cx + std::cos (ang) * rad * 0.15f, cy + std::sin (ang) * rad * 0.15f, cx + std::cos (ang) * rad * 0.72f, cy + std::sin (ang) * rad * 0.72f, 2.0f);
                }
                // which of the 12 knobs turns it now (Shift: the second layer)
                const int hw = box.hardwareKnobFor (idx);
                const bool layer = hw > 0 && ((hw <= 12) != box.isShift());
                if (layer)
                {
                    const auto badge = juce::Rectangle<float> (12.0f, 12.0f).withPosition (kc.getRight() - 6.0f, kc.getY() - 3.0f);
                    g.setColour (acc);
                    g.fillEllipse (badge);
                    text1 (g, juce::String ((hw - 1) % 12 + 1), badge.toNearestInt(), 8.0f, acc.contrasting (0.9f), juce::Justification::centred, true);
                }
            }
        }
    };
    layoutRow (rowTop, 0, split);
    layoutRow (rowBottom, split, (int) sections.size());
}

void View::drawSynthLive (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
    const auto& d = engineDef (EngineSynth);
    const int mdl = juce::roundToInt (tr.params[(size_t) d.index ("model")]);
    const auto& lk = synthModelLook (mdl);
    const juce::Colour pnl (lk.panel), acc (lk.accent), ink (lk.ink);
    g.setColour (pnl);
    g.fillRect (r);
    r = r.reduced (6, 4);
    // left: the circular scope; middle: the wave and its spectrum; right: the filter and the two envelopes
    const int side = juce::jmin (r.getHeight(), 240);
    drawPolarScope (g, r.removeFromLeft (side), acc);
    r.removeFromLeft (6);
    auto right = r.removeFromRight (230);
    r.removeFromRight (6);
    auto mid = r;
    auto frame = [&] (juce::Rectangle<int> b, const juce::String& title)
    {
        g.setColour (pnl.contrasting (0.07f));
        g.fillRoundedRectangle (b.toFloat(), 6.0f);
        text1 (g, title, b.withHeight (16).reduced (8, 0), 10.0f, ink.withAlpha (0.7f), juce::Justification::centredLeft, true);
        return b.withTrimmedTop (16).reduced (6, 4);
    };
    auto scopeR = mid.removeFromTop (mid.getHeight() / 2);
    mid.removeFromTop (6);
    {
        auto in = frame (scopeR, "WAVE");
        g.setColour (pnl.contrasting (0.12f));
        g.drawHorizontalLine (in.getCentreY(), (float) in.getX(), (float) in.getRight());
        auto& a = box.getAudio();
        const int n = Audio::scopeSize, w = a.scope.size() > 0 ? a.scopeWrite.load (std::memory_order_relaxed) : 0;
        int start = 0;
        for (int i = 1; i < n / 2; ++i)
            if (a.scope[(size_t) ((w + i - 1) % n)].load (std::memory_order_relaxed) <= 0.0f && a.scope[(size_t) ((w + i) % n)].load (std::memory_order_relaxed) > 0.0f) { start = i; break; }
        juce::Path p;
        const int len = n / 2;
        for (int i = 0; i < len; ++i)
        {
            const float v = juce::jlimit (-1.0f, 1.0f, a.scope[(size_t) ((w + start + i) % n)].load (std::memory_order_relaxed) * 1.8f);
            const float x = in.getX() + in.getWidth() * (float) i / (float) (len - 1), y = in.getCentreY() - v * in.getHeight() * 0.46f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (acc.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (acc);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    drawSpectrum (g, frame (mid, "SPECTRUM"), acc);
    // right column
    auto fr = right.removeFromTop (right.getHeight() / 2);
    right.removeFromTop (6);
    drawFilterCurve (g, frame (fr, "FILTER  " + d.params[(size_t) d.index ("cutoff")].format (tr.params[(size_t) d.index ("cutoff")])
                                   + "  " + d.params[(size_t) d.index ("fmode")].format (tr.params[(size_t) d.index ("fmode")])), acc);
    auto envR = right;
    auto e1 = envR.removeFromLeft (envR.getWidth() / 2 - 3);
    envR.removeFromLeft (6);
    auto par = [&] (const char* id) { return tr.params[(size_t) d.index (id)]; };
    drawEnvelope (g, frame (e1, "FILTER ENV"), {}, par ("fa"), par ("fd"), par ("fs"), par ("fr"), acc);
    drawEnvelope (g, frame (envR, "AMP ENV"), {}, par ("a"), par ("d"), par ("s"), par ("r"), acc);
}

void View::drawPolarScope (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour acc)
{
    // the wave wrapped around a circle: one period = one turn, so a tone stands still as a flower; the old turns glow on
    const auto c = r.getCentre().toFloat();
    const float R = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f - 6.0f;
    g.setColour (acc.withAlpha (0.08f));
    g.fillEllipse (juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
    g.setColour (acc.withAlpha (0.18f));
    g.drawEllipse (juce::Rectangle<float> (R * 1.1f, R * 1.1f).withCentre (c), 1.0f);
    g.drawEllipse (juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c), 1.0f);
    auto& a = box.getAudio();
    const int n = Audio::scopeSize, w = a.scopeWrite.load (std::memory_order_relaxed);
    auto at = [&a, n, w] (int i) { return a.scope[(size_t) (((w + i) % n + n) % n)].load (std::memory_order_relaxed); };
    // one period: from a rising zero crossing to the next
    int z0 = -1, z1 = -1;
    for (int i = 1; i < n - 1 && z1 < 0; ++i)
        if (at (i - 1) <= 0.0f && at (i) > 0.0f) { if (z0 < 0) z0 = i; else if (i - z0 > 6) z1 = i; }
    int period = z0 >= 0 && z1 > z0 ? z1 - z0 : n / 2;
    if (z0 < 0) z0 = 0;
    period = juce::jlimit (16, n - z0 - 1, period * (period < 40 ? 4 : period < 90 ? 2 : 1));   // short periods: a few turns
    float peak = 0.0f;
    for (int i = 0; i < period; ++i) peak = juce::jmax (peak, std::abs (at (z0 + i)));
    const float gain = peak > 0.0001f ? 0.42f / juce::jmax (0.08f, peak) : 0.0f;
    juce::Path p;
    for (int i = 0; i <= period; ++i)
    {
        const float v = juce::jlimit (-1.0f, 1.0f, at (z0 + (i % period)) * gain);
        const float ang = juce::MathConstants<float>::twoPi * (float) i / (float) period - juce::MathConstants<float>::halfPi;
        const float rad = R * (0.55f + v);
        const float x = c.x + std::cos (ang) * rad, y = c.y + std::sin (ang) * rad;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    p.closeSubPath();
    polarTrail.push_back (p);
    if (polarTrail.size() > 8) polarTrail.erase (polarTrail.begin());
    for (size_t k = 0; k < polarTrail.size(); ++k)
    {
        const float age = (float) (k + 1) / (float) polarTrail.size();
        g.setColour (acc.withAlpha (0.05f + 0.25f * age * age));
        g.strokePath (polarTrail[k], juce::PathStrokeType (1.0f + 3.0f * age));
    }
    g.setColour (acc.brighter (0.4f));
    g.strokePath (p, juce::PathStrokeType (1.6f));
    g.setColour (acc.withAlpha (0.12f));
    g.fillPath (p);
}

void View::drawSpectrum (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour acc)
{
    // 48 bands from 40 Hz to 11 kHz (Goertzel on the scope's 512 samples at 24 kHz, Hann window)
    auto& a = box.getAudio();
    const int n = Audio::scopeSize, w = a.scopeWrite.load (std::memory_order_relaxed);
    const double fs = a.getSampleRate() / 2.0;
    std::array<float, Audio::scopeSize> x {};
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = a.scope[(size_t) ((w + i) % n)].load (std::memory_order_relaxed) * (0.5f - 0.5f * std::cos (dsp::twoPi * (float) i / (float) (n - 1)));
    const int bands = (int) spectrum.size();
    const float bw = (float) r.getWidth() / (float) bands;
    for (int b = 0; b < bands; ++b)
    {
        const double f = 40.0 * std::pow (11000.0 / 40.0, (double) b / (double) (bands - 1));
        const double coeff = 2.0 * std::cos (2.0 * juce::MathConstants<double>::pi * f / fs);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < n; ++i) { const double s0 = x[(size_t) i] + coeff * s1 - s2; s2 = s1; s1 = s0; }
        const double mag = std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / (n * 0.25);
        const float db = juce::Decibels::gainToDecibels ((float) mag, -80.0f);
        const float lvl = juce::jlimit (0.0f, 1.0f, (db + 66.0f) / 60.0f);
        spectrum[(size_t) b] = lvl > spectrum[(size_t) b] ? lvl : spectrum[(size_t) b] * 0.86f + lvl * 0.14f;
        const float h = spectrum[(size_t) b] * (float) r.getHeight();
        auto bar = juce::Rectangle<float> (r.getX() + b * bw + 1.0f, r.getBottom() - h, juce::jmax (1.0f, bw - 2.0f), h);
        g.setColour (acc.withAlpha (0.35f + 0.65f * spectrum[(size_t) b]));
        g.fillRoundedRectangle (bar, 1.5f);
    }
}

void View::drawFilterCurve (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour acc)
{
    // the filter's answer over 20 Hz - 20 kHz (-48 .. +18 dB): the model's slope, the resonance peak, the mode
    const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
    const auto& d = engineDef (EngineSynth);
    auto par = [&] (const char* id) { return tr.params[(size_t) d.index (id)]; };
    const int mdl = juce::roundToInt (par ("model")), mode = juce::roundToInt (par ("fmode"));
    const float fc = par ("cutoff"), reso = par ("reso"), hp = par ("hpf");
    const bool twoPole = mdl == ModelOberheim || mdl == ModelMS || mdl == ModelBuchla;
    const int poles = mdl == ModelAcid ? 3 : 4;
    g.setColour (acc.withAlpha (0.12f));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        const float x = r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
        g.drawVerticalLine ((int) x, (float) r.getY(), (float) r.getBottom());
    }
    auto dbToY = [&r] (float db) { return r.getY() + r.getHeight() * (1.0f - (juce::jlimit (-48.0f, 18.0f, db) + 48.0f) / 66.0f); };
    g.drawHorizontalLine ((int) dbToY (0.0f), (float) r.getX(), (float) r.getRight());
    juce::Path p, fill;
    const int steps = r.getWidth();
    for (int i = 0; i <= steps; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / (float) steps);
        const std::complex<float> s (0.0f, f / fc);
        std::complex<float> h;
        if (twoPole)
        {
            const float q = (mdl == ModelMS ? 0.6f + reso * reso * 28.0f : 0.55f + reso * reso * 18.0f) * 0.5f + 0.35f;
            const auto den = s * s + s / q + 1.0f;
            switch (mode) { case 1: h = (s / q) / den; break; case 2: h = (s * s) / den; break; case 3: h = (s * s + 1.0f) / den; break; default: h = 1.0f / den; }
        }
        else
        {
            const float k = reso * 3.9f;
            auto G = 1.0f / (1.0f + s);
            std::complex<float> Gn (1.0f, 0.0f);
            for (int q = 0; q < poles; ++q) Gn *= G;
            const auto lp = Gn / (1.0f + k * Gn) * (1.0f + 0.5f * k * 0.5f);
            switch (mode)
            {
                case 1: h = lp * s * 2.0f; break;
                case 2: { auto hpRes = s * s * s * s / ((1.0f + s) * (1.0f + s) * (1.0f + s) * (1.0f + s)); h = hpRes / (1.0f + k * Gn); break; }
                case 3: h = lp + (s * s * s * s / ((1.0f + s) * (1.0f + s) * (1.0f + s) * (1.0f + s))) / (1.0f + k * Gn); break;
                default: h = lp; break;
            }
        }
        if (hp > 21.0f) h *= std::complex<float> (0.0f, f / hp) / (1.0f + std::complex<float> (0.0f, f / hp));
        const float db = juce::Decibels::gainToDecibels (std::abs (h), -60.0f);
        const float x = (float) r.getX() + (float) i, y = dbToY (db);
        if (i == 0) { p.startNewSubPath (x, y); fill.startNewSubPath (x, (float) r.getBottom()); }
        else p.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo ((float) r.getRight(), (float) r.getBottom());
    fill.closeSubPath();
    g.setColour (acc.withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (acc);
    g.strokePath (p, juce::PathStrokeType (2.0f));
}

void View::drawEnvelope (juce::Graphics& g, juce::Rectangle<int> er, const juce::String&, float a, float dd, float s, float rel, juce::Colour acc)
{
    const float total = a + dd + 0.4f + rel;
    const float W = (float) er.getWidth(), H = (float) er.getHeight();
    auto X = [&] (float tt) { return er.getX() + W * std::sqrt (tt / total); };
    juce::Path p;
    p.startNewSubPath ((float) er.getX(), (float) er.getBottom());
    p.lineTo (X (a), (float) er.getY());
    p.quadraticTo (X (a + dd * 0.3f), er.getY() + H * (1.0f - s) * 0.85f, X (a + dd), er.getY() + H * (1.0f - s));
    p.lineTo (X (a + dd + 0.4f), er.getY() + H * (1.0f - s));
    p.quadraticTo (X (a + dd + 0.4f + rel * 0.3f), (float) er.getBottom(), X (total), (float) er.getBottom());
    juce::Path fill (p);
    fill.lineTo ((float) er.getX(), (float) er.getBottom());
    fill.closeSubPath();
    g.setColour (acc.withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (acc);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void View::drawTouch (juce::Graphics& g, juce::Rectangle<int> area, bool big)
{
    const auto& t = box.lastTouch();
    if (t.name.isEmpty()) return;
    const auto age = juce::Time::getMillisecondCounter() - t.at;
    if (age > 1600) return;
    const float alpha = age < 1200 ? 1.0f : 1.0f - (float) (age - 1200) / 400.0f;
    if (! big)   // the PC: one line
    {
        const auto s = t.name + "  " + t.value + (t.waiting ? "   (turn the knob to " + juce::String (juce::roundToInt (t.norm * 100.0f)) + "% to take over)" : juce::String());
        g.setFont (font (15.0f, true));
        const int w = juce::jmin (screenW - 40, juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) + 36);
        const juce::Rectangle<int> pill ((screenW - w) / 2, area.getBottom() - 46, w, 30);
        g.setColour (juce::Colour (0xf0282a2f).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (pill.toFloat(), 15.0f);
        text1 (g, s, pill, 15.0f, text.withMultipliedAlpha (alpha), juce::Justification::centred, true);
        return;
    }
    // the box: big in the middle - the name, the value, a ring; waiting: where the pot is, where it takes over
    const auto b = juce::Rectangle<int> (300, 190).withCentre (area.getCentre());
    g.setColour (juce::Colour (0xf0101010).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (b.toFloat(), 14.0f);
    g.setColour (accent.withMultipliedAlpha (alpha * 0.6f));
    g.drawRoundedRectangle (b.toFloat(), 14.0f, 1.5f);
    auto in = b.reduced (14, 10);
    text1 (g, t.name.toUpperCase(), in.removeFromTop (26), 20.0f, juce::Colours::white.withMultipliedAlpha (alpha), juce::Justification::centred, true);
    auto ringR = in.removeFromTop (100).toFloat().withSizeKeepingCentre (96, 96);
    const float a0 = -2.4f, a1 = 2.4f, cx = ringR.getCentreX(), cy = ringR.getCentreY(), rad = 44.0f;
    juce::Path back; back.addCentredArc (cx, cy, rad, rad, 0.0f, a0, a1, true);
    g.setColour (juce::Colours::white.withAlpha (0.15f * alpha));
    g.strokePath (back, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path arc; arc.addCentredArc (cx, cy, rad, rad, 0.0f, a0, a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, t.norm) + 0.001f, true);
    g.setColour (accent.withMultipliedAlpha (alpha));
    g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (t.waiting && t.potNorm >= 0.0f)   // where the pot is now: a dot on the ring
    {
        const float ang = a0 + (a1 - a0) * t.potNorm - juce::MathConstants<float>::halfPi;
        g.setColour (juce::Colours::white.withMultipliedAlpha (alpha));
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ cx + std::cos (ang) * rad, cy + std::sin (ang) * rad }));
    }
    text1 (g, t.value, ringR.toNearestInt(), 18.0f, juce::Colours::white.withMultipliedAlpha (alpha), juce::Justification::centred, true);
    text1 (g, t.waiting ? juce::String ("turn to the orange end to take over") : juce::String(), in, 13.0f,
           juce::Colours::white.withAlpha (0.7f * alpha), juce::Justification::centred);
}

void View::drawDrumMachine (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& p = box.getProject();
    const int t = box.getTrack();
    const auto& tr = p.tracks[(size_t) t];
    auto bar = r.removeFromTop (34);
    r.removeFromTop (4);
    auto button = [&] (juce::Rectangle<int> b, const juce::String& label, int action, bool on = false, float size = 14.0f)
    {
        g.setColour (on ? accent : cell);
        g.fillRoundedRectangle (b.toFloat().reduced (2.0f), 6.0f);
        text1 (g, label, b, size, on ? bg : text, juce::Justification::centred, true);
        actionHits.add ({ b, action });
    };
    const juce::String arrowL = juce::String::fromUTF8 ("\xe2\x97\x80"), arrowR = juce::String::fromUTF8 ("\xe2\x96\xb6");
    // kit
    button (bar.removeFromLeft (34), arrowL, 1, false, 11.0f);
    {
        auto k = bar.removeFromLeft (190);
        g.setColour (panel); g.fillRoundedRectangle (k.toFloat().reduced (2.0f), 6.0f);
        text1 (g, "KIT", k.removeFromLeft (34), 10.0f, dim, juce::Justification::centred, true);
        text1 (g, box.presetName (t), k, 15.0f, text, juce::Justification::centredLeft, true);
    }
    button (bar.removeFromLeft (34), arrowR, 2, false, 11.0f);
    bar.removeFromLeft (8);
    // ready-made patterns
    button (bar.removeFromLeft (34), arrowL, 3, false, 11.0f);
    {
        auto k = bar.removeFromLeft (150);
        g.setColour (panel); g.fillRoundedRectangle (k.toFloat().reduced (2.0f), 6.0f);
        text1 (g, "BEAT", k.removeFromLeft (40), 10.0f, dim, juce::Justification::centred, true);
        text1 (g, box.drumPatternName(), k, 14.0f, text, juce::Justification::centredLeft, true);
    }
    button (bar.removeFromLeft (34), arrowR, 4, false, 11.0f);
    bar.removeFromLeft (8);
    // tempo, play, clear
    button (bar.removeFromLeft (28), "-", 5);
    text1 (g, juce::String (juce::roundToInt (p.m.tempo)) + " BPM", bar.removeFromLeft (70), 14.0f, text, juce::Justification::centred, true);
    button (bar.removeFromLeft (28), "+", 6);
    bar.removeFromLeft (8);
    button (bar.removeFromRight (70), "CLEAR", 8, false, 12.0f);
    button (bar.removeFromRight (80), box.getAudio().isPlaying() ? "STOP" : "PLAY", 7, box.getAudio().isPlaying());
    juce::ignoreUnused (tr);
    drawSeq (g, r);
}

void View::drawScenes (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& p = box.getProject();
    text1 (g, "SCENES" + sep + "pad 1-8 = recall (at the next bar)" + sep + "Rec + pad = save", r.removeFromTop (22), 13.0f, dim, juce::Justification::centredLeft, true);
    r.removeFromTop (6);
    auto top = r.removeFromTop (r.getHeight() / 2);
    const int w = top.getWidth() / 8;
    for (int s = 0; s < numScenes; ++s)
    {
        auto c = juce::Rectangle<int> (top.getX() + s * w, top.getY(), w, top.getHeight()).reduced (4);
        const bool has = box.sceneExists (s);
        g.setColour (has ? cell.brighter (0.25f) : cell);
        g.fillRoundedRectangle (c.toFloat(), 6.0f);
        text1 (g, juce::String (s + 1), c, 26.0f, has ? text : faint, juce::Justification::centred, true);
    }
    for (int t = 0; t < numTracks; ++t)
    {
        auto c = juce::Rectangle<int> (r.getX() + t * w, r.getY() + 8, w, r.getHeight() - 8).reduced (4);
        const bool on = ! p.tracks[(size_t) t].mute;
        g.setColour (on ? cell.brighter (0.2f) : bg);
        g.fillRoundedRectangle (c.toFloat(), 6.0f);
        g.setColour (line);
        g.drawRoundedRectangle (c.toFloat(), 6.0f, 1.0f);
        text1 (g, juce::String (t + 1) + (on ? "" : " off"), c, 16.0f, on ? text : faint, juce::Justification::centred, true);
    }
}

void View::drawLearn (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (bg.withAlpha (0.96f));
    g.fillRect (r);
    auto in = r.reduced (40, 30);
    text1 (g, "TEACH THE BOX A MIDI KEYBOARD  (" + juce::String (box.learnStep() + 1) + " / " + juce::String (box.learnSteps()) + ")",
           in.removeFromTop (24), 14.0f, dim, juce::Justification::centredLeft, true);
    text1 (g, box.learnPrompt(), in.removeFromTop (70), 44.0f, text, juce::Justification::centredLeft, true);
    text1 (g, box.learnHint(), in.removeFromTop (30), 18.0f, dim, juce::Justification::centredLeft);
    text1 (g, box.learnLast(), in.removeFromTop (30), 15.0f, accent, juce::Justification::centredLeft);
    auto buttons = in.removeFromBottom (60);
    skipHit = buttons.removeFromLeft (200);
    buttons.removeFromLeft (20);
    doneHit = buttons.removeFromLeft (200);
    for (auto [b, s] : { std::pair<juce::Rectangle<int>, const char*> { skipHit, "Skip" }, { doneHit, "Done" } })
    {
        g.setColour (cell.brighter (0.2f));
        g.fillRoundedRectangle (b.toFloat(), 8.0f);
        text1 (g, s, b, 20.0f, text, juce::Justification::centred, true);
    }
}

void View::drawHelp (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (bg.withAlpha (0.97f));
    g.fillRect (r);
    auto in = r.reduced (28, 18);
    text1 (g, "HELP  (touch, F1 or MENU > Help closes)", in.removeFromTop (24), 13.0f, dim, juce::Justification::centredLeft, true);
    in.removeFromTop (6);
    const auto lines = box.helpLines();
    for (int i = 0; i < lines.size(); ++i)
        text1 (g, lines[i], in.removeFromTop (i == 0 ? 34 : 27), i == 0 ? 20.0f : 16.0f, i == 0 ? text : text.withAlpha (0.85f), juce::Justification::centredLeft, i == 0);
    if (kiosk) return;
    in.removeFromTop (10);
    g.setColour (line);
    g.drawHorizontalLine (in.getY(), (float) in.getX(), (float) in.getRight());
    in.removeFromTop (8);
    const char* keys[] = {
        "PC:  1-8 / Q-I pads   Space Play   Enter Rec   Shift   Tab next track   Left / Right Prev / Next",
        "F2 Sound   F3 Seq   F4 Note FX   F5 Mix   F6 Project (twice: MENU)   N Scene   C (hold) Chaos   Z / X pad octave",
        "Mouse: drag or scroll a knob, double-click = start value; right-click = sound card, MIDI, full screen",
    };
    for (auto* l : keys) text1 (g, l, in.removeFromTop (22), 13.0f, dim, juce::Justification::centredLeft);
}

void View::drawMenu (juce::Graphics& g, juce::Rectangle<int> r)
{
    text1 (g, box.menuTitle(), r.removeFromTop (22), 14.0f, dim, juce::Justification::centredLeft, true);
    r.removeFromTop (4);
    const auto tiles = box.menuTiles();
    const int confirm = box.menuConfirming();
    // 2 rows of 8, like the pads (pads 1-8 on top)
    const int cw = r.getWidth() / 8, ch = r.getHeight() / 2;
    for (int i = 0; i < numPads; ++i)
    {
        auto c = juce::Rectangle<int> (r.getX() + (i % 8) * cw, r.getY() + (i / 8) * ch, cw, ch).reduced (3);
        menuHits.add (c);
        const auto& t = tiles[(size_t) i];
        if (t.label.isEmpty()) { g.setColour (bg); g.fillRoundedRectangle (c.toFloat(), 6.0f); continue; }
        const bool red = confirm == i;
        g.setColour (red ? recRed : (t.on ? accent.withAlpha (0.35f) : cell.brighter (t.enabled ? 0.18f : 0.0f)));
        g.fillRoundedRectangle (c.toFloat(), 6.0f);
        auto in = c.reduced (7, 6);
        text1 (g, juce::String (i + 1), in.removeFromTop (12), 10.0f, red ? text : faint, juce::Justification::topLeft);
        g.setColour (t.enabled ? text : faint);
        g.setFont (font (14.0f, true));
        g.drawFittedText (red ? "Press again" : t.label, in.removeFromTop (40), juce::Justification::topLeft, 2, 0.85f);
        if (t.detail.isNotEmpty() && ! red) text1 (g, t.detail, in, 10.5f, t.enabled ? dim : faint, juce::Justification::bottomLeft);
        if (t.danger && ! red) { g.setColour (recRed.withAlpha (0.8f)); g.fillRect (c.removeFromBottom (3).reduced (8, 0)); }
    }
}

//==============================================================================
// The PC panel around the screen
juce::Rectangle<float> View::knobRect (int k) const
{
    const float cw = screenW / 6.0f;
    return { 24.0f + (k % 6) * cw, 524.0f + (k / 6) * 84.0f, cw, 80.0f };
}

juce::Rectangle<float> View::buttonRect (int b) const
{
    const float x0 = 852.0f, w = 100.0f, h = 54.0f, gap = 8.0f;
    if (b >= BtnTrack1 && b <= BtnTrack8) return { x0 + (b % 4) * (w + gap), 24.0f + (b / 4) * (h + gap), w, h };
    const int row2[] = { BtnSound, BtnSeq, BtnFx, BtnMix };
    for (int i = 0; i < 4; ++i) if (row2[i] == b) return { x0 + i * (w + gap), 160.0f, w, h };
    const int row3[] = { BtnProject, BtnScene, BtnPrev, BtnNext };
    for (int i = 0; i < 4; ++i) if (row3[i] == b) return { x0 + i * (w + gap), 222.0f, w, h };
    const int row4[] = { BtnShift, BtnChaos, BtnRec, BtnPlay };
    for (int i = 0; i < 4; ++i) if (row4[i] == b) return { x0 + i * (w + gap), 284.0f, w, h };
    return {};
}

juce::Rectangle<float> View::padRect (int p) const
{
    const float w = 98.0f, h = 70.0f, gap = 8.0f;
    return { 24.0f + (p % 8) * (w + gap), 702.0f + (p / 8) * (h + gap), w, h };
}

juce::Rectangle<float> View::sliderRect() const { return { 870.0f, 370.0f, 60.0f, 300.0f }; }
juce::Rectangle<float> View::joyRect() const { return { 960.0f, 370.0f, 300.0f, 300.0f }; }

void View::drawPanel (juce::Graphics& g)
{
    // the screen's bezel
    g.setColour (juce::Colour (0xff050506));
    g.fillRoundedRectangle (screenRect().toFloat().expanded (6.0f), 8.0f);

    // knobs (under the screen's knob cells)
    for (int k = 0; k < numKnobs; ++k)
    {
        const auto r = knobRect (k);
        const auto v = box.knob (k);
        const auto c = r.getCentre().translated (0, -6);
        const float rad = 26.0f;
        g.setColour (hwBody);
        g.fillEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c));
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
        juce::Path ring;
        ring.addCentredArc (c.x, c.y, rad + 5, rad + 5, 0.0f, a0, a1, true);
        g.setColour (line);
        g.strokePath (ring, juce::PathStrokeType (3.0f));
        if (v.used)
        {
            const float av = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, v.norm);
            juce::Path val;
            val.addCentredArc (c.x, c.y, rad + 5, rad + 5, 0.0f, v.bipolar ? (a0 + a1) * 0.5f : a0, av, true);
            g.setColour (accent);
            g.strokePath (val, juce::PathStrokeType (3.0f));
            g.setColour (text);
            g.drawLine (c.x + std::sin (av) * rad * 0.35f, c.y - std::cos (av) * rad * 0.35f, c.x + std::sin (av) * rad * 0.85f, c.y - std::cos (av) * rad * 0.85f, 3.0f);
        }
        text1 (g, juce::String (k + 1), juce::Rectangle<float> (r.getX(), r.getBottom() - 16, r.getWidth(), 14).toNearestInt(), 11.0f, faint, juce::Justification::centred);
    }

    // buttons
    for (int b = 0; b < NumButtons; ++b)
    {
        const auto r = buttonRect (b);
        if (r.isEmpty()) continue;
        bool lit = false;
        if (b >= BtnTrack1 && b <= BtnTrack8) lit = box.getTrack() == b;
        else if (b == BtnSound) lit = box.getMode() == ModeSound || box.getMode() == ModeBrowse;
        else if (b == BtnSeq) lit = box.getMode() == ModeSeq;
        else if (b == BtnFx) lit = box.getMode() == ModeFx;
        else if (b == BtnMix) lit = box.getMode() == ModeMix;
        else if (b == BtnProject) lit = box.getMode() == ModeProject;
        else if (b == BtnShift) lit = box.isShift();
        else if (b == BtnScene) lit = box.isSceneMode();
        else if (b == BtnPlay) lit = box.getAudio().isPlaying();
        else if (b == BtnRec) lit = box.isRecArmed();
        const bool pressed = keyButtonDown[(size_t) b] || (drag == DragButton && dragIndex == b);
        g.setColour (pressed ? hwKey.brighter (0.3f) : hwKey);
        g.fillRoundedRectangle (r, 6.0f);
        if (lit)
        {
            g.setColour (b == BtnRec ? recRed : (b >= BtnTrack1 && b <= BtnTrack8 ? text : accent));
            g.fillRoundedRectangle (r.withHeight (4.0f).reduced (10.0f, 0.0f).translated (0, 6), 2.0f);
        }
        juce::String name = buttonName (b);
        if (b == BtnPrev) name = juce::String::fromUTF8 ("\xe2\x97\x80  Prev");
        if (b == BtnNext) name = juce::String::fromUTF8 ("Next  \xe2\x96\xb6");
        if (b >= BtnTrack1 && b <= BtnTrack8)
        {
            const auto& tr = box.getProject().tracks[(size_t) b];
            text1 (g, juce::String (b + 1), r.toNearestInt().reduced (10, 6).removeFromTop (22), 18.0f, text, juce::Justification::centredLeft, true);
            text1 (g, shortEngine (tr.engine), r.toNearestInt().reduced (10, 6).removeFromBottom (16), 11.0f, tr.mute ? faint : dim, juce::Justification::centredLeft, true);
            continue;
        }
        text1 (g, name, r.toNearestInt(), 14.0f, b == BtnRec ? recRed.withAlpha (0.9f) : text, juce::Justification::centred, true);
    }

    // the slider (DJ filter) and the joystick (bend / mod)
    {
        const auto s = sliderRect();
        g.setColour (hwBody);
        g.fillRoundedRectangle (s, 8.0f);
        g.setColour (line);
        g.fillRect (s.withSizeKeepingCentre (4.0f, s.getHeight() - 30.0f));
        const float y = s.getBottom() - 15.0f - (s.getHeight() - 30.0f) * box.getProject().m.djFilter;
        g.setColour (text);
        g.fillRoundedRectangle (juce::Rectangle<float> (s.getWidth() - 14.0f, 14.0f).withCentre ({ s.getCentreX(), y }), 3.0f);
        text1 (g, "FILTER", juce::Rectangle<float> (s.getX() - 10, s.getBottom() + 4, s.getWidth() + 20, 16).toNearestInt(), 11.0f, dim, juce::Justification::centred, true);
        const auto j = joyRect();
        g.setColour (hwBody);
        g.fillRoundedRectangle (j, 12.0f);
        g.setColour (line);
        g.drawLine (j.getCentreX(), j.getY() + 12, j.getCentreX(), j.getBottom() - 12, 1.0f);
        g.drawLine (j.getX() + 12, j.getCentreY(), j.getRight() - 12, j.getCentreY(), 1.0f);
        const auto jp = j.getCentre() + juce::Point<float> (joyPos.x * (j.getWidth() * 0.5f - 24), -joyPos.y * (j.getHeight() * 0.5f - 24));
        g.setColour (text);
        g.fillEllipse (juce::Rectangle<float> (34, 34).withCentre (jp));
        text1 (g, "JOYSTICK  bend / mod", juce::Rectangle<float> (j.getX(), j.getBottom() + 4, j.getWidth(), 16).toNearestInt(), 11.0f, dim, juce::Justification::centred, true);
    }

    // pads
    const auto menuNow = box.getMode() == ModeMenu ? box.menuTiles() : std::array<Controller::MenuTile, numPads> {};
    for (int p = 0; p < numPads; ++p)
    {
        const auto r = padRect (p);
        const bool down = keyPadDown[(size_t) p] || (drag == DragPad && dragIndex == p);
        g.setColour (down ? hwKey.brighter (0.35f) : hwKey.darker (0.1f));
        g.fillRoundedRectangle (r, 6.0f);
        if (box.padLit (p))
        {
            g.setColour (text.withAlpha (0.55f));
            g.drawRoundedRectangle (r.reduced (2.0f), 5.0f, 2.0f);
        }
        juce::String label;
        if (box.isSceneMode()) label = p < 8 ? "Scene " + juce::String (p + 1) : "Mute " + juce::String (p - 7);
        else if (box.getMode() == ModeMix) label = p < 8 ? "Mute " + juce::String (p + 1) : "Solo " + juce::String (p - 7);
        else if (box.getMode() == ModeSeq) label = "Step " + juce::String (box.seqBar() * 16 + p + 1);
        else if (box.getMode() == ModeMenu) label = menuNow[(size_t) p].label;
        else
        {
            const int n = box.padNote (box.getTrack(), p);
            const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
            label = tr.isDrums() ? box.laneName (box.getTrack(), p) : music::noteName (n);
            if (n < 0) label = {};
        }
        text1 (g, label, r.toNearestInt().reduced (8, 6), 13.0f, dim, juce::Justification::bottomLeft, true);
        const char* keys = "12345678QWERTYUI";
        text1 (g, juce::String::charToString ((juce::juce_wchar) keys[p]), r.toNearestInt().reduced (8, 6), 11.0f, faint, juce::Justification::topRight);
    }
    text1 (g, "F1 = keys" + sep + "right-click = settings", juce::Rectangle<int> (870, 830, 400, 20), 12.0f, faint, juce::Justification::centredRight);
}

//==============================================================================
// Mouse and touch
int View::screenKnobAt (juce::Point<float> sp) const
{
    if (! knobArea.contains (sp.toInt())) return -1;
    const int col = juce::jlimit (0, 5, (int) (sp.x / (screenW / 6.0f)));
    const int row = sp.y < knobArea.getCentreY() ? 0 : 1;
    return row * 6 + col;
}

bool View::screenTouch (juce::Point<float> sp, bool down)
{
    if (! down) return false;
    const auto p = sp.toInt();
    if (box.isLearning())
    {
        if (skipHit.contains (p)) { box.learnSkip(); return true; }
        if (doneHit.contains (p)) { box.learnDone(); return true; }
        return true;
    }
    if (box.isHelpShown()) { box.toggleHelp(); return true; }
    for (const auto& h : screenHits)
        if (h.r.contains (p)) { if (h.action == 0) box.goSynth(); else if (h.action == 1) box.goDrums(); else box.goStudio(); return true; }
    if (modelHit.contains (p)) { box.nextModel (1); return true; }
    for (const auto& h : actionHits)
        if (h.r.contains (p))
        {
            switch (h.action)
            {
                case 1: box.nextKit (-1); break;
                case 2: box.nextKit (1); break;
                case 3: box.nextDrumPattern (-1); break;
                case 4: box.nextDrumPattern (1); break;
                case 5: box.setMaster ([] (Master& m) { m.tempo = juce::jmax (40.0f, std::round (m.tempo) - 1.0f); }); break;
                case 6: box.setMaster ([] (Master& m) { m.tempo = juce::jmin (240.0f, std::round (m.tempo) + 1.0f); }); break;
                case 7: box.setPlaying (! box.getAudio().isPlaying()); break;
                case 8: box.clearPattern (box.getTrack()); break;
                default: break;
            }
            return true;
        }
    if (box.getMode() == ModeSeq)
        for (const auto& h : seqHits)
            if (h.r.contains (p))
            {
                if (h.lane >= 0 && h.step < 0) { box.keyDown (36 + h.lane, 100); box.keyUp (36 + h.lane); }   // a drum's name: hear it, pick it
                else if (h.lane >= 0) box.toggleDrumStep (h.lane, h.step);
                else box.clickStep (h.step);
                return true;
            }
    for (int i = 0; i < menuHits.size(); ++i) if (menuHits[i].contains (p)) { box.menuPress (i); return true; }
    for (int i = 0; i < trackHits.size(); ++i) if (trackHits[i].contains (p)) { box.selectTrack (i); return true; }
    for (int i = 0; i < modeHits.size(); ++i)
        if (modeHits[i].contains (p))
        {
            const int m = modeOrder[i];
            if (m == ModeSound && (box.getMode() == ModeSound || box.getMode() == ModeBrowse)) { box.buttonDown (BtnSound); box.buttonUp (BtnSound); }
            else box.setMode (m);
            return true;
        }
    for (int i = 0; i < pageHits.size(); ++i)
        if (pageHits[i].contains (p))
        {
            const int steps = i - box.pageIndex();
            const int b = steps > 0 ? BtnNext : BtnPrev;
            for (int k = 0; k < std::abs (steps); ++k) { box.buttonDown (b); box.buttonUp (b); }
            return true;
        }
    if (playHit.contains (p)) { box.buttonDown (BtnPlay); box.buttonUp (BtnPlay); return true; }
    if (box.getMode() == ModeBrowse)
        for (const auto& h : browseHits) if (h.r.contains (p)) { box.browseTo (h.column, h.index); return true; }
    if (soundNameHit.contains (p))   // the sound's name: open the browser (or close it)
    {
        if (box.getMode() == ModeBrowse) box.setMode (ModeSound);
        else { box.setMode (ModeSound); box.buttonDown (BtnSound); box.buttonUp (BtnSound); }
        return true;
    }
    return false;
}

void View::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto lp = toLogical (e.position);
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Sound card and MIDI...", onSettings != nullptr);
        m.addItem (2, "Learn a MIDI keyboard / controller...");
        m.addItem (3, "Forget the MIDI keyboard", box.numLearned() > 0);
        m.addItem (6, "Factory reset...");
        m.addSeparator();
        m.addItem (4, "Full screen", onToggleFullScreen != nullptr);
        m.addItem (5, "Keys (F1)");
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
        {
            if (r == 1 && onSettings) onSettings();
            if (r == 2) box.startLearn();
            if (r == 3) box.forgetMidiMap();
            if (r == 4 && onToggleFullScreen) onToggleFullScreen();
            if (r == 5) box.toggleHelp();
            if (r == 6)
            {
                juce::Component::SafePointer<View> safe (this);
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Factory reset",
                    "Start again with the factory project? Tracks, patterns, mixer and scenes go back to the start.\n"
                    "Your saved sounds, samples and the MIDI keyboard stay. Shift + Chaos (Undo) brings the project back.",
                    "Reset", "Cancel", nullptr,
                    juce::ModalCallbackFunction::create ([safe] (int ok) { if (ok == 1 && safe != nullptr) safe->box.factoryReset(); }));
            }
        });
        return;
    }
    const auto sr = screenRect().toFloat();
    if (sr.contains (lp))
    {
        const auto sp = lp - sr.getPosition();
        if (screenTouch (sp, true)) return;
        for (const auto& h : panelHits)
            if (h.r.contains (sp.toInt()))   // a knob of the synth panel: drag it (a click on a switch flips it)
            {
                drag = DragPanel; dragIndex = h.param; dragLastY = lp.y; panelActive = h.param; panelMoved = false;
                return;
            }
        const int k = screenKnobAt (sp);
        if (k >= 0) { drag = DragKnob; dragIndex = k; dragLastY = lp.y; return; }
        return;
    }
    if (kiosk) return;
    for (int k = 0; k < numKnobs; ++k)
        if (knobRect (k).contains (lp)) { drag = DragKnob; dragIndex = k; dragLastY = lp.y; return; }
    for (int b = 0; b < NumButtons; ++b)
        if (buttonRect (b).contains (lp))
        {
            drag = DragButton; dragIndex = b;
            if (b == BtnShift)   // a click on Shift latches it (on / off) so the mouse can use it
            {
                shiftLatchedByTouch = ! shiftLatchedByTouch;
                shiftLatchedByTouch ? box.buttonDown (BtnShift) : box.buttonUp (BtnShift);
                return;
            }
            box.buttonDown (b);
            return;
        }
    for (int p = 0; p < numPads; ++p)
        if (padRect (p).contains (lp))
        {
            const auto r = padRect (p);
            const int vel = juce::jlimit (20, 127, juce::roundToInt (127.0f - 100.0f * (lp.y - r.getY()) / r.getHeight()));
            drag = DragPad; dragIndex = p;
            box.padDown (p, vel);
            return;
        }
    if (sliderRect().contains (lp)) { drag = DragSlider; mouseDrag (e); return; }
    if (joyRect().contains (lp)) { drag = DragJoy; mouseDrag (e); return; }
}

void View::mouseDrag (const juce::MouseEvent& e)
{
    const auto lp = toLogical (e.position);
    switch (drag)
    {
        case DragKnob:
        {
            const float dy = dragLastY - lp.y;
            if (std::abs (dy) >= 2.0f)
            {
                box.turnKnob (dragIndex, (float) (int) (dy / 2.0f) * (e.mods.isCtrlDown() ? 0.25f : 1.0f));
                dragLastY -= (float) (int) (dy / 2.0f) * 2.0f;
            }
            break;
        }
        case DragPanel:
        {
            const float dy = dragLastY - lp.y;
            if (std::abs (dy) >= 1.0f)
            {
                box.nudgeParam (dragIndex, dy * (e.mods.isCtrlDown() ? 0.0012f : 0.0055f));
                dragLastY = lp.y;
                panelMoved = true;
            }
            break;
        }
        case DragSlider:
        {
            const auto s = sliderRect();
            box.setSlider ((s.getBottom() - 15.0f - lp.y) / (s.getHeight() - 30.0f));
            break;
        }
        case DragJoy:
        {
            const auto j = joyRect();
            joyPos = { juce::jlimit (-1.0f, 1.0f, (lp.x - j.getCentreX()) / (j.getWidth() * 0.5f - 24)),
                       juce::jlimit (-1.0f, 1.0f, (j.getCentreY() - lp.y) / (j.getHeight() * 0.5f - 24)) };
            box.setJoystick (joyPos.x, joyPos.y);
            break;
        }
        default: break;
    }
}

void View::mouseUp (const juce::MouseEvent&)
{
    if (drag == DragPanel)
    {
        const auto& d = engineDef (box.getProject().tracks[(size_t) box.getTrack()].engine);
        if (! panelMoved && dragIndex >= Controller::panelFx0 && box.panelFxStepped (dragIndex)) box.cycleParam (dragIndex, 1);
        else if (! panelMoved && dragIndex >= 0 && dragIndex < (int) d.params.size()
            && (d.params[(size_t) dragIndex].unit == ParamDef::Toggle || d.params[(size_t) dragIndex].unit == ParamDef::Choice))
            box.cycleParam (dragIndex, 1);   // a click: the next position
        panelActive = -1;
    }
    if (drag == DragPad) box.padUp (dragIndex);
    if (drag == DragButton && dragIndex != BtnShift) box.buttonUp (dragIndex);
    if (drag == DragJoy) { joyPos = {}; box.setJoystick (0.0f, 0.0f); }   // springs back
    drag = DragNone;
    dragIndex = -1;
}

void View::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto lp = toLogical (e.position);
    const auto sr = screenRect().toFloat();
    if (sr.contains (lp))
        for (const auto& h : panelHits)
            if (h.r.contains ((lp - sr.getPosition()).toInt()))
            {
                const auto& d = engineDef (box.getProject().tracks[(size_t) box.getTrack()].engine);
                if (h.param >= Controller::panelFx0) { if (! box.panelFxStepped (h.param)) box.resetParam (h.param); }
                else if (h.param < (int) d.params.size() && d.params[(size_t) h.param].unit != ParamDef::Choice && d.params[(size_t) h.param].unit != ParamDef::Toggle)
                    box.resetParam (h.param);
                return;
            }
    int k = -1;
    if (sr.contains (lp)) k = screenKnobAt (lp - sr.getPosition());
    else if (! kiosk) for (int i = 0; i < numKnobs; ++i) if (knobRect (i).contains (lp)) k = i;
    if (k >= 0) box.resetKnob (k);
    if (! kiosk && sliderRect().contains (lp)) box.setSlider (0.5f);
}

void View::mouseMove (const juce::MouseEvent& e)
{
    int c = -1, idx = -1;
    if (box.getMode() == ModeBrowse)
    {
        const auto lp = toLogical (e.position);
        const auto sr = screenRect().toFloat();
        if (sr.contains (lp))
        {
            const auto sp = (lp - sr.getPosition()).toInt();
            for (const auto& h : browseHits) if (h.r.contains (sp)) { c = h.column; idx = h.index; }
        }
    }
    const auto spos = (toLogical (e.position) - screenRect().toFloat().getPosition()).toInt();
    const bool overName = soundNameHit.contains (spos) || modelHit.contains (spos);
    int ph = -1;
    for (const auto& h : panelHits) if (h.r.contains (spos)) ph = h.param;
    bool overAction = false;
    for (const auto& h : actionHits) overAction = overAction || h.r.contains (spos);
    for (const auto& h : screenHits) overAction = overAction || h.r.contains (spos);
    if (box.getMode() == ModeSeq) for (const auto& h : seqHits) overAction = overAction || h.r.contains (spos);
    setMouseCursor (ph >= 0 ? juce::MouseCursor::UpDownResizeCursor
                    : (c >= 0 || overName || overAction ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor));
    if (ph != panelHover) { panelHover = ph; repaint(); }
    if (c != hoverColumn || idx != hoverIndex) { hoverColumn = c; hoverIndex = idx; repaint(); }
}

void View::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto lp = toLogical (e.position);
    const auto sr = screenRect().toFloat();
    int k = -1;
    const float dd = (w.deltaY != 0.0f ? w.deltaY : -w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    if (sr.contains (lp))
        for (const auto& h : panelHits)
            if (h.r.contains ((lp - sr.getPosition()).toInt()))   // the wheel turns the panel knob under the mouse
            {
                const auto& pdefs = engineDef (box.getProject().tracks[(size_t) box.getTrack()].engine).params;
                const bool fxStep = h.param >= Controller::panelFx0 && box.panelFxStepped (h.param);
                const bool stepped = fxStep || (h.param < (int) pdefs.size() && pdefs[(size_t) h.param].stepped());
                const float stepN = fxStep ? 0.15f : stepped ? 1.01f / juce::jmin (24.0f, pdefs[(size_t) h.param].hi - pdefs[(size_t) h.param].lo + 1.0f) : 0.02f;
                if (dd != 0.0f) box.nudgeParam (h.param, dd > 0 ? stepN : -stepN);
                return;
            }
    if (sr.contains (lp) && box.getMode() == ModeBrowse)   // the wheel scrolls the list under the mouse
    {
        const auto sp = (lp - sr.getPosition()).toInt();
        for (int c = 0; c < 3; ++c)
            if (browseColumns[(size_t) c].contains (sp))
            {
                const int cur = c == 0 ? box.browseEngineIndex() : c == 1 ? box.browseCategory() : box.browsePresetIndex();
                const int step = dd > 0 ? -1 : 1;
                if (dd != 0.0f) box.browseTo (c, cur + step);
                return;
            }
    }
    if (sr.contains (lp)) k = screenKnobAt (lp - sr.getPosition());
    else if (! kiosk) for (int i = 0; i < numKnobs; ++i) if (knobRect (i).contains (lp)) k = i;
    if (k < 0) return;
    const float d = (w.deltaY != 0.0f ? w.deltaY : -w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    const float clicks = d > 0 ? juce::jmax (1.0f, std::round (d * 8.0f)) : juce::jmin (-1.0f, std::round (d * 8.0f));
    box.turnKnob (k, clicks);
}

//==============================================================================
// The computer keyboard
bool View::keyPressed (const juce::KeyPress& k)
{
    const int code = k.getKeyCode();
    const auto mods = k.getModifiers();
    auto press = [this] (int b) { box.buttonDown (b); box.buttonUp (b); };
    if (code == juce::KeyPress::F1Key) { box.toggleHelp(); return true; }
    if (code == juce::KeyPress::spaceKey) { press (BtnPlay); return true; }
    if (code == juce::KeyPress::returnKey) { press (BtnRec); return true; }
    if (code == juce::KeyPress::tabKey) { box.selectTrack ((box.getTrack() + (mods.isShiftDown() ? numTracks - 1 : 1)) % numTracks); return true; }
    if (box.getMode() == ModeBrowse && (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey))
    {
        box.browseTo (2, box.browsePresetIndex() + (code == juce::KeyPress::upKey ? -1 : 1));
        return true;
    }
    if (box.getMode() == ModeBrowse && (code == juce::KeyPress::pageUpKey || code == juce::KeyPress::pageDownKey))
    {
        box.browseTo (1, box.browseCategory() + (code == juce::KeyPress::pageUpKey ? -1 : 1));
        return true;
    }
    if (code == juce::KeyPress::leftKey) { press (BtnPrev); return true; }
    if (code == juce::KeyPress::rightKey) { press (BtnNext); return true; }
    if (code == juce::KeyPress::F2Key) { press (BtnSound); return true; }
    if (code == juce::KeyPress::F3Key) { press (BtnSeq); return true; }
    if (code == juce::KeyPress::F4Key) { press (BtnFx); return true; }
    if (code == juce::KeyPress::F5Key) { press (BtnMix); return true; }
    if (code == juce::KeyPress::F6Key) { press (BtnProject); return true; }
    if (code == juce::KeyPress::F11Key && onToggleFullScreen) { onToggleFullScreen(); return true; }
    if (code == juce::KeyPress::escapeKey) { if (box.isHelpShown()) box.toggleHelp(); if (box.isLearning()) box.learnDone(); return true; }
    const juce::juce_wchar ch = juce::CharacterFunctions::toUpperCase (k.getTextCharacter());
    if (ch == 'N') { press (BtnScene); return true; }
    if (ch == 'Z' || ch == 'X') { box.buttonDown (BtnShift); press (ch == 'Z' ? BtnPrev : BtnNext); box.buttonUp (BtnShift); return true; }
    // pads and Chaos are handled in keyStateChanged (held keys)
    return juce::String ("12345678QWERTYUIC").containsChar (ch) || (ch >= '!' && ch <= '*');
}

bool View::keyStateChanged (bool)
{
    const char* keys = "12345678QWERTYUI";
    bool used = false;
    for (int p = 0; p < numPads; ++p)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown (keys[p]) || juce::KeyPress::isKeyCurrentlyDown (juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) keys[p]));
        if (down != keyPadDown[(size_t) p])
        {
            keyPadDown[(size_t) p] = down;
            down ? box.padDown (p, 100) : box.padUp (p);
            used = true;
        }
    }
    const bool c = juce::KeyPress::isKeyCurrentlyDown ('c') || juce::KeyPress::isKeyCurrentlyDown ('C');
    if (c != keyButtonDown[BtnChaos])
    {
        keyButtonDown[BtnChaos] = c;
        c ? box.buttonDown (BtnChaos) : box.buttonUp (BtnChaos);
        used = true;
    }
    return used;
}

void View::modifierKeysChanged (const juce::ModifierKeys& m)
{
    if (shiftLatchedByTouch) return;
    if (m.isShiftDown() != box.isShift()) m.isShiftDown() ? box.buttonDown (BtnShift) : box.buttonUp (BtnShift);
}
} // namespace catbox

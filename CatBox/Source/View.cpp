#include "View.h"
#include "Music.h"
#include "Samples.h"
#include <cmath>

namespace catbox
{
namespace
{
// calm: greys, one accent; red only for Rec
const juce::Colour bg       { 0xff0f1012 };
const juce::Colour panel    { 0xff18191c };
const juce::Colour cell     { 0xff1f2024 };
const juce::Colour line     { 0xff2c2e33 };
const juce::Colour text     { 0xffeeeeee };
const juce::Colour dim      { 0xff8b8e95 };
const juce::Colour faint    { 0xff4a4d54 };
const juce::Colour accent   { 0xffff8a3d };
const juce::Colour recRed   { 0xffff453a };
const juce::Colour hwBody   { 0xff2a2b2f };
const juce::Colour hwKey    { 0xff3a3c41 };

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
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
    static const char* n[] = { "ANLG", "ACID", "DRUM", "KEYS", "GLDE", "FM", "WAVE", "CHOR", "SMPL", "GRAN" };
    return n[juce::jlimit (0, 9, e)];
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
    g.setColour (bg);
    g.fillRect (0, 0, screenW, screenH);
    auto r = juce::Rectangle<int> (0, 0, screenW, screenH);
    drawHeader (g, r.removeFromTop (52));
    drawTabs (g, r.removeFromTop (34));
    auto tracks = r.removeFromBottom (32);
    auto knobs = r.removeFromBottom (134);
    knobArea = knobs;
    drawContent (g, r.reduced (10, 4));
    drawKnobs (g, knobs);
    drawTracks (g, tracks);

    if (box.isLearning()) drawLearn (g, juce::Rectangle<int> (0, 52, screenW, screenH - 52));
    if (showHelp) drawHelp (g, juce::Rectangle<int> (0, 0, screenW, screenH));

    // a short message, on top of the content
    const auto f = box.flashText();
    if (f.isNotEmpty() && ! box.isLearning())
    {
        g.setFont (font (16.0f, true));
        const int w = juce::jmin (screenW - 40, juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), f) + 36);
        const juce::Rectangle<int> pill ((screenW - w) / 2, 92, w, 32);
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
    g.setColour (text);
    g.fillRoundedRectangle (num.toFloat().reduced (0, 2), 6.0f);
    text1 (g, juce::String (t + 1), num, 24.0f, bg, juce::Justification::centred, true);
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

    // engine above, sound name big
    auto top = r.removeFromTop (14);
    text1 (g, engineDef (tr.engine).name.toUpperCase() + (tr.fxType != FxOff ? "   +  " + noteFxNames()[tr.fxType].toUpperCase() : juce::String()),
           top, 12.0f, dim, juce::Justification::centredLeft, true);
    text1 (g, box.presetName (t) + (box.isEdited (t) ? " *" : ""), r, 24.0f, text, juce::Justification::centredLeft, true);
}

void View::drawTabs (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (line);
    g.drawHorizontalLine (r.getBottom() - 1, 0.0f, (float) screenW);
    r = r.reduced (10, 0);
    modeHits.clearQuick();
    pageHits.clearQuick();
    const int m = box.getMode();
    // modes (left)
    for (int mm : modeOrder)
    {
        const juce::String name = mm == ModeSound && m == ModeBrowse ? "BROWSE" : modeName (mm);
        g.setFont (font (14.0f, true));
        const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 18;
        auto cellR = r.removeFromLeft (w);
        const bool on = mm == m || (mm == ModeSound && m == ModeBrowse);
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
    auto col = [&] (juce::Rectangle<int> c, const juce::String& title, const juce::StringArray& items, int sel, float size)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (c.toFloat(), 8.0f);
        c = c.reduced (10, 8);
        text1 (g, title, c.removeFromTop (16), 12.0f, dim, juce::Justification::centredLeft, true);
        c.removeFromTop (4);
        const int rowH = (int) (size + 12);
        const int rows = juce::jmax (1, c.getHeight() / rowH);
        const int first = juce::jlimit (0, juce::jmax (0, items.size() - rows), sel - rows / 2);
        for (int i = first; i < juce::jmin (items.size(), first + rows); ++i)
        {
            auto row = c.removeFromTop (rowH);
            if (i == sel)
            {
                g.setColour (text);
                g.fillRoundedRectangle (row.toFloat(), 5.0f);
            }
            text1 (g, items[i], row.reduced (8, 0), size, i == sel ? bg : dim, juce::Justification::centredLeft, i == sel);
        }
    };
    const int w = r.getWidth();
    juce::StringArray engines = engineNames();
    // the engine shown is what knob 1 picks
    int eSel = 0;
    const auto k0 = box.knob (0);
    for (int i = 0; i < engines.size(); ++i) if (engines[i] == k0.value) eSel = i;
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
            const auto name = d.params[(size_t) l * 4].name.upToFirstOccurrenceOf (" tune", false, false);
            if (sel) { g.setColour (cell.brighter (0.1f)); g.fillRect (juce::Rectangle<float> ((float) r.getX(), y, (float) r.getWidth(), rowH)); }
            const float glow = laneGlow[(size_t) l];
            text1 (g, name, juce::Rectangle<float> ((float) r.getX() + 4, y, (float) nameW - 6, rowH).toNearestInt(), juce::jmin (13.0f, rowH),
                   sel ? text : dim.interpolatedWith (text, glow), juce::Justification::centredLeft, sel);
            for (int s = 0; s < 16; ++s)
            {
                const int step = barIdx * 16 + s;
                auto c = juce::Rectangle<float> (r.getX() + nameW + s * cw, y, cw, rowH).reduced (1.5f, 1.5f);
                if (step >= pat.length) { g.setColour (bg); g.fillRect (c); continue; }
                const int v = pat.drums[(size_t) l][(size_t) step];
                const bool held = sel && box.isStepHeld (step);
                g.setColour (v > 0 ? (v >= 120 ? accent : text.withAlpha (0.6f + 0.4f * v / 127.0f)) : (s % 4 == 0 ? line.brighter (0.05f) : line.darker (0.15f)));
                g.fillRoundedRectangle (c, 2.0f);
                if (held) { g.setColour (accent); g.drawRoundedRectangle (c, 2.0f, 2.0f); }
                if (step == playing) { g.setColour (accent.withAlpha (0.6f)); g.fillRoundedRectangle (c.withHeight (2.0f).withY (c.getBottom() - 2.0f), 1.0f); }
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
        const auto& st = pat.steps[(size_t) step];
        const bool held = box.isStepHeld (step);
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
        if (st.slide) marks << "SLIDE ";
        if (st.accent) marks << "ACC ";
        if (st.chance < 100) marks << st.chance << "% ";
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
        auto sA = sends.removeFromLeft (sends.getWidth() / 2).reduced (1, 4).toFloat(), sB = sends.reduced (1, 4).toFloat();
        g.setColour (line); g.fillRect (sA); g.fillRect (sB);
        g.setColour (dim); g.fillRect (sA.withWidth (sA.getWidth() * tr.sendA)); g.fillRect (sB.withWidth (sB.getWidth() * tr.sendB));
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
    auto in = r.reduced (28, 20);
    text1 (g, "KEYS ON THE PC  (F1 closes)", in.removeFromTop (28), 18.0f, text, juce::Justification::centredLeft, true);
    const char* lines[] = {
        "1-8 / Q-I    the 16 pads (top row / bottom row)",
        "Space        Play / Stop          Enter   Rec",
        "Shift        Shift (hold)          Tab     next track (Shift + Tab: back)",
        "F2  Sound (again: browse)   F3  Seq   F4  Note FX   F5  Mix   F6  Project",
        "Left / Right Prev / Next page       N       Scenes      C (hold)  Chaos",
        "Z / X        pad octave down / up",
        "Mouse        drag or scroll a knob, double-click = back to the start",
        "             click a pad high = hard, low = soft; click tracks, modes and pages on the screen",
        "Right-click  sound card, MIDI, learn a keyboard, full screen",
    };
    for (auto* l : lines) text1 (g, l, in.removeFromTop (30), 16.0f, dim, juce::Justification::centredLeft);
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
        else
        {
            const int n = box.padNote (box.getTrack(), p);
            const auto& tr = box.getProject().tracks[(size_t) box.getTrack()];
            label = tr.isDrums() ? engineDef (tr.engine).params[(size_t) p * 4].name.upToFirstOccurrenceOf (" tune", false, false) : music::noteName (n);
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
    if (showHelp) { showHelp = false; return true; }
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
        m.addSeparator();
        m.addItem (4, "Full screen", onToggleFullScreen != nullptr);
        m.addItem (5, "Keys (F1)");
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
        {
            if (r == 1 && onSettings) onSettings();
            if (r == 2) box.startLearn();
            if (r == 3) box.forgetMidiMap();
            if (r == 4 && onToggleFullScreen) onToggleFullScreen();
            if (r == 5) showHelp = ! showHelp;
        });
        return;
    }
    const auto sr = screenRect().toFloat();
    if (sr.contains (lp))
    {
        const auto sp = lp - sr.getPosition();
        if (screenTouch (sp, true)) return;
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
    int k = -1;
    if (sr.contains (lp)) k = screenKnobAt (lp - sr.getPosition());
    else if (! kiosk) for (int i = 0; i < numKnobs; ++i) if (knobRect (i).contains (lp)) k = i;
    if (k >= 0) box.resetKnob (k);
    if (! kiosk && sliderRect().contains (lp)) box.setSlider (0.5f);
}

void View::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto lp = toLogical (e.position);
    const auto sr = screenRect().toFloat();
    int k = -1;
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
    if (code == juce::KeyPress::F1Key) { showHelp = ! showHelp; return true; }
    if (code == juce::KeyPress::spaceKey) { press (BtnPlay); return true; }
    if (code == juce::KeyPress::returnKey) { press (BtnRec); return true; }
    if (code == juce::KeyPress::tabKey) { box.selectTrack ((box.getTrack() + (mods.isShiftDown() ? numTracks - 1 : 1)) % numTracks); return true; }
    if (code == juce::KeyPress::leftKey) { press (BtnPrev); return true; }
    if (code == juce::KeyPress::rightKey) { press (BtnNext); return true; }
    if (code == juce::KeyPress::F2Key) { press (BtnSound); return true; }
    if (code == juce::KeyPress::F3Key) { press (BtnSeq); return true; }
    if (code == juce::KeyPress::F4Key) { press (BtnFx); return true; }
    if (code == juce::KeyPress::F5Key) { press (BtnMix); return true; }
    if (code == juce::KeyPress::F6Key) { press (BtnProject); return true; }
    if (code == juce::KeyPress::F11Key && onToggleFullScreen) { onToggleFullScreen(); return true; }
    if (code == juce::KeyPress::escapeKey) { showHelp = false; if (box.isLearning()) box.learnDone(); return true; }
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

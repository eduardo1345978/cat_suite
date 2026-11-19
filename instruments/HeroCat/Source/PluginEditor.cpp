#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr int designW = 1100, designH = 720;
// an old PC: a deep blue desktop, grey bevelled windows, a red frame, a green phosphor LCD
const Colour deskTop (0xff0d3060), deskBottom (0xff061633), winGrey (0xffc3c3c3), winLight (0xffffffff), winDark (0xff808080), winDarker (0xff3a3a3a),
             titleA (0xff0a1e7a), titleB (0xff3a8ae0), redFrame (0xffc8231d), redDark (0xff6e0f0b), pageBg (0xff17171c),
             lcdBg (0xff03170b), phosphor (0xff44ff88), phosDim (0xff1d7a42), yellow (0xffffd84a), cyan (0xff4fe3ff);

/** the Windows 98 bevel: light on the top-left, dark on the bottom-right (raised), or the other way (sunken) */
void bevel (Graphics& g, Rectangle<int> r, bool raised, Colour face = winGrey)
{
    g.setColour (face); g.fillRect (r);
    g.setColour (raised ? winLight : winDarker); g.fillRect (r.getX(), r.getY(), r.getWidth(), 1); g.fillRect (r.getX(), r.getY(), 1, r.getHeight());
    g.setColour (raised ? winDarker : winLight); g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1); g.fillRect (r.getRight() - 1, r.getY(), 1, r.getHeight());
    g.setColour (raised ? Colour (0xffdfdfdf) : winDark); g.fillRect (r.getX() + 1, r.getY() + 1, r.getWidth() - 2, 1); g.fillRect (r.getX() + 1, r.getY() + 1, 1, r.getHeight() - 2);
    g.setColour (raised ? winDark : Colour (0xffdfdfdf)); g.fillRect (r.getX() + 1, r.getBottom() - 2, r.getWidth() - 2, 1); g.fillRect (r.getRight() - 2, r.getY() + 1, 1, r.getHeight() - 2);
}

/** a window: grey frame, blue title bar with the three little buttons */
void window (Graphics& g, Rectangle<int> r, const String& title)
{
    bevel (g, r, true);
    auto bar = Rectangle<int> (r.getX() + 3, r.getY() + 3, r.getWidth() - 6, 20);
    g.setGradientFill (ColourGradient (titleA, (float) bar.getX(), 0.0f, titleB, (float) bar.getRight(), 0.0f, false));
    g.fillRect (bar);
    g.setColour (Colours::white);
    g.setFont (cat::catFont (12.0f, true));
    g.drawText (title, bar.withTrimmedLeft (6), Justification::centredLeft);
    for (int i = 0; i < 3; ++i)
    {
        auto b = Rectangle<int> (bar.getRight() - 18 - i * 18, bar.getY() + 2, 16, 14);
        bevel (g, b, true);
        g.setColour (Colours::black);
        if (i == 0) { g.drawLine ((float) b.getX() + 4, (float) b.getY() + 3, (float) b.getRight() - 4, (float) b.getBottom() - 3, 1.5f); g.drawLine ((float) b.getRight() - 4, (float) b.getY() + 3, (float) b.getX() + 4, (float) b.getBottom() - 3, 1.5f); }
        if (i == 1) g.drawRect (b.getX() + 4, b.getY() + 3, 8, 8, 1);
        if (i == 2) g.fillRect (b.getX() + 4, b.getBottom() - 5, 7, 2);
    }
}

void pixelMap (Graphics& g, const char* const* rows, int numRows, int x, int y, int px, std::function<Colour (char)> col)
{
    for (int r = 0; r < numRows; ++r)
        for (int c = 0; rows[r][c] != 0; ++c)
            if (rows[r][c] != '.') { g.setColour (col (rows[r][c])); g.fillRect (x + c * px, y + r * px, px, px); }
}

const char* const heroMap[] = {
    "............Y...",
    "...........Y....",
    "..........Y.....",
    ".X...X...Y......",
    ".XX.XX..Y.......",
    ".XXXXX.Y........",
    ".XOXOX#.........",
    ".XXXXXX.........",
    "..XXXX..........",
    ".XXXXXX.........",
    ".XXXXXX.....T...",
    ".XXXXXXX..TT....",
    ".XX..XXXXTT.....",
    ".XX..XX.........",
};
constexpr int horizon = 222, heroX = 60, heroPx = 4;
constexpr int heroY = horizon - 12 - 14 * heroPx;
}

//==============================================================================
void BoxArt::tick()
{
    ++frame;
    // QUEST: the world scrolls by, the hero jumps on every melody note and a coin pops out
    const bool questOn = processor.apvts.getRawParameterValue ("quest")->load() > 0.5f;
    const int pos = processor.engine.logPos.load();
    if (questOn)
    {
        scroll += 2.0f;
        bool newNote = false;
        for (int k = jmax (seenLog, pos - 8); k < pos; ++k)
        {
            const int n = processor.engine.noteLog[(size_t) (k & 63)].load() & 0xff;
            if (n >= 60) newNote = true;               // a melody note (the chords are lower)
        }
        if (newNote && jump < 4.0f) { jumpV = 4.2f; coins.push_back ({ (float) (heroX + 22), (float) heroY - 10.0f, 0.0f, -1.6f, 1.0f }); }
    }
    seenLog = pos;
    jump += jumpV; jumpV -= 0.7f;
    if (jump <= 0.0f) { jump = 0.0f; jumpV = 0.0f; }
    for (auto& c : coins) { c.y += c.vy; c.life -= 0.035f; }
    coins.erase (std::remove_if (coins.begin(), coins.end(), [] (const Spark& c) { return c.life <= 0.0f; }), coins.end());
    flash *= 0.85f;
    for (auto& s : sparks) { s.x += s.vx; s.y += s.vy; s.vy += 0.25f; s.life -= 0.025f; }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const Spark& s) { return s.life <= 0.0f; }), sparks.end());
    repaint();
}

void BoxArt::forge()
{
    flash = 1.0f;
    const float tipX = (float) (heroX + 12 * heroPx + 2), tipY = (float) heroY + 2.0f - jump;     // the sword's tip
    for (int i = 0; i < 70; ++i)
    {
        const float a = rng.nextFloat() * MathConstants<float>::twoPi, sp = 1.5f + rng.nextFloat() * 6.0f;
        sparks.push_back ({ tipX, tipY, std::cos (a) * sp, std::sin (a) * sp - 2.5f, 0.6f + rng.nextFloat() * 0.6f });
    }
}

void BoxArt::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    const int W = b.getWidth(), H = b.getHeight(), px = 4;
    // the sky: hard colour bands (no smooth gradient - it is pixel art)
    const Colour sky[] = { Colour (0xff12062e), Colour (0xff2b1055), Colour (0xff53156b), Colour (0xff7a1f6b), Colour (0xffb02e6a), Colour (0xffd9446b), Colour (0xffff7046), Colour (0xffffa94d) };
    for (int i = 0; i < 8; ++i) { g.setColour (sky[i]); g.fillRect (0, i * horizon / 8, W, horizon / 8 + 1); }
    // stars that blink
    Random r (5);
    for (int i = 0; i < 40; ++i)
    {
        const int x = r.nextInt (W / 2) * 2, y = r.nextInt (90) + 4;
        const float a = 0.4f + 0.4f * r.nextFloat();
        if (((frame / 6) + i * 7) % 23 < 18) { g.setColour (Colours::white.withAlpha (a)); g.fillRect (x, y, 2, 2); }
    }
    // the sun with stripes cut out
    const int sx = 190, sy = 168, sr = 50;
    for (int y = -sr; y <= sr; y += px)
    {
        if (y > 0 && ((y / px) % 3 == 0)) continue;
        const int half = (int) std::sqrt ((float) (sr * sr - y * y)) / px * px;
        g.setColour (y < -20 ? Colour (0xffffe066) : y < 10 ? Colour (0xffffb347) : Colour (0xffff7a59));
        g.fillRect (sx - half, sy + y, half * 2, px);
    }
    // the mountains and the castle, as dark silhouettes
    g.setColour (Colour (0xff2a0f3d));
    for (int x = 0; x < W; x += px)
    {
        const float wx = (float) x + scroll * 0.3f;
        const int h = 30 + (int) (22.0f * std::sin (wx * 0.021f) + 14.0f * std::sin (wx * 0.057f + 1.0f));
        g.fillRect (x, horizon - h, px, h);
    }
    g.setColour (Colour (0xff1a0828));
    const int cx = (int) std::fmod (298.0f - scroll * 0.6f + 10000.0f * (float) (W + 140), (float) (W + 140)) - 70;   // (it scrolls by in QUEST)
    g.fillRect (cx, horizon - 64, 52, 64);
    for (int t : { -6, 22, 46 }) { g.fillRect (cx + t, horizon - 92, 14, 92); for (int k = 0; k < 3; ++k) g.fillRect (cx + t + k * 5, horizon - 98, 3, 6); }
    g.setColour (yellow.withAlpha (0.8f));
    g.fillRect (cx + 26, horizon - 50, 4, 6); g.fillRect (cx + 2, horizon - 78, 4, 6);
    // the ground: a grid running to the horizon
    g.setColour (Colour (0xff12052a)); g.fillRect (0, horizon, W, H - horizon);
    g.setColour (cyan.withAlpha (0.45f));
    for (int i = 0; i < 6; ++i) { const int y = horizon + (int) std::pow (2.0f, (float) i + 1.4f) + ((frame / 3) % 4); if (y < H) g.fillRect (0, y, W, 1); }
    const float slide = std::fmod (scroll * 1.5f, 90.0f);
    for (int i = -9; i <= 9; ++i) g.drawLine ((float) W * 0.5f + (float) i * 14.0f - slide * 14.0f / 90.0f, (float) horizon, (float) W * 0.5f + (float) i * 90.0f - slide, (float) H, 1.0f);
    // the rock and the hero on it, sword up
    g.setColour (Colour (0xff1a0828)); g.fillRect (44, horizon - 6, 90, 10); g.fillRect (52, horizon - 12, 70, 8);
    pixelMap (g, heroMap, 14, heroX, heroY - (int) jump / 2 * 2, heroPx, [this] (char c)
    {
        if (c == 'O') return yellow;
        if (c == 'Y') return Colour (0xffbff6ff).interpolatedWith (Colours::white, flash);
        if (c == '#') return Colour (0xffffb000);
        return Colour (0xff14062a);
    });
    // the title
    g.setFont (cat::lcdFont (54.0f));
    const auto title = Rectangle<int> (0, 10, W, 52);
    g.setColour (Colour (0xff2b0a3d));
    for (auto d : { Point<int> (-3, 0), Point<int> (3, 0), Point<int> (0, -3), Point<int> (0, 4), Point<int> (3, 4) }) g.drawText ("HERO-CAT", title.translated (d.x, d.y), Justification::centred);
    g.setGradientFill (ColourGradient (Colour (0xfffff3a0), 0.0f, 18.0f, Colour (0xffff9a3d), 0.0f, 56.0f, false));
    g.drawText ("HERO-CAT", title, Justification::centred);
    g.setFont (cat::lcdFont (20.0f));
    g.setColour (Colour (0xffffe0f0));
    g.drawText ("~ THE LEAD QUEST ~", 0, 58, W, 20, Justification::centred);
    // the publisher's badge in the corner
    g.setColour (Colour (0xffff3d3d)); g.fillRect (W - 74, H - 26, 66, 18);
    g.setColour (Colours::white); g.setFont (cat::lcdFont (16.0f)); g.drawText ("CAT SUITE", W - 74, H - 26, 66, 18, Justification::centred);

    // the coins (QUEST)
    for (auto& c : coins)
    {
        const int x = (int) c.x, y = (int) c.y;
        g.setColour (Colour (0xffffd84a).withAlpha (jmin (1.0f, c.life * 1.5f))); g.fillRect (x, y, 8, 10);
        g.setColour (Colour (0xffb07a00).withAlpha (jmin (1.0f, c.life * 1.5f))); g.fillRect (x + 3, y + 2, 2, 6);
    }
    if (processor.apvts.getRawParameterValue ("quest")->load() > 0.5f)
    {
        const int st = processor.engine.questStep.load();
        g.setFont (cat::lcdFont (20.0f));
        g.setColour (Colours::white);
        g.drawText ("STAGE 1-" + String (st < 0 ? 1 : st / 8 + 1), 8, H - 26, 120, 20, Justification::centredLeft);
    }

    // sparks and the flash of the CRAFT
    for (auto& s : sparks)
    {
        g.setColour ((s.life > 0.7f ? Colours::white : s.life > 0.4f ? yellow : Colour (0xffff7a2e)).withAlpha (jmin (1.0f, s.life)));
        g.fillRect ((int) s.x / 2 * 2, (int) s.y / 2 * 2, 3, 3);
    }
    if (flash > 0.02f) { g.setColour (Colours::white.withAlpha (flash * 0.5f)); g.fillRect (b); }
}

//==============================================================================
void CraftButton::paintButton (Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f, 2.0f).withTrimmedBottom (4.0f);
    if (down) b.translate (0.0f, 3.0f);
    // the shadow, the dark rim, the green body, the shine
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (b.translated (0.0f, down ? 1.0f : 4.0f), 14.0f);
    g.setColour (Colour (0xff0b4d12));
    g.fillRoundedRectangle (b, 14.0f);
    const auto body = b.reduced (3.0f);
    const Colour top = over ? Colour (0xff9dff7a) : Colour (0xff7df060), bottom = down ? Colour (0xff137a1d) : Colour (0xff1f9a2a);
    g.setGradientFill (ColourGradient (top, 0.0f, body.getY(), bottom, 0.0f, body.getBottom(), false));
    g.fillRoundedRectangle (body, 11.0f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.6f), 0.0f, body.getY(), Colours::white.withAlpha (0.05f), 0.0f, body.getCentreY(), false));
    g.fillRoundedRectangle (body.reduced (8.0f, 3.0f).withHeight (body.getHeight() * 0.45f), 9.0f);
    g.setFont (cat::lcdFont (56.0f));
    g.setColour (Colour (0xff0b4d12)); g.drawText ("CRAFT", body.translated (0.0f, 3.0f).toNearestInt(), Justification::centred);
    g.setColour (Colours::white); g.drawText ("CRAFT", body.toNearestInt(), Justification::centred);
}

//==============================================================================
void DesktopIcon::paintButton (Graphics& g, bool over, bool)
{
    const auto b = getLocalBounds();
    const int px = 3, ix = b.getCentreX() - 16 * px / 2, iy = 4;
    static const char* const wave[] = {
        "XXXXXXXXXXXXXXXX", "XWWWWWWWWWWWWWWX", "XWWWWWWWWWWWWWWX", "XWWWGWWWWWWWWWWX", "XWWGWGWWWWWWWWWX", "XWGWWWGWWWWWWWWX",
        "XWWWWWWGWWWWWGWX", "XWWWWWWWGWWWGWWX", "XWWWWWWWWGWGWWWX", "XWWWWWWWWWGWWWWX", "XWWWWWWWWWWWWWWX", "XXXXXXXXXXXXXXXX", "....XXXXXXXX....", "..XXXXXXXXXXXX.." };
    static const char* const notes[] = {
        ".....XXXXXXXXXXX", ".....XXXXXXXXXXX", ".....X.........X", ".....X.........X", ".....X.........X", ".....X.........X",
        ".....X.........X", "..XXXX.......XXX", ".XXXXX......XXXX", ".XXXXX......XXXX", "..XXX........XX.", "................", "................", "................" };
    static const char* const cloud[] = {
        "................", ".....WWWW.......", "...WWWWWWWW.....", "..WWWWWWWWWWWW..", ".WWWWWWWWWWWWWW.", ".WWWWWWWWWWWWWW.",
        "..WWWWWWWWWWWW..", "................", "..C...C...C...C.", ".C...C...C...C..", "................", "...C...C...C....", "..C...C...C.....", "................" };
    static const char* const knob[] = {
        ".....XXXXXX.....", "...XXGGGGGGXX...", "..XGGGGGGGGGGX..", ".XGGGGGWGGGGGGX.", ".XGGGGGWGGGGGGX.", "XGGGGGGWGGGGGGGX",
        "XGGGGGGWGGGGGGGX", "XGGGGGGGGGGGGGGX", "XGGGGGGGGGGGGGGX", ".XGGGGGGGGGGGGX.", ".XGGGGGGGGGGGGX.", "..XGGGGGGGGGGX..", "...XXGGGGGGXX...", ".....XXXXXX....." };
    const char* const* maps[] = { wave, notes, cloud, knob };
    const int k = kind;
    pixelMap (g, maps[k], 14, ix, iy, px, [k] (char c)
    {
        switch (c)
        {
            case 'W': return k == 2 ? Colour (0xffe8e8f0) : Colour (0xff0b2a17);
            case 'G': return k == 3 ? Colour (0xff9a9aa4) : Colour (0xff44ff88);
            case 'C': return Colour (0xff4fe3ff);
            default:  return k == 3 ? Colour (0xff3a3a44) : Colour (0xffe8e8e8);
        }
    });
    auto label = Rectangle<int> (2, iy + 14 * px + 4, b.getWidth() - 4, 16);
    if (selected || over)
    {
        g.setColour (Colour (0xff0a2a9a).withAlpha (selected ? 1.0f : 0.5f));
        g.fillRect (label);
        if (selected) { g.setColour (Colours::white.withAlpha (0.7f)); g.drawRect (label, 1); }
    }
    g.setColour (Colours::white);
    g.setFont (cat::catFont (11.0f, true));
    g.drawText (getName(), label, Justification::centred);
}

//==============================================================================
void RetroButton::paintButton (Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds();
    if (tab)
    {
        auto r = selected ? b : b.withTrimmedTop (3);
        bevel (g, r.withHeight (r.getHeight() + 2), true, selected ? winGrey : Colour (0xffa9a9a9));
        if (selected) { g.setColour (winGrey); g.fillRect (r.getX() + 2, r.getBottom() - 1, r.getWidth() - 4, 3); }
    }
    else bevel (g, b, ! (down || getToggleState()), getToggleState() ? Colour (0xffb8e8c0) : over ? Colour (0xffd2d2d2) : winGrey);
    g.setColour (isEnabled() ? Colours::black : winDark);
    g.setFont (cat::catFont (12.0f, true));
    const bool in = (down || getToggleState()) && ! tab;
    g.drawText (getButtonText().isNotEmpty() ? getButtonText() : getName(), b.translated (in ? 1 : 0, in ? 1 : 0), Justification::centred);
}

//==============================================================================
void LcdScreen::tick()
{
    ++frame;
    if (messageFrames > 0) --messageFrames;
    auto& e = processor.engine;
    const int pos = e.scopePos.load();
    // a steady picture: start at a rising zero crossing
    int start = pos - 1024;
    for (int i = 0; i < 600; ++i)
    {
        const float a = e.scope[(size_t) ((start + i) & 2047)].load (std::memory_order_relaxed);
        const float b = e.scope[(size_t) ((start + i + 1) & 2047)].load (std::memory_order_relaxed);
        if (a <= 0.0f && b > 0.0f) { start += i; break; }
    }
    for (size_t i = 0; i < wave.size(); ++i)
        wave[i] = e.scope[(size_t) ((start + (int) i) & 2047)].load (std::memory_order_relaxed);
    meter += (jmin (1.0f, e.level.load() * 1.4f) - meter) * 0.3f;
    repaint();
}

void LcdScreen::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    g.setColour (lcdBg); g.fillRect (b);
    // the LCD's dot grid
    g.setColour (phosDim.withAlpha (0.18f));
    for (int y = 4; y < b.getHeight(); y += 8) for (int x = 4; x < b.getWidth(); x += 8) g.fillRect (x, y, 1, 1);

    // the oscilloscope (the real sound)
    const auto sc = Rectangle<float> (12.0f, 30.0f, (float) b.getWidth() * 0.58f, (float) b.getHeight() - 58.0f);
    g.setColour (phosDim.withAlpha (0.5f)); g.drawHorizontalLine ((int) sc.getCentreY(), sc.getX(), sc.getRight());
    Path p;
    for (size_t i = 0; i < wave.size(); ++i)
    {
        const float x = sc.getX() + sc.getWidth() * (float) i / (float) (wave.size() - 1);
        const float y = sc.getCentreY() - jlimit (-1.0f, 1.0f, wave[i] * 1.6f) * sc.getHeight() * 0.48f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (phosphor.withAlpha (0.22f)); g.strokePath (p, PathStrokeType (5.0f));
    g.setColour (phosphor); g.strokePath (p, PathStrokeType (1.6f));

    // the sound's name and the chord
    String name = processor.craftName;
    if (name.isEmpty())
    {
        const int pr = processor.currentPreset.load();
        name = isPositiveAndBelow (pr, (int) HeroCatProcessor::presets().size()) ? String (HeroCatProcessor::presets()[(size_t) pr].name) : String ("Custom");
    }
    g.setFont (cat::lcdFont (22.0f));
    g.setColour (phosphor);
    if (processor.apvts.getRawParameterValue ("quest")->load() > 0.5f)
    {
        const int st = processor.engine.questStep.load();
        name = "QUEST  BAR " + String (st < 0 ? 1 : st / 8 + 1) + "/8  " + name;
    }
    g.drawText ("> " + name.toUpperCase() + ((frame / 15) % 2 == 0 ? "_" : " "), 12, 4, (int) sc.getWidth() + 60, 24, Justification::centredLeft);
    const auto chord = processor.chordName();
    const auto right = Rectangle<int> ((int) sc.getRight() + 10, 18, b.getWidth() - (int) sc.getRight() - 20, b.getHeight() - 44);
    g.setFont (cat::lcdFont (chord.isEmpty() ? 26.0f : 64.0f));
    g.setColour (chord.isEmpty() ? phosDim : phosphor);
    g.drawText (chord.isEmpty() ? "PRESS A KEY" : chord, right, Justification::centred);

    // what is on, and the level
    auto& ap = processor.apvts;
    auto choice = [&ap] (const char* id, const StringArray& names) { return names[roundToInt (ap.getRawParameterValue (id)->load())].toUpperCase(); };
    g.setFont (cat::lcdFont (19.0f));
    g.setColour (phosphor.withAlpha (0.8f));
    g.drawText ("CHORD " + choice ("chordMode", hero::chordModeNames()) + "   ARP " + choice ("arpMode", hero::arpModeNames())
                    + "   TEX " + choice ("texture", hero::textureNames()), 12, b.getHeight() - 24, b.getWidth() - 150, 20, Justification::centredLeft);
    for (int i = 0; i < 12; ++i)
    {
        const bool on = (float) i / 12.0f < meter;
        g.setColour (on ? (i > 9 ? Colour (0xffff5a4a) : i > 7 ? yellow : phosphor) : phosDim.withAlpha (0.35f));
        g.fillRect (b.getWidth() - 130 + i * 10, b.getHeight() - 19, 8, 10);
    }
    if (messageFrames > 0 && (messageFrames / 4) % 2 == 0)
    {
        g.setFont (cat::lcdFont (30.0f));
        const auto m = Rectangle<int> (0, 0, GlyphArrangement::getStringWidthInt (g.getCurrentFont(), message) + 30, 38).withCentre (b.getCentre());
        g.setColour (lcdBg); g.fillRect (m);
        g.setColour (phosphor); g.drawRect (m, 2);
        g.drawText (message, m, Justification::centred);
    }
    // scanlines
    g.setColour (Colours::black.withAlpha (0.25f));
    for (int y = 0; y < b.getHeight(); y += 3) g.fillRect (0, y, b.getWidth(), 1);
}

//==============================================================================
PatternView::PatternView (HeroCatProcessor& p) : processor (p) {}

void PatternView::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    g.setColour (lcdBg); g.fillRect (b);
    const int am = roundToInt (processor.apvts.getRawParameterValue ("arpMode")->load());
    const bool active = am == hero::ArpPattern;
    const int now = active ? processor.engine.arpStep.load() : -1;
    const float cw = (float) b.getWidth() / 16.0f;
    const int rows = 6, rh = (b.getHeight() - 14) / rows;
    for (int i = 0; i < 16; ++i)
    {
        const float v = processor.engine.pattern[(size_t) i].load();
        const int x = (int) ((float) i * cw) + 3, w = (int) cw - 6;
        const int lit = roundToInt (v * rows);
        for (int k = 0; k < rows; ++k)
        {
            const int y = b.getBottom() - 8 - (k + 1) * rh;
            g.setColour (k < lit ? (i == now ? Colours::white : (active ? phosphor : phosDim)) : phosDim.withAlpha (0.18f));
            g.fillRect (x, y, w, rh - 3);
        }
        g.setColour (phosDim.withAlpha (i % 4 == 0 ? 0.9f : 0.35f));
        g.fillRect (x, b.getBottom() - 5, w, 2);
    }
    if (! active)
    {
        g.setFont (cat::lcdFont (18.0f));
        const auto msg = b.withSizeKeepingCentre (290, 22);
        g.setColour (lcdBg); g.fillRect (msg);
        g.setColour (phosphor.withAlpha (0.8f));
        g.drawText ("ARP = PATTERN plays these steps", msg, Justification::centred);
    }
    g.setColour (Colours::black.withAlpha (0.25f));
    for (int y = 0; y < b.getHeight(); y += 3) g.fillRect (0, y, b.getWidth(), 1);
}

void PatternView::setFrom (Point<float> p)
{
    const int i = jlimit (0, 15, (int) (p.x / ((float) getWidth() / 16.0f)));
    const float v = jlimit (0.0f, 1.0f, 1.0f - (p.y - 6.0f) / (float) (getHeight() - 14));
    processor.engine.pattern[(size_t) i] = std::round (v * 6.0f) / 6.0f;
    repaint();
}
void PatternView::mouseDown (const MouseEvent& e) { setFrom (e.position); }
void PatternView::mouseDrag (const MouseEvent& e) { setFrom (e.position); }
void PatternView::mouseDoubleClick (const MouseEvent& e)
{
    const int i = jlimit (0, 15, (int) (e.position.x / ((float) getWidth() / 16.0f)));
    auto& x = processor.engine.pattern[(size_t) i];
    x = x.load() > 0.0f ? 0.0f : 1.0f;
    repaint();
}

//==============================================================================
HeroCatEditor::HeroCatEditor (HeroCatProcessor& p) : processor (p)
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (art);
    addAndMakeVisible (craftButton);
    craftButton.setTooltip ("CRAFT: a whole new sound - sounds, chords, arp, texture and effects picked for you (UNDO goes back)");
    craftButton.onClick = [this] { doCraft(); };
    addAndMakeVisible (undo);
    undo.setTooltip ("UNDO: back to the sound before the last CRAFT");
    undo.onClick = [this] { if (processor.undoCraft()) { lcd.message = "UNDONE"; lcd.messageFrames = 40; } };

    quest.setClickingTogglesState (true);
    quest.setTooltip ("QUEST: Hero-Cat plays a whole game theme by itself (8 bars: chords + a melody from a motif), with this sound, in the key and scale of the CHORD tab. Play along!");
    questAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "quest", quest);
    newSong.setTooltip ("NEW SONG: QUEST makes up another theme");
    newSong.onClick = [this] { processor.newSong (rng); lcd.message = "NEW SONG!"; lcd.messageFrames = 40; };
    for (auto* b : { &quest, &newSong }) addAndMakeVisible (*b);

    const char* tabNames[] = { "SOUND", "CHORD + ARP", "TEXTURE", "FX" };
    const char* iconNames[] = { "SOUND", "CHORDS", "TEXTURE", "FX" };
    for (int i = 0; i < 4; ++i)
    {
        icons[(size_t) i] = std::make_unique<DesktopIcon> (iconNames[i], i);
        icons[(size_t) i]->onClick = [this, i] { showTab (i); };
        addAndMakeVisible (*icons[(size_t) i]);
        tabs[(size_t) i] = std::make_unique<RetroButton> (tabNames[i], true);
        tabs[(size_t) i]->onClick = [this, i] { showTab (i); };
        addAndMakeVisible (*tabs[(size_t) i]);
    }
    addAndMakeVisible (lcd);

    dice.setTooltip ("DICE: a new random pattern");
    clear.setTooltip ("CLEAR: every step at full loudness");
    dice.onClick = [this] { processor.engine.randomisePattern (rng); pattern.repaint(); };
    clear.onClick = [this] { for (auto& x : processor.engine.pattern) x = 1.0f; pattern.repaint(); };

    struct C { cat::ParamCombo* c; const char* tip; };
    const C cs[] = {
        { &srcA, "Sound A: synth waves (saw, square, supersaw, chip) or small physical models (guitar, strings, flute, choir, bell)" },
        { &srcB, "Sound B: a second sound under every note (A / B mixes them)" },
        { &octB, "Sound B an octave lower or higher" },
        { &chordMode, "CHORD LOCK: one key becomes a chord - from the scale, or always the same shape" },
        { &key, "The song's key (for the scale chords and the chimes)" },
        { &scale, "The song's scale: keys outside it snap into it" },
        { &arpMode, "ARP: plays the held chord as a melody. WATERFALL: falls from the top and keeps ringing. PATTERN: your drawn steps" },
        { &rate, "How fast the arp goes (with the DAW's tempo)" },
        { &octaves, "How many octaves the arp climbs" },
        { &texture, "A sound bed under everything: rain, birds, a quiet room, chimes on your chord, wind" },
        { &lfoShape, "The LFO's shape" },
        { &lfoTarget, "What the LFO moves" },
    };
    for (auto& c : cs) c.c->getCombo().setTooltip (c.tip);

    struct S { cat::LineSlider* s; const char* tip; };
    const S ss[] = {
        { &mix, "A / B: how much of sound B" }, { &detune, "DETUNE: the saws spread apart (fatter)" },
        { &cutoff, "CUTOFF: darker or brighter" }, { &reso, "RESO: a whistle at the cutoff" },
        { &envAmt, "F.ENV: the filter opens with every note" }, { &vibrato, "VIBRATO: the singer's wobble that comes in after a moment" },
        { &attack, "ATTACK: how slowly a note starts" }, { &decay, "DECAY: how fast it falls to the sustain" },
        { &sustain, "SUSTAIN: the level while you hold" }, { &release, "RELEASE: how long it rings after" },
        { &glide, "GLIDE: the notes slide into each other" },
        { &strum, "STRUM: the chord's notes one after another, like a guitar" },
        { &gate, "GATE: how long each arp note is" }, { &swing, "SWING: every second step a little late" },
        { &chaos, "CHAOS: the arp improvises - rests, octave jumps, double hits, other notes" },
        { &texLevel, "TEXTURE LEVEL: how loud the sound bed is" }, { &lfoRate, "LFO RATE: how fast it moves" },
        { &lfoDepth, "LFO DEPTH: how much it moves" }, { &drive, "DRIVE: warm distortion" },
        { &crush, "CRUSH: fewer bits, lower sample rate - an old console" }, { &chorus, "CHORUS: wide and shiny" },
        { &delay, "DELAY: echoes (dotted 1/8, left-right)" }, { &reverb, "REVERB: the hall" },
        { &width, "WIDTH: the stereo width" }, { &output, "OUTPUT: the level that comes out" },
    };
    for (auto& s : ss) { s.s->setColours (phosDim, phosphor); s.s->getSlider().setTooltip (s.tip); }
    voiceLead.setTooltip ("VOICE LEAD: each chord note moves to the nearest note of the last chord (smooth changes)");
    latch.setTooltip ("LATCH: the arp keeps playing after you let go - press new keys for a new chord");
    midiOut.setTooltip ("MIDI OUT: the chords and arp notes also go to the DAW, so another instrument can play them");

    pages[0] = { &srcA, &srcB, &octB, &mix, &detune, &cutoff, &reso, &envAmt, &vibrato, &glide, &attack, &decay, &sustain, &release };
    pages[1] = { &chordMode, &key, &scale, &voiceLead, &arpMode, &rate, &octaves, &latch, &midiOut, &strum, &gate, &swing, &chaos, &pattern, &dice, &clear };
    pages[2] = { &texture, &texLevel, &lfoShape, &lfoTarget, &lfoRate, &lfoDepth };
    pages[3] = { &drive, &crush, &chorus, &delay, &reverb, &width, &output };
    for (auto& pg : pages) for (auto* c : pg) addChildComponent (*c);

    keys.setLowestVisibleKey (36);
    keys.setColour (MidiKeyboardComponent::keyDownOverlayColourId, phosphor.withAlpha (0.8f));
    keys.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, cyan.withAlpha (0.3f));
    addAndMakeVisible (keys);
    showTab (0);
    setSize (designW, designH);
    startTimerHz (30);
}

HeroCatEditor::~HeroCatEditor() { stopTimer(); }

void HeroCatEditor::showTab (int t)
{
    tab = t;
    for (int i = 0; i < 4; ++i)
    {
        for (auto* c : pages[(size_t) i]) c->setVisible (i == t);
        tabs[(size_t) i]->selected = i == t; tabs[(size_t) i]->repaint();
        icons[(size_t) i]->selected = i == t; icons[(size_t) i]->repaint();
    }
    repaint();
}

void HeroCatEditor::doCraft()
{
    processor.craft (rng);
    art.forge();
    lcd.message = "CRAFTED!";
    lcd.messageFrames = 45;
    pattern.repaint();
}

void HeroCatEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    // the desktop
    g.setGradientFill (ColourGradient (deskTop, 0.0f, 0.0f, deskBottom, 0.0f, (float) b.getHeight(), false));
    g.fillAll();
    g.setColour (Colours::white.withAlpha (0.035f));
    for (int y = 0; y < b.getHeight(); y += 4) for (int x = (y / 4) % 2 * 2; x < b.getWidth(); x += 4) g.fillRect (x, y, 1, 1);
    // the program's title bar across the top
    g.setGradientFill (ColourGradient (titleA, 0.0f, 0.0f, titleB, (float) b.getWidth(), 0.0f, false));
    g.fillRect (0, 0, b.getWidth(), 26);
    g.setColour (Colours::white);
    g.setFont (cat::catFont (13.0f, true));
    g.drawText ("HERO-CAT.EXE  -  Cat Suite", 10, 0, 400, 26, Justification::centredLeft);

    // the box art's frame, and the main window with its red frame
    bevel (g, { 88, 36, 308, 308 }, false);
    window (g, { 400, 66, 694, 412 }, "Hero-Cat  -  " + String (tab == 0 ? "Sound" : tab == 1 ? "Chord + Arp" : tab == 2 ? "Texture + Mod" : "Effects"));
    g.setColour (redDark); g.fillRect (404, 92, 686, 382);
    g.setColour (redFrame); g.fillRect (406, 94, 682, 378);
    g.setColour (pageBg); g.fillRect (412, 126, 670, 340);
    // the LCD's sunken frame
    bevel (g, { 414, 132, 666, 128 }, false);

    // the keyboard's window
    window (g, { 8, 486, 1086, 228 }, "KEYS.EXE");

    if (tab == 2)
    {
        g.setColour (phosphor.withAlpha (0.55f));
        g.setFont (cat::lcdFont (18.0f));
        g.drawText ("TEXTURE plays even when you don't. CHIMES rings on your chord's notes.", 424, 436, 650, 20, Justification::centredLeft);
    }
}

void HeroCatEditor::resized()
{
    presetBar.setBounds (684, 32, 410, 30);
    art.setBounds (90, 38, 304, 304);
    craftButton.setBounds (88, 352, 308, 86);
    undo.setBounds (88, 446, 96, 28);
    quest.setBounds (190, 446, 96, 28);
    newSong.setBounds (292, 446, 104, 28);
    for (int i = 0; i < 4; ++i)
    {
        icons[(size_t) i]->setBounds (6, 40 + i * 96, 78, 72);
        tabs[(size_t) i]->setBounds (412 + i * 124, 100, 120, 26);
    }
    lcd.setBounds (416, 134, 662, 124);

    const int x0 = 424, y0 = 272, cw = 152, gap = 12;
    auto col = [&] (int i) { return x0 + i * (cw + gap); };
    // SOUND
    srcA.setBounds (x0, y0, 250, 28); srcB.setBounds (x0 + 262, y0, 230, 28); octB.setBounds (x0 + 504, y0, 142, 28);
    cat::LineSlider* rowS[3][4] = { { &mix, &detune, &cutoff, &reso }, { &envAmt, &vibrato, &glide, nullptr }, { &attack, &decay, &sustain, &release } };
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 4; ++c) if (rowS[r][c] != nullptr) rowS[r][c]->setBounds (col (c), y0 + 46 + r * 50, cw, 40);
    // CHORD + ARP
    chordMode.setBounds (x0, y0, 236, 28); key.setBounds (x0 + 244, y0, 96, 28); scale.setBounds (x0 + 348, y0, 170, 28); voiceLead.setBounds (x0 + 528, y0 + 1, 118, 26);
    arpMode.setBounds (x0, y0 + 34, 236, 28); rate.setBounds (x0 + 244, y0 + 34, 110, 28); octaves.setBounds (x0 + 362, y0 + 34, 92, 28);
    latch.setBounds (x0 + 462, y0 + 35, 84, 26); midiOut.setBounds (x0 + 554, y0 + 35, 92, 26);
    cat::LineSlider* rowA[4] = { &strum, &gate, &swing, &chaos };
    for (int c = 0; c < 4; ++c) rowA[c]->setBounds (col (c), y0 + 68, cw, 40);
    pattern.setBounds (x0, y0 + 114, 560, 76);
    dice.setBounds (x0 + 570, y0 + 114, 76, 36);
    clear.setBounds (x0 + 570, y0 + 154, 76, 36);
    // TEXTURE + MOD
    texture.setBounds (x0, y0, 300, 28); texLevel.setBounds (x0 + 320, y0 - 6, 300, 40);
    lfoShape.setBounds (x0, y0 + 60, 300, 28); lfoTarget.setBounds (x0 + 320, y0 + 60, 300, 28);
    lfoRate.setBounds (x0, y0 + 104, 300, 40); lfoDepth.setBounds (x0 + 320, y0 + 104, 300, 40);
    // FX
    cat::LineSlider* rowF[2][4] = { { &drive, &crush, &chorus, &delay }, { &reverb, &width, &output, nullptr } };
    for (int r = 0; r < 2; ++r) for (int c = 0; c < 4; ++c) if (rowF[r][c] != nullptr) rowF[r][c]->setBounds (col (c), y0 + 10 + r * 70, cw, 40);

    keys.setBounds (14, 514, 1074, 194);
}

void HeroCatEditor::timerCallback()
{
    art.tick();
    lcd.tick();
    if (tab == 1) pattern.repaint();
    undo.setEnabled (processor.canUndo());
}

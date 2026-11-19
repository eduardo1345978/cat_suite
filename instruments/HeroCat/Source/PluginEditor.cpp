#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr int designW = 1100, designH = 700;
// a 90s game menu: deep indigo night, hot pink, cyan, sunny yellow, mint
const Colour bgTop (0xff1a1338), bgBottom (0xff0b0920), panelC (0xff201a46), panelEdge (0xff4b3d96), ink (0xfff3eeff), dimC (0xff9a90c8),
             pink (0xffff5fa2), cyan (0xff4fe3ff), yellow (0xffffd84a), mint (0xff7dffb0), screenC (0xff07061a);

/** A panel with "pixel" corners: the corner squares are cut off, the edge is 2 px with a darker shadow inside. */
void pixelPanel (Graphics& g, Rectangle<int> r, Colour fill, Colour edge, const String& title = {}, Colour titleC = yellow)
{
    const int c = 4;
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRect (r.translated (4, 4).reduced (c, 0)); g.fillRect (r.translated (4, 4).reduced (0, c));
    g.setColour (edge);
    g.fillRect (r.reduced (c, 0)); g.fillRect (r.reduced (0, c));
    g.setColour (fill);
    g.fillRect (r.reduced (c + 2, 2)); g.fillRect (r.reduced (2, c + 2));
    g.fillRect (r.reduced (4));
    if (title.isNotEmpty())
    {
        g.setFont (cat::lcdFont (22.0f));
        const int w = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), title) + 16;
        g.setColour (edge);
        g.fillRect (r.getX() + 14, r.getY() - 10, w, 20);
        g.setColour (titleC);
        g.drawText (title, r.getX() + 14, r.getY() - 11, w, 20, Justification::centred);
    }
}

/** A small pixel-art cat hero (12 x 10), drawn from a map */
void pixelCat (Graphics& g, int x, int y, int px, Colour body, Colour eye)
{
    static const char* map[] = {
        "X.........X.",
        "XX.......XX.",
        "XXXXXXXXXXX.",
        "XXXXXXXXXXX.",
        "XXOXXXXXOXX.",
        "XXXXXXXXXXX.",
        "XXXXX.XXXXX.",
        ".XXXXXXXXX..",
        "..PPPPPPP...",
        "..P.P.P.P...",
    };
    for (int r = 0; r < 10; ++r)
        for (int c = 0; c < 12; ++c)
        {
            const char ch = map[r][c];
            if (ch == '.') continue;
            g.setColour (ch == 'O' ? eye : ch == 'P' ? pink : body);
            g.fillRect (x + c * px, y + r * px, px, px);
        }
}
}

//==============================================================================
ScreenView::ScreenView (HeroCatProcessor& p) : processor (p)
{
    Random r (11);
    for (int i = 0; i < 60; ++i) stars.push_back ({ r.nextFloat(), r.nextFloat(), r.nextInt (40) });
    seenLog = processor.engine.logPos.load();
}

void ScreenView::tick()
{
    ++frame;
    const auto& e = processor.engine;
    const int pos = e.logPos.load();
    if (pos - seenLog > 64) seenLog = pos - 64;
    static const Colour cols[] = { pink, cyan, yellow, mint };
    for (; seenLog < pos; ++seenLog)
    {
        const int v = e.noteLog[(size_t) (seenLog & 63)].load();
        const int note = v & 0xff, vel = v >> 8;
        const float x = jlimit (0.04f, 0.96f, (float) (note - 36) / 60.0f) * (float) getWidth();
        const int n = 3 + vel / 30;
        for (int k = 0; k < n; ++k)
            pixels.push_back ({ x + (float) ((k % 3) - 1) * 8.0f, 50.0f - (float) k * 6.0f, 1.2f + (float) vel / 50.0f + (float) k * 0.15f, 1.0f,
                                cols[note % 4], k == 0 ? 8 : 4 });
    }
    for (auto& p : pixels) { p.y += p.vy; p.vy += 0.06f; p.life -= 0.012f; }
    pixels.erase (std::remove_if (pixels.begin(), pixels.end(), [this] (const Pixel& p) { return p.life <= 0.0f || p.y > (float) getHeight(); }), pixels.end());
    if (pixels.size() > 600) pixels.erase (pixels.begin(), pixels.begin() + (long) (pixels.size() - 600));
    meter += (jmin (1.0f, e.level.load() * 1.4f) - meter) * 0.3f;
    repaint();
}

void ScreenView::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    g.setColour (screenC);
    g.fillRect (b);
    // twinkling stars (they blink like sprites, not smoothly)
    for (auto& s : stars)
    {
        const bool on = ((frame / 8 + s.phase) % 40) < 30;
        g.setColour ((on ? ink : dimC).withAlpha (on ? 0.55f : 0.2f));
        g.fillRect ((int) (s.x * (float) b.getWidth()) / 2 * 2, (int) (s.y * (float) b.getHeight()) / 2 * 2, 2, 2);
    }
    // the ground: a pixel horizon
    for (int x = 0; x < b.getWidth(); x += 8)
    {
        const int h = 6 + ((x * 7 + 3) % 5) * 2;
        g.setColour (Colour (0xff2a1f5c));
        g.fillRect (x, b.getBottom() - h, 8, h);
    }

    // the falling notes, snapped to a 2 px grid
    for (auto& p : pixels)
    {
        g.setColour (p.c.withAlpha (jlimit (0.0f, 1.0f, p.life)));
        g.fillRect ((int) p.x / 2 * 2, (int) p.y / 2 * 2, p.size, p.size);
    }

    // the chord's name, with a hard shadow
    const auto name = processor.chordName();
    const auto txt = name.isEmpty() ? String ("PRESS ANY KEY") : name;
    const bool blink = name.isEmpty() && (frame / 15) % 2 == 1;
    if (! blink)
    {
        g.setFont (cat::lcdFont (name.isEmpty() ? 34.0f : 72.0f));
        const auto area = b.withTrimmedTop (name.isEmpty() ? 70 : 30).withHeight (name.isEmpty() ? 40 : 80);
        g.setColour (pink.darker (0.6f)); g.drawText (txt, area.translated (4, 4), Justification::centred);
        g.setColour (name.isEmpty() ? dimC : yellow); g.drawText (txt, area, Justification::centred);
    }

    // what is on: the chord lock and the arp
    auto& ap = processor.apvts;
    const int cm = roundToInt (ap.getRawParameterValue ("chordMode")->load());
    const int am = roundToInt (ap.getRawParameterValue ("arpMode")->load());
    g.setFont (cat::lcdFont (20.0f));
    g.setColour (cyan);
    g.drawText ("CHORD: " + hero::chordModeNames()[cm].toUpperCase(), 14, 8, 300, 20, Justification::centredLeft);
    g.setColour (mint);
    g.drawText ("ARP: " + hero::arpModeNames()[am].toUpperCase(), b.getWidth() - 214, 8, 200, 20, Justification::centredRight);

    // the "MP" bar: how loud it plays
    const int bx = 14, by = b.getBottom() - 34, bw = 150;
    g.setColour (ink); g.drawText ("MP", bx, by - 2, 30, 20, Justification::centredLeft);
    g.setColour (Colour (0xff2a1f5c)); g.fillRect (bx + 30, by + 4, bw, 10);
    const int segs = 15;
    for (int i = 0; i < segs; ++i)
        if ((float) i / (float) segs < meter)
        {
            g.setColour (i > 12 ? pink : i > 9 ? yellow : cyan);
            g.fillRect (bx + 31 + i * 10, by + 5, 8, 8);
        }
    pixelCat (g, b.getWidth() - 62, b.getBottom() - 58, 4, ink, cyan);

    // scanlines over everything (a CRT)
    g.setColour (Colours::black.withAlpha (0.22f));
    for (int y = 0; y < b.getHeight(); y += 3) g.fillRect (0, y, b.getWidth(), 1);
}

//==============================================================================
PatternView::PatternView (HeroCatProcessor& p) : processor (p) {}

void PatternView::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    g.setColour (screenC);
    g.fillRect (b);
    const int am = roundToInt (processor.apvts.getRawParameterValue ("arpMode")->load());
    const int now = am == hero::ArpPattern ? processor.engine.arpStep.load() : -1;
    const float cw = (float) b.getWidth() / 16.0f;
    for (int i = 0; i < 16; ++i)
    {
        const float v = processor.engine.pattern[(size_t) i].load();
        const int x = (int) ((float) i * cw) + 3, w = (int) cw - 6;
        const int blocks = roundToInt (v * 8.0f);
        // empty slots, then the lit blocks from the bottom (like an equaliser in an old stereo)
        for (int k = 0; k < 8; ++k)
        {
            const int y = b.getBottom() - 8 - (k + 1) * ((b.getHeight() - 20) / 8);
            const int h = (b.getHeight() - 20) / 8 - 3;
            if (k < blocks) g.setColour (i == now ? ink : (k > 5 ? pink : k > 3 ? yellow : cyan));
            else g.setColour (Colour (0xff1c1640));
            g.fillRect (x, y, w, h);
        }
        if (i % 4 == 0) { g.setColour (dimC.withAlpha (0.5f)); g.fillRect (x, b.getBottom() - 5, w, 2); }
    }
    if (am != hero::ArpPattern)
    {
        g.setColour (screenC.withAlpha (0.55f));
        g.fillRect (b);
        g.setFont (cat::lcdFont (20.0f));
        const auto msg = b.withSizeKeepingCentre (300, 26);
        g.setColour (screenC); g.fillRect (msg);
        g.setColour (dimC);
        g.drawText ("set ARP to PATTERN to play these", msg, Justification::centred);
    }
}

void PatternView::setFrom (Point<float> p)
{
    const int i = jlimit (0, 15, (int) (p.x / ((float) getWidth() / 16.0f)));
    const float v = jlimit (0.0f, 1.0f, 1.0f - (p.y - 6.0f) / (float) (getHeight() - 14));
    processor.engine.pattern[(size_t) i] = std::round (v * 8.0f) / 8.0f;
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
    addAndMakeVisible (screen);
    addAndMakeVisible (pattern);
    for (auto* b : { &dice, &clear }) addAndMakeVisible (*b);
    dice.setTooltip ("DICE: a new random pattern");
    clear.setTooltip ("CLEAR: every step at full loudness");
    dice.onClick = [this] { processor.engine.randomisePattern (rng); pattern.repaint(); };
    clear.onClick = [this] { for (auto& x : processor.engine.pattern) x = 1.0f; pattern.repaint(); };

    struct C { cat::ParamCombo* c; const char* tip; };
    const C cs[] = {
        { &srcA, "Sound A: synth waves (saw, square, supersaw, chip) or small physical models (guitar, strings, flute, choir, bell)" },
        { &srcB, "Sound B: a second sound under every note (A / B mixes them)" },
        { &octB, "Sound B an octave lower or higher" },
        { &chordMode, "CHORD LOCK: one key becomes a chord - from the scale below, or always the same shape" },
        { &key, "The song's key (for the scale chords and the chimes)" },
        { &scale, "The song's scale: keys outside it snap into it" },
        { &arpMode, "ARP: plays the held chord as a melody. WATERFALL: falls from the top and keeps ringing. PATTERN: your drawn steps" },
        { &rate, "How fast the arp goes (with the DAW's tempo)" },
        { &octaves, "How many octaves the arp climbs" },
        { &texture, "A sound bed under everything: rain, birds, a quiet room, chimes on your chord, wind" },
        { &lfoShape, "The LFO's shape" },
        { &lfoTarget, "What the LFO moves" },
    };
    for (auto& c : cs) { c.c->getCombo().setTooltip (c.tip); addAndMakeVisible (*c.c); }

    struct S { cat::LineSlider* s; const char* tip; Colour a, b; };
    const S ss[] = {
        { &mix, "A / B: how much of sound B", cyan, pink }, { &detune, "DETUNE: the saws spread apart (fatter)", cyan, pink },
        { &cutoff, "CUTOFF: darker or brighter", cyan, pink }, { &reso, "RESO: a whistle at the cutoff", cyan, pink },
        { &envAmt, "F.ENV: the filter opens with every note", cyan, pink }, { &vibrato, "VIBRATO: the singer's wobble that comes in after a moment", cyan, pink },
        { &attack, "ATTACK: how slowly a note starts", cyan, pink }, { &decay, "DECAY: how fast it falls to the sustain", cyan, pink },
        { &sustain, "SUSTAIN: the level while you hold", cyan, pink }, { &release, "RELEASE: how long it rings after", cyan, pink },
        { &glide, "GLIDE: the notes slide into each other", cyan, pink },
        { &strum, "STRUM: the chord's notes one after another, like a guitar", mint, yellow },
        { &gate, "GATE: how long each arp note is", mint, yellow }, { &swing, "SWING: every second step a little late", mint, yellow },
        { &chaos, "CHAOS: the arp improvises - rests, octave jumps, double hits, other notes", mint, pink },
        { &texLevel, "TEXTURE: how loud the sound bed is", yellow, pink }, { &lfoRate, "LFO RATE: how fast it moves", yellow, pink },
        { &lfoDepth, "LFO DEPTH: how much it moves", yellow, pink }, { &drive, "DRIVE: warm distortion", yellow, pink },
        { &crush, "CRUSH: fewer bits, lower sample rate - an old console", yellow, pink }, { &chorus, "CHORUS: wide and shiny", yellow, pink },
        { &delay, "DELAY: echoes (dotted 1/8, left-right)", yellow, pink }, { &reverb, "REVERB: the hall", yellow, pink },
        { &width, "WIDTH: the stereo width", yellow, pink }, { &output, "OUTPUT: the level that comes out", yellow, pink },
    };
    for (auto& s : ss) { s.s->setColours (s.a, s.b); s.s->getSlider().setTooltip (s.tip); addAndMakeVisible (*s.s); }
    voiceLead.setTooltip ("VOICE LEAD: each chord note moves to the nearest note of the last chord (smooth changes)");
    latch.setTooltip ("LATCH: the arp keeps playing after you let go - press new keys for a new chord");
    midiOut.setTooltip ("MIDI OUT: the chords and arp notes also go to the DAW, so another instrument can play them");
    for (auto* t : { &voiceLead, &latch, &midiOut }) addAndMakeVisible (*t);

    keys.setLowestVisibleKey (36);
    keys.setColour (MidiKeyboardComponent::keyDownOverlayColourId, pink.withAlpha (0.8f));
    keys.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, cyan.withAlpha (0.3f));
    addAndMakeVisible (keys);
    setSize (designW, designH);
    startTimerHz (30);
}

HeroCatEditor::~HeroCatEditor() { stopTimer(); }

void HeroCatEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    g.setGradientFill (ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) b.getHeight(), false));
    g.fillAll();
    // a dither of dots in the background, like an old game's menu
    g.setColour (Colours::white.withAlpha (0.03f));
    for (int y = 0; y < b.getHeight(); y += 6)
        for (int x = (y / 6) % 2 * 3; x < b.getWidth(); x += 6) g.fillRect (x, y, 1, 1);

    // the title with a hard shadow
    g.setFont (cat::lcdFont (52.0f));
    g.setColour (pink.darker (0.5f)); g.drawText ("HERO-CAT", 27, 9, 300, 50, Justification::centredLeft);
    g.setColour (yellow); g.drawText ("HERO-CAT", 24, 6, 300, 50, Justification::centredLeft);
    g.setFont (cat::lcdFont (20.0f));
    g.setColour (dimC);
    g.drawText ("a lead synth for game & anime melodies", 228, 24, 480, 24, Justification::centredLeft);

    pixelPanel (g, { 16, 78, 316, 372 }, panelC, panelEdge, "SOUND", cyan);
    pixelPanel (g, { 768, 78, 316, 372 }, panelC, panelEdge, "CHORD + ARP", mint);
    pixelPanel (g, { 344, 78, 412, 250 }, panelEdge, panelEdge);
    pixelPanel (g, { 344, 344, 412, 106 }, panelEdge, panelEdge, "PATTERN", pink);
    pixelPanel (g, { 16, 466, 1068, 104 }, panelC, panelEdge, "TEXTURE  /  MOD  /  FX", yellow);
}

void HeroCatEditor::resized()
{
    presetBar.setBounds (getWidth() - 356, 16, 330, 34);
    screen.setBounds (350, 84, 400, 238);
    pattern.setBounds (350, 356, 336, 88);
    dice.setBounds (692, 358, 58, 40);
    clear.setBounds (692, 402, 58, 40);

    // SOUND
    srcA.setBounds (30, 96, 288, 28);
    srcB.setBounds (30, 130, 200, 28);
    octB.setBounds (238, 130, 80, 28);
    cat::LineSlider* left[] = { &mix, &cutoff, &envAmt, &attack, &sustain, &glide };
    cat::LineSlider* right[] = { &detune, &reso, &vibrato, &decay, &release };
    for (int i = 0; i < 6; ++i) left[i]->setBounds (30, 172 + i * 44, 136, 38);
    for (int i = 0; i < 5; ++i) right[i]->setBounds (182, 172 + i * 44, 136, 38);

    // CHORD + ARP
    chordMode.setBounds (782, 96, 288, 28);
    key.setBounds (782, 130, 110, 28);
    scale.setBounds (900, 130, 170, 28);
    strum.setBounds (782, 166, 136, 38);
    voiceLead.setBounds (934, 172, 136, 26);
    arpMode.setBounds (782, 222, 288, 28);
    rate.setBounds (782, 256, 140, 28);
    octaves.setBounds (930, 256, 140, 28);
    gate.setBounds (782, 292, 136, 38);
    swing.setBounds (934, 292, 136, 38);
    chaos.setBounds (782, 340, 288, 38);
    latch.setBounds (782, 392, 136, 26);
    midiOut.setBounds (934, 392, 136, 26);

    // TEXTURE / MOD / FX
    texture.setBounds (30, 484, 200, 28);
    lfoShape.setBounds (246, 484, 160, 28);
    lfoTarget.setBounds (414, 484, 140, 28);
    const int w = 96, gap = 9;
    int x = 30;
    for (auto* s : { &texLevel, &lfoRate, &lfoDepth, &drive, &crush, &chorus, &delay, &reverb, &width, &output })
    { s->setBounds (x, 522, w, 38); x += w + gap; }

    keys.setBounds (16, 584, getWidth() - 32, 104);
}

void HeroCatEditor::timerCallback()
{
    screen.tick();
    pattern.tick();
}

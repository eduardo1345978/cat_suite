#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr int designW = 1000, designH = 640;
// a clear night above a choir: deep blue-violet, warm gold halo, pale planets
const Colour night (0xff0a0b1e), deep (0xff04040d), gold (0xffffd27a), haloC (0xfffff1c9), pale (0xffe9ecff), dim (0xff8c91b8),
             planetC[4] = { Colour (0xff9fd3ff), Colour (0xffffa7c4), Colour (0xffb9f5c8), Colour (0xffd2b8ff) };
}

//==============================================================================
SkyView::SkyView (HaloCatProcessor& p) : processor (p)
{
    Random rnd (7);
    for (int i = 0; i < 140; ++i) stars.push_back ({ rnd.nextFloat(), rnd.nextFloat(), 0.6f + rnd.nextFloat() * 1.6f, rnd.nextFloat() * 6.28f });
}

void SkyView::tick()
{
    time += 1.0f / 30.0f;
    glow += (jmin (1.0f, processor.engine.level.load() * 3.0f) - glow) * 0.15f;
    repaint();
}

void SkyView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    for (auto& s : stars)
    {
        const float a = 0.25f + 0.35f * (0.5f + 0.5f * std::sin (time * 1.3f + s.twinkle));
        g.setColour (pale.withAlpha (a));
        g.fillEllipse (s.x * b.getWidth() - s.size * 0.5f, s.y * b.getHeight() - s.size * 0.5f, s.size, s.size);
    }

    // the orbits, flattened a little (seen from the side)
    const float rx[4] = { 150.0f, 230.0f, 310.0f, 390.0f };
    const float squash = 0.42f;
    for (int k = 0; k < 4; ++k)
    {
        g.setColour (planetC[k].withAlpha (0.16f));
        g.drawEllipse (c.x - rx[k], c.y - rx[k] * squash, rx[k] * 2.0f, rx[k] * 2.0f * squash, 1.0f);
    }
    // the top line: where a planet rings its bell
    g.setColour (gold.withAlpha (0.18f));
    g.drawLine (c.x, c.y - rx[3] * squash - 14.0f, c.x, c.y - 70.0f, 1.0f);

    // the halo: the choir, brighter when it sings
    for (int i = 5; i >= 1; --i)
    {
        const float r = 46.0f + (float) i * 10.0f * (0.6f + glow);
        g.setColour (gold.withAlpha (0.035f * (float) (6 - i) * (0.3f + glow)));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }
    g.setColour (haloC.withAlpha (0.5f + 0.5f * glow));
    g.drawEllipse (c.x - 52.0f, c.y - 52.0f, 104.0f, 104.0f, 2.5f);
    const auto name = processor.chordName();
    g.setColour (name.isEmpty() ? dim : haloC);
    g.setFont (cat::displayFont (name.isEmpty() ? 15.0f : 30.0f));
    g.drawText (name.isEmpty() ? "hold a key" : name, Rectangle<float> (c.x - 60.0f, c.y - 20.0f, 120.0f, 40.0f), Justification::centred);

    // the planets: behind the halo when far away, in front when near
    for (int k = 0; k < 4; ++k)
    {
        const float ph = processor.engine.planetPhase[(size_t) k].load();
        const float ang = ph * MathConstants<float>::twoPi;            // 0 = the top
        const float x = c.x + std::sin (ang) * rx[k], y = c.y - std::cos (ang) * rx[k] * squash;
        const float flash = processor.engine.planetFlash[(size_t) k].load();
        const float size = 9.0f + 3.0f * (float) k + 6.0f * flash;
        if (flash > 0.05f)
        {
            g.setColour (planetC[k].withAlpha (0.35f * flash));
            g.fillEllipse (x - size * 1.6f, y - size * 1.6f, size * 3.2f, size * 3.2f);
        }
        g.setGradientFill (ColourGradient (Colours::white, x - size * 0.3f, y - size * 0.3f, planetC[k], x + size * 0.5f, y + size * 0.5f, true));
        g.fillEllipse (x - size * 0.5f, y - size * 0.5f, size, size);
    }
}

//==============================================================================
HaloCatEditor::HaloCatEditor (HaloCatProcessor& p) : processor (p)
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (sky);
    for (auto* cb : { &progression, &change, &orbits }) addAndMakeVisible (*cb);
    progression.getCombo().setTooltip ("Hold ONE key: the choir sings this progression in that key. Hold several keys: your own chord");
    change.getCombo().setTooltip ("How often the chord changes (with one key held)");
    orbits.getCombo().setTooltip ("How fast each planet goes round: every time one passes the top it rings a bell");
    struct T { cat::LineSlider* s; const char* tip; };
    const T ts[] = {
        { &choir, "CHOIR: how loud the singing choir is" },
        { &vowel, "VOWEL: what the choir sings - Ah, Oh, Oo, Ee" },
        { &glide, "GLIDE: how slowly the voices slide to the next chord" },
        { &bells, "BELLS: how loud the planets' bells are" },
        { &spin, "SPIN: how fast the planets go round (with the DAW's tempo)" },
        { &dust, "STAR DUST: tiny high pings on the chord's notes, at random" },
        { &shimmer, "SHIMMER: the big hall around it all" },
        { &brightness, "BRIGHT: darker or brighter, choir and bells" },
        { &output, "OUTPUT: the level that comes out" },
    };
    for (auto& t : ts)
    {
        t.s->setColours (Colour (0xff6f78d8), gold);
        t.s->getSlider().setTooltip (t.tip);
        addAndMakeVisible (*t.s);
    }
    keys.setLowestVisibleKey (36);
    keys.setColour (MidiKeyboardComponent::keyDownOverlayColourId, gold.withAlpha (0.7f));
    keys.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, gold.withAlpha (0.25f));
    addAndMakeVisible (keys);
    setSize (designW, designH);
    startTimerHz (30);
}

HaloCatEditor::~HaloCatEditor() { stopTimer(); }

void HaloCatEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (night, b.getCentreX(), 240.0f, deep, b.getCentreX(), b.getBottom(), false));
    g.fillAll();
    g.setColour (gold);
    g.setFont (cat::displayFont (28.0f));
    g.drawText ("HALO-CAT", 36, 20, 300, 36, Justification::centredLeft);
    g.setColour (dim);
    g.setFont (cat::catFont (11.0f));
    g.drawText ("a choir with planets  -  hold one key for a progression, or play your own chord", 38, 54, 520, 16, Justification::centredLeft);
}

void HaloCatEditor::resized()
{
    presetBar.setBounds (getWidth() - 360, 24, 320, 34);
    sky.setBounds (0, 76, getWidth(), 330);
    progression.setBounds (40, 414, 300, 28);
    change.setBounds (356, 414, 170, 28);
    orbits.setBounds (542, 414, 220, 28);
    const int w = 96, gap = 12, y = 456;
    int x = (getWidth() - (9 * w + 8 * gap)) / 2;
    for (auto* s : { &choir, &vowel, &glide, &bells, &spin, &dust, &shimmer, &brightness, &output }) { s->setBounds (x, y, w, 40); x += w + gap; }
    keys.setBounds (40, 516, getWidth() - 80, 100);
}

void HaloCatEditor::timerCallback() { sky.tick(); }

#include "PluginEditor.h"

using namespace juce;
namespace lk = tomb::look;

namespace { constexpr int designW = 760, designH = 520; }

CryptFxEditor::CryptFxEditor (CryptFxProcessor& p) : processor (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (presetBar);
    struct S { const char* id; const char* cap; Colour c; const char* tip; };
    const S defs[] = {
        { "bits", "Bits", lk::poison, "BITS: fewer bits = dirtier, more 8-bit (16 = clean)" },
        { "rate", "Rate", lk::poison, "RATE: a lower sample rate = the crunch of an old console" },
        { "drive", "Drive", lk::blood, "DRIVE: distortion" },
        { "gloom", "Gloom", lk::blood, "GLOOM: darker (a low-pass filter)" },
        { "crypt", "Crypt", lk::flame, "CRYPT: a big cold stone room" },
        { "echo", "Echo", lk::flame, "ECHO: a dotted echo with the song, darker every time" },
        { "mix", "Mix", lk::bone, "MIX: how much of the crypt (0 = the sound as it came in)" },
        { "output", "Output", lk::bone, "OUTPUT: the volume" },
    };
    for (auto& d : defs)
    {
        auto* s = sliders.add (new lk::PixelSlider (processor.apvts, d.id, d.cap, d.c));
        s->getSlider().setTooltip (d.tip);
        addAndMakeVisible (s);
    }
    setSize (designW, designH);
    startTimerHz (30);
}

CryptFxEditor::~CryptFxEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CryptFxEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    lk::wall (g, b, 0.8f + 0.2f * std::sin (time * 7.0f) * std::sin (time * 3.1f));
    ColourGradient shade (Colours::black.withAlpha (0.0f), b.getCentreX(), b.getCentreY(), Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
    g.setGradientFill (shade);
    g.fillRect (b);
    lk::torch (g, { 44.0f, 64.0f }, time);
    lk::torch (g, { b.getWidth() - 44.0f, 64.0f }, time + 1.3f);
    lk::pixelText (g, "Crypt FX", 84.0f, 22.0f, 5.0f, lk::bone);
    lk::pixelText (g, "any sound into dark 8-bit", 88.0f, 66.0f, 2.0f, lk::boneDim);

    // the screen: the sound in pixels, a skull
    lk::panel (g, screenArea, 3.0f);
    const auto inner = screenArea.reduced (12.0f);
    g.setColour (Colours::black.withAlpha (0.25f));
    for (float y = inner.getY(); y < inner.getBottom(); y += 4.0f) g.fillRect (inner.getX(), y, inner.getWidth(), 1.0f);
    const auto wave = inner.withTrimmedRight (110.0f);
    const float px = 4.0f;
    const int cols = (int) (wave.getWidth() / px);
    const int start = processor.scopePos.load();
    const int len = (int) processor.scope.size();
    for (int c = 0; c < cols; ++c)
    {
        const float v = processor.scope[(size_t) ((start + c * len / cols) % len)].load (std::memory_order_relaxed);
        const float y = std::round ((wave.getCentreY() - jlimit (-1.0f, 1.0f, v) * wave.getHeight() * 0.45f) / px) * px;
        g.setColour (lk::poison);
        g.fillRect (wave.getX() + (float) c * px, y, px - 1.0f, px - 1.0f);
    }
    lk::skull (g, { inner.getRight() - 50.0f, inner.getCentreY() + 4.0f }, 6.0f, jaw, lk::blood.interpolatedWith (lk::flame, jaw));

    lk::panel (g, { 30.0f, 270.0f, 340.0f, 230.0f }, 3.0f);
    lk::panel (g, { 390.0f, 270.0f, 340.0f, 230.0f }, 3.0f);
    lk::pixelText (g, "8-bit", 46.0f, 282.0f, 3.0f, lk::poison);
    lk::pixelText (g, "Crypt", 406.0f, 282.0f, 3.0f, lk::flame);
}

void CryptFxEditor::resized()
{
    presetBar.setBounds (getWidth() - 380, 26, 300, 34);
    screenArea = { 30.0f, 100.0f, 700.0f, 154.0f };
    for (int i = 0; i < 8; ++i)
    {
        const bool right = i >= 4 && i < 6;
        const int col = (i < 2 || i == 2 || i == 3) ? 0 : (right ? 1 : 1);
        ignoreUnused (col);
    }
    // left: bits, rate, drive, gloom; right: crypt, echo, mix, output
    for (int i = 0; i < 4; ++i) sliders[i]->setBounds (46, 312 + i * 46, 308, 36);
    for (int i = 4; i < 8; ++i) sliders[i]->setBounds (406, 312 + (i - 4) * 46, 308, 36);
}

void CryptFxEditor::timerCallback()
{
    const auto now = Time::getMillisecondCounter();
    const float dt = lastTick == 0 ? 1.0f / 30.0f : jlimit (0.005f, 0.1f, (float) (now - lastTick) / 1000.0f);
    lastTick = now;
    time += dt;
    jaw += (jlimit (0.0f, 1.0f, processor.level.load() * 3.0f) - jaw) * jmin (1.0f, dt * 20.0f);
    repaint();
}

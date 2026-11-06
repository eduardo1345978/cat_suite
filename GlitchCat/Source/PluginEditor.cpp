#include "PluginEditor.h"
#include "CatTheme.h"

using namespace juce;

namespace
{
constexpr int designW = 1200, designH = 840;
const Colour acid (0xff3dff9a), magenta (0xffff2bd6), cyan (0xff33d6ff), errorRed (0xffff3b3b), amber (0xffffd23d), ink (0xffd9ffe9);
const Colour noteColours[] = { acid, magenta, cyan, amber, Colour (0xffa58bff), Colour (0xffff8a3d) };
constexpr int keyLo = 36, keyHi = 96;

String noteName (int key)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return String (n[((key % 12) + 12) % 12]) + String (key / 12 - 2);     // Cubase: C3 = 60
}

/** Text that falls apart: characters swapped for junk, more of them the madder it is. */
String corrupt (const String& s, float amount, Random& rng)
{
    if (amount <= 0.0f) return s;
    static const char* junk = "#%&@!?/\\<>*=+01";
    String out;
    for (auto c : s)
        out << (c != ' ' && rng.nextFloat() < amount ? (juce_wchar) junk[rng.nextInt (15)] : c);
    return out;
}

void panel (Graphics& g, Rectangle<float> r, const String& title)
{
    g.setColour (Colour (0xe0080a0c));
    g.fillRect (r);
    g.setColour (acid.withAlpha (0.25f));
    g.drawRect (r, 1.0f);
    // corner brackets
    g.setColour (acid.withAlpha (0.8f));
    const float k = 10.0f;
    for (auto [x, y, sx, sy] : { std::tuple { r.getX(), r.getY(), 1.0f, 1.0f }, std::tuple { r.getRight(), r.getY(), -1.0f, 1.0f },
                                 std::tuple { r.getX(), r.getBottom(), 1.0f, -1.0f }, std::tuple { r.getRight(), r.getBottom(), -1.0f, -1.0f } })
    {
        g.fillRect (sx > 0 ? x : x - k, sy > 0 ? y : y - 2.0f, k, 2.0f);
        g.fillRect (sx > 0 ? x : x - 2.0f, sy > 0 ? y : y - k, 2.0f, k);
    }
    g.setFont (cat::lcdFont (20.0f));
    g.drawText ("> " + title, r.reduced (12.0f, 4.0f).withHeight (22.0f), Justification::centredLeft);
}
}

//==============================================================================
SwarmScreen::SwarmScreen (GlitchCatProcessor& p) : processor (p)
{
    setTooltip ("The bugs: every pixel is a bug - left / right is where you hear it, up / down its pitch. "
                "Click the strip at the bottom to play (the magenta keys are drums)");
    setOpaque (true);
}

void SwarmScreen::resized()
{
    layer = Image (Image::ARGB, jmax (1, getWidth()), jmax (1, getHeight()), true);
}

Rectangle<float> SwarmScreen::keyboardArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f).removeFromBottom (30.0f);
}

int SwarmScreen::keyAt (Point<float> pt) const
{
    const auto kb = keyboardArea();
    if (! kb.contains (pt)) return -1;
    const float w = kb.getWidth() / (float) (keyHi - keyLo + 1);
    return jlimit (keyLo, keyHi, keyLo + (int) ((pt.x - kb.getX()) / w));
}

void SwarmScreen::mouseDown (const MouseEvent& e)
{
    playing = keyAt (e.position);
    if (playing >= 0) processor.keyboardState.noteOn (1, playing, 0.85f);
}

void SwarmScreen::mouseDrag (const MouseEvent& e)
{
    const int k = keyAt (e.position);
    if (k == playing || k < 0) return;
    if (playing >= 0) processor.keyboardState.noteOff (1, playing, 0.0f);
    playing = k;
    processor.keyboardState.noteOn (1, playing, 0.85f);
}

void SwarmScreen::mouseUp (const MouseEvent&)
{
    if (playing >= 0) processor.keyboardState.noteOff (1, playing, 0.0f);
    playing = -1;
}

void SwarmScreen::tick()
{
    ++frame;
    // the pitch range follows the notes
    float nlo = 200.0f, nhi = -1.0f;
    for (auto& k : processor.engine.displayKey)
        if (const int key = k.load(); key >= 0) { nlo = jmin (nlo, (float) key); nhi = jmax (nhi, (float) key); }
    if (nhi >= 0.0f)
    {
        const float wantLo = jmin (nlo - 14.0f, nhi - 30.0f), wantHi = jmax (nhi + 14.0f, nlo + 30.0f);
        lo += (wantLo - lo) * 0.08f;
        hi += (wantHi - hi) * 0.08f;
    }
    repaint();
}

void SwarmScreen::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float m = processor.apvts.getRawParameterValue ("madness")->load();
    const int glitchNow = processor.engine.glitchNow.load();
    auto& eng = processor.engine;

    // the screen
    g.fillAll (Colour (0xff020403));
    {
        ColourGradient glow (Colour (0xff0a1a12), b.getCentreX(), b.getCentreY(), Colour (0xff020403), b.getX(), b.getY(), true);
        g.setGradientFill (glow);
        g.fillRect (b.reduced (2.0f));
    }
    auto plot = b.reduced (10.0f);
    plot.removeFromBottom (36.0f);
    auto yOf = [&] (float pitch) { return plot.getBottom() - (pitch - lo) / jmax (1.0f, hi - lo) * plot.getHeight(); };

    // grid: octave lines with names
    g.setFont (cat::lcdFont (16.0f));
    for (int k = ((int) lo / 12 + 1) * 12; k <= (int) hi; k += 12)
    {
        const float y = yOf ((float) k);
        g.setColour (acid.withAlpha (0.10f));
        for (float x = plot.getX(); x < plot.getRight(); x += 6.0f) g.fillRect (x, y, 2.0f, 1.0f);
        g.setColour (acid.withAlpha (0.4f));
        g.drawText (noteName (k), Rectangle<float> (plot.getX() + 4.0f, y - 14.0f, 40.0f, 14.0f), Justification::centredLeft);
    }
    g.setColour (acid.withAlpha (0.08f));
    g.drawVerticalLine (roundToInt (plot.getCentreX()), plot.getY(), plot.getBottom());
    g.setColour (acid.withAlpha (0.35f));
    g.drawText ("L", plot.removeFromLeft (0.0f).withWidth (20.0f).withY (plot.getBottom() - 16.0f).withHeight (16.0f), Justification::centredLeft);
    g.drawText ("R", Rectangle<float> (plot.getRight() - 20.0f, plot.getBottom() - 16.0f, 20.0f, 16.0f), Justification::centredRight);

    // the swarm, into its own layer (so it can be split and torn)
    layer.clear (layer.getBounds());
    bool any = false;
    {
        Graphics lg (layer);
        for (size_t n = 0; n < eng.displayKey.size(); ++n)
        {
            const int key = eng.displayKey[n].load();
            const auto col = noteColours[(size_t) (key < 0 ? 0 : key) % std::size (noteColours)];
            if (key >= 0)
            {
                any = true;
                const float y = yOf ((float) key);
                lg.setColour (col.withAlpha (0.25f));
                for (float x = plot.getX() + 46.0f; x < plot.getRight(); x += 10.0f) lg.fillRect (x, y, 4.0f, 1.0f);
                lg.setColour (col);
                lg.setFont (cat::lcdFont (18.0f));
                lg.drawText (noteName (key), Rectangle<float> (plot.getRight() - 46.0f, y - 16.0f, 44.0f, 16.0f), Justification::centredRight);
            }
            for (int j = 0; j < glitch::Engine::bugsPerNote; ++j)
            {
                const size_t idx = n * glitch::Engine::bugsPerNote + (size_t) j;
                auto& d = eng.display[idx];
                auto& tr = trails[idx];
                const float bright = d.bright.load();
                if (key < 0 || bright < 0.01f) { tr.n = 0; continue; }
                const Point<float> pt (plot.getX() + d.x.load() * plot.getWidth(), yOf (d.pitch.load()));
                for (int t = trail - 1; t > 0; --t) tr.p[(size_t) t] = tr.p[(size_t) t - 1];
                tr.p[0] = pt;
                tr.n = jmin (trail, tr.n + 1);
                for (int t = tr.n - 1; t >= 1; --t)
                {
                    lg.setColour (col.withAlpha (0.12f * bright * (1.0f - (float) t / trail)));
                    lg.fillRect (Rectangle<float> (4.0f, 4.0f).withCentre (tr.p[(size_t) t]));
                }
                const float size = 5.0f + 6.0f * bright;
                lg.setColour (col.withAlpha (0.25f * bright));
                lg.fillRect (Rectangle<float> (size * 2.2f, size * 2.2f).withCentre (pt));
                lg.setColour (col.withAlpha (jlimit (0.2f, 1.0f, bright * 1.2f)));
                lg.fillRect (Rectangle<float> (size, size).withCentre (pt));
                lg.setColour (Colours::white.withAlpha (0.7f * bright));
                lg.fillRect (Rectangle<float> (2.0f, 2.0f).withCentre (pt));
            }
        }
    }

    // RGB split: the madder, the further the colours slide apart
    const float split = 1.0f + m * 6.0f + (glitchNow != glitch::GlitchNone ? 4.0f : 0.0f);
    g.setColour (cyan.withAlpha (0.55f));
    g.drawImageAt (layer, (int) -split, 0, true);
    g.setColour (magenta.withAlpha (0.55f));
    g.drawImageAt (layer, (int) split, 0, true);
    g.drawImageAt (layer, 0, 0);

    // tears: slices of the picture shoved sideways
    const int tears = (int) (m * m * 5.0f) + (glitchNow != glitch::GlitchNone ? 3 + rng.nextInt (4) : 0);
    for (int i = 0; i < tears; ++i)
    {
        const int h = 3 + rng.nextInt (18), y = rng.nextInt (jmax (1, (int) plot.getBottom() - h));
        const int dx = (rng.nextInt (2) == 0 ? -1 : 1) * (6 + rng.nextInt (40));
        g.setColour (Colour (0xff020403));
        g.fillRect (0, y, getWidth(), h);
        g.drawImage (layer, dx, y, getWidth(), h, 0, y, getWidth(), h);
        if (rng.nextInt (3) == 0)
        {
            g.setColour ((rng.nextBool() ? magenta : acid).withAlpha (0.25f));
            g.fillRect (rng.nextInt (getWidth()), y, 20 + rng.nextInt (160), h);
        }
    }

    if (! any)
    {
        g.setColour (acid.withAlpha (0.75f));
        g.setFont (cat::lcdFont (28.0f));
        const bool blink = (frame / 15) % 2 == 0;
        g.drawText (String ("PLAY SOME NOTES - THE BUGS ARE WAITING") + (blink ? "_" : " "), plot, Justification::centred);
    }

    // the drums: four flashing blocks top right
    {
        static const char* names[] = { "KICK", "SNARE", "HATS", "PERC" };
        auto row = b.reduced (12.0f).removeFromTop (22.0f).removeFromRight (280.0f);
        const float w = row.getWidth() / 4.0f;
        g.setFont (cat::lcdFont (17.0f));
        for (int i = 0; i < 4; ++i)
        {
            auto cell = row.removeFromLeft (w).reduced (3.0f, 0.0f);
            const float f = jlimit (0.0f, 1.0f, eng.drumFlash[(size_t) i].load());
            g.setColour (magenta.withAlpha (0.12f + 0.8f * f));
            g.fillRect (cell);
            g.setColour (f > 0.4f ? Colour (0xff050607) : magenta.withAlpha (0.8f));
            g.drawText (names[i], cell, Justification::centred);
        }
    }
    // the glitch it is doing now
    if (glitchNow != glitch::GlitchNone)
    {
        static const char* names[] = { "", "ERR_REPEAT", "ERR_REVERSE", "ERR_TAPESTOP", "ERR_CHOP", "ERR_PITCH" };
        g.setFont (cat::lcdFont (24.0f));
        g.setColour (errorRed);
        g.drawText (names[jlimit (0, 5, glitchNow)], b.reduced (14.0f, 8.0f).removeFromTop (24.0f), Justification::centredLeft);
    }

    // the keyboard strip
    {
        const auto kb = keyboardArea();
        const float w = kb.getWidth() / (float) (keyHi - keyLo + 1);
        const int mode = roundToInt (processor.apvts.getRawParameterValue ("beatMode")->load());
        const bool drumKeys = mode == glitch::BeatKeys || mode == glitch::BeatBoth;
        for (int k = keyLo; k <= keyHi; ++k)
        {
            const int pc = k % 12;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            const bool drum = drumKeys && k < glitch::drumKeysBelow;
            auto r = Rectangle<float> (kb.getX() + (float) (k - keyLo) * w, kb.getY(), w, kb.getHeight()).reduced (0.5f, 0.0f);
            bool on = processor.keyboardState.isNoteOnForChannels (0xffff, k);
            for (auto& dk : eng.displayKey) on |= dk.load() == k;
            const auto base = drum ? magenta : acid;
            g.setColour (on ? base : base.withAlpha (black ? 0.12f : 0.28f));
            g.fillRect (black ? r.withTrimmedBottom (r.getHeight() * 0.35f) : r);
        }
        g.setFont (cat::lcdFont (15.0f));
        g.setColour (Colour (0xff050607));
        if (drumKeys)
            g.drawText ("DRUMS", Rectangle<float> (kb.getX(), kb.getY(), w * (glitch::drumKeysBelow - keyLo), kb.getHeight()), Justification::centred);
    }

    // CRT: scan lines, a vignette, the bezel
    g.setColour (Colour (0x40000000));
    for (int y = 0; y < getHeight(); y += 3) g.fillRect (0, y, getWidth(), 1);
    g.setColour (acid.withAlpha (0.3f));
    g.drawRect (getLocalBounds(), 1);
    if (m > 0.6f && rng.nextFloat() < (m - 0.6f))          // dead pixels flicker
        for (int i = 0; i < 12; ++i)
        {
            g.setColour (noteColours[rng.nextInt ((int) std::size (noteColours))].withAlpha (0.8f));
            g.fillRect (rng.nextInt (getWidth()), rng.nextInt (getHeight()), 2 + rng.nextInt (6), 2);
        }
}

//==============================================================================
MadnessKnob::MadnessKnob() : Slider (Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox)
{
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    setTooltip ("MADNESS: gentle (warm, wobbly bugs on the beat) to insane (rolls, jumps, stutters, reversed bits, off the grid). "
                "The mod wheel turns it up too");
    setVelocityBasedMode (false);
    setMouseDragSensitivity (300);
}

void MadnessKnob::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float size = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float v = (float) valueToProportionOfLength (getValue());
    const auto rp = getRotaryParameters();
    const float r = size * 0.46f;

    // the ring of pixels
    constexpr int segs = 44;
    for (int i = 0; i < segs; ++i)
    {
        const float t = (float) i / (segs - 1);
        const float a = rp.startAngleRadians + t * (rp.endAngleRadians - rp.startAngleRadians);
        const bool lit = t <= v + 1.0e-4f;
        Colour col = t < 0.5f ? acid.interpolatedWith (amber, t * 2.0f) : (t < 0.8f ? amber.interpolatedWith (magenta, (t - 0.5f) / 0.3f) : magenta.interpolatedWith (errorRed, (t - 0.8f) / 0.2f));
        const Point<float> p (c.x + std::sin (a) * r, c.y - std::cos (a) * r);
        const float s = size * 0.045f;
        if (lit)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillRect (Rectangle<float> (s * 2.0f, s * 2.0f).withCentre (p));
        }
        g.setColour (lit ? col : col.withAlpha (0.12f));
        g.fillRect (Rectangle<float> (s, s).withCentre (p));
    }

    // the body: a dark disc full of noise (more noise when mad)
    const auto disc = Rectangle<float> (size * 0.72f, size * 0.72f).withCentre (c);
    g.setColour (Colour (0xff0a0d10));
    g.fillEllipse (disc);
    g.setColour (acid.withAlpha (0.35f));
    g.drawEllipse (disc, 1.5f);
    {
        Graphics::ScopedSaveState save (g);
        Path clip; clip.addEllipse (disc.reduced (2.0f));
        g.reduceClipRegion (clip);
        const int dots = (int) (40 + v * v * 500);
        for (int i = 0; i < dots; ++i)
        {
            g.setColour ((rng.nextInt (3) == 0 ? magenta : acid).withAlpha (0.08f + 0.25f * v * rng.nextFloat()));
            g.fillRect (disc.getX() + rng.nextFloat() * disc.getWidth(), disc.getY() + rng.nextFloat() * disc.getHeight(), 2.0f, 2.0f);
        }
        // the pointer: a line of pixels
        const float a = rp.startAngleRadians + v * (rp.endAngleRadians - rp.startAngleRadians);
        for (float k = 0.25f; k < 0.95f; k += 0.08f)
        {
            const Point<float> p (c.x + std::sin (a) * disc.getWidth() * 0.5f * k, c.y - std::cos (a) * disc.getWidth() * 0.5f * k);
            g.setColour (Colours::white.withAlpha (0.4f + 0.6f * k));
            g.fillRect (Rectangle<float> (5.0f, 5.0f).withCentre (p));
        }
    }

    // the words fall apart
    const float jit = v * v * 4.0f;
    const auto off = Point<float> (rng.nextFloat() * jit - jit * 0.5f, rng.nextFloat() * jit - jit * 0.5f);
    g.setFont (cat::lcdFont (size * 0.09f));
    g.setColour (ink.withAlpha (0.8f));
    g.drawText (corrupt ("MADNESS", v * v * 0.35f, rng), disc.withHeight (disc.getHeight() * 0.42f).translated (off.x, off.y), Justification::centredBottom);
    g.setFont (cat::lcdFont (size * 0.2f));
    const auto valCol = v < 0.5f ? acid : (v < 0.8f ? amber : magenta);
    const String val = String (roundToInt (v * 100.0f)) + "%";
    if (v > 0.5f)
    {
        g.setColour (cyan.withAlpha (0.5f));
        g.drawText (val, disc.translated (-2.0f - jit, 0.0f), Justification::centred);
    }
    g.setColour (valCol);
    g.drawText (corrupt (val, v > 0.85f ? 0.2f : 0.0f, rng), disc.translated (off.x, off.y), Justification::centred);
    g.setFont (cat::lcdFont (size * 0.07f));
    g.setColour (ink.withAlpha (0.6f));
    g.drawText (v < 0.33f ? "gentle" : v < 0.66f ? "unstable" : corrupt ("insane", v - 0.5f, rng), disc.withTrimmedTop (disc.getHeight() * 0.62f), Justification::centredTop);

    g.setFont (cat::lcdFont (size * 0.075f));
    g.setColour (acid.withAlpha (0.7f));
    g.drawText ("GENTLE", Rectangle<float> (b.getX(), b.getBottom() - size * 0.12f, size * 0.42f, size * 0.1f), Justification::centredLeft);
    g.setColour (errorRed.withAlpha (0.8f));
    g.drawText ("INSANE", Rectangle<float> (b.getRight() - size * 0.42f, b.getBottom() - size * 0.12f, size * 0.42f, size * 0.1f), Justification::centredRight);
}

//==============================================================================
GlitchCatEditor::GlitchCatEditor (GlitchCatProcessor& p) : processor (p)
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (mutateButton);
    mutateButton.setTooltip ("Shake the sound: new bugs, flight, tape, tone, shape and space round where they are (Madness stays)");
    mutateButton.onClick = [this] { processor.mutate(); status = "MUTATED"; };
    addAndMakeVisible (screen);
    addAndMakeVisible (madness);
    madnessAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, "madness", madness);

    detune.setTooltip ("Madness pulls the bugs out of tune - and at the top, jumps them by octaves and fifths");
    rolls.setTooltip ("Madness turns the bugs (and the drums) into rolls and ratchets");
    stutter.setTooltip ("Madness stutters, reverses, tape-stops and chops the whole sound");
    odd.setTooltip ("Madness gives every bug and drum its own odd length: they drift apart and meet again");
    for (auto* t : { &detune, &rolls, &stutter, &odd }) addAndMakeVisible (*t);

    for (auto* k : { &swarm, &flight, &tape, &attack, &release, &synth, &tone, &shape, &voice, &drums, &space, &volume, &tempo })
        addAndMakeVisible (*k);
    swarm.getSlider().setTooltip ("How many bugs a note falls apart into (2 - 8)");
    flight.getSlider().setTooltip ("How far and how fast the bugs fly round left / right");
    tape.getSlider().setTooltip ("Old tape: wobbling pitch, warmth, a softer top, a little hiss");
    shape.getSlider().setTooltip ("The synth's wave: saw -> square -> glass bells");
    tempo.getSlider().setTooltip ("The tempo when Cubase isn't playing (in Cubase it follows the project)");
    for (auto* c : { &voiceType, &kit, &beatMode, &beatStyle }) addAndMakeVisible (*c);
    voiceType.getCombo().setTooltip ("Whisper, vowels, a choir, a talking mouth - or My voice: drop a recording here or record it");
    beatMode.getCombo().setTooltip ("Bug beat: it drums along to your notes. Low keys: the keys below C2 play drums. Both: the low keys take the beat over");

    for (auto* b : { &loadButton, &recButton, &clearButton }) addAndMakeVisible (*b);
    loadButton.setTooltip ("Load a recording of your voice (wav, aiff, flac, mp3) - or drop it anywhere on Glitch-Cat");
    recButton.setTooltip ("Record your voice through Glitch-Cat's side-chain input (Cubase: activate Side-Chain, route your mic track there)");
    clearButton.setTooltip ("Forget the voice");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("A recording of your voice", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [safe = SafePointer<GlitchCatEditor> (this)] (const FileChooser& fc)
        {
            if (safe != nullptr && fc.getResult().existsAsFile()) safe->loadVoice (fc.getResult());
        });
    };
    recButton.onClick = [this]
    {
        if (processor.isRecording()) { processor.stopRecording(); status = processor.voiceName().isNotEmpty() ? "VOICE RECORDED" : "NOTHING HEARD"; }
        else if (! processor.hasInput()) status = "NO INPUT: ROUTE A MIC TO THE SIDE-CHAIN, OR DROP A FILE";
        else { processor.startRecording(); status = "RECORDING..."; }
    };
    clearButton.onClick = [this] { processor.clearVoice(); status = "VOICE CLEARED"; };

    setSize (designW, designH);
    startTimerHz (30);
}

GlitchCatEditor::~GlitchCatEditor() { stopTimer(); }

void GlitchCatEditor::paint (Graphics& g)
{
    cat::paintBackdrop (g, getLocalBounds());
    const float m = processor.apvts.getRawParameterValue ("madness")->load();
    Random rng ((int64) (frame / 3));

    // the logo: GLITCH-CAT, its colours sliding apart
    auto logo = logoArea.toFloat();
    g.setFont (cat::lcdFont (54.0f));
    const float split = 2.0f + m * 5.0f;
    const bool hiccup = rng.nextFloat() < 0.03f + m * 0.2f;
    g.setColour (cyan.withAlpha (0.7f));
    g.drawText ("GLITCH-CAT", logo.translated (-split, 0.0f), Justification::centredLeft);
    g.setColour (magenta.withAlpha (0.7f));
    g.drawText ("GLITCH-CAT", logo.translated (split, hiccup ? 2.0f : 0.0f), Justification::centredLeft);
    g.setColour (ink);
    g.drawText (corrupt ("GLITCH-CAT", hiccup ? 0.25f : 0.0f, rng), logo, Justification::centredLeft);
    if (hiccup)
    {
        const int y = logoArea.getY() + rng.nextInt (jmax (1, logoArea.getHeight() - 8));
        g.setColour (Colour (0xff050607));
        g.fillRect (logoArea.getX(), y, logoArea.getWidth(), 4);
    }
    g.setFont (cat::lcdFont (19.0f));
    g.setColour (acid.withAlpha (0.6f));
    g.drawText ("every note falls apart into bugs", logo.withTrimmedLeft (250.0f).withTrimmedTop (22.0f), Justification::centredLeft);

    // panels
    static const char* titles[] = { "BUGS", "SYNTH", "VOICE", "DRUMS", "OUT" };
    for (int i = 0; i < 5; ++i) panel (g, panels[(size_t) i].toFloat(), titles[i]);
    panel (g, madnessArea.toFloat(), "MADNESS");

    // the voice
    g.setFont (cat::lcdFont (18.0f));
    const auto name = processor.voiceName();
    if (processor.isRecording())
    {
        g.setColour (errorRed);
        g.drawText ("REC " + String (processor.recordedSeconds(), 1) + " s", voiceInfo, Justification::centredLeft);
    }
    else
    {
        g.setColour (name.isNotEmpty() ? acid : ink.withAlpha (0.45f));
        g.drawFittedText (name.isNotEmpty() ? "VOICE: " + name.toUpperCase() + " (" + String (processor.voiceSeconds(), 1) + " s)"
                                            : "NO VOICE YET - DROP A FILE HERE", voiceInfo, Justification::centredLeft, 2);
    }

    // status
    g.setColour (status.isNotEmpty() ? magenta : acid.withAlpha (0.7f));
    g.setFont (cat::lcdFont (18.0f));
    g.drawFittedText (status.isNotEmpty() ? status : "BUGS " + String (processor.engine.activeBugs.load()) + "  GLITCHES " + String (processor.engine.glitchEvents.load()),
                      statusArea, Justification::centredRight, 1, 0.7f);
}

void GlitchCatEditor::paintOverChildren (Graphics& g)
{
    if (! dropHover) return;
    g.setColour (magenta.withAlpha (0.2f));
    g.fillRect (getLocalBounds());
    g.setColour (magenta);
    g.drawRect (getLocalBounds().reduced (4), 3);
    g.setFont (cat::lcdFont (40.0f));
    g.drawText ("FEED YOUR VOICE TO THE BUGS", getLocalBounds(), Justification::centred);
}

void GlitchCatEditor::resized()
{
    auto r = getLocalBounds().reduced (20, 14);
    auto top = r.removeFromTop (54);
    logoArea = top.removeFromLeft (520);
    mutateButton.setBounds (top.removeFromRight (110).withSizeKeepingCentre (110, 36));
    top.removeFromRight (12);
    presetBar.setBounds (top.removeFromLeft (400).withSizeKeepingCentre (400, 34));
    statusArea = { 20, 70, designW - 40, 0 };
    r.removeFromTop (10);

    auto main = r.removeFromTop (440);
    madnessArea = main.removeFromRight (360);
    main.removeFromRight (14);
    screen.setBounds (main);
    {
        auto m = madnessArea.reduced (14, 10);
        m.removeFromTop (22);
        auto toggles = m.removeFromBottom (90);
        madness.setBounds (m.withSizeKeepingCentre (jmin (m.getWidth(), m.getHeight()), jmin (m.getWidth(), m.getHeight())));
        auto row1 = toggles.removeFromTop (42), row2 = toggles.withTrimmedTop (6);
        detune.setBounds (row1.removeFromLeft (row1.getWidth() / 2 - 3)); row1.removeFromLeft (6); rolls.setBounds (row1);
        stutter.setBounds (row2.removeFromLeft (row2.getWidth() / 2 - 3)); row2.removeFromLeft (6); odd.setBounds (row2);
    }

    r.removeFromTop (14);
    auto bottom = r;
    statusArea = bottom.removeFromBottom (0);
    const int widths[] = { 330, 210, 260, 220, 0 };
    int x = bottom.getX();
    for (int i = 0; i < 5; ++i)
    {
        const int w = i < 4 ? widths[i] : bottom.getRight() - x;
        panels[(size_t) i] = { x, bottom.getY(), w, bottom.getHeight() };
        x += w + 10;
    }
    auto inner = [] (Rectangle<int> p) { auto q = p.reduced (10, 8); q.removeFromTop (24); return q; };
    {
        auto q = inner (panels[0]);
        auto row1 = q.removeFromTop (q.getHeight() / 2), row2 = q;
        const int w = row1.getWidth() / 3;
        for (auto* k : { &swarm, &flight, &tape }) k->setBounds (row1.removeFromLeft (w));
        row2.removeFromLeft (w / 2);
        for (auto* k : { &attack, &release }) k->setBounds (row2.removeFromLeft (w));
    }
    {
        auto q = inner (panels[1]);
        auto row1 = q.removeFromTop (q.getHeight() / 2), row2 = q;
        const int w = row1.getWidth() / 2;
        synth.setBounds (row1.removeFromLeft (w)); tone.setBounds (row1);
        shape.setBounds (row2.withSizeKeepingCentre (w, row2.getHeight()));
    }
    {
        auto q = inner (panels[2]);
        auto row1 = q.removeFromTop (q.getHeight() / 2);
        voice.setBounds (row1.removeFromLeft (100));
        row1.removeFromLeft (8);
        voiceType.setBounds (row1.withSizeKeepingCentre (row1.getWidth(), 32).translated (0, -10));
        q.removeFromTop (4);
        auto buttons = q.removeFromTop (34);
        clearButton.setBounds (buttons.removeFromRight (34)); buttons.removeFromRight (6);
        recButton.setBounds (buttons.removeFromRight (64)); buttons.removeFromRight (6);
        loadButton.setBounds (buttons);
        q.removeFromTop (6);
        voiceInfo = q;
    }
    {
        auto q = inner (panels[3]);
        auto row1 = q.removeFromTop (q.getHeight() / 2);
        drums.setBounds (row1.removeFromLeft (90));
        row1.removeFromLeft (6);
        kit.setBounds (row1.withSizeKeepingCentre (row1.getWidth(), 30).translated (0, -10));
        q.removeFromTop (6);
        beatMode.setBounds (q.removeFromTop (30));
        q.removeFromTop (8);
        beatStyle.setBounds (q.removeFromTop (30));
    }
    {
        auto q = inner (panels[4]);
        const int h = q.getHeight() / 3;
        for (auto* k : { &space, &volume, &tempo }) k->setBounds (q.removeFromTop (h));
    }
    statusArea = madnessArea.reduced (12, 4).withHeight (24).withTrimmedLeft (110);
}

void GlitchCatEditor::timerCallback()
{
    ++frame;
    screen.tick();
    madness.frame = frame;
    if (processor.apvts.getRawParameterValue ("madness")->load() > 0.3f || frame % 4 == 0) madness.repaint();
    if (frame % 3 == 0) repaint (logoArea);
    repaint (statusArea);
    if (frame % 10 == 0) repaint (panels[2]);
    if (frame % 90 == 0) status = {};
}

bool GlitchCatEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void GlitchCatEditor::filesDropped (const StringArray& files, int, int)
{
    dropHover = false;
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) { loadVoice (File (f)); break; }
    repaint();
}

void GlitchCatEditor::loadVoice (const File& f)
{
    String error;
    if (processor.loadVoice (f, error))
    {
        status = "VOICE LOADED";
        if (processor.apvts.getRawParameterValue ("voiceLevel")->load() < 0.05f)
            if (auto* prm = processor.apvts.getParameter ("voiceLevel")) prm->setValueNotifyingHost (0.8f);
        if (auto* prm = processor.apvts.getParameter ("voiceType")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) glitch::MyVoice));
    }
    else status = error.toUpperCase();
    repaint();
}

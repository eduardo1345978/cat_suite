#include "PluginEditor.h"

using namespace juce;
namespace lk = tomb::look;

namespace
{
constexpr int designW = 1000, designH = 660;
const char* const kindTips[] = {
    "A sword: a swish of air or a ring of steel", "A hit: a low thump", "A spell: a shimmer that climbs, with an echo",
    "Fire: low crackling noise", "A door: a slow creak in a stone room", "A trap: a click and a metal rattle",
    "Death: falling, wobbling, crushed", "A chest / coins: the old coin sound", "A step on stone", "A pickup: a little blip up",
    "A growl: a low rough monster voice", "A ghost: a long whisper with a wobble", "Level up: a minor run up", "A church bell far away" };
}

//==============================================================================
void CryptScreen::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    lk::panel (g, b, 3.0f);
    const auto inner = b.reduced (12.0f);
    auto& a = processor.apvts;
    auto val = [&a] (const char* id) { return a.getRawParameterValue (id)->load(); };
    const auto w = (tomb::Wave) jlimit (0, 8, roundToInt (val ("wave")));

    // scan lines
    g.setColour (Colours::black.withAlpha (0.25f));
    for (float y = inner.getY(); y < inner.getBottom(); y += 4.0f) g.fillRect (inner.getX(), y, inner.getWidth(), 1.0f);

    // the wave: 3 periods in big pixels
    const float px = 4.0f;
    const auto waveBox = inner.withWidth (inner.getWidth() * 0.55f).reduced (4.0f, 10.0f);
    const int cols = (int) (waveBox.getWidth() / px);
    tomb::ChipOsc osc;
    const float bits = val ("bits");
    const float levels = std::exp2 (jlimit (2.0f, 16.0f, bits) - 1.0f);
    for (int c = 0; c < cols; ++c)
    {
        float y = osc.tick (w, 3.0 * 1000.0 / (double) cols, 1000.0);
        if (bits < 15.9f) y = std::round (y * levels) / levels;
        const float yy = waveBox.getCentreY() - y * waveBox.getHeight() * 0.42f;
        const float pyy = std::round (yy / px) * px;
        g.setColour (lk::poison.withAlpha (0.18f));
        g.fillRect (waveBox.getX() + (float) c * px, jmin (pyy, waveBox.getCentreY()), px - 1.0f, std::abs (pyy - waveBox.getCentreY()));
        g.setColour (lk::poison);
        g.fillRect (waveBox.getX() + (float) c * px, pyy, px - 1.0f, px - 1.0f);
    }
    lk::pixelText (g, tomb::waveNames()[(int) w], waveBox.getX(), inner.getY(), 2.0f, lk::poison.withAlpha (0.8f));

    // the envelope
    const auto envBox = Rectangle<float> (inner.getX() + inner.getWidth() * 0.58f, inner.getY() + 18.0f, inner.getWidth() * 0.24f, inner.getHeight() - 30.0f);
    const float at = tomb::envSeconds (val ("attack"), 2.0f), de = tomb::envSeconds (val ("decay"), 3.0f), su = val ("sustain"),
                re = tomb::envSeconds (val ("release"), 3.0f);
    const float total = at + de + 0.6f + re;
    const bool st = val ("steps") > 0.5f;
    for (float x = 0.0f; x < envBox.getWidth(); x += px)
    {
        const float t = x / envBox.getWidth() * total;
        float e;
        if (t < at) e = t / at;
        else if (t < at + de) e = 1.0f + (su - 1.0f) * (t - at) / de;
        else if (t < at + de + 0.6f) e = su;
        else e = su * (1.0f - (t - at - de - 0.6f) / re);
        e = jlimit (0.0f, 1.0f, e);
        if (st) e = std::floor (e * 15.0f + 0.5f) / 15.0f;
        const float y = std::round ((envBox.getBottom() - e * envBox.getHeight()) / px) * px;
        g.setColour (lk::ember);
        g.fillRect (envBox.getX() + x, y, px - 1.0f, px - 1.0f);
    }
    lk::pixelText (g, "Envelope", envBox.getX(), inner.getY(), 2.0f, lk::ember.withAlpha (0.8f));

    // the skull sings along
    lk::skull (g, { inner.getRight() - 70.0f, inner.getCentreY() + 4.0f }, 6.0f, jaw,
               lk::blood.interpolatedWith (lk::flame, jaw).withAlpha (0.6f + 0.4f * jaw));
}

//==============================================================================
void SfxScreen::setSound (const AudioBuffer<float>& b, double sr)
{
    const int cols = 150;
    peaks.assign ((size_t) cols, 0.0f);
    const int n = b.getNumSamples();
    for (int c = 0; c < cols; ++c)
    {
        const int a = n * c / cols, e = jmax (a + 1, n * (c + 1) / cols);
        peaks[(size_t) c] = b.getMagnitude (0, a, jmin (e, n) - a);
    }
    seconds = (float) (n / sr);
    repaint();
}

void SfxScreen::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    lk::panel (g, b, 3.0f);
    const auto inner = b.reduced (14.0f);
    g.setColour (Colours::black.withAlpha (0.25f));
    for (float y = inner.getY(); y < inner.getBottom(); y += 4.0f) g.fillRect (inner.getX(), y, inner.getWidth(), 1.0f);

    const auto& s = processor.sfx;
    const String name = tomb::kindFileName (s.kind) + "  #" + String (s.number).paddedLeft ('0', 2);
    lk::pixelText (g, tomb::kindNames()[(int) s.kind], inner.getX(), inner.getY(), 4.0f, lk::flame);
    lk::pixelText (g, String (seconds, 2) + " s", inner.getRight() - lk::pixelTextWidth (String (seconds, 2) + " S", 2.0f), inner.getY(), 2.0f, lk::boneDim);
    lk::pixelText (g, tomb::waveNames()[(int) s.wave], inner.getRight() - lk::pixelTextWidth (tomb::waveNames()[(int) s.wave], 2.0f), inner.getY() + 18.0f, 2.0f, lk::boneDim);
    ignoreUnused (name);

    const auto area = inner.withTrimmedTop (44.0f).withTrimmedBottom (20.0f);
    if (! peaks.empty())
    {
        const float cw = area.getWidth() / (float) peaks.size();
        const float pos = processor.previewPos.load();
        for (size_t c = 0; c < peaks.size(); ++c)
        {
            const float h = std::round (peaks[c] * area.getHeight() * 0.5f / 3.0f) * 3.0f;
            const float x = area.getX() + (float) c * cw;
            const bool played = pos >= 0.0f && (float) c / (float) peaks.size() < pos;
            g.setColour (played ? lk::flame : lk::poison.withAlpha (0.85f));
            g.fillRect (x, area.getCentreY() - h, jmax (1.0f, cw - 1.0f), h * 2.0f + 1.0f);
        }
    }
    if (message.isNotEmpty()) lk::pixelText (g, message, inner.getX(), inner.getBottom() - 14.0f, 2.0f, lk::bone);
}

//==============================================================================
SfxKnob::SfxKnob (const String& cap, float lo, float hi, float def, std::function<String (float)> t, Colour c)
    : caption (cap), text (std::move (t)), colour (c)
{
    slider.setRange (lo, hi);
    slider.setValue (def, dontSendNotification);
    slider.setDoubleClickReturnValue (true, def);
    slider.setSliderSnapsToMousePosition (false);
    slider.setAlpha (0.0f);
    slider.onValueChange = [this] { repaint(); if (onChange) onChange ((float) slider.getValue()); };
    addAndMakeVisible (slider);
}

void SfxKnob::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    lk::pixelText (g, caption, b.getX(), b.getY(), 2.0f, lk::boneDim);
    const String v = text ((float) slider.getValue());
    lk::pixelText (g, v, b.getRight() - lk::pixelTextWidth (v.toUpperCase(), 2.0f), b.getY(), 2.0f, colour.brighter (0.2f));
    const float f = (float) ((slider.getValue() - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum()));
    constexpr int blocks = 20;
    const float bw = b.getWidth() / (float) blocks;
    for (int i = 0; i < blocks; ++i)
    {
        const Rectangle<float> blk (b.getX() + (float) i * bw, b.getY() + 20.0f, bw - 2.0f, 12.0f);
        g.setColour (lk::ink);
        g.fillRect (blk);
        g.setColour (i < (int) std::round (f * blocks) ? colour.withMultipliedBrightness (0.55f + 0.45f * (float) i / blocks) : Colour (0xff1c1a24));
        g.fillRect (blk.reduced (1.0f));
    }
}

//==============================================================================
CryptEditor::CryptEditor (CryptProcessor& p) : processor (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (presetBar);
    for (auto* t : { &synthTab, &sfxTab })
    {
        t->setClickingTogglesState (false);
        t->textPx = 3.0f;
        addAndMakeVisible (*t);
    }
    synthTab.onClick = [this] { showTab (0); };
    sfxTab.onClick = [this] { showTab (1); };
    synthTab.setTooltip ("The synth: play dark chip music with the keys or your MIDI keyboard");
    sfxTab.setTooltip ("The sound-effect maker: swords, spells, doors, traps ... saved as .wav for the game");

    // ---- synth
    addAndMakeVisible (screen);
    synthParts.push_back (&screen);
    auto combo = [this] (ComboBox& c, const StringArray& items, const char* id, std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment>& att, const String& tip)
    {
        c.addItemList (items, 1);
        att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, id, c);
        c.setTooltip (tip);
        addAndMakeVisible (c);
        synthParts.push_back (&c);
    };
    combo (wave, tomb::waveNames(), "wave", waveA, "WAVE: NES pulses (thin 12%, 25%, square), the NES triangle, Game Boy wave tables (organ, bells, ghost), noise and metal noise");
    combo (chord, tomb::chordNames(), "chord", chordA, "CHORD: every key plays a dark chord (minor, diminished, power, phrygian = with the creepy minor second ...)");
    combo (arp, tomb::arpNames(), "arp", arpA, "ARP: the chord at once, or fast one note after another like old consoles (up, down, up & down, random)");
    combo (arpRate, tomb::arpRateNames(), "arpRate", rateA, "ARP RATE: how fast the arpeggio runs, with the song's tempo (1/64 = the buzzing NES chord)");
    stepsA = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "steps", steps);
    steps.setClickingTogglesState (true);
    steps.setTooltip ("16 STEPS: the volume moves in 16 steps like the real chips (off = smooth)");
    addAndMakeVisible (steps);
    synthParts.push_back (&steps);

    struct S { const char* id; const char* cap; Colour c; const char* tip; };
    const S defs[] = {
        { "octave", "Octave", lk::bone, "OCTAVE: everything higher or lower" },
        { "sub", "Sub", lk::violet, "SUB: a triangle one octave down (the NES bass)" },
        { "detune", "Detune", lk::violet, "DETUNE: a second voice a bit out of tune - wider, sicker" },
        { "sweep", "Sweep", lk::violet, "SWEEP: the pitch falls (or rises) into the note - right = kicks and lasers, left = rising" },
        { "vibrato", "Vibrato", lk::violet, "VIBRATO: a late wobble on long notes" },
        { "attack", "Attack", lk::ember, "ATTACK: how slowly a note comes in" },
        { "decay", "Decay", lk::ember, "DECAY: how fast it falls to the sustain level" },
        { "sustain", "Sustain", lk::ember, "SUSTAIN: the level while you hold the key (0 = plucks and drums)" },
        { "release", "Release", lk::ember, "RELEASE: how long it rings after you let go" },
        { "bits", "Bits", lk::poison, "BITS: fewer bits = dirtier, more 8-bit" },
        { "rate", "Rate", lk::poison, "RATE: a lower sample rate = a crunchy old-console sound" },
        { "drive", "Drive", lk::blood, "DRIVE: distortion" },
        { "gloom", "Gloom", lk::blood, "GLOOM: darker (a low-pass filter)" },
        { "crypt", "Crypt", lk::flame, "CRYPT: a big cold stone room" },
        { "echo", "Echo", lk::flame, "ECHO: a dotted echo with the song, darker every time it comes back" },
        { "output", "Output", lk::bone, "OUTPUT: the volume" },
    };
    for (auto& d : defs)
    {
        auto* s = sliders.add (new lk::PixelSlider (processor.apvts, d.id, d.cap, d.c));
        s->getSlider().setTooltip (d.tip);
        addAndMakeVisible (s);
        synthParts.push_back (s);
    }

    keys.setAvailableRange (24, 96);
    keys.setOctaveForMiddleC (4);
    keys.setColour (MidiKeyboardComponent::whiteNoteColourId, lk::bone.darker (0.15f));
    keys.setColour (MidiKeyboardComponent::blackNoteColourId, lk::ink);
    keys.setColour (MidiKeyboardComponent::keyDownOverlayColourId, lk::ember.withAlpha (0.8f));
    keys.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, lk::ember.withAlpha (0.25f));
    keys.setColour (MidiKeyboardComponent::keySeparatorLineColourId, lk::stone);
    keys.setColour (MidiKeyboardComponent::shadowColourId, Colours::transparentBlack);
    keys.setWantsKeyboardFocus (false);
    addAndMakeVisible (keys);
    synthParts.push_back (&keys);

    // ---- sound effects
    addAndMakeVisible (sfxScreen);
    sfxParts.push_back (&sfxScreen);
    const auto names = tomb::kindNames();
    for (int k = 0; k < names.size(); ++k)
    {
        auto* b = kinds.add (new lk::PixelButton (names[k], lk::bone));
        b->setTooltip (String (kindTips[k]) + " - click for a new one");
        b->onClick = [this, k]
        {
            processor.newSfx ((tomb::Kind) k);
            refreshSfx (true);
        };
        addAndMakeVisible (b);
        sfxParts.push_back (b);
    }
    generate.textPx = 3.0f;
    generate.setTooltip ("GENERATE: a new one of the same kind (every one is different)");
    generate.onClick = [this] { processor.newSfx (processor.sfx.kind); refreshSfx (true); };
    mutate.setTooltip ("MUTATE: the same sound, a little different");
    mutate.onClick = [this] { processor.mutateSfx(); refreshSfx (true); };
    play.setTooltip ("PLAY it again");
    play.onClick = [this] { refreshSfx (true); };
    back.setTooltip ("BACK: the one before");
    back.onClick = [this]
    {
        if (processor.history.empty()) return;
        processor.sfx = processor.history.front();
        processor.history.erase (processor.history.begin());
        refreshSfx (true);
    };
    saveGame.setTooltip ("SAVE TO GAME: saves it into the game folder as sword_01.wav, sword_02.wav ... (44.1 kHz, 16 bit, mono)");
    saveGame.onClick = [this] { saveToGame(); };
    saveAsB.setTooltip ("SAVE AS: pick a name and a place");
    saveAsB.onClick = [this] { saveAs(); };
    folderB.setTooltip ("FOLDER: where SAVE TO GAME puts the files (e.g. your game's sounds folder)");
    folderB.onClick = [this] { chooseFolder(); };
    for (auto* b : { &generate, &mutate, &play, &back, &saveGame, &saveAsB, &folderB })
    {
        addAndMakeVisible (*b);
        sfxParts.push_back (b);
    }
    darkness.slider.setValue (processor.sfxDarkness, dontSendNotification);
    pitch.slider.setValue (processor.sfxPitch, dontSendNotification);
    length.slider.setValue (processor.sfxLength, dontSendNotification);
    darkness.setTooltip ("DARKNESS: new sounds come out lower, darker, with more stone around them");
    pitch.setTooltip ("PITCH: the sound higher or lower");
    length.setTooltip ("LENGTH: the sound shorter or longer");
    darkness.onChange = [this] (float v) { processor.sfxDarkness = v; };
    pitch.onChange = [this] (float v) { processor.sfxPitch = v; };
    length.onChange = [this] (float v) { processor.sfxLength = v; };
    pitch.slider.onDragEnd = [this] { refreshSfx (true); };
    length.slider.onDragEnd = [this] { refreshSfx (true); };
    for (auto* k : { &darkness, &pitch, &length })
    {
        addAndMakeVisible (*k);
        sfxParts.push_back (k);
    }
    rebuildHistory();
    refreshSfx (false);

    setSize (designW, designH);
    showTab (processor.tab);
    startTimerHz (30);
}

CryptEditor::~CryptEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CryptEditor::showTab (int t)
{
    processor.tab = t;
    synthTab.setToggleState (t == 0, dontSendNotification);
    sfxTab.setToggleState (t == 1, dontSendNotification);
    for (auto* c : synthParts) c->setVisible (t == 0);
    for (auto* c : sfxParts) c->setVisible (t == 1);
    presetBar.setVisible (t == 0);
    for (auto* b : historyButtons) b->setVisible (t == 1);
    for (int k = 0; k < kinds.size(); ++k) kinds[k]->setToggleState (k == (int) processor.sfx.kind, dontSendNotification);
    repaint();
}

void CryptEditor::refreshSfx (bool playIt)
{
    const auto shown = processor.renderSfx (44100.0);
    sfxScreen.setSound (shown, 44100.0);
    sfxScreen.message = {};
    if (playIt)
    {
        const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;
        processor.playPreview (std::abs (sr - 44100.0) < 1.0 ? shown : processor.renderSfx (sr));
    }
    for (int k = 0; k < kinds.size(); ++k) kinds[k]->setToggleState (k == (int) processor.sfx.kind, dontSendNotification);
    rebuildHistory();
}

void CryptEditor::rebuildHistory()
{
    historyButtons.clear();
    for (size_t i = 0; i < processor.history.size() && i < 6; ++i)
    {
        const auto s = processor.history[i];
        auto* b = historyButtons.add (new lk::PixelButton (tomb::kindNames()[(int) s.kind], lk::boneDim));
        b->setTooltip ("An earlier one: click to get it back");
        b->onClick = [this, i]
        {
            if (i >= processor.history.size()) return;
            std::swap (processor.sfx, processor.history[i]);
            refreshSfx (true);
        };
        addChildComponent (b);
        b->setVisible (processor.tab == 1);
    }
    resized();
}

void CryptEditor::saveToGame()
{
    const auto f = processor.saveSfx();
    sfxScreen.message = f.existsAsFile() ? "Saved: " + f.getFileName() : String ("Could not save - pick a folder");
    sfxScreen.repaint();
}

void CryptEditor::saveAs()
{
    const auto start = processor.sfxFolder.getChildFile (tomb::kindFileName (processor.sfx.kind) + ".wav");
    chooser = std::make_unique<FileChooser> ("Save the sound", start, "*.wav");
    SafePointer<CryptEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == File()) return;
        const auto f = safe->processor.saveSfx (fc.getResult().withFileExtension ("wav"));
        safe->sfxScreen.message = f.existsAsFile() ? "Saved: " + f.getFileName() : String ("Could not save");
        safe->sfxScreen.repaint();
    });
}

void CryptEditor::chooseFolder()
{
    chooser = std::make_unique<FileChooser> ("Where should SAVE TO GAME put the sounds?", processor.sfxFolder);
    SafePointer<CryptEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [safe] (const FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == File()) return;
        safe->processor.sfxFolder = fc.getResult();
        safe->repaint();
    });
}

void CryptEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float light = 0.8f + 0.2f * std::sin (time * 7.0f) * std::sin (time * 3.1f);
    lk::wall (g, b, light);
    // the darkness in the middle
    ColourGradient shade (Colours::black.withAlpha (0.0f), b.getCentreX(), b.getCentreY(), Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
    g.setGradientFill (shade);
    g.fillRect (b);
    lk::torch (g, { 50.0f, 64.0f }, time);
    lk::torch (g, { b.getWidth() - 50.0f, 64.0f }, time + 1.7f);

    lk::pixelText (g, "Crypt", 92.0f, 22.0f, 6.0f, lk::bone);
    lk::pixelText (g, "dark chip music + sound effects", 96.0f, 72.0f, 2.0f, lk::boneDim);

    if (processor.tab == 0)
    {
        const float colX[] = { 40.0f, 275.0f, 510.0f, 745.0f };
        const char* heads[] = { "Voice", "Envelope", "Chord", "Crypt" };
        const Colour hc[] = { lk::violet, lk::ember, lk::flame, lk::poison };
        for (int i = 0; i < 4; ++i)
        {
            lk::panel (g, { colX[i] - 10.0f, 282.0f, 225.0f, 300.0f }, 3.0f);
            lk::pixelText (g, heads[i], colX[i], 292.0f, 3.0f, hc[i]);
        }
        lk::pixelText (g, "wave", 40.0f, 318.0f, 2.0f, lk::boneDim);
        lk::pixelText (g, "chord", 510.0f, 318.0f, 2.0f, lk::boneDim);
        lk::pixelText (g, "play", 510.0f, 368.0f, 2.0f, lk::boneDim);
        lk::pixelText (g, "speed", 510.0f, 418.0f, 2.0f, lk::boneDim);
        lk::pixelText (g, "one key = a dark chord.", 510.0f, 486.0f, 2.0f, lk::boneDim.withAlpha (0.7f));
        lk::pixelText (g, "1/64 + up = the old", 510.0f, 506.0f, 2.0f, lk::boneDim.withAlpha (0.7f));
        lk::pixelText (g, "buzzing console chord", 510.0f, 526.0f, 2.0f, lk::boneDim.withAlpha (0.7f));
    }
    else
    {
        lk::panel (g, { 30.0f, 128.0f, 270.0f, 470.0f }, 3.0f);
        lk::pixelText (g, "What is it?", 44.0f, 140.0f, 3.0f, lk::flame);
        lk::panel (g, { 320.0f, 444.0f, 650.0f, 154.0f }, 3.0f);
        String folder = processor.sfxFolder.getFullPathName();
        if (folder.length() > 50) folder = "..." + folder.getLastCharacters (47);
        lk::pixelText (g, "Game folder:", 334.0f, 534.0f, 2.0f, lk::boneDim);
        g.setColour (lk::bone);
        g.setFont (FontOptions (13.0f));
        g.drawText (folder, 420, 529, 540, 18, Justification::centredLeft);
        lk::pixelText (g, "Before:", 334.0f, 566.0f, 2.0f, lk::boneDim);
        lk::pixelText (g, "files: 44.1 kHz 16 bit mono .wav", 334.0f, 612.0f, 2.0f, lk::boneDim.withAlpha (0.7f));
    }
}

void CryptEditor::resized()
{
    presetBar.setBounds (getWidth() - 400, 30, 300, 34);
    synthTab.setBounds (92, 92, 130, 30);
    sfxTab.setBounds (228, 92, 150, 30);

    // synth
    screen.setBounds (30, 130, 940, 142);
    wave.setBounds (40, 332, 205, 26);
    auto sl = [this] (int i) { return sliders[i]; };
    int y = 366;
    for (int i : { 0, 1, 2, 3, 4 }) { sl (i)->setBounds (40, y, 205, 36); y += 42; }
    y = 322;
    for (int i : { 5, 6, 7, 8 }) { sl (i)->setBounds (275, y, 205, 36); y += 46; }
    steps.setBounds (275, y + 4, 205, 28);
    chord.setBounds (510, 332, 205, 26);
    arp.setBounds (510, 382, 205, 26);
    arpRate.setBounds (510, 432, 205, 26);
    y = 322;
    for (int i : { 9, 10, 11, 12, 13, 14, 15 }) { sl (i)->setBounds (745, y, 205, 34); y += 37; }
    keys.setBounds (30, 592, 940, 58);
    keys.setKeyWidth (940.0f / 36.0f);
    keys.setLowestVisibleKey (36);

    // sound effects
    for (int k = 0; k < kinds.size(); ++k)
        kinds[k]->setBounds (44 + (k % 2) * 124, 172 + (k / 2) * 44, 118, 36);
    darkness.setBounds (44, 490, 242, 34);
    pitch.setBounds (44, 528, 242, 34);
    length.setBounds (44, 562, 242, 34);
    sfxScreen.setBounds (320, 128, 650, 230);
    generate.setBounds (320, 370, 210, 50);
    mutate.setBounds (540, 370, 130, 50);
    play.setBounds (680, 370, 140, 50);
    back.setBounds (830, 370, 140, 50);
    saveGame.setBounds (334, 456, 240, 42);
    saveAsB.setBounds (584, 456, 180, 42);
    folderB.setBounds (774, 456, 180, 42);
    for (int i = 0; i < historyButtons.size(); ++i) historyButtons[i]->setBounds (420 + i * 92, 558, 88, 26);
}

void CryptEditor::timerCallback()
{
    const auto now = Time::getMillisecondCounter();
    const float dt = lastTick == 0 ? 1.0f / 30.0f : jlimit (0.005f, 0.1f, (float) (now - lastTick) / 1000.0f);
    lastTick = now;
    time += dt;
    screen.time = time;
    screen.jaw += (jlimit (0.0f, 1.0f, processor.level.load() * 3.0f) - screen.jaw) * jmin (1.0f, dt * 20.0f);
    if (processor.tab == 0) screen.repaint();
    else if (processor.previewPos.load() >= 0.0f) sfxScreen.repaint();
    repaint (0, 0, 110, 130);
    repaint (getWidth() - 110, 0, 110, 130);
}

#include "PluginEditor.h"
#include "CatTheme.h"
#include "MidiExport.h"

using namespace juce;

namespace
{
constexpr int designW = 1200, designH = 800;
const Colour partColours[humcat::NumParts] = { Colour (0xffffb35c), Colour (0xff6fc3ff), Colour (0xffff6f4f), Colour (0xffc7a6ff) };
const Colour rose (0xffff6f4f), cream (0xfff3e9e2);

String noteName (int midi)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return String (n[((midi % 12) + 12) % 12]) + String (midi / 12 - 2);   // Cubase: C3 = 60
}

void glassWell (Graphics& g, Rectangle<float> r, float corner = 14.0f)
{
    g.setColour (Colour (0xcc0c0a0e));
    g.fillRoundedRectangle (r, corner);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (1.5f), corner - 1.0f, 1.0f);
}
}

//==============================================================================
void RecButton::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (6.0f);
    const float d = jmin (b.getWidth(), b.getHeight());
    auto r = b.withSizeKeepingCentre (d, d);
    const bool on = processor.isRecording();
    const float level = jlimit (0.0f, 1.0f, std::sqrt (processor.inputLevel.load()) * 1.2f);

    // metal ring
    ColourGradient ring (Colour (0xff5a5260), r.getX(), r.getY(), Colour (0xff151217), r.getRight(), r.getBottom(), false);
    g.setGradientFill (ring);
    g.fillEllipse (r);
    // the input level as a glowing arc round it
    {
        Path arc;
        const float a0 = -MathConstants<float>::pi * 0.8f;
        arc.addCentredArc (r.getCentreX(), r.getCentreY(), d * 0.47f, d * 0.47f, 0.0f, a0, a0 + MathConstants<float>::pi * 1.6f, true);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.strokePath (arc, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
        if (level > 0.01f)
        {
            Path lv;
            lv.addCentredArc (r.getCentreX(), r.getCentreY(), d * 0.47f, d * 0.47f, 0.0f, a0, a0 + MathConstants<float>::pi * 1.6f * level, true);
            g.setColour ((level > 0.9f ? Colour (0xffff3030) : Colour (0xff7dffa0)).withAlpha (0.9f));
            g.strokePath (lv, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }
    auto inner = r.reduced (d * 0.12f);
    if (on)
    {
        g.setColour (rose.withAlpha (0.35f));
        g.fillEllipse (inner.expanded (8.0f));
    }
    ColourGradient body (on ? Colour (0xffff8a6a) : Colour (0xff8f3a32), inner.getCentreX(), inner.getY(),
                         on ? Colour (0xffc22a20) : Colour (0xff3e1612), inner.getCentreX(), inner.getBottom(), false);
    g.setGradientFill (body);
    g.fillEllipse (inner);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.fillEllipse (inner.reduced (inner.getWidth() * 0.18f).withHeight (inner.getHeight() * 0.3f).translated (0.0f, -inner.getHeight() * 0.12f));

    g.setColour (on ? Colours::white : cream.withAlpha (0.9f));
    if (on)
        g.fillRoundedRectangle (inner.withSizeKeepingCentre (d * 0.2f, d * 0.2f), 4.0f);
    else
        g.fillEllipse (inner.withSizeKeepingCentre (d * 0.14f, d * 0.14f).translated (0.0f, -d * 0.08f));
    g.setFont (cat::displayFont (15.0f));
    g.drawText (on ? "STOP" : "REC", inner.withTrimmedTop (inner.getHeight() * 0.58f).withHeight (20.0f), Justification::centred);
}

void RecButton::mouseUp (const MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()) && onToggle) onToggle();
}

//==============================================================================
void SongView::refresh()
{
    track = processor.getPitchTrack();
    song = processor.getSong();
    sung = processor.getSung();
    repaint();
}

void SongView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    glassWell (g, b, 16.0f);
    b = b.reduced (16.0f, 12.0f);
    auto heard = b.removeFromTop (b.getHeight() * 0.36f);
    b.removeFromTop (10.0f);
    paintHeard (g, heard);
    paintSong (g, b);

    if (dropHover)
    {
        g.setColour (rose.withAlpha (0.18f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (4.0f), 14.0f);
        g.setColour (rose);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (4.0f), 14.0f, 2.0f);
        g.setFont (cat::displayFont (26.0f));
        g.drawText ("Drop it - Hum-Cat listens", getLocalBounds(), Justification::centred);
    }
    else if (processor.isAnalysing())
    {
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 14.0f);
        g.setColour (cream);
        g.setFont (cat::displayFont (24.0f));
        g.drawText ("Listening ...", getLocalBounds(), Justification::centred);
    }
}

void SongView::paintHeard (Graphics& g, Rectangle<float> r)
{
    g.setColour (cream.withAlpha (0.55f));
    g.setFont (cat::catFont (11.5f));
    g.drawText ("WHAT YOU SANG", r.removeFromTop (16.0f), Justification::centredLeft);
    auto plot = r.reduced (0.0f, 2.0f);
    g.setColour (Colours::white.withAlpha (0.04f));
    g.fillRoundedRectangle (plot, 8.0f);

    if (track == nullptr || track->midi.empty())
    {
        g.setColour (cream.withAlpha (0.4f));
        g.setFont (cat::catFont (14.0f));
        g.drawText ("Press REC and sing, hum or whistle a tune - or drop a recording here", plot, Justification::centred);
        return;
    }
    // range of the voice
    float lo = 127.0f, hi = 0.0f;
    for (float m : track->midi) if (m > 0.0f) { lo = jmin (lo, m); hi = jmax (hi, m); }
    if (hi < lo) { lo = 55.0f; hi = 72.0f; }
    lo = std::floor (lo) - 2.0f; hi = std::ceil (hi) + 2.0f;
    const double total = track->midi.size() * track->hopSeconds;
    auto xOf = [&] (double t) { return plot.getX() + (float) (t / total) * plot.getWidth(); };
    auto yOf = [&] (float m) { return plot.getBottom() - (m - lo) / (hi - lo) * plot.getHeight(); };

    // loudness as a soft floor
    {
        Path lv;
        lv.startNewSubPath (plot.getX(), plot.getBottom());
        const int step = jmax (1, (int) track->level.size() / 600);
        for (size_t i = 0; i < track->level.size(); i += (size_t) step)
            lv.lineTo (xOf (i * track->hopSeconds), plot.getBottom() - track->level[i] * plot.getHeight() * 0.35f);
        lv.lineTo (plot.getRight(), plot.getBottom());
        lv.closeSubPath();
        g.setColour (rose.withAlpha (0.10f));
        g.fillPath (lv);
    }
    // notes heard
    for (const auto& n : sung)
    {
        auto pill = Rectangle<float> (xOf (n.start), yOf ((float) n.pitch + 0.5f), jmax (3.0f, xOf (n.start + n.length) - xOf (n.start)), plot.getHeight() / (hi - lo));
        g.setColour (cream.withAlpha (0.16f));
        g.fillRoundedRectangle (pill, 3.0f);
    }
    // the pitch curve
    Path curve;
    bool open = false;
    for (size_t i = 0; i < track->midi.size(); ++i)
    {
        const float m = track->midi[i];
        if (m <= 0.0f) { open = false; continue; }
        const float x = xOf (i * track->hopSeconds), y = yOf (m);
        if (! open) { curve.startNewSubPath (x, y); open = true; }
        else curve.lineTo (x, y);
    }
    g.setColour (rose.withAlpha (0.3f));
    g.strokePath (curve, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xffffc2ae));
    g.strokePath (curve, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (cream.withAlpha (0.5f));
    g.setFont (cat::catFont (11.0f));
    g.drawText (noteName ((int) hi - 1), plot.reduced (6.0f, 2.0f), Justification::topRight);
    g.drawText (noteName ((int) lo + 1), plot.reduced (6.0f, 2.0f), Justification::bottomRight);
    g.drawText (String (total, 1) + " s  -  " + String ((int) sung.size()) + " notes", plot.reduced (6.0f, 2.0f), Justification::bottomLeft);
}

void SongView::paintSong (Graphics& g, Rectangle<float> r)
{
    auto title = r.removeFromTop (18.0f);
    g.setFont (cat::catFont (11.5f));
    g.setColour (cream.withAlpha (0.55f));
    g.drawText ("THE SONG", title, Justification::centredLeft);
    if (song == nullptr || song->parts[humcat::Lead].empty())
    {
        g.setColour (Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (cream.withAlpha (0.35f));
        g.setFont (cat::catFont (14.0f));
        g.drawText ("Lead, chords, bass and drums appear here", r, Justification::centred);
        return;
    }
    g.setColour (rose);
    g.drawText (humcat::keyName (song->key) + (song->scale == 1 ? " minor" : " major") + "   " + String (roundToInt (song->bpm)) + " BPM   "
                + String (roundToInt (song->lengthBeats / 4.0)) + " bars", title, Justification::centredRight);

    const auto on = processor.partsOn();
    const double len = jmax (4.0, song->lengthBeats);
    auto xOf = [&] (double beat) { return r.getX() + (float) (beat / len) * r.getWidth(); };

    // bar lines
    for (int bar = 0; bar <= (int) (len / 4.0); ++bar)
    {
        g.setColour (Colours::white.withAlpha (bar % 4 == 0 ? 0.14f : 0.06f));
        g.drawVerticalLine (roundToInt (xOf (bar * 4.0)), r.getY(), r.getBottom());
    }

    auto chordRow = r.removeFromTop (30.0f);
    r.removeFromTop (4.0f);
    auto drumRow = r.removeFromBottom (34.0f);
    r.removeFromBottom (4.0f);
    auto bassRow = r.removeFromBottom (r.getHeight() * 0.28f);
    r.removeFromBottom (4.0f);
    auto leadRow = r;

    // chord names
    for (const auto& c : song->chords)
    {
        auto cell = Rectangle<float> (xOf (c.start), chordRow.getY(), xOf (c.start + c.length) - xOf (c.start), chordRow.getHeight()).reduced (2.0f, 2.0f);
        g.setColour (partColours[humcat::Chords].withAlpha (on[humcat::Chords] ? 0.22f : 0.07f));
        g.fillRoundedRectangle (cell, 6.0f);
        g.setColour (partColours[humcat::Chords].withAlpha (on[humcat::Chords] ? 1.0f : 0.4f));
        g.setFont (cat::displayFont (cell.getWidth() > 50.0f ? 16.0f : 12.0f));
        g.drawText (song->chordName (c), cell, Justification::centred);
    }

    auto roll = [&] (Rectangle<float> row, int part, int pad)
    {
        const auto& notes = song->parts[(size_t) part];
        if (notes.empty()) return;
        int lo = 127, hi = 0;
        for (const auto& n : notes) { lo = jmin (lo, n.pitch); hi = jmax (hi, n.pitch); }
        lo -= pad; hi += pad;
        const float h = row.getHeight() / (float) (hi - lo + 1);
        const auto c = partColours[part].withAlpha (on[(size_t) part] ? 1.0f : 0.3f);
        for (const auto& n : notes)
        {
            auto pill = Rectangle<float> (xOf (n.start), row.getBottom() - (n.pitch - lo + 1) * h, jmax (2.0f, xOf (n.start + n.length) - xOf (n.start) - 1.0f), jmax (2.0f, h - 1.0f));
            g.setColour (c.withAlpha (c.getFloatAlpha() * (0.45f + 0.55f * n.velocity)));
            g.fillRoundedRectangle (pill, jmin (3.0f, pill.getHeight() * 0.5f));
        }
    };
    g.setColour (Colours::white.withAlpha (0.03f));
    g.fillRoundedRectangle (leadRow, 6.0f);
    roll (leadRow, humcat::Lead, 2);
    // the chord tones as a faint backdrop behind the lead
    g.setColour (Colours::white.withAlpha (0.03f));
    g.fillRoundedRectangle (bassRow, 6.0f);
    roll (bassRow, humcat::Bass, 1);

    // drums: hits as dots in three rows (kick, snare, hats / rest)
    g.setColour (Colours::white.withAlpha (0.03f));
    g.fillRoundedRectangle (drumRow, 6.0f);
    for (const auto& n : song->parts[humcat::Drums])
    {
        const int lane = (n.pitch == 36 || n.pitch == 35) ? 2 : (n.pitch == 38 || n.pitch == 40 || n.pitch == 39) ? 1 : 0;
        const float y = drumRow.getY() + drumRow.getHeight() * (0.2f + 0.3f * lane);
        g.setColour (partColours[humcat::Drums].withAlpha ((on[humcat::Drums] ? 0.9f : 0.3f) * (0.4f + 0.6f * n.velocity)));
        g.fillEllipse (xOf (n.start) - 2.0f, y - 2.0f, 4.0f, 4.0f);
    }

    // labels
    g.setFont (cat::catFont (11.0f));
    auto label = [&] (Rectangle<float> row, int part)
    {
        g.setColour (partColours[part].withAlpha (0.8f));
        g.drawText (humcat::partNames()[part].toUpperCase(), row.reduced (6.0f, 2.0f), Justification::topLeft);
    };
    label (leadRow, humcat::Lead); label (bassRow, humcat::Bass); label (drumRow, humcat::Drums);

    // the play head
    const double pos = processor.playPosition.load();
    if (pos >= 0.0)
    {
        const float x = xOf (pos);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.drawLine (x, chordRow.getY(), x, drumRow.getBottom(), 1.5f);
        g.setColour (rose.withAlpha (0.25f));
        g.fillRect (x - 4.0f, chordRow.getY(), 8.0f, drumRow.getBottom() - chordRow.getY());
    }
}

//==============================================================================
HumCatEditor::HumCatEditor (HumCatProcessor& p) : processor (p)
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (rec);
    rec.onToggle = [this]
    {
        if (processor.isRecording()) { processor.stopRecording(); status = "Listening to what you sang ..."; }
        else { processor.previewPlaying = false; processor.startRecording(); status = "Recording - sing!"; }
        rec.repaint();
    };
    addAndMakeVisible (view);
    view.setTooltip ("Top: the pitch it heard. Bottom: the song it wrote. Drop a wav / mp3 / aiff / flac here");

    for (auto* b : { &loadButton, &clearButton, &demoButton, &playButton, &dragSong }) addAndMakeVisible (*b);
    loadButton.onClick = [this] { chooseFile(); };
    clearButton.onClick = [this] { processor.previewPlaying = false; processor.clear(); status = "Cleared"; };
    demoButton.setTooltip ("No mic at hand? A sung demo tune to try it with");
    demoButton.onClick = [this] { processor.previewPlaying = false; processor.loadDemo(); status = "Listening to the demo tune ..."; };
    playButton.setClickingTogglesState (true);
    playButton.setTooltip ("Play the song here (without Cubase playing)");
    playButton.onClick = [this] { processor.previewPlaying = playButton.getToggleState(); };
    dragSong.setTooltip ("Drag onto Cubase's project: every part on its own MIDI track");
    dragSong.onClick = [] {};
    dragSong.addMouseListener (this, false);
    dragSong.setColour (TextButton::buttonColourId, rose.withAlpha (0.85f));

    const char* soundIds[humcat::NumParts] = { "chordSound", nullptr, "leadSound", "kit" };
    for (int i = 0; i < humcat::NumParts; ++i)
    {
        auto& s = parts[(size_t) i];
        s.on = std::make_unique<cat::ParamToggle> (processor.apvts, "out" + humcat::partNames()[i], humcat::partNames()[i]);
        s.on->setColour (TextButton::buttonOnColourId, partColours[i]);
        s.on->setColour (TextButton::buttonColourId, partColours[i]);
        s.on->setColour (TextButton::textColourOnId, Colour (0xff1a1418));
        s.on->setColour (TextButton::textColourOffId, partColours[i]);
        addAndMakeVisible (*s.on);
        s.drag = std::make_unique<TextButton> ("Drag");
        s.drag->setTooltip ("Drag only the " + humcat::partNames()[i] + " into Cubase");
        s.drag->addMouseListener (this, false);
        s.drag->getProperties().set ("part", i);
        addAndMakeVisible (*s.drag);
        if (soundIds[i] != nullptr)
        {
            s.sound = std::make_unique<cat::ParamCombo> (processor.apvts, soundIds[i]);
            addAndMakeVisible (*s.sound);
        }
    }
    dragSong.getProperties().set ("part", -1);

    for (auto* c : { &style, &chordRate, &key, &scale, &grid }) addAndMakeVisible (*c);
    for (auto* t : { &snap, &trim, &withHost, &ownSound }) addAndMakeVisible (*t);
    for (auto* k : { &sensitivity, &leadOct, &tempo, &volume }) addAndMakeVisible (*k);
    sensitivity.getSlider().setTooltip ("How hard it listens: higher hears quieter, breathier singing (and more noise)");
    tempo.getSlider().setTooltip ("Tempo when Cubase doesn't send one (in Cubase it follows the project tempo)");
    trim.setTooltip ("On: your first note is bar 1. Off: it stays where you sang it on Cubase's timeline");

    setSize (designW, designH);
    view.refresh();
    shownVersion = processor.songVersion.load();
    startTimerHz (30);
}

HumCatEditor::~HumCatEditor()
{
    for (auto& s : parts) if (s.drag) s.drag->removeMouseListener (this);
    dragSong.removeMouseListener (this);
}

void HumCatEditor::paint (Graphics& g)
{
    cat::paintBackdrop (g, getLocalBounds());

    // logo: hum·cat in a warm neon
    auto logo = header.toFloat().withWidth (400.0f);
    g.setFont (cat::displayFont (40.0f));
    const String text = String::fromUTF8 ("hum\xc2\xb7" "cat");
    g.setColour (rose.withAlpha (0.25f));
    for (auto d : { Point<float> (-1.5f, 0.0f), Point<float> (1.5f, 0.0f), Point<float> (0.0f, 1.5f), Point<float> (0.0f, -1.5f) })
        g.drawText (text, logo.translated (d.x, d.y), Justification::centredLeft);
    g.setColour (cream);
    g.drawText (text, logo, Justification::centredLeft);
    g.setFont (cat::catFont (12.0f));
    g.setColour (cream.withAlpha (0.55f));
    g.drawText ("sing it  -  get a song", logo.withTrimmedLeft (236.0f).withTrimmedTop (12.0f), Justification::centredLeft);

    // mic panel
    auto mic = micArea.toFloat();
    glassWell (g, mic, 16.0f);
    g.setColour (cream.withAlpha (0.6f));
    g.setFont (cat::catFont (12.0f));
    const float secs = processor.isRecording() ? processor.recordedSeconds() : 0.0f;
    g.setFont (cat::lcdFont (34.0f));
    g.setColour (processor.isRecording() ? rose : cream.withAlpha (0.7f));
    const int whole = (int) secs;
    g.drawText (String::formatted ("%02d:%02d", whole / 60, whole % 60), mic.withTrimmedTop (232.0f).withHeight (36.0f), Justification::centred);
    g.setFont (cat::catFont (11.5f));
    g.setColour (cream.withAlpha (0.45f));
    g.drawFittedText ("Mic on this audio track,\nmonitoring on. Max 3 min.", mic.withTrimmedTop (268.0f).withHeight (34.0f).toNearestInt(), Justification::centred, 2);

    // part + settings panels
    for (auto r : { partsArea, settingsArea })
    {
        g.setColour (Colour (0xb0141116));
        g.fillRoundedRectangle (r.toFloat(), 14.0f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 14.0f, 1.0f);
    }
    g.setFont (cat::catFont (11.0f));
    g.setColour (cream.withAlpha (0.5f));
    g.drawText ("PARTS", partsArea.reduced (14, 8), Justification::topLeft);
    g.drawText ("HOW IT WRITES", settingsArea.reduced (14, 8), Justification::topLeft);

    // status line
    g.setColour (cream.withAlpha (0.7f));
    g.setFont (cat::catFont (12.5f));
    g.drawText (status, statusArea, Justification::centredLeft);
}

void HumCatEditor::resized()
{
    auto r = getLocalBounds().reduced (22, 16);
    header = r.removeFromTop (54);
    presetBar.setBounds (header.withTrimmedLeft (420).withWidth (380).withSizeKeepingCentre (380, 34));
    r.removeFromTop (10);

    auto top = r.removeFromTop (420);
    micArea = top.removeFromLeft (240);
    top.removeFromLeft (14);
    view.setBounds (top);
    {
        auto m = micArea.reduced (16);
        rec.setBounds (m.removeFromTop (210));
        m.removeFromTop (100);
        loadButton.setBounds (m.removeFromTop (34));
        m.removeFromTop (8);
        auto row = m.removeFromTop (30);
        clearButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4));
        demoButton.setBounds (row.withTrimmedLeft (8));
    }

    r.removeFromTop (14);
    auto bottomBar = r.removeFromBottom (40);
    r.removeFromBottom (12);
    partsArea = r.removeFromLeft (480);
    r.removeFromLeft (14);
    settingsArea = r;

    {
        auto pa = partsArea.reduced (14, 10);
        pa.removeFromTop (20);
        const int w = pa.getWidth() / humcat::NumParts;
        for (int i = 0; i < humcat::NumParts; ++i)
        {
            auto col = pa.removeFromLeft (w).reduced (5, 0);
            auto& s = parts[(size_t) i];
            s.on->setBounds (col.removeFromTop (54));
            col.removeFromTop (14);
            if (s.sound) s.sound->setBounds (col.removeFromTop (32));
            else col.removeFromTop (32);
            col.removeFromTop (14);
            s.drag->setBounds (col.removeFromTop (34));
        }
    }
    {
        auto sa = settingsArea.reduced (14, 10);
        sa.removeFromTop (20);
        auto row1 = sa.removeFromTop (30);
        const int cw = (row1.getWidth() - 4 * 8) / 5;
        for (auto* c : { &style, &chordRate, &key, &scale, &grid })
        {
            c->setBounds (row1.removeFromLeft (cw));
            row1.removeFromLeft (8);
        }
        sa.removeFromTop (10);
        auto row2 = sa;
        auto toggles = row2.removeFromLeft (180);
        const int th = (toggles.getHeight() - 3 * 6) / 4;
        for (auto* t : { &snap, &trim, &withHost, &ownSound })
        {
            t->setBounds (toggles.removeFromTop (th));
            toggles.removeFromTop (6);
        }
        row2.removeFromLeft (12);
        const int kw = row2.getWidth() / 4;
        for (auto* k : { &sensitivity, &leadOct, &tempo, &volume })
            k->setBounds (row2.removeFromLeft (kw).reduced (4, 0));
    }

    playButton.setBounds (bottomBar.removeFromLeft (140));
    bottomBar.removeFromLeft (12);
    dragSong.setBounds (bottomBar.removeFromRight (260));
    statusArea = bottomBar.reduced (8, 0);
}

void HumCatEditor::timerCallback()
{
    rec.repaint();
    const auto v = processor.songVersion.load();
    const bool an = processor.isAnalysing();
    if (v != shownVersion || an != wasAnalysing)
    {
        shownVersion = v;
        view.refresh();
        if (wasAnalysing && ! an)
        {
            const auto sung = processor.getSung();
            status = sung.empty() ? "Didn't hear a tune - try singing louder or turn Ears up"
                                  : "Heard " + String ((int) sung.size()) + " notes. Press Play, or drag it into Cubase";
        }
        wasAnalysing = an;
    }
    if (processor.playPosition.load() >= 0.0 || processor.isAnalysing()) view.repaint();
    if (processor.isRecording())
    {
        repaint (micArea);
        if (processor.recordedSeconds() >= (float) HumCatProcessor::maxRecordSeconds - 0.2f)
        {
            processor.stopRecording();
            status = "3 minutes is the limit - listening now";
        }
    }
    if (playButton.getToggleState() != processor.previewPlaying.load())
        playButton.setToggleState (processor.previewPlaying.load(), dontSendNotification);
    repaint (statusArea);
}

//==============================================================================
bool HumCatEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (const auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void HumCatEditor::filesDropped (const StringArray& files, int, int)
{
    view.dropHover = false;
    for (const auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) { loadFile (File (f)); break; }
    view.repaint();
}

void HumCatEditor::chooseFile()
{
    chooser = std::make_unique<FileChooser> ("A recording of your singing", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    SafePointer<HumCatEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [safe] (const FileChooser& fc)
    {
        if (safe != nullptr && fc.getResult().existsAsFile()) safe->loadFile (fc.getResult());
    });
}

void HumCatEditor::loadFile (const File& f)
{
    String error;
    processor.previewPlaying = false;
    if (processor.loadFile (f, error)) status = "Listening to " + f.getFileName() + " ...";
    else status = error;
}

void HumCatEditor::dragOut (int part)
{
    auto s = processor.getSong();
    if (s == nullptr || s->parts[humcat::Lead].empty()) { status = "Sing something first"; return; }
    std::array<bool, humcat::NumParts> which {};
    if (part < 0) which = processor.partsOn();
    else which[(size_t) part] = true;
    auto midi = humcat::toMidiFile (*s, which);
    const String name = part < 0 ? String ("Hum-Cat Song") : "Hum-Cat " + humcat::partNames()[part];
    auto file = File::getSpecialLocation (File::tempDirectory).getChildFile (name + ".mid");
    file.deleteFile();
    {
        FileOutputStream out (file);
        if (! out.openedOk()) { status = "Couldn't write the MIDI file"; return; }
        midi.writeTo (out, 1);
    }
    cat::midiexport::dragFileOut (file, this);
}

// drag buttons: a drag that leaves the button starts the OS drag
void HumCatEditor::mouseDrag (const MouseEvent& e)
{
    if (auto* b = dynamic_cast<TextButton*> (e.eventComponent))
        if (b->getProperties().contains ("part") && e.getDistanceFromDragStart() > 6 && ! dragging)
        {
            dragging = true;
            dragOut ((int) b->getProperties()["part"]);
        }
}

void HumCatEditor::mouseUp (const MouseEvent& e)
{
    if (! dragging)
        if (auto* b = dynamic_cast<TextButton*> (e.eventComponent))
            if (b->getProperties().contains ("part") && b->getLocalBounds().contains (e.getPosition()))
                status = "Hold the button and drag it onto Cubase";
    dragging = false;
}

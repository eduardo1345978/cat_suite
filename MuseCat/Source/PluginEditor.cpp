#include "PluginEditor.h"
#include "MidiExport.h"
#include "Arranger.h"

using namespace cat::muse;

namespace
{
    constexpr int designW = 1160, designH = 760;

    juce::Colour sectionColour (int type)
    {
        static const float hues[] = { 0.58f, 0.52f, 0.12f, 0.95f, 0.75f, 0.08f, 0.0f, 0.62f, 0.55f };
        return juce::Colour::fromHSV (hues[juce::jlimit (0, 8, type)], 0.4f, 0.5f, 1.0f);
    }
}

//==============================================================================
juce::Colour SongMap::partColour (int part)
{
    switch (part)
    {
        case Chords:  return cat::Colours::blue;
        case Bass:    return cat::Colours::green;
        case Drums:   return cat::Colours::red;
        case Melody:  return cat::Colours::accent.brighter (0.2f);
        default:      return cat::Colours::purple;
    }
}

void SongMap::setSong (std::shared_ptr<const Song> s)
{
    if (s == song) return;
    song = std::move (s);
    rebuild();
    repaint();
}

void SongMap::setBeat (double b)
{
    if (std::abs (b - beat) < 0.06) return;
    beat = b;
    repaint();
}

void SongMap::rebuild()
{
    if (getWidth() <= 0 || getHeight() <= 0) { image = {}; return; }
    image = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g (image);
    cat::CatLookAndFeel::drawGlassPanel (g, image.getBounds().toFloat(), 8.0f);
    if (song == nullptr || song->lengthBeats <= 0.0) return;

    auto r = image.getBounds().toFloat().reduced (10.0f, 8.0f);
    auto timeArea = r.withTrimmedLeft (labelWidth);
    const float x0 = timeArea.getX(), w = timeArea.getWidth();
    const double len = song->lengthBeats;
    auto xOf = [&] (double b) { return x0 + (float) (b / len) * w; };

    auto secRow = timeArea.removeFromTop (30.0f);
    auto chordRow = timeArea.removeFromTop (17.0f);
    timeArea.removeFromTop (5.0f);
    g.setFont (cat::catFont (12.0f, true));
    for (const auto& sec : song->sections)
    {
        juce::Rectangle<float> b (xOf (sec.start), secRow.getY(), xOf (sec.end()) - xOf (sec.start), secRow.getHeight());
        g.setColour (sectionColour (sec.type));
        g.fillRoundedRectangle (b.reduced (1.0f, 0.0f), 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        const auto name = sectionName (sec.type);
        g.drawFittedText (b.getWidth() > 56.0f ? name : name.substring (0, 1), b.reduced (3.0f, 2.0f).toNearestInt(), juce::Justification::centred, 1);
    }
    g.setFont (cat::catFont (10.5f));
    g.setColour (cat::Colours::textDim);
    for (const auto& ch : song->chords)
    {
        const float cx = xOf (ch.start), cw = xOf (ch.start + ch.length) - cx;
        if (cw >= 24.0f) g.drawText (ch.name, juce::Rectangle<float> (cx + 2.0f, chordRow.getY(), cw - 2.0f, chordRow.getHeight()), juce::Justification::centredLeft, true);
    }
    static const int order[] = { Melody, Counter, Chords, Bass, Drums };
    const float laneH = timeArea.getHeight() / (float) NumParts;
    for (int li = 0; li < NumParts; ++li)
    {
        const int part = order[li];
        juce::Rectangle<float> lane (timeArea.getX(), timeArea.getY() + li * laneH, w, laneH - 3.0f);
        const auto c = partColour (part);
        g.setColour (cat::Colours::textDim);
        g.setFont (cat::catFont (11.5f, true));
        g.drawText (partNames()[part].toUpperCase(), juce::Rectangle<float> (r.getX(), lane.getY(), labelWidth - 8.0f, lane.getHeight()), juce::Justification::centredLeft);
        g.setColour (cat::Colours::raised.withAlpha (0.6f));
        g.fillRoundedRectangle (lane, 3.0f);
        for (const auto& sec : song->sections)
            if (sec.on[(size_t) part])
            {
                g.setColour (c.withAlpha (0.15f));
                g.fillRoundedRectangle (juce::Rectangle<float> (xOf (sec.start), lane.getY(), xOf (sec.end()) - xOf (sec.start), lane.getHeight()).reduced (1.0f, 0.0f), 3.0f);
            }
        const auto& notes = song->parts[(size_t) part];
        if (notes.empty()) continue;
        int lo = 127, hi = 0;
        for (const auto& n : notes) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
        const float span = (float) juce::jmax (6, hi - lo);
        g.setColour (c.withAlpha (0.9f));
        for (const auto& n : notes)
        {
            if (part == Drums && n.pitch != drum::kick && n.pitch != drum::snare && n.pitch != drum::clap) continue;
            const float nx = xOf (n.start), nw = juce::jmax (1.2f, xOf (n.start + n.length) - nx);
            const float ny = part == Drums ? lane.getY() + lane.getHeight() * (n.pitch == drum::kick ? 0.65f : 0.25f)
                                           : lane.getBottom() - 3.0f - (float) (n.pitch - lo) / span * (lane.getHeight() - 6.0f);
            g.fillRect (nx, ny - 1.0f, nw, 2.2f);
        }
    }
}

int SongMap::sectionAt (juce::Point<int> p) const
{
    if (song == nullptr || song->lengthBeats <= 0.0) return -1;
    const float x0 = 10.0f + labelWidth, w = (float) getWidth() - 20.0f - labelWidth;
    if (w <= 0.0f || (float) p.x < x0) return -1;
    const double b = ((float) p.x - x0) / w * song->lengthBeats;
    for (int i = 0; i < (int) song->sections.size(); ++i)
        if (b >= song->sections[(size_t) i].start && b < song->sections[(size_t) i].end()) return i;
    return -1;
}

int SongMap::partAt (juce::Point<int> p) const
{
    // the same geometry as rebuild(): sections 30, chords 17, gap 5, then one lane per part
    const float top = 8.0f + 30.0f + 17.0f + 5.0f, h = (float) getHeight() - 8.0f - top;
    if ((float) p.y < top || h <= 0.0f) return -1;
    static const int order[] = { Melody, Counter, Chords, Bass, Drums };
    const int li = (int) (((float) p.y - top) / (h / (float) NumParts));
    return juce::isPositiveAndBelow (li, (int) NumParts) ? order[li] : -1;
}

void SongMap::mouseMove (const juce::MouseEvent& e)
{
    const bool onLabel = (float) e.x < 10.0f + labelWidth && partAt (e.getPosition()) >= 0;
    setMouseCursor (onLabel ? juce::MouseCursor::DraggingHandCursor
                            : (sectionAt (e.getPosition()) >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor));
}

void SongMap::mouseDown (const juce::MouseEvent& e)
{
    dragPart = partAt (e.getPosition());
    dragged = false;
}

void SongMap::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragged && dragPart >= 0 && e.getDistanceFromDragStart() > 6 && onDragPart)
    {
        dragged = true;
        onDragPart (dragPart);
    }
}

void SongMap::mouseUp (const juce::MouseEvent& e)
{
    if (dragged || e.getDistanceFromDragStart() > 6) return;
    const int si = sectionAt (e.getMouseDownPosition());
    if (si >= 0 && onSectionClick) onSectionClick (si, e.getScreenPosition());
}

void SongMap::paint (juce::Graphics& g)
{
    if (image.isValid()) g.drawImageAt (image, 0, 0);
    if (song == nullptr || song->lengthBeats <= 0.0) return;
    if (beat < 0.0) return;
    const float x0 = 10.0f + labelWidth, w = (float) getWidth() - 20.0f - labelWidth;
    const int si = song->sectionIndexAt (beat);
    if (si >= 0)
    {
        const auto& sec = song->sections[(size_t) si];
        const float sx = x0 + (float) (sec.start / song->lengthBeats) * w, sw = (float) (sec.bars * 4.0 / song->lengthBeats) * w;
        g.setColour (cat::Colours::text.withAlpha (0.8f));
        g.drawRoundedRectangle (juce::Rectangle<float> (sx, 8.0f, sw, 30.0f).reduced (1.0f, 0.0f), 5.0f, 2.0f);
    }
    const float px = x0 + (float) (beat / song->lengthBeats) * w;
    g.setColour (cat::Colours::accent);
    g.fillRect (px - 1.0f, 6.0f, 2.5f, (float) getHeight() - 12.0f);
}

//==============================================================================
WordsPanel::WordsPanel (MuseCatProcessor& p) : processor (p)
{
    const auto names = progressionNames();
    for (int i = 0; i < 4; ++i)
    {
        auto& b = chordBoxes[(size_t) i];
        b.setFont (cat::catFont (16.0f, true));
        b.setTooltip ("The " + names[i].toLowerCase() + "'s chords, played in a loop: chord names (C Am7 F G, F#m Bsus4, Bbmaj7)"
                      " or numbers in the song's key (I vi IV V, ii7 V7 I, i bVI bIII bVII). Empty = Muse-Cat's own.");
        b.onReturnKey = [this] { apply(); };
        addAndMakeVisible (b);
    }
    wordsBox.setMultiLine (true, false);
    wordsBox.setReturnKeyStartsNewLine (true);
    wordsBox.setScrollbarsShown (true);
    wordsBox.setFont (cat::catFont (15.0f));
    wordsBox.setTooltip ("Your words: one line per sung line. Each word is split into syllables, one per melody note"
                         " (write mor-ning to choose where a word splits). An empty line keeps Muse-Cat's line.");
    addAndMakeVisible (wordsBox);
    applyButton.setTooltip ("Play the song with these chords and words (the melody, bass and counter-melody follow the chords)");
    applyButton.onClick = [this] { apply(); };
    resetButton.setTooltip ("Forget your chords and words: back to the ones Muse-Cat wrote");
    resetButton.onClick = [this]
    {
        juce::String err;
        processor.setOwnWords ({}, {}, err);
        load();
        message = "Back to Muse-Cat's own chords and words";
        messageIsError = false;
        if (onApplied) onApplied();
        repaint();
    };
    for (auto* b : { &applyButton, &resetButton, &closeButton }) addAndMakeVisible (b);
}

void WordsPanel::load()
{
    auto song = processor.getSong();
    if (song == nullptr) return;
    const auto own = processor.getOwnChords();
    for (int i = 0; i < 4; ++i)
    {
        shownChords[(size_t) i] = own[(size_t) i].isNotEmpty() ? own[(size_t) i] : progressionText (*song, i);
        chordBoxes[(size_t) i].setText (shownChords[(size_t) i], false);
    }
    shownWords = song->lyricLines.joinIntoString ("\n");
    wordsBox.setText (shownWords, false);
    message = song->settings.endless ? "Endless music: your chords carry on into every new part"
                                     : juce::String (song->lyricLines.size()) + " sung lines - change any line, keep the line count";
    messageIsError = false;
    repaint();
}

bool WordsPanel::apply()
{
    auto own = processor.getOwnChords();
    std::array<juce::String, 4> chords;
    for (int i = 0; i < 4; ++i)
    {
        const auto t = chordBoxes[(size_t) i].getText().trim();
        // unchanged and not yours before: leave it to Muse-Cat
        chords[(size_t) i] = (t == shownChords[(size_t) i].trim() && own[(size_t) i].isEmpty()) ? juce::String() : t;
    }
    juce::StringArray words;
    const auto wt = wordsBox.getText().replace ("\r", "");
    if (wt.trim() != shownWords.trim() || ! processor.getOwnLyrics().isEmpty())
        words = juce::StringArray::fromLines (wt);
    juce::String err;
    if (! processor.setOwnWords (chords, words, err))
    {
        message = err;
        messageIsError = true;
        repaint();
        return false;
    }
    load();
    message = "Playing your chords and words";
    messageIsError = false;
    if (onApplied) onApplied();
    repaint();
    return true;
}

void WordsPanel::paint (juce::Graphics& g)
{
    cat::CatLookAndFeel::drawGlassPanel (g, getLocalBounds().toFloat(), 10.0f);
    g.setColour (cat::Colours::text);
    g.setFont (cat::catFont (13.0f, true));
    const auto names = progressionNames();
    for (int i = 0; i < 4; ++i) g.drawText (names[i].toUpperCase(), chordLabels[(size_t) i], juce::Justification::centredLeft);
    g.drawText ("WORDS (one line per sung line)", wordsLabel, juce::Justification::centredLeft);
    g.setColour (cat::Colours::textDim);
    g.setFont (cat::catFont (12.5f));
    g.drawFittedText ("Chords by name:  C  Am7  F#m  Bbmaj7  G7  Dsus4  Edim  Caug  Fadd9\n"
                      "Or by number in the song's key:  I  vi  IV  V7  ii7  bVII  (small = minor)\n"
                      "Each box loops through its sections. Up to 16 chords.\n"
                      "Words: one syllable per note - write mor-ning to split a word your way.",
                      exampleArea, juce::Justification::topLeft, 6);
    g.setFont (cat::catFont (12.5f));
    g.setColour (messageIsError ? juce::Colour (0xffff7a7a) : cat::Colours::textDim);
    g.drawFittedText (message.isNotEmpty() ? message
                                           : juce::String ("Chords: C Am7 F G  or  I vi IV V. Words: mor-ning splits a word your way."),
                      helpArea, juce::Justification::centredLeft, 2);
}

void WordsPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    auto bottom = r.removeFromBottom (32);
    closeButton.setBounds (bottom.removeFromRight (90));
    bottom.removeFromRight (8);
    resetButton.setBounds (bottom.removeFromRight (140));
    bottom.removeFromRight (8);
    applyButton.setBounds (bottom.removeFromRight (100));
    bottom.removeFromRight (12);
    helpArea = bottom;
    r.removeFromBottom (10);
    auto left = r.removeFromLeft (juce::jmin (420, r.getWidth() / 2));
    r.removeFromLeft (16);
    for (int i = 0; i < 4; ++i)
    {
        chordLabels[(size_t) i] = left.removeFromTop (20);
        chordBoxes[(size_t) i].setBounds (left.removeFromTop (32));
        left.removeFromTop (8);
    }
    exampleArea = left.reduced (0, 4);
    wordsLabel = r.removeFromTop (20);
    wordsBox.setBounds (r);
}

//==============================================================================
MuseCatEditor::MuseCatEditor (MuseCatProcessor& p) : processor (p)
{
    setSize (designW, designH);

    playButton.setTooltip ("Play the song here (in Cubase, Muse-Cat follows Cubase's Play)");
    playButton.onClick = [this] { processor.previewPlaying = ! processor.previewPlaying.load(); };
    playButton.setButtonText ("LAUNCH SONG");
    playButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffd8361f));
    playButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    dragButton.setTooltip ("Drag the whole song into Cubase as MIDI (every part on its own channel, with the words)");
    dragButton.onDragOut = [this]
    {
        if (auto s = processor.getSong())
        {
            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Muse-Cat " + juce::File::createLegalFileName (s->title) + ".mid");
            f.deleteFile();
            if (auto os = f.createOutputStream()) { toMidiFile (*s).writeTo (*os); os.reset(); cat::midiexport::dragFileOut (f, &dragButton); }
        }
    };
    dragButton.onClick = [this] { status = "Drag this button onto a Cubase track to get the song as MIDI (drag a part's name for just that part)"; repaint(); };
    map.onDragPart = [this] (int part) { dragPartOut (part); };
    wordsButton.setTooltip ("Write your own words and chords for this song");
    wordsButton.onClick = [this] { showWords (! wordsPanel.isVisible()); };
    wordsPanel.closeButton.onClick = [this] { showWords (false); };
    wordsPanel.onApplied = [this] { refreshSong(); };
    addChildComponent (wordsPanel);
    map.onSectionClick = [this] (int section, juce::Point<int> screen)
    {
        auto song = processor.getSong();
        if (song == nullptr) return;
        const bool canUndo = ! undoForm.empty() && undoKey == arranger::songKey (*song);
        juce::Component::SafePointer<MuseCatEditor> sp (this);
        arranger::menu (*song, section, canUndo).showMenuAsync (
            juce::PopupMenu::Options().withTargetComponent (&map).withTargetScreenArea (juce::Rectangle<int> (screen.x, screen.y, 1, 1)),
            [sp, section] (int result) { if (sp != nullptr && result > 0) sp->arrangeSection (section, result); });
    };
    map.setTooltip ("Click a section to arrange the song. Drag a part (its lane or name) onto a Cubase track to get just that part as MIDI");
    exportButton.setTooltip ("Save the song as a WAV, as one WAV per part (stems), or as a MIDI file");
    exportButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "The song as a WAV file");
        m.addItem (2, "Each part as its own WAV (stems)");
        m.addItem (3, "MIDI file (all parts, with the words)");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (exportButton),
                         [sp = juce::Component::SafePointer<MuseCatEditor> (this)] (int r) { if (sp != nullptr && r > 0) sp->exportSong (r - 1); });
    };

    seedBox.setInputRestrictions (6, "0123456789");
    seedBox.setJustification (juce::Justification::centred);
    seedBox.setTooltip ("The song's number: the same number, genre and mood always write the same song");
    auto commitSeed = [this] { processor.setSeed ((juce::uint32) juce::jmax (1, seedBox.getText().getIntValue())); refreshSong(); };
    seedBox.onReturnKey = commitSeed;
    seedBox.onFocusLost = commitSeed;
    newSeedButton.setTooltip ("A new song in the same genre and mood");
    newSeedButton.onClick = [this] { processor.newSeed(); refreshSong(); };
    for (auto* b : { &songButton, &endlessButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (303);
    }
    songButton.setTooltip ("A whole song: intro, verses, choruses, bridge, outro");
    endlessButton.setTooltip ("Music that never ends: it keeps writing new parts while it plays");
    auto setEndless = [this] (bool e)
    {
        if (auto* prm = processor.apvts.getParameter ("endless")) prm->setValueNotifyingHost (e ? 1.0f : 0.0f);
        processor.updateSong();
    };
    songButton.onClick = [setEndless, this] { if (songButton.getToggleState()) setEndless (false); };
    endlessButton.onClick = [setEndless, this] { if (endlessButton.getToggleState()) setEndless (true); };

    static const char* tips[] = { "New chords (the melody, bass and counter-melody follow them)", "A new bass line", "New drum grooves and fills",
                                  "A new melody and hook (with new words)", "A new counter-melody", "New words for the same melody",
                                  "A new song form: other sections in another order" };
    const auto names = targetNames();
    for (int t = 0; t < names.size(); ++t)
    {
        auto* b = renewButtons.add (new juce::TextButton (names[t]));
        b->setTooltip (tips[t]);
        b->onClick = [this, t] { processor.regenerate (t); refreshSong(); };
        addAndMakeVisible (b);
    }
    static const char* volIds[] = { "volChords", "volBass", "volDrums", "volMelody", "volCounter" };
    static const char* sndIds[] = { "sndChords", "sndBass", "sndDrums", "sndMelody", "sndCounter" };
    for (int part = 0; part < NumParts; ++part)
    {
        auto* sb = soundBoxes.add (new cat::ParamCombo (processor.apvts, sndIds[part]));
        sb->getCombo().setTooltip ("Which sound plays the " + partNames()[part].toLowerCase());
        addAndMakeVisible (sb);
        auto* k = volumeKnobs.add (new cat::Knob (processor.apvts, volIds[part], "Volume", nullptr, SongMap::partColour (part)));
        addAndMakeVisible (k);
    }
    soundToggle.setTooltip ("Muse-Cat plays the song with its own sounds (turn off to only send MIDI)");
    midiToggle.setTooltip ("Sends every part as MIDI: chords ch 1, bass 2, melody 3, counter-melody 4, drums 10");

    for (juce::Component* c : { (juce::Component*) &presetBar, (juce::Component*) &playButton, (juce::Component*) &dragButton,
                                (juce::Component*) &exportButton, (juce::Component*) &genreBox, (juce::Component*) &moodBox,
                                (juce::Component*) &seedBox, (juce::Component*) &newSeedButton, (juce::Component*) &songButton,
                                (juce::Component*) &endlessButton, (juce::Component*) &map, (juce::Component*) &masterKnob,
                                (juce::Component*) &soundToggle, (juce::Component*) &midiToggle, (juce::Component*) &wordsButton })
        addAndMakeVisible (c);
    resized();
    refreshSong();
    startTimerHz (30);
}

MuseCatEditor::~MuseCatEditor() { stopTimer(); }

void MuseCatEditor::refreshSong()
{
    auto s = processor.getSong();
    map.setSong (s);
    shownSong = s;
    shownLine = -2;
    if (! seedBox.hasKeyboardFocus (true))
        seedBox.setText (juce::String (processor.currentSettings().seed), false);
    const bool endless = processor.currentSettings().endless;
    (endless ? endlessButton : songButton).setToggleState (true, juce::dontSendNotification);
    repaint();
}

void MuseCatEditor::timerCallback()
{
    auto s = processor.getSong();
    if (s != shownSong) refreshSong();
    const bool running = processor.isRunning();
    map.setBeat (running ? processor.getSongBeat() : -1.0);
    const bool preview = processor.previewPlaying.load();
    if (preview != shownPlaying)
    {
        shownPlaying = preview;
        playButton.setButtonText (preview ? "STOP" : "LAUNCH SONG");
        playButton.setColour (juce::TextButton::buttonColourId, preview ? juce::Colour (0xff8a1a12) : juce::Colour (0xffd8361f));
        playButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    }
    if (s != nullptr)
    {
        const int line = running ? s->lyricLineAt (processor.getSongBeat() + 0.25) : -1;
        if (line != shownLine) { shownLine = line; repaint (lyricsArea); }
    }
    for (int part = 0; part < NumParts; ++part)
    {
        const float m = juce::jmax (processor.partMeter (part), shownMeters[(size_t) part] * 0.85f);
        if (std::abs (m - shownMeters[(size_t) part]) > 0.004f) { shownMeters[(size_t) part] = m; repaint (partLabelAreas[(size_t) part]); repaint (lampArea); }
    }
    if (! seedBox.hasKeyboardFocus (true))
    {
        const auto want = juce::String (processor.currentSettings().seed);
        if (seedBox.getText() != want) seedBox.setText (want, false);
    }
    const bool endless = processor.currentSettings().endless;
    if (endlessButton.getToggleState() != endless) (endless ? endlessButton : songButton).setToggleState (true, juce::dontSendNotification);
}

void MuseCatEditor::arrangeSection (int section, int result)
{
    auto song = processor.getSong();
    if (song == nullptr || song->settings.endless) return;
    std::vector<Section> form;
    if (result == arranger::Undo)
    {
        if (undoForm.empty() || undoKey != arranger::songKey (*song)) return;
        form = undoForm;
        undoForm.clear();
    }
    else
    {
        if (! arranger::apply (*song, section, result, (juce::uint32) (1 + juce::Random().nextInt (0x7ffffff)), form)) return;
        undoForm = song->sections;
        undoKey = arranger::songKey (*song);
    }
    if (processor.arrange (form))
    {
        status = "Arranged: " + juce::String ((int) form.size()) + " sections";
        refreshSong();
    }
}

void MuseCatEditor::showWords (bool show)
{
    if (show) wordsPanel.load();
    wordsPanel.setVisible (show);
    wordsButton.setToggleState (show, juce::dontSendNotification);
    if (show) { wordsPanel.toFront (true); wordsPanel.chordBoxes[0].grabKeyboardFocus(); }
}

void MuseCatEditor::dragPartOut (int part)
{
    const auto f = processor.writeMidiForDrag (part);
    if (f.existsAsFile()) cat::midiexport::dragFileOut (f, this);
}

void MuseCatEditor::mouseDown (const juce::MouseEvent& e)
{
    labelDragPart = -1;
    labelDragged = false;
    for (int part = 0; part < NumParts; ++part)
        if (partLabelAreas[(size_t) part].contains (e.getPosition())) labelDragPart = part;
    if (labelDragPart >= 0) { status = "Drag " + partNames()[labelDragPart] + " onto a Cubase track to get just that part as MIDI"; repaint(); }
}

void MuseCatEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (labelDragPart >= 0 && ! labelDragged && e.getDistanceFromDragStart() > 6)
    {
        labelDragged = true;
        dragPartOut (labelDragPart);
    }
}

void MuseCatEditor::exportSong (int what)
{
    auto s = processor.getSong();
    if (s == nullptr) return;
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Muse-Cat");
    dir.createDirectory();
    const auto name = juce::File::createLegalFileName (s->title.isNotEmpty() ? s->title : juce::String ("Muse-Cat song"));
    juce::File shown;
    bool ok = true;
    {
        juce::MouseCursor::showWaitCursor();
        if (what == 0) { shown = dir.getNonexistentChildFile (name, ".wav", false); ok = processor.exportWav (shown); }
        else if (what == 1)
        {
            shown = dir.getNonexistentChildFile (name + " stems", {}, false);
            shown.createDirectory();
            for (int part = 0; part < NumParts && ok; ++part)
                ok = processor.exportWav (shown.getChildFile (juce::String (part + 1) + " " + partNames()[part] + ".wav"), part);
        }
        else { shown = dir.getNonexistentChildFile (name, ".mid", false); ok = processor.exportMidi (shown); }
        juce::MouseCursor::hideWaitCursor();
    }
    status = ok ? "Saved: " + shown.getFullPathName() : "Could not save " + shown.getFullPathName();
    if (ok) shown.revealToUser();
    repaint();
}

void MuseCatEditor::paint (juce::Graphics& g)
{
    cat::paintBackdrop (g, getLocalBounds());

    // MUSE·CAT  SONG MISSION CONTROL                         lamps
    {
        auto l = logoArea;
        g.setColour (cat::Colours::text);
        g.setFont (cat::displayFont (22.0f).withExtraKerningFactor (0.2f));
        const juce::String name = juce::String ("MUSE") + juce::String::charToString ((juce::juce_wchar) 0x00b7) + "CAT";
        g.drawText (name, l.removeFromLeft (190), juce::Justification::centredLeft);
        g.setColour (cat::Colours::textDim);
        g.setFont (cat::catFont (11.0f).withExtraKerningFactor (0.25f));
        g.drawText ("SONG MISSION CONTROL", l, juce::Justification::centredLeft);

        auto lamps = lampArea;
        for (int part = NumParts - 1; part >= 0; --part)
        {
            auto one = lamps.removeFromRight (lamps.getWidth() / (part + 1));
            const bool on = shownMeters[(size_t) part] > 0.0005f;
            const auto c = on ? juce::Colour (0xff4ce08a) : juce::Colour (0xff3a3f46);
            auto dot = one.removeFromLeft (14).toFloat().withSizeKeepingCentre (9.0f, 9.0f);
            if (on) { g.setColour (c.withAlpha (0.35f)); g.fillEllipse (dot.expanded (4.0f)); }
            g.setColour (c);
            g.fillEllipse (dot);
            g.setColour (cat::Colours::text.withAlpha (on ? 1.0f : 0.55f));
            g.setFont (cat::catFont (11.0f).withExtraKerningFactor (0.1f));
            g.drawText (partNames()[part].toUpperCase() + (on ? " OK" : ""), one.withTrimmedLeft (4), juce::Justification::centredLeft, true);
        }
    }
    g.setColour (cat::Colours::textDim);
    g.setFont (cat::catFont (12.0f, true));
    g.drawText ("SONG #", seedBox.getBounds().translated (-62, 0).withWidth (58), juce::Justification::centredRight);

    // the arrangement screen
    auto screen = screenArea.toFloat();
    g.setColour (juce::Colour (0xff2e3238));
    g.fillRoundedRectangle (screen, 12.0f);
    auto glass = screen.reduced (6.0f);
    g.setColour (juce::Colour (0xff0a0f14));
    g.fillRoundedRectangle (glass, 9.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b1a24), glass.getCentreX(), glass.getY() + glass.getHeight() * 0.4f,
                                             juce::Colour (0xff060c11), glass.getX(), glass.getBottom(), true));
    g.fillRoundedRectangle (glass.reduced (6.0f), 7.0f);

    auto s = shownSong;
    if (s != nullptr)
    {
        auto h = headerArea;
        const auto st = s->settings;
        const int secs = (int) std::round (s->lengthBeats * 60.0 / juce::jmax (30.0, s->bpm));
        juce::String info = s->title.toUpperCase() + "  " + juce::String::charToString ((juce::juce_wchar) 0x00b7) + "  " + keyName (s->key).toUpperCase() + " "
                          + scaleName (s->scale).toUpperCase() + "  " + juce::String::charToString ((juce::juce_wchar) 0x00b7) + "  " + juce::String (juce::roundToInt (s->bpm)) + " BPM";
        if (processor.isHostPlaying() && std::abs (processor.getHostBpm() - s->bpm) > 0.5)
            info << " (CUBASE " << juce::roundToInt (processor.getHostBpm()) << ")";
        g.setColour (juce::Colour (0xff4fd1ff));
        g.setFont (cat::catFont (13.0f, true).withExtraKerningFactor (0.05f));
        g.drawText (info, h, juce::Justification::centredLeft, true);
        g.drawText (st.endless ? "ENDLESS  " + juce::String (s->chunk + 1) : juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2),
                    h, juce::Justification::centredRight, true);
    }

    // the lyrics terminal
    {
        auto la = lyricsArea.toFloat();
        g.setColour (juce::Colour (0xff2e3238));
        g.fillRoundedRectangle (la, 10.0f);
        g.setColour (juce::Colour (0xff0a0f14));
        g.fillRoundedRectangle (la.reduced (2.0f), 9.0f);
        auto t = la.reduced (12.0f, 8.0f);
        const auto green = juce::Colour (0xff7dffb2);
        g.setFont (cat::catFont (12.0f, true));
        g.setColour (green.withAlpha (0.7f));
        g.drawText ("> LYRICS", t.removeFromTop (16.0f), juce::Justification::centredLeft);
        if (s != nullptr)
        {
            const auto& lines = s->lyricLines;
            const int line = shownLine;
            if (lines.isEmpty())
            {
                g.drawText ("(no words in this song)", t.removeFromTop (18.0f), juce::Justification::centredLeft);
            }
            else
            {
                const int cur = line >= 0 ? line : 0;
                g.setColour (green.withAlpha (0.45f));
                g.setFont (cat::catFont (12.0f));
                if (cur >= 1) g.drawFittedText (lines[cur - 1], t.removeFromTop (18.0f).toNearestInt(), juce::Justification::centredLeft, 1);
                else t.removeFromTop (18.0f);
                g.setColour (green);
                g.setFont (cat::catFont (14.0f, true));
                g.drawFittedText (lines[cur], t.removeFromTop (40.0f).toNearestInt(), juce::Justification::centredLeft, 2);
                g.setColour (green.withAlpha (0.45f));
                g.setFont (cat::catFont (12.0f));
                if (cur + 1 < lines.size()) g.drawFittedText (lines[cur + 1], t.removeFromTop (18.0f).toNearestInt(), juce::Justification::centredLeft, 1);
            }
        }
    }

    g.setColour (cat::Colours::textDim);
    g.setFont (cat::catFont (11.0f).withExtraKerningFactor (0.2f));
    g.drawText ("WRITE AGAIN", renewLabelArea, juce::Justification::centredLeft);

    // the parts console
    {
        auto pa = partsArea.toFloat();
        g.setColour (juce::Colour (0xff1a1d22));
        g.fillRoundedRectangle (pa, 12.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (pa.reduced (1.0f), 12.0f, 2.0f);
    }
    for (int part = 0; part < NumParts; ++part)
    {
        auto a = partLabelAreas[(size_t) part].toFloat();
        const auto c = SongMap::partColour (part);
        g.setColour (c);
        g.fillEllipse (a.removeFromLeft (12.0f).withSizeKeepingCentre (8.0f, 8.0f));
        g.setColour (cat::Colours::text);
        g.setFont (cat::catFont (11.5f, true).withExtraKerningFactor (0.1f));
        g.drawText (partNames()[part].toUpperCase(), a.removeFromLeft (a.getWidth() - 34.0f).translated (4.0f, 0.0f), juce::Justification::centredLeft, true);
        auto meter = a.reduced (2.0f, 7.0f);
        g.setColour (juce::Colour (0xff0a0f14));
        g.fillRoundedRectangle (meter, 2.0f);
        const float lv = juce::jlimit (0.0f, 1.0f, std::sqrt (shownMeters[(size_t) part]) * 1.4f);
        g.setColour (c);
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * lv), 2.0f);
    }
    g.setColour (cat::Colours::textDim);
    g.setFont (cat::catFont (11.0f));
    g.drawFittedText ("MIDI out: chords ch 1, bass 2, melody 3, counter 4, drums 10", midiInfoArea, juce::Justification::centredLeft, 2);
    if (status.isNotEmpty())
    {
        g.setColour (cat::Colours::accent);
        g.drawText (status, getLocalBounds().removeFromBottom (18).reduced (14, 0), juce::Justification::centredLeft, true);
    }
}

void MuseCatEditor::resized()
{
    if (soundBoxes.size() < NumParts || volumeKnobs.size() < NumParts)
        return;   // still being built (setSize in the constructor comes first)
    auto r = getLocalBounds().reduced (18, 12);
    {
        auto top = r.removeFromTop (34);
        logoArea = top.removeFromLeft (420);
        lampArea = top.withTrimmedLeft (20);
    }
    r.removeFromTop (8);
    {
        auto row = r.removeFromTop (32);
        presetBar.setBounds (row.removeFromLeft (310)); row.removeFromLeft (10);
        genreBox.setBounds (row.removeFromLeft (190)); row.removeFromLeft (10);
        moodBox.setBounds (row.removeFromLeft (160)); row.removeFromLeft (66);
        seedBox.setBounds (row.removeFromLeft (74)); row.removeFromLeft (6);
        newSeedButton.setBounds (row.removeFromLeft (104)); row.removeFromLeft (12);
        songButton.setBounds (row.removeFromLeft (70));
        endlessButton.setBounds (row.removeFromLeft (84));
    }
    r.removeFromTop (12);
    r.removeFromBottom (14);

    // the side: lyrics terminal, write-again switches, export, LAUNCH
    auto aside = r.removeFromRight (250);
    r.removeFromRight (14);
    lyricsArea = aside.removeFromTop (130);
    aside.removeFromTop (10);
    playButton.setBounds (aside.removeFromBottom (56));
    aside.removeFromBottom (10);
    {
        auto row = aside.removeFromBottom (30);
        dragButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
        row.removeFromLeft (6);
        exportButton.setBounds (row);
        aside.removeFromBottom (6);
        wordsButton.setBounds (aside.removeFromBottom (30));
        aside.removeFromBottom (10);
    }
    renewLabelArea = aside.removeFromTop (18);
    const int bh = juce::jmin (32, (aside.getHeight() - 4 * (renewButtons.size() - 1)) / juce::jmax (1, renewButtons.size()));
    for (auto* b : renewButtons) { b->setBounds (aside.removeFromTop (bh)); aside.removeFromTop (4); }

    // the parts console
    partsArea = r.removeFromBottom (168);
    r.removeFromBottom (12);
    screenArea = r;
    auto inner = screenArea.reduced (20, 14);
    headerArea = inner.removeFromTop (24);
    inner.removeFromTop (4);
    map.setBounds (inner);
    wordsPanel.setBounds (screenArea);

    auto pa = partsArea.reduced (12, 10);
    auto right = pa.removeFromRight (190);
    masterKnob.setBounds (right.removeFromLeft (76));
    right.removeFromLeft (8);
    soundToggle.setBounds (right.removeFromTop (30));
    right.removeFromTop (6);
    midiToggle.setBounds (right.removeFromTop (30));
    midiInfoArea = right.withTrimmedTop (4);
    pa.removeFromRight (10);
    const int cw = pa.getWidth() / NumParts;
    for (int part = 0; part < NumParts; ++part)
    {
        auto c = pa.removeFromLeft (cw).reduced (5, 0);
        partLabelAreas[(size_t) part] = c.removeFromTop (22);
        c.removeFromTop (4);
        soundBoxes[part]->setBounds (c.removeFromTop (26));
        c.removeFromTop (2);
        volumeKnobs[part]->setBounds (c.withSizeKeepingCentre (juce::jmin (c.getWidth(), 86), c.getHeight()));
    }
}

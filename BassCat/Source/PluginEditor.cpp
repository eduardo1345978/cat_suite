#include "PluginEditor.h"
#include "MidiExport.h"

namespace C = cat::Colours;
using namespace basscat;

namespace
{
    // Display order, top to bottom (high to low on the neck)
    const int rowOrder[] = { Octave, Seventh, Fifth, Third, Root, WalkUp, WalkDown, Approach, Ghost };
    constexpr int numRows = 9;
}

//==============================================================================
DegreeGrid::DegreeGrid (BassCatProcessor& p) : processor (p)
{
    setTooltip ("The bass line in chord degrees. Click an empty cell to add a note, click a note to remove it, "
                "drag a note up / down to change its velocity.");
    startTimerHz (30);
}

juce::Colour DegreeGrid::degreeColour (int degree)
{
    switch (degree)
    {
        case Root:     return C::accent;
        case Fifth:    return C::teal;
        case Octave:   return C::purple;
        case Third:    return C::green;
        case Seventh:  return C::red;
        case Approach: return juce::Colour (0xffd8b08a);
        case WalkUp:
        case WalkDown: return C::slate;
        case Ghost:    return C::textDim;
        default:       return C::text;
    }
}

int DegreeGrid::rowFor (int degree) const
{
    for (int r = 0; r < numRows; ++r)
        if (rowOrder[r] == degree) return r;
    return 0;
}

int DegreeGrid::degreeForRow (int row) const { return rowOrder[juce::jlimit (0, numRows - 1, row)]; }

juce::Rectangle<float> DegreeGrid::gridArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f).withTrimmedLeft (50.0f).withTrimmedTop (16.0f);
}

void DegreeGrid::timerCallback()
{
    const auto v = processor.patternVersion.load();
    if (v != seenVersion)
    {
        seenVersion = v;
        pattern = processor.getPattern();
    }
    repaint();
}

void DegreeGrid::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff101116));
    g.fillRoundedRectangle (bounds, 8.0f);

    const auto area = gridArea();
    const int steps = pattern.bars * 16;
    const float cw = area.getWidth() / (float) steps, rh = area.getHeight() / (float) numRows;
    const auto names = getDegreeNames();

    for (int r = 0; r < numRows; ++r)
    {
        const auto row = juce::Rectangle<float> (area.getX(), area.getY() + rh * (float) r, area.getWidth(), rh);
        g.setColour (r % 2 == 0 ? juce::Colour (0xff15161b) : juce::Colour (0xff121318));
        g.fillRect (row);
        g.setColour (degreeColour (rowOrder[r]).withAlpha (0.85f));
        g.setFont (cat::catFont (11.0f, true));
        g.drawText (names[rowOrder[r]], juce::Rectangle<float> (bounds.getX() + 8.0f, row.getY(), 46.0f, rh), juce::Justification::centredLeft);
    }
    for (int s = 0; s <= steps; ++s)
    {
        g.setColour (s % 16 == 0 ? C::outline.brighter (0.4f) : (s % 4 == 0 ? C::outline : C::outline.withAlpha (0.35f)));
        g.drawVerticalLine (juce::roundToInt (area.getX() + cw * (float) s), area.getY(), area.getBottom());
    }
    g.setColour (C::textDim);
    g.setFont (cat::catFont (10.0f));
    for (int b = 0; b < pattern.bars; ++b)
        g.drawText ("Bar " + juce::String (b + 1), juce::Rectangle<float> (area.getX() + cw * 16.0f * (float) b + 3.0f, bounds.getY() + 4.0f, 60.0f, 12.0f),
                    juce::Justification::centredLeft);

    // Notes
    for (const auto& n : pattern.notes)
    {
        const int r = rowFor (n.degree);
        auto rect = juce::Rectangle<float> (area.getX() + (float) (n.pos * 4.0) * cw + 1.0f, area.getY() + rh * (float) r + 3.0f,
                                            juce::jmax (4.0f, (float) (n.len * 4.0) * cw - 2.0f), rh - 6.0f);
        const auto col = degreeColour (n.degree);
        g.setColour (col.withAlpha (0.25f + 0.75f * (float) n.velocity / 127.0f));
        g.fillRoundedRectangle (rect, 3.0f);
        if (n.slide)
        {
            g.setColour (col.brighter (0.4f));
            g.drawLine (rect.getRight() - 6.0f, rect.getCentreY() + 4.0f, rect.getRight() + 2.0f, rect.getCentreY() - 4.0f, 1.6f);
        }
        if (n.octave != 0)
        {
            g.setColour (C::background);
            g.setFont (cat::catFont (9.0f, true));
            g.drawText (n.octave > 0 ? "+" + juce::String (n.octave) : juce::String (n.octave), rect.withWidth (16.0f), juce::Justification::centred);
        }
    }

    // Playhead
    if (processor.uiTransportRunning.load() && processor.uiChordRoot.load() >= 0)
    {
        const double L = pattern.lengthPpq();
        const double ph = std::fmod (processor.uiPpq.load(), L);
        const float x = area.getX() + (float) (ph / L) * area.getWidth();
        g.setColour (C::text.withAlpha (0.7f));
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
    }
    else if (processor.uiChordRoot.load() < 0)
    {
        g.setColour (C::textDim);
        g.setFont (cat::catFont (12.0f));
        g.drawText ("Hold a chord (or route Chord-Cat's MIDI Out here) - the bass follows it",
                    area.withTrimmedTop (area.getHeight() - 22.0f), juce::Justification::centred);
    }
}

void DegreeGrid::mouseDown (const juce::MouseEvent& e)
{
    const auto area = gridArea();
    if (! area.contains (e.position)) return;
    const int steps = pattern.bars * 16;
    const int step = juce::jlimit (0, steps - 1, (int) ((e.position.x - area.getX()) / area.getWidth() * (float) steps));
    const int row = juce::jlimit (0, numRows - 1, (int) ((e.position.y - area.getY()) / area.getHeight() * (float) numRows));
    const double pos = step * 0.25;

    for (int i = 0; i < (int) pattern.notes.size(); ++i)
    {
        const auto& n = pattern.notes[(size_t) i];
        if (rowFor (n.degree) == row && pos >= n.pos - 1.0e-6 && pos < n.pos + juce::jmax (0.25, n.len) - 1.0e-6)
        {
            dragNote = i;
            dragStartVel = n.velocity;
            dragStartY = e.position.y;
            return;
        }
    }

    BassNote n;
    n.pos = pos;
    n.len = degreeForRow (row) == Ghost ? 0.08 : 0.22;
    n.degree = degreeForRow (row);
    n.velocity = degreeForRow (row) == Ghost ? 40 : 100;
    pattern.notes.push_back (n);
    std::sort (pattern.notes.begin(), pattern.notes.end(), [] (const BassNote& a, const BassNote& b) { return a.pos < b.pos; });
    processor.setPattern (pattern);
    seenVersion = processor.patternVersion.load();
    dragNote = -1;
}

void DegreeGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNote < 0 || dragNote >= (int) pattern.notes.size()) return;
    const float dy = dragStartY - e.position.y;
    if (std::abs (dy) < 3.0f) return;
    pattern.notes[(size_t) dragNote].velocity = juce::jlimit (1, 127, dragStartVel + juce::roundToInt (dy * 0.8f));
    processor.setPattern (pattern);
    seenVersion = processor.patternVersion.load();
}

void DegreeGrid::mouseUp (const juce::MouseEvent& e)
{
    // A click without dragging removes the note
    if (dragNote >= 0 && dragNote < (int) pattern.notes.size() && e.getDistanceFromDragStart() < 3)
    {
        pattern.notes.erase (pattern.notes.begin() + dragNote);
        processor.setPattern (pattern);
        seenVersion = processor.patternVersion.load();
    }
    dragNote = -1;
}

//==============================================================================
BassCatEditor::BassCatEditor (BassCatProcessor& p)
    : processor (p),
      presetBar (p.presetManager),
      style (p.apvts, "style", "Style", &p.midiLearn, 40),
      length (p.apvts, "bars", "Length", &p.midiLearn, 48),
      density (p.apvts, "density", "Density", &p.midiLearn, 54),
      fills (p.apvts, "fills", "Fills", &p.midiLearn, 34),
      ghosts (p.apvts, "ghosts", "Ghosts", &p.midiLearn, 48),
      slides (p.apvts, "slides", "Slides", &p.midiLearn, 44),
      swing (p.apvts, "swing", "Swing", &p.midiLearn, 44),
      human (p.apvts, "human", "Human", &p.midiLearn, 48),
      gate (p.apvts, "gate", "Gate", &p.midiLearn, 36),
      velocity (p.apvts, "velocity", "Velocity", &p.midiLearn, 56),
      velRandom (p.apvts, "velRandom", "Vel Rnd", &p.midiLearn, 52),
      chance (p.apvts, "chance", "Chance", &p.midiLearn, 50),
      followGroove (p.apvts, "followGroove", "Follow Drum", &p.midiLearn),
      latch (p.apvts, "latch", "Latch", &p.midiLearn),
      thru (p.apvts, "thru", "Thru", &p.midiLearn),
      keyTrigger (p.apvts, "keyTrigger", "Key Play", &p.midiLearn),
      grid (p),
      keyboard (p.noteMonitor, 16, 76),
      catView (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, cat::CatStyle::Bass),
      surpriseControls (p, p.surpriseUndo, [&p] { p.surprise(); }),
      bootScreen (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); },
                  "BASS", "-cat", "BASS LINES THAT FOLLOW YOUR CHORDS", cat::CatStyle::Bass)
{
    setLookAndFeel (&lnf);

    generateButton.setTooltip ("New bass line in this style (the < > arrows step through 20 variations)");
    generateButton.onClick = [this] { processor.generate (true); };
    variationStepper = std::make_unique<cat::Stepper> ("Var", 1, BassCatProcessor::numVariations,
        [this] { return processor.getVariation(); },
        [this] (int v) { processor.setVariation (v); },
        [] (int v) { return juce::String (v) + " / " + juce::String (BassCatProcessor::numVariations); });
    lowNote = cat::Stepper::forParameter (p.apvts, "lowNote", "Low Note", [] (int v) { return juce::MidiMessage::getMidiNoteName (v, true, true, 3); });
    lowNote->setTooltip ("Lowest root: the bass stays in the octave above this note");
    octave = cat::Stepper::forParameter (p.apvts, "octave", "Octave", [] (int v) { return v > 0 ? "+" + juce::String (v) : juce::String (v); });
    channel = cat::Stepper::forParameter (p.apvts, "outChannel", "Ch");

    dragButton.setTooltip ("Drag the bass line onto a Cubase MIDI track (played against the held chord)");
    dragButton.onDragOut = [this]
    {
        const auto seq = processor.buildPatternSequence();
        if (seq.getNumEvents() == 0) return;
        cat::midiexport::dragFileOut (cat::midiexport::writeMidiFile (seq, processor.getHostBpm(), "Bass-Cat Line"), this);
    };


    followGroove.setTooltip ("Take swing and human timing from Drum-Cat (Cat Link)");
    latch.setTooltip ("Keep playing the last chord after you let go");
    thru.setTooltip ("Also send the chord you play to the output");
    keyTrigger.setTooltip ("With the transport stopped, holding a chord plays the line");
    slides.getSlider().setTooltip ("Slides into the next note (808 glides need a mono / legato bass sound)");
    ghosts.getSlider().setTooltip ("Muted 'dead' notes between the main notes");
    fills.getSlider().setTooltip ("Chance of a walk-up into the next phrase");

    chordLabel.setFont (cat::catFont (13.0f, true));
    chordLabel.setColour (juce::Label::textColourId, C::text);
    chordLabel.setJustificationType (juce::Justification::centredRight);
    chordTimer.fn = [this] { chordLabel.setText ("Chord  " + processor.getChordText(), juce::dontSendNotification); };
    chordTimer.startTimerHz (10);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &generateButton, variationStepper.get(), lowNote.get(), octave.get(),
             channel.get(), &dragButton, &style, &length, &density, &fills, &ghosts, &slides, &swing, &human, &gate, &velocity, &velRandom,
             &chance, &followGroove, &latch, &thru, &keyTrigger, &chordLabel, &grid, &keyboard })
        addAndMakeVisible (c);

    addAndMakeVisible (catView);
    addAndMakeVisible (surpriseControls);
    addAndMakeVisible (bootScreen);
    setSize (1100, 680);
}

BassCatEditor::~BassCatEditor()
{
    setLookAndFeel (nullptr);
}

void BassCatEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::background);
    cat::CatLookAndFeel::drawLogo (g, juce::Rectangle<float> (14.0f, 8.0f, 160.0f, 32.0f), "BASS", "-cat");
}

void BassCatEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto item = [] (juce::Component& c, float w, float h = 30.0f) { return juce::FlexItem (c).withWidth (w).withHeight (h).withMargin ({ 0, 3, 0, 3 }); };
    auto row = [] (juce::Rectangle<int> area, std::initializer_list<juce::FlexItem> items)
    {
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        for (auto& it : items) fb.items.add (it);
        fb.performLayout (area);
    };

    {
        auto top = r.removeFromTop (40);
        auto logo = top.removeFromLeft (150);
        catView.setBounds (logo.removeFromRight (40).expanded (0, 6));
        row (top, { item (presetBar, 330), juce::FlexItem().withFlex (1.0f), item (generateButton, 90), item (*variationStepper, 116),
                    item (surpriseControls, 122), item (dragButton, 92) });
    }
    r.removeFromTop (10);
    row (r.removeFromTop (30), { item (style, 200, 28), item (length, 140, 28), item (density, 120, 28).withFlex (1.0f), item (fills, 100, 28).withFlex (1.0f),
                                 item (ghosts, 110, 28).withFlex (1.0f), item (slides, 110, 28).withFlex (1.0f) });
    r.removeFromTop (6);
    row (r.removeFromTop (30), { item (swing, 110, 28).withFlex (1.0f), item (human, 110, 28).withFlex (1.0f), item (followGroove, 92, 28),
                                 item (gate, 100, 28).withFlex (1.0f), item (velocity, 120, 28).withFlex (1.0f), item (velRandom, 110, 28).withFlex (1.0f),
                                 item (chance, 110, 28).withFlex (1.0f) });
    r.removeFromTop (6);
    row (r.removeFromTop (30), { item (*lowNote, 140, 28), item (*octave, 110, 28), item (*channel, 80, 28), item (latch, 64, 28), item (thru, 60, 28),
                                 item (keyTrigger, 80, 28), juce::FlexItem().withFlex (1.0f), item (chordLabel, 180, 28) });
    r.removeFromTop (10);
    keyboard.setBounds (r.removeFromBottom (54));
    r.removeFromBottom (8);
    grid.setBounds (r);

    bootScreen.setBounds (getLocalBounds());
}

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
    return getLocalBounds().toFloat().reduced (8.0f).withTrimmedLeft (54.0f).withTrimmedTop (14.0f);
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
    g.setColour (juce::Colour (0xff1a1a1c));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto area = gridArea();
    const int steps = pattern.bars * 16;
    const float cw = area.getWidth() / (float) steps, rh = area.getHeight() / (float) numRows;
    const auto names = getDegreeNames();

    for (int r = 0; r < numRows; ++r)
    {
        const auto row = juce::Rectangle<float> (area.getX(), area.getY() + rh * (float) r, area.getWidth(), rh);
        g.setColour (r % 2 == 0 ? juce::Colour (0xff232326) : juce::Colour (0xff1e1e21));
        g.fillRect (row);
        g.setColour (juce::Colour (0xfff1f1f2).withAlpha (rowOrder[r] == Ghost ? 0.5f : 0.9f));
        g.setFont (cat::catFont (11.0f, true));
        g.drawText (names[rowOrder[r]].toUpperCase(), juce::Rectangle<float> (bounds.getX() + 8.0f, row.getY(), 46.0f, rh), juce::Justification::centredLeft);
    }
    for (int s = 0; s <= steps; ++s)
    {
        g.setColour (juce::Colours::white.withAlpha (s % 16 == 0 ? 0.3f : (s % 4 == 0 ? 0.14f : 0.05f)));
        g.drawVerticalLine (juce::roundToInt (area.getX() + cw * (float) s), area.getY(), area.getBottom());
    }
    // step LEDs along the top, the one under the playhead lit
    {
        int lit = -1;
        if (processor.uiTransportRunning.load() && processor.uiChordRoot.load() >= 0)
            lit = (int) (std::fmod (processor.uiPpq.load(), pattern.lengthPpq()) * 4.0);
        for (int st = 0; st < steps; ++st)
        {
            const auto c = juce::Point<float> (area.getX() + cw * ((float) st + 0.5f), bounds.getY() + 9.0f);
            bool has = false;
            for (const auto& n : pattern.notes) has = has || (int) std::floor (n.pos * 4.0 + 0.01) == st;
            const auto led = st == lit ? juce::Colour (0xffff4a3a) : (has ? juce::Colour (0xffa0362f) : juce::Colour (0xff5a2a27));
            if (st == lit)
            {
                g.setColour (led.withAlpha (0.35f));
                g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (c));
            }
            g.setColour (led);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (c));
        }
    }


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
        g.setColour (juce::Colour (0xffff4a3a).withAlpha (0.8f));
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
      density (p.apvts, "density", "DENSITY", &p.midiLearn),
      fills (p.apvts, "fills", "FILLS", &p.midiLearn),
      ghosts (p.apvts, "ghosts", "GHOSTS", &p.midiLearn),
      slides (p.apvts, "slides", "SLIDES", &p.midiLearn),
      swing (p.apvts, "swing", "SWING", &p.midiLearn),
      human (p.apvts, "human", "HUMAN", &p.midiLearn),
      gate (p.apvts, "gate", "GATE", &p.midiLearn),
      velocity (p.apvts, "velocity", "VELOCITY", &p.midiLearn),
      velRandom (p.apvts, "velRandom", "VEL RND", &p.midiLearn),
      chance (p.apvts, "chance", "CHANCE", &p.midiLearn),
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

    dragButton.setTooltip ("Drag the bass line onto a MIDI track in your DAW (played against the held chord)");
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

    chordLabel.setFont (cat::displayFont (30.0f));
    chordLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff4a3a));
    chordLabel.setJustificationType (juce::Justification::centred);
    chordLabel.setInterceptsMouseClicks (false, false);
    chordTimer.fn = [this]
    {
        auto chord = processor.getChordText();
        if (chord.isEmpty() || chord == "-") chord = "- - -";
        chordLabel.setText (chord.toUpperCase() + "  " + juce::String (processor.getVariation()).paddedLeft ('0', 2), juce::dontSendNotification);
    };
    chordTimer.startTimerHz (10);

    for (auto* b : std::initializer_list<juce::TextButton*> { &generateButton, &followGroove, &latch, &thru, &keyTrigger, &dragButton })
    {
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a2d));
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a2a2d));
        b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xfff1f1f2));
    }

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &generateButton, variationStepper.get(), lowNote.get(), octave.get(),
             channel.get(), &dragButton, &style, &length, &density, &fills, &ghosts, &slides, &swing, &human, &gate, &velocity, &velRandom,
             &chance, &followGroove, &latch, &thru, &keyTrigger, &chordLabel, &grid, &keyboard })
        addAndMakeVisible (c);

    addAndMakeVisible (catView);
    addAndMakeVisible (surpriseControls);
    addAndMakeVisible (bootScreen);
    setSize (1100, 720);
}

BassCatEditor::~BassCatEditor()
{
    setLookAndFeel (nullptr);
}

void BassCatEditor::paint (juce::Graphics& g)
{
    cat::paintBackdrop (g, getLocalBounds());

    // Bass Cat BC-303 .................. COMPUTER CONTROLLED
    const auto ink = juce::Colour (0xff1a1a1c);
    g.setColour (ink);
    g.setFont (cat::displayFont (30.0f));
    g.drawText ("Bass Cat", 26, 14, 150, 40, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffc1272d));
    g.setFont (cat::displayFont (30.0f).withStyle (juce::Font::plain));
    g.drawText ("BC-303", 160, 14, 120, 40, juce::Justification::centredLeft);
    g.setColour (ink);
    g.setFont (cat::catFont (13.0f, true).withExtraKerningFactor (0.3f));
    g.drawText ("COMPUTER CONTROLLED", getWidth() - 266, 20, 240, 30, juce::Justification::centredRight);
    g.fillRect (26, 60, getWidth() - 52, 2);

    // LEDs above the mode keys
    for (auto* b : std::initializer_list<juce::Button*> { &followGroove, &latch, &thru, &keyTrigger })
    {
        const auto c = juce::Point<float> ((float) b->getBounds().getCentreX(), (float) b->getY() - 8.0f);
        const bool on = b->getToggleState();
        if (on)
        {
            g.setColour (juce::Colour (0xffff4a3a).withAlpha (0.35f));
            g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (c));
        }
        g.setColour (on ? juce::Colour (0xffff4a3a) : juce::Colour (0xff6e2a26));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (c));
    }

    // the red LED window
    auto led = chordLabel.getBounds().toFloat();
    g.setColour (juce::Colour (0xff23110f));
    g.fillRoundedRectangle (led, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (led.reduced (1.0f), 4.0f, 2.0f);
    g.setColour (juce::Colour (0xffff4a3a).withAlpha (0.12f));
    g.fillRoundedRectangle (led.reduced (8.0f, 10.0f), 6.0f);

    // the red / yellow / black stripe
    const auto st = stripeBounds;
    const juce::Colour stripe[] = { juce::Colour (0xffc1272d), juce::Colour (0xfff2b51d), ink };
    for (int x = st.getX(), i = 0; x < st.getRight(); x += 60, ++i)
    {
        g.setColour (stripe[i % 3]);
        g.fillRect (juce::Rectangle<int> (x, st.getY(), juce::jmin (60, st.getRight() - x), st.getHeight()));
    }
}

void BassCatEditor::resized()
{
    auto r = getLocalBounds().reduced (26, 14);
    {
        auto top = r.removeFromTop (46);
        top.removeFromLeft (270);
        catView.setBounds (top.removeFromLeft (46));
        top.removeFromLeft (14);
        presetBar.setBounds (top.removeFromLeft (320).withSizeKeepingCentre (320, 30));
    }
    r.removeFromTop (14);

    // one long row of big 303 knobs
    knobBounds = r.removeFromTop (104);
    {
        auto k = knobBounds;
        const int w = k.getWidth() / 10;
        for (auto* c : std::initializer_list<juce::Component*> { &density, &fills, &ghosts, &slides, &swing, &human, &gate, &velocity, &velRandom, &chance })
            c->setBounds (k.removeFromLeft (w).reduced (4, 0));
    }
    r.removeFromTop (14);

    // mode keys with LEDs + selectors | the red LED read-out
    {
        auto band = r.removeFromTop (78);
        chordLabel.setBounds (band.removeFromRight (230).withSizeKeepingCentre (230, 58));
        band.removeFromRight (16);
        auto keys = band.removeFromTop (44).withTrimmedTop (14);
        const int kw = 92;
        for (auto* b : std::initializer_list<juce::Component*> { &followGroove, &latch, &thru, &keyTrigger })
        {
            b->setBounds (keys.removeFromLeft (kw).reduced (4, 0));
        }
        keys.removeFromLeft (12);
        generateButton.setBounds (keys.removeFromLeft (100).reduced (4, 0));
        variationStepper->setBounds (keys.removeFromLeft (124).reduced (4, 0));
        surpriseControls.setBounds (keys.removeFromLeft (130).reduced (4, 0));
        dragButton.setBounds (keys.removeFromLeft (100).reduced (4, 0));
        band.removeFromTop (6);
        auto sel = band.removeFromTop (28);
        style.setBounds (sel.removeFromLeft (200).reduced (4, 0));
        length.setBounds (sel.removeFromLeft (150).reduced (4, 0));
        lowNote->setBounds (sel.removeFromLeft (150).reduced (4, 0));
        octave->setBounds (sel.removeFromLeft (120).reduced (4, 0));
        channel->setBounds (sel.removeFromLeft (90).reduced (4, 0));
    }
    r.removeFromTop (12);

    stripeBounds = r.removeFromBottom (10);
    r.removeFromBottom (10);
    keyboard.setBounds (r.removeFromBottom (48));
    r.removeFromBottom (10);
    grid.setBounds (r);

    bootScreen.setBounds (getLocalBounds());
}

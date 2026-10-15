#include "PluginEditor.h"
#include "MidiExport.h"

using namespace drumcat;
namespace C = cat::Colours;

namespace
{
    constexpr int headerW = 128;
    juce::String muteParam (int lane) { return "mute" + getLaneNames()[lane].removeCharacters (" "); }
}

//==============================================================================
juce::Colour DrumGrid::laneColour (int lane)
{
    static const juce::Colour colours[] = {
        juce::Colour (0xff8a9bd0), juce::Colour (0xffc48a88), juce::Colour (0xffc4a184), juce::Colour (0xff9fb5a0),
        juce::Colour (0xff76aab0), juce::Colour (0xff8fa3c8), juce::Colour (0xffd0c08a), juce::Colour (0xffa08fcc),
        juce::Colour (0xffb194c4),
    };
    return colours[juce::jlimit (0, NumLanes - 1, lane)];
}

DrumGrid::DrumGrid (DrumCatProcessor& p) : processor (p)
{
    setRepaintsOnMouseActivity (false);
    startTimerHz (30);
}

juce::Rectangle<float> DrumGrid::rowBounds (int lane) const
{
    const float h = (float) getHeight() / (float) NumLanes;
    return { 0.0f, h * (float) lane, (float) getWidth(), h };
}

int DrumGrid::laneAt (float y) const
{
    return juce::jlimit (0, NumLanes - 1, (int) (y / ((float) getHeight() / (float) NumLanes)));
}

juce::Rectangle<float> DrumGrid::hitBounds (const Hit& h) const
{
    const auto row = rowBounds (h.lane).reduced (0.0f, 4.0f);
    const float pxPerPpq = (float) getWidth() / (float) pattern.lengthPpq();
    const float x = (float) (h.pos + h.micro) * pxPerPpq;
    const float w = juce::jlimit (3.0f, 9.0f, pxPerPpq * 0.25f * 0.45f);
    const float hh = row.getHeight() * (0.25f + 0.75f * (float) h.velocity / 127.0f);
    return { x - w * 0.5f, row.getBottom() - hh, w, hh };
}

int DrumGrid::hitAt (juce::Point<float> p) const
{
    for (int i = (int) pattern.hits.size(); --i >= 0;)
    {
        const auto& h = pattern.hits[(size_t) i];
        auto r = hitBounds (h).expanded (3.0f, 0.0f);
        r.setTop (rowBounds (h.lane).getY());
        if (r.contains (p)) return i;
    }
    return -1;
}

void DrumGrid::timerCallback()
{
    if (processor.patternVersion.load() != shownVersion && dragHit < 0)
    {
        shownVersion = processor.patternVersion.load();
        pattern = processor.getPattern();
        repaint();
    }

    const double L = pattern.lengthPpq();
    const double ph = processor.uiTransportRunning.load() ? std::fmod (std::fmod (processor.uiPpq.load(), L) + L, L) : -1.0;
    if (std::abs (ph - playhead) > 1.0e-4)
    {
        playhead = ph;
        repaint();
    }
}

void DrumGrid::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (C::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    const double L = pattern.lengthPpq();
    const float pxPerPpq = bounds.getWidth() / (float) L;

    // Rows
    for (int l = 0; l < NumLanes; ++l)
        if (l % 2 == 1)
        {
            g.setColour (juce::Colours::white.withAlpha (0.018f));
            g.fillRect (rowBounds (l));
        }

    // Grid: 16ths faint, beats stronger, bars strongest
    for (int s = 0; s <= (int) std::round (L * 4.0); ++s)
    {
        const float x = (float) s * 0.25f * pxPerPpq;
        const float alpha = s % 16 == 0 ? 0.16f : (s % 4 == 0 ? 0.08f : 0.035f);
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.drawVerticalLine (juce::roundToInt (x), 0.0f, bounds.getBottom());
    }

    // Hits
    for (int i = 0; i < (int) pattern.hits.size(); ++i)
    {
        const auto& h = pattern.hits[(size_t) i];
        const bool muted = processor.apvts.getRawParameterValue (muteParam (h.lane))->load() > 0.5f;
        auto colour = laneColour (h.lane);
        if (h.lane == Tom) colour = colour.brighter (0.15f * (float) (h.variant - 1));
        g.setColour (colour.withAlpha (muted ? 0.2f : (i == hoverHit ? 1.0f : 0.85f)));
        g.fillRoundedRectangle (hitBounds (h), 2.0f);
    }

    if (playhead >= 0.0)
    {
        g.setColour (C::text.withAlpha (0.7f));
        g.drawVerticalLine (juce::roundToInt ((float) playhead * pxPerPpq), 0.0f, bounds.getBottom());
    }
}

void DrumGrid::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitAt (e.position);
    if (h != hoverHit) { hoverHit = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void DrumGrid::mouseExit (const juce::MouseEvent&)
{
    hoverHit = -1;
    repaint();
}

void DrumGrid::mouseDown (const juce::MouseEvent& e)
{
    dragged = false;
    dragHit = hitAt (e.position);

    if (dragHit < 0)
    {
        // Add a hit on the nearest step of the edit grid
        const double gridPpq = gridToPpq (juce::roundToInt (processor.apvts.getRawParameterValue ("grid")->load()));
        const double ppq = e.position.x / (double) getWidth() * pattern.lengthPpq();
        Hit h;
        h.lane = laneAt (e.position.y);
        h.pos = juce::jlimit (0.0, pattern.lengthPpq() - gridPpq, std::floor (ppq / gridPpq + 0.5) * gridPpq);
        const auto row = rowBounds (h.lane);
        h.velocity = juce::jlimit (20, 127, juce::roundToInt (127.0f * (row.getBottom() - e.position.y) / row.getHeight() + 30.0f));
        if (h.lane == Tom)
            h.variant = juce::jlimit (0, 2, 2 - (int) ((e.position.y - row.getY()) / row.getHeight() * 3.0f));
        pattern.hits.push_back (h);
        pattern.sort();
        processor.setPattern (pattern);
        shownVersion = processor.patternVersion.load();
        dragHit = -1;
        repaint();
        return;
    }
    dragStartVelocity = pattern.hits[(size_t) dragHit].velocity;
}

void DrumGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragHit < 0 || e.getDistanceFromDragStart() < 3)
        return;
    dragged = true;
    auto& h = pattern.hits[(size_t) dragHit];
    h.velocity = juce::jlimit (1, 127, dragStartVelocity - e.getDistanceFromDragStartY());
    repaint();
}

void DrumGrid::mouseUp (const juce::MouseEvent&)
{
    if (dragHit >= 0)
    {
        if (! dragged)
            pattern.hits.erase (pattern.hits.begin() + dragHit);   // click on a hit removes it
        processor.setPattern (pattern);
        shownVersion = processor.patternVersion.load();
    }
    dragHit = -1;
    hoverHit = -1;
    repaint();
}

//==============================================================================
LaneHeader::LaneHeader (DrumCatProcessor& p, int l)
    : lane (l), mute (p.apvts, muteParam (l), "M", &p.midiLearn)
{
    mute.setTooltip ("Mute " + getLaneNames()[l]);
    mute.setColour (juce::TextButton::buttonOnColourId, C::red);
    addAndMakeVisible (mute);
}

void LaneHeader::resized()
{
    mute.setBounds (getLocalBounds().removeFromRight (26).withSizeKeepingCentre (24, juce::jmin (22, getHeight() - 6)));
}

void LaneHeader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.0f, 2.0f);
    const auto colour = DrumGrid::laneColour (lane);
    g.setColour (C::panel.interpolatedWith (colour, 0.12f * glow));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colour.withAlpha (0.5f + 0.5f * glow));
    g.fillRoundedRectangle (r.removeFromLeft (3.0f), 1.5f);
    g.setColour (C::text);
    g.setFont (cat::catFont (13.0f));
    g.drawText (getLaneNames()[lane], getLocalBounds().withTrimmedLeft (12).withTrimmedRight (30), juce::Justification::centredLeft);
}

//==============================================================================
DrumCatEditor::DrumCatEditor (DrumCatProcessor& p)
    : processor (p),
      swing (p.apvts, "swing", "Swing", &p.midiLearn, 48),
      human (p.apvts, "human", "Human", &p.midiLearn, 52),
      velocity (p.apvts, "velocity", "Velocity", &p.midiLearn, 58),
      velRandom (p.apvts, "velRandom", "Vel Rnd", &p.midiLearn, 54),
      chance (p.apvts, "chance", "Chance", &p.midiLearn, 54),
      open (p.apvts, "open", "Open", &p.midiLearn, 44),
      density (p.apvts, "density", "Density", &p.midiLearn, 56),
      fills (p.apvts, "fills", "Fills", &p.midiLearn, 40),
      grid (p.apvts, "grid", "Grid", &p.midiLearn, 34),
      length (p.apvts, "bars", "Length", &p.midiLearn, 50),
      map (p.apvts, "map", "Map", &p.midiLearn, 34),
      keyTrigger (p.apvts, "keyTrigger", "Key Play", &p.midiLearn),
      drumGrid (p),
      catView (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, cat::CatStyle::Drum),
      surpriseControls (p, p.surpriseUndo, [&p] { p.surprise(); }),
      bootScreen (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); },
                  "DRUM", "-cat", "DRUM PATTERN GENERATOR", cat::CatStyle::Drum)
{
    setLookAndFeel (&lnf);

    const auto styles = getStyleNames();
    for (int i = 0; i < styles.size(); ++i)
    {
        auto* b = styleButtons.add (new juce::TextButton (styles[i]));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { processor.selectStyle (i); };
        addAndMakeVisible (b);
    }

    generateButton.setColour (juce::TextButton::buttonColourId, C::accent.withAlpha (0.16f));
    generateButton.setTooltip ("New set of patterns in this style (the < > arrows step through 20 variations)");
    generateButton.onClick = [this] { processor.generate (true); };
    addAndMakeVisible (generateButton);

    variationStepper = std::make_unique<cat::Stepper> ("Var", 1, DrumCatProcessor::numVariations,
        [this] { return processor.getVariation(); },
        [this] (int v) { processor.setVariation (v); },
        [] (int v) { return juce::String (v) + " / " + juce::String (DrumCatProcessor::numVariations); });
    addAndMakeVisible (*variationStepper);

    presetStepper = std::make_unique<cat::PresetBar> (p.presetManager);
    addAndMakeVisible (*presetStepper);

    dragButton.setTooltip ("Drag the pattern onto a Cubase MIDI track");
    dragButton.onDragOut = [this]
    {
        const auto seq = processor.buildPatternSequence();
        if (seq.getNumEvents() == 0) return;
        cat::midiexport::dragFileOut (cat::midiexport::writeMidiFile (seq, processor.getHostBpm(), "Drum-Cat Pattern"), this);
    };
    addAndMakeVisible (dragButton);

    swing.getSlider().setTooltip ("Delays the off-beats of the Grid (on top of the style's own feel)");
    human.getSlider().setTooltip ("Random timing, in ms (early and late)");
    velocity.getSlider().setTooltip ("Overall dynamics");
    chance.getSlider().setTooltip ("Probability that each hit plays - each loop is a little different");
    open.getSlider().setTooltip ("Turns closed hats on the off-beat 8ths into open hats");
    density.getSlider().setTooltip ("How busy generated patterns are");
    fills.getSlider().setTooltip ("Chance of a fill at the end of generated patterns");
    keyTrigger.setTooltip ("With the transport stopped, holding any key plays the pattern");

    channel = cat::Stepper::forParameter (p.apvts, "outChannel", "Ch");
    for (auto* c : std::initializer_list<juce::Component*> { &swing, &human, &velocity, &velRandom, &chance, &open, &density, &fills,
                                                             &grid, &length, &map, channel.get(), &keyTrigger })
        addAndMakeVisible (c);

    for (int l = 0; l < NumLanes; ++l)
        addAndMakeVisible (lanes.add (new LaneHeader (p, l)));
    addAndMakeVisible (drumGrid);

    addAndMakeVisible (catView);
    addAndMakeVisible (surpriseControls);
    addAndMakeVisible (bootScreen);

    for (int l = 0; l < NumLanes; ++l)
        seenHits[(size_t) l] = p.laneHits[(size_t) l].load();

    setSize (1180, 756);
    startTimerHz (30);
}

DrumCatEditor::~DrumCatEditor()
{
    setLookAndFeel (nullptr);
}

void DrumCatEditor::timerCallback()
{
    const int style = juce::roundToInt (processor.apvts.getRawParameterValue ("style")->load());
    for (int i = 0; i < styleButtons.size(); ++i)
        styleButtons[i]->setToggleState (i == style, juce::dontSendNotification);

    for (int l = 0; l < NumLanes; ++l)
    {
        const auto n = processor.laneHits[(size_t) l].load();
        if (n != seenHits[(size_t) l]) { seenHits[(size_t) l] = n; lanes[l]->flash(); }
        lanes[l]->decay();
    }
}

void DrumCatEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::background);
    cat::CatLookAndFeel::drawLogo (g, juce::Rectangle<float> (14.0f, 8.0f, 150.0f, 32.0f), "DRUM", "-cat");

    g.setColour (C::textDim);
    g.setFont (cat::catFont (11.0f));
    g.drawText ("Click empty space to add a hit  -  drag a hit up / down for velocity  -  click a hit to remove it",
                drumGrid.getBounds().translated (0, drumGrid.getHeight() + 4).withHeight (16), juce::Justification::centredLeft);
}

void DrumCatEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto item = [] (juce::Component& c, float w, float h = 30.0f) { return juce::FlexItem (c).withWidth (w).withHeight (h).withMargin ({ 0, 3, 0, 3 }); };

    // Header: logo + cat | styles | generate / variations | surprise | drag
    {
        auto top = r.removeFromTop (40);
        auto logo = top.removeFromLeft (150);
        catView.setBounds (logo.removeFromRight (40).expanded (0, 6));
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        fb.items.add (juce::FlexItem().withFlex (1.0f));
        fb.items.add (item (generateButton, 90));
        fb.items.add (item (*variationStepper, 116));
        fb.items.add (item (surpriseControls, 122));
        fb.items.add (item (dragButton, 92));
        fb.performLayout (top);
    }
    r.removeFromTop (8);
    {
        // styles: one row of buttons
        auto row = r.removeFromTop (30);
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        for (auto* b : styleButtons) fb.items.add (item (*b, 70, 28).withFlex (1.0f));
        fb.performLayout (row);
    }
    r.removeFromTop (8);

    // Feel
    {
        auto row = r.removeFromTop (30);
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        for (auto* s : { &swing, &human, &velocity, &velRandom, &chance, &open })
            fb.items.add (item (*s, 150, 28).withFlex (1.0f));
        fb.items.add (item (grid, 100, 28));
        fb.performLayout (row);
    }
    r.removeFromTop (6);

    // Generator + output
    {
        auto row = r.removeFromTop (30);
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        fb.items.add (item (density, 200, 28));
        fb.items.add (item (fills, 200, 28));
        fb.items.add (item (length, 130, 28));
        fb.items.add (juce::FlexItem().withFlex (1.0f));
        fb.items.add (item (*presetStepper, 320, 28));
        fb.items.add (juce::FlexItem().withFlex (1.0f));
        fb.items.add (item (map, 190, 28));
        fb.items.add (item (*channel, 86, 28));
        fb.items.add (item (keyTrigger, 80, 28));
        fb.performLayout (row);
    }
    r.removeFromTop (10);

    r.removeFromBottom (20);   // hint line
    auto headers = r.removeFromLeft (headerW);
    r.removeFromLeft (6);
    drumGrid.setBounds (r);
    const float laneH = (float) headers.getHeight() / (float) NumLanes;
    for (int l = 0; l < NumLanes; ++l)
        lanes[l]->setBounds (headers.getX(), headers.getY() + juce::roundToInt (laneH * (float) l), headers.getWidth(), juce::roundToInt (laneH));

    bootScreen.setBounds (getLocalBounds());
}

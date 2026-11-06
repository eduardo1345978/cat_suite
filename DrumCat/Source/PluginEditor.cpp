#include "PluginEditor.h"
#include "MidiExport.h"

using namespace drumcat;
namespace C = cat::Colours;

namespace
{
    constexpr int headerW = 104;
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
    g.setColour (juce::Colour (0xff262723));
    g.fillRoundedRectangle (bounds, 6.0f);

    const double L = pattern.lengthPpq();
    const float pxPerPpq = bounds.getWidth() / (float) L;

    const bool light = false;   // the grid sits in the dark pad well
    const auto ink = light ? juce::Colours::black : juce::Colours::white;

    // Rows
    for (int l = 0; l < NumLanes; ++l)
        if (l % 2 == 1)
        {
            g.setColour (ink.withAlpha (light ? 0.03f : 0.018f));
            g.fillRect (rowBounds (l));
        }

    // Grid: 16ths faint, beats stronger, bars strongest
    for (int s = 0; s <= (int) std::round (L * 4.0); ++s)
    {
        const float x = (float) s * 0.25f * pxPerPpq;
        const float alpha = s % 16 == 0 ? 0.16f : (s % 4 == 0 ? 0.08f : 0.035f);
        g.setColour (ink.withAlpha (light ? alpha * 1.3f : alpha));
        g.drawVerticalLine (juce::roundToInt (x), 0.0f, bounds.getBottom());
    }

    // Hits
    for (int i = 0; i < (int) pattern.hits.size(); ++i)
    {
        const auto& h = pattern.hits[(size_t) i];
        const bool muted = processor.apvts.getRawParameterValue (muteParam (h.lane))->load() > 0.5f;
        auto colour = laneColour (h.lane);
        if (h.lane == Tom) colour = colour.brighter (0.15f * (float) (h.variant - 1));
        if (light) colour = colour.withMultipliedSaturation (1.6f).darker (0.35f);
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
    g.setColour (juce::Colour (0xff2c2d29).interpolatedWith (colour, 0.25f * glow));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colour.withAlpha (0.5f + 0.5f * glow));
    g.fillRoundedRectangle (r.removeFromLeft (3.0f), 1.5f);
    g.setColour (juce::Colour (0xffd6d7d1));
    g.setFont (cat::catFont (12.0f, true));
    g.drawText (getLaneNames()[lane].toUpperCase(), getLocalBounds().withTrimmedLeft (12).withTrimmedRight (30), juce::Justification::centredLeft);
}

//==============================================================================
DrumCatEditor::DrumCatEditor (DrumCatProcessor& p)
    : processor (p),
      swing (p.apvts, "swing", "SWING", &p.midiLearn),
      human (p.apvts, "human", "HUMAN", &p.midiLearn),
      velocity (p.apvts, "velocity", "VELOCITY", &p.midiLearn),
      velRandom (p.apvts, "velRandom", "VEL RND", &p.midiLearn),
      chance (p.apvts, "chance", "CHANCE", &p.midiLearn),
      open (p.apvts, "open", "OPEN", &p.midiLearn),
      density (p.apvts, "density", "DENSITY", &p.midiLearn),
      fills (p.apvts, "fills", "FILLS", &p.midiLearn),
      lcd (p), pads (p),
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

    generateButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0302a));
    generateButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
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
    addAndMakeVisible (lcd);
    addAndMakeVisible (pads);

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
    cat::paintBackdrop (g, getLocalBounds());

    // "DRUM CAT  DC-16 RHYTHM"
    g.setFont (cat::displayFont (30.0f));
    g.setColour (juce::Colour (0xff1c1c1b));
    g.drawText ("DRUM CAT", 24, 12, 200, 40, juce::Justification::centredLeft);
    g.setFont (cat::catFont (12.0f, true));
    g.setColour (juce::Colour (0xffb3261e));
    g.drawText ("D C - 1 6   R H Y T H M", 212, 22, 180, 24, juce::Justification::centredLeft);

    // the dark well that holds the pads and the groove grid
    auto well = wellBounds.toFloat();
    g.setColour (juce::Colour (0xff1e1f1c));
    g.fillRoundedRectangle (well, 12.0f);
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.12f - 0.03f * (float) i));
        g.drawRoundedRectangle (well.reduced ((float) i + 0.5f), 12.0f, 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawHorizontalLine (juce::roundToInt (well.getBottom()) + 1, well.getX() + 12.0f, well.getRight() - 12.0f);

    g.setColour (juce::Colour (0xff8f908a));
    g.setFont (cat::catFont (11.0f));
    g.drawText ("Pads: click to mute  -  grid: click empty space to add a hit, drag up / down for velocity, click a hit to remove it",
                wellBounds.getX(), wellBounds.getBottom() + 4, wellBounds.getWidth(), 16, juce::Justification::centredLeft);
}

void DrumCatEditor::resized()
{
    auto r = getLocalBounds().reduced (24, 14);
    auto item = [] (juce::Component& c, float w, float h = 30.0f) { return juce::FlexItem (c).withWidth (w).withHeight (h).withMargin ({ 0, 3, 0, 3 }); };

    // Left: logo, LCD, knobs, settings, transport-like keys
    auto left = r.removeFromLeft (420);
    r.removeFromLeft (24);
    {
        auto top = left.removeFromTop (44);
        catView.setBounds (top.removeFromRight (48).expanded (0, 4));
    }
    left.removeFromTop (6);
    lcd.setBounds (left.removeFromTop (150));
    left.removeFromTop (12);

    auto knobRow = [&left] (std::initializer_list<juce::Component*> ks)
    {
        auto row = left.removeFromTop (94);
        const int w = row.getWidth() / 4;
        for (auto* k : ks) k->setBounds (row.removeFromLeft (w).reduced (4, 0));
        left.removeFromTop (6);
    };
    knobRow ({ &swing, &human, &velocity, &velRandom });
    knobRow ({ &density, &fills, &chance, &open });

    auto twoUp = [&left] (juce::Component& a, juce::Component& b)
    {
        auto row = left.removeFromTop (30);
        a.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4));
        row.removeFromLeft (8);
        b.setBounds (row);
        left.removeFromTop (8);
    };
    twoUp (grid, length);
    twoUp (map, *channel);
    presetStepper->setBounds (left.removeFromTop (30));
    left.removeFromTop (12);

    // keys: Generate, Var, Surprise, Key Play, Drag
    {
        auto row = left.removeFromTop (36);
        const int w = row.getWidth() / 4;
        generateButton.setBounds (row.removeFromLeft (w).reduced (4, 0));
        variationStepper->setBounds (row.removeFromLeft (w * 2).reduced (4, 2));
        keyTrigger.setBounds (row.reduced (4, 0));
        left.removeFromTop (8);
        auto row2 = left.removeFromTop (36);
        surpriseControls.setBounds (row2.removeFromLeft (w * 2).reduced (4, 2));
        dragButton.setBounds (row2.reduced (4, 0));
    }

    // Right: style keys on top, then the well with pads + groove grid
    {
        auto row = r.removeFromTop (44).withTrimmedTop (10);
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        for (auto* b : styleButtons) fb.items.add (item (*b, 40, 30).withFlex (1.0f).withMargin ({ 0, 2, 0, 2 }));
        fb.performLayout (row);
    }
    r.removeFromTop (10);
    r.removeFromBottom (20);   // hint line
    wellBounds = r;
    auto w = r.reduced (14);
    pads.setBounds (w.removeFromTop (juce::roundToInt (w.getHeight() * 0.42f)));
    w.removeFromTop (12);
    auto headers = w.removeFromLeft (headerW);
    w.removeFromLeft (6);
    drumGrid.setBounds (w);
    const float laneH = (float) headers.getHeight() / (float) NumLanes;
    for (int l = 0; l < NumLanes; ++l)
        lanes[l]->setBounds (headers.getX(), headers.getY() + juce::roundToInt (laneH * (float) l), headers.getWidth(), juce::roundToInt (laneH));

    bootScreen.setBounds (getLocalBounds());
}

//==============================================================================
void DrumLcd::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff2b2e26));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (1.0f), 8.0f, 2.0f);
    auto glass = r.reduced (10.0f);
    g.setColour (juce::Colour (0xff9fb57a));
    g.fillRoundedRectangle (glass, 4.0f);
    // inner shade
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.18f), glass.getX(), glass.getY(),
                                             juce::Colours::transparentBlack, glass.getX(), glass.getY() + 24.0f, false));
    g.fillRoundedRectangle (glass, 4.0f);

    auto& ap = processor.apvts;
    const int style = juce::roundToInt (ap.getRawParameterValue ("style")->load());
    const auto styles = getStyleNames();
    const float swing = ap.getRawParameterValue ("swing")->load();
    const int vel = juce::roundToInt (ap.getRawParameterValue ("velocity")->load());
    const double bpm = processor.getHostBpm();

    const auto ink = juce::Colour (0xff1e2814);
    auto t = glass.reduced (12.0f, 6.0f);
    g.setColour (ink);
    g.setFont (cat::lcdFont (28.0f));
    g.drawText ("VAR " + juce::String (processor.getVariation()).paddedLeft ('0', 2) + " - " + styles[style].toUpperCase(),
                t.removeFromTop (28.0f), juce::Justification::centredLeft);
    g.setFont (cat::lcdFont (21.0f));
    g.drawText ("TEMPO " + (bpm > 0.0 ? juce::String (bpm, 1).paddedLeft ('0', 5) : juce::String ("---.-"))
                    + "  SWING " + juce::String (juce::roundToInt (swing)) + "%",
                t.removeFromTop (22.0f), juce::Justification::centredLeft);
    g.drawText (processor.presetManager.currentName().toUpperCase() + "  VEL " + juce::String (vel),
                t.removeFromTop (22.0f), juce::Justification::centredLeft);

    // kick steps of bar 1
    const auto pat = processor.getPattern();
    std::array<bool, 16> on {};
    for (const auto& h : pat.hits)
        if (h.lane == Kick && h.pos < 4.0)
            on[(size_t) juce::jlimit (0, 15, (int) std::floor (h.pos * 4.0 + 0.01))] = true;
    auto steps = t.removeFromTop (20.0f).withTrimmedTop (4.0f);
    const float sw = juce::jmin (16.0f, steps.getWidth() / 16.0f);
    for (int i = 0; i < 16; ++i)
    {
        auto cell = juce::Rectangle<float> (steps.getX() + (float) i * sw, steps.getY(), sw - 4.0f, steps.getHeight());
        g.setColour (ink.withAlpha (on[(size_t) i] ? 0.95f : 0.0f));
        g.fillRect (cell);
        g.setColour (ink.withAlpha (0.8f));
        g.drawRect (cell, 1.4f);
    }
}

//==============================================================================
DrumPads::DrumPads (DrumCatProcessor& p) : processor (p)
{
    for (int l = 0; l < NumLanes; ++l) seen[(size_t) l] = p.laneHits[(size_t) l].load();
    startTimerHz (30);
}

juce::Rectangle<float> DrumPads::padBounds (int lane) const
{
    const auto b = getLocalBounds().toFloat();
    const float gap = 12.0f, w = (b.getWidth() - gap * 2.0f) / 3.0f, h = (b.getHeight() - gap * 2.0f) / 3.0f;
    // MPC order: low drums at the bottom
    static const int order[] = { Crash, Ride, OpenHat, Tom, Perc, Hat, Kick, Snare, Rim };
    int slot = 0;
    for (int i = 0; i < NumLanes; ++i) if (order[i] == lane) slot = i;
    return { b.getX() + (float) (slot % 3) * (w + gap), b.getY() + (float) (slot / 3) * (h + gap), w, h };
}

void DrumPads::timerCallback()
{
    bool dirty = false;
    for (int l = 0; l < NumLanes; ++l)
    {
        const auto n = processor.laneHits[(size_t) l].load();
        if (n != seen[(size_t) l]) { seen[(size_t) l] = n; glow[(size_t) l] = 1.0f; dirty = true; }
        else if (glow[(size_t) l] > 0.0f) { glow[(size_t) l] = juce::jmax (0.0f, glow[(size_t) l] - 0.08f); dirty = true; }
    }
    if (dirty) repaint();
}

void DrumPads::paint (juce::Graphics& g)
{
    const auto names = getLaneNames();
    for (int l = 0; l < NumLanes; ++l)
    {
        auto pad = padBounds (l);
        const float gl = glow[(size_t) l];
        const bool muted = processor.apvts.getRawParameterValue (muteParam (l))->load() > 0.5f;

        // rubber pad
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (pad.translated (0.0f, 3.0f), 8.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3b37), 0.0f, pad.getY(), juce::Colour (0xff2c2d29), 0.0f, pad.getBottom(), false));
        g.fillRoundedRectangle (pad, 8.0f);
        if (gl > 0.0f)
        {
            g.setColour (juce::Colour (0xffff6e3c).withAlpha (0.25f * gl));
            g.fillRoundedRectangle (pad.expanded (4.0f), 11.0f);
            juce::ColourGradient lit (juce::Colour (0xffff9a6a), pad.getCentreX(), pad.getY() + pad.getHeight() * 0.4f,
                                      juce::Colour (0xffe05634), pad.getRight(), pad.getBottom(), true);
            g.setGradientFill (lit);
            g.setOpacity (gl);
            g.fillRoundedRectangle (pad, 8.0f);
            g.setOpacity (1.0f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine (juce::roundToInt (pad.getY()) + 1, pad.getX() + 8.0f, pad.getRight() - 8.0f);

        const auto ink = gl > 0.5f ? juce::Colour (0xff3a1410) : juce::Colour (0xffd6d7d1);
        auto label = pad.reduced (10.0f, 8.0f);
        g.setColour (ink.withAlpha (muted ? 0.35f : 1.0f));
        g.setFont (cat::catFont (12.0f, true));
        g.drawText (names[l].toUpperCase(), label.removeFromTop (16.0f), juce::Justification::topLeft);
        g.setFont (cat::catFont (10.5f));
        g.setColour ((gl > 0.5f ? juce::Colour (0xff5e2620) : juce::Colour (0xffa9aaa4)).withAlpha (muted ? 0.35f : 1.0f));
        g.drawText ("A" + juce::String (l + 1).paddedLeft ('0', 2), label.removeFromTop (14.0f), juce::Justification::topLeft);
        if (muted)
        {
            g.setColour (juce::Colour (0xffd8452c));
            g.setFont (cat::catFont (11.0f, true));
            g.drawText ("MUTE", label, juce::Justification::bottomRight);
        }
    }
}

void DrumPads::mouseDown (const juce::MouseEvent& e)
{
    for (int l = 0; l < NumLanes; ++l)
        if (padBounds (l).contains (e.position))
        {
            if (auto* prm = processor.apvts.getParameter (muteParam (l)))
                prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
            repaint();
        }
}

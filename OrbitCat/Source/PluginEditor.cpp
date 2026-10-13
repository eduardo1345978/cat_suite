#include "PluginEditor.h"

using namespace orbitcat;
namespace C = cat::Colours;

//==============================================================================
juce::Colour OrbitView::slotColour (int slot)
{
    static const juce::Colour colours[] = {
        juce::Colour (0xff9db0f5), juce::Colour (0xff9fd0b0), juce::Colour (0xffe2c48f), juce::Colour (0xffd59a9a),
        juce::Colour (0xff8fd0d6), juce::Colour (0xffbfa3e0), juce::Colour (0xffd8d58f), juce::Colour (0xffe0a8c8),
    };
    return colours[(size_t) ((slot % 8) + 8) % 8];
}

OrbitView::OrbitView (OrbitCatProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, false);
    juce::Random r (42);
    for (int i = 0; i < 110; ++i)
        stars.push_back ({ r.nextFloat(), r.nextFloat() });
    startTimerHz (60);
}

juce::Point<float> OrbitView::toScreen (float x, float y) const
{
    return { centre.x + x * pxPerAu, centre.y - y * pxPerAu * tiltCos };
}

void OrbitView::timerCallback()
{
    time += 1.0f / 60.0f;
    const auto bounds = getLocalBounds().toFloat();
    centre = bounds.getCentre();
    tiltCos = std::cos (juce::degreesToRadians (processor.apvts.getRawParameterValue ("tilt")->load()));

    // Zoom to fit the widest active orbit (smoothly)
    float extent = 10.0f;
    for (const auto& v : processor.bodyViews)
        if (v.active.load())
            extent = juce::jmax (extent, v.semiMajor.load() * (1.0f + v.eccentricity.load()));
    const float target = juce::jmin (bounds.getWidth() * 0.46f, bounds.getHeight() * 0.46f / juce::jmax (0.3f, tiltCos)) / extent;
    pxPerAu += (target - pxPerAu) * 0.08f;

    for (int i = 0; i < maxBodies; ++i)
    {
        const auto& v = processor.bodyViews[(size_t) i];
        if (! v.active.load()) { trailCount[(size_t) i] = 0; continue; }
        trailHead[(size_t) i] = (trailHead[(size_t) i] + 1) % trailLength;
        trails[(size_t) i][(size_t) trailHead[(size_t) i]] = { v.x.load(), v.y.load() };
        trailCount[(size_t) i] = juce::jmin (trailLength, trailCount[(size_t) i] + 1);
    }
    repaint();
}

void OrbitView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b0c10));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.reduceClipRegion (getLocalBounds());

    const float glow = processor.apvts.getRawParameterValue ("glow")->load() / 100.0f;

    // Stars (gently twinkling)
    for (size_t i = 0; i < stars.size(); ++i)
    {
        const float a = 0.12f + 0.18f * (0.5f + 0.5f * std::sin (time * 0.7f + (float) i * 1.7f));
        g.setColour (juce::Colours::white.withAlpha (a));
        g.fillEllipse (bounds.getX() + stars[i].x * bounds.getWidth(), bounds.getY() + stars[i].y * bounds.getHeight(), 1.4f, 1.4f);
    }

    // Alignment spokes
    const float align = juce::degreesToRadians (processor.apvts.getRawParameterValue ("align")->load());
    const int spokes = juce::roundToInt (processor.apvts.getRawParameterValue ("spokes")->load());
    for (int k = 0; k < spokes; ++k)
    {
        const float a = align + juce::MathConstants<float>::twoPi * (float) k / (float) spokes;
        const float far = bounds.getWidth() + bounds.getHeight();
        g.setColour (C::accent.withAlpha (0.22f));
        g.drawLine ({ centre, toScreen (std::cos (a) * far / pxPerAu, std::sin (a) * far / pxPerAu) }, 1.0f);
    }

    // Orbits
    for (int i = 0; i < maxBodies; ++i)
    {
        const auto& v = processor.bodyViews[(size_t) i];
        if (! v.active.load()) continue;
        const float a = v.semiMajor.load(), e = v.eccentricity.load(), w = v.omega.load();
        juce::Path orbit;
        for (int k = 0; k <= 96; ++k)
        {
            const float nu = juce::MathConstants<float>::twoPi * (float) k / 96.0f;
            const float r = a * (1.0f - e * e) / (1.0f + e * std::cos (nu));
            const auto pt = toScreen (r * std::cos (w + nu), r * std::sin (w + nu));
            if (k == 0) orbit.startNewSubPath (pt); else orbit.lineTo (pt);
        }
        g.setColour (slotColour (v.slot.load()).withAlpha (0.2f));
        g.strokePath (orbit, juce::PathStrokeType (1.0f));
    }

    // Sun
    {
        const float r = 9.0f + 26.0f * glow;
        juce::ColourGradient halo (C::accent.withAlpha (0.55f * glow), centre.x, centre.y,
                                   C::accent.withAlpha (0.0f), centre.x + r * 2.2f, centre.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (r * 4.4f, r * 4.4f).withCentre (centre));
        g.setColour (juce::Colour (0xfff3f1e8));
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (centre));
    }

    // Trails and planets
    for (int i = 0; i < maxBodies; ++i)
    {
        const auto& v = processor.bodyViews[(size_t) i];
        if (! v.active.load()) continue;
        const auto colour = slotColour (v.slot.load());

        for (int k = 1; k < trailCount[(size_t) i]; ++k)
        {
            const int idx = (trailHead[(size_t) i] - k + trailLength) % trailLength;
            const float alpha = 0.5f * (1.0f - (float) k / (float) trailLength);
            const float size = 1.5f + 2.5f * (1.0f - (float) k / (float) trailLength);
            g.setColour (colour.withAlpha (alpha));
            g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (toScreen (trails[(size_t) i][(size_t) idx].x, trails[(size_t) i][(size_t) idx].y)));
        }

        const auto p = toScreen (v.x.load(), v.y.load());
        const float size = 7.0f + v.mass.load() / 100.0f * 9.0f;
        const float flash = v.flash.load();
        if (flash > 0.02f)
        {
            const float ring = size * (1.4f + 1.6f * (1.0f - flash));
            g.setColour (colour.withAlpha (0.6f * flash * (0.4f + 0.6f * glow)));
            g.drawEllipse (juce::Rectangle<float> (ring * 2.0f, ring * 2.0f).withCentre (p), 1.5f);
            g.setColour (colour.withAlpha (0.25f * flash * glow));
            g.fillEllipse (juce::Rectangle<float> (size * 2.6f, size * 2.6f).withCentre (p));
        }
        g.setColour (colour.interpolatedWith (juce::Colours::white, 0.5f * flash));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (p));

        g.setColour (C::textDim);
        g.setFont (cat::catFont (10.5f));
        g.drawText (juce::MidiMessage::getMidiNoteName (v.note.load(), true, true, 3), juce::Rectangle<float> (40.0f, 12.0f).withCentre (p.translated (0.0f, size * 0.5f + 9.0f)),
                    juce::Justification::centred);
    }

    if (processor.bodyViews[0].active.load() == false)
    {
        bool any = false;
        for (const auto& v : processor.bodyViews) any = any || v.active.load();
        if (! any)
        {
            g.setColour (C::textDim);
            g.setFont (cat::catFont (13.0f));
            g.drawText ("Hold some notes - each one becomes a planet", bounds.withTrimmedTop (bounds.getHeight() * 0.62f).withHeight (20.0f),
                        juce::Justification::centred);
        }
    }
}

//==============================================================================
OrbitCatEditor::OrbitCatEditor (OrbitCatProcessor& p)
    : processor (p),
      latch (p.apvts, "latch", "Latch", &p.midiLearn),
      followGroove (p.apvts, "followGroove", "Follow Drum", &p.midiLearn),
      quantize (p.apvts, "quantize", "Quantize", &p.midiLearn, 62),
      gravity (p.apvts, "gravity", "Gravity", &p.midiLearn, 56),
      drag (p.apvts, "drag", "Drag", &p.midiLearn, 40),
      turbulence (p.apvts, "turbulence", "Turbulence", &p.midiLearn, 72),
      align (p.apvts, "align", "Align", &p.midiLearn, 42),
      accent (p.apvts, "accent", "Accent", &p.midiLearn, 50),
      tilt (p.apvts, "tilt", "Tilt", &p.midiLearn, 32),
      glow (p.apvts, "glow", "Glow", &p.midiLearn, 38),
      swing (p.apvts, "swing", "Swing", &p.midiLearn, 44),
      view (p),
      catView (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, cat::CatStyle::Orbit),
      surpriseControls (p, p.surpriseUndo, [&p] { p.surprise(); }),
      bootScreen (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); },
                  "ORBIT", "-cat", "ORBITAL ARPEGGIATOR", cat::CatStyle::Orbit)
{
    setLookAndFeel (&lnf);

    presetStepper = std::make_unique<cat::PresetBar> (p.presetManager);
    spokes = cat::Stepper::forParameter (p.apvts, "spokes", "Spokes");
    channel = cat::Stepper::forParameter (p.apvts, "outChannel", "Out", [] (int v) { return v == 0 ? juce::String ("In") : juce::String (v); });

    gravity.getSlider().setTooltip ("Pull of the sun (overall speed) and of the planets on each other");
    drag.getSlider().setTooltip ("Orbits slowly spiral inwards while held: the pattern speeds up");
    turbulence.getSlider().setTooltip ("Random wobble in every orbit");
    align.getSlider().setTooltip ("Angle of the trigger line: a planet plays its note when it crosses it");
    tilt.getSlider().setTooltip ("Viewing angle of the display");
    spokes->setTooltip ("Number of trigger lines - with 2 or more, eccentric orbits pulse unevenly");
    latch.setTooltip ("Keep orbiting after you let go (a new chord replaces it)");

    for (auto* c : std::initializer_list<juce::Component*> { presetStepper.get(), spokes.get(), channel.get(), &latch, &quantize,
                                                             &gravity, &drag, &turbulence, &align, &accent, &tilt, &glow, &view, &swing, &followGroove })
        addAndMakeVisible (c);

    for (int s = 0; s < numSlots; ++s)
    {
        auto* b = slotButtons.add (new juce::TextButton (juce::String (s + 1)));
        b->setTooltip ("Planet " + juce::String (s + 1) + " (held notes are numbered from the lowest)");
        b->onClick = [this, s] { selectSlot (s); };
        b->setColour (juce::TextButton::buttonOnColourId, OrbitView::slotColour (s));
        addAndMakeVisible (b);
    }

    addAndMakeVisible (catView);
    addAndMakeVisible (surpriseControls);
    addAndMakeVisible (bootScreen);

    selectSlot (0);
    setSize (1040, 700);
    startTimerHz (10);
}

OrbitCatEditor::~OrbitCatEditor()
{
    setLookAndFeel (nullptr);
}

void OrbitCatEditor::selectSlot (int slot)
{
    selectedSlot = slot;
    static const char* ids[] = { "mass", "radius", "ecc", "speed", "oct", "damp", "gate", "boost" };
    static const char* names[] = { "Mass", "Radius", "Eccent", "Speed", "Octave", "Damp", "Gate", "Boost" };
    static const int widths[] = { 44, 50, 52, 46, 50, 46, 40, 46 };
    for (int i = 0; i < 8; ++i)
    {
        slotSliders[(size_t) i] = std::make_unique<cat::BarSlider> (processor.apvts, OrbitCatProcessor::slotParam (slot, ids[i]),
                                                                    names[i], &processor.midiLearn, widths[i]);
        slotSliders[(size_t) i]->getSlider().setColour (juce::Slider::trackColourId, OrbitView::slotColour (slot));
        addAndMakeVisible (*slotSliders[(size_t) i]);
    }
    for (int s = 0; s < slotButtons.size(); ++s)
        slotButtons[s]->setToggleState (s == slot, juce::dontSendNotification);
    bootScreen.toFront (false);
    resized();
}

void OrbitCatEditor::timerCallback()
{
    // Slot chips show which note currently uses each planet's settings
    std::array<int, numSlots> notes;
    notes.fill (-1);
    for (const auto& v : processor.bodyViews)
        if (v.active.load())
        {
            const int s = v.slot.load();
            if (notes[(size_t) s] < 0 || v.note.load() < notes[(size_t) s]) notes[(size_t) s] = v.note.load();
        }
    for (int s = 0; s < numSlots; ++s)
        slotButtons[s]->setButtonText (notes[(size_t) s] >= 0 ? juce::String (s + 1) + "  " + juce::MidiMessage::getMidiNoteName (notes[(size_t) s], true, true, 3)
                                                              : juce::String (s + 1));
}

void OrbitCatEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::background);
    cat::CatLookAndFeel::drawLogo (g, juce::Rectangle<float> (14.0f, 8.0f, 160.0f, 32.0f), "ORBIT", "-cat");

    g.setColour (C::textDim);
    g.setFont (cat::catFont (11.0f, true));
    const auto planetRow = slotButtons[0]->getBounds();
    g.drawText ("SYSTEM", juce::Rectangle<int> (12, gravity.getY() - 16, 100, 14), juce::Justification::centredLeft);
    g.drawText ("PLANET", juce::Rectangle<int> (12, planetRow.getY() - 16, 100, 14), juce::Justification::centredLeft);
}

void OrbitCatEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto item = [] (juce::Component& c, float w, float h = 30.0f) { return juce::FlexItem (c).withWidth (w).withHeight (h).withMargin ({ 0, 3, 0, 3 }); };

    {
        auto top = r.removeFromTop (40);
        auto logo = top.removeFromLeft (160);
        catView.setBounds (logo.removeFromRight (40).expanded (0, 6));
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        fb.items.add (juce::FlexItem().withFlex (1.0f));
        fb.items.add (item (*presetStepper, 330));
        fb.items.add (juce::FlexItem().withFlex (1.0f));
        fb.items.add (item (latch, 64));
        fb.items.add (item (followGroove, 92));
        fb.items.add (item (*channel, 90));
        fb.items.add (item (surpriseControls, 122));
        fb.performLayout (top);
    }
    r.removeFromTop (8);

    // Planet (per-note) rows at the bottom, system row above them
    auto planetRow = r.removeFromBottom (30);
    r.removeFromBottom (6);
    auto chipRow = r.removeFromBottom (26);
    r.removeFromBottom (22);
    auto systemRow = r.removeFromBottom (30);
    r.removeFromBottom (22);
    view.setBounds (r);

    {
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        for (auto* s : { &gravity, &drag, &turbulence, &align, &accent, &tilt, &glow, &swing })
            fb.items.add (item (*s, 110, 28).withFlex (1.0f));
        fb.items.add (item (quantize, 132, 28));
        fb.items.add (item (*spokes, 112, 28));
        fb.performLayout (systemRow);
    }
    {
        const int chipW = juce::jmin (90, chipRow.getWidth() / 8);
        for (int s = 0; s < slotButtons.size(); ++s)
            slotButtons[s]->setBounds (chipRow.removeFromLeft (chipW).reduced (2, 0));
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        for (auto& s : slotSliders)
            if (s != nullptr) fb.items.add (item (*s, 80, 28).withFlex (1.0f));
        fb.performLayout (planetRow);
    }

    bootScreen.setBounds (getLocalBounds());
}

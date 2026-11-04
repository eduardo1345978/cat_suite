#include "PluginEditor.h"

using namespace orbitcat;
namespace C = cat::Colours;

//==============================================================================
juce::Colour OrbitView::slotColour (int slot)
{
    static const juce::Colour colours[] = {
        juce::Colour (0xff7fd8ff), juce::Colour (0xffff8fc8), juce::Colour (0xff9bffb0), juce::Colour (0xffffd27a),
        juce::Colour (0xffb79bff), juce::Colour (0xffff9f7a), juce::Colour (0xff7affe0), juce::Colour (0xffe8ecff),
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
    const auto bounds = getLocalBounds().toFloat();   // transparent: deep space is the window itself
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
        juce::Path spoke;
        spoke.startNewSubPath (centre);
        spoke.lineTo (toScreen (std::cos (a) * far / pxPerAu, std::sin (a) * far / pxPerAu));
        const float dashes[] = { 4.0f, 6.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (spoke, spoke, dashes, 2);
        g.setColour (juce::Colour (0xffffd27a).withAlpha (0.35f));
        g.fillPath (spoke);
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
        g.setColour (slotColour (v.slot.load()).withAlpha (0.35f));
        g.strokePath (orbit, juce::PathStrokeType (1.2f));
    }

    // Sun
    {
        // a golden star: white-hot core, gold body, orange corona
        const float r = 34.0f + 40.0f * glow;
        juce::ColourGradient halo (juce::Colour (0xfffff6d8), centre.x, centre.y, juce::Colour (0x00ff8a2a), centre.x + r, centre.y, true);
        halo.addColour (0.4, juce::Colour (0xffffc44d).withAlpha (0.9f));
        halo.addColour (0.7, juce::Colour (0xffff8a2a).withAlpha (0.35f));
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
        g.setColour (juce::Colour (0xfffff3cf));
        g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (centre));
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
        juce::ColourGradient halo2 (colour.withAlpha (0.55f), p.x, p.y, colour.withAlpha (0.0f), p.x + size * 1.4f, p.y, true);
        g.setGradientFill (halo2);
        g.fillEllipse (juce::Rectangle<float> (size * 2.8f, size * 2.8f).withCentre (p));
        g.setColour (colour.interpolatedWith (juce::Colours::white, 0.5f * flash));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (p));

        g.setColour (juce::Colour (0xffc9d1f5));
        g.setFont (cat::displayFont (12.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (v.note.load(), true, true, 3),
                    juce::Rectangle<float> (p.x + size * 0.5f + 6.0f, p.y - 7.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
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
      gravity (p.apvts, "gravity", "Gravity", &p.midiLearn, juce::Colour (0xff7fd8ff), juce::Colour (0xffffd27a)),
      drag (p.apvts, "drag", "Drag", &p.midiLearn, juce::Colour (0xff7fd8ff), juce::Colour (0xffffd27a)),
      turbulence (p.apvts, "turbulence", "Turbulence", &p.midiLearn, juce::Colour (0xff7fd8ff), juce::Colour (0xffffd27a)),
      align (p.apvts, "align", "Align", &p.midiLearn, juce::Colour (0xff7fd8ff), juce::Colour (0xffffd27a)),
      accent (p.apvts, "accent", "Accent", &p.midiLearn, juce::Colour (0xff7fd8ff), juce::Colour (0xffffd27a)),
      tilt (p.apvts, "tilt", "Tilt", &p.midiLearn, juce::Colour (0xff7fd8ff), juce::Colour (0xffffd27a)),
      glow (p.apvts, "glow", "Glow", &p.midiLearn, juce::Colour (0xff7fd8ff), juce::Colour (0xffffd27a)),
      swing (p.apvts, "swing", "Swing", &p.midiLearn, juce::Colour (0xff7fd8ff), juce::Colour (0xffffd27a)),
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
    for (int i = 0; i < 8; ++i)
    {
        slotSliders[(size_t) i] = std::make_unique<cat::LineSlider> (processor.apvts, OrbitCatProcessor::slotParam (slot, ids[i]),
                                                                     names[i], &processor.midiLearn,
                                                                     OrbitView::slotColour (slot).withMultipliedAlpha (0.6f), OrbitView::slotColour (slot));
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
    cat::paintBackdrop (g, getLocalBounds());

    // ORBIT / C A T
    g.setColour (juce::Colour (0xffe8ecff));
    g.setFont (cat::displayFont (30.0f).withExtraKerningFactor (0.3f));
    g.drawText ("ORBIT", 28, 18, 220, 36, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff9aa6d8));
    g.setFont (cat::catFont (14.0f).withExtraKerningFactor (0.5f));
    g.drawText ("CAT", 30, 54, 120, 18, juce::Justification::centredLeft);

    auto glass = [&g] (juce::Rectangle<int> r, float radius)
    {
        g.setColour (juce::Colour (0xff141a3a).withAlpha (0.6f));
        g.fillRoundedRectangle (r.toFloat(), radius);
        g.setColour (juce::Colour (0xffa0b4ff).withAlpha (0.2f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), radius, 1.0f);
    };
    glass (asideBounds, 20.0f);
    glass (dockBounds, 18.0f);

    g.setColour (juce::Colour (0xff9aa6d8));
    g.setFont (cat::catFont (12.0f).withExtraKerningFactor (0.3f));
    g.drawText ("SYSTEM", asideBounds.getX() + 18, asideBounds.getY() + 14, 200, 16, juce::Justification::centredLeft);
    g.drawText ("PLANET", dockBounds.getX() + 18, dockBounds.getY() + 12, 100, 16, juce::Justification::centredLeft);
}

void OrbitCatEditor::resized()
{
    auto r = getLocalBounds().reduced (22);

    // the glass side panel: system controls
    asideBounds = r.removeFromRight (250);
    r.removeFromRight (18);
    {
        auto a = asideBounds.reduced (18, 14);
        a.removeFromTop (22);
        presetStepper->setBounds (a.removeFromTop (32));
        a.removeFromTop (10);
        auto two = [&a] (juce::Component& x, juce::Component& y)
        {
            auto row = a.removeFromTop (30);
            x.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4));
            row.removeFromLeft (8);
            y.setBounds (row);
            a.removeFromTop (8);
        };
        two (latch, followGroove);
        two (*channel, *spokes);
        quantize.setBounds (a.removeFromTop (30));
        a.removeFromTop (14);
        surpriseControls.setBounds (a.removeFromBottom (36).withSizeKeepingCentre (a.getWidth(), 36));
        a.removeFromBottom (10);
        const int h = juce::jmin (40, a.getHeight() / 8);
        for (auto* s : { &gravity, &drag, &turbulence, &align, &accent, &tilt, &glow, &swing })
            s->setBounds (a.removeFromTop (h).reduced (0, 3));
    }

    // the planet dock along the bottom of space
    dockBounds = r.removeFromBottom (128);
    r.removeFromBottom (10);
    {
        auto d = dockBounds.reduced (18, 10);
        auto chips = d.removeFromTop (26);
        chips.removeFromLeft (70);
        const int chipW = juce::jmin (78, chips.getWidth() / 8);
        for (int s = 0; s < slotButtons.size(); ++s)
            slotButtons[s]->setBounds (chips.removeFromLeft (chipW).reduced (3, 0));
        d.removeFromTop (8);
        const int w = d.getWidth() / 4;
        for (int row = 0; row < 2; ++row)
        {
            auto line = d.removeFromTop (d.getHeight() / (2 - row));
            for (int i = 0; i < 4; ++i)
                if (auto& s = slotSliders[(size_t) (row * 4 + i)]; s != nullptr)
                    s->setBounds (line.removeFromLeft (w).reduced (8, 3));
        }
    }

    catView.setBounds (150, 18, 52, 52);
    view.setBounds (r.withTrimmedTop (60));
    bootScreen.setBounds (getLocalBounds());
}

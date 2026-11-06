#include "PluginEditor.h"

using namespace phasecat;

namespace
{
    constexpr int designW = 1140, designH = 690;
    const juce::Colour laneColours[numLanes] = { cat::Colours::accent, cat::Colours::red, cat::Colours::green, cat::Colours::purple };
    juce::String lid (int lane, const char* what) { return "l" + juce::String (lane + 1) + what; }
}

//==============================================================================
StepGrid::StepGrid (PhaseCatProcessor& p, int l, juce::Colour c) : processor (p), lane (l), colour (c)
{
    setTooltip ("Click a step: on / off. Drag up / down: how loud. Drag across: paint");
}

int StepGrid::stepAt (float x) const
{
    const float w = (float) getWidth() / maxSteps;
    return juce::jlimit (0, maxSteps - 1, (int) (x / w));
}

void StepGrid::paint (juce::Graphics& g)
{
    const auto s = processor.currentSettings().lanes[(size_t) lane];
    const float w = (float) getWidth() / maxSteps, h = (float) getHeight();
    const int now = (int) processor.engine.position[(size_t) lane].load();
    for (int i = 0; i < maxSteps; ++i)
    {
        auto r = juce::Rectangle<float> (i * w, 0.0f, w, h).reduced (1.5f, 0.0f);
        const bool inside = i < s.length;
        g.setColour (juce::Colours::white.withAlpha (inside ? (i % 4 == 0 ? 0.10f : 0.06f) : 0.02f));
        g.fillRoundedRectangle (r, 3.0f);
        const float v = processor.getStep (lane, i);
        if (v > 0.0f)
        {
            g.setColour (colour.withAlpha (inside ? (s.on ? 0.9f : 0.35f) : 0.15f));
            g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() * (1.0f - v)), 3.0f);
        }
        if (inside && i == now && processor.isRunning())
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 3.0f, 1.5f);
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    dragStep = stepAt (e.position.x);
    dragStartValue = processor.getStep (lane, dragStep);
    paintValue = dragStartValue > 0.0f ? 0.0f : 1.0f;
    painting = false;
    processor.setStep (lane, dragStep, paintValue);
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    const int st = stepAt (e.position.x);
    if (st != dragStep) painting = true;
    if (painting)
        processor.setStep (lane, st, paintValue);                        // drag across: paint on / off
    else
        processor.setStep (lane, dragStep, juce::jlimit (0.05f, 1.0f, (dragStartValue > 0.0f ? dragStartValue : 1.0f) - e.getDistanceFromDragStartY() / 80.0f));
    repaint();
}

//==============================================================================
RingView::RingView (PhaseCatProcessor& p) : processor (p)
{
    setTooltip ("Every ring is a lane: its steps round the circle, the hand turning at the lane's speed");
}

void RingView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    cat::CatLookAndFeel::drawGlassPanel (g, b, 10.0f);
    const auto c = b.getCentre();
    const float maxR = juce::jmin (b.getWidth(), b.getHeight()) * 0.44f;
    const auto st = processor.currentSettings();
    for (int l = 0; l < numLanes; ++l)
    {
        const auto& lane = st.lanes[(size_t) l];
        const float r = maxR * (1.0f - 0.2f * (float) l);
        const auto col = laneColours[l].withAlpha (lane.on ? 1.0f : 0.25f);
        g.setColour (col.withAlpha (0.18f * col.getFloatAlpha()));
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 1.5f);
        for (int i = 0; i < lane.length; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) lane.length - juce::MathConstants<float>::halfPi;
            const auto pt = c + juce::Point<float> (std::cos (a), std::sin (a)) * r;
            const float v = lane.steps[(size_t) i];
            g.setColour (v > 0.0f ? col : col.withAlpha (0.2f * col.getFloatAlpha()));
            const float d = v > 0.0f ? 4.0f + 6.0f * v : 3.0f;
            g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (pt));
        }
        // the hand
        const float pos = processor.engine.position[(size_t) l].load();
        const float a = juce::MathConstants<float>::twoPi * pos / (float) juce::jmax (1, lane.length) - juce::MathConstants<float>::halfPi;
        const auto tip = c + juce::Point<float> (std::cos (a), std::sin (a)) * r;
        g.setColour (col.withAlpha (0.35f + 0.65f * juce::jlimit (0.0f, 1.0f, flash[(size_t) l])));
        g.drawLine ({ c + juce::Point<float> (std::cos (a), std::sin (a)) * (r - 14.0f), tip }, 3.0f);
        const float fs = 10.0f + 14.0f * flash[(size_t) l];
        g.fillEllipse (juce::Rectangle<float> (fs, fs).withCentre (tip));
        // Risset: the three tempo layers' loudness
        if (lane.risset)
        {
            for (int k = 0; k < numLayers; ++k)
            {
                const float lv = processor.engine.layerLevel[(size_t) l][(size_t) k].load();
                auto bar = juce::Rectangle<float> (b.getX() + 14.0f + k * 9.0f, b.getBottom() - 20.0f - 18.0f * l - 12.0f, 7.0f, 12.0f);
                g.setColour (col.withAlpha (0.2f));
                g.fillRect (bar);
                g.setColour (col);
                g.fillRect (bar.withTrimmedTop (bar.getHeight() * (1.0f - lv)));
            }
        }
    }
    // the beat now (metric modulation)
    const double ratio = processor.engine.ratio.load();
    g.setColour (cat::Colours::text);
    g.setFont (cat::catFont (15.0f, true));
    g.drawText ("x " + juce::String (ratio, 3).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."), b.withSizeKeepingCentre (120.0f, 24.0f).translated (0.0f, -10.0f), juce::Justification::centred);
    g.setColour (cat::Colours::textDim);
    g.setFont (cat::catFont (11.0f));
    g.drawText ("feels like " + juce::String (juce::roundToInt (processor.getBpm() * ratio)) + " BPM", b.withSizeKeepingCentre (160.0f, 18.0f).translated (0.0f, 12.0f), juce::Justification::centred);
}

//==============================================================================
PhaseCatEditor::LaneControls::LaneControls (PhaseCatProcessor& p, int l)
{
    on = std::make_unique<cat::ParamToggle> (p.apvts, lid (l, "On"), "Lane " + juce::String (l + 1));
    risset = std::make_unique<cat::ParamToggle> (p.apvts, lid (l, "Risset"), "Risset");
    note = std::make_unique<cat::BarSlider> (p.apvts, lid (l, "Note"), "Note", nullptr, 38);
    length = std::make_unique<cat::BarSlider> (p.apvts, lid (l, "Len"), "Steps", nullptr, 42);
    drift = std::make_unique<cat::BarSlider> (p.apvts, lid (l, "Drift"), "Drift", nullptr, 38);
    rate = std::make_unique<cat::ParamCombo> (p.apvts, lid (l, "Rate"), "Rate", nullptr, 36);
    grid = std::make_unique<StepGrid> (p, l, laneColours[l]);
    on->setTooltip ("This lane plays or rests");
    risset->setTooltip ("Risset rhythm: the lane plays in three tempos an octave apart whose loudness follows a curve; "
                        "together they seem to speed up forever but never get faster");
    note->getSlider().setTooltip ("The drum note it plays (General MIDI: 36 kick, 38 snare, 42 hi-hat ...)");
    length->getSlider().setTooltip ("How many steps before it repeats: different lengths in different lanes = polymeter");
    drift->getSlider().setTooltip ("Phasing: this lane runs a little faster (+) or slower (-) and slides away from the others, then meets them again");
    rate->getCombo().setTooltip ("How long one step is");
}

PhaseCatEditor::PhaseCatEditor (PhaseCatProcessor& p) : processor (p)
{
    setSize (designW, designH);
    for (int l = 0; l < numLanes; ++l)
    {
        auto lc = std::make_unique<LaneControls> (p, l);
        for (juce::Component* c : { (juce::Component*) lc->on.get(), (juce::Component*) lc->risset.get(), (juce::Component*) lc->note.get(),
                                    (juce::Component*) lc->length.get(), (juce::Component*) lc->drift.get(), (juce::Component*) lc->rate.get(),
                                    (juce::Component*) lc->grid.get() })
            addAndMakeVisible (c);
        lanes.push_back (std::move (lc));
    }
    const auto names = modulationNames();
    for (int i = 0; i < numModulations; ++i)
    {
        auto* b = modButtons.add (new juce::TextButton (names[i].upToFirstOccurrenceOf ("  ", false, false)));
        b->setTooltip ("Metric modulation at the next bar: the beat " + names[i]);
        b->onClick = [this, i] { processor.engine.requestModulation (i); };
        addAndMakeVisible (b);
    }
    resetModButton.setTooltip ("Back to Cubase's beat at the next bar");
    resetModButton.onClick = [this] { processor.engine.requestResetModulation(); };
    realignButton.setTooltip ("All lanes back together (phasing starts again) at the next bar");
    realignButton.onClick = [this] { processor.engine.requestRealign(); };
    playButton.setClickingTogglesState (true);
    playButton.setTooltip ("Play without Cubase (Cubase's Play also starts it)");
    playButton.onClick = [this] { processor.freeRun = playButton.getToggleState(); };
    rissetBars.getSlider().setTooltip ("How many bars one turn of the endless speed-up takes");
    rissetDown.setTooltip ("Risset lanes seem to slow down forever instead");
    gate.getSlider().setTooltip ("How long the notes are (part of a step)");
    channel.getSlider().setTooltip ("MIDI channel out (10 = drums)");
    for (juce::Component* c : { (juce::Component*) &presetBar, (juce::Component*) &playButton, (juce::Component*) &channel, (juce::Component*) &rings,
                                (juce::Component*) &resetModButton, (juce::Component*) &realignButton, (juce::Component*) &rissetBars,
                                (juce::Component*) &gate, (juce::Component*) &rissetDown })
        addAndMakeVisible (c);
    resized();
    startTimerHz (30);
}

PhaseCatEditor::~PhaseCatEditor() { stopTimer(); }

void PhaseCatEditor::timerCallback()
{
    for (int l = 0; l < numLanes; ++l)
    {
        const float f = processor.engine.flash[(size_t) l].exchange (0.0f);
        rings.flash[(size_t) l] = juce::jmax (f, rings.flash[(size_t) l] * 0.82f);
        lanes[(size_t) l]->grid->repaint();
    }
    if (playButton.getToggleState() != processor.freeRun.load()) playButton.setToggleState (processor.freeRun.load(), juce::dontSendNotification);
    rings.repaint();
}

void PhaseCatEditor::paint (juce::Graphics& g)
{
    cat::paintBackdrop (g, getLocalBounds());
    cat::CatLookAndFeel::drawLogo (g, logoArea.toFloat(), "PHASE", "CAT");
    g.setColour (cat::Colours::textDim);
    g.setFont (cat::catFont (12.5f, true));
    g.drawText ("METRIC MODULATION  (at the next bar)", modArea.withHeight (18), juce::Justification::centredLeft);
    g.drawText ("RISSET RHYTHM + NOTES", rissetArea.withHeight (18), juce::Justification::centredLeft);
    g.setFont (cat::catFont (11.5f));
    g.drawFittedText ("Put Phase-Cat on a MIDI track and route its MIDI Out to Drum-Cat or any drum instrument (channel " + juce::String (juce::roundToInt (processor.apvts.getRawParameterValue ("channel")->load()))
                      + "). It follows Cubase's tempo and Play.", infoArea, juce::Justification::centredLeft, 1);
}

void PhaseCatEditor::resized()
{
    if (lanes.size() < (size_t) numLanes || modButtons.size() < numModulations) return;
    auto r = getLocalBounds().reduced (14, 10);
    auto top = r.removeFromTop (40);
    logoArea = top.removeFromLeft (170);
    top.removeFromLeft (10);
    presetBar.setBounds (top.removeFromLeft (470));
    playButton.setBounds (top.removeFromRight (110));
    top.removeFromRight (10);
    channel.setBounds (top.removeFromRight (120));
    infoArea = getLocalBounds().removeFromBottom (22).reduced (16, 0);
    r.removeFromBottom (18);
    r.removeFromTop (12);

    auto right = r.removeFromRight (380);
    r.removeFromRight (14);
    laneArea = r;
    const int laneH = r.getHeight() / numLanes;
    for (int l = 0; l < numLanes; ++l)
    {
        auto row = r.removeFromTop (laneH).reduced (0, 6);
        auto& lc = *lanes[(size_t) l];
        auto head = row.removeFromTop (30);
        lc.on->setBounds (head.removeFromLeft (84)); head.removeFromLeft (6);
        lc.note->setBounds (head.removeFromLeft (150)); head.removeFromLeft (6);
        lc.rate->setBounds (head.removeFromLeft (160)); head.removeFromLeft (6);
        lc.length->setBounds (head.removeFromLeft (110)); head.removeFromLeft (6);
        lc.risset->setBounds (head.removeFromRight (74)); head.removeFromRight (6);
        lc.drift->setBounds (head);
        row.removeFromTop (6);
        lc.grid->setBounds (row);
    }

    auto ringsArea = right.removeFromTop (right.getHeight() - 230);
    rings.setBounds (ringsArea);
    right.removeFromTop (10);
    modArea = right.removeFromTop (110);
    auto m = modArea.withTrimmedTop (22);
    auto row1 = m.removeFromTop (38), row2 = m.removeFromTop (44).withTrimmedTop (6);
    const int bw = (row1.getWidth() - 10) / 3;
    for (int i = 0; i < 3; ++i) { modButtons[i * 2]->setBounds (row1.removeFromLeft (bw)); row1.removeFromLeft (5); }
    for (int i = 0; i < 3; ++i) { modButtons[i * 2 + 1]->setBounds (row2.removeFromLeft (bw)); row2.removeFromLeft (5); }
    right.removeFromTop (6);
    rissetArea = right;
    auto rr = rissetArea.withTrimmedTop (22);
    auto a = rr.removeFromTop (32);
    resetModButton.setBounds (a.removeFromLeft (a.getWidth() / 2 - 3)); a.removeFromLeft (6);
    realignButton.setBounds (a);
    rr.removeFromTop (8);
    auto b2 = rr.removeFromTop (32);
    rissetBars.setBounds (b2.removeFromLeft (150)); b2.removeFromLeft (6);
    rissetDown.setBounds (b2.removeFromLeft (110)); b2.removeFromLeft (6);
    gate.setBounds (b2);
}

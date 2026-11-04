#include "PluginEditor.h"

using namespace bendcat;
namespace C = cat::Colours;

//==============================================================================
EnvelopePanel::EnvelopePanel (BendCatProcessor& p)
    : playMode (p.apvts, "envPlayMode", "PlayMode", &p.midiLearn, 66),
      length (p.apvts, "envLength", "Length", &p.midiLearn, 52),
      palette (p), editor (p),
      depth (p.apvts, "envDepth", "Depth", &p.midiLearn),
      random (p.apvts, "envRandom", "Random", &p.midiLearn),
      chance (p.apvts, "envChance", "Chance", &p.midiLearn),
      loop (p.apvts, "envLoop", "Loop", &p.midiLearn),
      duration (p.apvts, "envDuration", "Free Time", &p.midiLearn)
{
    cycles = cat::Stepper::forParameter (p.apvts, "envCycles", "Cycle");
    cycles->setCaptionWidth (44);

    random.getSlider().setTooltip ("Per-note random reduction of the depth");
    chance.getSlider().setTooltip ("Probability that a note gets the envelope at all");
    loop.setTooltip ("Retrigger mode: loop the envelope while the note is held");

    for (auto* c : std::initializer_list<juce::Component*> { &playMode, &length, &palette, &editor, cycles.get(), &depth, &random, &chance, &loop, &duration })
        addAndMakeVisible (c);
}

void EnvelopePanel::resized()
{
    // Liquid Chrome: a row of settings, the big chrome curve, a row of chrome knobs
    auto r = getLocalBounds().reduced (8, 6);
    auto top = r.removeFromTop (30);
    playMode.setBounds (top.removeFromLeft (200));
    top.removeFromLeft (10);
    length.setBounds (top.removeFromLeft (190));
    top.removeFromLeft (10);
    cycles->setBounds (top.removeFromLeft (118));
    top.removeFromLeft (10);
    loop.setBounds (top.removeFromLeft (70));
    r.removeFromTop (8);

    auto knobs = r.removeFromBottom (104);
    r.removeFromBottom (8);
    palette.setBounds (r.removeFromLeft (150));
    r.removeFromLeft (10);
    editor.setBounds (r);

    const int kw = knobs.getWidth() / 4;
    for (auto* k : std::initializer_list<juce::Component*> { &depth, &random, &chance, &duration })
        k->setBounds (knobs.removeFromLeft (kw).withSizeKeepingCentre (juce::jmin (kw, 120), knobs.getHeight()));
}

//==============================================================================
GlidePanel::GlidePanel (BendCatProcessor& p)
    : time (p.apvts, "glideTime", "Glide Time", &p.midiLearn),
      curve (p.apvts, "glideCurve", "Curve", &p.midiLearn, 48),
      preview (p, CurvePreview::Kind::Glide)
{
    addAndMakeVisible (time);
    addAndMakeVisible (curve);
    addAndMakeVisible (preview);
}

void GlidePanel::paint (juce::Graphics& g)
{
    g.setColour (C::textDim);
    g.setFont (cat::catFont (12.0f));
    g.drawFittedText ("Portamento between chord changes: every new voice starts at the nearest tone of the previous chord "
                      "and glides to its own pitch (sample-accurate, per MPE channel).",
                      getLocalBounds().reduced (12).removeFromBottom (36), juce::Justification::centredLeft, 2);
}

void GlidePanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromBottom (40);
    auto left = r.removeFromLeft (180);
    time.setBounds (left.removeFromTop (120));
    left.removeFromTop (10);
    curve.setBounds (left.removeFromTop (28));
    r.removeFromLeft (10);
    preview.setBounds (r);
}

//==============================================================================
LfoPanel::LfoPanel (BendCatProcessor& p)
    : rate (p.apvts, "lfoRate", "Rate", &p.midiLearn),
      depth (p.apvts, "lfoDepth", "Depth", &p.midiLearn),
      voiceRandom (p.apvts, "lfoVoiceRandom", "Voice Random", &p.midiLearn, C::blue),
      sync (p.apvts, "lfoSync", "Host Sync", &p.midiLearn),
      phaseRandom (p.apvts, "lfoPhaseRandom", "Random Phase", &p.midiLearn),
      division (p.apvts, "lfoDivision", "Sync", &p.midiLearn, 40),
      wave (p.apvts, "lfoWave", "Wave", &p.midiLearn, 40),
      preview (p, CurvePreview::Kind::LFO)
{
    for (auto* c : std::initializer_list<juce::Component*> { &rate, &depth, &voiceRandom, &sync, &phaseRandom, &division, &wave, &preview })
        addAndMakeVisible (c);
}

void LfoPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto left = r.removeFromLeft (330);
    auto knobs = left.removeFromTop (110);
    const int kw = knobs.getWidth() / 3;
    rate.setBounds (knobs.removeFromLeft (kw));
    depth.setBounds (knobs.removeFromLeft (kw));
    voiceRandom.setBounds (knobs);
    left.removeFromTop (10);
    auto row = [&left] { auto x = left.removeFromTop (28); left.removeFromTop (6); return x; };
    wave.setBounds (row());
    {
        auto line = row();
        sync.setBounds (line.removeFromLeft (line.getWidth() / 2 - 3));
        line.removeFromLeft (6);
        division.setBounds (line);
    }
    phaseRandom.setBounds (row().removeFromLeft (160));
    r.removeFromLeft (10);
    preview.setBounds (r);
}

//==============================================================================
PatternPanel::PatternPanel (BendCatProcessor& p)
    : editor (p),
      division (p.apvts, "patDivision", "Step", &p.midiLearn, 40),
      smoothing (p.apvts, "patSmoothing", "Smoothing", &p.midiLearn)
{
    addAndMakeVisible (editor);
    addAndMakeVisible (division);
    addAndMakeVisible (smoothing);
}

void PatternPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto right = r.removeFromRight (140);
    division.setBounds (right.removeFromTop (28));
    right.removeFromTop (10);
    smoothing.setBounds (right.removeFromTop (110));
    r.removeFromRight (10);
    editor.setBounds (r);
}

//==============================================================================
BendTab::BendTab (BendCatProcessor& p)
    : processor (p), envelopePanel (p), glidePanel (p), lfoPanel (p), patternPanel (p)
{
    const auto names = getModeNames();
    auto* param = p.apvts.getParameter ("mode");

    modeAttachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { showMode (juce::roundToInt (v)); });

    for (int i = 0; i < 5; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (names[i]);
        b.setRadioGroupId (9001);
        b.setClickingTogglesState (true);
        b.onClick = [this, i] { if (modeButtons[(size_t) i].getToggleState()) modeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    addChildComponent (envelopePanel);
    addChildComponent (glidePanel);
    addChildComponent (lfoPanel);
    addChildComponent (patternPanel);
    modeAttachment->sendInitialUpdate();
}

void BendTab::showMode (int mode)
{
    currentMode = mode;
    for (int i = 0; i < 5; ++i)
        modeButtons[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);
    glidePanel.setVisible (mode == ModeGlide);
    envelopePanel.setVisible (mode == ModeEnvelope);
    lfoPanel.setVisible (mode == ModeLFO);
    patternPanel.setVisible (mode == ModePattern);
    repaint();
}

void BendTab::paint (juce::Graphics& g)
{
    if (currentMode == ModeOff)
    {
        g.setColour (C::textDim);
        g.setFont (cat::catFont (14.0f));
        g.drawFittedText ("Bend modulation is off. Detune, drift and microtuning (DETUNE tab) are still applied per voice.",
                          getLocalBounds().reduced (40), juce::Justification::centred, 3);
    }
}

void BendTab::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto bar = r.removeFromTop (30);
    const int w = juce::jmin (110, bar.getWidth() / 5);
    for (auto& b : modeButtons) b.setBounds (bar.removeFromLeft (w).reduced (2, 0));
    r.removeFromTop (4);
    for (auto* c : std::initializer_list<juce::Component*> { &envelopePanel, &glidePanel, &lfoPanel, &patternPanel })
        c->setBounds (r);
}

//==============================================================================
DetuneTab::DetuneTab (BendCatProcessor& p)
    : processor (p),
      detune (p.apvts, "detune", "Human Detune", &p.midiLearn),
      drift (p.apvts, "driftCents", "Drift", &p.midiLearn, C::blue),
      driftRate (p.apvts, "driftRate", "Drift Rate", &p.midiLearn, C::blue),
      scalaEnabled (p.apvts, "scalaEnabled", "Microtuning On", &p.midiLearn)
{
    detune.getSlider().setTooltip ("Constant random offset per voice (+- cents)");
    drift.getSlider().setTooltip ("Slow analog-style pitch drift per voice");

    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load Scala tuning", juce::File(), "*.scl");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            juce::String error;
            if (! processor.loadScala (f, error))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Bend-Cat", "Could not load " + f.getFileName() + ":\n" + error);
        });
    };
    clearButton.onClick = [this] { processor.clearScala(); };

    reference = cat::Stepper::forParameter (p.apvts, "scalaRef", "Ref Note", [] (int v) { return cat::cubaseNoteName (v); });

    for (auto* c : std::initializer_list<juce::Component*> { &detune, &drift, &driftRate, &scalaEnabled, &loadButton, &clearButton, reference.get() })
        addAndMakeVisible (c);
    startTimerHz (4);
    timerCallback();
}

void DetuneTab::timerCallback()
{
    static int lastRef = -1;
    const int ref = juce::roundToInt (processor.apvts.getRawParameterValue ("scalaRef")->load());
    if (processor.dataVersion.load() != shownVersion || ref != lastRef)
    {
        shownVersion = processor.dataVersion.load();
        lastRef = ref;
        description = processor.getScalaDescription();
        numDegrees = processor.getScalaNumDegrees();
        table = processor.getTuningTable();
        repaint();
    }
}

void DetuneTab::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (150);

    cat::CatLookAndFeel::drawGlassPanel (g, r.toFloat(), 6.0f, C::panelLight);
    auto inner = r.reduced (12);
    g.setColour (C::text);
    g.setFont (cat::catFont (13.0f, true));
    g.drawText (numDegrees > 0 ? "Scala: " + (description.isNotEmpty() ? description : juce::String ("(no description)"))
                                   + "   (" + juce::String (numDegrees) + " notes per period)"
                               : juce::String ("No Scala tuning loaded (12-TET)"),
                inner.removeFromTop (20), juce::Justification::centredLeft);
    inner.removeFromTop (8);

    const int ref = juce::roundToInt (processor.apvts.getRawParameterValue ("scalaRef")->load());
    const float w = inner.getWidth() / 12.0f;
    for (int i = 0; i < 12; ++i)
    {
        const int note = juce::jlimit (0, 127, ref + i);
        const float cents = table[(size_t) note];
        auto cell = juce::Rectangle<float> (inner.getX() + i * w, (float) inner.getY(), w - 4.0f, (float) inner.getHeight());
        g.setColour (juce::Colour (0xff1b1e25));
        g.fillRoundedRectangle (cell, 4.0f);

        const float barH = juce::jlimit (-1.0f, 1.0f, cents / 100.0f) * (cell.getHeight() * 0.35f);
        g.setColour (C::blue.withAlpha (0.8f));
        g.fillRect (cell.getX() + 6.0f, cell.getCentreY() - std::max (0.0f, barH), cell.getWidth() - 12.0f, std::abs (barH));
        g.setColour (C::outline);
        g.drawHorizontalLine ((int) cell.getCentreY(), cell.getX() + 4.0f, cell.getRight() - 4.0f);

        g.setColour (C::text);
        g.setFont (cat::catFont (11.0f, true));
        g.drawText (cat::cubaseNoteName (note), cell.removeFromTop (18.0f), juce::Justification::centred);
        g.setColour (C::textDim);
        g.drawText ((cents >= 0 ? "+" : "") + juce::String (cents, 1) + " ct", cell.removeFromBottom (18.0f), juce::Justification::centred);
    }
}

void DetuneTab::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (140);
    detune.setBounds (top.removeFromLeft (110));
    drift.setBounds (top.removeFromLeft (110));
    driftRate.setBounds (top.removeFromLeft (110));
    top.removeFromLeft (20);
    auto col = top.removeFromLeft (240);
    auto row = [&col] { auto x = col.removeFromTop (28); col.removeFromTop (8); return x; };
    scalaEnabled.setBounds (row());
    {
        auto line = row();
        loadButton.setBounds (line.removeFromLeft (150));
        line.removeFromLeft (6);
        clearButton.setBounds (line);
    }
    reference->setBounds (row());
}

//==============================================================================
BendCatEditor::BendCatEditor (BendCatProcessor& p)
    : processor (p),
      mpe (p.apvts, "mpe", "MPE", &p.midiLearn),
      sendConfig (p.apvts, "sendConfig", "Send RPN", &p.midiLearn),
      resolution (p.apvts, "resolution", "Res", &p.midiLearn, 30),
      miniScope (p),
      keyboard (p.noteMonitor, 24, 108),
      catView (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, cat::CatStyle::Bend),
      surpriseControls (p, p.surpriseUndo, [&p] { p.surprise(); }),
      bootScreen (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, "BEND", "-cat", "MPE PITCH-BEND MODULATOR", cat::CatStyle::Bend)
{
    setLookAndFeel (&lnf);

    range = cat::Stepper::forParameter (p.apvts, "range", "Bend Range", [] (int v) { return juce::String (v) + " st"; });
    range->setCaptionWidth (74);
    range->setTooltip ("Pitch-bend range in semitones - must be the SAME value as in your instrument "
                       "(MPE instruments often default to 48). Bend-Cat also sends it as RPN 0.");
    amount = cat::Stepper::forParameter (p.apvts, "bendAmount", "Amount", [] (int v) { return juce::String (v) + " st"; });
    amount->setCaptionWidth (74);
    amount->setTooltip ("How far the envelope and LFO bend at 100 % depth, in semitones (independent of the bend range)");
    mpe.setTooltip ("MPE: each voice gets its own channel (2-16) for independent bends - the instrument must be in MPE mode; "
                    "off = one channel-wide bend for ordinary instruments");
    sendConfig.setTooltip ("Send the MPE Configuration Message and pitch-bend range (RPN 0) to the instrument");
    voices = cat::Stepper::forParameter (p.apvts, "mpeVoices", "Voices");
    presetStepper = std::make_unique<cat::PresetBar> (p.presetManager, juce::String());
    voices->setCaptionWidth (48);
    miniScope.setCompact (true);

    for (auto* c : std::initializer_list<juce::Component*> { range.get(), amount.get(), &mpe, &sendConfig, voices.get(), &resolution, &miniScope,
                                                             presetStepper.get() })
        addAndMakeVisible (c);

    tabs.addTab ("BEND", juce::Colours::transparentBlack, new BendTab (p), true);
    tabs.addTab ("DETUNE", juce::Colours::transparentBlack, new DetuneTab (p), true);
    tabs.addTab ("VISUALIZER", C::panel, new BendVisualizer (p), true);
    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    keyboard.setColourForChannel ([] (int ch) { return cat::Colours::palette (ch - 1).brighter (0.2f); });
    addAndMakeVisible (keyboard);

    addAndMakeVisible (catView);
    addAndMakeVisible (surpriseControls);
    addAndMakeVisible (bootScreen);   // last: drawn over everything until it fades out

    setSize (1000, 700);
}

BendCatEditor::~BendCatEditor()
{
    setLookAndFeel (nullptr);
}

void BendCatEditor::paint (juce::Graphics& g)
{
    cat::paintBackdrop (g, getLocalBounds());
    cat::CatLookAndFeel::drawLogo (g, juce::Rectangle<float> (22.0f, 10.0f, 260.0f, 52.0f), "BEND", "-cat");
}

void BendCatEditor::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    auto header = r.removeFromTop (56);
    header.removeFromLeft (290);
    catView.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (12);
    presetStepper->setBounds (header.removeFromRight (290).withSizeKeepingCentre (290, 32));
    header.removeFromRight (10);
    surpriseControls.setBounds (header.removeFromRight (122).withSizeKeepingCentre (122, 32));
    header.removeFromRight (10);
    miniScope.setBounds (header.reduced (0, 6));

    r.removeFromTop (8);
    auto bar = r.removeFromTop (30);
    range->setBounds (bar.removeFromLeft (150));     bar.removeFromLeft (8);
    amount->setBounds (bar.removeFromLeft (140));    bar.removeFromLeft (8);
    voices->setBounds (bar.removeFromLeft (120));    bar.removeFromLeft (8);
    resolution.setBounds (bar.removeFromLeft (170)); bar.removeFromLeft (14);
    mpe.setBounds (bar.removeFromLeft (80));         bar.removeFromLeft (8);
    sendConfig.setBounds (bar.removeFromLeft (100));

    r.removeFromTop (6);
    keyboard.setBounds (r.removeFromBottom (44));
    r.removeFromBottom (8);
    tabs.setBounds (r);

    bootScreen.setBounds (getLocalBounds());
}

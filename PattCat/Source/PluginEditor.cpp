#include "PluginEditor.h"
#include "MidiExport.h"

using namespace pattcat;
namespace C = cat::Colours;

namespace
{
    const char* layerSuffix[] = { "A", "B", "C", "D" };
    juce::Colour layerColour (int l) { return cat::Colours::palette (l == 0 ? 0 : l == 1 ? 3 : l == 2 ? 1 : 7); }
}

//==============================================================================
MixerStrip::MixerStrip (PattCatEditor& o, PattCatProcessor& p, GridContext& c, int l)
    : owner (o), ctx (c), layer (l),
      mute (p.apvts, juce::String ("mute") + layerSuffix[l], "M", &p.midiLearn),
      solo (p.apvts, juce::String ("solo") + layerSuffix[l], "S", &p.midiLearn),
      volume (p.apvts, juce::String ("vol") + layerSuffix[l], "Vol", &p.midiLearn, 34)
{
    name.setEditable (false, true);
    name.setFont (cat::catFont (13.0f, true));
    name.setColour (juce::Label::textColourId, C::text);
    name.onTextChange = [this] { const auto t = name.getText(); owner.editLayer ([t] (PatternLayer& pl) { pl.name = t; }); };
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);

    mute.setColour (juce::TextButton::buttonOnColourId, C::red);
    solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffc9bd84));
    addAndMakeVisible (mute);
    addAndMakeVisible (solo);
    addAndMakeVisible (volume);

    channel = std::make_unique<cat::Stepper> ("Ch", 1, 16,
        [this] { return ctx.engine.layers[(size_t) layer].midiChannel; },
        [this] (int v) { owner.editLayer ([v] (PatternLayer& pl) { pl.midiChannel = v; }); });
    channel->setCaptionWidth (22);
    addAndMakeVisible (*channel);

    mpeButton.setClickingTogglesState (true);
    mpeButton.setTooltip ("Spread this layer's notes over MPE member channels 2-16");
    mpeButton.onClick = [this] { const bool on = mpeButton.getToggleState(); owner.editLayer ([on] (PatternLayer& pl) { pl.mpe = on; }); };
    addAndMakeVisible (mpeButton);
}

void MixerStrip::refresh()
{
    const auto& l = ctx.engine.layers[(size_t) layer];
    name.setText (juce::String (layerSuffix[layer]) + "  " + l.name, juce::dontSendNotification);
    mpeButton.setToggleState (l.mpe, juce::dontSendNotification);
    channel->repaint();
    repaint();
}

void MixerStrip::paint (juce::Graphics& g)
{
    const bool selected = ctx.engine.currentLayer == layer;
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    cat::CatLookAndFeel::drawGlassPanel (g, r, 6.0f, selected ? C::raised : C::panel);
    g.setColour (layerColour (layer));
    g.fillRoundedRectangle (r.removeFromLeft (4.0f).reduced (0.0f, 6.0f), 2.0f);
    if (selected)
    {
        g.setColour (C::accent);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f, 1.5f);
    }
}

void MixerStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    name.setBounds (r.removeFromTop (20));
    r.removeFromTop (4);
    auto row = r.removeFromTop (24);
    mute.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (4);
    solo.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (4);
    mpeButton.setBounds (row.removeFromRight (44));
    row.removeFromRight (4);
    channel->setBounds (row);
    r.removeFromTop (4);
    volume.setBounds (r.removeFromTop (22));
}

void MixerStrip::mouseDown (const juce::MouseEvent& e)
{
    owner.selectLayer (layer);
    if (e.getNumberOfClicks() >= 2)
        name.showEditor();
}

//==============================================================================
StepInspector::StepInspector (GridContext& c) : ctx (c)
{
    for (auto* b : { &activeButton, &tieButton, &restButton })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    activeButton.onClick = [this] { if (! updating) { const bool v = activeButton.getToggleState(); edit ([v] (Step& s) { s.active = v; }); } };
    tieButton.onClick = [this] { if (! updating) { const bool v = tieButton.getToggleState(); edit ([v] (Step& s) { s.isTie = v; if (v) s.isRest = false; }); } };
    restButton.onClick = [this] { if (! updating) { const bool v = restButton.getToggleState(); edit ([v] (Step& s) { s.isRest = v; if (v) s.isTie = false; }); } };

    auto setupSlider = [this] (juce::Slider& s, double lo, double hi, double step, std::function<juce::String (double)> fmt)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (lo, hi, step);
        s.textFromValueFunction = std::move (fmt);
        s.setColour (juce::Slider::trackColourId, C::blue);
        addAndMakeVisible (s);
    };
    setupSlider (velocity, 0.0, 1.5, 0.01, [] (double v) { return "Velocity " + juce::String (juce::roundToInt (v * 100.0)) + "%"; });
    setupSlider (gate, 0.1, 1.5, 0.01, [] (double v) { return "Gate " + juce::String (juce::roundToInt (v * 100.0)) + "%"; });
    setupSlider (probability, 0.0, 1.0, 0.01, [] (double v) { return "Probability " + juce::String (juce::roundToInt (v * 100.0)) + "%"; });
    velocity.onValueChange = [this] { if (! updating) { const float v = (float) velocity.getValue(); edit ([v] (Step& s) { s.velocity = v; }); } };
    gate.onValueChange = [this] { if (! updating) { const float v = (float) gate.getValue(); edit ([v] (Step& s) { s.gate = v; }); } };
    probability.onValueChange = [this] { if (! updating) { const float v = (float) probability.getValue(); edit ([v] (Step& s) { s.probability = v; }); } };

    octave = std::make_unique<cat::Stepper> ("Octave", -2, 2, [this] { return step().octaveOffset; },
                                             [this] (int v) { edit ([v] (Step& s) { s.octaveOffset = v; }); },
                                             [] (int v) { return (v > 0 ? "+" : "") + juce::String (v); });
    ratchet = std::make_unique<cat::Stepper> ("Ratchet", 1, 4, [this] { return step().ratchet; },
                                              [this] (int v) { edit ([v] (Step& s) { s.ratchet = v; }); },
                                              [] (int v) { return v == 1 ? juce::String ("off") : juce::String (v) + "x"; });
    addAndMakeVisible (*octave);
    addAndMakeVisible (*ratchet);

    voice.addItem ("All voices", 1);
    voice.addItem ("Top voice", 2);
    voice.addItem ("Bottom voice", 3);
    for (int v = 0; v < 6; ++v) voice.addItem ("Voice " + juce::String (v + 1), 4 + v);
    voice.onChange = [this]
    {
        if (updating) return;
        const int id = voice.getSelectedId();
        const int vi = id == 1 ? VoiceAll : id == 2 ? VoiceTop : id == 3 ? VoiceBottom : id - 4;
        edit ([vi] (Step& s) { s.voiceIndex = vi; });
    };
    addAndMakeVisible (voice);
}

Step& StepInspector::step()
{
    return ctx.engine.layers[(size_t) ctx.engine.currentLayer].steps[(size_t) juce::jlimit (0, 31, ctx.selectedStep)];
}

void StepInspector::edit (const std::function<void (Step&)>& fn)
{
    const int l = ctx.engine.currentLayer, s = juce::jlimit (0, 31, ctx.selectedStep);
    fn (step());
    ctx.processor.editEngine ([fn, l, s] (PattCatEngine& e)
    {
        auto& layer = e.layers[(size_t) l];
        if (s >= layer.length) layer.length = s + 1;
        fn (layer.steps[(size_t) s]);
    });
    if (ctx.onSelectionChanged) ctx.onSelectionChanged();
}

void StepInspector::refresh()
{
    const auto& s = step();
    updating = true;
    activeButton.setToggleState (s.active, juce::dontSendNotification);
    tieButton.setToggleState (s.isTie, juce::dontSendNotification);
    restButton.setToggleState (s.isRest, juce::dontSendNotification);
    velocity.setValue (s.velocity, juce::dontSendNotification);
    gate.setValue (s.gate, juce::dontSendNotification);
    probability.setValue (s.probability, juce::dontSendNotification);
    voice.setSelectedId (s.voiceIndex == VoiceAll ? 1 : s.voiceIndex == VoiceTop ? 2 : s.voiceIndex == VoiceBottom ? 3 : 4 + s.voiceIndex,
                         juce::dontSendNotification);
    octave->repaint();
    ratchet->repaint();
    updating = false;
    repaint();
}

void StepInspector::paint (juce::Graphics& g)
{
    cat::CatLookAndFeel::drawGlassPanel (g, getLocalBounds().toFloat().reduced (2.0f), 8.0f, C::panel);
    g.setColour (C::textDim);
    g.setFont (cat::catFont (11.0f, true));
    g.drawText ("STEP " + juce::String (ctx.selectedStep + 1) + "  /  LAYER " + layerSuffix[ctx.engine.currentLayer],
                getLocalBounds().reduced (12, 8).removeFromTop (16), juce::Justification::centredLeft);
}

void StepInspector::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    r.removeFromTop (22);
    auto row = [&r] (int h) { auto x = r.removeFromTop (h); r.removeFromTop (7); return x; };
    {
        auto line = row (26);
        const int w = line.getWidth() / 3;
        activeButton.setBounds (line.removeFromLeft (w).reduced (2, 0));
        tieButton.setBounds (line.removeFromLeft (w).reduced (2, 0));
        restButton.setBounds (line.reduced (2, 0));
    }
    velocity.setBounds (row (24));
    gate.setBounds (row (24));
    probability.setBounds (row (24));
    voice.setBounds (row (26));
    octave->setBounds (row (26));
    ratchet->setBounds (row (26));
}

//==============================================================================
PattCatEditor::PattCatEditor (PattCatProcessor& p)
    : processor (p), ctx (p),
      inputMode (p.apvts, "inputMode", {}, &p.midiLearn),
      latch (p.apvts, "latch", "Latch", &p.midiLearn),
      runStopped (p.apvts, "runStopped", "Free Run", &p.midiLearn),
      thru (p.apvts, "thru", "Thru", &p.midiLearn),
      useInputVel (p.apvts, "useInputVel", "In Vel", &p.midiLearn),
      followGroove (p.apvts, "followGroove", "Follow Drum", &p.midiLearn),
      globalSwing (p.apvts, "globalSwing", "Swing", &p.midiLearn, 48),
      gateRandom (p.apvts, "gateRandom", "Gate Rnd", &p.midiLearn, 64),
      keyboard (p.noteMonitor, 24, 108),
      catView (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, cat::CatStyle::Patt),
      surpriseControls (p, p.surpriseUndo, [&p] { p.surprise(); }),
      bootScreen (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, "PATT", "-cat", "PATTERN SEQUENCER", cat::CatStyle::Patt),
      bounceView (p),
      bounceMode (p.apvts, "bounceMode", "Bounce", &p.midiLearn, 52),
      bounceSync (p.apvts, "bounceSync", "R.Mode", &p.midiLearn, 52),
      bounceRate (p.apvts, "bounceRate", "Rate", &p.midiLearn, 38),
      bounceOctRnd (p.apvts, "bounceOctRnd", "Oct Rnd", &p.midiLearn, 54),
      bounceHold (p.apvts, "bounceHold", "Play", &p.midiLearn, 36),
      bounceMs (p.apvts, "bounceMs", "Time", &p.midiLearn, 40),
      bounceNotes (p.apvts, "bounceNotes", "Notes", &p.midiLearn, 46),
      bounceCurve (p.apvts, "bounceCurve", "Curve", &p.midiLearn, 46),
      bounceFade (p.apvts, "bounceFade", "Fade Out", &p.midiLearn, 62),
      bounceOctave (p.apvts, "bounceOctave", "Octave", &p.midiLearn, 52),
      bounceDur (p.apvts, "bounceDur", "Dur", &p.midiLearn, 34),
      bounceHuman (p.apvts, "bounceHuman", "Human", &p.midiLearn, 50)
{
    setLookAndFeel (&lnf);
    ctx.engine = processor.getEngineCopy();
    ctx.onSelectionChanged = [this] { inspector.refresh(); grid.repaint(); overview.repaint(); };

    presetStepper = std::make_unique<cat::PresetBar> (processor.presetManager, "Pattern");
    addAndMakeVisible (*presetStepper);

    generateButton.setColour (juce::TextButton::buttonColourId, C::accent.withAlpha (0.16f));
    generateButton.setColour (juce::TextButton::textColourOffId, C::text);
    generateButton.setTooltip ("Generate a new rhythm for the selected layer");
    generateButton.onClick = [this]
    {
        auto& rng = juce::Random::getSystemRandom();
        const int l = ctx.engine.currentLayer;
        juce::Random local (rng.nextInt64());
        processor.editEngine ([l, local] (PattCatEngine& e) mutable { e.randomizeLayer (l, local); });
    };

    clearChordsButton.setTooltip ("Advance mode: forget the memorised chord sequence");
    clearChordsButton.onClick = [this] { processor.clearChordMemory(); };

    dragButton.setTooltip ("Drag one loop of the pattern (played on the last chord) onto a Cubase track");
    dragButton.onDragOut = [this]
    {
        const auto seq = processor.buildPatternSequence();
        if (seq.getNumEvents() > 0)
            cat::midiexport::dragFileOut (cat::midiexport::writeMidiFile (seq, processor.getHostBpm(), "Patt-Cat Pattern"), this);
    };

    for (auto* c : std::initializer_list<juce::Component*> { &generateButton, &clearChordsButton, &inputMode, &latch, &runStopped, &thru,
                                                             &useInputVel, &followGroove, &globalSwing, &gateRandom, &dragButton })
        addAndMakeVisible (c);

    // Layer / euclid controls
    auto layerRef = [this] () -> const PatternLayer& { return ctx.engine.layers[(size_t) ctx.engine.currentLayer]; };
    fillStepper = std::make_unique<cat::Stepper> ("Fill", 0, 32, [layerRef] { return layerRef().euclidFill; },
                                                  [this] (int v) { editLayer ([v] (PatternLayer& l) { l.euclidFill = juce::jmin (v, l.length); }); });
    lengthStepper = std::make_unique<cat::Stepper> ("Steps", 1, 32, [layerRef] { return layerRef().length; },
                                                    [this] (int v) { editLayer ([v] (PatternLayer& l) { l.length = v; }); });
    rotationStepper = std::make_unique<cat::Stepper> ("Rotate", 0, 31, [layerRef] { return layerRef().euclidRotation; },
                                                      [this] (int v) { editLayer ([v] (PatternLayer& l) { l.euclidRotation = v; }); });
    swingStepper = std::make_unique<cat::Stepper> ("Swing", 0, 100, [layerRef] { return layerRef().swing; },
                                                   [this] (int v) { editLayer ([v] (PatternLayer& l) { l.swing = v; }); });
    velScaleStepper = std::make_unique<cat::Stepper> ("Vel", 0, 200, [layerRef] { return juce::roundToInt (layerRef().velocityScale * 100.0f); },
                                                      [this] (int v) { editLayer ([v] (PatternLayer& l) { l.velocityScale = (float) v / 100.0f; }); },
                                                      [] (int v) { return juce::String (v) + "%"; });
    fillStepper->setCaptionWidth (26);
    lengthStepper->setCaptionWidth (36);
    rotationStepper->setCaptionWidth (44);
    swingStepper->setCaptionWidth (40);
    velScaleStepper->setCaptionWidth (26);
    for (auto* s : { fillStepper.get(), lengthStepper.get(), rotationStepper.get(), swingStepper.get(), velScaleStepper.get() })
        addAndMakeVisible (s);

    euclidButton.setTooltip ("Distribute 'Fill' pulses evenly over 'Steps' (Bjorklund) and rotate");
    euclidButton.onClick = [this]
    {
        const int l = ctx.engine.currentLayer;
        const auto& layer = ctx.engine.layers[(size_t) l];
        const int fill = layer.euclidFill, steps = layer.length, rot = layer.euclidRotation;
        processor.editEngine ([l, fill, steps, rot] (PattCatEngine& e) { e.applyEuclidean (l, fill, steps, rot); });
    };
    polyButton.setClickingTogglesState (true);
    polyButton.setTooltip ("Polyrhythmic: the layer loops freely instead of restarting on bar lines");
    polyButton.onClick = [this] { const bool on = polyButton.getToggleState(); editLayer ([on] (PatternLayer& l) { l.isPolyrhythmic = on; }); };
    clearLayerButton.onClick = [this] { const int l = ctx.engine.currentLayer; processor.editEngine ([l] (PattCatEngine& e) { e.clearLayer (l); }); };

    divisionBox.addItemList (getDivisionNames(), 1);
    divisionBox.onChange = [this] { const int d = divisionBox.getSelectedId() - 1; if (d >= 0) editLayer ([d] (PatternLayer& l) { l.division = d; }); };

    for (auto* c : std::initializer_list<juce::Component*> { &euclidButton, &polyButton, &clearLayerButton, &divisionBox })
        addAndMakeVisible (c);

    for (int l = 0; l < 4; ++l)
        addAndMakeVisible (strips.add (new MixerStrip (*this, p, ctx, l)));

    addAndMakeVisible (overview);
    addAndMakeVisible (grid);
    addAndMakeVisible (inspector);
    addAndMakeVisible (presets);
    keyboard.setColourForChannel ([] (int) { return C::slate; });
    addAndMakeVisible (keyboard);

    // Bounce page
    stepsViewButton.setTooltip ("Step sequencer");
    bounceViewButton.setTooltip ("Bounce: every note bounces like a ball and re-plays on each bounce");
    stepsViewButton.onClick = [this] { setBounceView (false); };
    bounceViewButton.onClick = [this] { setBounceView (true); };
    addAndMakeVisible (stepsViewButton);
    addAndMakeVisible (bounceViewButton);

    bounceMode.getCombo().setTooltip ("Pattern Notes: every note Patt-Cat plays bounces.  Keys: every key you press bounces on its own");
    bounceSync.getCombo().setTooltip ("Sync: first gap is a note value at the song tempo.  Free: first gap in milliseconds (Time)");
    bounceCurve.getSlider().setTooltip ("+ the gaps get shorter like a real ball,  - the bounces slow down");
    bounceFade.getSlider().setTooltip ("How much quieter the last bounce is than the first note");
    bounceOctave.getSlider().setTooltip ("Octave of the bounces relative to the note");
    bounceHold.getCombo().setTooltip ("Keys mode: One Shot plays every bounce, While Held stops them when you let go");
    for (auto* c : std::initializer_list<juce::Component*> { &bounceView, &bounceMode, &bounceSync, &bounceRate, &bounceOctRnd, &bounceHold,
                                                             &bounceMs, &bounceNotes, &bounceCurve, &bounceFade, &bounceOctave, &bounceDur, &bounceHuman })
        addChildComponent (c);
    setBounceView (false);

    addAndMakeVisible (catView);
    addAndMakeVisible (surpriseControls);
    addAndMakeVisible (bootScreen);   // last: drawn over everything until it fades out

    setSize (1280, 820);
    refreshAll();
    startTimerHz (30);
}

PattCatEditor::~PattCatEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PattCatEditor::selectLayer (int layer)
{
    ctx.engine.currentLayer = layer;
    processor.editEngine ([layer] (PattCatEngine& e) { e.currentLayer = layer; });
    refreshAll();
}

void PattCatEditor::editLayer (const std::function<void (PatternLayer&)>& fn)
{
    const int l = ctx.engine.currentLayer;
    fn (ctx.engine.layers[(size_t) l]);
    processor.editEngine ([fn, l] (PattCatEngine& e) { fn (e.layers[(size_t) l]); });
    refreshAll();
}

void PattCatEditor::refreshAll()
{
    const int keepLayer = ctx.engine.currentLayer;
    ctx.engine = processor.getEngineCopy();
    ctx.engine.currentLayer = keepLayer;
    shownVersion = processor.engineVersion.load();

    const auto& layer = ctx.engine.layers[(size_t) keepLayer];
    divisionBox.setSelectedId (layer.division + 1, juce::dontSendNotification);
    polyButton.setToggleState (layer.isPolyrhythmic, juce::dontSendNotification);

    for (auto* s : strips) s->refresh();
    inspector.refresh();
    overview.setChordSize ((int) processor.getLastChord().size());
    overview.repaint();
    grid.repaint();
    presets.repaint();
    for (auto* s : { fillStepper.get(), lengthStepper.get(), rotationStepper.get(), swingStepper.get(), velScaleStepper.get() })
        s->repaint();
    clearChordsButton.setButtonText ("Clear Chords (" + juce::String (processor.getChordMemorySize()) + ")");
}

void PattCatEditor::timerCallback()
{
    if (processor.engineVersion.load() != shownVersion)
        refreshAll();

    const bool running = processor.uiRunning.load();
    ctx.barLength = processor.uiBarLength.load();
    ctx.playheadPpq = running ? processor.uiPpq.load() : -1.0;
    for (int l = 0; l < 4; ++l)
        ctx.playheadSteps[(size_t) l] = running ? ctx.engine.getStepAt (l, ctx.playheadPpq, ctx.barLength) : -1;

    static int lastMemory = -1;
    if (processor.getChordMemorySize() != lastMemory)
    {
        lastMemory = processor.getChordMemorySize();
        clearChordsButton.setButtonText ("Clear Chords (" + juce::String (lastMemory) + ")");
        overview.setChordSize ((int) processor.getLastChord().size());
    }

    overview.repaint();
    grid.repaint();
}

void PattCatEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::background);
    cat::CatLookAndFeel::drawLogo (g, juce::Rectangle<float> (12.0f, 8.0f, 160.0f, 32.0f), "PATT", "-cat");
}

void PattCatEditor::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto item = [] (juce::Component& c, float w, float h = 30.0f) { return juce::FlexItem (c).withWidth (w).withHeight (h).withMargin ({ 0, 3, 0, 3 }); };

    {
        auto top = r.removeFromTop (44);
        top.removeFromLeft (130);
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        fb.items.add (item (*presetStepper, 320));
        fb.items.add (item (generateButton, 90));
        fb.items.add (item (inputMode, 140));
        fb.items.add (item (latch, 58));
        fb.items.add (item (runStopped, 74));
        fb.items.add (item (thru, 50));
        fb.items.add (item (useInputVel, 62));
        fb.items.add (item (clearChordsButton, 124));
        fb.items.add (juce::FlexItem().withFlex (1.0f));
        fb.items.add (item (globalSwing, 130));
        fb.items.add (item (followGroove, 92));
        fb.items.add (item (gateRandom, 140));
        fb.items.add (item (dragButton, 92));
        fb.performLayout (top);
    }
    r.removeFromTop (6);

    keyboard.setBounds (r.removeFromBottom (44));
    r.removeFromBottom (6);
    presets.setBounds (r.removeFromBottom (juce::jlimit (130, 220, r.getHeight() / 4)));
    r.removeFromBottom (6);

    auto left = r.removeFromLeft (230);
    const int stripH = juce::jmin (100, left.getHeight() / 4);
    for (auto* s : strips) s->setBounds (left.removeFromTop (stripH).reduced (0, 2));
    left.removeFromTop (8);
    surpriseControls.setBounds (left.removeFromTop (30));
    left.removeFromTop (4);
    catView.setBounds (left.reduced (10, 0));
    r.removeFromLeft (8);

    const auto mainArea = r;
    inspector.setBounds (r.removeFromRight (230));
    r.removeFromRight (8);

    {
        auto bar = r.removeFromTop (30);
        stepsViewButton.setBounds (bar.removeFromLeft (58).reduced (0, 1));
        bar.removeFromLeft (4);
        bounceViewButton.setBounds (bar.removeFromLeft (64).reduced (0, 1));
        bar.removeFromLeft (10);
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        fb.items.add (item (*fillStepper, 86, 28));
        fb.items.add (item (*lengthStepper, 96, 28));
        fb.items.add (item (*rotationStepper, 104, 28));
        fb.items.add (item (euclidButton, 64, 28));
        fb.items.add (juce::FlexItem().withFlex (1.0f));
        fb.items.add (item (divisionBox, 74, 28));
        fb.items.add (item (*swingStepper, 104, 28));
        fb.items.add (item (*velScaleStepper, 104, 28));
        fb.items.add (item (polyButton, 50, 28));
        fb.items.add (item (clearLayerButton, 54, 28));
        fb.performLayout (bar);
    }
    r.removeFromTop (6);
    overview.setBounds (r.removeFromTop (juce::jlimit (110, 200, r.getHeight() * 2 / 5)));
    r.removeFromTop (6);
    grid.setBounds (r);

    // Bounce page: view on top, two rows of controls below (uses the whole centre + right area)
    {
        auto b = mainArea;
        b.removeFromTop (36);
        auto row2 = b.removeFromBottom (30);
        b.removeFromBottom (6);
        auto row1 = b.removeFromBottom (30);
        b.removeFromBottom (8);
        bounceView.setBounds (b);

        juce::FlexBox f1, f2;
        f1.alignItems = f2.alignItems = juce::FlexBox::AlignItems::center;
        f1.items.add (item (bounceMode, 200, 28));
        f1.items.add (item (bounceRate, 120, 28));
        f1.items.add (item (bounceSync, 140, 28));
        f1.items.add (item (bounceNotes, 150, 28).withFlex (1.0f));
        f1.items.add (item (bounceFade, 170, 28).withFlex (1.0f));
        f1.items.add (item (bounceHold, 170, 28));
        f1.performLayout (row1);
        f2.items.add (item (bounceCurve, 150, 28).withFlex (1.0f));
        f2.items.add (item (bounceOctave, 140, 28).withFlex (1.0f));
        f2.items.add (item (bounceOctRnd, 150, 28));
        f2.items.add (item (bounceDur, 130, 28).withFlex (1.0f));
        f2.items.add (item (bounceHuman, 140, 28).withFlex (1.0f));
        f2.items.add (item (bounceMs, 130, 28).withFlex (1.0f));
        f2.performLayout (row2);
    }

    bootScreen.setBounds (getLocalBounds());
}

void PattCatEditor::setBounceView (bool shouldShow)
{
    bounceVisible = shouldShow;
    stepsViewButton.setToggleState (! shouldShow, juce::dontSendNotification);
    bounceViewButton.setToggleState (shouldShow, juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { fillStepper.get(), lengthStepper.get(), rotationStepper.get(), &euclidButton,
                                                             &divisionBox, swingStepper.get(), velScaleStepper.get(), &polyButton,
                                                             &clearLayerButton, &overview, &grid, &inspector })
        c->setVisible (! shouldShow);
    for (auto* c : std::initializer_list<juce::Component*> { &bounceView, &bounceMode, &bounceSync, &bounceRate, &bounceOctRnd, &bounceHold,
                                                             &bounceMs, &bounceNotes, &bounceCurve, &bounceFade, &bounceOctave, &bounceDur, &bounceHuman })
        c->setVisible (shouldShow);
    bootScreen.toFront (false);
}

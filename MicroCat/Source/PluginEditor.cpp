#include "PluginEditor.h"

using namespace micro;

namespace
{
    constexpr int designW = 1120, designH = 720;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    juce::String centsText (double c)
    {
        const bool small = std::abs (c) < 10.0;
        return (c >= 0 ? "+" : "") + (small ? juce::String (std::round (c * 10.0) / 10.0, 1) : juce::String ((int) std::lround (c)));
    }

}

//==============================================================================
TuningCircle::TuningCircle()
{
    setTooltip ("Drag a note round the circle to retune it. Double-click an empty spot to add a note, double-click a note to remove it.");
}

juce::Rectangle<float> TuningCircle::ring() const
{
    const float s = (float) juce::jmin (getWidth(), getHeight()) - 64.0f;
    return getLocalBounds().toFloat().withSizeKeepingCentre (s, s);
}

juce::Point<float> TuningCircle::pointFor (double cents, float radius) const
{
    const float a = (float) (cents / tuning.period()) * twoPi - juce::MathConstants<float>::halfPi;
    const auto c = ring().getCentre();
    return { c.x + std::cos (a) * radius, c.y + std::sin (a) * radius };
}

double TuningCircle::centsAt (juce::Point<float> p) const
{
    const auto c = ring().getCentre();
    float a = std::atan2 (p.y - c.y, p.x - c.x) + juce::MathConstants<float>::halfPi;
    while (a < 0.0f) a += twoPi;
    while (a >= twoPi) a -= twoPi;
    return a / twoPi * tuning.period();
}

int TuningCircle::degreeAt (juce::Point<float> p) const
{
    const float r = ring().getWidth() * 0.5f;
    int best = -1;
    float bestD = 12.0f;
    for (int d = 0; d < tuning.notesPerPeriod(); ++d)
    {
        const float dist = p.getDistanceFrom (pointFor (tuning.degreeCents (d), r));
        if (dist < bestD) { bestD = dist; best = d; }
    }
    return best;
}

void TuningCircle::paint (juce::Graphics& g)
{
    cat::CatLookAndFeel::drawGlassPanel (g, getLocalBounds().toFloat(), 10.0f);
    if (tuning.cents.empty()) return;
    const auto rr = ring();
    const auto c = rr.getCentre();
    const float r = rr.getWidth() * 0.5f;
    g.setColour (cat::Colours::outline);
    g.drawEllipse (rr, 1.5f);
    // 12-EDO's notes for comparison (when the period is an octave)
    const double period = tuning.period();
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int k = 0; k * 100.0 < period - 1e-6; ++k)
    {
        const auto inner = pointFor (k * 100.0, r - 6.0f), outer = pointFor (k * 100.0, r + 6.0f);
        g.setColour (cat::Colours::textDim.withAlpha (0.5f));
        g.drawLine ({ inner, outer }, 1.2f);
        if (std::abs (period - 1200.0) < 1e-3)
        {
            const auto lp = pointFor (k * 100.0, r + 20.0f);
            g.setFont (cat::catFont (11.0f));
            g.drawText (names[k % 12], juce::Rectangle<float> (30.0f, 14.0f).withCentre (lp), juce::Justification::centred);
        }
    }
    // the tuning: a star through all its notes, then the notes
    juce::Path star;
    for (int d = 0; d < tuning.notesPerPeriod(); ++d)
    {
        const auto pt = pointFor (tuning.degreeCents (d), r);
        if (d == 0) star.startNewSubPath (pt); else star.lineTo (pt);
    }
    star.closeSubPath();
    g.setColour (cat::Colours::accent.withAlpha (0.10f));
    g.fillPath (star);
    g.setColour (cat::Colours::accent.withAlpha (0.35f));
    g.strokePath (star, juce::PathStrokeType (1.2f));
    const bool many = tuning.notesPerPeriod() > 36;
    for (int d = 0; d < tuning.notesPerPeriod(); ++d)
    {
        const double cents = tuning.degreeCents (d);
        const auto pt = pointFor (cents, r);
        const double off = cents - 100.0 * std::round (cents / 100.0);       // how far from a normal note
        const auto col = d == 0 ? cat::Colours::text
                                : cat::Colours::green.interpolatedWith (cat::Colours::red, (float) juce::jlimit (0.0, 1.0, std::abs (off) / 50.0));
        const float size = d == 0 ? 13.0f : (many ? 6.0f : 10.0f);
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (pt));
        if (d == selected)
        {
            g.setColour (cat::Colours::text);
            g.drawEllipse (juce::Rectangle<float> (size + 8.0f, size + 8.0f).withCentre (pt), 2.0f);
        }
        if (! many || d == selected)
        {
            const auto lp = pointFor (cents, r - 24.0f);
            g.setColour (d == selected ? cat::Colours::text : cat::Colours::textDim);
            g.setFont (cat::catFont (d == selected ? 12.5f : 10.5f, d == selected));
            g.drawText (juce::String ((int) std::lround (cents)), juce::Rectangle<float> (44.0f, 14.0f).withCentre (lp), juce::Justification::centred);
        }
    }
    // in the middle: the name and the size
    g.setColour (cat::Colours::text);
    g.setFont (cat::catFont (20.0f, true));
    g.drawFittedText (tuning.name, juce::Rectangle<int> ((int) c.x - 130, (int) c.y - 34, 260, 28), juce::Justification::centred, 1);
    g.setColour (cat::Colours::textDim);
    g.setFont (cat::catFont (13.0f));
    juce::String sub;
    sub << tuning.notesPerPeriod() << " notes  -  repeats every " << juce::String (period, std::abs (period - std::round (period)) < 1e-3 ? 0 : 2) << " cents";
    g.drawFittedText (sub, juce::Rectangle<int> ((int) c.x - 130, (int) c.y - 4, 260, 18), juce::Justification::centred, 1);
    g.drawFittedText (tuning.description, juce::Rectangle<int> ((int) c.x - 120, (int) c.y + 16, 240, 48), juce::Justification::centredTop, 3);
}

void TuningCircle::mouseDown (const juce::MouseEvent& e)
{
    const int d = degreeAt (e.position);
    dragging = -1;
    moved = false;
    if (d > 0)
    {
        selected = d;
        if (onSelect) onSelect();
        if (e.mods.isPopupMenu())
        {
            if (tuning.notesPerPeriod() > 1)
            {
                tuning.cents.erase (tuning.cents.begin() + (d - 1));
                selected = juce::jlimit (1, juce::jmax (1, tuning.notesPerPeriod() - 1), selected);
                if (onChange) onChange (tuning);
            }
            return;
        }
        dragging = d;
        repaint();
    }
}

void TuningCircle::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging <= 0 || dragging >= tuning.notesPerPeriod()) return;
    const double lo = tuning.degreeCents (dragging - 1) + 0.5, hi = tuning.degreeCents (dragging + 1) - 0.5;
    double c = centsAt (e.position);
    if (e.mods.isShiftDown()) c = std::round (c);                     // shift: whole cents
    tuning.cents[(size_t) dragging - 1] = juce::jlimit (lo, hi, c);
    moved = true;
    repaint();
    if (onChange) onChange (tuning);
}

void TuningCircle::mouseUp (const juce::MouseEvent&) { dragging = -1; }

void TuningCircle::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int d = degreeAt (e.position);
    if (d > 0)
    {
        if (tuning.notesPerPeriod() > 1)
        {
            tuning.cents.erase (tuning.cents.begin() + (d - 1));
            selected = juce::jlimit (1, juce::jmax (1, tuning.notesPerPeriod() - 1), selected);
            if (onChange) onChange (tuning);
        }
        return;
    }
    if (d == 0 || (int) tuning.cents.size() >= TuningData::maxNotes) return;
    const double c = centsAt (e.position);
    if (c < 1.0 || c > tuning.period() - 1.0) return;
    auto it = std::lower_bound (tuning.cents.begin(), tuning.cents.end(), c);
    if ((it != tuning.cents.end() && std::abs (*it - c) < 1.0) || (it != tuning.cents.begin() && std::abs (*(it - 1) - c) < 1.0)) return;
    selected = (int) (it - tuning.cents.begin()) + 1;
    tuning.cents.insert (it, c);
    if (onChange) onChange (tuning);
}

//==============================================================================
TuningKeyboard::TuningKeyboard (MicroCatProcessor& p) : processor (p) { startTimerHz (20); }

int TuningKeyboard::firstKey() const
{
    return juce::jlimit (0, 127 - numKeys, processor.currentSettings().rootKey - 6);
}

int TuningKeyboard::keyAt (juce::Point<float> pt) const
{
    const float w = (float) getWidth() / numKeys;
    const int i = (int) (pt.x / w);
    return juce::isPositiveAndBelow (i, numKeys) ? firstKey() + i : -1;
}

void TuningKeyboard::timerCallback()
{
    const auto st = processor.currentSettings();
    const int mapKey = processor.effectiveMapping() * 100000 + st.just * 10000 + juce::roundToInt (st.morph * 1000.0f)
                     + (st.just != 0 ? (processor.engine.justRoot.load() + 1) * 1000000 : 0);
    bool changed = processor.tuningVersion.load() != shownVersion || st.rootKey != shownRoot || mapKey != shownMap;
    for (int k = 0; k < 128; ++k)
    {
        const bool h = processor.engine.held[(size_t) k].load();
        if (h != shownHeld[(size_t) k]) { shownHeld[(size_t) k] = h; changed = true; }
    }
    if (changed)
    {
        shownVersion = processor.tuningVersion.load();
        shownRoot = processor.currentSettings().rootKey;
        shownMap = mapKey;
        repaint();
    }
}

void TuningKeyboard::paint (juce::Graphics& g)
{
    cat::CatLookAndFeel::drawGlassPanel (g, getLocalBounds().toFloat(), 8.0f);
    const auto pm = processor.currentMap();      // morph and live just intonation included
    const float w = (float) getWidth() / numKeys;
    const int first = firstKey();
    for (int i = 0; i < numKeys; ++i)
    {
        const int key = first + i;
        const int pc = key % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        auto r = juce::Rectangle<float> (i * w, 6.0f, w, (float) getHeight() - 12.0f).reduced (1.5f, 0.0f);
        const bool held = shownHeld[(size_t) key];
        g.setColour (held ? cat::Colours::accent : (black ? juce::Colour (0xff2a2d36) : juce::Colour (0xffe9ecf2)));
        g.fillRoundedRectangle (r, 4.0f);
        const auto kp = keyPitch (pm, key);
        const auto ink = held ? juce::Colours::white : (black ? cat::Colours::text : juce::Colour (0xff23262e));
        g.setColour (ink);
        g.setFont (cat::catFont (11.0f, true));
        g.drawText (juce::MidiMessage::getMidiNoteName (key, true, true, 4), r.removeFromTop (18.0f), juce::Justification::centred);
        g.setFont (cat::catFont (10.5f));
        if (! kp.valid) { g.drawText ("-", r, juce::Justification::centred); continue; }
        // what it really plays: the nearest normal note and the cents off it, then the Hz
        auto lower = r.removeFromBottom (r.getHeight() * 0.55f);
        const int cents = (int) std::lround (kp.bend * 100.0);
        g.setColour (cents == 0 ? ink.withAlpha (0.6f) : (held ? juce::Colours::white : cat::Colours::accent));
        g.drawFittedText (juce::MidiMessage::getMidiNoteName (kp.note, true, false, 4), r.toNearestInt(), juce::Justification::centredBottom, 1);
        g.drawFittedText (cents == 0 ? juce::String ("0") : centsText (cents), lower.removeFromTop (14.0f).toNearestInt(), juce::Justification::centred, 1);
        g.setColour (ink.withAlpha (0.55f));
        g.setFont (cat::catFont (9.0f));
        g.drawFittedText (kp.hz < 100.0 ? juce::String (std::round (kp.hz * 10.0) / 10.0, 1) : juce::String ((int) std::lround (kp.hz)), lower.toNearestInt(), juce::Justification::centredTop, 1);
        if (key == pm.rootKey)
        {
            g.setColour (cat::Colours::green);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ i * w + w * 0.5f, (float) getHeight() - 10.0f }));
        }
    }
}

void TuningKeyboard::mouseDown (const juce::MouseEvent& e)
{
    playing = keyAt (e.position);
    if (playing >= 0) processor.previewNote (playing, true);
}

void TuningKeyboard::mouseDrag (const juce::MouseEvent& e)
{
    const int k = keyAt (e.position);
    if (k == playing || k < 0) return;
    if (playing >= 0) processor.previewNote (playing, false);
    playing = k;
    processor.previewNote (playing, true);
}

void TuningKeyboard::mouseUp (const juce::MouseEvent&)
{
    if (playing >= 0) processor.previewNote (playing, false);
    playing = -1;
}

//==============================================================================
MicroCatEditor::MicroCatEditor (MicroCatProcessor& p) : processor (p)
{
    setSize (designW, designH);

    search.setTextToShowWhenEmpty ("Search tunings...", cat::Colours::textDim);
    search.onTextChange = [this] { refreshList(); };
    addAndMakeVisible (search);
    const juce::StringArray cats { "All", "Equal", "Just", "World", "Mine" };
    for (int i = 0; i < cats.size(); ++i)
    {
        auto* b = categoryButtons.add (new juce::TextButton (cats[i]));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (707);
        b->onClick = [this, i] { category = i - 1; refreshList(); };
        addAndMakeVisible (b);
    }
    categoryButtons[0]->setToggleState (true, juce::dontSendNotification);
    list.setRowHeight (38);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    circle.onChange = [this] (const Tuning& t) { edited (t); };
    circle.onSelect = [this] { showTuning(); };
    addAndMakeVisible (circle);

    nameBox.setTooltip ("The tuning's name (for saving)");
    nameBox.onReturnKey = [this] { auto t = processor.getTuning(); t.name = nameBox.getText().trim().substring (0, 40); if (t.name.isNotEmpty()) edited (t); };
    centsBox.setInputRestrictions (10, "0123456789.-");
    centsBox.setJustification (juce::Justification::centred);
    centsBox.setTooltip ("The selected note, in cents above the first note (type a value and press Return)");
    centsBox.onReturnKey = [this] { setSelectedCents (centsBox.getText().getDoubleValue()); };
    downButton.onClick = [this] { setSelectedCents (circle.getTuning().degreeCents (circle.selected) - 1.0); };
    upButton.onClick = [this] { setSelectedCents (circle.getTuning().degreeCents (circle.selected) + 1.0); };
    addButton.setTooltip ("A new note halfway to the next one");
    addButton.onClick = [this]
    {
        auto t = processor.getTuning();
        if ((int) t.cents.size() >= TuningData::maxNotes) return;
        const int d = juce::jlimit (0, t.notesPerPeriod() - 1, circle.selected);
        const double mid = 0.5 * (t.degreeCents (d) + t.degreeCents (d + 1));
        t.cents.insert (t.cents.begin() + d, mid);
        circle.selected = d + 1;
        edited (t);
    };
    removeButton.onClick = [this]
    {
        auto t = processor.getTuning();
        const int d = circle.selected;
        if (d < 1 || d >= t.notesPerPeriod() || t.notesPerPeriod() < 2) return;
        t.cents.erase (t.cents.begin() + (d - 1));
        circle.selected = juce::jlimit (1, juce::jmax (1, t.notesPerPeriod() - 1), d);
        edited (t);
    };
    saveButton.setTooltip ("Keep this tuning in My tunings (saved as a Scala .scl file)");
    saveButton.onClick = [this]
    {
        auto t = processor.getTuning();
        t.name = nameBox.getText().trim().isNotEmpty() ? nameBox.getText().trim().substring (0, 40) : t.name;
        t.name = t.name.replace (" (edited)", "");
        t.categoryId = CatMine; t.category = categoryNames()[CatMine];
        if (saveUserTuning (t)) { processor.setTuning (t, true); status = "Saved in My tunings: " + t.name; userTunings = loadUserTunings(); refreshList(); }
        else status = "Couldn't save the tuning";
        repaint();
    };
    loadButton.setTooltip ("Open a Scala .scl or Ableton .ascl tuning file (an .ascl also sets the root pitch)");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Open a tuning (Scala .scl or Ableton .ascl)", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.scl;*.ascl");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [sp = juce::Component::SafePointer<MicroCatEditor> (this)] (const juce::FileChooser& fc)
        {
            if (sp == nullptr || ! fc.getResult().existsAsFile()) return;
            Tuning t;
            juce::String err;
            if (parseScl (fc.getResult().loadFileAsString(), t, &err))
            {
                t.name = fc.getResult().getFileNameWithoutExtension();
                sp->processor.setTuning (t, true);
                int key = 60; double hz = 0.0;
                if (rootFromReference (t, key, hz)) sp->processor.setRoot (key, hz);   // .ascl: its reference pitch
                sp->status = "Opened " + fc.getResult().getFileName()
                           + (hz > 0.0 ? " (root " + juce::MidiMessage::getMidiNoteName (key, true, true, 4) + " = " + juce::String (hz, 2) + " Hz)" : juce::String());
            }
            else sp->status = "Couldn't read " + fc.getResult().getFileName() + ": " + err;
            sp->repaint();
        });
    };
    exportButton.setTooltip ("Save this tuning as a Scala .scl file (other synths can load it too)");
    exportButton.onClick = [this]
    {
        const auto t = processor.getTuning();
        chooser = std::make_unique<juce::FileChooser> ("Save as a Scala file",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (juce::File::createLegalFileName (t.name) + ".scl"), "*.scl");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [sp = juce::Component::SafePointer<MicroCatEditor> (this), t] (const juce::FileChooser& fc)
        {
            if (sp == nullptr || fc.getResult() == juce::File()) return;
            const auto f = fc.getResult().withFileExtension ("scl");
            sp->status = f.replaceWithText (toScl (t)) ? "Saved " + f.getFullPathName() : "Couldn't save " + f.getFileName();
            sp->repaint();
        });
    };
    newButton.setTooltip ("Start a new tuning from equal steps");
    newButton.onClick = [this]
    {
        juce::PopupMenu m;
        for (int n : { 5, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 26, 27, 29, 31, 34, 36, 41, 43, 46, 48, 53, 58, 72 })
            m.addItem (n, juce::String (n) + " equal steps per octave");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (newButton),
                         [sp = juce::Component::SafePointer<MicroCatEditor> (this)] (int r) { if (sp != nullptr && r > 0) sp->makeEqual (r); });
    };
    concertButton.setTooltip ("The root note at normal concert pitch (A4 = 440 Hz)");
    concertButton.onClick = [this] { const int k = processor.currentSettings().rootKey; processor.setRoot (k, MicroCatProcessor::concertHz (k)); };

    rootHz.setSliderStyle (juce::Slider::LinearBar);
    rootHz.setTooltip ("The root note's pitch in Hz (drag, or double-click to type)");
    rootHzAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, "rootHz", rootHz);
    voices.setSliderStyle (juce::Slider::IncDecButtons);
    voices.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 26);
    voices.setTooltip ("How many notes at once (one MIDI channel each, 2 - 16)");
    voicesAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, "voices", voices);
    rootBox.getCombo().setTooltip ("The key that plays the tuning's first note");
    mapBox.getCombo().setTooltip ("One key per step: every next key is the next note of the tuning (19-EDO = 19 keys an octave). "
                                  "12-key layout: a C stays on C, the keys are only moved by some cents");
    rangeBox.getCombo().setTooltip ("Must be the same as your synth's MPE pitch-bend range (MPE synths: 48)");
    configToggle.setTooltip ("Tells the synth it gets MPE and the bend range (MPE Configuration Message + RPN 0)");
    bypassToggle.setTooltip ("Play the notes as they come in (normal tuning)");
    mtsToggle.setTooltip ("Tune every MTS-ESP synth in the project (Surge XT, Vital, Pigments, Serum 2, u-he, Kontakt ...) - "
                          "no MIDI routing, no MPE, like Ableton's tuning. The MIDI out then plays the notes as they are");
    mtsResetButton.setTooltip ("A crashed program left MTS-ESP thinking another master runs: reset it and take over");
    mtsResetButton.onClick = [this] { processor.mts.reset(); };
    justBox.getCombo().setTooltip ("Live just intonation: the chord you hold is tuned pure, right now, around its lowest note "
                                   "(5-limit: glowing thirds and fifths; 7-limit: bluesy 7ths). Change the chord and the notes glide to their new pure places");
    for (size_t i = 0; i < library().size(); ++i) tuningBBox.addItem (library()[i].name, (int) i + 1);
    tuningBBox.setTooltip ("Tuning B: the Morph knob melts tuning A into this one");
    tuningBBox.onChange = [this]
    {
        const int i = tuningBBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (i, (int) library().size())) processor.setTuningB (library()[(size_t) i]);
    };
    morph.setSliderStyle (juce::Slider::LinearBar);
    morph.setTooltip ("Morph: 0% = tuning A, 100% = tuning B. Turn it while notes ring: they glide (automate it in Cubase!)");
    morphAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, "morph", morph);
    addAndMakeVisible (justBox);
    addAndMakeVisible (tuningBBox);
    addAndMakeVisible (morph);
    addChildComponent (mtsResetButton);

    for (juce::Component* c : { (juce::Component*) &presetBar, (juce::Component*) &nameBox, (juce::Component*) &centsBox,
                                (juce::Component*) &downButton, (juce::Component*) &upButton, (juce::Component*) &addButton,
                                (juce::Component*) &removeButton, (juce::Component*) &saveButton, (juce::Component*) &loadButton,
                                (juce::Component*) &exportButton, (juce::Component*) &newButton, (juce::Component*) &concertButton,
                                (juce::Component*) &keyboard, (juce::Component*) &rootBox, (juce::Component*) &mapBox, (juce::Component*) &rangeBox,
                                (juce::Component*) &rootHz, (juce::Component*) &voices, (juce::Component*) &configToggle, (juce::Component*) &bypassToggle,
                                (juce::Component*) &mtsToggle })
        addAndMakeVisible (c);

    userTunings = loadUserTunings();
    refreshList();
    showTuning();
    resized();
    startTimerHz (10);
}

MicroCatEditor::~MicroCatEditor() { stopTimer(); list.setModel (nullptr); }

void MicroCatEditor::refreshList()
{
    shown.clear();
    shownLibraryIndex.clear();
    const auto q = search.getText().trim();
    auto matches = [&] (const Tuning& t)
    {
        return (category < 0 || t.categoryId == category)
            && (q.isEmpty() || t.name.containsIgnoreCase (q) || t.description.containsIgnoreCase (q) || t.category.containsIgnoreCase (q));
    };
    const auto& lib = library();
    for (size_t i = 0; i < lib.size(); ++i) if (matches (lib[i])) { shown.push_back (lib[i]); shownLibraryIndex.push_back ((int) i); }
    for (auto& t : userTunings) if (matches (t)) { shown.push_back (t); shownLibraryIndex.push_back (-1); }
    updatingList = true;
    list.updateContent();
    list.deselectAllRows();
    const int cur = processor.currentPreset.load();
    const auto now = processor.getTuning();
    for (size_t i = 0; i < shown.size(); ++i)
        if ((cur >= 0 && shownLibraryIndex[i] == cur) || (cur < 0 && shownLibraryIndex[i] < 0 && shown[i].name == now.name))
            list.selectRow ((int) i, false, true);
    updatingList = false;
    list.repaint();
}

void MicroCatEditor::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) shown.size())) return;
    const auto& t = shown[(size_t) row];
    if (selected)
    {
        g.setColour (cat::Colours::accent.withAlpha (0.25f));
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f), 5.0f);
    }
    g.setColour (cat::Colours::text);
    g.setFont (cat::catFont (14.0f, true));
    g.drawText (t.name, 10, 3, w - 20, 18, juce::Justification::centredLeft, true);
    g.setColour (cat::Colours::textDim);
    g.setFont (cat::catFont (11.0f));
    g.drawText (t.category + "  -  " + juce::String (t.notesPerPeriod()) + " notes", 10, 20, w - 20, 15, juce::Justification::centredLeft, true);
}

void MicroCatEditor::selectedRowsChanged (int row)
{
    if (updatingList || ! juce::isPositiveAndBelow (row, (int) shown.size())) return;
    if (shownLibraryIndex[(size_t) row] >= 0) processor.applyPreset (shownLibraryIndex[(size_t) row]);
    else { processor.setTuning (shown[(size_t) row], true); }
    circle.selected = 1;
    status = {};
}

bool MicroCatEditor::pickByName (const juce::String& name)
{
    for (size_t i = 0; i < shown.size(); ++i)
        if (shown[i].name == name) { list.selectRow ((int) i); return true; }
    return false;
}

void MicroCatEditor::edited (Tuning t)
{
    if (t.mapping == MapTwelve && t.cents.size() != 12) t.mapping = MapPerStep;
    if (t.categoryId != CatMine)
    {
        t.categoryId = CatMine;
        t.category = categoryNames()[CatMine];
        if (! t.name.endsWith (" (edited)")) t.name = t.name.substring (0, 30) + " (edited)";
    }
    juce::String why;
    if (! t.isValid (&why)) { status = why; repaint(); return; }
    processor.setTuning (t, true);
    status = {};
}

void MicroCatEditor::setSelectedCents (double c)
{
    auto t = processor.getTuning();
    const int d = circle.selected;
    if (d < 1 || d >= t.notesPerPeriod()) return;
    t.cents[(size_t) d - 1] = juce::jlimit (t.degreeCents (d - 1) + 0.5, t.degreeCents (d + 1) - 0.5, c);
    edited (t);
}

void MicroCatEditor::makeEqual (int n)
{
    Tuning t;
    t.name = juce::String (n) + "-EDO (mine)";
    t.description = juce::String (n) + " equal steps per octave";
    for (int k = 1; k <= n; ++k) t.cents.push_back (1200.0 * k / n);
    t.mapping = n == 12 ? MapTwelve : MapPerStep;
    t.categoryId = CatMine; t.category = categoryNames()[CatMine];
    circle.selected = 1;
    processor.setTuning (t, true);
}

void MicroCatEditor::showTuning()
{
    const auto t = processor.getTuning();
    circle.selected = juce::jlimit (1, juce::jmax (1, t.notesPerPeriod() - 1), circle.selected);
    circle.setTuning (t);
    if (! nameBox.hasKeyboardFocus (true)) nameBox.setText (t.name, false);
    if (! centsBox.hasKeyboardFocus (true))
        centsBox.setText (t.notesPerPeriod() > 1 ? juce::String (t.degreeCents (circle.selected), 2) : juce::String(), false);
    const bool canEdit = t.notesPerPeriod() > 1;
    for (auto* b : { &downButton, &upButton, &removeButton }) b->setEnabled (canEdit);
    centsBox.setEnabled (canEdit);
    repaint (noteLabelArea);
}

void MicroCatEditor::timerCallback()
{
    const auto bName = processor.getTuningB().name;
    if (bName != shownB)
    {
        shownB = bName;
        int id = 0;
        for (size_t i = 0; i < library().size(); ++i) if (library()[i].name == bName) id = (int) i + 1;
        if (id > 0) tuningBBox.setSelectedId (id, juce::dontSendNotification);
        else tuningBBox.setText (bName, juce::dontSendNotification);
    }
    const auto ms = processor.mts.statusText();
    const bool showReset = processor.mts.canReset();
    if (ms != mtsStatus || showReset != mtsResetButton.isVisible())
    {
        mtsStatus = ms;
        mtsResetButton.setVisible (showReset);
        repaint (mtsStatusArea);
    }
    if (processor.tuningVersion.load() != shownVersion)
    {
        shownVersion = processor.tuningVersion.load();
        showTuning();
        refreshList();
    }
}

void MicroCatEditor::paint (juce::Graphics& g)
{
    g.fillAll (cat::Colours::background);
    cat::CatLookAndFeel::drawLogo (g, logoArea.toFloat(), "MICRO", "CAT");
    cat::CatLookAndFeel::drawGlassPanel (g, list.getBounds().expanded (4).toFloat(), 8.0f);
    cat::CatLookAndFeel::drawGlassPanel (g, settingsArea.toFloat(), 10.0f);

    const auto t = circle.getTuning();
    g.setColour (cat::Colours::textDim);
    g.setFont (cat::catFont (12.5f, true));
    if (t.notesPerPeriod() > 1)
    {
        const double c = t.degreeCents (circle.selected);
        const double off = c - 100.0 * std::round (c / 100.0);
        g.drawText ("NOTE " + juce::String (circle.selected + 1) + "  (" + centsText (off) + " from 12-EDO)", noteLabelArea, juce::Justification::centredLeft);
    }
    else g.drawText ("ONE STEP, REPEATING", noteLabelArea, juce::Justification::centredLeft);

    // settings: titles
    auto s = settingsArea.reduced (14, 10);
    g.setColour (cat::Colours::text);
    g.setFont (cat::catFont (13.0f, true));
    g.drawText ("PLAYING", s.removeFromTop (20), juce::Justification::centredLeft);
    g.setColour (cat::Colours::textDim);
    g.setFont (cat::catFont (11.5f));
    g.drawText ("Voices", voices.getBounds().withX (settingsArea.getX() + 14).withWidth (60), juce::Justification::centredLeft);
    g.drawText ("Morph to", morphLabelArea, juce::Justification::centredLeft);
    g.drawFittedText ("IN CUBASE: Micro-Cat on a MIDI track, its MIDI Out to an MPE synth (Vital, Surge XT, Serum, Pigments, "
                      "Retrologue in MPE mode) with the same Bend range. Or MTS-ESP: no routing at all.\n"
                      "GLIDE-, SING-, TALK-CAT follow over Cat Link - don't also send them Micro-Cat's MIDI.",
                      helpArea, juce::Justification::topLeft, 14);
    g.setColour (processor.mts.state() == micro::MtsMaster::State::Connected ? cat::Colours::accent : cat::Colours::textDim);
    g.setFont (cat::catFont (11.0f));
    g.drawFittedText (mtsStatus, mtsStatusArea, juce::Justification::topLeft, 2);
    if (status.isNotEmpty())
    {
        g.setColour (cat::Colours::accent);
        g.drawText (status, infoArea, juce::Justification::centredLeft, true);
    }
}

void MicroCatEditor::resized()
{
    if (categoryButtons.size() < 5) return;   // still being built
    auto r = getLocalBounds().reduced (14, 10);
    auto top = r.removeFromTop (40);
    logoArea = top.removeFromLeft (170);
    top.removeFromLeft (10);
    presetBar.setBounds (top.removeFromLeft (470));
    bypassToggle.setBounds (top.removeFromRight (110));

    r.removeFromTop (10);
    infoArea = r.removeFromBottom (18);
    keyboard.setBounds (r.removeFromBottom (112));
    r.removeFromBottom (10);

    auto left = r.removeFromLeft (270);
    search.setBounds (left.removeFromTop (30));
    left.removeFromTop (6);
    auto cats = left.removeFromTop (28);
    const int cw = cats.getWidth() / 5;
    for (auto* b : categoryButtons) b->setBounds (cats.removeFromLeft (cw).reduced (2, 0));
    left.removeFromTop (10);
    list.setBounds (left.reduced (4));
    r.removeFromLeft (14);

    settingsArea = r.removeFromRight (300);
    r.removeFromRight (14);
    auto s = settingsArea.reduced (14, 10);
    s.removeFromTop (24);
    rootBox.setBounds (s.removeFromTop (30)); s.removeFromTop (6);
    auto hz = s.removeFromTop (30);
    concertButton.setBounds (hz.removeFromRight (104)); hz.removeFromRight (6);
    rootHz.setBounds (hz); s.removeFromTop (6);
    mapBox.setBounds (s.removeFromTop (30)); s.removeFromTop (6);
    rangeBox.setBounds (s.removeFromTop (30)); s.removeFromTop (6);
    auto vr = s.removeFromTop (30);
    vr.removeFromLeft (60);
    voices.setBounds (vr.removeFromLeft (130)); s.removeFromTop (6);
    configToggle.setBounds (s.removeFromTop (30)); s.removeFromTop (8);
    justBox.setBounds (s.removeFromTop (30)); s.removeFromTop (6);
    auto br = s.removeFromTop (30);
    morphLabelArea = br.removeFromLeft (60);
    tuningBBox.setBounds (br.removeFromLeft (118)); br.removeFromLeft (6);
    morph.setBounds (br);
    s.removeFromTop (8);
    auto mr = s.removeFromTop (30);
    mtsToggle.setBounds (mr.removeFromLeft (150));
    mr.removeFromLeft (6);
    mtsResetButton.setBounds (mr);
    mtsStatusArea = s.removeFromTop (30);
    s.removeFromTop (6);
    helpArea = s;

    // the middle: the circle, then the selected note and the tuning's name
    auto mid = r;
    auto nameRow = mid.removeFromBottom (32);
    mid.removeFromBottom (6);
    auto noteRow = mid.removeFromBottom (32);
    mid.removeFromBottom (8);
    circle.setBounds (mid);
    noteLabelArea = noteRow.removeFromLeft (170);
    centsBox.setBounds (noteRow.removeFromLeft (74)); noteRow.removeFromLeft (4);
    downButton.setBounds (noteRow.removeFromLeft (36)); noteRow.removeFromLeft (4);
    upButton.setBounds (noteRow.removeFromLeft (36)); noteRow.removeFromLeft (8);
    const int nb = (noteRow.getWidth() - 6) / 2;
    addButton.setBounds (noteRow.removeFromLeft (nb)); noteRow.removeFromLeft (6);
    removeButton.setBounds (noteRow);
    nameBox.setBounds (nameRow.removeFromLeft (170)); nameRow.removeFromLeft (6);
    const int bw = (nameRow.getWidth() - 18) / 4;
    for (auto* b : { &saveButton, &newButton, &loadButton, &exportButton }) { b->setBounds (nameRow.removeFromLeft (bw)); nameRow.removeFromLeft (6); }
}

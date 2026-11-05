#include "PluginEditor.h"
#include <numeric>

using namespace micro;

namespace
{
    constexpr int designW = 1260, designH = 830;
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
    // the circle the notes sit on (the brass dial is drawn round it)
    const float s = (float) juce::jmin (getWidth(), getHeight());
    return getLocalBounds().toFloat().withSizeKeepingCentre (s * 0.572f, s * 0.572f);
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

namespace
{
    const juce::Colour brassHi (0xfff3d99b), brassMid (0xffc79a4c), brassLo (0xff7a5a26), gold (0xffd8b169), ivory (0xffede3cc), parchment (0xffc9bc9f);

    juce::ColourGradient brass (juce::Rectangle<float> r)
    {
        juce::ColourGradient gr (brassHi, r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.3f, brassLo, r.getRight(), r.getBottom(), true);
        gr.addColour (0.5, brassMid);
        return gr;
    }

    void brassDot (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour edge = juce::Colour (0xff0b1530))
    {
        const auto r = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);
        g.setGradientFill (brass (r));
        g.fillEllipse (r);
        g.setColour (edge);
        g.drawEllipse (r, 2.0f);
    }

    const char* const noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    /** A simple ratio near these cents (within a cent, the smallest one), or empty. */
    juce::String ratioNear (double cents)
    {
        for (int q = 1; q <= 16; ++q)
        {
            const int p = (int) std::lround (q * std::pow (2.0, cents / 1200.0));
            if (p <= 0 || p > 32 || std::gcd (p, q) != 1) continue;
            if (std::abs (1200.0 * std::log2 ((double) p / q) - cents) < 1.0) return juce::String (p) + "/" + juce::String (q);
        }
        return {};
    }

    /** A microtone's name: the nearest normal note and how far off it ("E -14"). */
    juce::String microName (double semis, bool octave)
    {
        const int n = (int) std::lround (semis);
        const int c = (int) std::lround ((semis - n) * 100.0);
        juce::String s = noteNames[((n % 12) + 12) % 12];
        if (octave) s << (n / 12 - 1);
        if (c != 0) s << " " << (c > 0 ? "+" : juce::String::fromUTF8 ("\xe2\x88\x92")) << std::abs (c);
        return s;
    }
}

void TuningCircle::paint (juce::Graphics& g)
{
    if (tuning.cents.empty()) return;
    const float s = (float) juce::jmin (getWidth(), getHeight());
    const auto c = getLocalBounds().toFloat().getCentre();
    const auto disc = [&] (float frac) { return juce::Rectangle<float> (s * frac * 2.0f, s * frac * 2.0f).withCentre (c); };
    const float r = ring().getWidth() * 0.5f;

    // the brass rim and the night-blue dial
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (disc (0.49f).translated (0.0f, s * 0.012f));
    g.setGradientFill (brass (disc (0.48f)));
    g.fillEllipse (disc (0.48f));
    g.setColour (juce::Colour (0xff5c4318));
    g.drawEllipse (disc (0.48f), 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff15254a), c.x - s * 0.1f, c.y - s * 0.12f, juce::Colour (0xff0a1430), c.x + s * 0.4f, c.y + s * 0.4f, true));
    g.fillEllipse (disc (0.436f));
    g.setColour (brassMid);
    g.drawEllipse (disc (0.436f), 1.5f);
    g.setColour (brassMid.withAlpha (0.35f));
    g.drawEllipse (disc (0.39f), 0.8f);

    // the dial: a tick every 10 cents, a long one every 100 (12-EDO), the note names
    const double period = tuning.period();
    for (int k = 0; k * 10.0 < period - 1e-6; ++k)
    {
        const bool big = k % 10 == 0;
        const auto outer = pointFor (k * 10.0, s * 0.436f), inner = pointFor (k * 10.0, s * (big ? 0.396f : 0.418f));
        g.setColour (brassMid.withAlpha (big ? 0.9f : 0.4f));
        g.drawLine ({ inner, outer }, big ? 2.0f : 1.0f);
    }
    if (std::abs (period - 1200.0) < 1e-3)
    {
        g.setFont (cat::displayFont (s * 0.032f));
        g.setColour (gold);
        for (int k = 0; k < 12; ++k)
            g.drawText (tuning.noteNames.size() == 12 ? tuning.noteNames[k] : juce::String (noteNames[k]),
                        juce::Rectangle<float> (s * 0.08f, s * 0.04f).withCentre (pointFor (k * 100.0, s * 0.357f)), juce::Justification::centred);
    }
    g.setColour (brassMid.withAlpha (0.4f));
    {
        juce::Path dash, circle;
        circle.addEllipse (ring());
        const float pattern[] = { 2.0f, 6.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dash, circle, pattern, 2);
        g.fillPath (dash);
    }

    // the tuning: a star through all its notes
    juce::Path star;
    for (int d = 0; d < tuning.notesPerPeriod(); ++d)
    {
        const auto pt = pointFor (tuning.degreeCents (d), r);
        if (d == 0) star.startNewSubPath (pt); else star.lineTo (pt);
    }
    star.closeSubPath();
    g.setColour (gold.withAlpha (0.08f));
    g.fillPath (star);
    g.setColour (gold.withAlpha (0.85f));
    g.strokePath (star, juce::PathStrokeType (1.2f));

    // the needle: the last note played (or the selected one)
    const double needle = needleCents >= 0.0 ? needleCents : tuning.degreeCents (juce::jlimit (0, tuning.notesPerPeriod() - 1, selected));
    {
        const auto tip = pointFor (needle, r - s * 0.018f);
        juce::Path n;
        const auto dir = (tip - c) / juce::jmax (1.0f, tip.getDistanceFrom (c));
        const juce::Point<float> side (-dir.y * 3.5f, dir.x * 3.5f);
        n.startNewSubPath (c + side); n.lineTo (tip); n.lineTo (c - side); n.lineTo (c - dir * s * 0.05f); n.closeSubPath();
        g.setGradientFill (brass (disc (0.3f)));
        g.fillPath (n);
    }

    // the notes
    const bool many = tuning.notesPerPeriod() > 36;
    for (int d = 0; d < tuning.notesPerPeriod(); ++d)
    {
        const double cents = tuning.degreeCents (d);
        const auto pt = pointFor (cents, r);
        const float size = s * (d == selected ? 0.0196f : (many ? 0.008f : 0.0143f));
        brassDot (g, pt, d == 0 ? size * 1.15f : size);
        if (d == selected)
        {
            g.setColour (ivory.withAlpha (0.8f));
            g.drawEllipse (juce::Rectangle<float> (size * 2.0f + 8.0f, size * 2.0f + 8.0f).withCentre (pt), 1.2f);
        }
        if (! many || d == selected)
        {
            const auto lp = pointFor (cents, r - s * 0.055f);
            g.setColour (d == selected ? ivory : parchment.withAlpha (0.75f));
            g.setFont (cat::catFont (d == selected ? 12.5f : 10.5f, d == selected));
            g.drawText (juce::String ((int) std::lround (cents)), juce::Rectangle<float> (44.0f, 14.0f).withCentre (lp), juce::Justification::centred);
        }
    }

    // what sounds now: glowing stars on the circle, with their cents off 12-EDO
    for (double sc : sounding)
    {
        const auto pt = pointFor (sc, r);
        for (float k : { 3.0f, 2.0f, 1.4f })
        {
            g.setColour (juce::Colour (0xfffff2c8).withAlpha (0.12f));
            g.fillEllipse (juce::Rectangle<float> (s * 0.02f * k * 2.0f, s * 0.02f * k * 2.0f).withCentre (pt));
        }
        g.setColour (juce::Colour (0xfffff6dc));
        g.fillEllipse (juce::Rectangle<float> (s * 0.022f, s * 0.022f).withCentre (pt));
        const auto lp = pointFor (sc, r + s * 0.05f);
        const double off = sc - 100.0 * std::round (sc / 100.0);
        g.setColour (juce::Colour (0xfffff6dc));
        g.setFont (cat::catFont (11.5f, true));
        g.drawText ((off >= 0 ? "+" : juce::String::fromUTF8 ("\xe2\x88\x92")) + juce::String (std::abs (off), 1),
                    juce::Rectangle<float> (50.0f, 14.0f).withCentre (lp), juce::Justification::centred);
    }

    // the hub
    brassDot (g, c, s * 0.0286f);
    g.setColour (juce::Colour (0xff3a2a10));
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (c));
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
        g.setColour (held ? cat::Colours::accent : (black ? juce::Colour (0xff142245) : juce::Colour (0xffede3cc)));
        g.fillRoundedRectangle (r, 4.0f);
        const auto kp = keyPitch (pm, key);
        const auto ink = held ? juce::Colour (0xff1a1206) : (black ? cat::Colours::text : juce::Colour (0xff2a2210));
        g.setColour (ink);
        g.setFont (cat::catFont (11.0f, true));
        g.drawText (juce::MidiMessage::getMidiNoteName (key, true, true, 4), r.removeFromTop (18.0f), juce::Justification::centred);
        g.setFont (cat::catFont (10.5f));
        if (! kp.valid) { g.drawText ("-", r, juce::Justification::centred); continue; }
        // what it really plays: the nearest normal note and the cents off it, then the Hz
        auto lower = r.removeFromBottom (r.getHeight() * 0.55f);
        const int cents = (int) std::lround (kp.bend * 100.0);
        g.setColour (cents == 0 ? ink.withAlpha (0.6f) : (held ? ink : (black ? cat::Colours::accent : juce::Colour (0xff8a5a10))));
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

    nameBox.setTooltip ("The tuning's name (Save as mine keeps it under this name)");
    nameBox.setTextToShowWhenEmpty ("Name for Save as mine", cat::Colours::textDim);
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
                                (juce::Component*) &mtsToggle, (juce::Component*) &ownSoundToggle, (juce::Component*) &soundBox,
                                (juce::Component*) &volumeSlider })
        addAndMakeVisible (c);
    ownSoundToggle.setTooltip ("Micro-Cat plays the tuning itself, like an instrument (turn off to only send tuned MIDI to another synth)");

    for (auto* t : { &catalogueTab, &instrumentTab })
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (708);
        addAndMakeVisible (*t);
    }
    catalogueTab.onClick = [this] { showTab (0); };
    instrumentTab.onClick = [this] { showTab (1); };
    catalogueTab.setTooltip ("Every tuning: equal, just and historical, from around the world, and yours");
    instrumentTab.setTooltip ("How Micro-Cat plays: its own sound, the root, MPE, live just intonation, MTS-ESP");

    userTunings = loadUserTunings();
    refreshList();
    showTuning();
    showTab (0);
    resized();
    startTimerHz (20);
}

MicroCatEditor::~MicroCatEditor() { stopTimer(); list.setModel (nullptr); }

void MicroCatEditor::showTab (int t)
{
    tab = t;
    catalogueTab.setToggleState (t == 0, juce::dontSendNotification);
    instrumentTab.setToggleState (t == 1, juce::dontSendNotification);
    search.setVisible (t == 0);
    nameBox.setVisible (t == 0);
    list.setVisible (t == 0);
    for (auto* b : categoryButtons) b->setVisible (t == 0);
    for (juce::Component* c : { (juce::Component*) &ownSoundToggle, (juce::Component*) &soundBox, (juce::Component*) &volumeSlider,
                                (juce::Component*) &rootBox, (juce::Component*) &rootHz, (juce::Component*) &concertButton,
                                (juce::Component*) &mapBox, (juce::Component*) &rangeBox, (juce::Component*) &voices,
                                (juce::Component*) &configToggle, (juce::Component*) &justBox, (juce::Component*) &mtsToggle })
        c->setVisible (t == 1);
    mtsResetButton.setVisible (t == 1 && processor.mts.canReset());
    repaint();
}

void MicroCatEditor::updateSounding()
{
    bool changed = false;
    for (int k = 0; k < 128; ++k)
    {
        const bool h = processor.engine.held[(size_t) k].load();
        if (h != shownHeld[(size_t) k])
        {
            shownHeld[(size_t) k] = h;
            changed = true;
            if (h) lastKey = k;
        }
    }
    static uint32_t lastBends = 0;
    const auto bends = (uint32_t) processor.engine.bendUpdates.load();
    if (bends != lastBends) { lastBends = bends; changed = true; }    // morph / live just moved held notes
    if (! changed) return;

    soundingNow.clear();
    const auto pm = processor.currentMap();
    const double rootSemis = 69.0 + 12.0 * std::log2 (pm.rootHz / 440.0);
    const double period = circle.getTuning().period();
    std::vector<double> cents;
    for (int k = 0; k < 128; ++k)
    {
        if (! shownHeld[(size_t) k]) continue;
        bool valid = true;
        const double semis = keySemis (pm, k, valid);
        if (! valid) continue;
        double c = std::fmod ((semis - rootSemis) * 100.0, period);
        if (c < 0.0) c += period;
        if (c > period - 0.01) c = 0.0;
        soundingNow.push_back ({ k, semis, c });
        cents.push_back (c);
    }
    double needle = -1.0;
    for (auto& sn : soundingNow) if (sn.key == lastKey) needle = sn.cents;
    if (needle < 0.0 && ! soundingNow.empty()) needle = soundingNow.back().cents;
    circle.setSounding (std::move (cents), needle);
    repaint (asideArea);
}

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
    repaint (asideArea);
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
    if (ms != mtsStatus || (showReset && tab == 1) != mtsResetButton.isVisible())
    {
        mtsStatus = ms;
        mtsResetButton.setVisible (showReset && tab == 1);
        repaint (mtsStatusArea);
    }
    if (processor.tuningVersion.load() != shownVersion)
    {
        shownVersion = processor.tuningVersion.load();
        showTuning();
        refreshList();
        shownHeld.fill (false);       // re-reads the sounding notes against the new tuning
        repaint (asideArea);
    }
    updateSounding();
}

void MicroCatEditor::paint (juce::Graphics& g)
{
    cat::paintBackdrop (g, getLocalBounds());
    const auto mono = [] (float h) { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain)); };
    const auto rule = [&] (int y, juce::Rectangle<int> span)
    {
        g.setColour (gold.withAlpha (0.4f));
        g.fillRect (span.getX(), y, span.getWidth(), 1);
    };

    // --- the aside: the title, the tuning, the read-out ---
    auto a = asideArea;
    g.setColour (gold);
    g.setFont (cat::displayFont (38.0f));
    g.drawText (juce::String::fromUTF8 ("Micro\xc2\xb7" "Cat"), a.removeFromTop (44), juce::Justification::centredLeft);
    g.setColour (ivory);
    g.setFont (cat::catFont (16.0f).italicised());
    g.drawText ("Observatory of tunings", a.removeFromTop (22), juce::Justification::centredLeft);
    a.removeFromTop (46);                                        // the preset bar
    const auto t = circle.getTuning();
    rule (a.getY(), a);
    a.removeFromTop (8);
    g.setColour (ivory);
    g.setFont (cat::catFont (24.0f, true));
    g.drawFittedText (t.name, a.removeFromTop (30), juce::Justification::centredLeft, 1);
    g.setColour (parchment);
    g.setFont (cat::catFont (14.0f).italicised());
    juce::String sub = t.description.isNotEmpty() ? t.description : t.category;
    g.drawFittedText (juce::String (t.notesPerPeriod()) + " notes  -  " + sub, a.removeFromTop (38), juce::Justification::topLeft, 2);
    a.removeFromTop (4);
    rule (a.getY(), a);

    // NOW SOUNDING: the microtone, big
    auto ro = readoutArea;
    g.setColour (parchment);
    g.setFont (cat::catFont (12.0f, true));
    g.drawText (soundingNow.empty() ? "CHOSEN NOTE" : "NOW SOUNDING", ro.removeFromTop (18), juce::Justification::centredLeft);
    const auto pm = processor.currentMap();
    const double rootSemis = 69.0 + 12.0 * std::log2 (pm.rootHz / 440.0);
    double semis = 0.0, cents = 0.0;
    int key = -1;
    if (! soundingNow.empty())
    {
        const Sounding* sn = &soundingNow.back();
        for (auto& x : soundingNow) if (x.key == lastKey) sn = &x;
        semis = sn->semis; cents = sn->cents; key = sn->key;
    }
    else if (t.notesPerPeriod() > 0)
    {
        cents = t.degreeCents (juce::jlimit (0, t.notesPerPeriod() - 1, circle.selected));
        semis = rootSemis + cents / 100.0;
    }
    g.setColour (soundingNow.empty() ? ivory : juce::Colour (0xfffff2c8));
    g.setFont (cat::displayFont (40.0f));
    g.drawText (microName (semis, ! soundingNow.empty()), ro.removeFromTop (46), juce::Justification::centredLeft);
    // the step it is, its cents, how far from 12-EDO, Hz, the ratio
    int step = -1;
    for (int d = 0; d <= t.notesPerPeriod(); ++d)
        if (std::abs (t.degreeCents (d) - cents) < 0.6) { step = d % juce::jmax (1, t.notesPerPeriod()); break; }
    const double off = (semis - std::round (semis)) * 100.0;
    const double hz = 440.0 * std::pow (2.0, (semis - 69.0) / 12.0);
    const auto ratio = ratioNear (cents);
    struct Row { const char* k; juce::String v; };
    const Row rows[] = {
        { "Step", step >= 0 ? juce::String (step + 1) + " of " + juce::String (t.notesPerPeriod()) : juce::String ("between steps (morph / just)") },
        { "Above the root", juce::String (cents, 2) + juce::String::fromUTF8 (" \xc2\xa2") },
        { "From 12-EDO", (off >= 0 ? "+" : juce::String::fromUTF8 ("\xe2\x88\x92")) + juce::String (std::abs (off), 2) + juce::String::fromUTF8 (" \xc2\xa2") },
        { "Frequency", juce::String (hz, 2) + " Hz" },
        { "Pure ratio", ratio.isNotEmpty() ? juce::String::fromUTF8 ("\xe2\x89\x88 ") + ratio : juce::String ("-") },
        { "Root", juce::MidiMessage::getMidiNoteName (pm.rootKey, true, true, 4) + " = " + juce::String (pm.rootHz, 2) + " Hz" },
    };
    auto rr = rowsArea;
    const int rh = rr.getHeight() / (int) std::size (rows);
    for (auto& row : rows)
    {
        auto line = rr.removeFromTop (rh);
        g.setColour (parchment);
        g.setFont (cat::catFont (15.0f));
        g.drawText (row.k, line, juce::Justification::centredLeft);
        g.setColour (ivory);
        g.setFont (mono (13.5f));
        g.drawText (row.v, line, juce::Justification::centredRight);
    }
    // every note held, when it is a chord
    if (soundingNow.size() > 1)
    {
        juce::String chord;
        for (auto& x : soundingNow) chord << microName (x.semis, true) << "    ";
        g.setColour (juce::Colour (0xfffff2c8).withAlpha (0.8f));
        g.setFont (mono (12.5f));
        g.drawFittedText (chord.trim(), ro.removeFromTop (18), juce::Justification::centredLeft, 1, 0.7f);
    }

    rule (noteLabelArea.getY() - 8, asideArea);
    g.setColour (parchment);
    g.setFont (cat::catFont (14.0f));
    if (t.notesPerPeriod() > 1)
        g.drawText ("Note " + juce::String (circle.selected + 1), noteLabelArea, juce::Justification::centredLeft);
    else
        g.drawText ("One step", noteLabelArea, juce::Justification::centredLeft);
    g.drawText ("Morph to", morphLabelArea, juce::Justification::centredLeft);

    // --- the right column ---
    auto panel = rightArea.withTrimmedTop (catalogueTab.getBottom() - rightArea.getY() + 6).toFloat();
    g.setColour (juce::Colour (0xcc0a1430));
    g.fillRoundedRectangle (panel, 10.0f);
    g.setColour (brassMid.withAlpha (0.6f));
    g.drawRoundedRectangle (panel.reduced (0.5f), 10.0f, 1.0f);
    if (tab == 1)
    {
        g.setColour (parchment);
        g.setFont (cat::catFont (13.0f));
        g.drawText ("Voices", voices.getBounds().withX (settingsArea.getX()).withWidth (60), juce::Justification::centredLeft);
        g.setFont (cat::catFont (11.5f));
        g.setColour (processor.mts.state() == micro::MtsMaster::State::Connected ? gold : parchment);
        g.drawFittedText (mtsStatus, mtsStatusArea, juce::Justification::topLeft, 2);
        g.setColour (parchment.withAlpha (0.8f));
        g.drawFittedText ("IN CUBASE: Micro-Cat on a MIDI track, its MIDI Out to an MPE synth (Vital, Surge XT, Serum, Pigments, "
                          "Retrologue in MPE mode) with the same Bend range. Or MTS-ESP: no routing at all. "
                          "Glide-, Sing-, Talk-Cat follow over Cat Link - don't also send them Micro-Cat's MIDI.",
                          helpArea, juce::Justification::topLeft, 12);
    }
    if (status.isNotEmpty())
    {
        g.setColour (gold);
        g.setFont (cat::catFont (12.5f));
        g.drawText (status, infoArea, juce::Justification::centredLeft, true);
    }
}

void MicroCatEditor::resized()
{
    if (categoryButtons.size() < 5) return;   // still being built
    auto r = getLocalBounds().reduced (24, 18);
    infoArea = r.removeFromBottom (18);
    keyboard.setBounds (r.removeFromBottom (112));
    r.removeFromBottom (12);

    // the astrolabe
    const int side = juce::jmin (r.getHeight(), 620);
    circle.setBounds (r.removeFromLeft (side).withSizeKeepingCentre (side, side));
    r.removeFromLeft (18);

    // the right column: Catalogue | Instrument
    rightArea = r.removeFromRight (270);
    r.removeFromRight (20);
    {
        auto rc = rightArea;
        auto top = rc.removeFromTop (30);
        bypassToggle.setBounds (top.removeFromRight (90));
        rc.removeFromTop (8);
        auto tabs = rc.removeFromTop (32);
        catalogueTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2).reduced (2, 0));
        instrumentTab.setBounds (tabs.reduced (2, 0));
        rc.removeFromTop (6);
        auto in = rc.reduced (10, 10);
        // catalogue
        auto cl = in;
        search.setBounds (cl.removeFromTop (30));
        cl.removeFromTop (6);
        auto cats = cl.removeFromTop (26);
        const int cw = cats.getWidth() / 5;
        for (auto* b : categoryButtons) b->setBounds (cats.removeFromLeft (cw).reduced (1, 0));
        cl.removeFromTop (8);
        nameBox.setBounds (cl.removeFromBottom (28));
        cl.removeFromBottom (8);
        list.setBounds (cl);
        // instrument
        settingsArea = in;
        auto s = in;
        auto snd = s.removeFromTop (30);
        ownSoundToggle.setBounds (snd.removeFromLeft (96));
        snd.removeFromLeft (6);
        soundBox.setBounds (snd);
        s.removeFromTop (4);
        volumeSlider.setBounds (s.removeFromTop (32));
        s.removeFromTop (8);
        rootBox.setBounds (s.removeFromTop (28)); s.removeFromTop (5);
        auto hz = s.removeFromTop (28);
        concertButton.setBounds (hz.removeFromRight (96)); hz.removeFromRight (6);
        rootHz.setBounds (hz); s.removeFromTop (5);
        mapBox.setBounds (s.removeFromTop (28)); s.removeFromTop (5);
        rangeBox.setBounds (s.removeFromTop (28)); s.removeFromTop (5);
        auto vr = s.removeFromTop (28);
        vr.removeFromLeft (60);
        voices.setBounds (vr.removeFromLeft (130)); s.removeFromTop (5);
        configToggle.setBounds (s.removeFromTop (28)); s.removeFromTop (5);
        justBox.setBounds (s.removeFromTop (28)); s.removeFromTop (8);
        auto mr = s.removeFromTop (28);
        mtsToggle.setBounds (mr.removeFromLeft (140));
        mr.removeFromLeft (6);
        mtsResetButton.setBounds (mr);
        mtsStatusArea = s.removeFromTop (30);
        s.removeFromTop (4);
        helpArea = s;
    }

    // the aside
    asideArea = r;
    auto a = r;
    a.removeFromTop (66);                                         // title
    presetBar.setBounds (a.removeFromTop (34));
    a.removeFromTop (12);
    a.removeFromTop (8 + 30 + 38 + 4);                            // the tuning's name and description
    a.removeFromTop (8);
    readoutArea = a.removeFromTop (84);
    a.removeFromTop (2);
    rowsArea = a.removeFromTop (120);

    // bottom of the aside: the note editor, morph, the four buttons, the name
    auto b = r;
    auto buttons2 = b.removeFromBottom (36);
    b.removeFromBottom (8);
    auto buttons1 = b.removeFromBottom (36);
    b.removeFromBottom (10);
    auto morphRow = b.removeFromBottom (30);
    b.removeFromBottom (8);
    auto noteRow = b.removeFromBottom (30);
    b.removeFromBottom (6);
    auto addRow = b.removeFromBottom (28);
    noteLabelArea = noteRow.removeFromLeft (64);
    centsBox.setBounds (noteRow.removeFromLeft (noteRow.getWidth() - 80)); noteRow.removeFromLeft (6);
    downButton.setBounds (noteRow.removeFromLeft (35)); noteRow.removeFromLeft (4);
    upButton.setBounds (noteRow);
    addButton.setBounds (addRow.removeFromLeft (addRow.getWidth() / 2 - 3)); addRow.removeFromLeft (6);
    removeButton.setBounds (addRow);
    morphLabelArea = morphRow.removeFromLeft (64);
    tuningBBox.setBounds (morphRow.removeFromLeft (morphRow.getWidth() / 2)); morphRow.removeFromLeft (6);
    morph.setBounds (morphRow);
    saveButton.setBounds (buttons1.removeFromLeft (buttons1.getWidth() / 2 - 4)); buttons1.removeFromLeft (8);
    newButton.setBounds (buttons1);
    loadButton.setBounds (buttons2.removeFromLeft (buttons2.getWidth() / 2 - 4)); buttons2.removeFromLeft (8);
    exportButton.setBounds (buttons2);
}

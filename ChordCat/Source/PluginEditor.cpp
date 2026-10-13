#include "PluginEditor.h"
#include "StrumEngine.h"
#include "FactoryPresets.h"
#include "MidiExport.h"

using namespace chordcat;
using cat::Colours::accent;
namespace C = cat::Colours;

namespace
{
    juce::String noteText (int v) { return juce::MidiMessage::getMidiNoteName (v, true, true, 3); }

    void styleSmallButton (juce::TextButton& b, bool toggle = false)
    {
        b.setClickingTogglesState (toggle);
        b.setColour (juce::TextButton::buttonColourId, C::panelLight);
    }

    void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text)
    {
        g.setColour (C::textDim);
        g.setFont (cat::catFont (11.0f, true));
        g.drawText (text.toUpperCase(), r, juce::Justification::centredLeft);
    }
}

//==============================================================================
// ChordPad
ChordPad::ChordPad (ChordCatEditor& o, int i) : owner (o), index (i) {}

void ChordPad::setContent (const ProgressionSlot& s, const std::vector<int>& n, const juce::String& r)
{
    slot = s;
    notes = n;
    roman = r;
    repaint();
}

void ChordPad::setState (bool sel, bool play, float ph)
{
    if (sel != selected || play != playing || std::abs (ph - playhead) > 0.002f)
    {
        selected = sel;
        playing = play;
        playhead = ph;
        repaint();
    }
}

void ChordPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    cat::CatLookAndFeel::drawGlassPanel (g, r, 6.0f, C::panel);

    if (! slot.isActive)
    {
        g.setColour (C::outline);
        g.setFont (cat::catFont (26.0f));
        g.drawText ("+", r, juce::Justification::centred);
        g.setFont (cat::catFont (11.0f));
        g.setColour (C::textDim.withAlpha (0.5f));
        g.drawText (juce::String (index + 1), r.reduced (8.0f, 6.0f), juce::Justification::bottomLeft);
        return;
    }

    const auto colour = playing ? juce::Colours::white : ChordCatEditor::colourForChord (slot.chord);
    auto area = r.reduced (r.getWidth() * 0.12f, r.getHeight() * 0.08f);

    // Voicing graphic: hourglass of lanes, voiced pitches drawn bright
    const int lanes = 18;
    int lo = 0, hi = 1;
    if (! notes.empty())
    {
        lo = notes.front() - 2;
        hi = notes.back() + 2;
    }
    std::array<bool, lanes> voiced {};
    for (int n : notes)
    {
        const int lane = juce::jlimit (0, lanes - 1, juce::roundToInt ((float) (hi - n) / (float) juce::jmax (1, hi - lo) * (lanes - 1)));
        voiced[(size_t) lane] = true;
    }

    const float laneH = area.getHeight() / (float) lanes;
    for (int i = 0; i < lanes; ++i)
    {
        const float t = (float) i / (float) (lanes - 1);
        float x0 = area.getX(), x1 = area.getRight();
        if (t < 0.5f) x0 += area.getWidth() * 0.32f * (1.0f - 2.0f * t);
        else          x1 -= area.getWidth() * 0.32f * (2.0f * t - 1.0f);

        const bool on = voiced[(size_t) i];
        const float thick = on ? laneH * 0.62f : laneH * 0.38f;
        g.setColour (colour.withAlpha (on ? 0.95f : (selected ? 0.35f : 0.22f)));
        g.fillRoundedRectangle (x0, area.getY() + i * laneH + (laneH - thick) * 0.5f, x1 - x0, thick, thick * 0.5f);
    }

    // Chord name chip
    const auto name = slot.chord.getName (slot.chord.rootNote == 3 || slot.chord.rootNote == 8 || slot.chord.rootNote == 10);
    const float fontH = juce::jlimit (13.0f, 24.0f, r.getHeight() * 0.16f);
    auto font = cat::catFont (fontH, false);
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, name, 0.0f, 0.0f);
    const float textW = juce::jmin (r.getWidth() - 16.0f, ga.getBoundingBox (0, -1, true).getWidth() + 18.0f);
    auto chip = juce::Rectangle<float> (textW, fontH + 12.0f).withCentre (r.getCentre());
    g.setColour (C::background.withAlpha (0.72f));
    g.fillRoundedRectangle (chip, 5.0f);
    g.setColour (C::text);
    g.setFont (font);
    g.drawFittedText (name, chip.toNearestInt().reduced (6, 0), juce::Justification::centred, 1, 0.7f);

    // Labels
    g.setFont (cat::catFont (11.0f, true));
    g.setColour (C::textDim);
    g.drawText (roman, r.reduced (8.0f, 5.0f), juce::Justification::topLeft);
    g.drawText (juce::String (index + 1), r.reduced (8.0f, 5.0f), juce::Justification::bottomLeft);
    if (slot.chord.isBorrowed)
    {
        g.setColour (accent);
        g.drawText ("borrowed", r.reduced (8.0f, 5.0f), juce::Justification::topRight);
    }
    if (slot.chord.rhythmPattern > 0)
    {
        g.setColour (C::textDim);
        g.drawText (getRhythmPatternNames()[slot.chord.rhythmPattern], r.reduced (8.0f, 5.0f), juce::Justification::bottomRight);
    }

    if (playing && playhead >= 0.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        const float x = area.getX() + playhead * area.getWidth();
        g.drawLine (x, area.getY(), x, area.getBottom(), 1.5f);
    }

    if (playing || selected)
    {
        g.setColour (playing ? juce::Colours::white.withAlpha (0.85f) : accent);
        g.drawRoundedRectangle (r, 6.0f, 2.0f);
    }
}

void ChordPad::mouseDown (const juce::MouseEvent& e)
{
    owner.selectSlot (index);
    dragStarted = false;

    if (e.mods.isPopupMenu())
    {
        owner.showPadMenu (index);
        return;
    }

    if (! slot.isActive)
    {
        if (e.getNumberOfClicks() >= 2)
            owner.getProcessor().editSlot (index, [] (ProgressionSlot& s) { s.isActive = true; });
        return;
    }

    auditioning = true;
    owner.getProcessor().auditionSlot (index, true);
}

void ChordPad::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStarted && slot.isActive && ! e.mods.isPopupMenu() && e.getDistanceFromDragStart() > 10)
    {
        dragStarted = true;
        if (auditioning)
        {
            owner.getProcessor().auditionSlot (index, false);
            auditioning = false;
        }
        owner.dragSlotOut (index);
    }
}

void ChordPad::mouseUp (const juce::MouseEvent&)
{
    if (auditioning)
        owner.getProcessor().auditionSlot (index, false);
    auditioning = false;
}

//==============================================================================
// RollView
RollView::RollView (ChordCatEditor& o) : owner (o) {}

void RollView::setContent (const Progression& p, const std::array<std::vector<int>, 16>& v, int pads, int sel, int play)
{
    progression = p;
    voicings = v;
    numPads = juce::jlimit (1, 16, pads);
    selected = sel;
    playingSlot = play;

    int lo = 127, hi = 0;
    for (int i = 0; i < numPads; ++i)
        for (int n : voicings[(size_t) i]) { lo = std::min (lo, n); hi = std::max (hi, n); }
    if (lo > hi) { lo = 48; hi = 84; }
    lo -= 4; hi += 4;
    if (hi - lo < 36) { const int mid = (hi + lo) / 2; lo = mid - 18; hi = mid + 18; }
    lowNote = juce::jmax (0, lo);
    highNote = juce::jmin (127, hi);
    repaint();
}

juce::Rectangle<float> RollView::gridArea() const
{
    return getLocalBounds().toFloat().reduced (6.0f).withTrimmedTop (30.0f).withTrimmedLeft (34.0f);
}

juce::Rectangle<float> RollView::noteRect (int column, int note) const
{
    const auto a = gridArea();
    const float colW = a.getWidth() / (float) numPads;
    const float rowH = a.getHeight() / (float) (highNote - lowNote + 1);
    return { a.getX() + column * colW + 5.0f, a.getY() + (highNote - note) * rowH + 0.75f,
             colW - 10.0f, juce::jmax (3.0f, rowH - 1.5f) };
}

std::pair<int, int> RollView::hitTestNote (juce::Point<float> pos) const
{
    for (int c = 0; c < numPads; ++c)
        for (size_t v = 0; v < voicings[(size_t) c].size(); ++v)
            if (noteRect (c, voicings[(size_t) c][v]).expanded (0.0f, 2.0f).contains (pos))
                return { c, (int) v };
    return { -1, -1 };
}

void RollView::paint (juce::Graphics& g)
{
    cat::CatLookAndFeel::drawGlassPanel (g, getLocalBounds().toFloat().reduced (2.0f), 6.0f, C::panel);
    const auto a = gridArea();
    const float rowH = a.getHeight() / (float) (highNote - lowNote + 1);
    const float colW = a.getWidth() / (float) numPads;

    for (int n = lowNote; n <= highNote; ++n)
    {
        const int pc = n % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const float y = a.getY() + (highNote - n) * rowH;
        g.setColour (black ? juce::Colour (0xff111318) : juce::Colour (0xff181b21));
        g.fillRect (a.getX(), y, a.getWidth(), rowH);
        if (pc == 0)
        {
            g.setColour (C::outline);
            g.fillRect (a.getX(), y + rowH - 1.0f, a.getWidth(), 1.0f);
            g.setColour (C::textDim);
            g.setFont (cat::catFont (10.0f));
            g.drawText (noteText (n), juce::Rectangle<float> (a.getX() - 34.0f, y + rowH - 12.0f, 32.0f, 12.0f), juce::Justification::centredRight);
        }
    }

    for (int c = 0; c < numPads; ++c)
    {
        const auto& slot = progression[(size_t) c];
        const auto colour = ChordCatEditor::colourForChord (slot.chord);
        auto tab = juce::Rectangle<float> (a.getX() + c * colW + 2.0f, 8.0f, colW - 4.0f, 22.0f);

        g.setColour (slot.isActive ? colour.withAlpha (c == selected ? 0.95f : 0.65f) : C::panelLight);
        g.fillRoundedRectangle (tab, 4.0f);
        if (c == selected)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (tab, 4.0f, 1.5f);
        }
        g.setColour (C::text);
        g.setFont (cat::catFont (12.0f, true));
        g.drawFittedText (juce::String (c + 1) + "  " + (slot.isActive ? slot.chord.getName() : juce::String ("-")),
                          tab.toNearestInt().reduced (5, 0), juce::Justification::centredLeft, 1, 0.6f);

        g.setColour (C::outline.withAlpha (0.5f));
        g.fillRect (a.getX() + c * colW, a.getY(), 1.0f, a.getHeight());

        if (c == playingSlot)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (a.getX() + c * colW, a.getY(), colW, a.getHeight());
        }

        if (! slot.isActive) continue;

        const auto& v = voicings[(size_t) c];
        for (size_t i = 0; i < v.size(); ++i)
        {
            const bool dragging = c == dragColumn && (int) i == dragVoice;
            auto rect = noteRect (c, juce::jlimit (lowNote, highNote, v[i] + (dragging ? dragDelta : 0)));
            g.setColour ((c == playingSlot ? juce::Colours::white : colour).withAlpha (dragging ? 1.0f : 0.88f));
            g.fillRoundedRectangle (rect, 3.0f);
            if (slot.chord.customOffsets[i < 6 ? i : 5] != 0 && i < 6)
            {
                g.setColour (accent);
                g.fillEllipse (rect.getRight() - 6.0f, rect.getCentreY() - 2.0f, 4.0f, 4.0f);
            }
        }
    }

    g.setColour (C::textDim);
    g.setFont (cat::catFont (11.0f));
    g.drawText ("Custom editor: drag a voice up/down (+-24 st), double-click to reset",
                getLocalBounds().reduced (10, 4), juce::Justification::bottomRight);
}

void RollView::mouseDown (const juce::MouseEvent& e)
{
    const auto a = gridArea();
    const auto pos = e.position;
    const int column = juce::jlimit (0, numPads - 1, (int) ((pos.x - a.getX()) / (a.getWidth() / (float) numPads)));

    if (pos.y < a.getY())
    {
        owner.selectSlot (column);
        if (e.mods.isPopupMenu()) owner.showPadMenu (column);
        return;
    }

    const auto [c, v] = hitTestNote (pos);
    if (c >= 0)
    {
        owner.selectSlot (c);
        dragColumn = c;
        dragVoice = v;
        dragDelta = 0;
        dragStartY = pos.y;
    }
    else if (pos.x >= a.getX())
    {
        owner.selectSlot (column);
    }
}

void RollView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragColumn < 0) return;
    const float rowH = gridArea().getHeight() / (float) (highNote - lowNote + 1);
    const int current = dragVoice < 6 ? progression[(size_t) dragColumn].chord.customOffsets[(size_t) dragVoice] : 0;
    dragDelta = juce::jlimit (-24 - current, 24 - current, juce::roundToInt ((dragStartY - e.position.y) / rowH));
    repaint();
}

void RollView::mouseUp (const juce::MouseEvent&)
{
    if (dragColumn >= 0 && dragDelta != 0 && dragVoice < 6)
    {
        const int voice = dragVoice, delta = dragDelta;
        owner.getProcessor().editSlot (dragColumn, [voice, delta] (ProgressionSlot& s)
        {
            auto& off = s.chord.customOffsets[(size_t) voice];
            off = juce::jlimit (-24, 24, off + delta);
        });
    }
    dragColumn = dragVoice = -1;
    dragDelta = 0;
    repaint();
}

void RollView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto [c, v] = hitTestNote (e.position);
    const auto a = gridArea();
    const int column = c >= 0 ? c : juce::jlimit (0, numPads - 1, (int) ((e.position.x - a.getX()) / (a.getWidth() / (float) numPads)));
    owner.getProcessor().editSlot (column, [v] (ProgressionSlot& s)
    {
        if (v >= 0 && v < 6) s.chord.customOffsets[(size_t) v] = 0;
        else s.chord.customOffsets = {};
    });
}

//==============================================================================
// Circle of fifths
void CircleOfFifths::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float size = juce::jmin (r.getWidth(), r.getHeight()) - 4.0f;
    auto circle = juce::Rectangle<float> (size, size).withCentre (r.getCentre());
    const auto c = circle.getCentre();
    const float outer = size * 0.5f, inner = size * 0.27f;

    for (int i = 0; i < 12; ++i)
    {
        const int pc = (i * 7) % 12;
        const float a0 = juce::MathConstants<float>::twoPi * ((float) i - 0.5f) / 12.0f;
        const float a1 = a0 + juce::MathConstants<float>::twoPi / 12.0f;

        juce::Path seg;
        seg.addPieSegment (circle, a0 + 0.02f, a1 - 0.02f, inner / outer);
        const bool sel = pc == selectedRoot;
        g.setColour (sel ? accent : (pc == key ? C::raised.brighter (0.25f) : C::raised));
        g.fillPath (seg);
        if (pc == key)
        {
            g.setColour (accent.withAlpha (0.8f));
            g.strokePath (seg, juce::PathStrokeType (1.2f));
        }

        const float mid = (a0 + a1) * 0.5f;
        const float rad = (outer + inner) * 0.5f;
        const auto pt = c.getPointOnCircumference (rad, mid);
        g.setColour (sel ? C::background : C::text);
        g.setFont (cat::catFont (juce::jmax (10.0f, size * 0.07f), sel));
        g.drawText (pitchClassName (pc, pc == 3 || pc == 8 || pc == 10), juce::Rectangle<float> (40.0f, 18.0f).withCentre (pt),
                    juce::Justification::centred);
    }

    g.setColour (C::textDim);
    g.setFont (cat::catFont (11.0f, true));
    g.drawText ("ROOT", juce::Rectangle<float> (inner * 2.0f, 20.0f).withCentre (c), juce::Justification::centred);
}

void CircleOfFifths::mouseDown (const juce::MouseEvent& e)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const auto d = e.position - c;
    float angle = std::atan2 (d.x, -d.y); // 0 at 12 o'clock, clockwise
    if (angle < 0) angle += juce::MathConstants<float>::twoPi;
    const int i = ((int) std::round (angle / (juce::MathConstants<float>::twoPi / 12.0f))) % 12;
    const int pc = (i * 7) % 12;
    selectedRoot = pc;
    repaint();
    if (onRootSelected) onRootSelected (pc);
}

//==============================================================================
// Edit panel
EditPanel::EditPanel (ChordCatEditor& o, ChordCatProcessor& p)
    : owner (o), processor (p),
      voiceLeading (p.apvts, "voiceLeading", "Voice Leading", &p.midiLearn),
      transpose (p.apvts, "transpose", "Transpose", &p.midiLearn),
      velocity (p.apvts, "velocity", "Velocity", &p.midiLearn),
      legato (p.apvts, "legato", "Legato", &p.midiLearn),
      bassSplit (p.apvts, "bassSplit", "Bass Split", &p.midiLearn),
      triggerMode (p.apvts, "triggerMode", "Keys", &p.midiLearn, 40)
{
    addAndMakeVisible (circle);
    circle.onRootSelected = [this] (int pc) { edit ([pc] (ProgressionSlot& s) { s.chord.rootNote = pc; s.isActive = true; }); };

    // Chord types grouped by extension
    const auto& types = getChordTypes();
    const char* groups[] = { "Triads, 6ths & sus", "Sevenths", "Ninths", "Elevenths", "Thirteenths & altered" };
    for (int cx = 0; cx < 5; ++cx)
    {
        typeBox.addSectionHeading (groups[cx]);
        for (size_t i = 0; i < types.size(); ++i)
            if (types[i].complexity == cx)
                typeBox.addItem (types[i].symbol.isEmpty() ? juce::String ("maj") : types[i].symbol, (int) i + 1);
    }
    typeBox.onChange = [this]
    {
        const int idx = typeBox.getSelectedId() - 1;
        if (! updating && idx >= 0)
            edit ([idx] (ProgressionSlot& s) { s.chord.chordType = getChordTypes()[(size_t) idx].id; s.isActive = true; });
    };
    addAndMakeVisible (typeBox);

    voicingBox.addItemList (getVoicingModelNames(), 1);
    voicingBox.onChange = [this]
    {
        const int v = voicingBox.getSelectedId() - 1;
        if (! updating && v >= 0) edit ([v] (ProgressionSlot& s) { s.chord.voicingModel = v; });
    };
    addAndMakeVisible (voicingBox);

    tensionBox.setEditableText (true);
    tensionBox.setTextWhenNothingSelected ("no tension");
    tensionBox.addItemList ({ "9", "11", "#11", "13", "b9", "#9", "b13", "6", "add2", "9 13", "b9 b13", "9 #11" }, 1);
    tensionBox.onChange = [this]
    {
        if (updating) return;
        const auto t = tensionBox.getText().trim();
        edit ([t] (ProgressionSlot& s) { s.chord.tension = t; });
    };
    addAndMakeVisible (tensionBox);

    rhythmBox.addItemList (getRhythmPatternNames(), 1);
    rhythmBox.onChange = [this]
    {
        const int r = rhythmBox.getSelectedId() - 1;
        if (! updating && r >= 0) edit ([r] (ProgressionSlot& s) { s.chord.rhythmPattern = r; });
    };
    addAndMakeVisible (rhythmBox);

    const char* invNames[] = { "Root", "1st", "2nd", "3rd" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = inversionButtons[(size_t) i];
        b.setButtonText (invNames[i]);
        b.setRadioGroupId (4711);
        b.setClickingTogglesState (true);
        b.onClick = [this, i] { if (! updating && inversionButtons[(size_t) i].getToggleState()) edit ([i] (ProgressionSlot& s) { s.chord.inversion = i; }); };
        addAndMakeVisible (b);
    }

    octaveStepper = std::make_unique<cat::Stepper> ("Octave", 2, 6,
        [this] { return processor.getProgression()[(size_t) owner.getSelectedSlot()].chord.octave; },
        [this] (int v) { edit ([v] (ProgressionSlot& s) { s.chord.octave = v; }); });
    addAndMakeVisible (*octaveStepper);

    strumOverride.setSliderStyle (juce::Slider::LinearBar);
    strumOverride.setRange (-1.0, 1000.0, 0.5);
    strumOverride.setSkewFactor (0.35);
    strumOverride.textFromValueFunction = [] (double v) { return v < 0.0 ? juce::String ("Strum: global") : "Strum: " + juce::String (v, 1) + " ms"; };
    strumOverride.onValueChange = [this]
    {
        if (updating) return;
        const float v = (float) strumOverride.getValue();
        edit ([v] (ProgressionSlot& s) { s.chord.strumTimeMs = v; });
    };
    strumOverride.setColour (juce::Slider::trackColourId, C::blue);
    addAndMakeVisible (strumOverride);

    activeButton.setClickingTogglesState (true);
    activeButton.onClick = [this] { if (! updating) { const bool on = activeButton.getToggleState(); edit ([on] (ProgressionSlot& s) { s.isActive = on; }); } };
    addAndMakeVisible (activeButton);

    nameLabel.setFont (cat::catFont (22.0f, true));
    nameLabel.setColour (juce::Label::textColourId, C::text);
    addAndMakeVisible (nameLabel);
    romanLabel.setFont (cat::catFont (13.0f, true));
    romanLabel.setColour (juce::Label::textColourId, accent);
    addAndMakeVisible (romanLabel);

    for (auto* c : std::initializer_list<juce::Component*> { &voiceLeading, &transpose, &velocity, &legato, &bassSplit, &triggerMode })
        addAndMakeVisible (c);

    auto noteFmt = [] (int v) { return noteText (v); };
    bassChannel = cat::Stepper::forParameter (p.apvts, "bassChannel", "Bass Ch");
    inChannel = cat::Stepper::forParameter (p.apvts, "inChannel", "In Ch", [] (int v) { return v == 0 ? juce::String ("Omni") : juce::String (v); });
    outChannel = cat::Stepper::forParameter (p.apvts, "outChannel", "Out Ch");
    triggerLow = cat::Stepper::forParameter (p.apvts, "triggerLow", "Trig Lo", noteFmt);
    triggerHigh = cat::Stepper::forParameter (p.apvts, "triggerHigh", "Trig Hi", noteFmt);
    for (auto* s : { bassChannel.get(), inChannel.get(), outChannel.get(), triggerLow.get(), triggerHigh.get() })
        addAndMakeVisible (s);
}

void EditPanel::edit (const std::function<void (ProgressionSlot&)>& fn)
{
    processor.editSlot (owner.getSelectedSlot(), fn);
}

void EditPanel::refresh()
{
    const int slotIndex = owner.getSelectedSlot();
    const auto slot = processor.getProgression()[(size_t) slotIndex];
    const auto g = processor.getGlobalState();

    updating = true;
    circle.setRoot (slot.chord.rootNote, g.keyRoot);
    typeBox.setSelectedId (juce::jmax (0, findChordType (slot.chord.chordType)) + 1, juce::dontSendNotification);
    voicingBox.setSelectedId (slot.chord.voicingModel + 1, juce::dontSendNotification);
    tensionBox.setText (slot.chord.tension, juce::dontSendNotification);
    rhythmBox.setSelectedId (slot.chord.rhythmPattern + 1, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
        inversionButtons[(size_t) i].setToggleState (slot.chord.inversion == i, juce::dontSendNotification);
    strumOverride.setValue (slot.chord.strumTimeMs, juce::dontSendNotification);
    activeButton.setToggleState (slot.isActive, juce::dontSendNotification);
    nameLabel.setText ("Pad " + juce::String (slotIndex + 1) + ":  " + (slot.isActive ? slot.chord.getName() : juce::String ("empty")),
                       juce::dontSendNotification);
    romanLabel.setText (slot.isActive ? processor.getRomanNumeral (slotIndex) + (slot.chord.isBorrowed ? "   borrowed / chromatic" : "")
                                      : juce::String(), juce::dontSendNotification);
    octaveStepper->repaint();
    updating = false;
}

void EditPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (8);
    const int third = r.getWidth() / 3;
    drawSectionTitle (g, r.withWidth (200).withHeight (14), "Chord");
    drawSectionTitle (g, r.withTrimmedLeft (third * 2 - 10).withHeight (14), "Voicing & Trigger");
}

void EditPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (16);

    const int circleSize = juce::jmin (r.getHeight(), 200);
    circle.setBounds (r.removeFromLeft (circleSize).withSizeKeepingCentre (circleSize, circleSize));
    r.removeFromLeft (14);

    auto right = r.removeFromRight (juce::jmin (420, r.getWidth() / 2));
    auto mid = r.reduced (4, 0);

    auto row = [&mid] (int h) { auto x = mid.removeFromTop (h); mid.removeFromTop (6); return x; };
    {
        auto top = row (28);
        nameLabel.setBounds (top.removeFromLeft (top.getWidth() - 90));
        activeButton.setBounds (top.withSizeKeepingCentre (80, 24));
    }
    romanLabel.setBounds (row (18));
    {
        auto line = row (26);
        typeBox.setBounds (line.removeFromLeft (line.getWidth() / 2 - 3));
        line.removeFromLeft (6);
        voicingBox.setBounds (line);
    }
    {
        auto line = row (26);
        const int w = line.getWidth() / 4;
        for (auto& b : inversionButtons) b.setBounds (line.removeFromLeft (w).reduced (2, 0));
    }
    {
        auto line = row (26);
        octaveStepper->setBounds (line.removeFromLeft (line.getWidth() / 2 - 3));
        line.removeFromLeft (6);
        tensionBox.setBounds (line);
    }
    {
        auto line = row (26);
        rhythmBox.setBounds (line.removeFromLeft (line.getWidth() / 2 - 3));
        line.removeFromLeft (6);
        strumOverride.setBounds (line);
    }

    // Right: global voicing / trigger settings
    auto knobs = right.removeFromTop (juce::jmin (96, right.getHeight() / 2));
    const int kw = knobs.getWidth() / 3;
    voiceLeading.setBounds (knobs.removeFromLeft (kw));
    transpose.setBounds (knobs.removeFromLeft (kw));
    velocity.setBounds (knobs);

    right.removeFromTop (4);
    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    grid.templateColumns = { Track (juce::Grid::Fr (1)), Track (juce::Grid::Fr (1)) };
    grid.templateRows = { Track (juce::Grid::Fr (1)), Track (juce::Grid::Fr (1)), Track (juce::Grid::Fr (1)), Track (juce::Grid::Fr (1)) };
    grid.rowGap = juce::Grid::Px (5);
    grid.columnGap = juce::Grid::Px (6);
    grid.items = { juce::GridItem (legato), juce::GridItem (bassSplit),
                   juce::GridItem (*inChannel), juce::GridItem (*outChannel),
                   juce::GridItem (*triggerLow), juce::GridItem (*triggerHigh),
                   juce::GridItem (triggerMode), juce::GridItem (*bassChannel) };
    grid.performLayout (right);
}

//==============================================================================
// Strum panel
StrumDisplay::StrumDisplay (ChordCatProcessor& p) : processor (p)
{
    startTimerHz (30);
}

void StrumDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff101116));
    g.fillRoundedRectangle (bounds, 6.0f);

    // Pitch range: the notes of the chord being strummed (with a little margin)
    int lo = 127, hi = 0;
    for (int n = 0; n < 128; ++n)
        if (processor.uiChordNotes[(size_t) n].load()) { lo = juce::jmin (lo, n); hi = juce::jmax (hi, n); }
    const double now = processor.uiNowSeconds.load();
    const int w = processor.uiStrumWrite.load (std::memory_order_acquire);
    for (int k = 0; k < 256; ++k)
    {
        const auto& e = processor.uiStrumEvents[(size_t) ((w - 1 - k) & 255)];
        if (e.time < 0.0 || now - e.time > 2.5) continue;
        lo = juce::jmin (lo, e.note); hi = juce::jmax (hi, e.note);
    }
    auto area = bounds.reduced (8.0f).withTrimmedLeft (34.0f).withTrimmedTop (14.0f);
    g.setColour (C::textDim);
    g.setFont (cat::catFont (10.0f, true));
    g.drawText ("LIVE STRUM", bounds.reduced (8.0f, 3.0f).removeFromTop (12.0f), juce::Justification::centredLeft);
    if (lo > hi)
    {
        g.drawText ("Play a chord - every strummed note appears here", area, juce::Justification::centred);
        return;
    }
    lo -= 1; hi += 1;
    const float rowH = area.getHeight() / (float) (hi - lo + 1);

    // Pitch rows: chord notes highlighted, stack notes in purple
    for (int n = lo; n <= hi; ++n)
    {
        const float y = area.getBottom() - rowH * (float) (n - lo + 1);
        const bool inChord = processor.uiChordNotes[(size_t) juce::jlimit (0, 127, n)].load();
        const bool stack = n > processor.uiBaseTop.load();
        g.setColour (inChord ? (stack ? C::purple : C::accent).withAlpha (0.12f) : juce::Colour (0xff14151a));
        g.fillRect (area.getX(), y, area.getWidth(), rowH - 1.0f);
        if (inChord && rowH > 7.0f)
        {
            g.setColour (stack ? C::purple : C::accent);
            g.setFont (cat::catFont (juce::jmin (10.0f, rowH), true));
            g.drawText (juce::MidiMessage::getMidiNoteName (n, true, true, 3), juce::Rectangle<float> (bounds.getX() + 4.0f, y, 32.0f, rowH),
                        juce::Justification::centredLeft);
        }
    }

    // Time grid: newest on the right
    g.setColour (C::outline.withAlpha (0.6f));
    for (int t = 0; t <= 5; ++t)
        g.drawVerticalLine (juce::roundToInt (area.getRight() - area.getWidth() * (float) t / 5.0f), area.getY(), area.getBottom());

    for (int k = 0; k < 256; ++k)
    {
        const auto& e = processor.uiStrumEvents[(size_t) ((w - 1 - k) & 255)];
        const double age = now - e.time;
        if (e.time < 0.0 || age > 2.5 || age < -0.6) continue;
        const float x = area.getRight() - (float) (juce::jmax (0.0, age) / 2.5) * area.getWidth();
        const float y = area.getBottom() - rowH * (float) (e.note - lo + 1) + rowH * 0.5f;
        const float size = 3.0f + 5.0f * e.velocity / 127.0f;
        const auto col = e.stack ? C::purple : C::accent;
        g.setColour (col.withAlpha (age < 0.0 ? 0.35f : 0.35f + 0.65f * (float) (1.0 - age / 2.5)));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ x, y }));
    }
}

StrumPanel::StrumPanel (ChordCatProcessor& p)
    : processor (p),
      sync (p.apvts, "strumSync", "Sync", &p.midiLearn, 34),
      direction (p.apvts, "strumDir", "Direction", &p.midiLearn, 62),
      shape (p.apvts, "strumShapeType", "Shape", &p.midiLearn, 42),
      burst (p.apvts, "strumBurst", "Burst", &p.midiLearn, 40),
      time (p.apvts, "strumMs", "Time", &p.midiLearn, 36),
      curve (p.apvts, "strumShape", "Curve", &p.midiLearn, 40),
      tilt (p.apvts, "strumCurve", "Tilt", &p.midiLearn, 30),
      human (p.apvts, "humanTime", "Human", &p.midiLearn, 46),
      velRnd (p.apvts, "humanVel", "Vel Rnd", &p.midiLearn, 50),
      mute (p.apvts, "strumMute", "Mute", &p.midiLearn, 36),
      roundTrip (p.apvts, "strumRoundTrip", "Round Trip", &p.midiLearn),
      release (p.apvts, "strumRelease", "On Release", &p.midiLearn),
      display (p)
{
    const auto& presets = getStrumPresets();
    preset = std::make_unique<cat::Stepper> ("Strum", -1, (int) presets.size() - 1,
        [this] { return processor.currentStrumPreset.load(); },
        [this] (int v) { processor.applyStrumPreset (juce::jmax (0, v)); },
        [&presets] (int v) { return v < 0 ? juce::String ("Custom") : juce::String (v + 1) + ". " + presets[(size_t) v].name; });
    stack = cat::Stepper::forParameter (p.apvts, "strumStack", "Stack", [] (int v) { return v == 0 ? juce::String ("Off") : "+" + juce::String (v); });
    target = cat::Stepper::forParameter (p.apvts, "strumTarget", "Target", [] (int v) { return v == 0 ? juce::String ("Off") : juce::String (v); });
    window = cat::Stepper::forParameter (p.apvts, "strumWindow", "Classic", [] (int v) { return v < 2 ? juce::String ("Off") : juce::String (v); });
    repeats = cat::Stepper::forParameter (p.apvts, "strumRepeats", "Strokes");
    repeats->setCaptionWidth (54);
    dice.onClick = [this] { processor.randomStrum(); };

    time.getSlider().setTooltip ("Time from the first to the last note of one stroke (5 ms guitar .. 1 s harp)");
    direction.getCombo().setTooltip ("Which note goes first. Up / Down alternate on every chord; Guitar strokes down on the beat and up off the beat");
    shape.getCombo().setTooltip ("Timing curve of the stroke: accelerating, decelerating, elastic, bouncing, stepped...");
    curve.getSlider().setTooltip ("Extra bend of the timing: + starts slow, - starts fast");
    tilt.getSlider().setTooltip ("Velocity across the stroke: + louder towards the last note");
    stack->setTooltip ("Octave stack: the chord again one, two or three octaves higher");
    target->setTooltip ("Target velocity: the last note of the stroke reaches this velocity (Off = use Tilt)");
    window->setTooltip ("Classic: only this many neighbouring notes per stroke, sliding one step every stroke");
    repeats->setTooltip ("Strokes per chord");
    roundTrip.setTooltip ("Every second stroke turns around: up-down-up... (ornaments, rolls)");
    burst.getCombo().setTooltip ("Re-strum the held chord on every note value, alternating strokes like a strumming hand");
    mute.getSlider().setTooltip ("Burst: share of strokes played as short muted chops");
    release.setTooltip ("Let go of the chord note by note, top to bottom");
    dice.setTooltip ("Random strum (musically bounded)");

    for (auto* c : std::initializer_list<juce::Component*> { preset.get(), stack.get(), target.get(), window.get(), repeats.get(), &dice,
             &sync, &direction, &shape, &burst, &time, &curve, &tilt, &human, &velRnd, &mute, &roundTrip, &release, &display })
        addAndMakeVisible (c);
}

void StrumPanel::paint (juce::Graphics& g)
{
    drawSectionTitle (g, getLocalBounds().reduced (8).withHeight (14), "Strum  -  rolls, stacks, bursts");
}

void StrumPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (18);
    auto right = r.removeFromRight (juce::jmax (220, r.getWidth() * 2 / 5));
    r.removeFromRight (10);
    display.setBounds (right);

    auto item = [] (juce::Component& c, float w) { return juce::FlexItem (c).withWidth (w).withHeight (28.0f).withMargin ({ 0, 3, 0, 3 }); };
    auto row = [&] (std::initializer_list<juce::FlexItem> items)
    {
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        for (auto& it : items) fb.items.add (it);
        fb.performLayout (r.removeFromTop (30));
        r.removeFromTop (6);
    };
    row ({ item (*preset, 260).withFlex (1.0f), item (dice, 64) });
    row ({ item (time, 160).withFlex (1.0f), item (sync, 116), item (direction, 190) });
    row ({ item (shape, 170), item (curve, 120).withFlex (1.0f), item (tilt, 110).withFlex (1.0f), item (*stack, 112) });
    row ({ item (*target, 116), item (human, 120).withFlex (1.0f), item (velRnd, 120).withFlex (1.0f), item (*window, 126) });
    row ({ item (*repeats, 116), item (roundTrip, 96), item (burst, 120), item (mute, 100).withFlex (1.0f), item (release, 96) });
}

//==============================================================================
// Perform panel
PerformPanel::PerformPanel (ChordCatEditor& o, ChordCatProcessor& p)
    : owner (o), processor (p),
      strum (p.apvts, "strumMs", "Strum", &p.midiLearn),
      strumShape (p.apvts, "strumShape", "Strum Shape", &p.midiLearn, C::blue),
      strumCurve (p.apvts, "strumCurve", "Strum Vel Curve", &p.midiLearn, C::blue),
      humanTime (p.apvts, "humanTime", "Humanize Time", &p.midiLearn),
      humanVel (p.apvts, "humanVel", "Humanize Vel", &p.midiLearn),
      swing (p.apvts, "swing", "Swing", &p.midiLearn, C::blue),
      velSpread (p.apvts, "velSpread", "Vel Spread", &p.midiLearn),
      octSpread (p.apvts, "octSpread", "Oct Spread", &p.midiLearn, C::blue),
      strumDir (p.apvts, "strumDir", "Direction", &p.midiLearn, 64),
      strumSync (p.apvts, "strumSync", "Strum Sync", &p.midiLearn, 74),
      groove (p.apvts, "groove", "Groove", &p.midiLearn, 64),
      followGroove (p.apvts, "followGroove", "Follow Drum", &p.midiLearn)
{
    for (auto* c : std::initializer_list<juce::Component*> { &strum, &strumShape, &strumCurve, &humanTime, &humanVel, &swing, &velSpread,
                                                             &octSpread, &strumDir, &strumSync, &groove, &recordButton, &dragCaptureButton, &followGroove })
        addAndMakeVisible (c);

    strum.getSlider().setTooltip ("Time from the first to the last note of the chord (up to 1 s), unless Strum Sync is set");
    strumShape.getSlider().setTooltip ("Timing curve: + starts slow and speeds up, - starts fast and slows down");
    strumCurve.getSlider().setTooltip ("Velocity across the strum: + gets louder towards the last note, - softer");

    followGroove.setTooltip ("Take swing and humanize timing from Drum-Cat (Cat Link)");
    recordButton.setClickingTogglesState (true);
    recordButton.setColour (juce::TextButton::buttonOnColourId, C::red);
    recordButton.setTooltip ("MIDI Capture: records what you play as chords, quantised to 1/16");
    recordButton.onClick = [this] { processor.setCapturing (recordButton.getToggleState()); };

    dragCaptureButton.setTooltip ("Drag the captured performance onto a Cubase MIDI track");
    dragCaptureButton.onDragOut = [this] { owner.dragSequenceOut (processor.getCaptureSequence(), "Chord-Cat Capture"); };
    startTimerHz (5);
}

void PerformPanel::timerCallback()
{
    recordButton.setToggleState (processor.isCapturing(), juce::dontSendNotification);
    recordButton.setButtonText (processor.isCapturing() ? "Recording..." : "Capture");
    dragCaptureButton.setEnabled (processor.hasCapture());
}

void PerformPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (8);
    drawSectionTitle (g, r.withHeight (14), "Strum  /  Humanize  /  Groove");
    drawSectionTitle (g, r.withTrimmedLeft (r.getWidth() - 200).withHeight (14), "MIDI Capture");
}

void PerformPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (18);
    auto capture = r.removeFromRight (200);
    r.removeFromRight (10);

    auto combos = r.removeFromBottom (28);
    const int comboW = (combos.getWidth() - 20) / 3;
    strumDir.setBounds (combos.removeFromLeft (comboW));
    combos.removeFromLeft (10);
    strumSync.setBounds (combos.removeFromLeft (comboW));
    combos.removeFromLeft (10);
    groove.setBounds (combos);
    r.removeFromBottom (8);

    juce::FlexBox fb;
    fb.flexWrap = juce::FlexBox::Wrap::noWrap;
    fb.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    fb.alignItems = juce::FlexBox::AlignItems::center;
    for (auto* k : { &strum, &strumShape, &strumCurve, &humanTime, &humanVel, &swing, &velSpread, &octSpread })
        fb.items.add (juce::FlexItem (*k).withFlex (1.0f).withMaxWidth (110.0f).withMinWidth (70.0f).withHeight (juce::jmin (120.0f, (float) r.getHeight())));
    fb.performLayout (r);

    recordButton.setBounds (capture.removeFromTop (36).reduced (0, 2));
    capture.removeFromTop (8);
    dragCaptureButton.setBounds (capture.removeFromTop (36).reduced (0, 2));
    capture.removeFromTop (8);
    followGroove.setBounds (capture.removeFromTop (32).reduced (0, 2));
}

//==============================================================================
// Generate panel
GeneratePanel::GeneratePanel (ChordCatEditor& o, ChordCatProcessor& p)
    : owner (o), processor (p),
      mood (p.apvts, "genMood", "Mood", &p.midiLearn, 80),
      genre (p.apvts, "genGenre", "Genre", &p.midiLearn, 80),
      complexity (p.apvts, "genComplexity", "Complexity", &p.midiLearn, 80),
      keyRoot (p.apvts, "keyRoot", "Key", &p.midiLearn, 50),
      scale (p.apvts, "scale", "Scale", &p.midiLearn, 50),
      scaleLock (p.apvts, "scaleLock", "Scale Lock", &p.midiLearn),
      chordTrackSync (p.apvts, "chordTrackSync", "Chord Track Sync", &p.midiLearn)
{
    for (auto* c : std::initializer_list<juce::Component*> { &mood, &genre, &complexity, &keyRoot, &scale, &scaleLock,
                                                             &chordTrackSync, &generateButton, &moodScaleButton, &importButton })
        addAndMakeVisible (c);

    generateButton.setColour (juce::TextButton::buttonColourId, accent.withAlpha (0.16f));
    generateButton.setColour (juce::TextButton::textColourOffId, C::text);
    generateButton.onClick = [this] { generate(); };

    moodScaleButton.setClickingTogglesState (true);
    moodScaleButton.setToggleState (true, juce::dontSendNotification);
    moodScaleButton.setTooltip ("Let the mood choose the scale (Dark/Sad = minor, Tension = harmonic minor...)");

    scaleLock.setTooltip ("Quantise programmed chords to diatonic chords of the key; chromatic chords are flagged");
    chordTrackSync.setTooltip ("Follow incoming chords (route Cubase's Chord Track / chord MIDI into Chord-Cat): "
                               "each new chord is written to the next pad and re-voiced");
    importButton.setTooltip ("Import chords from a MIDI file (e.g. Cubase Chord Track exported via 'Chords to MIDI')");
    importButton.onClick = [this] { chooseImportFile(); };

    chordCount = std::make_unique<cat::Stepper> ("Chords", 4, 16, [this] { return numChords; }, [this] (int v) { numChords = v; });
    addAndMakeVisible (*chordCount);

    for (int i = 0; i < 9; ++i)
    {
        auto& b = historyButtons[(size_t) i];
        b.setButtonText (juce::String (i + 1));
        b.setTooltip ("Recall generated progression #" + juce::String (i + 1));
        b.onClick = [this, i] { processor.recallHistory (i); };
        addAndMakeVisible (b);
    }

    for (int i = 0; i < 12; ++i)
    {
        auto& b = customScaleButtons[(size_t) i];
        b.setButtonText (pitchClassName (i));
        b.setClickingTogglesState (true);
        b.onClick = [this, i]
        {
            auto mask = processor.customScaleMask.load();
            mask ^= (1 << i);
            processor.customScaleMask = mask | 1;
        };
        addChildComponent (b);
    }
    startTimerHz (5);
}

void GeneratePanel::generate()
{
    GeneratorSettings s;
    auto val = [this] (const char* id) { return juce::roundToInt (processor.apvts.getRawParameterValue (id)->load()); };
    s.mood = val ("genMood");
    s.genre = val ("genGenre");
    s.complexity = val ("genComplexity");
    s.keyRoot = val ("keyRoot");
    s.scaleIndex = val ("scale");
    s.customMask = (uint16_t) processor.customScaleMask.load();
    s.moodSetsScale = moodScaleButton.getToggleState();
    s.numChords = numChords;
    processor.generate (s);
}

void GeneratePanel::chooseImportFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Import chords from MIDI", juce::File(), "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile() && ! processor.importMidiFile (f))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Chord-Cat", "No chords found in " + f.getFileName());
    });
}

void GeneratePanel::timerCallback()
{
    for (int i = 0; i < 9; ++i)
        historyButtons[(size_t) i].setEnabled (i < processor.getNumHistory());

    const bool custom = juce::roundToInt (processor.apvts.getRawParameterValue ("scale")->load()) == getCustomScaleIndex();
    const int mask = processor.customScaleMask.load();
    for (int i = 0; i < 12; ++i)
    {
        customScaleButtons[(size_t) i].setVisible (custom);
        customScaleButtons[(size_t) i].setToggleState ((mask >> i) & 1, juce::dontSendNotification);
    }
}

void GeneratePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (8);
    drawSectionTitle (g, r.withHeight (14), "Progression generator (Markov)");
    drawSectionTitle (g, r.withTrimmedLeft (r.getWidth() / 2 + 10).withHeight (14), "Harmony");
    drawSectionTitle (g, r.withTrimmedTop (r.getHeight() - 46).withHeight (14), "History");
}

void GeneratePanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (18);
    auto historyRow = r.removeFromBottom (26);
    r.removeFromBottom (18);

    auto left = r.removeFromLeft (r.getWidth() / 2);
    auto right = r.withTrimmedLeft (18);

    auto row = [] (juce::Rectangle<int>& area, int h) { auto x = area.removeFromTop (h); area.removeFromTop (6); return x; };
    mood.setBounds (row (left, 26));
    genre.setBounds (row (left, 26));
    complexity.setBounds (row (left, 26));
    {
        auto line = row (left, 30);
        chordCount->setBounds (line.removeFromLeft (150));
        line.removeFromLeft (8);
        moodScaleButton.setBounds (line.removeFromLeft (130));
        line.removeFromLeft (8);
        generateButton.setBounds (line);
    }

    {
        auto line = row (right, 26);
        keyRoot.setBounds (line.removeFromLeft (130));
        line.removeFromLeft (8);
        scale.setBounds (line);
    }
    {
        auto line = row (right, 26);
        scaleLock.setBounds (line.removeFromLeft (line.getWidth() / 2 - 4));
        line.removeFromLeft (8);
        chordTrackSync.setBounds (line);
    }
    importButton.setBounds (row (right, 26));
    {
        auto line = row (right, 24);
        const int w = line.getWidth() / 12;
        for (auto& b : customScaleButtons) b.setBounds (line.removeFromLeft (w).reduced (1, 0));
    }

    const int hw = juce::jmin (40, historyRow.getWidth() / 9);
    for (auto& b : historyButtons) b.setBounds (historyRow.removeFromLeft (hw).reduced (2, 0));
}

//==============================================================================
// Preset browser
PresetBrowser::PresetBrowser (ChordCatEditor& o, ChordCatProcessor& p) : owner (o), processor (p)
{
    search.setTextToShowWhenEmpty ("Search presets", C::textDim);
    search.onTextChange = [this] { refreshList(); };
    addAndMakeVisible (search);

    juce::StringArray tags;
    tags.addArray (getGenreNames());
    tags.addArray (getMoodNames());
    tags.addArray (getComplexityNames());
    for (const auto& t : tags)
    {
        auto* b = tagButtons.add (new juce::TextButton (t));
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonColourId, C::panelLight);
        b->onClick = [this] { refreshList(); };
        addAndMakeVisible (b);
    }

    list.setModel (this);
    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    saveButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save Chord-Cat preset", ChordCatProcessor::getUserPresetFolder(), "*.ccpreset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("ccpreset")) f = f.withFileExtension ("ccpreset");
            processor.saveUserPreset (f);
            refreshList();
        });
    };
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load Chord-Cat preset", ChordCatProcessor::getUserPresetFolder(), "*.ccpreset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) processor.loadUserPreset (fc.getResult());
        });
    };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);
    refreshList();
}

void PresetBrowser::refreshList()
{
    entries.clear();
    juce::StringArray selGenre, selMood, selExt;
    for (auto* b : tagButtons)
    {
        if (! b->getToggleState()) continue;
        const auto t = b->getButtonText();
        if (getGenreNames().contains (t)) selGenre.add (t);
        else if (getMoodNames().contains (t)) selMood.add (t);
        else selExt.add (t);
    }
    const auto text = search.getText().trim();

    const auto& presets = getFactoryPresets();
    for (size_t i = 0; i < presets.size(); ++i)
    {
        const auto& fp = presets[i];
        if (! selGenre.isEmpty() && ! selGenre.contains (fp.genre)) continue;
        if (! selMood.isEmpty() && ! selMood.contains (fp.mood)) continue;
        if (! selExt.isEmpty() && ! selExt.contains (fp.extension)) continue;
        if (text.isNotEmpty() && ! fp.name.containsIgnoreCase (text)) continue;
        entries.push_back ({ fp.name, fp.genre + " / " + fp.mood + " / " + fp.extension, (int) i, {} });
    }

    if (selGenre.isEmpty() && selMood.isEmpty() && selExt.isEmpty())
        for (const auto& f : ChordCatProcessor::getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.ccpreset"))
            if (text.isEmpty() || f.getFileNameWithoutExtension().containsIgnoreCase (text))
                entries.push_back ({ f.getFileNameWithoutExtension(), "User", -1, f });

    list.updateContent();
    list.repaint();
}

int PresetBrowser::getNumRows() { return (int) entries.size(); }

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) entries.size())) return;
    const auto& e = entries[(size_t) row];
    if (selected)
    {
        g.setColour (accent.withAlpha (0.18f));
        g.fillRoundedRectangle (0.0f, 1.0f, (float) w, (float) h - 2.0f, 4.0f);
    }
    g.setColour (selected ? accent : C::text);
    g.setFont (cat::catFont (13.0f));
    g.drawText (e.name, 8, 0, w - 16, h, juce::Justification::centredLeft);
    juce::ignoreUnused (owner);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (! juce::isPositiveAndBelow (row, (int) entries.size())) return;
    const auto& e = entries[(size_t) row];
    if (e.factoryIndex >= 0) processor.loadFactoryPreset (e.factoryIndex);
    else processor.loadUserPreset (e.file);
    list.setTooltip (e.tags);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    cat::CatLookAndFeel::drawGlassPanel (g, getLocalBounds().toFloat().reduced (2.0f), 8.0f, C::panel);
    auto r = getLocalBounds().reduced (10);
    drawSectionTitle (g, r.removeFromTop (16), "Presets");
    r.removeFromTop (36);
    const char* cats[] = { "Genre", "Mood", "Extension" };
    for (int c = 0; c < 3; ++c)
    {
        drawSectionTitle (g, r.removeFromTop (14), cats[c]);
        r.removeFromTop (60);
    }
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (10);
    r.removeFromTop (18);
    search.setBounds (r.removeFromTop (26));
    r.removeFromTop (10);

    for (int c = 0; c < 3; ++c)
    {
        r.removeFromTop (14);
        auto block = r.removeFromTop (54);
        r.removeFromTop (6);
        auto row1 = block.removeFromTop (26);
        block.removeFromTop (2);
        auto row2 = block;
        for (int i = 0; i < 5; ++i)
        {
            auto* b = tagButtons[c * 5 + i];
            auto& rowRef = i < 3 ? row1 : row2;
            const int w = i < 3 ? row1.getWidth() / 3 : (row2.getWidth() / 2);
            juce::ignoreUnused (rowRef);
            if (i < 3) b->setBounds (row1.getX() + i * w, row1.getY(), w - 3, row1.getHeight());
            else       b->setBounds (row2.getX() + (i - 3) * w, row2.getY(), w - 3, row2.getHeight());
        }
    }

    auto buttons = r.removeFromBottom (28);
    saveButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 3));
    buttons.removeFromLeft (6);
    loadButton.setBounds (buttons);
    r.removeFromBottom (6);
    list.setBounds (r);
}

//==============================================================================
// Editor
ChordCatEditor::ChordCatEditor (ChordCatProcessor& p)
    : processor (p),
      keyCombo (p.apvts, "keyRoot", {}, &p.midiLearn),
      scaleCombo (p.apvts, "scale", {}, &p.midiLearn),
      genreCombo (p.apvts, "genGenre", {}, &p.midiLearn),
      moodCombo (p.apvts, "genMood", {}, &p.midiLearn),
      browser (*this, p),
      keyboard (p.noteMonitor, 36, 96),
      catView (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, cat::CatStyle::Chord),
      surpriseControls (p, p.surpriseUndo, [&p] { p.surprise(); }),
      bootScreen (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, "CHORD", "-cat", "CHORD PROGRESSIONS", cat::CatStyle::Chord)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (keyCombo);
    addAndMakeVisible (scaleCombo);
    padsStepper = cat::Stepper::forParameter (p.apvts, "numPads", "Pads");
    addAndMakeVisible (*padsStepper);
    presetBar = std::make_unique<cat::PresetBar> (p.presetManager, juce::String());
    addAndMakeVisible (*presetBar);

    for (auto* b : { &undoButton, &redoButton, &aButton, &bButton, &padsViewButton, &rollViewButton, &diceRoot, &diceType, &diceVoicing })
    {
        styleSmallButton (*b);
        addAndMakeVisible (b);
    }
    addAndMakeVisible (dragButton);
    addAndMakeVisible (generateButton);
    addAndMakeVisible (genreCombo);
    addAndMakeVisible (moodCombo);

    undoButton.onClick = [this] { processor.undo(); };
    redoButton.onClick = [this] { processor.redo(); };
    aButton.onClick = [this] { processor.selectAB (0); };
    bButton.onClick = [this] { processor.selectAB (1); };
    aButton.setTooltip ("A/B compare: progression A");
    bButton.setTooltip ("A/B compare: progression B");

    dragButton.setTooltip ("Drag the whole progression (with chord-name markers) onto a Cubase track");
    dragButton.setColour (juce::TextButton::buttonColourId, C::raised);
    dragButton.onDragOut = [this] { dragSequenceOut (processor.buildProgressionSequence(), "Chord-Cat Progression"); };

    padsViewButton.onClick = [this] { setRollView (false); };
    rollViewButton.onClick = [this] { setRollView (true); };
    padsViewButton.setTooltip ("Pad view");
    rollViewButton.setTooltip ("Piano-roll view / custom voicing editor");

    generateButton.setColour (juce::TextButton::buttonColourId, accent.withAlpha (0.16f));
    generateButton.setColour (juce::TextButton::textColourOffId, C::text);
    generateButton.onClick = [this] { if (generatePanel != nullptr) generatePanel->generate(); };

    diceRoot.onClick = [this] { processor.diceSlot (getSelectedSlot(), 0); };
    diceType.onClick = [this] { processor.diceSlot (getSelectedSlot(), 1); };
    diceVoicing.onClick = [this] { processor.diceSlot (getSelectedSlot(), 2); };

    addAndMakeVisible (padArea);
    for (int i = 0; i < 16; ++i)
        padArea.addAndMakeVisible (pads.add (new ChordPad (*this, i)));
    addChildComponent (roll);

    editPanel = new EditPanel (*this, p);
    generatePanel = new GeneratePanel (*this, p);
    tabs.addTab ("EDIT", C::panel, editPanel, true);
    tabs.addTab ("PERFORM", C::panel, new PerformPanel (*this, p), true);
    tabs.addTab ("STRUM", C::panel, new StrumPanel (p), true);
    tabs.addTab ("GENERATE", C::panel, generatePanel, true);
    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    addAndMakeVisible (browser);

    keyboard.setColourForChannel ([this] (int ch)
    {
        const int bassCh = juce::roundToInt (processor.apvts.getRawParameterValue ("bassChannel")->load());
        return ch == bassCh ? C::teal : C::slate;
    });
    addAndMakeVisible (keyboard);

    setRollView (false);
    addAndMakeVisible (catView);
    addAndMakeVisible (surpriseControls);
    addAndMakeVisible (bootScreen);   // last: drawn over everything until it fades out

    setSize (1180, 840);

    refreshFromProcessor();
    startTimerHz (30);
}

ChordCatEditor::~ChordCatEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::Colour ChordCatEditor::colourForChord (const ChordDefinition& c)
{
    switch (getChordTypeInfo (c.chordType).family)
    {
        case FamilyMajor:      return juce::Colour (0xff8a9bd0);
        case FamilyMinor:      return juce::Colour (0xffa08fcc);
        case FamilyDominant:   return juce::Colour (0xffc48a88);
        case FamilyDiminished: return juce::Colour (0xff76aab0);
        case FamilyAugmented:  return juce::Colour (0xffb9a487);
        case FamilySuspended:  return juce::Colour (0xff7e9bd6);
        case FamilyOther:
        default:               return juce::Colour (0xff8590ad);
    }
}

void ChordCatEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::background);
    cat::CatLookAndFeel::drawLogo (g, juce::Rectangle<float> (242.0f, 6.0f, 170.0f, 32.0f), "CHORD", "-cat");
}

void ChordCatEditor::setRollView (bool shouldShowRoll)
{
    rollVisible = shouldShowRoll;
    roll.setVisible (rollVisible);
    padArea.setVisible (! rollVisible);
    padsViewButton.setToggleState (! rollVisible, juce::dontSendNotification);
    rollViewButton.setToggleState (rollVisible, juce::dontSendNotification);
}

void ChordCatEditor::resized()
{
    auto r = getLocalBounds().reduced (8);

    {
        auto left = r.removeFromLeft (220);
        presetBar->setBounds (left.removeFromTop (30));
        left.removeFromTop (6);
        browser.setBounds (left);
    }
    r.removeFromLeft (10);

    // Top bar
    {
        auto top = r.removeFromTop (36);
        auto logoZone = top.removeFromLeft (170);              // logo + cat
        catView.setBounds (logoZone.removeFromRight (44).expanded (0, 5));
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        auto item = [] (juce::Component& c, float w) { return juce::FlexItem (c).withWidth (w).withHeight (28.0f).withMargin ({ 0, 3, 0, 3 }); };
        fb.items.add (item (keyCombo, 64));
        fb.items.add (item (scaleCombo, 150));
        fb.items.add (item (*padsStepper, 110));
        fb.items.add (juce::FlexItem().withFlex (1.0f));
        fb.items.add (item (undoButton, 54));
        fb.items.add (item (redoButton, 54));
        fb.items.add (item (aButton, 32));
        fb.items.add (item (bButton, 32));
        fb.items.add (item (dragButton, 96));
        fb.performLayout (top);
    }
    r.removeFromTop (6);

    // Toolbar
    {
        auto bar = r.removeFromTop (34);
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        auto item = [] (juce::Component& c, float w) { return juce::FlexItem (c).withWidth (w).withHeight (28.0f).withMargin ({ 0, 3, 0, 3 }); };
        fb.items.add (item (padsViewButton, 56));
        fb.items.add (item (rollViewButton, 56));
        fb.items.add (juce::FlexItem().withWidth (20.0f));
        fb.items.add (item (generateButton, 100));
        fb.items.add (item (genreCombo, 110));
        fb.items.add (item (moodCombo, 110));
        fb.items.add (juce::FlexItem().withFlex (1.0f));
        fb.items.add (item (surpriseControls, 122));
        fb.items.add (juce::FlexItem().withWidth (10.0f));
        fb.items.add (item (diceRoot, 84));
        fb.items.add (item (diceType, 84));
        fb.items.add (item (diceVoicing, 100));
        fb.performLayout (bar);
    }
    r.removeFromTop (6);

    keyboard.setBounds (r.removeFromBottom (66));
    r.removeFromBottom (8);
    tabs.setBounds (r.removeFromBottom (juce::jlimit (250, 330, r.getHeight() / 2 - 20)));
    r.removeFromBottom (8);

    padArea.setBounds (r);
    roll.setBounds (r);
    layoutPads();

    bootScreen.setBounds (getLocalBounds());
}

void ChordCatEditor::layoutPads()
{
    const int numPads = processor.getNumPads();
    const int cols = 4;
    const int rows = (numPads + cols - 1) / cols;
    auto area = padArea.getLocalBounds();
    const int w = area.getWidth() / cols;
    const int h = area.getHeight() / juce::jmax (1, rows);

    for (int i = 0; i < 16; ++i)
    {
        auto* pad = pads[i];
        pad->setVisible (i < numPads);
        if (i < numPads)
            pad->setBounds (area.getX() + (i % cols) * w, area.getY() + (i / cols) * h, w, h);
    }
    shownPads = numPads;
}

void ChordCatEditor::selectSlot (int slot)
{
    processor.selectedSlot = juce::jlimit (0, 15, slot);
    if (editPanel != nullptr) editPanel->refresh();
    roll.setContent (progression, voicings, processor.getNumPads(), getSelectedSlot(), processor.playingSlot.load());
}

void ChordCatEditor::refreshFromProcessor()
{
    progression = processor.getProgression();
    voicings = processor.getPreviewVoicings();
    lastGlobal = processor.getGlobalState();
    shownVersion = processor.progressionVersion.load();

    for (int i = 0; i < 16; ++i)
        pads[i]->setContent (progression[(size_t) i], voicings[(size_t) i],
                             progression[(size_t) i].isActive ? processor.getRomanNumeral (i) : juce::String());

    if (shownPads != processor.getNumPads())
        layoutPads();

    roll.setContent (progression, voicings, processor.getNumPads(), getSelectedSlot(), processor.playingSlot.load());
    if (editPanel != nullptr) editPanel->refresh();
}

void ChordCatEditor::timerCallback()
{
    const auto g = processor.getGlobalState();
    const bool globalChanged = g.keyRoot != lastGlobal.keyRoot || g.scaleIndex != lastGlobal.scaleIndex
                            || g.scaleLock != lastGlobal.scaleLock || g.transpose != lastGlobal.transpose
                            || g.customScaleMask != lastGlobal.customScaleMask;
    static int lastVoiceLeading = -1;
    const int vl = juce::roundToInt (processor.apvts.getRawParameterValue ("voiceLeading")->load());

    if (processor.progressionVersion.load() != shownVersion || processor.getNumPads() != shownPads || globalChanged || vl != lastVoiceLeading)
    {
        lastVoiceLeading = vl;
        refreshFromProcessor();
    }

    const int playing = processor.playingSlot.load();
    const bool running = processor.uiRunning.load();
    const float fraction = running ? (float) (std::fmod (processor.uiPpq.load(), 4.0) / 4.0) : -1.0f;
    const int selected = getSelectedSlot();
    for (int i = 0; i < 16; ++i)
        pads[i]->setState (i == selected, i == playing, i == playing ? fraction : -1.0f);

    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());
    aButton.setToggleState (processor.getCurrentAB() == 0, juce::dontSendNotification);
    bButton.setToggleState (processor.getCurrentAB() == 1, juce::dontSendNotification);

    static int lastPlaying = -2;
    if (playing != lastPlaying && rollVisible)
        roll.setContent (progression, voicings, processor.getNumPads(), selected, playing);
    lastPlaying = playing;
}

void ChordCatEditor::showPadMenu (int slot)
{
    const auto& s = progression[(size_t) slot];
    const auto subs = ProgressionGenerator::getSubstitutions (s.chord, processor.getGlobalState());

    juce::PopupMenu subMenu;
    for (size_t i = 0; i < subs.size(); ++i)
        subMenu.addItem (100 + (int) i, subs[i].label);

    juce::PopupMenu voicingMenu;
    const auto vNames = getVoicingModelNames();
    for (int v = 0; v < vNames.size(); ++v)
        voicingMenu.addItem (200 + v, vNames[v], true, s.chord.voicingModel == v);

    juce::PopupMenu rhythmMenu;
    const auto rNames = getRhythmPatternNames();
    for (int r = 0; r < rNames.size(); ++r)
        rhythmMenu.addItem (300 + r, rNames[r], true, s.chord.rhythmPattern == r);

    juce::PopupMenu menu;
    menu.addSectionHeader ("Pad " + juce::String (slot + 1) + (s.isActive ? "  " + s.chord.getName() : juce::String()));
    menu.addSubMenu ("Substitute", subMenu, s.isActive);
    menu.addSubMenu ("Voicing", voicingMenu, s.isActive);
    menu.addSubMenu ("Rhythm", rhythmMenu, s.isActive);
    menu.addSeparator();
    menu.addItem (1, "Dice root (keep type)");
    menu.addItem (2, "Dice type (keep root)");
    menu.addItem (3, "Dice voicing only");
    menu.addSeparator();
    menu.addItem (4, "Copy", s.isActive);
    menu.addItem (5, "Paste", clipboard.has_value());
    menu.addItem (6, "Clear pad", s.isActive);
    menu.addItem (7, s.isActive ? "Deactivate pad" : "Activate pad");

    juce::Component::SafePointer<ChordCatEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe, slot, subs] (int result)
    {
        if (safe == nullptr || result == 0) return;
        auto& proc = safe->processor;

        if (result >= 300)      proc.editSlot (slot, [result] (ProgressionSlot& x) { x.chord.rhythmPattern = result - 300; });
        else if (result >= 200) proc.editSlot (slot, [result] (ProgressionSlot& x) { x.chord.voicingModel = result - 200; });
        else if (result >= 100)
        {
            const auto chord = subs[(size_t) (result - 100)].chord;
            proc.editSlot (slot, [chord] (ProgressionSlot& x) { x.chord = chord; x.isActive = true; });
        }
        else if (result <= 3)   proc.diceSlot (slot, result - 1);
        else if (result == 4)   safe->clipboard = safe->progression[(size_t) slot].chord;
        else if (result == 5 && safe->clipboard)
        {
            const auto chord = *safe->clipboard;
            proc.editSlot (slot, [chord] (ProgressionSlot& x) { x.chord = chord; x.isActive = true; });
        }
        else if (result == 6)   proc.editSlot (slot, [] (ProgressionSlot& x) { x = {}; });
        else if (result == 7)   proc.editSlot (slot, [] (ProgressionSlot& x) { x.isActive = ! x.isActive; });
    });
}

void ChordCatEditor::dragSlotOut (int slot)
{
    dragSequenceOut (processor.buildSlotSequence (slot), "Chord-Cat " + progression[(size_t) slot].chord.getName().replace ("/", "-"));
}

void ChordCatEditor::dragSequenceOut (const juce::MidiMessageSequence& seq, const juce::String& name)
{
    if (seq.getNumEvents() == 0)
        return;
    const auto file = cat::midiexport::writeMidiFile (seq, processor.getHostBpm(), name);
    cat::midiexport::dragFileOut (file, this);
}

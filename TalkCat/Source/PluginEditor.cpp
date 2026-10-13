#include "PluginEditor.h"
#include "MidiExport.h"

namespace C = cat::Colours;
using namespace talkcat;

namespace
{
    bool isBlack (int pitch) { const int pc = ((pitch % 12) + 12) % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    constexpr float minPitch = 24.0f, maxPitch = 108.0f;
}

//==============================================================================
std::vector<ScoreNote>& PianoRoll::clipboard()
{
    static std::vector<ScoreNote> c;
    return c;
}

PianoRoll::PianoRoll (TalkCatProcessor& p) : processor (p)
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (hBar);
    addAndMakeVisible (vBar);
    hBar.addListener (this);
    vBar.addListener (this);
    hBar.setAutoHide (false);
    vBar.setAutoHide (false);
    reload();
    // centre the view on the notes
    if (! edit.notes.empty())
    {
        float lo = 127, hi = 0;
        for (const auto& n : edit.notes) { lo = juce::jmin (lo, (float) n.pitch); hi = juce::jmax (hi, (float) n.pitch); }
        topPitch = juce::jlimit (minPitch + 20.0f, maxPitch + 0.5f, 0.5f * (lo + hi) + 14.0f);
    }
    startTimerHz (30);
}

PianoRoll::~PianoRoll()
{
    hBar.removeListener (this);
    vBar.removeListener (this);
}

juce::Rectangle<float> PianoRoll::gridArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft ((float) keysWidth).withTrimmedTop ((float) rulerHeight)
                           .withTrimmedRight ((float) barThickness).withTrimmedBottom ((float) barThickness);
}

float PianoRoll::beatToX (double beat) const { return gridArea().getX() + (float) ((beat - scrollBeat) * pxPerBeat); }
double PianoRoll::xToBeat (float x) const { return scrollBeat + (double) (x - gridArea().getX()) / pxPerBeat; }
float PianoRoll::pitchToY (float pitch) const { return gridArea().getY() + (topPitch - pitch) * rowH; }
float PianoRoll::yToPitch (float y) const { return topPitch - (y - gridArea().getY()) / rowH; }

juce::Rectangle<float> PianoRoll::noteRect (const ScoreNote& n) const
{
    const float x0 = beatToX (n.start), x1 = beatToX (n.end());
    return { x0, pitchToY ((float) n.pitch + 0.5f), juce::jmax (3.0f, x1 - x0), rowH };
}

double PianoRoll::snapBeat (double b) const
{
    return snap > 0.0 ? std::round (b / snap) * snap : b;
}

void PianoRoll::resized()
{
    const auto b = getLocalBounds();
    hBar.setBounds (keysWidth, b.getHeight() - barThickness, b.getWidth() - keysWidth - barThickness, barThickness);
    vBar.setBounds (b.getWidth() - barThickness, rulerHeight, barThickness, b.getHeight() - rulerHeight - barThickness);
    if (lyricEditor != nullptr) finishLyricEdit (true, 0);
    updateScrollBars();
}

void PianoRoll::updateScrollBars()
{
    const auto g = gridArea();
    const double visibleBeats = g.getWidth() / pxPerBeat;
    const double total = juce::jmax (edit.lengthBeats + 8.0, scrollBeat + visibleBeats);
    hBar.setRangeLimits (0.0, total, juce::dontSendNotification);
    hBar.setCurrentRange (scrollBeat, visibleBeats, juce::dontSendNotification);
    const double visibleRows = g.getHeight() / rowH;
    vBar.setRangeLimits (0.0, (double) (maxPitch - minPitch + 1.0f), juce::dontSendNotification);
    vBar.setCurrentRange ((double) (maxPitch + 0.5f - topPitch), visibleRows, juce::dontSendNotification);
}

void PianoRoll::scrollBarMoved (juce::ScrollBar* bar, double start)
{
    if (bar == &hBar) scrollBeat = juce::jmax (0.0, start);
    else topPitch = maxPitch + 0.5f - (float) start;
    if (lyricEditor != nullptr) finishLyricEdit (true, 0);
    repaint();
    if (onViewChanged) onViewChanged();
}

void PianoRoll::zoomTime (float f)
{
    const auto g = gridArea();
    const double centre = xToBeat (g.getCentreX());
    pxPerBeat = juce::jlimit (8.0f, 400.0f, pxPerBeat * f);
    scrollBeat = juce::jmax (0.0, centre - g.getWidth() * 0.5 / pxPerBeat);
    updateScrollBars();
    repaint();
    if (onViewChanged) onViewChanged();
}

void PianoRoll::zoomPitch (float f)
{
    const auto g = gridArea();
    const float centre = yToPitch (g.getCentreY());
    rowH = juce::jlimit (7.0f, 40.0f, rowH * f);
    topPitch = juce::jlimit (minPitch + 10.0f, maxPitch + 0.5f, centre + g.getHeight() * 0.5f / rowH);
    updateScrollBars();
    repaint();
}

//==============================================================================
void PianoRoll::reload()
{
    edit = processor.getScore();
    seenVersion = processor.scoreVersion.load();
    for (auto it = selection.begin(); it != selection.end();)
        it = edit.find (*it) == nullptr ? selection.erase (it) : std::next (it);
    updateScrollBars();
    repaint();
    if (onSelectionChanged) onSelectionChanged();
}

void PianoRoll::commit()
{
    edit.makeMonophonic();
    processor.setScore (edit);
    seenVersion = processor.scoreVersion.load();
    for (auto it = selection.begin(); it != selection.end();)
        it = edit.find (*it) == nullptr ? selection.erase (it) : std::next (it);
    updateScrollBars();
    repaint();
    if (onSelectionChanged) onSelectionChanged();
}

std::vector<int> PianoRoll::selectedIndices() const
{
    std::vector<int> v;
    for (int i = 0; i < (int) edit.notes.size(); ++i)
        if (selection.count (edit.notes[(size_t) i].id)) v.push_back (i);
    return v;
}

void PianoRoll::selectAll()
{
    selection.clear();
    for (const auto& n : edit.notes) selection.insert (n.id);
    repaint();
    if (onSelectionChanged) onSelectionChanged();
}

void PianoRoll::deleteSelection()
{
    if (selection.empty()) return;
    edit.notes.erase (std::remove_if (edit.notes.begin(), edit.notes.end(), [this] (const ScoreNote& n) { return selection.count (n.id) > 0; }), edit.notes.end());
    selection.clear();
    commit();
}

void PianoRoll::copySelection()
{
    auto& c = clipboard();
    c.clear();
    for (const auto& n : edit.notes) if (selection.count (n.id)) c.push_back (n);
}

void PianoRoll::paste()
{
    const auto& c = clipboard();
    if (c.empty()) return;
    double first = 1.0e9, lastEnd = 0.0;
    for (const auto& n : c) first = juce::jmin (first, n.start);
    for (const auto& n : edit.notes) if (selection.count (n.id)) lastEnd = juce::jmax (lastEnd, n.end());
    if (selection.empty())
    {
        const double ph = processor.uiScoreBeat.load();
        lastEnd = ph >= 0.0 ? snapBeat (ph) : snapBeat (xToBeat (gridArea().getX() + 40.0f));
    }
    selection.clear();
    for (auto n : c)
    {
        n.start = n.start - first + lastEnd;
        selection.insert (edit.addNote (n));
    }
    edit.lengthBeats = juce::jmax (edit.lengthBeats, std::ceil ((lastEnd + 4.0) / 4.0) * 4.0);
    commit();
}

void PianoRoll::duplicateSelection()
{
    copySelection();
    paste();
}

void PianoRoll::transposeSelection (int semis)
{
    if (selection.empty()) return;
    for (auto& n : edit.notes) if (selection.count (n.id)) n.pitch = juce::jlimit ((int) minPitch, (int) maxPitch, n.pitch + semis);
    commit();
    for (const auto& n : edit.notes) if (selection.count (n.id)) { previewNote (n); break; }
}

void PianoRoll::moveSelection (double beats)
{
    if (selection.empty()) return;
    for (auto& n : edit.notes) if (selection.count (n.id)) n.start = juce::jmax (0.0, n.start + beats);
    commit();
}

void PianoRoll::splitNote (int id)
{
    auto* n = edit.find (id);
    if (n == nullptr) return;
    ScoreNote b = *n;
    const double half = juce::jmax (snap > 0 ? snap : 0.125, snapBeat (n->length * 0.5));
    if (half >= n->length) return;
    n->length = half;
    b.start = n->start + half;
    b.length -= half;
    b.lyric = "-";
    b.phonemes = {};
    edit.addNote (b);
    commit();
}

void PianoRoll::previewNote (const ScoreNote& n)
{
    if (n.phonemes.isNotEmpty()) processor.preview (n.pitch, n.phonemes, n.language);
    else processor.preview (n.pitch, n.lyric == "-" ? juce::String ("a") : n.lyric, n.language);
}

int PianoRoll::noteAt (juce::Point<float> pos, bool& onEdge) const
{
    onEdge = false;
    for (int i = (int) edit.notes.size(); --i >= 0;)
    {
        const auto r = noteRect (edit.notes[(size_t) i]);
        if (r.expanded (0.0f, 0.5f).contains (pos))
        {
            onEdge = pos.x > r.getRight() - juce::jmin (7.0f, r.getWidth() * 0.35f);
            return edit.notes[(size_t) i].id;
        }
    }
    return -1;
}

//==============================================================================
void PianoRoll::timerCallback()
{
    if (drag == Drag::None && lyricEditor == nullptr && processor.scoreVersion.load() != seenVersion)
        reload();

    const double beat = processor.uiScoreBeat.load();
    const float pitch = processor.engine.uiPitch[0].load();
    if (beat >= 0.0)
    {
        if (beat < lastTrailBeat - 0.5) trail.clear();      // jumped back / looped
        if (pitch > 0.0f) trail.push_back ({ beat, pitch });
        else if (! trail.empty() && trail.back().pitch > 0.0f) trail.push_back ({ beat, -1.0f });
        if (trail.size() > 6000) trail.erase (trail.begin(), trail.begin() + 1000);
        lastTrailBeat = beat;

        if (follow && drag == Drag::None && lyricEditor == nullptr)
        {
            const auto g = gridArea();
            const double visible = g.getWidth() / pxPerBeat;
            if (beat > scrollBeat + visible * 0.88 || beat < scrollBeat)
            {
                scrollBeat = juce::jmax (0.0, beat - visible * 0.1);
                updateScrollBars();
                if (onViewChanged) onViewChanged();
            }
        }
    }
    repaint();
}

void PianoRoll::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto grid = gridArea();
    g.setColour (juce::Colour (0xff101116));
    g.fillRect (bounds);

    // rows
    g.saveState();
    g.reduceClipRegion (grid.toNearestInt());
    const int pTop = (int) std::ceil (topPitch), pBottom = (int) std::floor (yToPitch (grid.getBottom()));
    for (int p = pBottom - 1; p <= pTop + 1; ++p)
    {
        const float y = pitchToY ((float) p + 0.5f);
        g.setColour (isBlack (p) ? juce::Colour (0xff121319) : juce::Colour (0xff181a20));
        g.fillRect (grid.getX(), y, grid.getWidth(), rowH);
        if (((p % 12) + 12) % 12 == 0)
        {
            g.setColour (C::outline.withAlpha (0.8f));
            g.drawHorizontalLine (juce::roundToInt (y + rowH), grid.getX(), grid.getRight());
        }
    }
    // beyond the score end
    const float endX = beatToX (edit.lengthBeats);
    if (endX < grid.getRight())
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (endX, grid.getY(), grid.getRight() - endX, grid.getHeight()));
    }
    // vertical grid
    const double b0 = std::floor (xToBeat (grid.getX())), b1 = xToBeat (grid.getRight());
    const double sub = snap > 0.0 && snap * pxPerBeat >= 6.0 ? snap : 1.0;
    for (double b = b0; b <= b1; b += sub)
    {
        const float x = beatToX (b);
        const bool bar = std::abs (std::fmod (b, 4.0)) < 1.0e-6;
        const bool beat = std::abs (b - std::round (b)) < 1.0e-6;
        g.setColour (bar ? C::outline.brighter (0.5f) : (beat ? C::outline.withAlpha (0.9f) : C::outline.withAlpha (0.35f)));
        g.drawVerticalLine (juce::roundToInt (x), grid.getY(), grid.getBottom());
    }

    // legato connections
    for (size_t i = 0; i + 1 < edit.notes.size(); ++i)
    {
        const auto& a = edit.notes[i];
        const auto& b = edit.notes[i + 1];
        if (b.start - a.end() > 0.02) continue;
        g.setColour (C::accent.withAlpha (0.35f));
        g.drawLine (beatToX (a.end()) - 2.0f, pitchToY ((float) a.pitch), beatToX (b.start) + 2.0f, pitchToY ((float) b.pitch), 1.5f);
    }

    // notes
    const int activeId = processor.uiActiveNote.load();
    const auto lang = processor.language();
    for (const auto& n : edit.notes)
    {
        const auto r = noteRect (n);
        if (r.getRight() < grid.getX() || r.getX() > grid.getRight()) continue;
        const bool sel = selection.count (n.id) > 0;
        const bool act = n.id == activeId;
        const bool hold = n.lyric == "-" || n.lyric == "+";
        auto col = hold ? C::slate : (n.language >= 0 && n.language != (int) lang ? C::purple : C::accent);
        if (act) col = C::teal;
        g.setColour (col.withAlpha (sel ? 0.95f : 0.45f + 0.4f * n.velocity));
        g.fillRoundedRectangle (r.reduced (0.5f, 1.0f), 3.0f);
        if (sel)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawRoundedRectangle (r.reduced (0.5f, 1.0f), 3.0f, 1.4f);
        }
        if (n.id == hoverNote && hoverEdge)
        {
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.fillRect (r.getRight() - 3.0f, r.getY() + 3.0f, 2.0f, r.getHeight() - 6.0f);
        }
        // lyric + phonemes
        g.setColour (sel || act ? C::background : C::background.withAlpha (0.9f));
        g.setFont (cat::catFont (juce::jlimit (9.0f, 13.0f, rowH - 2.0f), true));
        g.drawText (n.lyric, r.reduced (4.0f, 0.0f), juce::Justification::centredLeft, true);
        if (rowH >= 11.0f && r.getWidth() > 30.0f)
        {
            const auto syls = noteSyllables (n, lang);
            juce::StringArray ph;
            for (const auto& s : syls) ph.add (phonemeString (s));
            g.setColour (C::textDim);
            g.setFont (cat::catFont (9.5f));
            g.drawText (ph.joinIntoString (" | "), juce::Rectangle<float> (r.getX() + 2.0f, r.getBottom(), juce::jmax (60.0f, r.getWidth() + 40.0f), 11.0f),
                        juce::Justification::centredLeft, true);
        }
    }

    // sung pitch trail
    if (trail.size() > 1)
    {
        juce::Path path;
        bool on = false;
        for (const auto& t : trail)
        {
            if (t.pitch <= 0.0f) { on = false; continue; }
            const float x = beatToX (t.beat), y = pitchToY (t.pitch);
            if (! on) { path.startNewSubPath (x, y); on = true; } else path.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xffffd28a).withAlpha (0.85f));
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // rubber band
    if (drag == Drag::Band)
    {
        g.setColour (C::accent.withAlpha (0.15f));
        g.fillRect (band);
        g.setColour (C::accent.withAlpha (0.7f));
        g.drawRect (band, 1.0f);
    }

    // playhead
    const double ph = processor.uiScoreBeat.load();
    if (ph >= 0.0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawVerticalLine (juce::roundToInt (beatToX (ph)), grid.getY(), grid.getBottom());
    }
    if (edit.notes.empty())
    {
        g.setColour (C::textDim);
        g.setFont (cat::catFont (14.0f));
        g.drawText ("Click to draw notes, double-click a note to type its lyric (or load a song from Songs)", grid, juce::Justification::centred);
    }
    g.restoreState();

    // ruler
    const auto ruler = juce::Rectangle<float> (grid.getX(), 0.0f, grid.getWidth(), (float) rulerHeight);
    g.setColour (C::panel);
    g.fillRect (ruler);
    g.setFont (cat::catFont (10.5f, true));
    g.saveState();
    g.reduceClipRegion (ruler.toNearestInt());
    for (double b = std::floor (b0 / 4.0) * 4.0; b <= b1; b += 4.0)
    {
        const float x = beatToX (b);
        g.setColour (C::textDim);
        g.drawVerticalLine (juce::roundToInt (x), 8.0f, (float) rulerHeight);
        g.drawText (juce::String ((int) (b / 4.0) + edit.startBar), juce::Rectangle<float> (x + 3.0f, 2.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
    }
    if (ph >= 0.0)
    {
        juce::Path tri;
        const float x = beatToX (ph);
        tri.addTriangle (x - 5.0f, 8.0f, x + 5.0f, 8.0f, x, (float) rulerHeight);
        g.setColour (juce::Colours::white);
        g.fillPath (tri);
    }
    g.restoreState();

    // keyboard
    const auto keys = juce::Rectangle<float> (0.0f, grid.getY(), (float) keysWidth, grid.getHeight());
    g.saveState();
    g.reduceClipRegion (keys.toNearestInt());
    const float sung = processor.engine.uiPitch[0].load() > 0 ? processor.engine.uiPitch[0].load() : processor.engine.uiPitch[1].load();
    for (int p = pBottom - 1; p <= pTop + 1; ++p)
    {
        const float y = pitchToY ((float) p + 0.5f);
        const bool lit = sung > 0.0f && std::abs (sung - (float) p) < 0.5f;
        g.setColour (lit ? C::accent : (isBlack (p) ? juce::Colour (0xff23252c) : juce::Colour (0xffd8dae0)));
        g.fillRect (keys.getX(), y + 0.5f, keys.getWidth() - (isBlack (p) ? 16.0f : 1.0f), rowH - 1.0f);
        if (((p % 12) + 12) % 12 == 0 && rowH >= 9.0f)
        {
            g.setColour (C::background);
            g.setFont (cat::catFont (juce::jmin (11.0f, rowH - 1.0f), true));
            g.drawText (cat::cubaseNoteName (p), juce::Rectangle<float> (keys.getX() + 2.0f, y, keys.getWidth() - 6.0f, rowH), juce::Justification::centredRight);
        }
    }
    g.restoreState();
    g.setColour (C::panel);
    g.fillRect (0.0f, 0.0f, (float) keysWidth, (float) rulerHeight);
}

//==============================================================================
void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    bool edge = false;
    const int id = gridArea().contains (e.position) ? noteAt (e.position, edge) : -1;
    if (id != hoverNote || edge != hoverEdge) { hoverNote = id; hoverEdge = edge; repaint(); }
    if (tool == 2) setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else setMouseCursor (id >= 0 ? (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor) : juce::MouseCursor::NormalCursor);
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (lyricEditor != nullptr) finishLyricEdit (true, 0);
    changed = false;
    dragStart = e.position;
    const auto grid = gridArea();

    if (e.position.x < keysWidth && e.position.y > rulerHeight)
    {
        drag = Drag::Keys;
        lastPreviewPitch = juce::roundToInt (yToPitch (e.position.y));
        ScoreNote n; n.pitch = lastPreviewPitch;
        const auto sel = selectedIndices();
        if (! sel.empty()) { n.lyric = edit.notes[(size_t) sel[0]].lyric; n.phonemes = edit.notes[(size_t) sel[0]].phonemes; }
        previewNote (n);
        return;
    }
    if (! grid.contains (e.position)) return;

    bool edge = false;
    const int id = noteAt (e.position, edge);

    if (e.mods.isPopupMenu())
    {
        if (id >= 0)
        {
            if (! selection.count (id)) { selection = { id }; if (onSelectionChanged) onSelectionChanged(); }
            showNoteMenu (id);
        }
        return;
    }

    if (tool == 2)
    {
        drag = Drag::Erase;
        if (id >= 0) { edit.notes.erase (edit.notes.begin() + edit.indexOf (id)); changed = true; selection.erase (id); }
        repaint();
        return;
    }

    if (id >= 0)
    {
        if (e.mods.isShiftDown() || e.mods.isCommandDown())
        {
            if (selection.count (id)) selection.erase (id); else selection.insert (id);
        }
        else if (! selection.count (id))
            selection = { id };
        if (onSelectionChanged) onSelectionChanged();

        if (e.mods.isAltDown())
        {
            // Alt-drag copies the selection
            std::set<int> copies;
            for (auto n : std::vector<ScoreNote> (edit.notes))
                if (selection.count (n.id)) copies.insert (edit.addNote (n));
            selection = copies;
        }

        dragOriginals.clear();
        for (const auto& n : edit.notes) if (selection.count (n.id)) dragOriginals.push_back (n);
        dragNoteId = id;
        if (auto* n = edit.find (id)) { lastPreviewPitch = n->pitch; previewNote (*n); }
        if (! e.mods.isAltDown() && edge) drag = Drag::Resize;
        else
        {
            drag = Drag::Move;
            if (e.mods.isAltDown())
            {
                // the clicked note is now one of the copies: drag the copies
                dragOriginals.clear();
                for (const auto& n : edit.notes) if (selection.count (n.id)) dragOriginals.push_back (n);
                dragNoteId = dragOriginals.empty() ? -1 : dragOriginals.front().id;
                changed = true;
            }
        }
        repaint();
        return;
    }

    if (tool == 1 || e.mods.isShiftDown())
    {
        drag = Drag::Band;
        band = { e.position, e.position };
        if (! e.mods.isShiftDown()) selection.clear();
        if (onSelectionChanged) onSelectionChanged();
        return;
    }

    // Draw tool on empty space: a new note
    ScoreNote n;
    n.start = juce::jmax (0.0, snap > 0.0 ? std::floor (xToBeat (e.position.x) / snap) * snap : xToBeat (e.position.x));
    n.length = lastLength;
    n.pitch = juce::jlimit ((int) minPitch, (int) maxPitch, juce::roundToInt (yToPitch (e.position.y)));
    n.lyric = processor.language() == Language::Japanese ? "ra" : (processor.language() == Language::Cat ? "meow" : "la");
    const int newId = edit.addNote (n);
    selection = { newId };
    dragOriginals = { *edit.find (newId) };
    dragNoteId = newId;
    drag = Drag::Create;
    changed = true;
    lastPreviewPitch = n.pitch;
    previewNote (n);
    if (n.end() > edit.lengthBeats) edit.lengthBeats = std::ceil (n.end() / 4.0) * 4.0;
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    const auto grid = gridArea();
    switch (drag)
    {
        case Drag::Keys:
        {
            const int p = juce::roundToInt (yToPitch (e.position.y));
            if (p != lastPreviewPitch) { lastPreviewPitch = p; ScoreNote n; n.pitch = p; previewNote (n); }
            break;
        }
        case Drag::Erase:
        {
            bool edge = false;
            const int id = noteAt (e.position, edge);
            if (id >= 0) { edit.notes.erase (edit.notes.begin() + edit.indexOf (id)); changed = true; selection.erase (id); repaint(); }
            break;
        }
        case Drag::Band:
        {
            band = juce::Rectangle<float> (dragStart, e.position);
            if (! e.mods.isShiftDown()) selection.clear();
            for (const auto& n : edit.notes) if (noteRect (n).intersects (band)) selection.insert (n.id);
            repaint();
            break;
        }
        case Drag::Move:
        {
            double db = (e.position.x - dragStart.x) / pxPerBeat;
            if (snap > 0.0) db = std::round (db / snap) * snap;
            const int dp = juce::roundToInt ((dragStart.y - e.position.y) / rowH);
            if (std::abs (e.position.x - dragStart.x) < 3.0f && dp == 0) break;
            for (const auto& o : dragOriginals)
                if (auto* n = edit.find (o.id))
                {
                    n->start = juce::jmax (0.0, o.start + db);
                    n->pitch = juce::jlimit ((int) minPitch, (int) maxPitch, o.pitch + dp);
                }
            edit.sort();
            changed = true;
            if (auto* n = edit.find (dragNoteId); n != nullptr && n->pitch != lastPreviewPitch) { lastPreviewPitch = n->pitch; previewNote (*n); }
            repaint();
            break;
        }
        case Drag::Create:
        case Drag::Resize:
        {
            const double endBeat = xToBeat (e.position.x);
            for (const auto& o : dragOriginals)
                if (auto* n = edit.find (o.id))
                {
                    double newEnd = o.id == dragNoteId ? endBeat : o.end() + (endBeat - (edit.find (dragNoteId) ? dragOriginals.front().end() : endBeat));
                    if (o.id == dragNoteId && snap > 0.0) newEnd = std::ceil (newEnd / snap - 0.3) * snap;
                    n->length = juce::jmax (snap > 0.0 ? snap : 1.0 / 32.0, newEnd - n->start);
                    if (o.id == dragNoteId) lastLength = n->length;
                }
            changed = true;
            if (auto* n = edit.find (dragNoteId)) edit.lengthBeats = juce::jmax (edit.lengthBeats, std::ceil (n->end() / 4.0) * 4.0);
            repaint();
            break;
        }
        case Drag::None:
        default:
            break;
    }
    // auto-scroll
    if (drag != Drag::None && drag != Drag::Keys)
    {
        if (e.position.x > grid.getRight() - 10.0f) { scrollBeat += 0.25; updateScrollBars(); if (onViewChanged) onViewChanged(); }
        if (e.position.x < grid.getX() + 4.0f && scrollBeat > 0.0) { scrollBeat = juce::jmax (0.0, scrollBeat - 0.25); updateScrollBars(); if (onViewChanged) onViewChanged(); }
    }
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::Keys) processor.previewStop();
    if (changed) commit();
    drag = Drag::None;
    changed = false;
    repaint();
    if (onSelectionChanged) onSelectionChanged();
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    bool edge = false;
    const int id = noteAt (e.position, edge);
    if (id >= 0) editLyric (id);
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        zoomTime (w.deltaY > 0 ? 1.15f : 1.0f / 1.15f);
        return;
    }
    if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        scrollBeat = juce::jmax (0.0, scrollBeat - d * 40.0 / pxPerBeat * 4.0);
        if (onViewChanged) onViewChanged();
    }
    else
        topPitch = juce::jlimit (minPitch + gridArea().getHeight() / rowH, maxPitch + 0.5f, topPitch + w.deltaY * 8.0f);
    if (lyricEditor != nullptr) finishLyricEdit (true, 0);
    updateScrollBars();
    repaint();
}

bool PianoRoll::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const int code = k.getKeyCode();
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { deleteSelection(); return true; }
    if (mods.isCommandDown() && (code == 'A' || code == 'a')) { selectAll(); return true; }
    if (mods.isCommandDown() && (code == 'C' || code == 'c')) { copySelection(); return true; }
    if (mods.isCommandDown() && (code == 'X' || code == 'x')) { copySelection(); deleteSelection(); return true; }
    if (mods.isCommandDown() && (code == 'V' || code == 'v')) { paste(); return true; }
    if (mods.isCommandDown() && (code == 'D' || code == 'd')) { duplicateSelection(); return true; }
    if (mods.isCommandDown() && (code == 'Z' || code == 'z')) { mods.isShiftDown() ? processor.redo() : processor.undo(); reload(); return true; }
    if (mods.isCommandDown() && (code == 'Y' || code == 'y')) { processor.redo(); reload(); return true; }
    if (code == juce::KeyPress::upKey) { transposeSelection (mods.isShiftDown() ? 12 : 1); return true; }
    if (code == juce::KeyPress::downKey) { transposeSelection (mods.isShiftDown() ? -12 : -1); return true; }
    if (code == juce::KeyPress::rightKey) { moveSelection (mods.isShiftDown() ? 4.0 : (snap > 0 ? snap : 0.25)); return true; }
    if (code == juce::KeyPress::leftKey) { moveSelection (mods.isShiftDown() ? -4.0 : -(snap > 0 ? snap : 0.25)); return true; }
    if (code == juce::KeyPress::returnKey && ! selection.empty()) { editLyric (*selection.begin()); return true; }
    return false;
}

//==============================================================================
void PianoRoll::editLyric (int id)
{
    auto* n = edit.find (id);
    if (n == nullptr) return;
    // make sure it is visible
    const auto grid = gridArea();
    if (beatToX (n->start) < grid.getX() || beatToX (n->start) > grid.getRight() - 60.0f)
    {
        scrollBeat = juce::jmax (0.0, n->start - 2.0);
        updateScrollBars();
        if (onViewChanged) onViewChanged();
    }
    if (pitchToY ((float) n->pitch) < grid.getY() || pitchToY ((float) n->pitch) > grid.getBottom())
    {
        topPitch = juce::jlimit (minPitch + 10.0f, maxPitch + 0.5f, (float) n->pitch + grid.getHeight() * 0.5f / rowH);
        updateScrollBars();
    }
    selection = { id };
    if (onSelectionChanged) onSelectionChanged();
    lyricNoteId = id;
    lyricEditor = std::make_unique<LyricEditor>();
    auto r = noteRect (*n).toNearestInt();
    r.setWidth (juce::jmax (120, r.getWidth()));
    r.setHeight (juce::jmax (22, r.getHeight() + 4));
    lyricEditor->setBounds (r.translated (0, -2));
    lyricEditor->setFont (cat::catFont (13.0f, true));
    lyricEditor->setText (n->phonemes.isNotEmpty() && n->lyric.isEmpty() ? n->phonemes : n->lyric, false);
    lyricEditor->selectAll();
    lyricEditor->setColour (juce::TextEditor::backgroundColourId, C::raised);
    lyricEditor->setColour (juce::TextEditor::outlineColourId, C::accent);
    lyricEditor->setColour (juce::TextEditor::focusedOutlineColourId, C::accent);
    lyricEditor->setTooltip ("Lyric: a syllable or word. Several words fill the following notes. [l ah v] = phonemes, - = hold, br = breath. Tab = next note.");
    lyricEditor->onReturnKey = [this] { finishLyricEdit (true, 0); };
    lyricEditor->onEscapeKey = [this] { finishLyricEdit (false, 0); };
    lyricEditor->onFocusLost = [this] { if (lyricEditor != nullptr) juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PianoRoll> (this)] { if (sp != nullptr && sp->lyricEditor != nullptr && ! sp->lyricEditor->hasKeyboardFocus (true)) sp->finishLyricEdit (true, 0); }); };
    lyricEditor->onTab = [this] (int dir) { finishLyricEdit (true, dir); };
    addAndMakeVisible (*lyricEditor);
    lyricEditor->grabKeyboardFocus();
}

void PianoRoll::finishLyricEdit (bool commitText, int moveBy)
{
    if (lyricEditor == nullptr) return;
    const auto text = lyricEditor->getText().trim();
    const int id = lyricNoteId;
    auto ed = std::move (lyricEditor);
    removeChildComponent (ed.get());
    lyricNoteId = -1;
    int nextId = -1;
    if (commitText)
    {
        const int idx = edit.indexOf (id);
        if (idx >= 0 && text.isNotEmpty())
        {
            auto& n = edit.notes[(size_t) idx];
            const auto lang = n.language >= 0 ? (Language) n.language : processor.language();
            const bool several = ! text.startsWith ("[") && textToSyllables (text, lang).size() > 1
                                 && (text.containsChar (' ') || text.containsChar ('-'));
            if (several)
            {
                const int count = (int) textToSyllables (text, lang).size();
                edit.fillLyrics (text, idx, lang);
                moveBy = moveBy == 0 ? 0 : moveBy;
                const int after = juce::jmin ((int) edit.notes.size() - 1, idx + count);
                if (moveBy > 0 && after > idx) nextId = edit.notes[(size_t) after].id;
                moveBy = 0;
            }
            else if (text.startsWith ("["))
            {
                n.phonemes = text;
                n.lyric = text.removeCharacters ("[]").removeCharacters (" ");
            }
            else
            {
                n.lyric = text;
                n.phonemes = {};
            }
            commit();
            if (auto* nn = edit.find (id)) previewNote (*nn);
        }
    }
    if (moveBy != 0)
    {
        const int idx = edit.indexOf (id);
        const int to = idx + moveBy;
        if (to >= 0 && to < (int) edit.notes.size()) nextId = edit.notes[(size_t) to].id;
    }
    if (nextId >= 0)
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PianoRoll> (this), nextId] { if (sp != nullptr) sp->editLyric (nextId); });
    else
        grabKeyboardFocus();
    repaint();
}

void PianoRoll::showNoteMenu (int id)
{
    auto* n = edit.find (id);
    if (n == nullptr) return;
    juce::PopupMenu m;
    m.addSectionHeader ("\"" + n->lyric + "\"  " + cat::cubaseNoteName (n->pitch));
    m.addItem (1, "Edit lyric (double-click)");
    m.addItem (2, "Edit phonemes ...");
    m.addItem (3, "Use spelling (clear phonemes)", n->phonemes.isNotEmpty());
    m.addItem (4, "Preview");
    m.addSeparator();
    m.addItem (5, "Split in two");
    m.addItem (6, "Stretch to the next note (legato)");
    m.addItem (7, "Make it a hold (-)");
    m.addItem (8, "Make it a breath (br)");
    juce::PopupMenu lang;
    lang.addItem (20, "Plug-in language", true, n->language < 0);
    for (int i = 0; i < 4; ++i) lang.addItem (21 + i, languageNames()[i], true, n->language == i);
    m.addSubMenu ("Language", lang);
    m.addSeparator();
    m.addItem (9, "Copy");
    m.addItem (10, "Delete");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [sp = juce::Component::SafePointer<PianoRoll> (this), id] (int r)
    {
        if (sp == nullptr || r == 0) return;
        auto* note = sp->edit.find (id);
        if (note == nullptr) return;
        switch (r)
        {
            case 1: sp->editLyric (id); break;
            case 2:
            {
                const auto syls = noteSyllables (*note, sp->processor.language());
                juce::StringArray ph;
                for (const auto& s : syls) ph.add (phonemeString (s));
                note->phonemes = "[" + ph.joinIntoString (" | ") + "]";
                note->lyric = {};
                sp->editLyric (id);
                if (sp->lyricEditor != nullptr) sp->lyricEditor->setText (note->phonemes, false);
                break;
            }
            case 3: note->phonemes = {}; if (note->lyric.isEmpty()) note->lyric = "la"; sp->commit(); break;
            case 4: sp->previewNote (*note); break;
            case 5: sp->splitNote (id); break;
            case 6:
            {
                const int idx = sp->edit.indexOf (id);
                if (idx + 1 < (int) sp->edit.notes.size()) { note->length = sp->edit.notes[(size_t) idx + 1].start - note->start; sp->commit(); }
                break;
            }
            case 7: note->lyric = "-"; note->phonemes = {}; sp->commit(); break;
            case 8: note->lyric = "br"; note->phonemes = {}; sp->commit(); break;
            case 9: sp->copySelection(); break;
            case 10: sp->deleteSelection(); break;
            default:
                if (r >= 20 && r <= 24) { note->language = r == 20 ? -1 : r - 21; sp->commit(); }
                break;
        }
    });
}

//==============================================================================
LaneEditor::LaneEditor (TalkCatProcessor& p, PianoRoll& r) : processor (p), roll (r)
{
    for (int l = 0; l < numLanes; ++l) laneBox.addItem (laneInfo (l).name, l + 1);
    laneBox.setSelectedId (lane + 1, juce::dontSendNotification);
    laneBox.onChange = [this] { lane = laneBox.getSelectedId() - 1; repaint(); };
    laneBox.setTooltip ("Which curve to draw: Pitch (bends in cents), Dynamics, Breath, Brightness, Gender, Tension, Growl, Vibrato, Cat, Mouth");
    clearButton.setTooltip ("Clear this curve over the whole song");
    clearButton.onClick = [this]
    {
        auto s = roll.edit;
        s.lanes[(size_t) lane].clear();
        roll.edit = s;
        roll.commit();
        repaint();
    };
    addAndMakeVisible (laneBox);
    addAndMakeVisible (clearButton);
    setTooltip ("Draw a curve with the mouse. Right-drag erases (back to the default).");
    startTimerHz (20);
}

void LaneEditor::resized()
{
    const auto a = area().toNearestInt();
    laneBox.setBounds (a.getX() + 4, a.getY() + 4, 130, 22);
    clearButton.setBounds (a.getX() + 138, a.getY() + 4, 54, 22);
}

juce::Rectangle<float> LaneEditor::area() const
{
    const auto rollGrid = roll.gridArea();
    return { rollGrid.getX(), 4.0f, rollGrid.getWidth(), (float) getHeight() - 8.0f };
}

float LaneEditor::valueToY (float v) const
{
    const auto& li = laneInfo (lane);
    const auto a = area();
    return a.getBottom() - (v - li.min) / (li.max - li.min) * a.getHeight();
}

float LaneEditor::yToValue (float y) const
{
    const auto& li = laneInfo (lane);
    const auto a = area();
    return juce::jlimit (li.min, li.max, li.min + (a.getBottom() - y) / a.getHeight() * (li.max - li.min));
}

void LaneEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101116));
    const auto a = area();
    const auto& li = laneInfo (lane);
    g.setColour (juce::Colour (0xff15161b));
    g.fillRect (a);
    // beat grid
    const double b0 = std::floor (roll.xToBeat (a.getX())), b1 = roll.xToBeat (a.getRight());
    for (double b = b0; b <= b1; b += 1.0)
    {
        const bool bar = std::abs (std::fmod (b, 4.0)) < 1.0e-6;
        g.setColour (bar ? C::outline.brighter (0.4f) : C::outline.withAlpha (0.5f));
        g.drawVerticalLine (juce::roundToInt (roll.beatToX (b)), a.getY(), a.getBottom());
    }
    g.setColour (C::textDim.withAlpha (0.5f));
    g.drawHorizontalLine (juce::roundToInt (valueToY (li.def)), a.getX(), a.getRight());

    // notes as faint blocks (orientation)
    g.saveState();
    g.reduceClipRegion (a.toNearestInt());
    for (const auto& n : roll.edit.notes)
    {
        g.setColour (C::accent.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float> (roll.beatToX (n.start), a.getY(), roll.beatToX (n.end()) - roll.beatToX (n.start), a.getHeight()).reduced (0.5f, 0.0f));
    }
    // the curve
    juce::Path path;
    const auto& pts = roll.edit.lanes[(size_t) lane];
    const float step = 3.0f;
    bool first = true;
    for (float x = a.getX(); x <= a.getRight(); x += step)
    {
        const double b = roll.xToBeat (x);
        float v = roll.edit.laneValue (lane, b);
        if (! stroke.empty() && ! erasing && b >= juce::jmin (dragFrom, dragTo) && b <= juce::jmax (dragFrom, dragTo))
        {
            // show the stroke being drawn
            auto it = std::lower_bound (stroke.begin(), stroke.end(), b, [] (const CurvePoint& cp, double bb) { return cp.beat < bb; });
            if (it != stroke.end()) v = it->value;
        }
        const float y = valueToY (v);
        if (first) { path.startNewSubPath (x, y); first = false; } else path.lineTo (x, y);
    }
    g.setColour (pts.empty() && stroke.empty() ? C::textDim : C::teal);
    g.strokePath (path, juce::PathStrokeType (2.0f));
    // playhead
    const double ph = processor.uiScoreBeat.load();
    if (ph >= 0.0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawVerticalLine (juce::roundToInt (roll.beatToX (ph)), a.getY(), a.getBottom());
    }
    g.restoreState();

    g.setColour (C::textDim);
    g.setFont (cat::catFont (10.0f));
    g.drawText (juce::String (li.max) + " " + li.unit, juce::Rectangle<float> (a.getRight() - 104.0f, a.getY(), 100.0f, 12.0f), juce::Justification::centredRight);
    g.drawText (juce::String (li.min) + " " + li.unit, juce::Rectangle<float> (a.getRight() - 104.0f, a.getBottom() - 12.0f, 100.0f, 12.0f), juce::Justification::centredRight);
}

void LaneEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! area().contains (e.position)) return;
    erasing = e.mods.isPopupMenu();
    stroke.clear();
    dragFrom = dragTo = roll.xToBeat (e.position.x);
    if (! erasing) stroke.push_back ({ dragFrom, yToValue (e.position.y) });
    repaint();
}

void LaneEditor::mouseDrag (const juce::MouseEvent& e)
{
    const double b = roll.xToBeat (juce::jlimit (area().getX(), area().getRight(), e.position.x));
    dragTo = b;
    if (! erasing)
    {
        stroke.push_back ({ b, yToValue (e.position.y) });
        std::sort (stroke.begin(), stroke.end(), [] (const CurvePoint& x, const CurvePoint& y) { return x.beat < y.beat; });
        // thin out: one point per ~1/32 beat
        std::vector<CurvePoint> thin;
        for (const auto& p : stroke)
            if (thin.empty() || p.beat - thin.back().beat >= 1.0 / 48.0) thin.push_back (p); else thin.back().value = p.value;
        stroke = thin;
    }
    repaint();
}

void LaneEditor::mouseUp (const juce::MouseEvent&)
{
    if (erasing) roll.edit.clearLaneRange (lane, dragFrom, dragTo);
    else if (! stroke.empty())
    {
        auto pts = stroke;
        if (pts.size() == 1) { pts.push_back ({ pts[0].beat + 0.01, pts[0].value }); }
        roll.edit.setLaneRange (lane, pts.front().beat, pts.back().beat, pts);
    }
    stroke.clear();
    roll.commit();
    repaint();
}

//==============================================================================
void TractView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (6.0f);
    g.setColour (juce::Colour (0xff101116));
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (C::textDim);
    g.setFont (cat::catFont (11.0f, true));
    g.drawText ("VOCAL TRACT  (throat -> lips)", b.removeFromTop (18.0f).reduced (8.0f, 0.0f), juce::Justification::centredLeft);
    b = b.reduced (12.0f, 8.0f);
    const float mid = b.getCentreY();
    juce::Path upper, lower;
    for (int i = 0; i < numCtrl; ++i)
    {
        smooth[(size_t) i] += (processor.engine.uiArea[(size_t) i].load() - smooth[(size_t) i]) * 0.35f;
        const float x = b.getX() + b.getWidth() * (float) i / (float) (numCtrl - 1);
        const float r = juce::jlimit (0.5f, b.getHeight() * 0.5f, std::sqrt (juce::jmax (0.0f, smooth[(size_t) i])) * b.getHeight() * 0.12f);
        if (i == 0) { upper.startNewSubPath (x, mid - r); lower.startNewSubPath (x, mid + r); }
        else { upper.lineTo (x, mid - r); lower.lineTo (x, mid + r); }
    }
    juce::Path body (upper);
    juce::Path lowRev;
    {
        // closed shape: upper left->right, lower right->left
        juce::Path closed;
        closed.startNewSubPath (b.getX(), mid);
        for (int i = 0; i < numCtrl; ++i)
        {
            const float x = b.getX() + b.getWidth() * (float) i / (float) (numCtrl - 1);
            const float r = juce::jlimit (0.5f, b.getHeight() * 0.5f, std::sqrt (juce::jmax (0.0f, smooth[(size_t) i])) * b.getHeight() * 0.12f);
            closed.lineTo (x, mid - r);
        }
        for (int i = numCtrl; --i >= 0;)
        {
            const float x = b.getX() + b.getWidth() * (float) i / (float) (numCtrl - 1);
            const float r = juce::jlimit (0.5f, b.getHeight() * 0.5f, std::sqrt (juce::jmax (0.0f, smooth[(size_t) i])) * b.getHeight() * 0.12f);
            closed.lineTo (x, mid + r);
        }
        closed.closeSubPath();
        g.setColour (C::accent.withAlpha (0.15f + 0.5f * processor.engine.uiLevel[0].load()));
        g.fillPath (closed);
    }
    g.setColour (C::accent);
    g.strokePath (upper, juce::PathStrokeType (2.0f));
    g.strokePath (lower, juce::PathStrokeType (2.0f));
    juce::ignoreUnused (body, lowRev);
    g.setColour (C::textDim);
    g.setFont (cat::catFont (10.0f));
    g.drawText ("larynx", juce::Rectangle<float> (b.getX(), b.getBottom() - 12.0f, 60.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText ("lips", juce::Rectangle<float> (b.getRight() - 40.0f, b.getBottom() - 12.0f, 40.0f, 12.0f), juce::Justification::centredRight);
}

//==============================================================================
void SingSection::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (C::panelLight);
    g.fillRoundedRectangle (b, 7.0f);
    g.setColour (tint.withAlpha (0.35f));
    g.drawRoundedRectangle (b, 7.0f, 1.0f);
    g.setColour (tint);
    g.setFont (cat::catFont (11.5f, true));
    g.drawText (titleText, b.reduced (10.0f, 4.0f).removeFromTop (14.0f), juce::Justification::centredLeft);
}

void SingSection::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    r.removeFromTop (18);
    if (! left.empty())
    {
        auto col = r.removeFromLeft (leftWidth);
        r.removeFromLeft (8);
        const int h = juce::jmin (28, (col.getHeight() - 4 * ((int) left.size() - 1)) / (int) left.size());
        for (auto* c : left) { c->setBounds (col.removeFromTop (h)); col.removeFromTop (4); }
    }
    if (knobs.empty()) return;
    const int n = (int) knobs.size();
    const int w = juce::jmin (78, r.getWidth() / n);
    auto row = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 90));
    for (auto* c : knobs) c->setBounds (row.removeFromLeft (w).reduced (2, 0));
}

cat::Knob* SingPage::knob (const juce::String& id, const juce::String& caption, juce::Colour c)
{
    return static_cast<cat::Knob*> (owned.add (new cat::Knob (processor.apvts, id, caption, &processor.midiLearn, c)));
}

cat::ParamCombo* SingPage::combo (const juce::String& id, const juce::String& caption, int captionW)
{
    return static_cast<cat::ParamCombo*> (owned.add (new cat::ParamCombo (processor.apvts, id, caption, &processor.midiLearn, captionW)));
}

SingSection* SingPage::section (const juce::String& title, juce::Colour c)
{
    auto* s = sections.add (new SingSection (title, c));
    addAndMakeVisible (s);
    return s;
}

void SingPage::layoutRow (juce::Rectangle<int> area, std::initializer_list<std::pair<juce::Component*, float>> parts)
{
    float total = 0.0f;
    for (auto& p : parts) total += p.second;
    const int gap = 6;
    const int avail = area.getWidth() - gap * ((int) parts.size() - 1);
    for (auto& p : parts)
    {
        const int w = (int) ((float) avail * p.second / total);
        p.first->setBounds (area.removeFromLeft (w));
        area.removeFromLeft (gap);
    }
}

namespace
{
    //==========================================================================
    /** Per-note control: a knob whose lowest position means "use the global setting". */
    struct NoteKnob : public juce::Component
    {
        NoteKnob (const juce::String& caption, double lo, double hi, double autoBelow, std::function<juce::String (double)> fmt)
            : autoThreshold (autoBelow), format (std::move (fmt))
        {
            label.setText (caption, juce::dontSendNotification);
            label.setFont (cat::catFont (11.5f, true));
            label.setColour (juce::Label::textColourId, C::textDim);
            label.setJustificationType (juce::Justification::centred);
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
            slider.setRange (lo, hi, 0.0);
            slider.setColour (juce::Slider::rotarySliderFillColourId, C::purple);
            slider.textFromValueFunction = [this] (double v) { return v < autoThreshold ? juce::String ("Global") : format (v); };
            slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
            addAndMakeVisible (label);
            addAndMakeVisible (slider);
            slider.updateText();
        }
        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromTop (16));
            slider.setBounds (r);
        }
        juce::Label label;
        juce::Slider slider;
        double autoThreshold;
        std::function<juce::String (double)> format;
    };

    //==========================================================================
    class NotePage : public SingPage, private juce::Timer
    {
    public:
        NotePage (TalkCatProcessor& p, PianoRoll& r) : SingPage (p), roll (r)
        {
            for (auto* ed : { &lyricEd, &phonEd })
            {
                ed->setFont (cat::catFont (14.0f, true));
                ed->setColour (juce::TextEditor::backgroundColourId, C::raised);
                ed->setColour (juce::TextEditor::outlineColourId, C::outline);
                addAndMakeVisible (*ed);
            }
            lyricEd.setTextToShowWhenEmpty ("lyric (select a note)", C::textDim);
            phonEd.setTextToShowWhenEmpty ("phonemes, e.g. [l ah v] (empty = from the spelling)", C::textDim);
            lyricEd.setTooltip ("The lyric of the selected note(s). Several words fill the following notes.");
            phonEd.setTooltip ("Exact pronunciation as phonemes. Leave empty to use the spelling + language.");
            lyricEd.onReturnKey = [this] { applyLyric(); };
            lyricEd.onFocusLost = [this] { applyLyric(); };
            phonEd.onReturnKey = [this] { applyPhonemes(); };
            phonEd.onFocusLost = [this] { applyPhonemes(); };

            langBox.addItem ("Plug-in language", 1);
            for (int i = 0; i < 4; ++i) langBox.addItem (languageNames()[i], i + 2);
            langBox.onChange = [this] { if (! loading) apply ([this] (ScoreNote& n) { n.language = langBox.getSelectedId() - 2; }); };
            langBox.setTooltip ("Language of this note (e.g. one English word in a Japanese song)");
            addAndMakeVisible (langBox);

            auto ms = [] (double v) { return juce::String (juce::roundToInt (v)) + " ms"; };
            auto ct = [] (double v) { return juce::String (juce::roundToInt (v)) + " ct"; };
            auto hz = [] (double v) { return juce::String (v, 1) + " Hz"; };
            auto sec = [] (double v) { return juce::String (juce::roundToInt (v * 1000.0)) + " ms"; };
            auto pc = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
            knobs.add (new NoteKnob ("Velocity", 0.05, 1.0, -1.0, pc));
            knobs.add (new NoteKnob ("Portamento", -20.0, 400.0, 0.0, ms));
            knobs.add (new NoteKnob ("Vib Depth", -8.0, 150.0, 0.0, ct));
            knobs.add (new NoteKnob ("Vib Rate", 2.5, 9.0, 3.0, hz));
            knobs.add (new NoteKnob ("Vib Delay", -0.08, 1.5, 0.0, sec));
            knobs.add (new NoteKnob ("Consonants", 0.4, 2.0, -1.0, pc));
            knobs.add (new NoteKnob ("Cons Strength", -0.06, 1.0, 0.0, pc));
            for (int i = 0; i < knobs.size(); ++i)
            {
                addAndMakeVisible (knobs[i]);
                knobs[i]->slider.onValueChange = [this, i] { if (! loading) knobChanged (i); };
                knobs[i]->slider.onDragEnd = [this] { if (pendingCommit) { roll.commit(); pendingCommit = false; } };
            }
            knobs[0]->slider.setTooltip ("How loud / strong this note is sung");
            knobs[1]->slider.setTooltip ("Glide time into this note (Global = the PITCH tab setting)");
            knobs[5]->slider.setTooltip ("Length of the consonants of this note (100% = normal)");

            help.setFont (cat::catFont (11.0f));
            help.setColour (juce::Label::textColourId, C::textDim);
            help.setJustificationType (juce::Justification::topLeft);
            help.setText (phonemeHelp().joinIntoString ("\n"), juce::dontSendNotification);
            addAndMakeVisible (help);
            startTimerHz (10);
            load();
        }

        void load()
        {
            loading = true;
            const auto sel = roll.selectedIndices();
            const bool any = ! sel.empty();
            for (auto* c : std::initializer_list<juce::Component*> { &lyricEd, &phonEd, &langBox }) c->setEnabled (any);
            for (auto* k : knobs) k->setEnabled (any);
            if (any)
            {
                const auto& n = roll.edit.notes[(size_t) sel[0]];
                if (! lyricEd.hasKeyboardFocus (true)) lyricEd.setText (n.lyric, false);
                if (! phonEd.hasKeyboardFocus (true)) phonEd.setText (n.phonemes, false);
                langBox.setSelectedId (n.language + 2, juce::dontSendNotification);
                knobs[0]->slider.setValue (n.velocity, juce::dontSendNotification);
                knobs[1]->slider.setValue (n.portaMs < 0 ? -20.0 : n.portaMs, juce::dontSendNotification);
                knobs[2]->slider.setValue (n.vibDepth < 0 ? -8.0 : n.vibDepth, juce::dontSendNotification);
                knobs[3]->slider.setValue (n.vibRate < 0 ? 2.5 : n.vibRate, juce::dontSendNotification);
                knobs[4]->slider.setValue (n.vibDelay < 0 ? -0.08 : n.vibDelay, juce::dontSendNotification);
                knobs[5]->slider.setValue (n.consScale, juce::dontSendNotification);
                knobs[6]->slider.setValue (n.consStrength < 0 ? -0.06 : n.consStrength, juce::dontSendNotification);
                const auto syls = noteSyllables (n, processor.language());
                juce::StringArray ph;
                for (const auto& s : syls) ph.add (phonemeString (s));
                info = juce::String (sel.size()) + (sel.size() == 1 ? " note" : " notes") + " selected   |   sings: " + ph.joinIntoString (" | ")
                       + "   |   " + cat::cubaseNoteName (n.pitch) + "  " + juce::String (n.length, 2) + " beats";
            }
            else
            {
                lyricEd.setText ({}, false);
                phonEd.setText ({}, false);
                info = "Select a note in the piano roll to edit its lyric, phonemes and expression";
            }
            loading = false;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (C::text);
            g.setFont (cat::catFont (12.0f, true));
            g.drawText (info, getLocalBounds().reduced (12, 6).removeFromTop (18), juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (10, 6);
            r.removeFromTop (22);
            auto left = r.removeFromLeft (360);
            lyricEd.setBounds (left.removeFromTop (30));
            left.removeFromTop (6);
            phonEd.setBounds (left.removeFromTop (30));
            left.removeFromTop (6);
            langBox.setBounds (left.removeFromTop (26).withWidth (200));
            r.removeFromLeft (12);
            auto helpArea = r.removeFromRight (420);
            help.setBounds (helpArea.reduced (4, 0));
            auto row = r.withHeight (juce::jmin (r.getHeight(), 92));
            const int w = juce::jmin (84, row.getWidth() / knobs.size());
            for (auto* k : knobs) k->setBounds (row.removeFromLeft (w).reduced (2, 0));
        }

    private:
        void timerCallback() override
        {
            const auto sel = roll.selectedIndices();
            std::vector<int> ids;
            for (int i : sel) ids.push_back (roll.edit.notes[(size_t) i].id);
            if (ids != lastIds || processor.scoreVersion.load() != lastVersion)
            {
                lastIds = ids;
                lastVersion = processor.scoreVersion.load();
                load();
            }
        }

        void apply (const std::function<void (ScoreNote&)>& fn, bool commitNow = true)
        {
            bool any = false;
            for (auto& n : roll.edit.notes)
                if (roll.selection.count (n.id)) { fn (n); any = true; }
            if (any && commitNow) roll.commit();
            if (any && ! commitNow) { pendingCommit = true; roll.repaint(); }
        }

        void applyLyric()
        {
            if (loading) return;
            const auto text = lyricEd.getText().trim();
            const auto sel = roll.selectedIndices();
            if (sel.empty() || text.isEmpty() || text == roll.edit.notes[(size_t) sel[0]].lyric) return;
            const auto lang = processor.language();
            if (textToSyllables (text, lang).size() > 1 && (text.containsChar (' ') || text.containsChar ('-')))
            {
                roll.edit.fillLyrics (text, sel[0], lang);
                roll.commit();
            }
            else
                apply ([text] (ScoreNote& n) { n.lyric = text; n.phonemes = {}; });
            if (! sel.empty() && sel[0] < (int) roll.edit.notes.size())
            {
                const auto& n = roll.edit.notes[(size_t) sel[0]];
                processor.preview (n.pitch, n.phonemes.isNotEmpty() ? n.phonemes : n.lyric, n.language);
            }
        }

        void applyPhonemes()
        {
            if (loading) return;
            auto text = phonEd.getText().trim();
            const auto sel = roll.selectedIndices();
            if (sel.empty() || text == roll.edit.notes[(size_t) sel[0]].phonemes) return;
            if (text.isNotEmpty() && ! text.startsWith ("[")) text = "[" + text + "]";
            apply ([text] (ScoreNote& n) { n.phonemes = text; });
            const auto& n = roll.edit.notes[(size_t) sel[0]];
            processor.preview (n.pitch, n.phonemes.isNotEmpty() ? n.phonemes : n.lyric, n.language);
        }

        void knobChanged (int i)
        {
            const double v = knobs[i]->slider.getValue();
            const bool isAuto = v < knobs[i]->autoThreshold;
            apply ([i, v, isAuto] (ScoreNote& n)
            {
                switch (i)
                {
                    case 0: n.velocity = (float) v; break;
                    case 1: n.portaMs = isAuto ? -1.0f : (float) v; break;
                    case 2: n.vibDepth = isAuto ? -1.0f : (float) v; break;
                    case 3: n.vibRate = isAuto ? -1.0f : (float) v; break;
                    case 4: n.vibDelay = isAuto ? -1.0f : (float) v; break;
                    case 5: n.consScale = (float) v; break;
                    case 6: n.consStrength = isAuto ? -1.0f : (float) v; break;
                    default: break;
                }
            }, false);
        }

        PianoRoll& roll;
        juce::TextEditor lyricEd, phonEd;
        juce::ComboBox langBox;
        juce::OwnedArray<NoteKnob> knobs;
        juce::Label help;
        juce::String info;
        bool loading = false, pendingCommit = false;
        std::vector<int> lastIds;
        uint32_t lastVersion = 0;
    };

    //==========================================================================
    class LyricsPage : public SingPage
    {
    public:
        LyricsPage (TalkCatProcessor& p, PianoRoll& r) : SingPage (p), roll (r)
        {
            text.setMultiLine (true, true);
            text.setReturnKeyStartsNewLine (true);
            text.setFont (cat::catFont (15.0f));
            text.setColour (juce::TextEditor::backgroundColourId, C::raised);
            text.setTextToShowWhenEmpty ("Type or paste the lyrics of the song here, then press Fill. One syllable per note; "
                                         "\"-\" holds a syllable over the next note, \"br\" = breath, [l ah v] = phonemes.", C::textDim);
            addAndMakeVisible (text);
            fillSel.setTooltip ("Put the lyrics on the notes, starting at the first selected note");
            fillAll.setTooltip ("Put the lyrics on all notes from the start of the song");
            readBack.setTooltip ("Collect the lyrics of the notes into this box");
            fillSel.onClick = [this] { fill (false); };
            fillAll.onClick = [this] { fill (true); };
            readBack.onClick = [this]
            {
                juce::StringArray words;
                for (const auto& n : roll.edit.notes) words.add (n.lyric);
                text.setText (words.joinIntoString (" "), false);
                update();
            };
            text.onTextChange = [this] { update(); };
            readBack.onClick();          // start with the song's current lyrics
            for (auto* b : { &fillSel, &fillAll, &readBack }) addAndMakeVisible (*b);
            countLabel.setColour (juce::Label::textColourId, C::textDim);
            countLabel.setFont (cat::catFont (12.0f));
            addAndMakeVisible (countLabel);
            update();
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (10, 8);
            auto side = r.removeFromRight (230);
            r.removeFromRight (8);
            text.setBounds (r);
            fillSel.setBounds (side.removeFromTop (30));
            side.removeFromTop (6);
            fillAll.setBounds (side.removeFromTop (30));
            side.removeFromTop (6);
            readBack.setBounds (side.removeFromTop (30));
            side.removeFromTop (8);
            countLabel.setBounds (side);
        }
    private:
        void update()
        {
            const auto syls = textToSyllables (text.getText(), processor.language());
            juce::String preview;
            for (size_t i = 0; i < syls.size() && i < 24; ++i) preview << juce::String::fromUTF8 (syls[i].text) << (i + 1 < syls.size() ? " . " : "");
            countLabel.setText (juce::String ((int) syls.size()) + " syllables for " + juce::String ((int) roll.edit.notes.size()) + " notes ("
                                + languageNames()[(int) processor.language()] + ")\n\n" + preview, juce::dontSendNotification);
        }
        void fill (bool all)
        {
            const auto sel = roll.selectedIndices();
            const int first = all || sel.empty() ? 0 : sel[0];
            roll.edit.fillLyrics (text.getText(), first, processor.language());
            roll.commit();
        }
        PianoRoll& roll;
        juce::TextEditor text;
        juce::TextButton fillSel { "Fill from selected note" }, fillAll { "Fill all notes" }, readBack { "Read lyrics from notes" };
        juce::Label countLabel;
    };

    //==========================================================================
    class VoicePage : public SingPage
    {
    public:
        explicit VoicePage (TalkCatProcessor& p) : SingPage (p), tract (p)
        {
            body = section ("BODY", C::accent);
            for (auto [id, cap] : { std::pair { "size", "Size" }, { "gender", "Gender" }, { "age", "Age" }, { "cat", "Cat" } })
                body->addKnob (knob (id, cap, C::accent));
            source = section ("VOICE SOURCE", C::teal);
            for (auto [id, cap] : { std::pair { "breath", "Breath" }, { "tension", "Tension" }, { "rasp", "Rasp" }, { "jitter", "Rough" }, { "ring", "Ring" } })
                source->addKnob (knob (id, cap, C::teal));
            shape = section ("SHAPE", C::purple);
            for (auto [id, cap] : { std::pair { "nasal", "Nasal" }, { "jaw", "Jaw" }, { "lips", "Lips" }, { "tongue", "Tongue" }, { "throat", "Throat" }, { "bright", "Bright" } })
                shape->addKnob (knob (id, cap, C::purple));
            addAndMakeVisible (tract);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            tract.setBounds (r.removeFromRight (300));
            r.removeFromRight (6);
            auto top = r.removeFromTop (r.getHeight() / 2);
            r.removeFromTop (6);
            layoutRow (top, { { body, 4.0f }, { source, 5.0f } });
            shape->setBounds (r);
        }
    private:
        SingSection* body = nullptr;
        SingSection* source = nullptr;
        SingSection* shape = nullptr;
        TractView tract;
    };

    class PitchPage : public SingPage
    {
    public:
        explicit PitchPage (TalkCatProcessor& p) : SingPage (p)
        {
            moves = section ("PITCH MOVES", C::accent);
            for (auto [id, cap] : { std::pair { "porta", "Portamento" }, { "overshoot", "Overshoot" }, { "scoop", "Scoop" }, { "prep", "Prepare" },
                                    { "fall", "Phrase Fall" }, { "drift", "Drift" } })
                moves->addKnob (knob (id, cap, C::accent));
            vib = section ("VIBRATO", C::teal);
            for (auto [id, cap] : { std::pair { "vibDepth", "Depth" }, { "vibRate", "Rate" }, { "vibDelay", "Delay" } })
                vib->addKnob (knob (id, cap, C::teal));
            diction = section ("DICTION", C::purple);
            diction->leftWidth = 150;
            diction->addLeft (combo ("attack", "Attack", 50));
            for (auto [id, cap] : { std::pair { "consLength", "Cons Length" }, { "consStrength", "Cons Strength" }, { "release", "Release" }, { "autoBreath", "Auto Breath" } })
                diction->addKnob (knob (id, cap, C::purple));
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto top = r.removeFromTop (r.getHeight() / 2);
            r.removeFromTop (6);
            layoutRow (top, { { moves, 6.0f }, { vib, 3.0f } });
            diction->setBounds (r.withWidth (juce::jmin (r.getWidth(), 560)));
        }
    private:
        SingSection* moves = nullptr;
        SingSection* vib = nullptr;
        SingSection* diction = nullptr;
    };

    class HarmonyPage : public SingPage
    {
    public:
        explicit HarmonyPage (TalkCatProcessor& p) : SingPage (p)
        {
            key = section ("KEY (for the harmonies)", C::slate);
            key->leftWidth = 200;
            key->addLeft (combo ("key", "Key", 50));
            key->addLeft (combo ("scale", "Scale", 50));
            dbl = section ("DOUBLE TRACK", C::accent);
            for (auto [id, cap] : { std::pair { "dblLevel", "Level" }, { "dblDetune", "Detune" }, { "dblDelay", "Delay" }, { "width", "Width" } })
                dbl->addKnob (knob (id, cap, C::accent));
            for (int h = 1; h <= 3; ++h)
            {
                auto* s = section ("HARMONY " + juce::String (h), C::palette (h));
                s->leftWidth = 140;
                s->addLeft (combo ("h" + juce::String (h), "", 0));
                s->addKnob (knob ("h" + juce::String (h) + "Level", "Level", C::palette (h)));
                s->addKnob (knob ("h" + juce::String (h) + "Pan", "Pan", C::palette (h)));
                harm.push_back (s);
            }
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto top = r.removeFromTop (r.getHeight() / 2);
            r.removeFromTop (6);
            layoutRow (top, { { key, 3.0f }, { dbl, 4.0f } });
            layoutRow (r, { { harm[0], 1.0f }, { harm[1], 1.0f }, { harm[2], 1.0f } });
        }
    private:
        SingSection* key = nullptr;
        SingSection* dbl = nullptr;
        std::vector<SingSection*> harm;
    };

    class FxPage : public SingPage
    {
    public:
        explicit FxPage (TalkCatProcessor& p) : SingPage (p)
        {
            tone = section ("TONE", C::accent);
            for (auto [id, cap] : { std::pair { "lowCut", "Low Cut" }, { "presence", "Presence" }, { "air", "Air" } })
                tone->addKnob (knob (id, cap, C::accent));
            dyn = section ("DYNAMICS", C::teal);
            for (auto [id, cap] : { std::pair { "comp", "Compress" }, { "drive", "Drive" }, { "volume", "Volume" } })
                dyn->addKnob (knob (id, cap, C::teal));
            delay = section ("DELAY", C::purple);
            delay->leftWidth = 130;
            delay->addLeft (combo ("delayTime", "", 0));
            for (auto [id, cap] : { std::pair { "delayMix", "Mix" }, { "delayFb", "Feedback" } })
                delay->addKnob (knob (id, cap, C::purple));
            reverb = section ("REVERB", C::slate);
            reverb->leftWidth = 130;
            reverb->addLeft (combo ("reverb", "", 0));
            for (auto [id, cap] : { std::pair { "reverbMix", "Mix" }, { "reverbSize", "Size" } })
                reverb->addKnob (knob (id, cap, C::slate));
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto top = r.removeFromTop (r.getHeight() / 2);
            r.removeFromTop (6);
            layoutRow (top, { { tone, 1.0f }, { dyn, 1.0f } });
            layoutRow (r, { { delay, 1.0f }, { reverb, 1.0f } });
        }
    private:
        SingSection* tone = nullptr;
        SingSection* dyn = nullptr;
        SingSection* delay = nullptr;
        SingSection* reverb = nullptr;
    };

    class LivePage : public SingPage, private juce::Timer
    {
    public:
        explicit LivePage (TalkCatProcessor& p) : SingPage (p)
        {
            text.setMultiLine (true, true);
            text.setReturnKeyStartsNewLine (true);
            text.setFont (cat::catFont (15.0f));
            text.setColour (juce::TextEditor::backgroundColourId, C::raised);
            text.setText (p.getLiveLyrics(), false);
            text.setTooltip ("Live lyrics: every MIDI note you play sings the next syllable (loops at the end)");
            text.onTextChange = [this] { dirty = true; };
            addAndMakeVisible (text);
            restart.onClick = [this] { processor.restartLiveLyrics(); };
            restart.setTooltip ("Start the live lyrics from the first syllable again (also happens when Cubase starts playing)");
            record.setClickingTogglesState (true);
            record.setTooltip ("Record: notes you play while Cubase plays are written into the piano roll (with their live syllable)");
            record.onClick = [this] { processor.recordArmed = record.getToggleState(); };
            for (auto* b : { &restart, &record }) addAndMakeVisible (*b);
            bend = knob ("bendRange", "Bend Range", C::slate);
            addAndMakeVisible (bend);
            now.setFont (cat::catFont (22.0f, true));
            now.setColour (juce::Label::textColourId, C::accent);
            now.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (now);
            startTimerHz (15);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (10, 8);
            auto side = r.removeFromRight (260);
            r.removeFromRight (8);
            text.setBounds (r);
            now.setBounds (side.removeFromTop (40));
            auto row = side.removeFromTop (30);
            restart.setBounds (row.removeFromLeft (124));
            row.removeFromLeft (8);
            record.setBounds (row);
            side.removeFromTop (8);
            bend->setBounds (side.removeFromTop (90).withWidth (90));
        }
    private:
        void timerCallback() override
        {
            if (dirty && ! text.isMouseButtonDown()) { processor.setLiveLyrics (text.getText()); dirty = false; }
            const int idx = processor.uiLiveSyllable.load();
            const auto syls = textToSyllables (processor.getLiveLyrics(), processor.language());
            now.setText (idx >= 0 && idx < (int) syls.size() ? juce::String::fromUTF8 (syls[(size_t) idx].text) : juce::String ("-"), juce::dontSendNotification);
            record.setToggleState (processor.recordArmed.load(), juce::dontSendNotification);
        }
        juce::TextEditor text;
        juce::TextButton restart { "Restart lyrics" }, record { "Record to roll" };
        cat::Knob* bend = nullptr;
        juce::Label now;
        bool dirty = false;
    };
}

//==============================================================================
TalkCatEditor::TalkCatEditor (TalkCatProcessor& p)
    : processor (p),
      presetBar (p.presetManager, "Voice"),
      mode (p.apvts, "mode", "", &p.midiLearn, 0),
      languageBox (p.apvts, "language", "", &p.midiLearn, 0),
      styleBox (p.apvts, "style", "", &p.midiLearn, 0),
      roll (p),
      lanes (p, roll),
      catView (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, cat::CatStyle::Talk),
      surpriseControls (p, p.surpriseUndo, [&p] { p.surprise(); }),
      bootScreen (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); },
                  "TALK", "-cat", "A SINGER FOR PEOPLE AND CATS", cat::CatStyle::Talk)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    songsButton.setTooltip ("Demo songs, new song, import / export a MIDI file (with lyrics)");
    songsButton.onClick = [this] { showSongsMenu(); };
    undoButton.onClick = [this] { processor.undo(); roll.reload(); };
    redoButton.onClick = [this] { processor.redo(); roll.reload(); };
    mode.getCombo().setTooltip ("Score = sing the piano roll in sync with Cubase; Live = sing what you play (MIDI); or both");
    languageBox.getCombo().setTooltip ("Language of the lyrics: English, Slovak, Japanese (romaji) or Cat");
    styleBox.getCombo().setTooltip ("Singing style: how the pitch moves (scoops, vibrato, falls) and the voice colour");
    dragButton.setTooltip ("Drag the melody (with lyrics) onto a Cubase MIDI track");
    dragButton.onDragOut = [this]
    {
        auto seq = processor.getScore().toMidi();
        for (int i = 0; i < seq.getNumEvents(); ++i) seq.getEventPointer (i)->message.setTimeStamp (seq.getEventTime (i) / 960.0);
        if (seq.getNumEvents() == 0) return;
        cat::midiexport::dragFileOut (cat::midiexport::writeMidiFile (seq, processor.getHostBpm(), "Talk-Cat Melody"), this);
    };

    const char* toolNames[] = { "Draw", "Select", "Erase" };
    const char* toolTips[] = { "Draw: click to add notes, drag notes to move, drag their end to resize",
                               "Select: drag a box around notes, then move / copy / delete them",
                               "Erase: click or drag over notes to delete them" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = toolButtons[(size_t) i];
        b.setButtonText (toolNames[i]);
        b.setTooltip (toolTips[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (8123);
        b.onClick = [this, i] { if (toolButtons[(size_t) i].getToggleState()) roll.tool = i; };
        addAndMakeVisible (b);
    }
    toolButtons[0].setToggleState (true, juce::dontSendNotification);

    const char* snaps[] = { "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T", "Off" };
    for (int i = 0; i < 7; ++i) snapBox.addItem (juce::String ("Snap ") + snaps[i], i + 1);
    snapBox.setSelectedId (3, juce::dontSendNotification);
    roll.snap = 0.25;
    snapBox.onChange = [this]
    {
        static const double v[] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0, 0.0 };
        roll.snap = v[juce::jlimit (0, 6, snapBox.getSelectedId() - 1)];
        roll.repaint();
    };
    snapBox.setTooltip ("Grid for drawing and moving notes");
    zoomIn.onClick = [this] { roll.zoomTime (1.25f); };
    zoomOut.onClick = [this] { roll.zoomTime (0.8f); };
    rowsIn.onClick = [this] { roll.zoomPitch (1.2f); };
    rowsOut.onClick = [this] { roll.zoomPitch (1.0f / 1.2f); };
    zoomIn.setTooltip ("Zoom in (time)");
    zoomOut.setTooltip ("Zoom out (time)");
    rowsIn.setTooltip ("Taller rows");
    rowsOut.setTooltip ("Smaller rows");

    barsStepper = std::make_unique<cat::Stepper> ("Bars", 1, 256, [this] { return (int) std::ceil (roll.edit.lengthBeats / 4.0); },
                                                  [this] (int v) { roll.edit.lengthBeats = v * 4.0; roll.commit(); });
    barsStepper->setTooltip ("Length of the song in bars");
    startStepper = std::make_unique<cat::Stepper> ("Start", 1, 999, [this] { return roll.edit.startBar; },
                                                   [this] (int v) { roll.edit.startBar = v; roll.commit(); },
                                                   [] (int v) { return "Bar " + juce::String (v); });
    startStepper->setTooltip ("Where in the Cubase project the song starts");
    loopToggle.onClick = [this] { roll.edit.loop = loopToggle.getToggleState(); roll.commit(); };
    loopToggle.setTooltip ("Repeat the song over and over (for loops / jamming)");
    followToggle.setToggleState (true, juce::dontSendNotification);
    followToggle.onClick = [this] { roll.follow = followToggle.getToggleState(); };
    followToggle.setTooltip ("Scroll with the playhead");
    recordButton.setClickingTogglesState (true);
    recordButton.setTooltip ("Record notes you play (MIDI) into the piano roll while Cubase plays");
    recordButton.onClick = [this] { processor.recordArmed = recordButton.getToggleState(); };
    infoLabel.setFont (cat::catFont (12.0f));
    infoLabel.setColour (juce::Label::textColourId, C::textDim);
    infoLabel.setJustificationType (juce::Justification::centredRight);

    roll.onViewChanged = [this] { lanes.repaint(); };

    tabs.addTab ("NOTE", C::panel, new NotePage (p, roll), true);
    tabs.addTab ("LYRICS", C::panel, new LyricsPage (p, roll), true);
    tabs.addTab ("VOICE", C::panel, new VoicePage (p), true);
    tabs.addTab ("PITCH + DICTION", C::panel, new PitchPage (p), true);
    tabs.addTab ("HARMONY", C::panel, new HarmonyPage (p), true);
    tabs.addTab ("FX", C::panel, new FxPage (p), true);
    tabs.addTab ("LIVE", C::panel, new LivePage (p), true);
    tabs.setTabBarDepth (28);
    tabs.setOutline (0);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &songsButton, &mode, &languageBox, &styleBox, &undoButton, &redoButton,
             &dragButton, &snapBox, &zoomOut, &zoomIn, &rowsOut, &rowsIn, barsStepper.get(), startStepper.get(), &loopToggle, &followToggle,
             &recordButton, &infoLabel, &roll, &lanes, &tabs })
        addAndMakeVisible (c);
    addAndMakeVisible (catView);
    addAndMakeVisible (surpriseControls);
    addAndMakeVisible (bootScreen);
    setSize (1280, 880);
    startTimerHz (8);
}

TalkCatEditor::~TalkCatEditor()
{
    setLookAndFeel (nullptr);
}

void TalkCatEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::background);
    cat::CatLookAndFeel::drawLogo (g, juce::Rectangle<float> (14.0f, 8.0f, 150.0f, 32.0f), "TALK", "-cat");
}

void TalkCatEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto item = [] (juce::Component& c, float w, float h = 28.0f) { return juce::FlexItem (c).withWidth (w).withHeight (h).withMargin ({ 0, 3, 0, 3 }); };
    auto row = [] (juce::Rectangle<int> area, std::initializer_list<juce::FlexItem> items)
    {
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        for (auto& it : items) fb.items.add (it);
        fb.performLayout (area);
    };
    {
        auto top = r.removeFromTop (40);
        auto logo = top.removeFromLeft (150);
        catView.setBounds (logo.removeFromRight (40).expanded (0, 6));
        row (top, { item (presetBar, 300), item (songsButton, 64), item (mode, 120), item (languageBox, 104), item (styleBox, 124),
                    juce::FlexItem().withFlex (1.0f), item (undoButton, 50), item (redoButton, 50), item (surpriseControls, 122), item (dragButton, 88) });
    }
    r.removeFromTop (6);
    row (r.removeFromTop (28), { item (toolButtons[0], 56), item (toolButtons[1], 60), item (toolButtons[2], 56), item (snapBox, 104),
                                 item (zoomOut, 26), item (zoomIn, 26), item (rowsOut, 26), item (rowsIn, 26), item (*barsStepper, 100),
                                 item (*startStepper, 120), item (loopToggle, 60), item (followToggle, 70), item (recordButton, 48),
                                 item (infoLabel, 200).withFlex (1.0f) });
    r.removeFromTop (6);
    tabs.setBounds (r.removeFromBottom (236));
    r.removeFromBottom (6);
    lanes.setBounds (r.removeFromBottom (92));
    r.removeFromBottom (4);
    roll.setBounds (r);
    bootScreen.setBounds (getLocalBounds());
}

bool TalkCatEditor::keyPressed (const juce::KeyPress& k)
{
    return roll.keyPressed (k);
}

void TalkCatEditor::timerCallback()
{
    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());
    loopToggle.setToggleState (roll.edit.loop, juce::dontSendNotification);
    recordButton.setToggleState (processor.recordArmed.load(), juce::dontSendNotification);
    const double beat = processor.uiScoreBeat.load();
    infoLabel.setText (juce::String (roll.edit.notes.size()) + " notes   " + juce::String (processor.getHostBpm(), 1) + " BPM"
                       + (beat >= 0.0 ? "   bar " + juce::String ((int) (beat / 4.0) + roll.edit.startBar) + "." + juce::String ((int) std::fmod (beat, 4.0) + 1) : juce::String()),
                       juce::dontSendNotification);
}

void TalkCatEditor::showSongsMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Demo songs");
    const auto& demos = TalkCatProcessor::demoSongs();
    for (int i = 0; i < (int) demos.size(); ++i) m.addItem (100 + i, juce::String::fromUTF8 (demos[(size_t) i].name));
    m.addSeparator();
    m.addItem (1, "New empty song");
    m.addItem (2, "Import MIDI file (notes + lyrics) ...");
    m.addItem (3, "Export MIDI file ...");
    m.addItem (4, "Set all lyrics to \"la\"");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&songsButton),
                     [sp = juce::Component::SafePointer<TalkCatEditor> (this)] (int r)
    {
        if (sp == nullptr || r == 0) return;
        auto& proc = sp->processor;
        if (r >= 100) { proc.loadDemoSong (r - 100); sp->roll.reload(); return; }
        if (r == 1) { Score s; proc.setScore (s); sp->roll.reload(); }
        if (r == 2) sp->importMidi();
        if (r == 3)
        {
            sp->chooser = std::make_unique<juce::FileChooser> ("Export the melody", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Talk-Cat Melody.mid"), "*.mid");
            sp->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                      [sp] (const juce::FileChooser& fc)
            {
                if (sp == nullptr) return;
                const auto f = fc.getResult();
                if (f == juce::File()) return;
                f.deleteFile();
                juce::FileOutputStream os (f);
                if (os.openedOk()) sp->processor.exportMidi().writeTo (os);
            });
        }
        if (r == 4)
        {
            auto s = proc.getScore();
            for (auto& n : s.notes) { n.lyric = "la"; n.phonemes = {}; }
            proc.setScore (s);
            sp->roll.reload();
        }
    });
}

void TalkCatEditor::importMidi()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a MIDI file", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [sp = juce::Component::SafePointer<TalkCatEditor> (this)] (const juce::FileChooser& fc)
    {
        if (sp == nullptr) return;
        const auto f = fc.getResult();
        juce::FileInputStream in (f);
        juce::MidiFile mf;
        if (! in.openedOk() || ! mf.readFrom (in)) return;
        auto s = Score::fromMidi (mf);
        sp->processor.setScore (s);
        sp->roll.reload();
    });
}

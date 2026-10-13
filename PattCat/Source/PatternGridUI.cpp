#include "PatternGridUI.h"
#include "PluginProcessor.h"
#include "CatLookAndFeel.h"

using namespace pattcat;
namespace C = cat::Colours;

//==============================================================================
double PatternOverview::displayLength() const
{
    double len = ctx.barLength;
    for (int l = 0; l < 4; ++l)
        if (ctx.engine.isLayerAudible (l))
            len = std::max (len, ctx.engine.getCycleLengthPpq (l, ctx.barLength));
    return std::min (len, 4.0 * ctx.barLength);
}

void PatternOverview::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colour (0xff111318));
    g.fillRoundedRectangle (r, 6.0f);

    const double len = displayLength();
    const int lanes = juce::jmax (3, chordSize);
    auto area = r.reduced (10.0f, 8.0f);
    const float laneH = area.getHeight() / (float) lanes;

    // Beat grid
    for (double b = 0.0; b <= len + 1.0e-6; b += 1.0)
    {
        const float x = area.getX() + (float) (b / len) * area.getWidth();
        const bool barLine = std::fmod (b, ctx.barLength) < 1.0e-6;
        g.setColour (barLine ? C::outline : C::outline.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }

    g.setColour (C::textDim);
    g.setFont (cat::catFont (11.0f));
    g.drawText (juce::String (len / ctx.barLength, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " bar",
                r.reduced (8.0f, 4.0f), juce::Justification::topRight);

    juce::Random rng (1);
    std::vector<int> chord ((size_t) lanes);
    for (int i = 0; i < lanes; ++i) chord[(size_t) i] = i;
    std::vector<int> voices;

    ctx.engine.collectHits (0.0, len, ctx.barLength, 0, 0.0f, rng, false, [&] (const PatternHit& h)
    {
        PattCatEngine::selectVoices (chord, h.voiceIndex, voices);
        const auto colour = cat::Colours::palette (h.layer == 0 ? 0 : h.layer == 1 ? 3 : h.layer == 2 ? 1 : 7);
        const float x = area.getX() + (float) (h.ppq / len) * area.getWidth();
        const float w = juce::jmax (3.0f, (float) (h.lengthPpq / len) * area.getWidth() - 1.0f);

        for (int v : voices)
        {
            const float y = area.getBottom() - (float) (v + 1) * laneH + laneH * 0.2f;
            g.setColour (colour.withAlpha (0.35f + 0.6f * juce::jlimit (0.0f, 1.0f, h.velocity) * h.probability));
            g.fillRoundedRectangle (x, y, juce::jmin (w, area.getRight() - x), laneH * 0.6f, 2.0f);
        }
    });

    if (ctx.playheadPpq >= 0.0)
    {
        const double pos = std::fmod (ctx.playheadPpq, len);
        const float x = area.getX() + (float) (pos / len) * area.getWidth();
        g.setColour (C::accent);
        g.fillRect (x - 1.0f, area.getY() - 4.0f, 2.0f, area.getHeight() + 8.0f);
    }
}

//==============================================================================
juce::Rectangle<float> StepGrid::laneBounds (Lane lane) const
{
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    r.removeFromLeft (64.0f);
    const float h = r.getHeight();
    auto steps = r.removeFromTop (h * 0.42f);
    r.removeFromTop (6.0f);
    auto vel = r.removeFromTop ((h * 0.58f - 12.0f) * 0.5f);
    r.removeFromTop (6.0f);
    switch (lane)
    {
        case Lane::Steps:    return steps;
        case Lane::Velocity: return vel;
        case Lane::Gate:     return r;
        case Lane::None:
        default:             return {};
    }
}

StepGrid::Lane StepGrid::laneAt (float y) const
{
    for (auto l : { Lane::Steps, Lane::Velocity, Lane::Gate })
        if (y >= laneBounds (l).getY() - 3.0f && y <= laneBounds (l).getBottom() + 3.0f)
            return l;
    return Lane::None;
}

int StepGrid::stepAt (float x) const
{
    const auto r = laneBounds (Lane::Steps);
    return juce::jlimit (0, 31, (int) ((x - r.getX()) / (r.getWidth() / 32.0f)));
}

void StepGrid::paint (juce::Graphics& g)
{
    const auto& layer = ctx.engine.layers[(size_t) ctx.engine.currentLayer];
    const auto colour = cat::Colours::palette (ctx.engine.currentLayer == 0 ? 0 : ctx.engine.currentLayer == 1 ? 3 : ctx.engine.currentLayer == 2 ? 1 : 7);
    const int playStep = ctx.playheadSteps[(size_t) ctx.engine.currentLayer];

    auto full = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (C::textDim);
    g.setFont (cat::catFont (11.0f, true));
    g.drawText ("STEPS", full.withWidth (60.0f).withY (laneBounds (Lane::Steps).getY()).withHeight (laneBounds (Lane::Steps).getHeight()), juce::Justification::centredLeft);
    g.drawText ("VELOCITY", full.withWidth (60.0f).withY (laneBounds (Lane::Velocity).getY()).withHeight (laneBounds (Lane::Velocity).getHeight()), juce::Justification::centredLeft);
    g.drawText ("GATE", full.withWidth (60.0f).withY (laneBounds (Lane::Gate).getY()).withHeight (laneBounds (Lane::Gate).getHeight()), juce::Justification::centredLeft);

    const auto steps = laneBounds (Lane::Steps);
    const float w = steps.getWidth() / 32.0f;

    for (int s = 0; s < 32; ++s)
    {
        const auto& st = layer.steps[(size_t) s];
        const bool inRange = s < layer.length;
        const float x = steps.getX() + s * w;

        // Step cell
        auto cell = juce::Rectangle<float> (x + 1.5f, steps.getY(), w - 3.0f, steps.getHeight());
        juce::Colour fill = (s / 4) % 2 == 0 ? juce::Colour (0xff1a1d24) : juce::Colour (0xff15171c);
        if (inRange && st.active && ! st.isRest)
            fill = st.isTie ? colour.withAlpha (0.45f) : colour.withAlpha (0.35f + 0.5f * juce::jlimit (0.0f, 1.0f, st.velocity / 1.5f) * 1.4f);
        if (! inRange) fill = fill.withAlpha (0.35f);
        g.setColour (fill);
        g.fillRoundedRectangle (cell, 3.0f);

        if (inRange && st.active)
        {
            g.setColour (C::text.withAlpha (0.9f));
            g.setFont (cat::catFont (10.0f, true));
            juce::String tag;
            if (st.isRest) tag = "R";
            else if (st.isTie) tag = "~";
            else
            {
                if (st.voiceIndex == VoiceTop) tag = "T";
                else if (st.voiceIndex == VoiceBottom) tag = "B";
                else if (st.voiceIndex >= 0) tag = juce::String (st.voiceIndex + 1);
                if (st.ratchet > 1) tag << "x" << st.ratchet;
                if (st.octaveOffset != 0) tag << (st.octaveOffset > 0 ? "+" : "") << st.octaveOffset;
            }
            g.drawFittedText (tag, cell.toNearestInt().reduced (1, 2), juce::Justification::centredTop, 2, 0.6f);
            if (st.probability < 1.0f)
            {
                g.setColour (C::accent);
                g.drawText (juce::String (juce::roundToInt (st.probability * 100.0f)) + "%", cell.toNearestInt().reduced (0, 2),
                            juce::Justification::centredBottom);
            }
        }

        if (s == ctx.selectedStep)
        {
            g.setColour (C::accent);
            g.drawRoundedRectangle (cell, 3.0f, 1.5f);
        }
        if (s == playStep)
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRoundedRectangle (cell, 3.0f);
        }

        // Velocity / gate bars
        auto drawBar = [&] (juce::Rectangle<float> lane, float value, float maxValue)
        {
            auto bg = juce::Rectangle<float> (x + 1.5f, lane.getY(), w - 3.0f, lane.getHeight());
            g.setColour (juce::Colour (0xff15171c).withAlpha (inRange ? 1.0f : 0.4f));
            g.fillRoundedRectangle (bg, 2.0f);
            if (! inRange || ! st.active || st.isRest) return;
            const float h = lane.getHeight() * juce::jlimit (0.0f, 1.0f, value / maxValue);
            g.setColour (colour.withAlpha (0.85f));
            g.fillRoundedRectangle (bg.withTop (bg.getBottom() - h), 2.0f);
            // 100 % reference line
            const float ref = lane.getBottom() - lane.getHeight() * (1.0f / maxValue);
            g.setColour (C::outline);
            g.drawHorizontalLine ((int) ref, bg.getX(), bg.getRight());
        };
        drawBar (laneBounds (Lane::Velocity), st.velocity, 1.5f);
        drawBar (laneBounds (Lane::Gate), st.gate, 1.5f);
    }
}

void StepGrid::edit (const std::function<void (PatternLayer&)>& fn)
{
    const int layer = ctx.engine.currentLayer;
    fn (ctx.engine.layers[(size_t) layer]);
    ctx.processor.editEngine ([fn, layer] (PattCatEngine& e) { fn (e.layers[(size_t) layer]); });
    repaint();
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int step = stepAt (e.position.x);
    dragLane = laneAt (e.position.y);
    lastPaintedStep = -1;

    ctx.selectedStep = step;
    if (ctx.onSelectionChanged) ctx.onSelectionChanged();

    if (e.mods.isPopupMenu())
    {
        dragLane = Lane::None;
        showStepMenu (step);
        return;
    }

    if (dragLane == Lane::Steps)
    {
        const auto& st = ctx.engine.layers[(size_t) ctx.engine.currentLayer].steps[(size_t) step];
        paintValue = ! st.active;
        // Clicking past the end extends the layer
        edit ([step, value = paintValue] (PatternLayer& l)
        {
            if (step >= l.length) l.length = step + 1;
            l.steps[(size_t) step].active = value;
            if (value) { l.steps[(size_t) step].isRest = false; }
        });
        lastPaintedStep = step;
        return;
    }

    mouseDrag (e);
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    const int step = stepAt (e.position.x);

    if (dragLane == Lane::Steps)
    {
        if (step != lastPaintedStep)
        {
            edit ([step, value = paintValue] (PatternLayer& l)
            {
                if (step < l.length) l.steps[(size_t) step].active = value;
            });
            lastPaintedStep = step;
        }
        return;
    }

    if (dragLane == Lane::Velocity || dragLane == Lane::Gate)
    {
        const auto lane = laneBounds (dragLane);
        const float value = juce::jlimit (0.0f, 1.5f, (lane.getBottom() - e.position.y) / lane.getHeight() * 1.5f);
        const bool velocity = dragLane == Lane::Velocity;
        edit ([step, value, velocity] (PatternLayer& l)
        {
            auto& st = l.steps[(size_t) step];
            if (velocity) st.velocity = value;
            else st.gate = juce::jmax (0.1f, value);
        });
        ctx.selectedStep = step;
        if (ctx.onSelectionChanged) ctx.onSelectionChanged();
    }
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    dragLane = Lane::None;
}

void StepGrid::showStepMenu (int step)
{
    const auto& st = ctx.engine.layers[(size_t) ctx.engine.currentLayer].steps[(size_t) step];
    juce::PopupMenu m;
    m.addSectionHeader ("Step " + juce::String (step + 1));
    m.addItem (1, "Tie", true, st.isTie);
    m.addItem (2, "Rest", true, st.isRest);
    m.addItem (3, "Octave +1");
    m.addItem (4, "Octave -1");
    m.addItem (5, "Octave reset", st.octaveOffset != 0);

    juce::PopupMenu ratchet;
    for (int r = 1; r <= 4; ++r) ratchet.addItem (10 + r, r == 1 ? juce::String ("Off") : juce::String (r) + "x", true, st.ratchet == r);
    m.addSubMenu ("Ratchet", ratchet);

    juce::PopupMenu voice;
    voice.addItem (20, "All voices", true, st.voiceIndex == VoiceAll);
    voice.addItem (21, "Top voice", true, st.voiceIndex == VoiceTop);
    voice.addItem (22, "Bottom voice", true, st.voiceIndex == VoiceBottom);
    for (int v = 0; v < 6; ++v) voice.addItem (30 + v, "Voice " + juce::String (v + 1), true, st.voiceIndex == v);
    m.addSubMenu ("Voice Select", voice);

    juce::PopupMenu prob;
    for (int p : { 100, 90, 75, 50, 25, 10 }) prob.addItem (40 + p, juce::String (p) + "%", true, juce::roundToInt (st.probability * 100.0f) == p);
    m.addSubMenu ("Probability", prob);

    juce::Component::SafePointer<StepGrid> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe, step] (int r)
    {
        if (safe == nullptr || r == 0) return;
        safe->edit ([r, step] (PatternLayer& l)
        {
            auto& s = l.steps[(size_t) step];
            if (step >= l.length) l.length = step + 1;
            s.active = true;
            if (r == 1)       { s.isTie = ! s.isTie; if (s.isTie) s.isRest = false; }
            else if (r == 2)  { s.isRest = ! s.isRest; if (s.isRest) s.isTie = false; }
            else if (r == 3)  s.octaveOffset = juce::jmin (2, s.octaveOffset + 1);
            else if (r == 4)  s.octaveOffset = juce::jmax (-2, s.octaveOffset - 1);
            else if (r == 5)  s.octaveOffset = 0;
            else if (r >= 11 && r <= 14) s.ratchet = r - 10;
            else if (r == 20) s.voiceIndex = VoiceAll;
            else if (r == 21) s.voiceIndex = VoiceTop;
            else if (r == 22) s.voiceIndex = VoiceBottom;
            else if (r >= 30 && r < 36) s.voiceIndex = r - 30;
            else if (r >= 50) s.probability = (float) (r - 40) / 100.0f;
        });
        if (safe->ctx.onSelectionChanged) safe->ctx.onSelectionChanged();
    });
}

//==============================================================================
PresetGrid::PresetGrid (GridContext& context) : ctx (context)
{
    const int n = getPatternPresetNames().size();
    thumbs.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
        applyPatternPreset (thumbs[(size_t) i], i);
}

juce::Rectangle<float> PresetGrid::cellBounds (int index) const
{
    const int rows = ((int) thumbs.size() + columns - 1) / columns;
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    const float w = r.getWidth() / (float) columns, h = r.getHeight() / (float) rows;
    return { r.getX() + (index % columns) * w + 3.0f, r.getY() + (index / columns) * h + 3.0f, w - 6.0f, h - 6.0f };
}

void PresetGrid::paint (juce::Graphics& g)
{
    const auto names = getPatternPresetNames();
    const int current = ctx.processor.getCurrentPreset();
    std::vector<int> chord { 0, 1, 2, 3 }, voices;
    juce::Random rng (3);

    for (int i = 0; i < (int) thumbs.size(); ++i)
    {
        const auto cell = cellBounds (i);
        g.setColour (i == hover ? C::raised : juce::Colour (0xff1a1d24));
        g.fillRoundedRectangle (cell, 4.0f);

        // Tall cells: thumbnail above the name. Short cells (many presets): thumbnail on the left, name on the right.
        const bool compact = cell.getHeight() < 40.0f;
        auto area = compact ? cell.reduced (4.0f, 3.0f).withWidth (cell.getWidth() * 0.32f)
                            : cell.reduced (6.0f, 5.0f).withTrimmedBottom (12.0f);
        const auto& e = thumbs[(size_t) i];
        double len = 4.0;
        for (int l = 0; l < 4; ++l) len = std::max (len, std::min (8.0, e.getCycleLengthPpq (l, 4.0)));
        const float laneH = area.getHeight() / 5.0f;

        e.collectHits (0.0, len, 4.0, 0, 0.0f, rng, false, [&] (const PatternHit& h)
        {
            PattCatEngine::selectVoices (chord, h.voiceIndex, voices);
            const float x = area.getX() + (float) (h.ppq / len) * area.getWidth();
            const float w = juce::jmax (2.0f, (float) (h.lengthPpq / len) * area.getWidth());
            for (int v : voices)
            {
                const float y = area.getBottom() - (float) (v + (h.octaveOffset < 0 ? 0 : 1)) * laneH;
                g.setColour ((i == current ? C::accent : C::text).withAlpha (0.65f));
                g.fillRect (x, y, juce::jmin (w, area.getRight() - x), juce::jmax (1.5f, laneH * 0.45f));
            }
        });

        g.setColour (i == current ? C::accent : C::textDim);
        g.setFont (cat::catFont (10.0f));
        if (compact)
            g.drawFittedText (names[i], cell.withTrimmedLeft (cell.getWidth() * 0.34f + 2.0f).toNearestInt().reduced (2, 1),
                              juce::Justification::centredLeft, 1, 0.75f);
        else
            g.drawFittedText (names[i], cell.toNearestInt().reduced (4, 2), juce::Justification::centredBottom, 1, 0.7f);

        if (i == current)
        {
            g.setColour (C::accent);
            g.drawRoundedRectangle (cell, 4.0f, 1.5f);
        }
    }
}

void PresetGrid::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < (int) thumbs.size(); ++i)
        if (cellBounds (i).contains (e.position))
        {
            ctx.processor.applyPreset (i);
            repaint();
            return;
        }
}

void PresetGrid::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < (int) thumbs.size(); ++i)
        if (cellBounds (i).contains (e.position)) h = i;
    if (h != hover)
    {
        hover = h;
        setTooltip (h >= 0 ? getPatternPresetNames()[h] : juce::String());
        repaint();
    }
}

void PresetGrid::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

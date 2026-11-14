#include "BendVisualizer.h"
#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include <cmath>
#include <limits>

namespace C = cat::Colours;

BendVisualizer::BendVisualizer (BendCatProcessor& p) : processor (p)
{
    for (auto& h : history) h.fill (std::numeric_limits<float>::quiet_NaN());
    startTimerHz (60);
}

void BendVisualizer::timerCallback()
{
    writePos = (writePos + 1) % historySize;
    for (size_t v = 0; v < 16; ++v)
    {
        const auto& view = processor.voiceViews[v];
        history[v][(size_t) writePos] = view.active.load() ? view.bend.load() : std::numeric_limits<float>::quiet_NaN();
        wasActive[v] = view.active.load();
    }
    repaint();
}

void BendVisualizer::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff111318));
    g.fillRoundedRectangle (r, 6.0f);

    auto a = r.reduced (compact ? 6.0f : 44.0f, compact ? 6.0f : 16.0f);
    // Scale to what the bends actually use (the MIDI bend range can be up to 48 semitones)
    const float range = juce::jmin (processor.apvts.getRawParameterValue ("range")->load(),
                                    juce::jmax (2.0f, processor.apvts.getRawParameterValue ("bendAmount")->load()));

    // Semitone grid
    for (int s = -(int) range; s <= (int) range; ++s)
    {
        const float y = a.getCentreY() - (float) s / range * a.getHeight() * 0.5f;
        g.setColour (s == 0 ? C::outline : C::outline.withAlpha (0.3f));
        g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
        if (! compact && (range <= 4.0f || s % 2 == 0))
        {
            g.setColour (C::textDim);
            g.setFont (cat::catFont (10.0f));
            g.drawText ((s > 0 ? "+" : "") + juce::String (s) + " st", juce::Rectangle<float> (r.getX() + 2.0f, y - 7.0f, 38.0f, 14.0f),
                        juce::Justification::centredRight);
        }
    }

    for (size_t v = 0; v < 16; ++v)
    {
        juce::Path p;
        bool drawing = false;
        for (int i = 0; i < historySize; ++i)
        {
            const int idx = (writePos + 1 + i) % historySize;
            const float value = history[v][(size_t) idx];
            if (std::isnan (value)) { drawing = false; continue; }
            const juce::Point<float> pt (a.getX() + (float) i / (historySize - 1) * a.getWidth(),
                                         a.getCentreY() - juce::jlimit (-1.0f, 1.0f, value / range) * a.getHeight() * 0.5f);
            if (! drawing) { p.startNewSubPath (pt); drawing = true; }
            else p.lineTo (pt);
        }
        g.setColour (cat::Colours::palette ((int) v).brighter (0.2f));
        g.strokePath (p, juce::PathStrokeType (compact ? 1.2f : 1.8f));
    }

    if (compact)
        return;

    // Legend
    float x = a.getX();
    g.setFont (cat::catFont (11.0f, true));
    for (size_t v = 0; v < 16; ++v)
    {
        if (! wasActive[v]) continue;
        const auto& view = processor.voiceViews[v];
        const auto text = "ch" + juce::String ((int) v + 1) + " " + cat::cubaseNoteName (view.note.load())
                          + " " + juce::String (view.bend.load() * 100.0f, 0) + "ct";
        g.setColour (cat::Colours::palette ((int) v).brighter (0.2f));
        g.drawText (text, juce::Rectangle<float> (x, r.getY() + 2.0f, 120.0f, 14.0f), juce::Justification::centredLeft);
        x += 124.0f;
        if (x > a.getRight() - 120.0f) break;
    }
}

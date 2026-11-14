#include "BounceView.h"

namespace C = cat::Colours;

BounceView::BounceView (PattCatProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, false);
    cats.reserve (64);
    startTimerHz (60);
}

bool BounceView::isBlack (int n)
{
    const int pc = n % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

juce::Colour BounceView::colourFor (int note)
{
    // One soft colour per pitch class
    static const juce::Colour colours[] = {
        juce::Colour (0xffd59a9a), juce::Colour (0xffd8b08a), juce::Colour (0xffd8cf8f), juce::Colour (0xffb5d48f),
        juce::Colour (0xff8fd0a0), juce::Colour (0xff8fd0c8), juce::Colour (0xff8fc4e0), juce::Colour (0xff8fa8e8),
        juce::Colour (0xffa59be8), juce::Colour (0xffc39be0), juce::Colour (0xffdc9fd0), juce::Colour (0xffe0a0b4),
    };
    return colours[(size_t) (((note % 12) + 12) % 12)];
}

juce::Rectangle<float> BounceView::keyBounds (int note) const
{
    // Keyboard strip along the bottom: white keys evenly spaced, black keys on top
    const auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    int whites = 0;
    for (int n = lowNote; n <= highNote; ++n) whites += isBlack (n) ? 0 : 1;
    const float keyW = area.getWidth() / (float) whites;
    const float keyH = juce::jmin (70.0f, area.getHeight() * 0.22f);

    int whiteIndex = 0;
    for (int n = lowNote; n < note; ++n) whiteIndex += isBlack (n) ? 0 : 1;
    const float x = area.getX() + keyW * (float) whiteIndex;
    if (! isBlack (note))
        return { x, area.getBottom() - keyH, keyW, keyH };
    return { x - keyW * 0.32f, area.getBottom() - keyH, keyW * 0.64f, keyH * 0.62f };
}

float BounceView::heightAt (const PattCatProcessor::BounceEvent& e, double nowMs, bool& landed, double& endMs)
{
    landed = false;
    const double elapsed = nowMs - e.startMs;
    if (elapsed < 0.0)
        return -1.0f;   // not started yet (scheduled slightly ahead)

    // Walk the gaps: the cat is in the air between two hits, highest in the middle
    double t = 0.0, gap = e.firstGapMs;
    const double maxGap = e.ratio <= 1.0f ? e.firstGapMs : e.firstGapMs * std::pow ((double) e.ratio, juce::jmax (0, e.count - 1));
    for (int k = 0; k < e.count; ++k)
    {
        if (elapsed < t + gap)
        {
            const double s = (elapsed - t) / gap;
            landed = (elapsed - t) < 70.0;
            const double energy = (1.0 - e.fade * (double) k / (double) juce::jmax (1, e.count)) * e.velocity;
            return (float) (4.0 * s * (1.0 - s) * std::pow (gap / maxGap, 0.7) * (0.35 + 0.65 * energy));
        }
        t += gap;
        gap *= e.ratio;
    }

    endMs = e.startMs + t;
    landed = elapsed - t < 70.0;
    return 0.0f;   // finished: resting on its key, then fades out
}

void BounceView::timerCallback()
{
    // New bouncing notes from the audio thread
    const auto scope = processor.bounceFifo.read (processor.bounceFifo.getNumReady());
    auto take = [this] (int start, int size)
    {
        for (int i = 0; i < size; ++i)
        {
            const auto& e = processor.bounceEvents[(size_t) (start + i)];
            if (e.count == 0)
            {
                // Key released in "While Held" mode: that cat stops bouncing
                cats.erase (std::remove_if (cats.begin(), cats.end(), [&e] (const Cat& c) { return c.e.note == e.note; }), cats.end());
                continue;
            }
            // One cat per key: a new bounce on the same note takes over the old cat
            cats.erase (std::remove_if (cats.begin(), cats.end(), [&e] (const Cat& c) { return c.e.note == e.note; }), cats.end());
            if (cats.size() >= 64)
                cats.erase (cats.begin());
            cats.push_back ({ e, 0.0 });
        }
    };
    take (scope.startIndex1, scope.blockSize1);
    take (scope.startIndex2, scope.blockSize2);

    const double now = juce::Time::getMillisecondCounterHiRes();
    cats.erase (std::remove_if (cats.begin(), cats.end(), [now] (Cat& c)
    {
        bool landed = false;
        double end = 0.0;
        heightAt (c.e, now, landed, end);
        return end > 0.0 && now - end > 450.0;
    }), cats.end());

    repaint();
}

void BounceView::drawCat (juce::Graphics& g, juce::Point<float> c, float s, juce::Colour colour) const
{
    // Little cat head: round face, two ears, two eyes
    juce::Path head;
    head.addEllipse (c.x - s, c.y - s * 0.85f, s * 2.0f, s * 1.7f);
    head.addTriangle (c.x - s * 0.95f, c.y - s * 0.2f, c.x - s * 0.8f, c.y - s * 1.45f, c.x - s * 0.15f, c.y - s * 0.7f);
    head.addTriangle (c.x + s * 0.95f, c.y - s * 0.2f, c.x + s * 0.8f, c.y - s * 1.45f, c.x + s * 0.15f, c.y - s * 0.7f);
    g.setColour (colour);
    g.fillPath (head);

    g.setColour (C::background.withAlpha (0.85f));
    g.fillEllipse (c.x - s * 0.5f - s * 0.13f, c.y - s * 0.15f, s * 0.26f, s * 0.3f);
    g.fillEllipse (c.x + s * 0.5f - s * 0.13f, c.y - s * 0.15f, s * 0.26f, s * 0.3f);
}

void BounceView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff101116));
    g.fillRoundedRectangle (bounds, 8.0f);

    const double now = juce::Time::getMillisecondCounterHiRes();

    // Which keys are lit right now (a cat just landed on them)
    std::array<juce::Colour, 128> lit {};
    for (const auto& c : cats)
    {
        bool landed = false;
        double end = 0.0;
        const float h = heightAt (c.e, now, landed, end);
        if (h >= 0.0f && landed && juce::isPositiveAndBelow (c.e.note, 128))
            lit[(size_t) c.e.note] = colourFor (c.e.note);
    }

    // Keyboard: white keys, then black keys
    for (int pass = 0; pass < 2; ++pass)
        for (int n = lowNote; n <= highNote; ++n)
        {
            if (isBlack (n) != (pass == 1)) continue;
            auto k = keyBounds (n).reduced (pass == 0 ? 1.0f : 0.0f, 0.0f);
            const bool on = ! lit[(size_t) n].isTransparent();
            g.setColour (on ? lit[(size_t) n] : (pass == 0 ? juce::Colour (0xff1d1f26) : juce::Colour (0xff0b0c10)));
            g.fillRoundedRectangle (k, 2.0f);
            if (pass == 0 && n % 12 == 0)
            {
                g.setColour (C::textDim);
                g.setFont (cat::catFont (10.0f));
                g.drawText (juce::MidiMessage::getMidiNoteName (n, true, true, 3), k.removeFromBottom (14.0f), juce::Justification::centred);
            }
        }

    // Cats on sticks
    const float floorY = keyBounds (lowNote).getY() - 4.0f;
    const float maxJump = floorY - bounds.getY() - 46.0f;
    for (const auto& c : cats)
    {
        bool landed = false;
        double end = 0.0;
        const float h = heightAt (c.e, now, landed, end);
        if (h < 0.0f || ! juce::isPositiveAndBelow (c.e.note, 128)) continue;

        const float alpha = end > 0.0 ? (float) juce::jlimit (0.0, 1.0, 1.0 - (now - end) / 450.0) : 1.0f;
        const auto key = keyBounds (c.e.note);
        const float size = 7.0f + 5.0f * c.e.velocity;
        const juce::Point<float> head (key.getCentreX(), floorY - 18.0f - h * maxJump);
        const auto colour = colourFor (c.e.note).withMultipliedAlpha (alpha);

        g.setColour (colour.withAlpha (0.55f * alpha));
        g.drawLine (key.getCentreX(), key.getY(), head.x, head.y + size * 0.8f, 1.5f);
        drawCat (g, head, size, colour);

        g.setColour (colour);
        g.setFont (cat::catFont (10.5f, true));
        g.drawText (juce::MidiMessage::getMidiNoteName (c.e.note, true, true, 3),
                    juce::Rectangle<float> (40.0f, 13.0f).withCentre (head.translated (0.0f, -size * 1.45f - 8.0f)), juce::Justification::centred);
    }

    if (cats.empty())
    {
        const int mode = juce::roundToInt (processor.apvts.getRawParameterValue ("bounceMode")->load());
        g.setColour (C::textDim);
        g.setFont (cat::catFont (13.0f));
        g.drawText (mode == 0 ? "Bounce is off - choose Pattern Notes or Keys"
                              : (mode == 1 ? "Every note of the pattern will bounce" : "Play some keys - every note bounces"),
                    bounds.withHeight (bounds.getHeight() * 0.5f), juce::Justification::centred);
    }
}

#include "PluginEditor.h"

using namespace echocat;

namespace
{
    constexpr int designW = 1000, designH = 620;

    juce::String timeText (double seconds)
    {
        return seconds < 1.0 ? juce::String (juce::roundToInt (seconds * 1000.0)) + " ms" : juce::String (seconds, 2) + " s";
    }
}

//==============================================================================
EchoView::EchoView (EchoCatProcessor& p) : processor (p)
{
    setTooltip ("The cat's voice and its echoes: their spacing is the echo time, how many you see is the feedback");
    startTimerHz (30);
}

void EchoView::timerCallback()
{
    clock += 1.0 / 30.0;
    in = juce::jmax (processor.engine.inLevel.load(), in * 0.8f);
    echo = processor.engine.echoLevel.load();
    space = processor.engine.spaceLevel.load();
    duck = processor.engine.duckAmount.load();
    repaint();
}

void EchoView::drawCat (juce::Graphics& g, juce::Point<float> c, float s, float open, bool frozen) const
{
    const auto fur = frozen ? juce::Colour (0xffcfe8ff) : juce::Colour (0xfff1f3f8);
    const auto line = juce::Colour (0xff2a2f3a);
    // ears
    juce::Path ears;
    ears.addTriangle (c.x - s * 0.62f, c.y - s * 0.25f, c.x - s * 0.5f, c.y - s * 0.95f, c.x - s * 0.08f, c.y - s * 0.55f);
    ears.addTriangle (c.x + s * 0.62f, c.y - s * 0.25f, c.x + s * 0.5f, c.y - s * 0.95f, c.x + s * 0.08f, c.y - s * 0.55f);
    g.setColour (fur);
    g.fillPath (ears);
    g.setColour (line);
    g.strokePath (ears, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
    juce::Path inner;
    inner.addTriangle (c.x - s * 0.5f, c.y - s * 0.4f, c.x - s * 0.47f, c.y - s * 0.78f, c.x - s * 0.22f, c.y - s * 0.55f);
    inner.addTriangle (c.x + s * 0.5f, c.y - s * 0.4f, c.x + s * 0.47f, c.y - s * 0.78f, c.x + s * 0.22f, c.y - s * 0.55f);
    g.setColour (juce::Colour (0xffffb3c6));
    g.fillPath (inner);
    // head
    const auto head = juce::Rectangle<float> (s * 1.5f, s * 1.25f).withCentre (c);
    g.setColour (fur);
    g.fillEllipse (head);
    g.setColour (line);
    g.drawEllipse (head, 2.0f);
    // eyes (closed and happy while it sings loudly)
    const float ey = c.y - s * 0.1f;
    for (float side : { -1.0f, 1.0f })
    {
        const float ex = c.x + side * s * 0.3f;
        if (open > 0.5f)
        {
            juce::Path arc;
            arc.addCentredArc (ex, ey + s * 0.04f, s * 0.11f, s * 0.08f, 0.0f, -1.3f, 1.3f, true);
            g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
            g.fillEllipse (juce::Rectangle<float> (s * 0.13f, s * 0.17f).withCentre ({ ex, ey }));
    }
    // nose and mouth: open with the input level
    g.setColour (juce::Colour (0xffff8fa8));
    g.fillEllipse (juce::Rectangle<float> (s * 0.12f, s * 0.08f).withCentre ({ c.x, c.y + s * 0.1f }));
    const float mh = s * (0.05f + 0.3f * open);
    g.setColour (line);
    g.fillEllipse (juce::Rectangle<float> (s * (0.16f + 0.1f * open), mh).withCentre ({ c.x, c.y + s * 0.22f + mh * 0.5f }));
    // whiskers
    g.setColour (line.withAlpha (0.7f));
    for (float side : { -1.0f, 1.0f })
        for (int k = 0; k < 2; ++k)
            g.drawLine (c.x + side * s * 0.38f, c.y + s * (0.14f + 0.08f * k), c.x + side * s * 0.85f, c.y + s * (0.08f + 0.14f * k), 1.4f);
}

void EchoView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto st = processor.shownSettings();
    const bool frozen = st.freeze;
    // the sky: deeper and more starry with more space
    juce::ColourGradient sky (juce::Colour (0xff141a2e).interpolatedWith (juce::Colour (0xff261a3e), st.space), b.getX(), b.getY(),
                              juce::Colour (0xff0d1018), b.getX(), b.getBottom(), false);
    if (frozen) sky = juce::ColourGradient (juce::Colour (0xff1c3550), b.getX(), b.getY(), juce::Colour (0xff0e1824), b.getX(), b.getBottom(), false);
    g.setGradientFill (sky);
    g.fillRoundedRectangle (b, 10.0f);
    if (st.space > 0.01f)
    {
        juce::Random rng (42);
        for (int i = 0; i < 70; ++i)
        {
            const float x = b.getX() + rng.nextFloat() * b.getWidth(), y = b.getY() + rng.nextFloat() * b.getHeight();
            const float tw = 0.5f + 0.5f * std::sin ((float) clock * (1.0f + rng.nextFloat() * 3.0f) + (float) i);
            g.setColour (juce::Colours::white.withAlpha (juce::jlimit (0.0f, 1.0f, st.space * 0.5f * tw * (0.4f + space * 2.0f))));
            const float sz = 1.0f + 1.5f * rng.nextFloat() + (st.shimmer > 0.01f ? 1.5f * st.shimmer * tw : 0.0f);
            g.fillEllipse (x, y, sz, sz);
        }
    }

    const float catX = b.getX() + 90.0f, midY = b.getCentreY() + 10.0f;
    const double delay = juce::jmax (0.01, processor.engine.shownDelay.load());
    const float fb = st.freeze ? 1.0f : juce::jlimit (0.0f, 0.97f, st.feedback * 0.97f);
    const float x0 = catX + 70.0f, x1 = b.getRight() - 40.0f;
    // show about eight echoes (or what fits in eight seconds)
    const double window = juce::jlimit (0.6, 8.0, delay * 8.5);
    const float pxPerSec = (x1 - x0) / (float) window;
    // colour each time round: bright = cool blue, dark = warm amber; drive = redder
    const auto bright = juce::Colour (0xff9fd0ff), dark = juce::Colour (0xffd9a066);
    const auto echoCol = (frozen ? juce::Colour (0xffbfe6ff) : dark.interpolatedWith (bright, st.tone)).interpolatedWith (juce::Colour (0xffff7a6a), st.drive * 0.5f);
    const double phase = std::fmod (clock, delay) / delay;
    float amp = 1.0f;
    for (int k = 1; k <= 24; ++k)
    {
        const float x = x0 + (float) (k * delay) * pxPerSec;
        if (x > x1 + 10.0f) break;
        const float side = (k % 2 == 1 ? -1.0f : 1.0f) * st.pingpong;
        const float wob = st.wobble * 6.0f * std::sin ((float) clock * 1.7f + (float) k * 0.9f);
        const float y = midY + side * 48.0f + wob;
        const float a = amp * (st.mix > 0.001f ? 1.0f : 0.25f);
        const float r = 10.0f + 22.0f * amp;
        if (a > 0.02f)
        {
            // a ring that breathes in time
            const float pulse = (float) (1.0 - phase);
            g.setColour (echoCol.withAlpha (juce::jlimit (0.0f, 1.0f, a * 0.25f * pulse)));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f + 14.0f * (1.0f - pulse), r * 2.0f + 14.0f * (1.0f - pulse)).withCentre ({ x, y }), 2.0f);
            g.setColour (echoCol.withAlpha (juce::jlimit (0.0f, 1.0f, a * 0.75f)));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre ({ x, y }), 2.0f + 2.0f * amp);
            g.setFont (cat::catFont (8.0f + 12.0f * amp, true));
            g.drawText (k == 1 ? "MEOW" : "meow", juce::Rectangle<float> (90.0f, 24.0f).withCentre ({ x, y }), juce::Justification::centred);
            // lines from the cat (left / right)
            if (st.pingpong > 0.05f && k <= 2)
            {
                g.setColour (echoCol.withAlpha (0.12f));
                g.drawLine (catX + 40.0f, midY, x - r, y, 1.0f);
            }
        }
        amp *= fb;
        if (amp < 0.02f) break;
    }
    // shimmer sparkles rising out of the echoes
    if (st.shimmer > 0.01f && st.space > 0.01f)
    {
        juce::Random rng (7);
        for (int i = 0; i < 40; ++i)
        {
            const float x = x0 + rng.nextFloat() * (x1 - x0);
            const float speed = 10.0f + 30.0f * rng.nextFloat();
            const float y = b.getBottom() - std::fmod ((float) clock * speed + rng.nextFloat() * b.getHeight(), b.getHeight());
            g.setColour (juce::Colour (0xffffe9a8).withAlpha (st.shimmer * 0.6f * (y - b.getY()) / b.getHeight()));
            g.fillEllipse (x, y, 2.5f, 2.5f);
        }
    }
    drawCat (g, { catX, midY }, 52.0f, juce::jlimit (0.0f, 1.0f, std::sqrt (in) * 1.6f), frozen);

    // the time and tempo
    g.setColour (cat::Colours::text);
    g.setFont (cat::catFont (15.0f, true));
    juce::String t = st.sync ? divisionNames()[st.division] + "  =  " + timeText (st.delaySeconds()) : timeText (st.delaySeconds());
    t << "     " << juce::roundToInt (processor.getBpm()) << " BPM" << (processor.isHostSynced() ? "" : " (no host tempo)");
    g.drawText (t, b.reduced (16.0f, 10.0f).removeFromTop (20.0f), juce::Justification::topLeft);
    if (frozen)
    {
        g.setColour (juce::Colour (0xffbfe6ff));
        g.setFont (cat::catFont (18.0f, true));
        g.drawText ("FROZEN - the echoes hold forever", b.reduced (16.0f, 10.0f).removeFromTop (22.0f), juce::Justification::topRight);
    }
    else if (st.duck > 0.01f)
    {
        g.setColour (cat::Colours::textDim);
        g.setFont (cat::catFont (12.0f, true));
        auto d = b.reduced (16.0f, 12.0f).removeFromTop (14.0f).removeFromRight (160.0f);
        g.drawText ("DUCKING", d.removeFromLeft (64.0f), juce::Justification::centredLeft);
        g.setColour (cat::Colours::raised);
        g.fillRoundedRectangle (d.reduced (0.0f, 3.0f), 3.0f);
        g.setColour (cat::Colours::green);
        g.fillRoundedRectangle (d.reduced (0.0f, 3.0f).withWidth (d.getWidth() * juce::jlimit (0.0f, 1.0f, duck)), 3.0f);
    }
    // meters: in, echo, space
    auto m = b.reduced (14.0f).removeFromRight (16.0f).withTrimmedTop (34.0f);
    const float lv[3] = { in, echo, space };
    const juce::Colour mc[3] = { cat::Colours::text, echoCol, cat::Colours::purple };
    const float mw = 4.0f;
    for (int i = 0; i < 3; ++i)
    {
        auto bar = juce::Rectangle<float> (m.getX() + i * (mw + 2.0f), m.getY(), mw, m.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (bar, 2.0f);
        const float h = bar.getHeight() * juce::jlimit (0.0f, 1.0f, std::sqrt (lv[i]));
        g.setColour (mc[i]);
        g.fillRoundedRectangle (bar.removeFromBottom (h), 2.0f);
    }
}

//==============================================================================
void EchoSection::paint (juce::Graphics& g)
{
    cat::CatLookAndFeel::drawGlassPanel (g, getLocalBounds().toFloat(), 10.0f);
    g.setColour (tint);
    g.fillRoundedRectangle (juce::Rectangle<float> (14.0f, 14.0f, 4.0f, 14.0f), 2.0f);
    g.setColour (cat::Colours::text);
    g.setFont (cat::catFont (13.0f, true));
    g.drawText (titleText, juce::Rectangle<int> (24, 10, getWidth() - 30, 22), juce::Justification::centredLeft);
}

void EchoSection::resized()
{
    auto r = getLocalBounds().reduced (12, 10).withTrimmedTop (26);
    for (auto* c : top) { c->setBounds (r.removeFromTop (30)); r.removeFromTop (6); }
    if (knobs.empty()) return;
    const int kw = r.getWidth() / (int) knobs.size();
    for (auto* k : knobs) k->setBounds (r.removeFromLeft (kw).withSizeKeepingCentre (juce::jmin (kw, 110), juce::jmin (r.getHeight(), 140)));
}

//==============================================================================
cat::Knob* EchoCatEditor::knob (const juce::String& id, const juce::String& caption, juce::Colour c)
{
    auto* k = knobs.add (new cat::Knob (processor.apvts, id, caption, nullptr, c));
    return k;
}

EchoCatEditor::EchoCatEditor (EchoCatProcessor& p) : processor (p)
{
    setSize (designW, designH);
    freezeButton.setTooltip ("Hold the echoes: nothing new goes in, what is there repeats forever");
    syncButton.setTooltip ("The echo time follows Cubase's tempo (else set it in ms)");
    divisionBox.getCombo().setTooltip ("How long between echoes, as a note length");

    timeSection.addTop (&syncButton);
    timeSection.addTop (&divisionBox);
    timeKnob = knob ("timeMs", "Time", cat::Colours::accent);
    timeKnob->getSlider().setTooltip ("The echo time when it is not in time with Cubase");
    timeSection.addKnob (timeKnob);
    auto* fbk = knob ("feedback", "Feedback", cat::Colours::accent);
    fbk->getSlider().setTooltip ("How many echoes: a few ... nearly forever");
    timeSection.addKnob (fbk);
    auto* ppk = knob ("pingpong", "Ping-Pong", cat::Colours::accent);
    ppk->getSlider().setTooltip ("The echoes bounce between the left and right speaker");
    timeSection.addKnob (ppk);

    struct K { const char* id; const char* caption; const char* tip; };
    const K colourKnobs[] = { { "tone", "Tone", "Each echo gets darker (left) or stays bright (right)" },
                              { "lowcut", "Low Cut", "Each echo gets thinner: keeps the mix clean under vocals" },
                              { "wobble", "Wobble", "Old tape: the echoes waver in pitch" },
                              { "drive", "Drive", "Warm tape saturation each time round" } };
    for (auto& k : colourKnobs) { auto* kn = knob (k.id, k.caption, cat::Colours::red); kn->getSlider().setTooltip (k.tip); colourSection.addKnob (kn); }
    const K spaceKnobs[] = { { "space", "Space", "A big soft room around the sound and its echoes" },
                             { "shimmer", "Shimmer", "The space rings an octave higher - like a choir of glass" },
                             { "duck", "Duck", "The echoes step back while you sing or play, and come up in the gaps" } };
    for (auto& k : spaceKnobs) { auto* kn = knob (k.id, k.caption, cat::Colours::purple); kn->getSlider().setTooltip (k.tip); spaceSection.addKnob (kn); }
    const K mixKnobs[] = { { "mix", "Echo", "How loud the echoes are" },
                           { "dry", "Dry", "How loud your own sound is (turn down on an FX send)" },
                           { "output", "Output", "The level going out" } };
    for (auto& k : mixKnobs) { auto* kn = knob (k.id, k.caption, cat::Colours::green); kn->getSlider().setTooltip (k.tip); mixSection.addKnob (kn); }

    for (juce::Component* c : { (juce::Component*) &presetBar, (juce::Component*) &freezeButton, (juce::Component*) &view,
                                (juce::Component*) &timeSection, (juce::Component*) &colourSection, (juce::Component*) &spaceSection,
                                (juce::Component*) &mixSection })
        addAndMakeVisible (c);
    resized();
    startTimerHz (10);
}

EchoCatEditor::~EchoCatEditor() { stopTimer(); }

void EchoCatEditor::timerCallback()
{
    const bool sync = processor.shownSettings().sync;
    if (sync != shownSync || (timeKnob->getAlpha() > 0.99f) == sync)
    {
        shownSync = sync;
        timeKnob->setAlpha (sync ? 0.35f : 1.0f);
        divisionBox.setAlpha (sync ? 1.0f : 0.35f);
    }
}

void EchoCatEditor::paint (juce::Graphics& g)
{
    g.fillAll (cat::Colours::background);
    cat::CatLookAndFeel::drawLogo (g, logoArea.toFloat(), "ECHO", "CAT");
    g.setColour (cat::Colours::textDim);
    g.setFont (cat::catFont (12.0f));
    g.drawFittedText ("Put Echo-Cat on a vocal or instrument track (or an FX channel with Dry at 0%). Echo time follows Cubase's tempo.",
                      infoArea, juce::Justification::centredLeft, 1);
}

void EchoCatEditor::resized()
{
    if (knobs.size() < 13) return;   // still being built
    auto r = getLocalBounds().reduced (14, 10);
    auto top = r.removeFromTop (40);
    logoArea = top.removeFromLeft (150);
    top.removeFromLeft (12);
    presetBar.setBounds (top.removeFromLeft (440));
    freezeButton.setBounds (top.removeFromRight (110));
    infoArea = getLocalBounds().removeFromBottom (22).reduced (16, 0);
    r.removeFromBottom (16);
    r.removeFromTop (10);
    auto bottom = r.removeFromBottom (232);
    r.removeFromBottom (10);
    view.setBounds (r);
    const int gap = 10;
    const int w1 = 330, w2 = (bottom.getWidth() - w1 - 3 * gap) * 4 / 10, w3 = (bottom.getWidth() - w1 - 3 * gap) * 3 / 10;
    timeSection.setBounds (bottom.removeFromLeft (w1)); bottom.removeFromLeft (gap);
    colourSection.setBounds (bottom.removeFromLeft (w2)); bottom.removeFromLeft (gap);
    spaceSection.setBounds (bottom.removeFromLeft (w3)); bottom.removeFromLeft (gap);
    mixSection.setBounds (bottom);
}

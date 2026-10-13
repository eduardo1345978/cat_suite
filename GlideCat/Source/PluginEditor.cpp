#include "PluginEditor.h"

namespace C = cat::Colours;
using namespace glidecat;

namespace
{
    juce::String lid (int layer, const char* id) { return "l" + juce::String (layer) + "_" + id; }

    juce::Colour voiceColour (int i)
    {
        static const juce::Colour cols[] = { juce::Colour (0xff8fa3f0), juce::Colour (0xff6fc3b8), juce::Colour (0xffe0a86f), juce::Colour (0xffc27aa8),
                                             juce::Colour (0xff9ccf7a), juce::Colour (0xffd47c6c), juce::Colour (0xffb59cf0), juce::Colour (0xff6fa3d8) };
        return cols[i % 8].withMultipliedBrightness (i >= 8 ? 0.8f : 1.0f);
    }

    float paramValue (juce::AudioProcessorValueTreeState& s, const char* id)
    {
        if (auto* v = s.getRawParameterValue (id)) return v->load();
        return 0.0f;
    }

    const char* pilotHelp (int mode)
    {
        switch (mode)
        {
            case 1:  return "Octave Leaps: voices jump octaves up and down and glide back.";
            case 2:  return "Scale Walk: each voice steps along the scale, a different direction per voice.";
            case 3:  return "Glide Arp: one voice at a time glides out to a scale note and back, like an arpeggio.";
            case 4:  return "Bounce: voices bounce between their note and the Range above it.";
            case 5:  return "Weave: neighbouring voices trade places, so the lines cross.";
            case 6:  return "Breathe: the whole chord slowly opens up and closes again.";
            case 7:  return "Wander: voices drift to random scale notes within the Range.";
            case 8:  return "Mirror: upper voices rise while lower voices fall.";
            case 9:  return "Sequence: all voices follow the 8 steps below (semitones, snapped to the scale).";
            case 10: return "Random Loop: a random 8-step loop per voice, repeating until you change the chord.";
            default: return "Autopilot is off. Pick a mode to make held chords move by themselves.";
        }
    }
}

//==============================================================================
GlideView::GlideView (GlideCatProcessor& p) : processor (p)
{
    setTooltip ("Glide View: each line is one voice over the last 4 seconds. Arrows point to the note a voice is gliding to.");
    startTimerHz (30);
}

float GlideView::yFor (float pitch, juce::Rectangle<float> area) const
{
    return area.getBottom() - (pitch - lowPitch) / (highPitch - lowPitch) * area.getHeight();
}

void GlideView::timerCallback()
{
    float lo = 1000.0f, hi = -1000.0f;
    for (auto& v : processor.synth.views)
    {
        for (auto& h : v.history)
        {
            const float x = h.load (std::memory_order_relaxed);
            if (x >= 0.0f) { lo = juce::jmin (lo, x); hi = juce::jmax (hi, x); }
        }
        if (v.active.load())
        {
            lo = juce::jmin (lo, v.target.load()); hi = juce::jmax (hi, v.target.load());
        }
    }
    float tLo = 48.0f, tHi = 84.0f;
    if (hi >= lo)
    {
        tLo = lo - 4.0f; tHi = hi + 4.0f;
        if (tHi - tLo < 24.0f)
        {
            const float c = 0.5f * (tLo + tHi);
            tLo = c - 12.0f; tHi = c + 12.0f;
        }
    }
    lowPitch += (tLo - lowPitch) * 0.08f;
    highPitch += (tHi - highPitch) * 0.08f;
    repaint();
}

void GlideView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff101116));
    g.fillRoundedRectangle (bounds, 8.0f);

    auto inner = bounds.reduced (8.0f);
    auto header = inner.removeFromTop (20.0f);
    auto keys = inner.removeFromLeft (40.0f);
    auto arrows = inner.removeFromRight (64.0f);
    const auto plot = inner;

    // Header
    int sounding = 0;
    for (auto& v : processor.synth.views) sounding += v.active.load() ? 1 : 0;
    auto& s = processor.apvts;
    const bool rate = paramValue (s, "glideMode") > 0.5f;
    const juce::String glideText = rate ? juce::String (juce::roundToInt (paramValue (s, "rateMs"))) + " ms / semitone"
                                        : juce::String (juce::roundToInt (paramValue (s, "glideMs"))) + " ms";
    const int pilot = juce::roundToInt (paramValue (s, "pilot"));
    g.setFont (cat::catFont (12.0f, true));
    g.setColour (C::text);
    g.drawText ("GLIDE VIEW", header.removeFromLeft (100.0f), juce::Justification::centredLeft);
    g.setFont (cat::catFont (11.5f));
    g.setColour (C::textDim);
    g.drawText (juce::String (sounding) + " / " + juce::String (juce::roundToInt (paramValue (s, "voices"))) + " voices   |   "
                    + (paramValue (s, "leading") < 0.5f ? "Smart" : "Classic") + " leading   |   glide " + glideText + "   |   "
                    + curveNames()[juce::roundToInt (paramValue (s, "curve"))] + ", " + pathNames()[juce::roundToInt (paramValue (s, "path"))]
                    + (pilot > 0 ? "   |   Autopilot: " + pilotNames()[pilot] : juce::String()),
                header, juce::Justification::centredLeft);

    // Keyboard strip + semitone grid
    const int nLo = (int) std::floor (lowPitch), nHi = (int) std::ceil (highPitch);
    const float semi = plot.getHeight() / (highPitch - lowPitch);
    g.saveState();
    g.reduceClipRegion (keys.getUnion (plot).getUnion (arrows).toNearestInt());
    for (int n = nLo; n <= nHi; ++n)
    {
        const float y = yFor ((float) n, plot);
        const int pc = ((n % 12) + 12) % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        auto key = juce::Rectangle<float> (keys.getX(), y - semi * 0.5f, keys.getWidth() - 4.0f, semi);
        g.setColour (black ? juce::Colour (0xff1c1d24) : juce::Colour (0xff2c2e36));
        g.fillRect (key.reduced (0.0f, 0.5f));
        if (pc == 0)
        {
            g.setColour (C::textDim);
            g.setFont (cat::catFont (juce::jlimit (8.0f, 11.0f, semi + 2.0f)));
            g.drawText (cat::cubaseNoteName (n), key.withHeight (juce::jmax (12.0f, semi)).withCentre (key.getCentre()).translated (2.0f, 0.0f),
                        juce::Justification::centredLeft);
        }
        g.setColour (pc == 0 ? C::outline.brighter (0.25f) : C::outline.withAlpha (black ? 0.25f : 0.45f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), arrows.getRight());
    }
    g.restoreState();

    // Voices
    bool any = false;
    const float dx = plot.getWidth() / (float) (historyLength - 1);
    for (int i = 0; i < maxVoices; ++i)
    {
        auto& v = processor.synth.views[(size_t) i];
        const int w = v.write.load();
        bool has = false;
        for (int k = 0; k < historyLength; ++k)
        {
            scratch[(size_t) k] = v.history[(size_t) ((w + k) % historyLength)].load (std::memory_order_relaxed);
            has = has || scratch[(size_t) k] >= 0.0f;
        }
        if (! has && ! v.active.load()) continue;
        any = true;
        const auto col = voiceColour (i);

        juce::Path path;
        bool drawing = false;
        for (int k = 0; k < historyLength; ++k)
        {
            const float pitch = scratch[(size_t) k];
            if (pitch < 0.0f) { drawing = false; continue; }
            const float x = plot.getX() + dx * (float) k, y = yFor (pitch, plot);
            if (! drawing) { path.startNewSubPath (x, y); drawing = true; }
            else path.lineTo (x, y);
        }
        g.setColour (col.withAlpha (0.18f));
        g.strokePath (path, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col.withAlpha (0.9f));
        g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (v.active.load())
        {
            const float pitch = v.pitch.load(), target = v.target.load(), level = juce::jlimit (0.0f, 1.0f, v.level.load());
            const float x = plot.getRight(), y = yFor (pitch, plot), ty = yFor (target, plot);
            const float r = 3.0f + 4.0f * level;
            g.setColour (col);
            g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
            if (std::abs (target - pitch) > 0.08f)
            {
                juce::Path arrow;
                arrow.addArrow (juce::Line<float> (x + r + 2.0f, y, arrows.getX() + 24.0f, ty), 1.6f, 7.0f, 7.0f);
                g.setColour (col.withAlpha (0.75f));
                g.fillPath (arrow);
            }
            g.setColour (col.withAlpha (0.9f));
            g.setFont (cat::catFont (10.0f, true));
            g.drawText (cat::cubaseNoteName (juce::roundToInt (target)), juce::Rectangle<float> (arrows.getX() + 28.0f, ty - 7.0f, 36.0f, 14.0f),
                        juce::Justification::centredLeft);
        }
    }

    if (! any)
    {
        g.setColour (C::textDim);
        g.setFont (cat::catFont (13.0f));
        g.drawText ("Play a chord, then another one - the voices glide from chord to chord",
                    plot, juce::Justification::centred);
    }
}

//==============================================================================
void GlideSection::paint (juce::Graphics& g)
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

void GlideSection::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    r.removeFromTop (18);
    if (! left.empty())
    {
        auto col = r.removeFromLeft (leftWidth);
        r.removeFromLeft (8);
        const int h = juce::jmin (28, (col.getHeight() - 4 * ((int) left.size() - 1)) / (int) left.size());
        for (auto* c : left)
        {
            c->setBounds (col.removeFromTop (h));
            col.removeFromTop (4);
        }
    }
    if (knobs.empty()) return;
    const int n = (int) knobs.size();
    const int w = juce::jmin (84, r.getWidth() / n);
    const int h = juce::jmin (r.getHeight(), knobMaxHeight);
    auto row = r.withSizeKeepingCentre (r.getWidth(), h);
    for (auto* c : knobs)
        c->setBounds (row.removeFromLeft (w).reduced (2, 0));
}

//==============================================================================
cat::Knob* GlidePage::knob (const juce::String& id, const juce::String& caption, juce::Colour c)
{
    return static_cast<cat::Knob*> (owned.add (new cat::Knob (processor.apvts, id, caption, &processor.midiLearn, c)));
}

cat::ParamCombo* GlidePage::combo (const juce::String& id, const juce::String& caption, int captionW)
{
    return static_cast<cat::ParamCombo*> (owned.add (new cat::ParamCombo (processor.apvts, id, caption, &processor.midiLearn, captionW)));
}

cat::ParamToggle* GlidePage::toggle (const juce::String& id, const juce::String& text)
{
    return static_cast<cat::ParamToggle*> (owned.add (new cat::ParamToggle (processor.apvts, id, text, &processor.midiLearn)));
}

GlideSection* GlidePage::section (const juce::String& title, juce::Colour c)
{
    auto* s = sections.add (new GlideSection (title, c));
    addAndMakeVisible (s);
    return s;
}

namespace
{
    //==========================================================================
    class SynthPage : public GlidePage
    {
    public:
        explicit SynthPage (GlideCatProcessor& p) : GlidePage (p)
        {
            for (int l = 1; l <= 2; ++l)
            {
                auto& L = layers[(size_t) (l - 1)];
                L.osc = section (l == 1 ? "LAYER I - OSCILLATORS" : "LAYER II - OSCILLATORS", C::accent);
                L.osc->leftWidth = 150;
                L.osc->addLeft (toggle (lid (l, "on"), l == 1 ? "Layer I On" : "Layer II On"));
                L.osc->addLeft (combo (lid (l, "feet"), "Feet", 50));
                auto uni = cat::Stepper::forParameter (p.apvts, lid (l, "unison"), "Unison");
                uni->setTooltip ("Saws per oscillator (detuned by Detune)");
                L.osc->addLeft (owned.add (uni.release()));
                for (auto [id, cap] : { std::pair { "saw", "Saw" }, { "pulse", "Pulse" }, { "pw", "Width" }, { "pwm", "PWM" }, { "pwmRate", "PWM Rate" },
                                        { "sine", "Sine" }, { "noise", "Noise" }, { "detune", "Detune" }, { "fine", "Fine" }, { "brill", "Brilliance" },
                                        { "level", "Level" } })
                    L.osc->addKnob (knob (lid (l, id), cap, C::accent));

                L.filter = section (l == 1 ? "LAYER I - FILTERS + FILTER ENVELOPE" : "LAYER II - FILTERS + FILTER ENVELOPE", C::teal);
                for (auto [id, cap] : { std::pair { "hpf", "HP" }, { "hpRes", "HP Res" }, { "lpf", "LP" }, { "lpRes", "LP Res" }, { "key", "Key Trk" },
                                        { "egDepth", "EG Depth" }, { "egIL", "Initial" }, { "egAL", "Attack Lvl" }, { "egAtk", "EG Atk" },
                                        { "egDec", "EG Dec" }, { "egRel", "EG Rel" } })
                    L.filter->addKnob (knob (lid (l, id), cap, C::teal));

                L.amp = section (l == 1 ? "LAYER I - AMP" : "LAYER II - AMP", C::purple);
                for (auto [id, cap] : { std::pair { "attack", "Attack" }, { "decay", "Decay" }, { "sustain", "Sustain" }, { "release", "Release" } })
                    L.amp->addKnob (knob (lid (l, id), cap, C::purple));
            }
            global = section ("VOICE", C::slate);
            global->addKnob (knob ("ring", "Ring Mod", C::slate));
            global->addKnob (knob ("panSpread", "Pan Spread", C::slate));
            global->addKnob (knob ("volume", "Volume", C::slate));

            for (int i = 0; i < 2; ++i)
            {
                auto& b = layerButtons[(size_t) i];
                b.setButtonText (i == 0 ? "LAYER I" : "LAYER II");
                b.setClickingTogglesState (true);
                b.setRadioGroupId (4711);
                b.onClick = [this] { showLayer (layerButtons[1].getToggleState() ? 1 : 0); };
                addAndMakeVisible (b);
            }
            layerButtons[0].setToggleState (true, juce::dontSendNotification);
            layerButtons[0].setTooltip ("Edit layer I (each voice plays both layers)");
            layerButtons[1].setTooltip ("Edit layer II (e.g. a second octave, or a sine / noise layer)");
            showLayer (0);
        }

        void showLayer (int l)
        {
            for (int i = 0; i < 2; ++i)
                for (auto* s : { layers[(size_t) i].osc, layers[(size_t) i].filter, layers[(size_t) i].amp })
                    s->setVisible (i == l);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto top = r.removeFromTop (r.getHeight() / 2);
            r.removeFromTop (6);
            auto buttons = top.removeFromLeft (96);
            layerButtons[0].setBounds (buttons.removeFromTop (34).reduced (0, 2));
            buttons.removeFromTop (6);
            layerButtons[1].setBounds (buttons.removeFromTop (34).reduced (0, 2));
            top.removeFromLeft (6);
            global->setBounds (top.removeFromRight (230));
            top.removeFromRight (6);
            for (auto& L : layers)
            {
                L.osc->setBounds (top);
                auto bottom = r;
                L.amp->setBounds (bottom.removeFromRight (300));
                bottom.removeFromRight (6);
                L.filter->setBounds (bottom);
            }
        }

    private:
        struct Layer { GlideSection* osc = nullptr; GlideSection* filter = nullptr; GlideSection* amp = nullptr; };
        std::array<Layer, 2> layers;
        GlideSection* global = nullptr;
        std::array<juce::TextButton, 2> layerButtons;
    };

    //==========================================================================
    class GlideTabPage : public GlidePage
    {
    public:
        explicit GlideTabPage (GlideCatProcessor& p) : GlidePage (p)
        {
            time = section ("GLIDE TIME", C::accent);
            time->addLeft (combo ("glideMode", "Mode", 50));
            time->addLeft (combo ("curve", "Curve", 50));
            time->addKnob (knob ("glideMs", "Time", C::accent));
            time->addKnob (knob ("rateMs", "Rate", C::accent));
            time->addKnob (knob ("glideSpread", "Spread", C::accent));

            leading = section ("VOICE LEADING", C::teal);
            leading->leftWidth = 190;
            leading->addLeft (combo ("leading", "Leading", 62));
            leading->addLeft (combo ("newVoices", "New", 62));
            leading->addLeft (combo ("lineMode", "Lines", 62));
            leading->addLeft (toggle ("merge", "Merge spare voices"));
            leading->addKnob (knob ("lineMemory", "Memory", C::teal));

            path = section ("PATHS, STAGGER + RELEASE", C::purple);
            path->leftWidth = 210;
            path->addLeft (combo ("path", "Path", 62));
            path->addLeft (combo ("startFrom", "Start", 62));
            path->addLeft (combo ("staggerOrd", "Stagger", 62));
            path->addKnob (knob ("pathDepth", "Path Depth", C::purple));
            path->addKnob (knob ("startDepth", "Start Depth", C::purple));
            path->addKnob (knob ("staggerMs", "Stagger", C::purple));
            path->addKnob (knob ("releaseBend", "Release Bend", C::purple));

            help.setFont (cat::catFont (11.5f));
            help.setColour (juce::Label::textColourId, C::textDim);
            help.setJustificationType (juce::Justification::topLeft);
            help.setText ("Time: every glide takes the same time.  Rate: bigger jumps take longer.\n"
                          "Smart leading finds the smallest total movement; Classic moves voices in order.\n"
                          "Branch: extra notes split off the nearest voice.  Fresh: they start from Start Depth away.\n"
                          "Released Lines: voices remember the last chord for Memory seconds, so a new chord glides even after you let go.",
                          juce::dontSendNotification);
            addAndMakeVisible (help);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto top = r.removeFromTop (r.getHeight() / 2);
            r.removeFromTop (6);
            time->setBounds (top.removeFromLeft (top.getWidth() / 2 - 3));
            top.removeFromLeft (6);
            leading->setBounds (top);
            help.setBounds (r.removeFromRight (360).reduced (4, 8));
            r.removeFromRight (6);
            path->setBounds (r);
        }

    private:
        GlideSection* time = nullptr;
        GlideSection* leading = nullptr;
        GlideSection* path = nullptr;
        juce::Label help;
    };

    //==========================================================================
    /** One bipolar sequence step (-24..+24 semitones): drag up / down, double-click resets. */
    class SeqStep : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        SeqStep (juce::RangedAudioParameter& prm, int index)
            : param (prm), number (index), attachment (prm, [this] (float v) { value = juce::roundToInt (v); repaint(); })
        {
            attachment.sendInitialUpdate();
            setTooltip ("Sequence step " + juce::String (index) + ": semitones from the held note (Sequence mode). Drag, double-click = 0.");
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.0f);
            auto label = b.removeFromBottom (16.0f);
            g.setColour (juce::Colour (0xff101116));
            g.fillRoundedRectangle (b, 4.0f);
            const float mid = b.getCentreY(), half = b.getHeight() * 0.5f - 2.0f;
            g.setColour (C::outline.brighter (0.3f));
            g.drawHorizontalLine (juce::roundToInt (mid), b.getX(), b.getRight());
            const float y = mid - half * (float) value / 24.0f;
            g.setColour (C::teal.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (b.getX() + 4.0f, juce::jmin (y, mid), b.getWidth() - 8.0f, std::abs (y - mid) + 1.0f));
            g.setColour (C::text);
            g.setFont (cat::catFont (11.0f, true));
            g.drawText ((value > 0 ? "+" : "") + juce::String (value), label, juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent& e) override { attachment.beginGesture(); setFromY (e.position.y); }
        void mouseDrag (const juce::MouseEvent& e) override { setFromY (e.position.y); }
        void mouseUp (const juce::MouseEvent&) override { attachment.endGesture(); }
        void mouseDoubleClick (const juce::MouseEvent&) override { attachment.setValueAsCompleteGesture (0.0f); }

    private:
        void setFromY (float y)
        {
            auto b = getLocalBounds().toFloat().reduced (2.0f).withTrimmedBottom (16.0f);
            const float half = b.getHeight() * 0.5f - 2.0f;
            const int v = juce::jlimit (-24, 24, juce::roundToInt ((b.getCentreY() - y) / half * 24.0f));
            if (v != value) attachment.setValueAsPartOfGesture ((float) v);
        }

        juce::RangedAudioParameter& param;
        int number, value = 0;
        juce::ParameterAttachment attachment;
    };

    //==========================================================================
    class PilotPage : public GlidePage, private juce::Timer
    {
    public:
        explicit PilotPage (GlideCatProcessor& p) : GlidePage (p)
        {
            pilot = section ("AUTOPILOT", C::accent);
            pilot->leftWidth = 200;
            pilot->addLeft (combo ("pilot", "Mode", 52));
            pilot->addLeft (combo ("pilotSync", "Sync", 52));
            pilot->addLeft (combo ("pilotScale", "Scale", 52));
            pilot->addLeft (combo ("pilotRelease", "Release", 52));
            for (auto [id, cap] : { std::pair { "pilotRate", "Rate" }, { "pilotRandom", "Random" }, { "pilotDensity", "Density" },
                                    { "pilotSpread", "Spread" }, { "pilotSmooth", "Smooth" }, { "pilotRange", "Range" } })
                pilot->addKnob (knob (id, cap, C::accent));

            sequence = section ("SEQUENCE STEPS (semitones)", C::teal);
            sequence->knobMaxHeight = 400;
            for (int s = 1; s <= 8; ++s)
            {
                auto* sl = owned.add (new SeqStep (*p.apvts.getParameter ("step" + juce::String (s)), s));
                sequence->addKnob (sl);
            }

            help.setFont (cat::catFont (12.0f));
            help.setColour (juce::Label::textColourId, C::text);
            help.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (help);
            startTimerHz (5);
            timerCallback();
        }

        void timerCallback() override
        {
            help.setText (juce::String (pilotHelp (juce::roundToInt (paramValue (processor.apvts, "pilot")))) +
                          "\n\nRate sets how often the pilot moves (or Sync to the host tempo). Density is the chance a voice moves "
                          "on each step, Spread offsets the voices in time, Smooth is how much of the step is spent gliding.",
                          juce::dontSendNotification);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            pilot->setBounds (r.removeFromTop (r.getHeight() / 2));
            r.removeFromTop (6);
            help.setBounds (r.removeFromRight (420).reduced (6, 8));
            r.removeFromRight (6);
            sequence->setBounds (r);
        }

    private:
        GlideSection* pilot = nullptr;
        GlideSection* sequence = nullptr;
        juce::Label help;
    };

    //==========================================================================
    class ExpressPage : public GlidePage
    {
    public:
        explicit ExpressPage (GlideCatProcessor& p) : GlidePage (p)
        {
            autoEx = section ("AUTO EXPRESSION (per voice, every note)", C::accent);
            for (auto [id, cap] : { std::pair { "swell", "Swell" }, { "swellTime", "Swell Time" }, { "brighten", "Brighten" },
                                    { "drift", "Drift" }, { "wander", "Wander" } })
                autoEx->addKnob (knob (id, cap, C::accent));

            vib = section ("DELAYED VIBRATO", C::teal);
            for (auto [id, cap] : { std::pair { "vibDepth", "Depth" }, { "vibRate", "Rate" }, { "vibDelay", "Delay" } })
                vib->addKnob (knob (id, cap, C::teal));

            mpe = section ("MPE + CONTROLLERS", C::purple);
            mpe->addKnob (knob ("bendRange", "Bend Range", C::purple));

            help.setFont (cat::catFont (12.0f));
            help.setColour (juce::Label::textColourId, C::textDim);
            help.setJustificationType (juce::Justification::topLeft);
            help.setText ("MPE works out of the box: per-note pitch bend (Bend Range, 48 = MPE standard), pressure (louder + brighter) "
                          "and slide / CC74 (brightness). Without MPE, the pitch wheel bends every voice and the mod wheel adds vibrato.\n\n"
                          "Swell fades each note up over Swell Time, Brighten opens the filter as the note holds, Drift detunes voices "
                          "slowly like an old analog, Wander moves each voice's tone.",
                          juce::dontSendNotification);
            addAndMakeVisible (help);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto top = r.removeFromTop (r.getHeight() / 2);
            r.removeFromTop (6);
            autoEx->setBounds (top.removeFromLeft (top.getWidth() * 3 / 5));
            top.removeFromLeft (6);
            vib->setBounds (top);
            mpe->setBounds (r.removeFromLeft (140));
            r.removeFromLeft (10);
            help.setBounds (r.reduced (4, 8));
        }

    private:
        GlideSection* autoEx = nullptr;
        GlideSection* vib = nullptr;
        GlideSection* mpe = nullptr;
        juce::Label help;
    };

    //==========================================================================
    class ModPage : public GlidePage
    {
    public:
        explicit ModPage (GlideCatProcessor& p) : GlidePage (p)
        {
            for (int m = 1; m <= 4; ++m)
            {
                auto* s = section ("MODULATOR M" + juce::String (m), C::palette (m - 1));
                s->leftWidth = 150;
                s->addLeft (combo ("m" + juce::String (m) + "Type", "", 0));
                s->addLeft (combo ("m" + juce::String (m) + "Mode", "", 0));
                s->addKnob (knob ("m" + juce::String (m) + "Rate", "Rate", C::palette (m - 1)));
                mods.push_back (s);
            }
            matrix = section ("MATRIX  (source -> target, amount)", C::slate);
            for (int k = 1; k <= 8; ++k)
            {
                Row row;
                row.src = combo ("mx" + juce::String (k) + "Src", juce::String (k), 16);
                row.dst = combo ("mx" + juce::String (k) + "Dst", "->", 18);
                row.amt = static_cast<cat::BarSlider*> (owned.add (new cat::BarSlider (p.apvts, "mx" + juce::String (k) + "Amt", "Amt",
                                                                                     &p.midiLearn, 34)));
                for (auto* c : { (juce::Component*) row.src, (juce::Component*) row.dst, (juce::Component*) row.amt })
                    addAndMakeVisible (c);
                rows.push_back (row);
            }
            matrix->toBack();
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto top = r.removeFromTop (110);
            const int w = (top.getWidth() - 18) / 4;
            for (auto* s : mods)
            {
                s->setBounds (top.removeFromLeft (w));
                top.removeFromLeft (6);
            }
            r.removeFromTop (6);
            matrix->setBounds (r);
            auto inner = r.reduced (10, 6).withTrimmedTop (18);
            const int colW = (inner.getWidth() - 20) / 2;
            const int rh = juce::jmin (30, inner.getHeight() / 4);
            for (int k = 0; k < 8; ++k)
            {
                auto cell = juce::Rectangle<int> (inner.getX() + (k / 4) * (colW + 20), inner.getY() + (k % 4) * rh, colW, rh).reduced (0, 2);
                rows[(size_t) k].src->setBounds (cell.removeFromLeft (cell.getWidth() * 30 / 100));
                cell.removeFromLeft (6);
                rows[(size_t) k].dst->setBounds (cell.removeFromLeft (cell.getWidth() * 50 / 100));
                cell.removeFromLeft (6);
                rows[(size_t) k].amt->setBounds (cell);
            }
        }

    private:
        struct Row { cat::ParamCombo* src = nullptr; cat::ParamCombo* dst = nullptr; cat::BarSlider* amt = nullptr; };
        std::vector<GlideSection*> mods;
        GlideSection* matrix = nullptr;
        std::vector<Row> rows;
    };

    //==========================================================================
    class FxPage : public GlidePage
    {
    public:
        explicit FxPage (GlideCatProcessor& p) : GlidePage (p)
        {
            ens = section ("ENSEMBLE", C::accent);
            ens->leftWidth = 130;
            ens->addLeft (combo ("ensMode", "", 0));
            for (auto [id, cap] : { std::pair { "ensMix", "Mix" }, { "ensRate", "Rate" }, { "ensDepth", "Depth" } })
                ens->addKnob (knob (id, cap, C::accent));

            prism = section ("PRISM  (pitch-shifted cascading echoes)", C::teal);
            prism->leftWidth = 130;
            prism->addLeft (combo ("prismI1", "1", 14));
            prism->addLeft (combo ("prismI2", "2", 14));
            prism->addLeft (combo ("prismI3", "3", 14));
            for (auto [id, cap] : { std::pair { "prismMix", "Mix" }, { "prismTime", "Time" }, { "prismFb", "Feedback" } })
                prism->addKnob (knob (id, cap, C::teal));

            rooms = section ("ROOMS", C::purple);
            rooms->leftWidth = 130;
            rooms->addLeft (combo ("roomType", "", 0));
            for (auto [id, cap] : { std::pair { "roomMix", "Mix" }, { "roomSize", "Size" } })
                rooms->addKnob (knob (id, cap, C::purple));
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto top = r.removeFromTop (juce::jmin (r.getHeight(), 150));
            const int w = (top.getWidth() - 12) / 3;
            ens->setBounds (top.removeFromLeft (w));
            top.removeFromLeft (6);
            prism->setBounds (top.removeFromLeft (w + 40));
            top.removeFromLeft (6);
            rooms->setBounds (top);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (C::textDim);
            g.setFont (cat::catFont (12.0f));
            g.drawText ("Signal flow:  Synth  ->  Ensemble  ->  Prism  ->  Rooms  ->  Out.     Shimmer = octave-up feedback;  Bloom = a slow swelling tail.",
                        getLocalBounds().reduced (14).withTrimmedTop (170), juce::Justification::topLeft);
        }

    private:
        GlideSection* ens = nullptr;
        GlideSection* prism = nullptr;
        GlideSection* rooms = nullptr;
    };
}

//==============================================================================
GlideCatEditor::GlideCatEditor (GlideCatProcessor& p)
    : processor (p),
      presetBar (p.presetManager, "Sound"),
      volume (p.apvts, "volume", "Vol", &p.midiLearn, 30),
      view (p),
      catView (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, cat::CatStyle::Glide),
      surpriseControls (p, p.surpriseUndo, [&p] { p.surprise(); }),
      bootScreen (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); },
                  "GLIDE", "-cat", "CHORDS THAT GLIDE INTO EACH OTHER", cat::CatStyle::Glide)
{
    setLookAndFeel (&lnf);

    const int numGlides = (int) GlideCatProcessor::glidePresets().size();
    glideStepper = std::make_unique<cat::Stepper> ("Glide", 0, numGlides - 1,
        [&p] { return p.currentGlide.load(); },
        [&p] (int i) { p.applyGlidePreset (i); },
        [] (int i) { return i < 0 ? juce::String ("(dice roll)") : juce::String (GlideCatProcessor::glidePresets()[(size_t) i].name); });
    glideStepper->setTooltip ("Glide preset: how the voices move (glide, paths, autopilot). Independent from the sound preset.");
    voices = cat::Stepper::forParameter (p.apvts, "voices", "Voices");
    voices->setTooltip ("Number of voices (each voice is one gliding line)");
    diceButton.setTooltip ("Roll random glide settings (keeps the sound)");
    diceButton.onClick = [&p] { p.glideDice(); };

    tabs.addTab ("SYNTH", C::panel, new SynthPage (p), true);
    tabs.addTab ("GLIDE", C::panel, new GlideTabPage (p), true);
    tabs.addTab ("PILOT", C::panel, new PilotPage (p), true);
    tabs.addTab ("EXPRESS", C::panel, new ExpressPage (p), true);
    tabs.addTab ("MOD", C::panel, new ModPage (p), true);
    tabs.addTab ("FX", C::panel, new FxPage (p), true);
    tabs.setTabBarDepth (30);
    tabs.setOutline (0);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, glideStepper.get(), &diceButton, voices.get(), &volume, &view, &tabs })
        addAndMakeVisible (c);
    addAndMakeVisible (catView);
    addAndMakeVisible (surpriseControls);
    addAndMakeVisible (bootScreen);
    setSize (1180, 780);
}

GlideCatEditor::~GlideCatEditor()
{
    setLookAndFeel (nullptr);
}

void GlideCatEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::background);
    cat::CatLookAndFeel::drawLogo (g, juce::Rectangle<float> (14.0f, 8.0f, 160.0f, 32.0f), "GLIDE", "-cat");
}

void GlideCatEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto item = [] (juce::Component& c, float w, float h = 30.0f) { return juce::FlexItem (c).withWidth (w).withHeight (h).withMargin ({ 0, 3, 0, 3 }); };
    {
        auto top = r.removeFromTop (40);
        auto logo = top.removeFromLeft (160);
        catView.setBounds (logo.removeFromRight (40).expanded (0, 6));
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        for (auto it : { item (presetBar, 330), item (*glideStepper, 250), item (diceButton, 56), juce::FlexItem().withFlex (1.0f),
                         item (*voices, 110), item (volume, 130), item (surpriseControls, 122) })
            fb.items.add (it);
        fb.performLayout (top);
    }
    r.removeFromTop (8);
    tabs.setBounds (r.removeFromBottom (330));
    r.removeFromBottom (8);
    view.setBounds (r);

    bootScreen.setBounds (getLocalBounds());
}

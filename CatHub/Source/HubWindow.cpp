#include "HubWindow.h"

namespace hub
{
namespace
{
    juce::Colour sourceColour (int src)
    {
        if (src == SrcCounter) return theme().source[4].withRotatedHue (-0.14f);
        return theme().source[(size_t) juce::jlimit (0, 4, src)];
    }

    int defaultSoundFor (int src)
    {
        switch (src)
        {
            case SrcBass:  return SndSub;
            case SrcDrums: return SndBoomBap;
            case SrcLife:  return SndLifeOwn;
            case SrcLead:  return SndPluck;
            case SrcCounter: return SndPluck;
            default:       return SndPad;
        }
    }

    /** A little cat face with a sign of what it plays. */
    void drawCatIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, int src, float bounce)
    {
        const float s = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (s, s).translated (0.0f, -bounce * s * 0.05f);
        const auto head = r.reduced (s * 0.14f).withTrimmedTop (s * 0.1f);
        juce::Path ears;
        const float ew = head.getWidth() * 0.3f;
        ears.addTriangle (head.getX() + ew * 0.1f, head.getY() + ew * 0.7f, head.getX() + ew * 0.45f, head.getY() - ew * 0.7f, head.getX() + ew * 1.25f, head.getY() + ew * 0.1f);
        ears.addTriangle (head.getRight() - ew * 0.1f, head.getY() + ew * 0.7f, head.getRight() - ew * 0.45f, head.getY() - ew * 0.7f, head.getRight() - ew * 1.25f, head.getY() + ew * 0.1f);
        g.setColour (c);
        g.fillPath (ears);
        g.fillEllipse (head);
        g.setColour (theme().bg);
        const float eyeY = head.getY() + head.getHeight() * 0.42f, eyeW = head.getWidth() * 0.12f;
        const float blink = bounce > 0.85f ? 0.3f : 1.0f;
        g.fillEllipse (head.getX() + head.getWidth() * 0.3f - eyeW * 0.5f, eyeY - eyeW * 0.5f * blink, eyeW, eyeW * blink);
        g.fillEllipse (head.getX() + head.getWidth() * 0.7f - eyeW * 0.5f, eyeY - eyeW * 0.5f * blink, eyeW, eyeW * blink);
        const float cx = head.getCentreX(), my = head.getY() + head.getHeight() * 0.68f;
        juce::Path mouth;
        mouth.startNewSubPath (cx - eyeW, my);
        mouth.quadraticTo (cx - eyeW * 0.5f, my + eyeW * 0.6f, cx, my);
        mouth.quadraticTo (cx + eyeW * 0.5f, my + eyeW * 0.6f, cx + eyeW, my);
        g.strokePath (mouth, juce::PathStrokeType (juce::jmax (1.2f, s * 0.03f)));
        g.setColour (c.brighter (0.45f));
        const float k = s * 0.09f;
        switch (src)
        {
            case SrcChords: for (int i = 0; i < 3; ++i) g.fillEllipse (r.getRight() - k * 1.6f, r.getY() + k * (0.3f + 1.1f * i), k * 1.2f, k * 0.85f); break;
            case SrcBass:   g.fillRect (r.getX() + k, r.getBottom() - 3.0f, r.getWidth() - 2 * k, 2.0f); break;
            case SrcDrums:  g.drawLine (r.getRight() - k * 3, r.getY() + k, r.getRight(), r.getY() + k * 3, 2.0f);
                            g.drawLine (r.getRight() - k * 3, r.getY() + k * 3, r.getRight(), r.getY() + k, 2.0f); break;
            case SrcLife:   g.fillEllipse (r.getRight() - k * 2.4f, r.getY() + k * 0.4f, k * 2.0f, k * 1.1f); break;
            default:        { juce::Path star; star.addStar ({ r.getRight() - k * 1.2f, r.getY() + k * 1.2f }, 5, k * 0.5f, k * 1.2f); g.fillPath (star); }
        }
    }

    juce::String noteName (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 3); }

    juce::String chordName (const std::vector<int>& notes)
    {
        if (notes.empty()) return {};
        int mask = 0;
        for (int n : notes) mask |= 1 << (n % 12);
        struct T { int bits; const char* name; };
        auto b = [] (std::initializer_list<int> iv) { int m = 0; for (int i : iv) m |= 1 << i; return m; };
        const T types[] = {
            { b ({ 0, 4, 7 }), "" }, { b ({ 0, 3, 7 }), "m" }, { b ({ 0, 3, 6 }), "dim" }, { b ({ 0, 4, 8 }), "aug" },
            { b ({ 0, 2, 7 }), "sus2" }, { b ({ 0, 5, 7 }), "sus4" }, { b ({ 0, 4, 7, 10 }), "7" }, { b ({ 0, 4, 7, 11 }), "maj7" },
            { b ({ 0, 3, 7, 10 }), "m7" }, { b ({ 0, 3, 6, 10 }), "m7b5" }, { b ({ 0, 3, 6, 9 }), "dim7" }, { b ({ 0, 4, 7, 9 }), "6" },
            { b ({ 0, 3, 7, 9 }), "m6" }, { b ({ 0, 2, 4, 7, 10 }), "9" }, { b ({ 0, 2, 4, 7, 11 }), "maj9" }, { b ({ 0, 2, 3, 7, 10 }), "m9" },
            { b ({ 0, 2, 4, 7 }), "add9" }, { b ({ 0, 2, 3, 7 }), "madd9" }, { b ({ 0, 3, 7, 11 }), "mMaj7" }, { b ({ 0, 4, 10 }), "7" },
            { b ({ 0, 4, 11 }), "maj7" }, { b ({ 0, 3, 10 }), "m7" }, { b ({ 0, 7 }), "5" } };
        static const char* pcs[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        const int bass = *std::min_element (notes.begin(), notes.end()) % 12;
        for (int pass = 0; pass < 2; ++pass)
            for (int root = 0; root < 12; ++root)
            {
                if ((pass == 0) != (root == bass)) continue;
                const int rel = ((mask >> root) | (mask << (12 - root))) & 0xfff;
                for (const auto& t : types)
                    if (rel == t.bits)
                        return juce::String (pcs[root]) + t.name + (root != bass ? "/" + juce::String (pcs[bass]) : juce::String());
            }
        return {};
    }

    juce::String drumName (int n)
    {
        switch (n)
        {
            case 35: case 36: return "Kick";
            case 38: case 40: return "Snare";
            case 39: return "Clap";
            case 37: return "Rim";
            case 42: case 44: return "Hat";
            case 46: return "Open hat";
            case 49: case 52: case 55: case 57: return "Crash";
            case 51: case 53: case 59: return "Ride";
            case 41: case 43: case 45: case 47: case 48: case 50: return "Tom";
            case 56: return "Cowbell";
            case 69: case 70: case 82: return "Shaker";
            default: return "Perc";
        }
    }

    void label (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, juce::Justification j = juce::Justification::bottomLeft)
    {
        g.setColour (theme().textDim);
        g.setFont (hubFont (10.5f, true));
        g.drawText (text, r, j);
    }

    void styleFader (juce::Slider& s, const juce::String& tip)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRange (0.0, 1.0, 0.001);
        s.setTooltip (tip);
        s.setDoubleClickReturnValue (true, 0.75);
    }
}

//==============================================================================
CaptionKnob::CaptionKnob (const juce::String& c, const juce::String& tip, bool bipolar) : caption (c)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange (0.0, 1.0, 0.001);
    knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    knob.setTooltip (tip);
    knob.setDoubleClickReturnValue (true, bipolar ? 0.5 : 0.0);
    knob.getProperties().set ("bipolar", bipolar);
    addAndMakeVisible (knob);
}

void CaptionKnob::resized()
{
    knob.setBounds (getLocalBounds().withTrimmedBottom (14));
}

void CaptionKnob::paint (juce::Graphics& g)
{
    g.setColour (theme().textDim);
    g.setFont (hubFont (9.5f, true));
    g.drawText (caption, getLocalBounds().removeFromBottom (14), juce::Justification::centred);
}

//==============================================================================
RackRow::RackRow (HubMain& o, HubEngine& e, int i) : owner (o), engine (e), index (i)
{
    for (int s = 0; s < NumSources; ++s) sourceBox.addItem (sourceName (s), s + 1);
    sourceBox.setTooltip ("Where this part's notes come from");
    sourceBox.onChange = [this]
    {
        if (updating) return;
        const int src = sourceBox.getSelectedId() - 1;
        auto& t = engine.track (index);
        if (src < 0 || src == t.source.load()) return;
        const int oldSound = t.sound.load();
        // Life's own sound only fits the Life source, and drum kits only fit drums
        const bool soundFits = ((src == SrcLife) == (oldSound == SndLifeOwn))
                            && ((src == SrcDrums) == (soundGroup (oldSound) == 2));
        t.source = src;
        if (! soundFits)
        {
            const int i = index;
            owner.changeCatsLater ([&eng = engine, i, src] { eng.setSound (i, defaultSoundFor (src)); });
        }
        else owner.refreshAll();
    };

    for (int f = 0; f < NumMidiFx; ++f) fxBox.addItem (midiFxName (f), f + 1);
    fxBox.setTooltip ("A Cat that changes the notes before they sound: Patt-Cat plays patterns, Bend-Cat bends, Orbit-Cat arpeggiates");
    fxBox.onChange = [this]
    {
        if (updating) return;
        const int fx = fxBox.getSelectedId() - 1, i = index;
        if (fx < 0 || fx == engine.track (index).midiFx.load()) return;
        owner.changeCatsLater ([&eng = engine, i, fx] { eng.setMidiFx (i, fx); });
    };

    const auto groups = soundGroups();
    for (int grp = 0; grp < groups.size(); ++grp)
    {
        soundBox.addSectionHeading (groups[grp]);
        for (int s = 0; s < NumSounds; ++s)
            if (soundGroup (s) == grp) soundBox.addItem (soundName (s), s + 1);
    }
    soundBox.setTooltip ("What plays the notes: built-in keys, bass and drum kits, or Glide-Cat, Sing-Cat and Talk-Cat");
    soundBox.onChange = [this]
    {
        if (updating) return;
        const int s = soundBox.getSelectedId() - 1, i = index;
        if (s < 0 || s == engine.track (index).sound.load()) return;
        owner.changeCatsLater ([&eng = engine, i, s] { if (! eng.setSound (i, s)) eng.setSound (i, SndPad); });
    };

    fxPreset.setTooltip ("The MIDI-effect Cat's preset");
    fxPreset.onChange = [this]
    {
        if (updating) return;
        if (auto* c = engine.track (index).fxCat.get())
            if (fxPreset.getSelectedId() > 0) c->setCurrentProgram (fxPreset.getSelectedId() - 1);
    };
    soundPreset.setTooltip ("The sound Cat's preset");
    soundPreset.onChange = [this]
    {
        if (updating) return;
        if (auto* c = soundPresetCat())
            if (soundPreset.getSelectedId() > 0)
            {
                c->setCurrentProgram (soundPreset.getSelectedId() - 1);
                if (c == engine.getCoreCat (LifeCat)) engine.keepLifeRunning();
            }
    };

    fxEdit.setTooltip ("Open this Cat's own window");
    soundEdit.setTooltip ("Open this Cat's own window");
    fxEdit.onClick = [this] { owner.showCatWindow (engine.track (index).fxCat.get()); };
    soundEdit.onClick = [this] { owner.showCatWindow (soundPresetCat()); };

    for (auto* b : { &mute, &solo })
    {
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonOnColourId, b == &mute ? juce::Colour (0xffd0605a) : theme().accent);
    }
    mute.setTooltip ("Mute this part");
    solo.setTooltip ("Solo: hear only the soloed parts");
    mute.onClick = [this] { engine.track (index).mute = mute.getToggleState(); };
    solo.onClick = [this] { engine.track (index).solo = solo.getToggleState(); };
    remove.setTooltip ("Remove this part");
    remove.onClick = [this] { const int i = index; owner.changeCatsLater ([&eng = engine, i] { eng.removeTrack (i); }); };

    styleFader (volume, "Volume");
    volume.onValueChange = [this] { engine.track (index).volume = (float) volume.getValue(); };

    auto bind = [this] (CaptionKnob& k, std::atomic<float> Track::* member)
    {
        k.knob.onValueChange = [this, &k, member] { (engine.track (index).*member) = (float) k.knob.getValue(); };
    };
    bind (filter, &Track::filter); bind (drive, &Track::drive); bind (chorus, &Track::chorus);
    bind (echo, &Track::delay); bind (crush, &Track::crush); bind (room, &Track::reverb); bind (pan, &Track::pan);

    for (juce::Component* c : { (juce::Component*) &sourceBox, (juce::Component*) &fxBox, (juce::Component*) &fxPreset, (juce::Component*) &soundBox,
                                (juce::Component*) &soundPreset, (juce::Component*) &fxEdit, (juce::Component*) &soundEdit, (juce::Component*) &mute,
                                (juce::Component*) &solo, (juce::Component*) &remove, (juce::Component*) &volume, (juce::Component*) &filter,
                                (juce::Component*) &drive, (juce::Component*) &chorus, (juce::Component*) &echo, (juce::Component*) &crush,
                                (juce::Component*) &room, (juce::Component*) &pan })
        addChildComponent (c);
    refresh();
}

juce::AudioPluginInstance* RackRow::soundPresetCat()
{
    auto& t = engine.track (index);
    if (t.sound.load() == SndLifeOwn) return engine.getCoreCat (LifeCat);
    return t.soundCat.get();
}

void RackRow::fillPresets (juce::ComboBox& box, juce::AudioPluginInstance* c)
{
    box.clear (juce::dontSendNotification);
    if (c == nullptr || c->getNumPrograms() <= 1)
    {
        box.addItem (c == nullptr ? "-" : "(no presets)", 1);
        box.setSelectedId (1, juce::dontSendNotification);
        box.setEnabled (false);
        return;
    }
    for (int i = 0; i < c->getNumPrograms(); ++i) box.addItem (juce::String (i + 1) + "  " + c->getProgramName (i), i + 1);
    box.setSelectedId (juce::jlimit (0, c->getNumPrograms() - 1, c->getCurrentProgram()) + 1, juce::dontSendNotification);
    box.setEnabled (true);
}

void RackRow::refresh()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    auto& t = engine.track (index);
    const int src = t.source.load();
    sourceBox.setSelectedId (src + 1, juce::dontSendNotification);
    sourceBox.setItemEnabled (SrcLife + 1, engine.hasCat (LifeCat));
    fxBox.setSelectedId (t.midiFx.load() + 1, juce::dontSendNotification);
    for (int f = 1; f < NumMidiFx; ++f) fxBox.setItemEnabled (f + 1, engine.catAvailable (catForMidiFx (f)));
    soundBox.setSelectedId (t.sound.load() + 1, juce::dontSendNotification);
    for (int s = 0; s < NumSounds; ++s)
    {
        const int c = catForSound (s);
        soundBox.setItemEnabled (s + 1, (c < 0 || engine.catAvailable (c)) && (s != SndLifeOwn || src == SrcLife));
    }
    fillPresets (fxPreset, t.fxCat.get());
    fillPresets (soundPreset, soundPresetCat());
    fxEdit.setEnabled (t.fxCat != nullptr);
    soundEdit.setEnabled (soundPresetCat() != nullptr);
    mute.setToggleState (t.mute.load(), juce::dontSendNotification);
    solo.setToggleState (t.solo.load(), juce::dontSendNotification);
    volume.setValue (t.volume.load(), juce::dontSendNotification);
    volume.getProperties().set ("fill", (juce::int64) sourceColour (src).getARGB());
    filter.knob.setValue (t.filter.load(), juce::dontSendNotification);
    drive.knob.setValue (t.drive.load(), juce::dontSendNotification);
    chorus.knob.setValue (t.chorus.load(), juce::dontSendNotification);
    echo.knob.setValue (t.delay.load(), juce::dontSendNotification);
    crush.knob.setValue (t.crush.load(), juce::dontSendNotification);
    room.knob.setValue (t.reverb.load(), juce::dontSendNotification);
    pan.knob.setValue (t.pan.load(), juce::dontSendNotification);

    const bool adv = owner.isAdvanced();
    for (juce::Component* c : { (juce::Component*) &sourceBox, (juce::Component*) &fxBox, (juce::Component*) &fxPreset, (juce::Component*) &fxEdit,
                                (juce::Component*) &filter, (juce::Component*) &drive, (juce::Component*) &chorus, (juce::Component*) &echo,
                                (juce::Component*) &crush, (juce::Component*) &room, (juce::Component*) &pan, (juce::Component*) &remove })
        c->setVisible (adv);
    for (juce::Component* c : { (juce::Component*) &soundBox, (juce::Component*) &soundPreset, (juce::Component*) &soundEdit,
                                (juce::Component*) &mute, (juce::Component*) &solo, (juce::Component*) &volume })
        c->setVisible (true);
    remove.setEnabled (engine.getNumTracks() > 1);
    resized();
    repaint();
}

void RackRow::tick()
{
    auto& t = engine.track (index);
    if (++ticks % 3 == 0)
    {
        const auto notes = t.takeNotes();
        juce::String big, small;
        const int src = t.source.load();
        if (src == SrcDrums)
        {
            juce::StringArray names;
            for (int n : notes) names.addIfNotAlreadyThere (drumName (n));
            if (! names.isEmpty()) { big = names.joinIntoString ("  "); lastHit = juce::Time::getMillisecondCounter(); }
            else if (juce::Time::getMillisecondCounter() - lastHit < 400) big = nowBig;
        }
        else if (! notes.empty())
        {
            juce::StringArray names;
            for (int n : notes) names.add (noteName (n));
            if (src == SrcBass || src == SrcLead) big = noteName (*std::min_element (notes.begin(), notes.end()));
            else
            {
                big = src == SrcChords && t.midiFx.load() == FxNone ? chordName (notes) : juce::String();
                small = names.joinIntoString (" ");
                if (big.isEmpty()) { big = small; small = {}; }
            }
        }
        if (big != nowBig || small != nowSmall) { nowBig = big; nowSmall = small; repaint (nowArea.getUnion (nameArea)); }
    }
    const float m = t.meter.load();
    const float shown = juce::jmax (m, shownMeter * 0.88f);
    if (std::abs (shown - shownMeter) > 0.002f) { shownMeter = shown; repaint (meterArea.getUnion (iconArea)); }
}

void RackRow::paint (juce::Graphics& g)
{
    const auto& th = theme();
    auto& t = engine.track (index);
    const int src = t.source.load();
    const auto c = sourceColour (src);
    auto r = getLocalBounds().toFloat().reduced (2.0f, 3.0f);

    // a rack unit: body, ears with screws, colour strip
    juce::ColourGradient body (th.panelHi, r.getX(), r.getY(), th.panel, r.getX(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (th.outline);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
    for (auto ear : { r.withWidth (16.0f), r.withLeft (r.getRight() - 16.0f) })
    {
        g.setColour (th.rackEar);
        g.fillRoundedRectangle (ear.reduced (2.0f, 2.0f), 6.0f);
        for (float y : { ear.getY() + 12.0f, ear.getBottom() - 12.0f })
        {
            g.setColour (th.screw);
            g.fillEllipse (ear.getCentreX() - 3.5f, y - 3.5f, 7.0f, 7.0f);
            g.setColour (th.rackEar);
            g.drawLine (ear.getCentreX() - 2.5f, y, ear.getCentreX() + 2.5f, y, 1.2f);
        }
    }
    g.setColour (c);
    g.fillRoundedRectangle (r.getX() + 18.0f, r.getY() + 10.0f, 4.0f, r.getHeight() - 20.0f, 2.0f);
    if (t.mute.load()) { g.setColour (th.bg.withAlpha (0.35f)); g.fillRoundedRectangle (r, 8.0f); }

    drawCatIcon (g, iconArea.toFloat(), c, src, juce::jlimit (0.0f, 1.0f, shownMeter * 2.5f));

    // name + what it is
    auto na = nameArea;
    g.setColour (th.text);
    g.setFont (hubFont (owner.isAdvanced() ? 14.0f : 17.0f, true));
    const juce::String title = juce::String (index + 1) + "  " + sourceName (src);
    if (owner.isAdvanced())
        g.drawText (title, na.removeFromTop (18), juce::Justification::centredLeft);
    else
    {
        g.drawText (title, na.removeFromTop (na.getHeight() / 2), juce::Justification::bottomLeft);
        g.setColour (th.textDim);
        g.setFont (hubFont (12.0f));
        juce::String sub = soundName (t.sound.load());
        if (t.midiFx.load() != FxNone) sub = juce::String (catName (catForMidiFx (t.midiFx.load()))) + "  >  " + sub;
        g.drawText (sub, na, juce::Justification::topLeft);
    }

    // now playing
    if (! nowArea.isEmpty())
    {
        auto a = nowArea.toFloat();
        g.setColour (th.bg.withAlpha (th.light ? 0.35f : 0.55f));
        g.fillRoundedRectangle (a, 7.0f);
        const bool idle = nowBig.isEmpty();
        g.setColour (idle ? th.textDim : c);
        const float big = owner.isAdvanced() ? 16.0f : (src == SrcDrums ? 18.0f : 24.0f);
        g.setFont (hubFont (big, true));
        if (nowSmall.isEmpty() || nowArea.getHeight() < 40)
            g.drawFittedText (idle ? juce::String ("-") : nowBig, nowArea.reduced (8, 2), juce::Justification::centred, 2);
        else
        {
            auto ar = nowArea.reduced (8, 4);
            g.drawFittedText (nowBig, ar.removeFromTop (ar.getHeight() * 6 / 10), juce::Justification::centredBottom, 1);
            g.setColour (th.textDim);
            g.setFont (hubFont (11.5f));
            g.drawFittedText (nowSmall, ar, juce::Justification::centredTop, 1);
        }
    }

    // meter: LED segments
    if (! meterArea.isEmpty())
    {
        const int segs = 12;
        const float lv = juce::jlimit (0.0f, 1.0f, std::sqrt (shownMeter));
        const float sh = (float) meterArea.getHeight() / segs;
        for (int k = 0; k < segs; ++k)
        {
            auto seg = juce::Rectangle<float> ((float) meterArea.getX(), (float) meterArea.getBottom() - (k + 1) * sh, (float) meterArea.getWidth(), sh).reduced (0.0f, 1.0f);
            const bool lit = (float) (k + 1) / segs <= lv + 0.001f;
            const auto col = k >= segs - 1 ? juce::Colour (0xffe0605a) : (k >= segs - 3 ? juce::Colour (0xffe8c25a) : c);
            g.setColour (lit ? col : th.raised);
            g.fillRoundedRectangle (seg, 1.5f);
        }
    }

    if (owner.isAdvanced())
    {
        label (g, fxBox.getBounds().withY (fxBox.getY() - 15).withHeight (14), "MIDI EFFECT CAT");
        label (g, soundBox.getBounds().withY (soundBox.getY() - 15).withHeight (14), "SOUND");
        label (g, volume.getBounds().withY (volume.getY() - 15).withHeight (14), "VOLUME");
    }
    else
    {
        label (g, soundBox.getBounds().withY (soundBox.getY() - 15).withHeight (14), "SOUND  &  PRESET");
        label (g, volume.getBounds().withY (volume.getY() - 15).withHeight (14), "VOLUME");
        label (g, nowArea.withY (nowArea.getY() - 15).withHeight (14), "NOW PLAYING");
    }
}

void RackRow::resized()
{
    auto r = getLocalBounds().reduced (26, 8);   // inside the rack ears
    r.removeFromLeft (4);
    if (owner.isAdvanced())
    {
        iconArea = r.removeFromLeft (44).withSizeKeepingCentre (44, 44);
        r.removeFromLeft (8);
        auto col = [&r] (int w) { auto c = r.removeFromLeft (w); r.removeFromLeft (10); return c; };
        {
            auto c = col (150);
            nameArea = c.removeFromTop (20);
            c.removeFromTop (2);
            sourceBox.setBounds (c.removeFromTop (26));
            c.removeFromTop (4);
            nowArea = c;
        }
        {
            auto c = col (175).withTrimmedTop (16);
            fxBox.setBounds (c.removeFromTop (26));
            c.removeFromTop (5);
            auto row = c.removeFromTop (26);
            fxEdit.setBounds (row.removeFromRight (42));
            row.removeFromRight (4);
            fxPreset.setBounds (row);
        }
        {
            auto c = col (190).withTrimmedTop (16);
            soundBox.setBounds (c.removeFromTop (26));
            c.removeFromTop (5);
            auto row = c.removeFromTop (26);
            soundEdit.setBounds (row.removeFromRight (42));
            row.removeFromRight (4);
            soundPreset.setBounds (row);
        }
        auto right = r.removeFromRight (36);
        remove.setBounds (right.withSizeKeepingCentre (28, 28));
        r.removeFromRight (6);
        auto ms = r.removeFromRight (34);
        mute.setBounds (ms.removeFromTop (ms.getHeight() / 2).reduced (0, 3));
        solo.setBounds (ms.reduced (0, 3));
        r.removeFromRight (8);
        meterArea = r.removeFromRight (8).reduced (0, 6);
        r.removeFromRight (6);
        volume.setBounds (r.removeFromRight (juce::jlimit (90, 160, r.getWidth() - 7 * 50)).withTrimmedTop (16).withSizeKeepingCentre (juce::jlimit (90, 160, r.getWidth()), 30));
        r.removeFromRight (8);
        const int kw = juce::jlimit (36, 52, r.getWidth() / 7);
        for (auto* k : { &filter, &drive, &chorus, &echo, &crush, &room, &pan })
        {
            k->setBounds (r.removeFromLeft (kw).withSizeKeepingCentre (kw, juce::jmin (r.getHeight(), kw + 14)));
            if (k == &crush) r.removeFromLeft (8);
        }
    }
    else
    {
        iconArea = r.removeFromLeft (60).withSizeKeepingCentre (60, 60);
        r.removeFromLeft (10);
        nameArea = r.removeFromLeft (170);
        r.removeFromLeft (10);
        {
            auto c = r.removeFromLeft (230).withTrimmedTop (16);
            soundBox.setBounds (c.removeFromTop (28));
            c.removeFromTop (5);
            auto row = c.removeFromTop (26);
            soundEdit.setBounds (row.removeFromRight (46));
            row.removeFromRight (4);
            soundPreset.setBounds (row);
        }
        r.removeFromLeft (14);
        auto ms = r.removeFromRight (40);
        mute.setBounds (ms.removeFromTop (ms.getHeight() / 2).reduced (0, 4));
        solo.setBounds (ms.reduced (0, 4));
        r.removeFromRight (10);
        meterArea = r.removeFromRight (9).reduced (0, 6);
        r.removeFromRight (8);
        volume.setBounds (r.removeFromRight (juce::jlimit (120, 220, r.getWidth() / 3)).withTrimmedTop (16).withSizeKeepingCentre (juce::jlimit (120, 220, r.getWidth() / 3), 30));
        r.removeFromRight (14);
        nowArea = r.withTrimmedTop (16);
    }
}

//==============================================================================
RackView::RackView (HubMain& o, HubEngine& e) : owner (o), engine (e)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    addButton.setTooltip ("Add another part: e.g. a lead line, a second pad or a Cat as an effect");
    addButton.onClick = [this]
    {
        owner.changeCatsLater ([&eng = engine]
        {
            const bool patt = eng.catAvailable (PattCat);
            eng.addTrack (patt ? SrcChords : SrcLead, patt ? FxPatt : FxNone, SndPluck);
            auto& t = eng.track (eng.getNumTracks() - 1);
            t.volume = 0.6f; t.reverb = 0.25f; t.delay = 0.2f;
        });
    };
    content.addChildComponent (addButton);
    rebuild();
}

void RackView::rebuild()
{
    rows.clear();
    for (int i = 0; i < engine.getNumTracks(); ++i) content.addAndMakeVisible (rows.add (new RackRow (owner, engine, i)));
    resized();
}

void RackView::refresh() { for (auto* r : rows) r->refresh(); resized(); }
void RackView::tick()    { for (auto* r : rows) r->tick(); }

void RackView::resized()
{
    viewport.setBounds (getLocalBounds());
    const int rowH = owner.isAdvanced() ? 100 : 90;
    const int w = viewport.getMaximumVisibleWidth();
    int y = 6;
    for (auto* r : rows) { r->setBounds (8, y, w - 16, rowH); y += rowH + 4; }
    const bool canAdd = owner.isAdvanced() && engine.getNumTracks() < maxTracks;
    addButton.setVisible (canAdd);
    if (canAdd) { addButton.setBounds (8 + (w - 16) / 2 - 110, y + 6, 220, 34); y += 48; }
    content.setSize (w, y + 6);
}

void RackView::paint (juce::Graphics& g) { g.fillAll (theme().bg); }

//==============================================================================
int PadStrip::padAt (juce::Point<float> p) const
{
    const int n = engine.getNumPads();
    return juce::jlimit (0, n - 1, (int) (p.x / ((float) getWidth() / (float) n)));
}

void PadStrip::paint (juce::Graphics& g)
{
    const auto& th = theme();
    const int n = engine.getNumPads(), cur = engine.getCurrentPad();
    const float w = (float) getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto r = juce::Rectangle<float> (i * w, 0.0f, w, (float) getHeight()).reduced (4.0f, 3.0f);
        const bool on = i == cur;
        if (on)
        {
            juce::ColourGradient grad (th.accent.brighter (0.2f), r.getX(), r.getY(), th.accent.darker (0.2f), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
        }
        else g.setColour (th.raised);
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (on ? th.accent.brighter (0.4f) : th.outline);
        g.drawRoundedRectangle (r, 9.0f, 1.0f);
        g.setColour (on ? (th.light ? juce::Colours::white : th.bg) : th.textDim);
        g.setFont (hubFont (19.0f, true));
        g.drawText (juce::String (i + 1), r.removeFromTop (r.getHeight() * 0.62f), juce::Justification::centredBottom);
        g.setFont (hubFont (10.0f));
        g.drawText (i < 9 ? "key " + juce::String (i + 1) : juce::String(), r, juce::Justification::centredTop);
    }
}

void PadStrip::mouseDown (const juce::MouseEvent& e) { engine.setLivePad (padAt (e.position)); }
void PadStrip::mouseUp (const juce::MouseEvent&)     { engine.setLivePad (-1); }

void PadStrip::tick()
{
    if (engine.getCurrentPad() != shownPad || engine.getNumPads() != shownPads)
    {
        shownPad = engine.getCurrentPad();
        shownPads = engine.getNumPads();
        repaint();
    }
}

//==============================================================================
CatTab::CatTab (juce::AudioPluginInstance& plugin)
{
    editor.reset (plugin.createEditorAndMakeActive());
    if (editor != nullptr)
    {
        naturalW = editor->getWidth();
        naturalH = editor->getHeight();
        addAndMakeVisible (editor.get());
    }
}

CatTab::~CatTab() { editor.reset(); }     // the Cat's window always goes before the Cat itself

void CatTab::paint (juce::Graphics& g)
{
    g.fillAll (theme().bg);
    if (editor == nullptr)
    {
        g.setColour (theme().textDim);
        g.setFont (hubFont (14.0f));
        g.drawText ("This Cat has no window", getLocalBounds(), juce::Justification::centred);
    }
}

void CatTab::resized()
{
    if (editor == nullptr || naturalW <= 0 || naturalH <= 0 || getWidth() <= 0 || getHeight() <= 0) return;
    // Cat windows scale as one piece: make it as big as fits (up to 1.5x), aspect kept, centred.
    if (editor->isResizable())
    {
        const float k = juce::jmin (1.5f, juce::jmin ((float) getWidth() / (float) naturalW, (float) getHeight() / (float) naturalH));
        const int w = juce::jmax (100, (int) ((float) naturalW * k)), h = juce::jmax (80, (int) ((float) naturalH * k));
        if (w != editor->getWidth() || h != editor->getHeight()) editor->setSize (w, h);
    }
    editor->setTopLeftPosition (juce::jmax (0, (getWidth() - editor->getWidth()) / 2), juce::jmax (0, (getHeight() - editor->getHeight()) / 2));
}

//==============================================================================
namespace
{
    juce::Colour partColour (int part)
    {
        switch (part)
        {
            case cat::muse::Chords:  return sourceColour (SrcChords);
            case cat::muse::Bass:    return sourceColour (SrcBass);
            case cat::muse::Drums:   return sourceColour (SrcDrums);
            case cat::muse::Melody:  return sourceColour (SrcLead);
            default:                 return sourceColour (SrcCounter);
        }
    }

    juce::Colour sectionColour (int type)
    {
        static const float hues[] = { 0.58f, 0.52f, 0.12f, 0.95f, 0.75f, 0.08f, 0.0f, 0.62f, 0.55f };
        return juce::Colour::fromHSV (hues[juce::jlimit (0, 8, type)], theme().light ? 0.35f : 0.45f, theme().light ? 0.92f : 0.55f, 1.0f);
    }
}

ComposePage::ComposePage (HubMain& o, HubEngine& e) : owner (o), engine (e)
{
    genreBox.addItemList (cat::muse::genreNames(), 1);
    genreBox.setSelectedId (1, juce::dontSendNotification);
    genreBox.setTooltip ("What kind of music the song is");
    moodBox.addItemList (cat::muse::moodNames(), 1);
    moodBox.setSelectedId (1, juce::dontSendNotification);
    moodBox.setTooltip ("How the song feels: it picks the scale, the chords and the words");
    seedBox.setInputRestrictions (7, "0123456789");
    seedBox.setText ("1", false);
    seedBox.setJustification (juce::Justification::centred);
    seedBox.setTooltip ("The song's number: the same number, genre and mood always write the same song. Share it with a friend!");
    seedBox.onReturnKey = [this] { writeButton.triggerClick(); };
    newSeedButton.setTooltip ("A new number: a new song in the same genre and mood");
    newSeedButton.onClick = [this]
    {
        seedBox.setText (juce::String (1 + juce::Random().nextInt (999999)), false);
        writeButton.triggerClick();
    };
    for (auto* b : { &songButton, &endlessButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (202);
    }
    songButton.setToggleState (true, juce::dontSendNotification);
    songButton.setTooltip ("A whole song: intro, verses, choruses, bridge, outro (it starts again at the end)");
    endlessButton.setTooltip ("Music that never ends: it keeps writing new parts while it plays, with the same key and hook");
    auto modeChanged = [this]
    {
        if (engine.isSongMode() && engine.getSongSettings().endless != endlessButton.getToggleState())
        {
            engine.setSongEndless (endlessButton.getToggleState());
            owner.songChanged();
        }
    };
    songButton.onClick = modeChanged;
    endlessButton.onClick = modeChanged;
    writeButton.setTooltip ("Write the song and play it on the parts in the Rack");
    writeButton.onClick = [this]
    {
        const auto st = settingsFromControls();
        const bool newTracks = ! engine.isSongMode() || engine.getSongSettings().genre != st.genre;
        owner.changeCatsLater ([this, st, newTracks]
        {
            engine.composeSong (st, newTracks);
            if (! engine.isPlaying()) engine.setPlaying (true);
        });
    };
    exportButton.setTooltip ("Save the song: as a WAV, as one WAV per part (stems), or as a MIDI file for Cubase");
    exportButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "The song as a WAV file");
        m.addItem (2, "Each part as its own WAV (stems)");
        m.addItem (3, "MIDI file (all parts, with the words) for Cubase");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (exportButton),
                         [sp = juce::Component::SafePointer<HubMain> (&owner)] (int r) { if (sp != nullptr && r > 0) sp->exportSong (r - 1); });
    };
    jamButton.setTooltip ("Stop the song and jam with Chord-Cat's pads again");
    jamButton.onClick = [this] { owner.changeCatsLater ([this] { engine.applyStyle (0); }); };

    const auto names = cat::muse::targetNames();
    static const char* tips[] = { "New chords (the melody, bass and counter-melody follow them)", "A new bass line", "New drum grooves and fills",
                                  "A new melody and hook (with new words)", "A new counter-melody", "New words for the same melody",
                                  "A new song form: other sections in another order" };
    for (int t = 0; t < names.size(); ++t)
    {
        auto* b = renewButtons.add (new juce::TextButton (names[t]));
        b->setTooltip (tips[t]);
        b->onClick = [this, t]
        {
            engine.regenerateSong (t, (juce::uint32) (1 + juce::Random().nextInt (0x7ffffff)));
            owner.songChanged();
        };
        addAndMakeVisible (b);
    }
    for (juce::Component* c : { (juce::Component*) &genreBox, (juce::Component*) &moodBox, (juce::Component*) &seedBox, (juce::Component*) &newSeedButton,
                                (juce::Component*) &songButton, (juce::Component*) &endlessButton, (juce::Component*) &writeButton,
                                (juce::Component*) &exportButton, (juce::Component*) &jamButton })
        addAndMakeVisible (c);
    refresh();
}

cat::muse::Settings ComposePage::settingsFromControls() const
{
    cat::muse::Settings st;
    st.genre = juce::jmax (0, genreBox.getSelectedId() - 1);
    st.mood = juce::jmax (0, moodBox.getSelectedId() - 1);
    st.seed = (juce::uint32) juce::jmax (1, seedBox.getText().getIntValue());
    st.endless = endlessButton.getToggleState();
    return st;
}

void ComposePage::showSettings (const cat::muse::Settings& st)
{
    genreBox.setSelectedId (st.genre + 1, juce::dontSendNotification);
    moodBox.setSelectedId (st.mood + 1, juce::dontSendNotification);
    seedBox.setText (juce::String ((juce::int64) st.seed), false);
    (st.endless ? endlessButton : songButton).setToggleState (true, juce::dontSendNotification);
}

void ComposePage::refresh()
{
    const bool song = engine.isSongMode();
    if (song) showSettings (engine.getSongSettings());
    for (auto* b : renewButtons) b->setEnabled (song);
    exportButton.setEnabled (song);
    jamButton.setEnabled (song);
    writeButton.setColour (juce::TextButton::buttonColourId, theme().accent.withAlpha (0.75f));
    shownSong = nullptr;   // redraw the map
    tick();
    repaint();
}

void ComposePage::tick()
{
    auto song = engine.getSong();
    if (song.get() != shownSong)
    {
        shownSong = song.get();
        rebuildMap();
        repaint();
        return;
    }
    if (song == nullptr) return;
    const double beat = engine.isPlaying() ? engine.getSongBeat() : -1.0;
    if (std::abs (beat - shownBeat) > 0.06)
    {
        shownBeat = beat;
        repaint (mapBounds.expanded (2));
    }
    const int line = beat >= 0.0 ? song->lyricLineAt (beat + 0.25) : -1;
    if (line != shownLine) { shownLine = line; repaint (lyricsArea); }
}

void ComposePage::rebuildMap()
{
    auto song = engine.getSong();
    const auto area = mapBounds;
    if (area.isEmpty()) { mapImage = {}; return; }
    mapImage = juce::Image (juce::Image::ARGB, area.getWidth(), area.getHeight(), true);
    juce::Graphics g (mapImage);
    const auto& th = theme();
    g.setColour (th.panel);
    g.fillRoundedRectangle (mapImage.getBounds().toFloat(), 8.0f);
    if (song == nullptr || song->lengthBeats <= 0.0) return;

    auto r = mapImage.getBounds().toFloat().reduced (10.0f, 8.0f);
    const float labelW = 92.0f;
    auto timeArea = r.withTrimmedLeft (labelW);
    const float x0 = timeArea.getX(), w = timeArea.getWidth();
    const double len = song->lengthBeats;
    auto xOf = [&] (double beat) { return x0 + (float) (beat / len) * w; };

    // sections
    auto secRow = timeArea.removeFromTop (34.0f);
    auto chordRow = timeArea.removeFromTop (18.0f);
    timeArea.removeFromTop (6.0f);
    g.setFont (hubFont (12.5f, true));
    for (const auto& sec : song->sections)
    {
        juce::Rectangle<float> b (xOf (sec.start), secRow.getY(), xOf (sec.end()) - xOf (sec.start), secRow.getHeight());
        g.setColour (sectionColour (sec.type));
        g.fillRoundedRectangle (b.reduced (1.0f, 0.0f), 5.0f);
        g.setColour (th.light ? th.text : juce::Colours::white.withAlpha (0.92f));
        const auto name = cat::muse::sectionName (sec.type);
        g.drawFittedText (b.getWidth() > 60.0f ? name : name.substring (0, 1), b.reduced (4.0f, 2.0f).toNearestInt(), juce::Justification::centred, 1);
    }
    // chords
    g.setFont (hubFont (11.0f));
    for (const auto& ch : song->chords)
    {
        const float cx = xOf (ch.start), cw = xOf (ch.start + ch.length) - cx;
        if (cw < 26.0f) continue;
        g.setColour (th.textDim);
        g.drawText (ch.name, juce::Rectangle<float> (cx + 2.0f, chordRow.getY(), cw - 2.0f, chordRow.getHeight()), juce::Justification::centredLeft, true);
    }
    // a lane per part: where it plays, and its notes
    const int lanes = cat::muse::NumParts;
    const float laneH = timeArea.getHeight() / (float) lanes;
    static const int order[] = { cat::muse::Melody, cat::muse::Counter, cat::muse::Chords, cat::muse::Bass, cat::muse::Drums };
    for (int li = 0; li < lanes; ++li)
    {
        const int p = order[li];
        juce::Rectangle<float> lane (timeArea.getX(), timeArea.getY() + li * laneH, w, laneH - 3.0f);
        const auto c = partColour (p);
        g.setColour (th.textDim);
        g.setFont (hubFont (12.0f, true));
        g.drawText (cat::muse::partNames()[p].toUpperCase(), juce::Rectangle<float> (r.getX(), lane.getY(), labelW - 8.0f, lane.getHeight()),
                    juce::Justification::centredLeft);
        g.setColour (th.raised.withAlpha (0.6f));
        g.fillRoundedRectangle (lane, 3.0f);
        for (const auto& sec : song->sections)
            if (sec.on[(size_t) p])
            {
                g.setColour (c.withAlpha (0.16f));
                g.fillRoundedRectangle (juce::Rectangle<float> (xOf (sec.start), lane.getY(), xOf (sec.end()) - xOf (sec.start), lane.getHeight()).reduced (1.0f, 0.0f), 3.0f);
            }
        const auto& notes = song->parts[(size_t) p];
        if (notes.empty()) continue;
        int lo = 127, hi = 0;
        for (const auto& n : notes) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
        const float span = (float) juce::jmax (6, hi - lo);
        g.setColour (c.withAlpha (0.9f));
        for (const auto& n : notes)
        {
            if (p == cat::muse::Drums && n.pitch != cat::muse::drum::kick && n.pitch != cat::muse::drum::snare && n.pitch != cat::muse::drum::clap) continue;
            const float nx = xOf (n.start), nw = juce::jmax (1.2f, xOf (n.start + n.length) - nx);
            const float ny = p == cat::muse::Drums ? lane.getY() + lane.getHeight() * (n.pitch == cat::muse::drum::kick ? 0.65f : 0.25f)
                                                   : lane.getBottom() - 3.0f - (float) (n.pitch - lo) / span * (lane.getHeight() - 6.0f);
            g.fillRect (nx, ny - 1.0f, nw, 2.2f);
        }
    }
}

void ComposePage::paint (juce::Graphics& g)
{
    const auto& th = theme();
    g.fillAll (th.bg);
    auto song = engine.getSong();

    // header: the song's name and what it is
    auto h = headerArea;
    g.setColour (th.text);
    g.setFont (hubFont (24.0f, true));
    const auto st = engine.getSongSettings();
    if (song != nullptr && engine.isSongMode())
    {
        g.drawText ("\"" + song->title + "\"", h.removeFromLeft (h.getWidth() / 2), juce::Justification::centredLeft, true);
        g.setColour (th.textDim);
        g.setFont (hubFont (14.0f));
        juce::String info = cat::muse::genreNames()[st.genre] + "  /  " + cat::muse::moodNames()[st.mood] + "     "
                            + cat::muse::keyName (song->key) + " " + cat::muse::scaleName (song->scale) + "     "
                            + juce::String (juce::roundToInt (engine.bpm.load())) + " BPM     song #" + juce::String ((juce::int64) st.seed);
        if (st.endless) info << "     endless, part " << (song->chunk + 1);
        else
        {
            const int secs = (int) std::round (song->lengthBeats * 60.0 / juce::jmax (30.0, engine.bpm.load()));
            info << "     " << (secs / 60) << ":" << juce::String (secs % 60).paddedLeft ('0', 2) << " min";
        }
        g.drawText (info, h, juce::Justification::centredRight, true);
    }
    else
    {
        g.drawText ("Write a song", h.removeFromLeft (h.getWidth() / 2), juce::Justification::centredLeft);
        g.setColour (th.textDim);
        g.setFont (hubFont (14.0f));
        g.drawText ("Pick a genre and a mood, then press  Write a song!  (or roll the Dice)", h, juce::Justification::centredRight, true);
    }
    g.setColour (th.textDim);
    g.setFont (hubFont (12.0f, true));
    g.drawText ("NEW:", renewLabelArea, juce::Justification::centredLeft);

    // the map + playhead
    if (mapImage.isValid())
    {
        g.drawImageAt (mapImage, mapBounds.getX(), mapBounds.getY());
        if (song != nullptr && engine.isSongMode() && ! song->settings.endless)
        {
            g.setColour (th.textDim);
            g.setFont (hubFont (11.0f));
            g.drawText ("Click a section to arrange the song", mapBounds.withTrimmedBottom (2).removeFromBottom (14).withTrimmedRight (12),
                        juce::Justification::centredRight);
        }
    }
    else
    {
        g.setColour (th.panel);
        g.fillRoundedRectangle (mapBounds.toFloat(), 8.0f);
    }
    if (song != nullptr && engine.isSongMode() && song->lengthBeats > 0.0)
    {
        const float x0 = (float) mapBounds.getX() + 10.0f + 92.0f, w = (float) mapBounds.getWidth() - 20.0f - 92.0f;
        const double beat = engine.isPlaying() ? engine.getSongBeat() : -1.0;
        if (beat >= 0.0)
        {
            const int si = song->sectionIndexAt (beat);
            if (si >= 0)
            {
                const auto& sec = song->sections[(size_t) si];
                const float sx = x0 + (float) (sec.start / song->lengthBeats) * w, sw = (float) (sec.bars * 4.0 / song->lengthBeats) * w;
                g.setColour (th.text.withAlpha (0.8f));
                g.drawRoundedRectangle (juce::Rectangle<float> (sx, (float) mapBounds.getY() + 8.0f, sw, 34.0f).reduced (1.0f, 0.0f), 5.0f, 2.0f);
            }
            const float px = x0 + (float) (beat / song->lengthBeats) * w;
            g.setColour (th.accent);
            g.fillRect (px - 1.0f, (float) mapBounds.getY() + 6.0f, 2.5f, (float) mapBounds.getHeight() - 12.0f);
        }
    }
    else
    {
        g.setColour (th.textDim);
        g.setFont (hubFont (16.0f));
        g.drawFittedText ("The song's map shows up here: its sections, chords and every part's notes.", mapBounds.reduced (30), juce::Justification::centred, 2);
    }

    // the words
    auto la = lyricsArea.toFloat();
    g.setColour (th.panel);
    g.fillRoundedRectangle (la, 8.0f);
    la = la.reduced (16.0f, 8.0f);
    if (song != nullptr && engine.isSongMode())
    {
        const int line = shownLine;
        const auto& lines = song->lyricLines;
        bool singer = false;
        for (int i = 0; i < engine.getNumTracks(); ++i)
        {
            auto& t = engine.track (i);
            if (t.source.load() == SrcLead && (t.sound.load() == SndSing || t.sound.load() == SndTalk) && ! t.mute.load()) singer = true;
        }
        if (lines.isEmpty())
        {
            g.setColour (th.textDim);
            g.setFont (hubFont (15.0f));
            g.drawText ("(this song has no words)", la, juce::Justification::centred);
        }
        else
        {
            auto top = la.removeFromTop (la.getHeight() * 0.3f);
            auto mid = la.removeFromTop (la.getHeight() * 0.55f);
            g.setColour (th.textDim);
            g.setFont (hubFont (14.0f));
            if (line >= 1) g.drawText (lines[line - 1], top, juce::Justification::centred, true);
            else g.drawText (singer ? "WORDS" : "WORDS  -  tip: give the Lead / melody part Sing-Cat or Talk-Cat as its sound and it sings them",
                             top, juce::Justification::centred, true);
            g.setColour (th.text);
            g.setFont (hubFont (24.0f, true));
            g.drawText (line >= 0 ? lines[line] : lines[0], mid, juce::Justification::centred, true);
            g.setColour (th.textDim);
            g.setFont (hubFont (14.0f));
            const int next = line >= 0 ? line + 1 : 1;
            if (next < lines.size()) g.drawText (lines[next], la, juce::Justification::centred, true);
        }
    }
}

//==============================================================================
// the arranger: click a section on the map
namespace arr
{
    constexpr int Renew = 1, Louder = 2, Softer = 3, Duplicate = 4, Earlier = 5, Later = 6, Delete = 7, Undo = 8,
                  ChangeTo = 100, AddAfter = 200, Length = 300, PartToggle = 400;
    const int lengths[] = { 4, 8, 12, 16 };
}

int ComposePage::sectionAt (juce::Point<int> p) const
{
    auto song = engine.getSong();
    if (song == nullptr || ! engine.isSongMode() || song->lengthBeats <= 0.0 || ! mapBounds.contains (p)) return -1;
    const float x0 = (float) mapBounds.getX() + 10.0f + 92.0f, w = (float) mapBounds.getWidth() - 20.0f - 92.0f;
    if (w <= 0.0f || (float) p.x < x0) return -1;
    const double beat = ((float) p.x - x0) / w * song->lengthBeats;
    for (int i = 0; i < (int) song->sections.size(); ++i)
        if (beat >= song->sections[(size_t) i].start && beat < song->sections[(size_t) i].end()) return i;
    return -1;
}

void ComposePage::mouseDown (const juce::MouseEvent& e)
{
    const int idx = sectionAt (e.getPosition());
    auto song = engine.getSong();
    if (idx < 0 || song == nullptr) return;
    const auto& sec = song->sections[(size_t) idx];
    juce::PopupMenu m;
    m.addSectionHeader (cat::muse::sectionName (sec.type) + " (" + juce::String (sec.bars) + " bars)");
    if (song->settings.endless)
        m.addItem (1, "Endless songs arrange themselves - turn Endless off to arrange", false);
    else
    {
        m.addItem (arr::Renew, "Other chords for this section");
        juce::PopupMenu change, add, length, parts;
        for (int t = 0; t < cat::muse::NumSectionTypes; ++t)
        {
            change.addItem (arr::ChangeTo + t, cat::muse::sectionName (t), true, t == sec.type);
            add.addItem (arr::AddAfter + t, cat::muse::sectionName (t));
        }
        for (int k = 0; k < 4; ++k) length.addItem (arr::Length + k, juce::String (arr::lengths[k]) + " bars", true, sec.bars == arr::lengths[k]);
        for (int p = 0; p < cat::muse::NumParts; ++p) parts.addItem (arr::PartToggle + p, cat::muse::partNames()[p], true, sec.on[(size_t) p]);
        m.addSubMenu ("Make it a...", change);
        m.addSubMenu ("Length", length);
        m.addSubMenu ("Parts that play", parts);
        m.addItem (arr::Louder, "More energy", sec.energy < 0.99f);
        m.addItem (arr::Softer, "Less energy", sec.energy > 0.11f);
        m.addSeparator();
        m.addSubMenu ("Add a section after it", add, song->sections.size() < 40);
        m.addItem (arr::Duplicate, "Play it twice", song->sections.size() < 40);
        m.addItem (arr::Earlier, "Move earlier", idx > 0);
        m.addItem (arr::Later, "Move later", idx < (int) song->sections.size() - 1);
        m.addItem (arr::Delete, "Remove it", song->sections.size() > 1);
        m.addSeparator();
        m.addItem (arr::Undo, "Undo the last change", ! undoForm.empty()
                   && undoKey == song->settings.seed * 131u + (juce::uint32) song->settings.genre * 7u + (juce::uint32) song->settings.mood);
    }
    juce::Component::SafePointer<ComposePage> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (
                         juce::Rectangle<int> (e.getScreenX(), e.getScreenY(), 1, 1)),
                     [sp, idx] (int result) { if (sp != nullptr && result > 0) sp->arrangeSection (idx, result); });
}

void ComposePage::arrangeSection (int idx, int result)
{
    auto song = engine.getSong();
    if (song == nullptr || song->settings.endless || ! juce::isPositiveAndBelow (idx, (int) song->sections.size())) return;
    auto form = song->sections;
    auto& sec = form[(size_t) idx];
    const auto seed = (juce::uint32) (1 + juce::Random().nextInt (0x7ffffff));
    const int numProgressions = juce::jmax (1, (int) song->progressions.size());
    if (result == arr::Undo)
    {
        if (undoForm.empty() || undoKey != song->settings.seed * 131u + (juce::uint32) song->settings.genre * 7u + (juce::uint32) song->settings.mood) return;
        form = undoForm;
        undoForm.clear();
    }
    else
    {
        if (result == arr::Renew) sec.progression = (sec.progression + 1 + juce::Random().nextInt (juce::jmax (1, numProgressions - 1))) % numProgressions;
        else if (result == arr::Louder) sec.energy = juce::jmin (1.0f, sec.energy + 0.15f);
        else if (result == arr::Softer) sec.energy = juce::jmax (0.1f, sec.energy - 0.15f);
        else if (result == arr::Duplicate) { if (form.size() >= 40) return; const auto copy = sec; form.insert (form.begin() + idx + 1, copy); }
        else if (result == arr::Earlier) { if (idx == 0) return; std::swap (form[(size_t) idx], form[(size_t) idx - 1]); }
        else if (result == arr::Later) { if (idx + 1 >= (int) form.size()) return; std::swap (form[(size_t) idx], form[(size_t) idx + 1]); }
        else if (result == arr::Delete) { if (form.size() <= 1) return; form.erase (form.begin() + idx); }
        else if (result >= arr::PartToggle && result < arr::PartToggle + cat::muse::NumParts) { auto& on = sec.on[(size_t) (result - arr::PartToggle)]; on = ! on; }
        else if (result >= arr::Length && result < arr::Length + 4) sec.bars = arr::lengths[result - arr::Length];
        else if (result >= arr::ChangeTo && result < arr::ChangeTo + cat::muse::NumSectionTypes)
            sec = cat::muse::makeSection (*song, result - arr::ChangeTo, sec.bars, seed);
        else if (result >= arr::AddAfter && result < arr::AddAfter + cat::muse::NumSectionTypes)
        {
            if (form.size() >= 40) return;
            const int type = result - arr::AddAfter;
            form.insert (form.begin() + idx + 1, cat::muse::makeSection (*song, type, type == cat::muse::PreChorus || type == cat::muse::Build ? 4 : 8, seed));
        }
        else return;
        undoForm = song->sections;
        undoKey = song->settings.seed * 131u + (juce::uint32) song->settings.genre * 7u + (juce::uint32) song->settings.mood;
    }
    if (engine.arrangeSong (form)) owner.songChanged();
}

void ComposePage::resized()
{
    auto r = getLocalBounds().reduced (14, 10);
    headerArea = r.removeFromTop (36);
    r.removeFromTop (6);
    auto row = r.removeFromTop (34);
    auto col = [&row] (int w) { auto c = row.removeFromLeft (w); row.removeFromLeft (8); return c; };
    genreBox.setBounds (col (130));
    moodBox.setBounds (col (110));
    seedBox.setBounds (col (84));
    newSeedButton.setBounds (col (92));
    row.removeFromLeft (6);
    songButton.setBounds (row.removeFromLeft (70));
    endlessButton.setBounds (col (84));
    row.removeFromLeft (6);
    writeButton.setBounds (col (150));
    jamButton.setBounds (row.removeFromRight (140));
    row.removeFromRight (8);
    exportButton.setBounds (row.removeFromRight (100));

    r.removeFromTop (8);
    auto renew = r.removeFromTop (30);
    renewLabelArea = renew.removeFromLeft (44);
    const int bw = juce::jmin (130, (renew.getWidth() - 6 * renewButtons.size()) / juce::jmax (1, renewButtons.size()));
    for (auto* b : renewButtons) { b->setBounds (renew.removeFromLeft (bw)); renew.removeFromLeft (6); }

    r.removeFromTop (10);
    lyricsArea = r.removeFromBottom (juce::jlimit (80, 120, r.getHeight() / 4));
    r.removeFromBottom (8);
    const auto oldMap = mapBounds;
    mapBounds = r;
    if (oldMap != mapBounds) rebuildMap();
}

//==============================================================================
SettingsPage::SettingsPage (HubMain& o, HubEngine& e, juce::AudioDeviceManager& dm)
    : owner (o), engine (e), audio (dm, 0, 0, 2, 2, true, false, true, false)
{
    keyboardBox.setTooltip ("Which part a MIDI keyboard plays (turn the keyboard on under Sound card - Active MIDI inputs)");
    keyboardBox.onChange = [this] { engine.keyboardTrack = keyboardBox.getSelectedId() - 2; };
    addAndMakeVisible (keyboardBox);
    for (int i = 0; i < (int) themes().size(); ++i) themeBox.addItem (themes()[(size_t) i].name, i + 1);
    themeBox.onChange = [this] { owner.applyTheme (themeBox.getSelectedId() - 1); };
    advancedToggle.onClick = [this] { owner.setAdvanced (advancedToggle.getToggleState()); };
    tutorialButton.onClick = [this] { owner.startTutorial(); };
    findButton.onClick = [this] { owner.findCats(); };
    resetButton.setTooltip ("Back to the default jam (the Cat Jam style)");
    resetButton.onClick = [this] { owner.changeCatsLater ([&eng = engine] { eng.applyStyle (0); }); };
    for (juce::Component* c : { (juce::Component*) &themeBox, (juce::Component*) &advancedToggle, (juce::Component*) &tutorialButton,
                                (juce::Component*) &findButton, (juce::Component*) &resetButton, (juce::Component*) &audio })
        addAndMakeVisible (c);
    refresh();
}

void SettingsPage::refresh()
{
    themeBox.setSelectedId (themeIndex() + 1, juce::dontSendNotification);
    keyboardBox.clear (juce::dontSendNotification);
    keyboardBox.addItem ("Automatic (the Lead)", 1);
    for (int i = 0; i < engine.getNumTracks(); ++i)
        keyboardBox.addItem (juce::String (i + 1) + ": " + sourceName (engine.track (i).source.load()) + " - " + soundName (engine.track (i).sound.load()), i + 2);
    keyboardBox.setSelectedId (juce::jlimit (-1, engine.getNumTracks() - 1, engine.keyboardTrack.load()) + 2, juce::dontSendNotification);
    advancedToggle.setToggleState (owner.isAdvanced(), juce::dontSendNotification);
    repaint();
}

void SettingsPage::paint (juce::Graphics& g)
{
    const auto& th = theme();
    g.fillAll (th.bg);
    auto r = getLocalBounds().reduced (24, 16).withTrimmedBottom (20);
    auto left = r.removeFromLeft (r.getWidth() / 2).reduced (8, 0);
    auto right = r.reduced (8, 0);
    for (auto box : { left, right })
    {
        g.setColour (th.panel);
        g.fillRoundedRectangle (box.toFloat(), 10.0f);
        g.setColour (th.outline);
        g.drawRoundedRectangle (box.toFloat(), 10.0f, 1.0f);
    }
    auto head = [&g, &th] (juce::Rectangle<int> a, const juce::String& t)
    {
        g.setColour (th.text);
        g.setFont (hubFont (15.0f, true));
        g.drawText (t, a, juce::Justification::centredLeft);
    };
    head (themeBox.getBounds().withY (themeBox.getY() - 26).withHeight (22), "Theme");
    head (advancedToggle.getBounds().withY (advancedToggle.getY() - 26).withHeight (22), "Mode");
    head (tutorialButton.getBounds().withY (tutorialButton.getY() - 26).withHeight (22), "Help");
    head (keyboardBox.getBounds().withY (keyboardBox.getY() - 26).withHeight (22), "MIDI keyboard plays");
    head (catsArea.withHeight (22), "The Cats");
    head (audio.getBounds().withY (audio.getY() - 26).withHeight (22), "Sound card");
    if (audio.getNumChildComponents() == 0 || audio.getHeight() < 40)
    {
        g.setColour (th.textDim);
        g.setFont (hubFont (13.0f));
        g.drawText ("No sound card found", audio.getBounds(), juce::Justification::centred);
    }

    // which Cats were found
    auto ca = catsArea.withTrimmedTop (26);
    const int colW = ca.getWidth() / 2;
    for (int id = 0; id < NumCats; ++id)
    {
        auto cell = juce::Rectangle<int> (ca.getX() + (id % 2) * colW, ca.getY() + (id / 2) * 22, colW, 22);
        const bool ok = engine.catAvailable (id);
        g.setColour (ok ? juce::Colour (0xff7fb393) : juce::Colour (0xffd0605a));
        g.fillEllipse (cell.getX() + 2.0f, cell.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setColour (ok ? th.text : th.textDim);
        g.setFont (hubFont (13.0f));
        g.drawText (juce::String (catName (id)) + (ok ? "" : "  (missing)"), cell.withTrimmedLeft (16), juce::Justification::centredLeft);
    }
    g.setColour (th.textDim);
    g.setFont (hubFont (12.0f));
    g.drawFittedText ("Cat Hub " + juce::JUCEApplicationBase::getInstance()->getApplicationVersion()
                      + "  -  Space: play / stop   1-8: hold a pad   Every Cat's own window is a tab at the top.",
                      getLocalBounds().reduced (32, 4).removeFromBottom (20), juce::Justification::centredLeft, 1);
}

void SettingsPage::resized()
{
    auto r = getLocalBounds().reduced (24, 16).withTrimmedBottom (20);
    auto left = r.removeFromLeft (r.getWidth() / 2).reduced (28, 20);
    auto right = r.reduced (28, 20);
    left.removeFromTop (28);
    auto first = left.removeFromTop (30);
    themeBox.setBounds (first.removeFromLeft (juce::jmin (240, first.getWidth() / 2 - 8)));
    first.removeFromLeft (16);
    keyboardBox.setBounds (first.withWidth (juce::jmin (300, first.getWidth())));
    left.removeFromTop (40);
    advancedToggle.setBounds (left.removeFromTop (28));
    left.removeFromTop (40);
    auto row = left.removeFromTop (32);
    tutorialButton.setBounds (row.removeFromLeft (220));
    row.removeFromLeft (10);
    resetButton.setBounds (row.removeFromLeft (180));
    left.removeFromTop (22);
    catsArea = left.removeFromTop (26 + 5 * 22);
    left.removeFromTop (8);
    findButton.setBounds (left.removeFromTop (30).withWidth (200));
    right.removeFromTop (28);
    audio.setBounds (right);
}

//==============================================================================
Tutorial::Tutorial (std::vector<Step> s, std::function<void()> f) : steps (std::move (s)), onFinish (std::move (f))
{
    nextButton.onClick = [this] { next(); };
    skipButton.onClick = [this] { step = (int) steps.size(); next(); };
    nextButton.setColour (juce::TextButton::buttonColourId, theme().accent);
    nextButton.setColour (juce::TextButton::textColourOffId, theme().light ? juce::Colours::white : theme().bg);
    addAndMakeVisible (nextButton);
    addAndMakeVisible (skipButton);
    setWantsKeyboardFocus (false);
}

void Tutorial::next()
{
    if (++step >= (int) steps.size())
    {
        auto fn = onFinish;   // this may be deleted by onFinish
        if (fn) fn();
        return;
    }
    resized();
    repaint();
}

juce::Rectangle<int> Tutorial::bubbleBounds() const
{
    const auto target = steps[(size_t) step].target ? steps[(size_t) step].target() : juce::Rectangle<int>();
    const int w = 380, h = 170;
    if (target.isEmpty()) return getLocalBounds().withSizeKeepingCentre (w + 60, h + 30);
    juce::Rectangle<int> b (w, h);
    // below the target if there is room, else above; keep it on screen
    if (target.getBottom() + 14 + h < getHeight()) b.setPosition (target.getCentreX() - w / 2, target.getBottom() + 14);
    else b.setPosition (target.getCentreX() - w / 2, target.getY() - 14 - h);
    return b.constrainedWithin (getLocalBounds().reduced (10));
}

void Tutorial::resized()
{
    auto b = bubbleBounds().reduced (16, 12);
    auto row = b.removeFromBottom (32);
    nextButton.setBounds (row.removeFromRight (120));
    nextButton.setButtonText (step >= (int) steps.size() - 1 ? "Let's jam!" : "Next");
    skipButton.setBounds (row.removeFromLeft (140));
    skipButton.setVisible (step < (int) steps.size() - 1);
}

void Tutorial::paint (juce::Graphics& g)
{
    if (step >= (int) steps.size()) return;
    const auto& th = theme();
    const auto& s = steps[(size_t) step];
    const auto target = s.target ? s.target() : juce::Rectangle<int>();

    // dim everything except the thing we talk about
    juce::Path dim;
    dim.addRectangle (getLocalBounds().toFloat());
    if (! target.isEmpty())
    {
        dim.addRoundedRectangle (target.toFloat().expanded (6.0f), 10.0f);
        dim.setUsingNonZeroWinding (false);
    }
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillPath (dim);
    if (! target.isEmpty())
    {
        g.setColour (th.accent);
        g.drawRoundedRectangle (target.toFloat().expanded (6.0f), 10.0f, 2.5f);
    }

    auto b = bubbleBounds().toFloat();
    g.setColour (th.panelHi);
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (th.accent);
    g.drawRoundedRectangle (b, 12.0f, 2.0f);
    auto tr = b.reduced (18.0f, 14.0f);
    g.setColour (th.textDim);
    g.setFont (hubFont (11.0f, true));
    g.drawText (juce::String (step + 1) + " / " + juce::String ((int) steps.size()), tr.removeFromTop (14.0f), juce::Justification::centredRight);
    g.setColour (th.text);
    g.setFont (hubFont (18.0f, true));
    g.drawText (s.title, tr.removeFromTop (24.0f).translated (0.0f, -12.0f), juce::Justification::centredLeft);
    g.setColour (th.text.withAlpha (0.85f));
    g.setFont (hubFont (13.5f));
    g.drawFittedText (s.text, tr.withTrimmedBottom (40.0f).toNearestInt().translated (0, -6), juce::Justification::topLeft, 5);
}

//==============================================================================
HubMain::HubMain (HubEngine& e, juce::AudioDeviceManager& dm, juce::PropertiesFile& s)
    : engine (e), deviceManager (dm), settings (s)
{
    setTheme (settings.getIntValue ("theme", 0));
    lnf.applyTheme();
    advanced = settings.getBoolValue ("advanced", false);
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    playButton.setTooltip ("Start / stop every Cat (Space)");
    playButton.onClick = [this] { togglePlay(); };
    recordButton.setTooltip ("Record what you hear to a WAV file (Music \\ Cat Hub)");
    recordButton.onClick = [this] { toggleRecord(); };
    diceButton.setTooltip ("Roll the dice: a brand new song (genre, mood, chords, melody, words, sounds)");
    diceButton.onClick = [this]
    {
        changeCatsLater ([this]
        {
            juce::Random r;
            engine.roll (r);
            if (! engine.isPlaying()) engine.setPlaying (true);
        });
    };
    helpButton.setTooltip ("Show the tutorial again");
    helpButton.onClick = [this] { startTutorial(); };
    jamMenuButton.setTooltip ("Save or open a jam (everything: Cats, parts, sounds, effects, the song), full screen");
    jamMenuButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "Save jam...  (Ctrl+S)");
        m.addItem (2, "Open jam...  (Ctrl+O)");
        m.addSeparator();
        m.addItem (3, "Start a fresh jam");
        m.addSeparator();
        auto* w = findParentComponentOfClass<juce::ResizableWindow>();
        m.addItem (4, "Full screen  (F11)", w != nullptr, w != nullptr && w->isFullScreen());
        juce::Component::SafePointer<HubMain> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&jamMenuButton), [sp] (int r)
        {
            if (sp == nullptr) return;
            if (r == 1) sp->saveJam();
            else if (r == 2) sp->openJam();
            else if (r == 3) sp->changeCatsLater ([e = &sp->engine] { e->applyStyle (0); });
            else if (r == 4) sp->toggleFullScreen();
        });
    };
    fxButton.setTooltip ("Master effects: room, tone, glue, warmth, width");
    fxButton.onClick = [this] { showMasterFx(); };
    for (auto* b : { &simpleButton, &advancedButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (101);
    }
    simpleButton.setTooltip ("Simple: just the sounds and volumes");
    advancedButton.setTooltip ("Advanced: effects, Cats as MIDI effects, extra parts");
    simpleButton.onClick = [this] { if (simpleButton.getToggleState()) setAdvanced (false); };
    advancedButton.onClick = [this] { if (advancedButton.getToggleState()) setAdvanced (true); };

    bpm.setSliderStyle (juce::Slider::LinearBar);
    bpm.setRange (50.0, 200.0, 1.0);
    bpm.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 20);
    bpm.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " BPM"; };
    bpm.setValue (engine.bpm.load(), juce::dontSendNotification);
    bpm.onValueChange = [this] { engine.bpm = bpm.getValue(); };
    bpm.setTooltip ("Tempo for every Cat (drag, or double-click to type)");
    master.setSliderStyle (juce::Slider::LinearBar);
    master.setRange (0.0, 1.0, 0.01);
    master.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 20);
    master.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    master.setValue (engine.masterVolume.load(), juce::dontSendNotification);
    master.onValueChange = [this] { engine.masterVolume = (float) master.getValue(); };
    master.setTooltip ("Master volume");

    styleBox.addItemList (HubEngine::styleNames(), 1);
    styleBox.setTextWhenNothingSelected ("Pick a style...");
    styleBox.setTooltip ("Song styles: sets every Cat, sound, effect and the tempo in one click");
    styleBox.onChange = [this]
    {
        const int i = styleBox.getSelectedId() - 1;
        if (i >= 0) changeCatsLater ([this, i] { engine.applyStyle (i); });
    };
    static const char* keys[] = { "C", "C# / Db", "D", "Eb", "E", "F", "F# / Gb", "G", "Ab", "A", "Bb", "B" };
    for (int k = 0; k < 12; ++k) keyBox.addItem (keys[k], k + 1);
    keyBox.setTooltip ("The key everything plays in");
    keyBox.onChange = [this] { if (keyBox.getSelectedId() > 0) engine.setKey (keyBox.getSelectedId() - 1); };
    progressionBox.setTooltip ("Chord-Cat's chord progression");
    progressionBox.onChange = [this]
    {
        if (progressionBox.getSelectedId() > 0) { engine.setProgression (progressionBox.getSelectedId() - 1); refreshSongBar(); }
    };
    lengthBox.addItemList ({ "1 bar", "2 bars", "4 bars" }, 1);
    lengthBox.setTooltip ("How long each chord lasts while the song plays");
    lengthBox.onChange = [this] { engine.barsPerChord = lengthBox.getSelectedId() == 3 ? 4 : lengthBox.getSelectedId(); };
    autoChords.setTooltip ("On: the Hub walks through Chord-Cat's pads by itself. Off: you play the pads (keys 1-8)");
    autoChords.onClick = [this] { engine.autoChords = autoChords.getToggleState(); };

    for (juce::Component* c : { (juce::Component*) &playButton, (juce::Component*) &recordButton, (juce::Component*) &diceButton,
                                (juce::Component*) &helpButton, (juce::Component*) &simpleButton, (juce::Component*) &advancedButton,
                                (juce::Component*) &jamMenuButton, (juce::Component*) &fxButton,
                                (juce::Component*) &bpm, (juce::Component*) &master, (juce::Component*) &styleBox, (juce::Component*) &keyBox,
                                (juce::Component*) &progressionBox, (juce::Component*) &lengthBox, (juce::Component*) &autoChords,
                                (juce::Component*) &tabs, (juce::Component*) &pads })
        addAndMakeVisible (c);

    tabs.setTabBarDepth (36);
    tabs.setOutline (0);
    tabs.setIndent (8);
    rack = new RackView (*this, engine);
    composePage = new ComposePage (*this, engine);
    settingsPage = new SettingsPage (*this, engine, deviceManager);
    rebuildTabs();

    setSize (1280, 820);
    refreshAll();
    startTimerHz (30);

    if (! settings.getBoolValue ("tutorialDone", false))
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<HubMain> (this)] { if (sp != nullptr) sp->startTutorial(); });
}

HubMain::~HubMain()
{
    stopTimer();
    tutorial.reset();
    chooser.reset();
    exportJob.reset();
    tabs.clearTabs();      // deletes the Cat windows (and the rack) before anything else goes
    setLookAndFeel (nullptr);
}

juce::Rectangle<int> HubMain::boundsOf (juce::Component& c) const
{
    return getLocalArea (&c, c.getLocalBounds());
}

//==============================================================================
void HubMain::closeCatTabs()
{
    while (tabs.getNumTabs() > 3) tabs.removeTab (tabs.getNumTabs() - 1);
}

void HubMain::rebuildTabs()
{
    const int current = tabs.getCurrentTabIndex();
    const auto currentName = current >= 0 ? tabs.getTabNames()[current] : juce::String();
    if (tabs.getNumTabs() == 0)
    {
        tabs.addTab ("Rack", theme().bg, rack, true);
        tabs.addTab ("Compose", theme().bg, composePage, true);
        tabs.addTab ("Settings", theme().bg, settingsPage, true);
    }
    closeCatTabs();
    for (int id = 0; id < 4; ++id)
        if (auto* c = engine.getCoreCat (id)) tabs.addTab (catName (id), theme().bg, new CatTab (*c), true);
    for (int i = 0; i < engine.getNumTracks(); ++i)
    {
        auto& t = engine.track (i);
        if (t.fxCat != nullptr) tabs.addTab (juce::String (i + 1) + " " + catName (t.fxCatId), theme().bg, new CatTab (*t.fxCat), true);
        if (t.soundCat != nullptr) tabs.addTab (juce::String (i + 1) + " " + catName (t.soundCatId), theme().bg, new CatTab (*t.soundCat), true);
    }
    const int again = tabs.getTabNames().indexOf (currentName);
    tabs.setCurrentTabIndex (again >= 0 ? again : 0, false);
}

void HubMain::showCatWindow (juce::AudioPluginInstance* p)
{
    if (p == nullptr) return;
    juce::String name;
    for (int id = 0; id < 4; ++id) if (engine.getCoreCat (id) == p) name = catName (id);
    for (int i = 0; i < engine.getNumTracks() && name.isEmpty(); ++i)
    {
        auto& t = engine.track (i);
        if (t.fxCat.get() == p) name = juce::String (i + 1) + " " + catName (t.fxCatId);
        if (t.soundCat.get() == p) name = juce::String (i + 1) + " " + catName (t.soundCatId);
    }
    const int idx = tabs.getTabNames().indexOf (name);
    if (idx >= 0) tabs.setCurrentTabIndex (idx);
}

void HubMain::changeCats (std::function<void()> fn)
{
    closeCatTabs();
    if (fn) fn();
    rack->rebuild();
    rebuildTabs();
    refreshAll();
}

void HubMain::changeCatsLater (std::function<void()> fn)
{
    // Called from inside a row / combo callback: the change rebuilds (deletes) that row, so run it after.
    juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<HubMain> (this), fn]
    {
        if (sp != nullptr) sp->changeCats (fn);
    });
}

void HubMain::refreshAll()
{
    simpleButton.setToggleState (! advanced, juce::dontSendNotification);
    advancedButton.setToggleState (advanced, juce::dontSendNotification);
    bpm.setValue (engine.bpm.load(), juce::dontSendNotification);
    master.setValue (engine.masterVolume.load(), juce::dontSendNotification);
    refreshSongBar();
    rack->refresh();
    composePage->refresh();
    settingsPage->refresh();
    status = [this]
    {
        juce::StringArray missing;
        for (int id = 0; id < 4; ++id) if (! engine.hasCat (id)) missing.add (catName (id));
        if (engine.lastLoadError.isNotEmpty()) { auto e = engine.lastLoadError; engine.lastLoadError = {}; return e; }
        return missing.isEmpty() ? juce::String() : "Not found: " + missing.joinIntoString (", ") + "  -  Settings > Find Cats folder";
    }();
    resized();
    repaint();
}

void HubMain::songChanged()
{
    bpm.setValue (engine.bpm.load(), juce::dontSendNotification);
    refreshSongBar();
    rack->refresh();
    composePage->refresh();
    repaint();
}

void HubMain::refreshSongBar()
{
    const bool song = engine.isSongMode();
    for (juce::Component* c : { (juce::Component*) &keyBox, (juce::Component*) &progressionBox, (juce::Component*) &lengthBox, (juce::Component*) &autoChords })
        c->setEnabled (! song);
    if (song)
    {
        styleBox.setSelectedId (0, juce::dontSendNotification);
        if (auto s = engine.getSong()) styleBox.setTextWhenNothingSelected ("Song: " + s->title);
        if (auto s = engine.getSong()) keyBox.setSelectedId (s->key + 1, juce::dontSendNotification);
        return;
    }
    const int styleIdx = HubEngine::styleNames().indexOf (engine.currentStyle);
    styleBox.setTextWhenNothingSelected ("Pick a style...");
    if (styleIdx >= 0) styleBox.setSelectedId (styleIdx + 1, juce::dontSendNotification);
    else { styleBox.setSelectedId (0, juce::dontSendNotification); styleBox.setTextWhenNothingSelected (engine.currentStyle.isNotEmpty() ? engine.currentStyle : "Pick a style..."); }
    const int key = engine.getKey();
    keyBox.setSelectedId (key >= 0 ? key + 1 : 0, juce::dontSendNotification);
    keyBox.setEnabled (key >= 0);
    const auto names = engine.getProgressionNames();
    if (progressionBox.getNumItems() != names.size())
    {
        progressionBox.clear (juce::dontSendNotification);
        for (int i = 0; i < names.size(); ++i) progressionBox.addItem (names[i], i + 1);
    }
    progressionBox.setSelectedId (engine.getProgression() + 1, juce::dontSendNotification);
    progressionBox.setEnabled (! names.isEmpty());
    const int bars = engine.barsPerChord.load();
    lengthBox.setSelectedId (bars >= 4 ? 3 : (bars >= 2 ? 2 : 1), juce::dontSendNotification);
    autoChords.setToggleState (engine.autoChords.load(), juce::dontSendNotification);
}

void HubMain::setAdvanced (bool a)
{
    advanced = a;
    settings.setValue ("advanced", a);
    refreshAll();
}

void HubMain::applyTheme (int index)
{
    setTheme (index);
    settings.setValue ("theme", index);
    lnf.applyTheme();
    for (int i = 0; i < tabs.getNumTabs(); ++i) tabs.setTabBackgroundColour (i, theme().bg);
    sendLookAndFeelChange();
    refreshAll();
    if (auto* top = getTopLevelComponent())
        if (auto* w = dynamic_cast<juce::DocumentWindow*> (top)) w->setBackgroundColour (theme().bg);
}

void HubMain::findCats()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose the folder with Chord-Cat.vst3, Bass-Cat.vst3, Drum-Cat.vst3 ...",
                                                   juce::File (settings.getValue ("catsFolder")));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
    {
        const auto dir = fc.getResult();
        if (! dir.isDirectory()) return;
        settings.setValue ("catsFolder", dir.getFullPathName());
        settings.saveIfNeeded();
        changeCats ([this, dir]
        {
            deviceManager.removeAudioCallback (&engine);          // never swap Cats while they play
            engine.loadCats (HubEngine::defaultSearchFolders (dir));
            deviceManager.addAudioCallback (&engine);
        });
    });
}

//==============================================================================
static juce::File jamFolder (juce::PropertiesFile& settings)
{
    juce::File dir (settings.getValue ("jamFolder"));
    if (! dir.isDirectory())
    {
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Cat Hub jams");
        dir.createDirectory();
    }
    return dir;
}

bool HubMain::saveJamTo (const juce::File& file)
{
    auto xml = engine.saveState().createXml();
    if (xml == nullptr) return false;
    xml->setAttribute ("theme", themeIndex());
    xml->setAttribute ("advanced", advanced);
    const bool ok = xml->writeTo (file);
    status = ok ? "Saved the jam: " + file.getFullPathName() : "Could not save " + file.getFullPathName();
    resized();
    repaint();
    return ok;
}

bool HubMain::openJamFrom (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    const auto tree = xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
    if (! tree.hasType ("CATHUB"))
    {
        status = "That is not a Cat Hub jam: " + file.getFileName();
        resized();
        repaint();
        return false;
    }
    changeCats ([this, tree] { engine.restoreState (tree); });
    if (xml->hasAttribute ("advanced")) setAdvanced (xml->getBoolAttribute ("advanced"));
    status = "Opened the jam: " + file.getFileNameWithoutExtension();
    resized();
    repaint();
    return true;
}

void HubMain::saveJam()
{
    const auto dir = jamFolder (settings);
    auto name = juce::File::createLegalFileName (engine.currentStyle.isNotEmpty() ? engine.currentStyle : juce::String ("My jam"));
    chooser = std::make_unique<juce::FileChooser> ("Save the jam", dir.getNonexistentChildFile (name, ".catjam", false), "*.catjam");
    juce::Component::SafePointer<HubMain> sp (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [sp] (const juce::FileChooser& fc)
    {
        if (sp == nullptr) return;
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (! f.hasFileExtension ("catjam")) f = f.withFileExtension ("catjam");
        sp->settings.setValue ("jamFolder", f.getParentDirectory().getFullPathName());
        sp->saveJamTo (f);
    });
}

void HubMain::openJam()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a jam", jamFolder (settings), "*.catjam");
    juce::Component::SafePointer<HubMain> sp (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [sp] (const juce::FileChooser& fc)
    {
        if (sp == nullptr) return;
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        sp->settings.setValue ("jamFolder", f.getParentDirectory().getFullPathName());
        sp->openJamFrom (f);
    });
}

void HubMain::toggleFullScreen()
{
    if (auto* w = findParentComponentOfClass<juce::ResizableWindow>())
    {
        w->setFullScreen (! w->isFullScreen());
        grabKeyboardFocus();
    }
}

void HubMain::showMasterFx()
{
    auto panel = std::make_unique<MasterFxPanel> (engine);
    panel->setSize (400, 130);
    juce::CallOutBox::launchAsynchronously (std::move (panel), fxButton.getScreenBounds(), nullptr);
}

//==============================================================================
MasterFxPanel::MasterFxPanel (HubEngine& e) : engine (e)
{
    for (auto* k : { &room, &tone, &glue, &warmth, &width }) addAndMakeVisible (k);
    load();
    room.knob.setDoubleClickReturnValue (true, 0.5);
    room.knob.onValueChange = [this] { engine.masterRoom = (float) room.knob.getValue(); };
    tone.knob.onValueChange = [this] { engine.masterTone = (float) (tone.knob.getValue() * 2.0 - 1.0); };
    glue.knob.onValueChange = [this] { engine.masterGlue = (float) glue.knob.getValue(); };
    warmth.knob.onValueChange = [this] { engine.masterWarmth = (float) warmth.knob.getValue(); };
    width.knob.onValueChange = [this] { engine.masterWidth = (float) (width.knob.getValue() * 2.0); };
    resetButton.setTooltip ("Back to a clean master");
    resetButton.onClick = [this]
    {
        engine.masterRoom = 0.5f; engine.masterTone = 0.0f; engine.masterGlue = 0.0f; engine.masterWarmth = 0.0f; engine.masterWidth = 1.0f;
        load();
    };
    addAndMakeVisible (resetButton);
    startTimerHz (20);
}

void MasterFxPanel::load()
{
    room.knob.setValue (engine.masterRoom.load(), juce::dontSendNotification);
    tone.knob.setValue ((engine.masterTone.load() + 1.0) * 0.5, juce::dontSendNotification);
    glue.knob.setValue (engine.masterGlue.load(), juce::dontSendNotification);
    warmth.knob.setValue (engine.masterWarmth.load(), juce::dontSendNotification);
    width.knob.setValue (engine.masterWidth.load() * 0.5, juce::dontSendNotification);
}

void MasterFxPanel::timerCallback()
{
    const float red = engine.getMasterReduction();
    if (std::abs (red - shownReduction) > 0.1f) { shownReduction = red; repaint(); }
}

void MasterFxPanel::paint (juce::Graphics& g)
{
    const auto& th = theme();
    g.setColour (th.text);
    g.setFont (hubFont (14.0f, true));
    auto r = getLocalBounds().reduced (8, 4);
    auto head = r.removeFromTop (22);
    g.drawText ("Master effects", head, juce::Justification::centredLeft);
    // how hard the glue squeezes
    auto meter = head.removeFromRight (140).withTrimmedRight (70).reduced (0, 7).toFloat();
    g.setColour (th.raised);
    g.fillRoundedRectangle (meter, 3.0f);
    g.setColour (th.accent);
    g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * juce::jlimit (0.0f, 1.0f, shownReduction / 12.0f)), 3.0f);
    g.setColour (th.textDim);
    g.setFont (hubFont (10.0f));
    g.drawText ("glue", meter.toNearestInt().translated (0, 10).withHeight (12), juce::Justification::centredLeft);
}

void MasterFxPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    auto head = r.removeFromTop (22);
    resetButton.setBounds (head.removeFromRight (60).reduced (0, 1));
    r.removeFromTop (4);
    const int w = r.getWidth() / 5;
    for (auto* k : { &room, &tone, &glue, &warmth, &width }) k->setBounds (r.removeFromLeft (w).reduced (4, 0));
}

//==============================================================================
namespace
{
    /** Renders the song (or its stems) with a progress window; the sound card is paused meanwhile. */
    class ExportJob : public juce::ThreadWithProgressWindow
    {
    public:
        ExportJob (HubEngine& e, std::vector<std::pair<int, juce::File>> j, std::function<void (bool, juce::String)> done)
            : juce::ThreadWithProgressWindow ("Saving the song...", true, true), engine (e), jobs (std::move (j)), onDone (std::move (done)) {}

        void run() override
        {
            for (size_t i = 0; i < jobs.size() && ! threadShouldExit(); ++i)
            {
                setStatusMessage ("Saving " + jobs[i].second.getFileName());
                ok = engine.exportSong (jobs[i].second, jobs[i].first, [this, i] (float p)
                {
                    setProgress (((double) i + p) / (double) jobs.size());
                    return ! threadShouldExit();
                }) && ok;
            }
        }
        void threadComplete (bool cancelled) override { if (onDone) onDone (ok && ! cancelled, jobs.empty() ? juce::String() : jobs.front().second.getFullPathName()); }

    private:
        HubEngine& engine;
        std::vector<std::pair<int, juce::File>> jobs;
        std::function<void (bool, juce::String)> onDone;
        bool ok = true;
    };
}

void HubMain::exportSong (int what)
{
    auto song = engine.getSong();
    if (song == nullptr || exportJob != nullptr) return;
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Cat Hub");
    dir.createDirectory();
    const auto name = juce::File::createLegalFileName (song->title.isNotEmpty() ? song->title : juce::String ("Cat Hub song"));
    if (what == 2)
    {
        const auto f = dir.getNonexistentChildFile (name, ".mid", false);
        status = engine.exportSongMidi (f) ? "Saved: " + f.getFullPathName() : "Could not save " + f.getFullPathName();
        if (f.existsAsFile()) f.revealToUser();
        resized();
        repaint();
        return;
    }
    std::vector<std::pair<int, juce::File>> jobs;
    if (what == 0) jobs.push_back ({ -1, dir.getNonexistentChildFile (name, ".wav", false) });
    else
    {
        auto stems = dir.getNonexistentChildFile (name + " stems", {}, false);
        stems.createDirectory();
        for (int i = 0; i < engine.getNumTracks(); ++i)
            jobs.push_back ({ i, stems.getChildFile (juce::String (i + 1) + " " + juce::File::createLegalFileName (sourceName (engine.track (i).source.load())) + ".wav") });
    }
    exporting = true;
    deviceManager.removeAudioCallback (&engine);          // the song is rendered here, faster than real time
    exportJob = std::make_unique<ExportJob> (engine, jobs, [sp = juce::Component::SafePointer<HubMain> (this), what] (bool ok, juce::String path)
    {
        if (sp == nullptr) return;
        sp->deviceManager.addAudioCallback (&sp->engine);
        sp->exporting = false;
        sp->status = ok ? "Saved: " + path + (what == 1 ? "  (and the other parts next to it)" : "") : juce::String ("Saving stopped");
        if (ok) juce::File (path).revealToUser();
        sp->resized();
        sp->repaint();
        juce::MessageManager::callAsync ([sp] { if (sp != nullptr) sp->exportJob.reset(); });
    });
    exportJob->launchThread();
}

std::vector<Tutorial::Step> HubMain::tutorialSteps()
{
    auto at = [this] (juce::Component& c) { return [this, &c] { return c.isShowing() ? boundsOf (c) : juce::Rectangle<int>(); }; };
    std::vector<Tutorial::Step> s;
    s.push_back ({ "Welcome to the Cat Hub!", "Chord-Cat, Bass-Cat, Drum-Cat and Life-Cat play together here - no Cubase needed. "
                   "This short tour shows you around. You can watch it again any time with the ? button.", {} });
    s.push_back ({ "Press Play", "Start and stop every Cat at once. The Space bar does the same.", at (playButton) });
    s.push_back ({ "Pick a style", "Lo-fi, Trap, House, Neo Soul, Ambient... One click sets every Cat, sound, effect and the tempo.", at (styleBox) });
    s.push_back ({ "Key and chords", "Change the key everything plays in, and which chord progression Chord-Cat plays.",
                   [this] { return boundsOf (keyBox).getUnion (boundsOf (progressionBox)); } });
    s.push_back ({ "Feeling lucky?", "The dice writes a brand new song: genre, mood, chords, melody, words and sounds. Roll it as often as you like!", at (diceButton) });
    s.push_back ({ "Write your own song", "The Compose tab writes songs that sound good: pick a genre and a mood, then press Write a song! "
                   "Don't like the drums or the melody? Press New on just that part. Export saves it as a WAV or a MIDI file.",
                   [this] { return boundsOf (tabs.getTabbedButtonBar()); } });
    s.push_back ({ "The rack", "Every row is a part. Choose its sound and preset, set the volume, mute (M) or solo (S) it. "
                   "The cat bounces when it plays and NOW PLAYING shows the chord or notes.",
                   [this] { if (auto* r = rack->getRow (0)) return r->isShowing() ? boundsOf (*r) : juce::Rectangle<int>(); return juce::Rectangle<int>(); } });
    s.push_back ({ "Play the pads", "These are Chord-Cat's chords. The song steps through them; hold one with the mouse or the keys 1-8 to play it yourself.", at (pads) });
    s.push_back ({ "Advanced mode", "Shows effects (filter, drive, chorus, echo, crush), lets you put Patt-, Bend- or Orbit-Cat on a part as an effect, "
                   "use Glide-, Sing- or Talk-Cat as the sound, and add more parts.", at (advancedButton) });
    s.push_back ({ "Every Cat's window", "Each Cat has its own tab up here with all of its settings. Settings has themes, your sound card and this tour.",
                   [this] { return boundsOf (tabs.getTabbedButtonBar()); } });
    s.push_back ({ "Have fun!", "Tip: Rec records what you hear into your Music folder. Now press Play and jam!", at (recordButton) });
    return s;
}

void HubMain::startTutorial()
{
    tabs.setCurrentTabIndex (0);
    tutorial = std::make_unique<Tutorial> (tutorialSteps(), [this]
    {
        settings.setValue ("tutorialDone", true);
        settings.saveIfNeeded();
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<HubMain> (this)] { if (sp != nullptr) sp->tutorial.reset(); });
    });
    addAndMakeVisible (*tutorial);
    tutorial->setBounds (getLocalBounds());
    tutorial->toFront (false);
}

//==============================================================================
void HubMain::paint (juce::Graphics& g)
{
    const auto& th = theme();
    g.fillAll (th.bg);
    auto r = getLocalBounds();
    auto top = r.removeFromTop (64);
    juce::ColourGradient grad (th.panelHi, 0.0f, 0.0f, th.panel, 0.0f, 64.0f, false);
    g.setGradientFill (grad);
    g.fillRect (top);
    auto song = r.removeFromTop (50);
    g.setColour (th.panel);
    g.fillRect (song);
    g.setColour (th.outline);
    g.drawHorizontalLine (64, 0.0f, (float) getWidth());
    g.drawHorizontalLine (114, 0.0f, (float) getWidth());

    // logo
    auto lg = logoArea.toFloat();
    drawCatIcon (g, lg.removeFromLeft (40.0f), th.accent, SrcChords, 0.0f);
    g.setColour (th.text);
    g.setFont (hubFont (22.0f, true));
    g.drawText ("CAT", lg.removeFromLeft (52.0f), juce::Justification::centredLeft);
    g.setColour (th.textDim);
    g.setFont (hubFont (22.0f, false));
    g.drawText ("HUB", lg, juce::Justification::centredLeft);

    // position: bar . beat + beat lights
    {
        auto pa = positionArea.toFloat();
        const bool playing = engine.isPlaying();
        const double ppq = engine.getPpq();
        const int bar = (int) std::floor (ppq / 4.0) + 1, beat = (int) std::floor (std::fmod (ppq, 4.0));
        g.setColour (th.text);
        g.setFont (hubFont (18.0f, true));
        g.drawText (playing ? "BAR " + juce::String (bar) : juce::String ("READY"), pa.removeFromTop (pa.getHeight() * 0.55f), juce::Justification::centredBottom);
        const float d = 8.0f, gap = 6.0f, x0 = pa.getCentreX() - (4 * d + 3 * gap) * 0.5f;
        for (int k = 0; k < 4; ++k)
        {
            g.setColour (playing && k == beat ? th.accent : th.raised);
            g.fillEllipse (x0 + k * (d + gap), pa.getY() + 6.0f, d, d);
        }
    }
    // labels
    label (g, bpm.getBounds().withY (bpm.getY() - 15).withHeight (14), "TEMPO");
    label (g, master.getBounds().withY (master.getY() - 15).withHeight (14), "MASTER");
    label (g, songLabelArea, "SONG", juce::Justification::centredLeft);
    label (g, padsLabelArea, engine.isSongMode() ? juce::String ("SONG MODE  -  the song plays every part (Compose tab). Pick a style or press Back to jamming to play the pads again")
                                                 : juce::String ("CHORD-CAT PADS  -  hold one with the mouse or keys 1-8   (Space = play / stop)"), juce::Justification::centredLeft);
    // master meter
    {
        auto m = masterMeterArea.toFloat();
        g.setColour (th.raised);
        g.fillRoundedRectangle (m, 3.0f);
        const float lv = juce::jlimit (0.0f, 1.0f, std::sqrt (shownMasterMeter));
        g.setColour (lv > 0.95f ? juce::Colour (0xffe0605a) : th.accent);
        g.fillRoundedRectangle (m.withTop (m.getBottom() - m.getHeight() * lv), 3.0f);
    }
    if (status.isNotEmpty())
    {
        g.setColour (th.textDim);
        g.setFont (hubFont (12.0f));
        g.drawText (status, getLocalBounds().removeFromBottom (20).reduced (14, 0), juce::Justification::centredLeft);
    }
}

void HubMain::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (64).reduced (12, 10);
    const bool narrow = top.getWidth() < 1200;     // small windows: no logo, tighter buttons
    const int gap = narrow ? 7 : 10;
    logoArea = top.removeFromLeft (narrow ? 0 : 150);
    top.removeFromLeft (narrow ? 0 : 8);
    auto col = [&top, gap] (int w) { auto c = top.removeFromLeft (w); top.removeFromLeft (gap); return c; };
    playButton.setBounds (col (narrow ? 80 : 96));
    positionArea = col (narrow ? 80 : 96);
    bpm.setBounds (col (narrow ? 124 : 150).withTrimmedTop (14));
    diceButton.setBounds (col (narrow ? 62 : 76));
    recordButton.setBounds (col (narrow ? 62 : 70));
    jamMenuButton.setBounds (col (narrow ? 56 : 66));
    helpButton.setBounds (top.removeFromRight (narrow ? 32 : 40));
    top.removeFromRight (gap);
    advancedButton.setBounds (top.removeFromRight (narrow ? 84 : 96));
    simpleButton.setBounds (top.removeFromRight (narrow ? 66 : 80));
    top.removeFromRight (narrow ? 10 : 14);
    fxButton.setBounds (top.removeFromRight (narrow ? 40 : 46));
    top.removeFromRight (8);
    masterMeterArea = top.removeFromRight (8);
    top.removeFromRight (6);
    master.setBounds (top.removeFromRight (juce::jlimit (60, 140, top.getWidth())).withTrimmedTop (14));

    auto song = r.removeFromTop (50).reduced (12, 9);
    songLabelArea = song.removeFromLeft (60);
    auto scol = [&song] (int w) { auto c = song.removeFromLeft (w); song.removeFromLeft (10); return c; };
    styleBox.setBounds (scol (200));
    keyBox.setBounds (scol (96));
    progressionBox.setBounds (scol (230));
    lengthBox.setBounds (scol (100));
    autoChords.setBounds (scol (200));

    r.removeFromBottom (status.isNotEmpty() ? 20 : 4);
    auto bottom = r.removeFromBottom (100).reduced (10, 2);
    padsLabelArea = bottom.removeFromTop (22).reduced (4, 0);
    pads.setBounds (bottom);
    tabs.setBounds (r);
    if (tutorial != nullptr) tutorial->setBounds (getLocalBounds());
}

void HubMain::updateTopBar()
{
    const bool playing = engine.isPlaying();
    if (playing != shownPlaying)
    {
        shownPlaying = playing;
        playButton.setButtonText (playing ? "Stop" : "Play");
        playButton.setColour (juce::TextButton::buttonColourId, playing ? theme().accent.withAlpha (0.55f) : theme().raised);
    }
    const bool rec = engine.isRecording();
    if (rec != shownRecording)
    {
        shownRecording = rec;
        recordButton.setColour (juce::TextButton::buttonColourId, rec ? juce::Colour (0xffd0605a) : theme().raised);
        recordButton.setButtonText (rec ? "Stop rec" : "Rec");
    }
    const double ppq = engine.getPpq();
    if (std::floor (ppq) != std::floor (shownPpq) || playing != (shownPpq >= 0.0)) { shownPpq = playing ? ppq : -1.0; repaint (positionArea); }
    const float m = engine.getMasterMeter();
    const float shown = juce::jmax (m, shownMasterMeter * 0.88f);
    if (std::abs (shown - shownMasterMeter) > 0.003f) { shownMasterMeter = shown; repaint (masterMeterArea.expanded (2)); }
}

void HubMain::timerCallback()
{
    if (++ticks % 10 == 0) { engine.pollPluginSettings(); engine.keepLifeRunning(); }
    if (! exporting)
    {
        const auto before = engine.getSong();
        engine.tickSong();
        if (engine.isSongMode() && engine.getSong() != before) refreshSongBar();
    }
    composePage->tick();
    rack->tick();
    pads.tick();
    updateTopBar();
}

void HubMain::togglePlay() { engine.setPlaying (! engine.isPlaying()); }

void HubMain::toggleRecord()
{
    if (engine.isRecording())
    {
        const auto f = engine.getRecordingFile();
        engine.stopRecording();
        status = "Saved: " + f.getFullPathName();
        resized();
        repaint();
        return;
    }
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Cat Hub");
    dir.createDirectory();
    const auto file = dir.getChildFile ("Cat Hub " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".wav");
    status = engine.startRecording (file) ? "Recording to " + file.getFullPathName() : "Could not record to " + file.getFullPathName();
    resized();
    repaint();
}

bool HubMain::keyPressed (const juce::KeyPress& key)
{
    if (tutorial != nullptr && (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)) { tutorial->next(); return true; }
    if (key == juce::KeyPress::spaceKey) { togglePlay(); return true; }
    if (key == juce::KeyPress (juce::KeyPress::F11Key)) { toggleFullScreen(); return true; }
    if (key.getModifiers().isCommandDown() && (key.getKeyCode() == 'S' || key.getKeyCode() == 's')) { saveJam(); return true; }
    if (key.getModifiers().isCommandDown() && (key.getKeyCode() == 'O' || key.getKeyCode() == 'o')) { openJam(); return true; }
    if (key == juce::KeyPress::escapeKey)
        if (auto* w = findParentComponentOfClass<juce::ResizableWindow>(); w != nullptr && w->isFullScreen()) { w->setFullScreen (false); return true; }
    const auto ch = key.getTextCharacter();
    if (ch >= '1' && ch <= '9')
    {
        heldKeyPad = (int) (ch - '1');
        heldKeyCode = key.getKeyCode();
        engine.setLivePad (heldKeyPad);
        return true;
    }
    return false;
}

bool HubMain::keyStateChanged (bool isKeyDown)
{
    if (! isKeyDown && heldKeyPad >= 0 && ! juce::KeyPress::isKeyCurrentlyDown (heldKeyCode))
    {
        heldKeyPad = -1;
        engine.setLivePad (-1);
        return true;
    }
    return false;
}
} // namespace hub

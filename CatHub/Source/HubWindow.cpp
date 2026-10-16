#include "HubWindow.h"

namespace hub
{
using namespace cat;

namespace
{
    juce::Colour partColour (int part)
    {
        switch (part)
        {
            case ChordPart: return Colours::accent;
            case BassPart:  return Colours::purple;
            case DrumPart:  return Colours::red;
            default:        return Colours::green;
        }
    }

    int catForPart (int part)
    {
        switch (part)
        {
            case ChordPart: return ChordCat;
            case BassPart:  return BassCat;
            case DrumPart:  return DrumCat;
            default:        return LifeCat;
        }
    }

    /** A little cat face, the Hub's icon for each part. */
    void drawCatFace (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, int part, float bounce)
    {
        r = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));
        r = r.translated (0.0f, -bounce * r.getHeight() * 0.06f);
        const auto head = r.reduced (r.getWidth() * 0.12f).withTrimmedTop (r.getHeight() * 0.12f);
        juce::Path ears;
        const float ew = head.getWidth() * 0.28f;
        ears.addTriangle (head.getX() + ew * 0.1f, head.getY() + ew * 0.7f, head.getX() + ew * 0.45f, head.getY() - ew * 0.75f, head.getX() + ew * 1.25f, head.getY() + ew * 0.1f);
        ears.addTriangle (head.getRight() - ew * 0.1f, head.getY() + ew * 0.7f, head.getRight() - ew * 0.45f, head.getY() - ew * 0.75f, head.getRight() - ew * 1.25f, head.getY() + ew * 0.1f);
        g.setColour (c);
        g.fillPath (ears);
        g.fillEllipse (head);
        g.setColour (Colours::background);
        const float eyeY = head.getY() + head.getHeight() * 0.42f, eyeW = head.getWidth() * 0.11f;
        const float blink = bounce > 0.8f ? 0.25f : 1.0f;
        g.fillEllipse (head.getX() + head.getWidth() * 0.3f - eyeW * 0.5f, eyeY - eyeW * 0.5f * blink, eyeW, eyeW * blink);
        g.fillEllipse (head.getX() + head.getWidth() * 0.7f - eyeW * 0.5f, eyeY - eyeW * 0.5f * blink, eyeW, eyeW * blink);
        // a different "hat" per part
        const float cx = head.getCentreX(), my = head.getY() + head.getHeight() * 0.66f;
        juce::Path mouth;
        mouth.startNewSubPath (cx - eyeW, my);
        mouth.quadraticTo (cx - eyeW * 0.5f, my + eyeW * 0.6f, cx, my);
        mouth.quadraticTo (cx + eyeW * 0.5f, my + eyeW * 0.6f, cx + eyeW, my);
        g.strokePath (mouth, juce::PathStrokeType (1.6f));
        g.setColour (c.brighter (0.5f));
        if (part == ChordPart)        // three little notes stacked like a chord
            for (int k = 0; k < 3; ++k) g.fillEllipse (r.getRight() - eyeW * 2.2f, r.getY() + eyeW * (0.4f + 1.1f * k), eyeW * 1.1f, eyeW * 0.8f);
        else if (part == BassPart)    // a long low string
            g.fillRect (r.getX(), r.getBottom() - 3.0f, r.getWidth(), 2.0f);
        else if (part == DrumPart)    // drum sticks
        {
            g.drawLine (r.getRight() - eyeW * 3.0f, r.getY() + eyeW, r.getRight(), r.getY() + eyeW * 3.0f, 2.0f);
            g.drawLine (r.getRight() - eyeW * 3.0f, r.getY() + eyeW * 3.0f, r.getRight(), r.getY() + eyeW, 2.0f);
        }
        else                          // a leaf
            g.fillEllipse (r.getRight() - eyeW * 2.4f, r.getY() + eyeW * 0.5f, eyeW * 2.0f, eyeW * 1.1f);
    }

    juce::String noteName (int n)
    {
        return juce::MidiMessage::getMidiNoteName (n, true, true, 3);
    }

    /** "Cmaj7", "Am/C", "G7" ... from the notes that sound; empty if it is not a known chord. */
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
        for (int pass = 0; pass < 2; ++pass)           // the bass note as root first
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

    void styleBar (juce::Slider& s, double lo, double hi, double step)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (lo, hi, step);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 20);
    }
}

//==============================================================================
CatCard::CatCard (HubMain& o, HubEngine& e, int p) : owner (o), engine (e), part (p)
{
    auto& s = engine.parts[(size_t) part];

    styleBar (volume, 0.0, 1.0, 0.01);
    volume.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    volume.setValue (s.volume.load(), juce::dontSendNotification);
    volume.onValueChange = [this] { engine.parts[(size_t) part].volume = (float) volume.getValue(); };
    volume.setTooltip ("How loud this Cat is in the mix");
    addAndMakeVisible (volume);

    styleBar (reverb, 0.0, 1.0, 0.01);
    reverb.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    reverb.setValue (s.reverb.load(), juce::dontSendNotification);
    reverb.onValueChange = [this] { engine.parts[(size_t) part].reverb = (float) reverb.getValue(); };
    reverb.setTooltip ("How much of this Cat goes into the room reverb");
    addAndMakeVisible (reverb);

    for (auto* l : { &soundLabel, &volLabel, &revLabel })
    {
        l->setColour (juce::Label::textColourId, Colours::textDim);
        l->setFont (catFont (11.0f, true));
        addAndMakeVisible (l);
    }
    soundLabel.setText ("SOUND", juce::dontSendNotification);
    volLabel.setText ("VOLUME", juce::dontSendNotification);
    revLabel.setText ("ROOM", juce::dontSendNotification);

    for (auto* b : { &mute, &solo })
    {
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonOnColourId, b == &mute ? Colours::red : Colours::accent);
        addAndMakeVisible (b);
    }
    mute.setTooltip ("Mute");
    solo.setTooltip ("Solo: only hear the soloed Cats");
    mute.onClick = [this] { engine.parts[(size_t) part].mute = mute.getToggleState(); };
    solo.onClick = [this] { engine.parts[(size_t) part].solo = solo.getToggleState(); };

    open.setButtonText ("Open " + juce::String (catName (catForPart (part))));
    open.setTooltip ("Show this Cat's own window (all its settings)");
    open.onClick = [this] { owner.showCat (catForPart (part)); };
    addAndMakeVisible (open);

    sound.onChange = [this]
    {
        if (sound.getSelectedId() > 0) engine.parts[(size_t) part].sound = sound.getSelectedId() - 1;
        // the Glide-Cat tab comes and goes with this choice; rebuild later, since that deletes this card
        if (part == ChordPart)
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<HubMain> (&owner)] { if (sp != nullptr) sp->updateGlideTab(); });
    };
    addAndMakeVisible (sound);
    refresh();
}

void CatCard::refresh()
{
    auto& s = engine.parts[(size_t) part];
    sound.clear (juce::dontSendNotification);
    juce::StringArray names;
    if (part == ChordPart) names = KeysSynth::soundNames();
    else if (part == BassPart) names = BassSynth::soundNames();
    else if (part == DrumPart) names = DrumKit::soundNames();
    for (int i = 0; i < names.size(); ++i) sound.addItem (names[i], i + 1);
    if (part == ChordPart)
    {
        sound.addItem ("Glide-Cat (synth)", names.size() + 1);
        sound.setItemEnabled (names.size() + 1, engine.glideAvailable());
    }
    if (part == LifePart)
    {
        sound.addItem ("Its own sound", 1);
        sound.setSelectedId (1, juce::dontSendNotification);
        sound.setEnabled (false);
        sound.setTooltip ("Life-Cat makes its own sound - change it in Life-Cat's window (SOUND tab)");
    }
    else
    {
        int sel = juce::jlimit (0, sound.getNumItems() - 1, s.sound.load());
        if (! sound.isItemEnabled (sel + 1)) sel = 0;
        sound.setSelectedId (sel + 1, juce::dontSendNotification);
        sound.setTooltip ("The built-in sound that plays this Cat's notes");
    }
    volume.setValue (s.volume.load(), juce::dontSendNotification);
    reverb.setValue (s.reverb.load(), juce::dontSendNotification);
    volume.updateText();
    reverb.updateText();
    mute.setToggleState (s.mute.load(), juce::dontSendNotification);
    solo.setToggleState (s.solo.load(), juce::dontSendNotification);
    open.setEnabled (engine.hasCat (catForPart (part)));
    repaint();
}

void CatCard::tick()
{
    // what this Cat is playing (10 times a second)
    if (++ticks % 3 == 0)
    {
        const auto notes = engine.parts[(size_t) part].takeNotes();
        juce::String big, small;
        if (part == DrumPart)
        {
            juce::StringArray names;
            for (int n : notes) names.addIfNotAlreadyThere (drumName (n));
            if (! names.isEmpty()) { big = names.joinIntoString ("  "); lastHitTime = juce::Time::getMillisecondCounter(); }
            else if (juce::Time::getMillisecondCounter() - lastHitTime < 400) big = nowBig;   // keep hits readable
        }
        else if (! notes.empty())
        {
            juce::StringArray names;
            for (int n : notes) names.add (noteName (n));
            if (part == BassPart) big = noteName (*std::min_element (notes.begin(), notes.end()));
            else
            {
                big = part == ChordPart ? chordName (notes) : juce::String();
                small = names.joinIntoString (" ");
                if (big.isEmpty()) { big = small; small = {}; }
            }
        }
        if (big != nowBig || small != nowSmall) { nowBig = big; nowSmall = small; repaint (nowArea); }
    }

    const float m = engine.parts[(size_t) part].meter.load();
    const float shown = juce::jmax (m, shownMeter * 0.88f);
    if (std::abs (shown - shownMeter) > 0.002f) { shownMeter = shown; repaint(); }
}

void CatCard::paint (juce::Graphics& g)
{
    const auto c = partColour (part);
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    CatLookAndFeel::drawGlassPanel (g, r, 10.0f, Colours::panel);
    g.setColour (c.withAlpha (0.8f));
    g.fillRoundedRectangle (r.removeFromTop (4.0f).reduced (14.0f, 0.0f), 2.0f);

    auto head = getLocalBounds().reduced (16, 12).removeFromTop (36);
    g.setColour (Colours::text);
    g.setFont (catFont (19.0f, true));
    g.drawText (catName (catForPart (part)), head.removeFromLeft (head.getWidth() - 60), juce::Justification::centredLeft);
    const bool here = engine.hasCat (catForPart (part));
    g.setColour (here ? c : Colours::red);
    g.setFont (catFont (11.0f, true));
    g.drawText (here ? partName (part) : "NOT FOUND", head, juce::Justification::centredRight);

    // face, bouncing with the level
    auto face = getLocalBounds().reduced (16, 0).withTrimmedTop (50).removeFromTop (92).toFloat();
    drawCatFace (g, face.withTrimmedRight (26.0f), here ? c : Colours::textDim, part, juce::jlimit (0.0f, 1.0f, shownMeter * 2.5f));

    // level meter
    auto meter = face.removeFromRight (12.0f).reduced (0.0f, 6.0f);
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (meter, 3.0f);
    const float lv = juce::jlimit (0.0f, 1.0f, std::sqrt (shownMeter));
    g.setColour (lv > 0.95f ? Colours::red : c);
    g.fillRoundedRectangle (meter.withTop (meter.getBottom() - meter.getHeight() * lv), 3.0f);

    // now playing
    if (here && ! nowArea.isEmpty())
    {
        auto a = nowArea.toFloat();
        g.setColour (Colours::raised.withAlpha (0.5f));
        g.fillRoundedRectangle (a, 8.0f);
        g.setColour (Colours::textDim);
        g.setFont (catFont (10.5f, true));
        g.drawText ("NOW PLAYING", a.removeFromTop (22.0f).reduced (10.0f, 0.0f), juce::Justification::bottomLeft);
        const bool idle = nowBig.isEmpty();
        g.setColour (idle ? Colours::textDim : c);
        g.setFont (catFont (part == DrumPart ? 20.0f : 30.0f, true));
        g.drawFittedText (idle ? juce::String ("-") : nowBig, a.removeFromTop (a.getHeight() * 0.6f).toNearestInt().reduced (8, 0), juce::Justification::centred, 2);
        g.setColour (Colours::textDim);
        g.setFont (catFont (12.0f));
        g.drawFittedText (part == LifePart && idle ? juce::String ("its creatures sing along") : nowSmall,
                          a.toNearestInt().reduced (8, 2), juce::Justification::centredTop, 2);
    }

    if (! here)
    {
        g.setColour (Colours::textDim);
        g.setFont (catFont (12.0f));
        g.drawFittedText ("Install the Cat Suite plug-ins, or use \"Find Cats...\" at the top.",
                          getLocalBounds().reduced (18).withTrimmedTop (140).removeFromTop (40), juce::Justification::centred, 3);
    }
}

void CatCard::resized()
{
    auto r = getLocalBounds().reduced (16, 12).withTrimmedTop (36 + 96);
    soundLabel.setBounds (r.removeFromTop (16));
    sound.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);
    volLabel.setBounds (r.removeFromTop (16));
    volume.setBounds (r.removeFromTop (24));
    r.removeFromTop (8);
    revLabel.setBounds (r.removeFromTop (16));
    reverb.setBounds (r.removeFromTop (24));
    r.removeFromTop (10);
    auto row = r.removeFromTop (28);
    mute.setBounds (row.removeFromLeft (40));
    row.removeFromLeft (6);
    solo.setBounds (row.removeFromLeft (40));
    row.removeFromLeft (8);
    open.setBounds (row);
    r.removeFromTop (14);
    nowArea = r.withHeight (juce::jmin (r.getHeight(), 130));
}

//==============================================================================
int PadStrip::padAt (juce::Point<float> p) const
{
    const int n = engine.getNumPads();
    const float w = (float) getWidth() / (float) n;
    return juce::jlimit (0, n - 1, (int) (p.x / w));
}

void PadStrip::paint (juce::Graphics& g)
{
    const int n = engine.getNumPads();
    const int cur = engine.getCurrentPad();
    const float w = (float) getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto r = juce::Rectangle<float> (i * w, 0.0f, w, (float) getHeight()).reduced (4.0f);
        const bool on = i == cur;
        g.setColour (on ? Colours::accent.withAlpha (0.85f) : Colours::raised);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (on ? Colours::background : Colours::textDim);
        g.setFont (catFont (18.0f, true));
        g.drawText (juce::String (i + 1), r.removeFromTop (r.getHeight() * 0.62f), juce::Justification::centredBottom);
        g.setFont (catFont (10.5f));
        g.drawText (i < 9 ? "key " + juce::String (i + 1) : juce::String(), r, juce::Justification::centredTop);
    }
}

void PadStrip::mouseDown (const juce::MouseEvent& e)  { engine.setLivePad (padAt (e.position)); }
void PadStrip::mouseUp (const juce::MouseEvent&)      { engine.setLivePad (-1); }

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
JamPage::JamPage (HubMain& owner, HubEngine& e) : engine (e), pads (e)
{
    for (int p = 0; p < NumParts; ++p) addAndMakeVisible (cards.add (new CatCard (owner, engine, p)));
    addAndMakeVisible (pads);
    padsLabel.setText ("CHORD-CAT PADS  -  the song steps through them; hold one with the mouse or keys 1-8  (Space = play / stop)", juce::dontSendNotification);
    padsLabel.setFont (catFont (11.5f, true));
    padsLabel.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (padsLabel);
}

void JamPage::refresh() { for (auto* c : cards) c->refresh(); }
void JamPage::tick()    { for (auto* c : cards) c->tick(); pads.tick(); }

void JamPage::paint (juce::Graphics& g) { g.fillAll (Colours::background); }

void JamPage::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto bottom = r.removeFromBottom (110);
    padsLabel.setBounds (bottom.removeFromTop (22).reduced (6, 0));
    pads.setBounds (bottom);
    r.removeFromBottom (6);
    const int w = r.getWidth() / juce::jmax (1, cards.size());
    for (auto* c : cards) c->setBounds (r.removeFromLeft (w));
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

CatTab::~CatTab()
{
    editor.reset();      // the Cat's window always goes before the Cat itself
}

void CatTab::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);
    if (editor == nullptr)
    {
        g.setColour (Colours::textDim);
        g.setFont (catFont (14.0f));
        g.drawText ("This Cat has no window", getLocalBounds(), juce::Justification::centred);
    }
}

void CatTab::resized()
{
    if (editor == nullptr || naturalW <= 0 || naturalH <= 0 || getWidth() <= 0 || getHeight() <= 0) return;
    // A Cat's window scales as one piece (50 % - 250 %): make it fill the tab, aspect kept.
    // No scrolling viewport: a plug-in window on Windows is a native window and would
    // draw over the tab bar when scrolled.
    if (editor->isResizable())
    {
        const float k = juce::jmin ((float) getWidth() / (float) naturalW, (float) getHeight() / (float) naturalH);
        const int w = juce::jmax (100, (int) ((float) naturalW * k)), h = juce::jmax (80, (int) ((float) naturalH * k));
        if (w != editor->getWidth() || h != editor->getHeight()) editor->setSize (w, h);
    }
    editor->setTopLeftPosition (juce::jmax (0, (getWidth() - editor->getWidth()) / 2), juce::jmax (0, (getHeight() - editor->getHeight()) / 2));
}

//==============================================================================
HubMain::HubMain (HubEngine& e, juce::AudioDeviceManager& dm, juce::PropertiesFile& s)
    : engine (e), deviceManager (dm), settings (s)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    playButton.setClickingTogglesState (false);
    playButton.setTooltip ("Start / stop every Cat (Space)");
    playButton.onClick = [this] { togglePlay(); };
    recordButton.setTooltip ("Record what you hear to a WAV file (in your Music folder)");
    recordButton.onClick = [this] { toggleRecord(); };
    audioButton.setTooltip ("Sound card and buffer size");
    audioButton.onClick = [this] { showAudioSettings(); };
    findButton.setTooltip ("Choose the folder that has Chord-Cat.vst3, Bass-Cat.vst3 ...");
    findButton.onClick = [this] { findCats(); };
    for (auto* b : { &playButton, &recordButton, &audioButton, &findButton }) addAndMakeVisible (b);

    styleBar (bpm, 50.0, 200.0, 1.0);
    bpm.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " BPM"; };
    bpm.setValue (engine.bpm.load(), juce::dontSendNotification);
    bpm.onValueChange = [this] { engine.bpm = bpm.getValue(); };
    bpm.setTooltip ("Tempo for every Cat");
    addAndMakeVisible (bpm);

    styleBar (master, 0.0, 1.0, 0.01);
    master.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    master.setValue (engine.masterVolume.load(), juce::dontSendNotification);
    master.onValueChange = [this] { engine.masterVolume = (float) master.getValue(); };
    master.setTooltip ("Master volume");
    addAndMakeVisible (master);

    chordLength.addItemList ({ "1 bar", "2 bars", "4 bars" }, 1);
    chordLength.setSelectedId (engine.barsPerChord.load() >= 4 ? 3 : (engine.barsPerChord.load() >= 2 ? 2 : 1), juce::dontSendNotification);
    chordLength.onChange = [this] { engine.barsPerChord = chordLength.getSelectedId() == 3 ? 4 : chordLength.getSelectedId(); };
    chordLength.setTooltip ("How long each chord (pad) lasts while the song plays");
    addAndMakeVisible (chordLength);

    autoChords.setToggleState (engine.autoChords.load(), juce::dontSendNotification);
    autoChords.onClick = [this] { engine.autoChords = autoChords.getToggleState(); };
    autoChords.setTooltip ("On: the Hub walks through Chord-Cat's pads by itself. Off: you play the pads (keys 1-8).");
    autoChords.setColour (juce::ToggleButton::textColourId, Colours::text);
    addAndMakeVisible (autoChords);

    for (auto* l : { &bpmLabel, &masterLabel, &lengthLabel })
    {
        l->setFont (catFont (10.5f, true));
        l->setColour (juce::Label::textColourId, Colours::textDim);
        addAndMakeVisible (l);
    }
    bpmLabel.setText ("TEMPO", juce::dontSendNotification);
    masterLabel.setText ("MASTER", juce::dontSendNotification);
    lengthLabel.setText ("EACH CHORD", juce::dontSendNotification);
    positionLabel.setFont (catFont (16.0f, true));
    positionLabel.setColour (juce::Label::textColourId, Colours::text);
    positionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (positionLabel);
    statusLabel.setFont (catFont (11.5f));
    statusLabel.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (statusLabel);

    tabs.setTabBarDepth (32);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);
    rebuildTabs();

    setSize (1100, 760);
    updateTopBar();
    startTimerHz (30);
}

HubMain::~HubMain()
{
    stopTimer();
    if (auto* w = audioWindow.getComponent()) delete w;   // never leave a window behind
    chooser.reset();
    tabs.clearTabs();      // deletes the Cat windows before anything else goes
    setLookAndFeel (nullptr);
}

juce::String HubMain::getMissingText() const
{
    juce::StringArray missing;
    for (int id = 0; id < GlideCat; ++id)
        if (! engine.hasCat (id)) missing.add (catName (id));
    return missing.isEmpty() ? juce::String() : "Not found: " + missing.joinIntoString (", ");
}

void HubMain::rebuildTabs()
{
    const int current = tabs.getCurrentTabIndex();
    tabs.clearTabs();
    jam = new JamPage (*this, engine);
    tabs.addTab ("Jam", Colours::panel, jam, true);
    tabIndexForCat.fill (-1);
    for (int id = 0; id < NumCats; ++id)
    {
        auto* c = engine.getCat (id);
        if (c == nullptr) continue;
        if (id == GlideCat && engine.parts[ChordPart].sound.load() != engine.keysGlideIndex()) continue;
        tabIndexForCat[(size_t) id] = tabs.getNumTabs();
        tabs.addTab (catName (id), Colours::panel, new CatTab (*c), true);
    }
    tabs.setCurrentTabIndex (juce::jlimit (0, tabs.getNumTabs() - 1, juce::jmax (0, current)), false);
    findButton.setVisible (getMissingText().isNotEmpty());
    statusLabel.setText (getMissingText(), juce::dontSendNotification);
    resized();
}

void HubMain::updateGlideTab()
{
    const bool want = engine.hasCat (GlideCat) && engine.parts[ChordPart].sound.load() == engine.keysGlideIndex();
    if (want != (tabIndexForCat[(size_t) GlideCat] >= 0)) rebuildTabs();
}

void HubMain::showCat (int catId)
{
    if (juce::isPositiveAndBelow (catId, (int) NumCats) && tabIndexForCat[(size_t) catId] >= 0)
        tabs.setCurrentTabIndex (tabIndexForCat[(size_t) catId]);
}

void HubMain::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);
    auto top = getLocalBounds().removeFromTop (64).toFloat();
    g.setColour (Colours::panel);
    g.fillRect (top);
    CatLookAndFeel::drawLogo (g, top.removeFromLeft (170.0f).reduced (16.0f, 18.0f), "CAT", "HUB");
}

void HubMain::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (64).reduced (10, 10);
    top.removeFromLeft (165);
    playButton.setBounds (top.removeFromLeft (88));
    top.removeFromLeft (10);
    auto col = [&top] (int w) { auto c = top.removeFromLeft (w); top.removeFromLeft (10); return c; };
    {
        auto c = col (140);
        bpmLabel.setBounds (c.removeFromTop (14));
        bpm.setBounds (c);
    }
    {
        auto c = col (100);
        lengthLabel.setBounds (c.removeFromTop (14));
        chordLength.setBounds (c);
    }
    autoChords.setBounds (col (170).withTrimmedTop (14));
    positionLabel.setBounds (col (90));
    recordButton.setBounds (col (58));
    {
        auto c = col (110);
        masterLabel.setBounds (c.removeFromTop (14));
        master.setBounds (c);
    }
    audioButton.setBounds (col (64));
    if (findButton.isVisible()) findButton.setBounds (col (100));

    auto status = r.removeFromBottom (22);
    statusLabel.setBounds (status.reduced (12, 0));
    tabs.setBounds (r);
}

void HubMain::updateTopBar()
{
    const bool playing = engine.isPlaying();
    if (playing != shownPlaying)
    {
        shownPlaying = playing;
        playButton.setButtonText (playing ? "Stop" : "Play");
        playButton.setColour (juce::TextButton::buttonColourId, playing ? Colours::accent.withAlpha (0.45f) : Colours::raised);
    }
    const bool rec = engine.isRecording();
    if (rec != shownRecording)
    {
        shownRecording = rec;
        recordButton.setColour (juce::TextButton::buttonColourId, rec ? Colours::red : Colours::raised);
        recordButton.setButtonText (rec ? "Stop rec" : "Rec");
    }
    const double ppq = engine.getPpq();
    const int bar = (int) std::floor (ppq / 4.0) + 1;
    const int beat = (int) std::floor (std::fmod (ppq, 4.0)) + 1;
    positionLabel.setText (playing ? juce::String (bar) + "." + juce::String (beat) : juce::String ("-.-"), juce::dontSendNotification);
}

void HubMain::timerCallback()
{
    static int counter = 0;
    if (++counter % 10 == 0) engine.pollPluginSettings();
    if (jam != nullptr) jam->tick();
    updateTopBar();
}

void HubMain::togglePlay()
{
    engine.setPlaying (! engine.isPlaying());
}

void HubMain::toggleRecord()
{
    if (engine.isRecording())
    {
        const auto f = engine.getRecordingFile();
        engine.stopRecording();
        statusLabel.setText ("Saved: " + f.getFullPathName(), juce::dontSendNotification);
        return;
    }
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Cat Hub");
    dir.createDirectory();
    const auto file = dir.getChildFile ("Cat Hub " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".wav");
    if (engine.startRecording (file)) statusLabel.setText ("Recording to " + file.getFullPathName(), juce::dontSendNotification);
    else statusLabel.setText ("Could not record to " + file.getFullPathName(), juce::dontSendNotification);
}

void HubMain::showAudioSettings()
{
    if (audioWindow != nullptr) { audioWindow->toFront (true); return; }
    auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 0, 2, 2, false, false, true, false);
    selector->setSize (520, 380);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector);
    o.dialogTitle = "Cat Hub - audio settings";
    o.dialogBackgroundColour = Colours::panel;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.escapeKeyTriggersCloseButton = true;
    audioWindow = o.launchAsync();
}

void HubMain::findCats()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose the folder with Chord-Cat.vst3, Bass-Cat.vst3, Drum-Cat.vst3, Life-Cat.vst3",
                                                   juce::File (settings.getValue ("catsFolder")));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
    {
        const auto dir = fc.getResult();
        if (! dir.isDirectory()) return;
        settings.setValue ("catsFolder", dir.getFullPathName());
        settings.saveIfNeeded();
        tabs.clearTabs();
        jam = nullptr;
        deviceManager.removeAudioCallback (&engine);          // never swap Cats while they play
        const auto report = engine.loadCats (HubEngine::defaultSearchFolders (dir));
        deviceManager.addAudioCallback (&engine);
        rebuildTabs();
        if (report.isNotEmpty()) statusLabel.setText (report.replace ("\n", "  |  "), juce::dontSendNotification);
    });
}

bool HubMain::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey) { togglePlay(); return true; }
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

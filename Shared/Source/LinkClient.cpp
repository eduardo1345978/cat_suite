#include "LinkClient.h"
#include "CatLookAndFeel.h"

namespace cat
{
namespace
{
    const char* optionIds[] = { "catLinkOn", "catLinkKey", "catLinkChords", "catLinkLife", "catLinkTempo" };

    juce::String noteNames (const int* notes, int n)
    {
        juce::String s;
        for (int i = 0; i < n; ++i) s << juce::MidiMessage::getMidiNoteName (notes[i], true, false, 3) << (i + 1 < n ? " " : "");
        return s;
    }
}

LinkClient::LinkClient (juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& s, const juce::String& name, Caps c)
    : caps (std::move (c)), pluginName (name), state (s), processor (p),
      id ((juce::uint64) juce::Random::getSystemRandom().nextInt64() | 1u)
{
    readOptions();
    seenChordSeq = bus->chordSeq();       // never replay a chord from before we existed
    seenEvents = bus->eventCount();
    seenLinkAll = bus->linkAllSeq();
    seenKeySeq = bus->keySeq();
    slot = bus->join (pluginName, id);
    startTimerHz (10);
}

LinkClient::~LinkClient()
{
    stopTimer();
    bus->leave (slot, id);
}

void LinkClient::readOptions()
{
    const bool defaults[] = { true, caps.key, caps.sendChords, caps.sendLife, true };
    for (int i = 0; i < numOptions; ++i)
        options[(size_t) i].store ((bool) state.state.getProperty (optionIds[i], defaults[i]), std::memory_order_relaxed);
}

void LinkClient::setOption (Option o, bool on)
{
    state.state.setProperty (optionIds[(int) o], on, nullptr);
    options[(size_t) o].store (on, std::memory_order_relaxed);
    if (on && (o == Chords || o == On)) seenChordSeq = 0;          // pick up the current chord straight away
    if (on && (o == Key || o == On) && caps.key) keyInitialised = false;
}

void LinkClient::timerCallback()
{
    readOptions();
    const int beat = ++beatCount;
    if (beat % 5 == 0)
        bus->heartbeat (slot, id, (option (On) ? 1u : 0u) | (option (Key) ? 2u : 0u) | (option (Chords) ? 4u : 0u) | (option (Life) ? 8u : 0u));
    if (slot < 0 && beat % 20 == 0) slot = bus->join (pluginName, id);

    // "Link all Cats" from any panel
    const auto la = bus->linkAllSeq();
    if (la != seenLinkAll)
    {
        seenLinkAll = la;
        setOption (On, true);
        if (caps.key) setOption (Key, true);
        if (caps.sendChords || caps.receiveChords) setOption (Chords, true);
        if (caps.sendLife || caps.receiveLife) setOption (Life, true);
        setOption (Tempo, true);
        if (caps.grooveParam.isNotEmpty())
            if (auto* prm = state.getParameter (caps.grooveParam)) prm->setValueNotifyingHost (1.0f);
    }

    // key + scale: share my changes, follow the others
    if (caps.key && getKey && applyKey)
    {
        const auto own = getKey();
        if (! keyInitialised) { lastOwnKey = own; keyInitialised = true; }
        else if (own != lastOwnKey)
        {
            lastOwnKey = own;
            if (sharing (Key)) bus->publishKey (own.first, own.second, id);
            seenKeySeq = bus->keySeq();
        }
        int root = 0;
        juce::uint16 mask = 0;
        juce::uint64 source = 0;
        if (bus->readKey (seenKeySeq, root, mask, source) && source != id && sharing (Key))
        {
            applyKey (root, mask);
            lastOwnKey = getKey();      // do not echo it back
        }
    }
}

//==============================================================================
void LinkClient::releaseInjected (juce::MidiBuffer& midi)
{
    for (int i = 0; i < numInjected; ++i) midi.addEvent (juce::MidiMessage::noteOff (1, injected[(size_t) i]), 0);
    numInjected = 0;
    uiChordCount = 0;
}

void LinkClient::processInput (juce::MidiBuffer& midi, int)
{
    if (! caps.receiveChords) return;
    if (! sharing (Chords))
    {
        if (numInjected > 0) releaseInjected (midi);
        return;
    }
    std::array<int, 16> notes {};
    int n = 0;
    juce::uint64 source = 0;
    if (! bus->readChord (seenChordSeq, notes, n, source) || source == id) return;
    releaseInjected (midi);
    for (int i = 0; i < n; ++i)
    {
        midi.addEvent (juce::MidiMessage::noteOn (1, notes[(size_t) i], (juce::uint8) 100), 0);
        injected[(size_t) i] = notes[(size_t) i];
        uiChord[i].store (notes[(size_t) i], std::memory_order_relaxed);
    }
    numInjected = n;
    uiChordCount = n;
}

void LinkClient::processOutput (const juce::MidiBuffer& midi, int numSamples, double sampleRate)
{
    if (! caps.sendChords) return;
    bool changed = false;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) { auto& h = outHeld[(size_t) m.getNoteNumber()]; if (h < 255) ++h; changed = true; }
        else if (m.isNoteOff()) { auto& h = outHeld[(size_t) m.getNoteNumber()]; if (h > 0) --h; changed = true; }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { outHeld.fill (0); changed = true; }
    }
    if (changed) { outChanged = true; outSettle = 0.0; return; }     // wait until the strum has settled
    if (! outChanged) return;
    outSettle += (double) numSamples / juce::jmax (1.0, sampleRate);
    if (outSettle < 0.03) return;

    std::array<int, 16> notes {};
    int n = 0;
    for (int k = 0; k < 128 && n < 16; ++k) if (outHeld[(size_t) k] > 0) notes[(size_t) n++] = k;
    // a rhythm pattern lets go between hits: only a real pause (0.4 s) ends the chord for the followers
    if (n == 0 && outSettle < 0.4) return;
    outChanged = false;
    if (n == numLastSent && std::equal (notes.begin(), notes.begin() + n, lastSent.begin())) return;
    lastSent = notes;
    numLastSent = n;
    for (int i = 0; i < n; ++i) uiChord[i].store (notes[(size_t) i], std::memory_order_relaxed);
    uiChordCount = n;
    if (sharing (Chords)) bus->publishChord (notes.data(), n, id);
}

void LinkClient::publishLifeEvent (int kind, int role)
{
    if (caps.sendLife && sharing (Life)) bus->publishEvent (kind, role, id);
}

void LinkClient::pollLifeEvents (const std::function<void (int, int)>& fn)
{
    if (! caps.receiveLife) return;
    if (! sharing (Life)) { seenEvents = bus->eventCount(); return; }
    bus->readEvents (seenEvents, id, fn);
}

juce::AudioPlayHead* LinkClient::playHead (juce::AudioPlayHead* host, int numSamples, double sampleRate)
{
    if (! isStandalone() || ! sharing (Tempo)) return host;
    bool playing = false;
    double bpm = 120.0;
    juce::int64 start = 0;
    if (! bus->readTransport (playing, bpm, start)) return host;
    sharedHead.bpm = bpm;
    if (playing)
    {
        // where the shared clock says we are; follow it smoothly, jump if far off
        const double expected = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) * bpm / 60.0;
        if (! sharedHead.playing || std::abs (expected - nextPpq) > 0.08) nextPpq = expected;
        else nextPpq += (expected - nextPpq) * 0.05;
        sharedHead.ppq = nextPpq;
        nextPpq += (double) numSamples / juce::jmax (1.0, sampleRate) * bpm / 60.0;
    }
    sharedHead.playing = playing;
    return &sharedHead;
}

void LinkClient::setTransport (bool playing, double bpm) { bus->publishTransport (playing, bpm, id); }

bool LinkClient::transportPlaying() const
{
    bool p = false; double b = 120.0; juce::int64 t = 0;
    return bus->readTransport (p, b, t) && p;
}

double LinkClient::transportBpm() const
{
    bool p = false; double b = 120.0; juce::int64 t = 0;
    bus->readTransport (p, b, t);
    return b;
}

std::vector<LinkBus::MemberInfo> LinkClient::members() const { return bus->members (id); }
void LinkClient::linkAll() { bus->requestLinkAll(); }

juce::String LinkClient::lastChordText() const
{
    int n = juce::jlimit (0, 16, uiChordCount.load());
    int notes[16] {};
    for (int i = 0; i < n; ++i) notes[i] = uiChord[i].load (std::memory_order_relaxed);
    return n > 0 ? noteNames (notes, n) : juce::String();
}

//==============================================================================
/** The Link panel (opens from the LINK button). */
class LinkPanel : public juce::Component, private juce::Timer
{
public:
    explicit LinkPanel (LinkClient& c) : client (c)
    {
        setLookAndFeel (&lnf);
        auto setup = [this] (juce::TextButton& b, const juce::String& text, LinkClient::Option o, const juce::String& tip)
        {
            b.setButtonText (text);
            b.setName (text);
            b.setClickingTogglesState (true);
            b.setToggleState (client.option (o), juce::dontSendNotification);
            b.setTooltip (tip);
            b.onClick = [this, &b, o] { client.setOption (o, b.getToggleState()); refresh(); };
            addAndMakeVisible (b);
        };
        setup (onButton, "Link this Cat", LinkClient::On, "Turn Cat Link on or off for this Cat");
        if (client.caps.key) setup (keyButton, "Share key & scale", LinkClient::Key, "Change the key or scale here and the other Cats follow (and the other way round)");
        if (client.caps.sendChords) setup (chordButton, "Send my chords to the others", LinkClient::Chords, "Bass-Cat, Patt-Cat, Life-Cat... can follow these chords without MIDI routing");
        else if (client.caps.receiveChords) setup (chordButton, client.caps.receiveChordsText, LinkClient::Chords, "Play along with Chord-Cat without routing MIDI in Cubase");
        if (client.caps.sendLife) setup (lifeButton, "Send births & deaths", LinkClient::Life, "Drum-Cat can answer them with hits");
        else if (client.caps.receiveLife) setup (lifeButton, client.caps.receiveLifeText, LinkClient::Life, "Births and deaths in Life-Cat play extra hits");
        if (client.isStandalone())
        {
            setup (tempoButton, "Share tempo & play / stop", LinkClient::Tempo, "The Cat apps play together: same tempo, start and stop together");
            playButton.setTooltip ("Start / stop every linked Cat app");
            playButton.onClick = [this] { client.setTransport (! client.transportPlaying(), client.transportBpm()); refresh(); };
            addAndMakeVisible (playButton);
            bpm.setSliderStyle (juce::Slider::LinearBar);
            bpm.setRange (40.0, 240.0, 1.0);
            bpm.setTextValueSuffix (" BPM");
            bpm.setValue (client.transportBpm(), juce::dontSendNotification);
            bpm.onValueChange = [this] { client.setTransport (client.transportPlaying(), bpm.getValue()); };
            bpm.setTooltip ("Tempo for all linked Cat apps");
            addAndMakeVisible (bpm);
        }
        if (client.caps.grooveParam.isNotEmpty() && client.state.getParameter (client.caps.grooveParam) != nullptr)
        {
            grooveButton.setButtonText ("Follow Drum-Cat's groove");
            grooveButton.setName ("Follow Drum-Cat's groove");
            grooveButton.setClickingTogglesState (true);
            grooveButton.setTooltip ("Take swing and human timing from Drum-Cat");
            grooveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (client.state, client.caps.grooveParam, grooveButton);
            addAndMakeVisible (grooveButton);
        }
        for (auto* b : { &onButton, &keyButton, &chordButton, &lifeButton, &tempoButton, &grooveButton, &allButton, &playButton })
        {
            b->setColour (juce::TextButton::buttonColourId, Colours::raised);
            b->setColour (juce::TextButton::buttonOnColourId, Colours::accent);
        }
        allButton.setButtonText ("Link all Cats");
        allButton.setTooltip ("Turn on every link in every open Cat");
        allButton.onClick = [this] { client.linkAll(); };
        addAndMakeVisible (allButton);
        setSize (330, 150 + 34 * numToggles() + (client.isStandalone() ? 38 : 0));
        startTimerHz (4);
        refresh();
    }

    ~LinkPanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (12, 10);
        g.setColour (Colours::text);
        g.setFont (catFont (15.0f, true));
        g.drawText ("CAT LINK", r.removeFromTop (22), juce::Justification::centredLeft);
        g.setFont (catFont (11.5f));
        g.setColour (Colours::textDim);
        const auto list = client.members();
        juce::String who = client.busOk() ? juce::String (list.size()) + (list.size() == 1 ? " Cat open" : " Cats open") : "Cat Link is not available";
        g.drawText (who, r.removeFromTop (16), juce::Justification::centredLeft);

        // chips with the open Cats
        auto chips = r.removeFromTop (48).toFloat();
        float x = chips.getX(), y = chips.getY();
        g.setFont (catFont (11.0f, true));
        for (const auto& m : list)
        {
            const auto text = m.plugin + (m.self ? " (this)" : "");
            const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 22.0f;
            if (x + w > chips.getRight()) { x = chips.getX(); y += 22.0f; }
            if (y > chips.getBottom() - 18.0f) break;
            const bool on = (m.flags & 1u) != 0;
            juce::Rectangle<float> chip (x, y, w, 18.0f);
            g.setColour (m.self ? Colours::accent.withAlpha (0.25f) : Colours::raised);
            g.fillRoundedRectangle (chip, 9.0f);
            g.setColour (on ? Colours::green : Colours::textDim);
            g.fillEllipse (chip.getX() + 7.0f, chip.getCentreY() - 3.0f, 6.0f, 6.0f);
            g.setColour (Colours::text);
            g.drawText (text, chip.withTrimmedLeft (16.0f), juce::Justification::centredLeft);
            x += w + 6.0f;
        }

        auto bottom = getLocalBounds().reduced (12, 10).removeFromBottom (58).removeFromTop (20);
        g.setFont (catFont (11.0f));
        g.setColour (Colours::textDim);
        const auto chord = client.lastChordText();
        if (chord.isNotEmpty() && client.option (LinkClient::Chords))
            g.drawText ((client.caps.sendChords ? "Sending: " : "Following: ") + chord, bottom, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 10);
        r.removeFromTop (22 + 16 + 48 + 4);
        for (auto* b : toggles())
            if (b->isVisible()) b->setBounds (r.removeFromTop (34).reduced (0, 2));
        if (client.isStandalone())
        {
            auto row = r.removeFromTop (36).reduced (0, 3);
            playButton.setBounds (row.removeFromLeft (90));
            row.removeFromLeft (8);
            bpm.setBounds (row);
        }
        allButton.setBounds (r.removeFromBottom (30).reduced (0, 1));
    }

private:
    std::vector<juce::Button*> toggles() { return { &onButton, &keyButton, &chordButton, &lifeButton, &tempoButton, &grooveButton }; }
    int numToggles() { int n = 0; for (auto* b : toggles()) n += b->isVisible() ? 1 : 0; return n; }
    void refresh()
    {
        const bool on = client.option (LinkClient::On);
        for (auto* b : { &keyButton, &chordButton, &lifeButton, &tempoButton }) b->setEnabled (on);
        playButton.setButtonText (client.transportPlaying() ? "Stop" : "Play");
        playButton.setEnabled (on && client.option (LinkClient::Tempo));
        bpm.setEnabled (on && client.option (LinkClient::Tempo));
        for (auto* b : { &onButton, &keyButton, &chordButton, &lifeButton, &tempoButton, &grooveButton })
            if (b->getName().isNotEmpty()) b->setButtonText ((b->getToggleState() ? "ON    " : "off    ") + b->getName());
        repaint();
    }
    void timerCallback() override
    {
        onButton.setToggleState (client.option (LinkClient::On), juce::dontSendNotification);
        keyButton.setToggleState (client.option (LinkClient::Key), juce::dontSendNotification);
        chordButton.setToggleState (client.option (LinkClient::Chords), juce::dontSendNotification);
        lifeButton.setToggleState (client.option (LinkClient::Life), juce::dontSendNotification);
        tempoButton.setToggleState (client.option (LinkClient::Tempo), juce::dontSendNotification);
        if (! bpm.isMouseButtonDown()) bpm.setValue (client.transportBpm(), juce::dontSendNotification);
        refresh();
    }

    CatLookAndFeel lnf;
    LinkClient& client;
    juce::TextButton onButton, keyButton, chordButton, lifeButton, tempoButton, grooveButton, allButton, playButton { "Play" };
    juce::Slider bpm;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> grooveAttachment;
};

//==============================================================================
LinkButton::LinkButton (LinkClient& c) : juce::Button ("Link"), client (c)
{
    setTooltip ("Cat Link: share key, chords, groove and Life-Cat events with the other Cats (click for options)");
    startTimerHz (2);
    timerCallback();
}

void LinkButton::timerCallback()
{
    int n = 0;
    for (const auto& m : client.members()) n += m.self ? 0 : 1;
    if (n != others) { others = n; repaint(); }
}

void LinkButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = client.option (LinkClient::On);
    g.setColour ((on && others > 0 ? Colours::accent.withAlpha (0.28f) : Colours::raised).brighter (highlighted ? 0.12f : 0.0f).darker (down ? 0.2f : 0.0f));
    g.fillRoundedRectangle (r, 5.0f);
    // two linked rings
    const float cy = r.getCentreY(), cx = r.getX() + 13.0f;
    g.setColour (on ? (others > 0 ? Colours::accent : Colours::text) : Colours::textDim);
    g.drawEllipse (cx - 7.0f, cy - 4.0f, 9.0f, 8.0f, 1.6f);
    g.drawEllipse (cx - 1.0f, cy - 4.0f, 9.0f, 8.0f, 1.6f);
    g.setFont (catFont (11.0f, true));
    g.drawText (others > 0 ? juce::String (others) : juce::String ("-"), r.withTrimmedLeft (24.0f), juce::Justification::centred);
}

void LinkButton::clicked()
{
    juce::CallOutBox::launchAsynchronously (createLinkPanel (client), getScreenBounds(), nullptr);
}

std::unique_ptr<juce::Component> createLinkPanel (LinkClient& c) { return std::make_unique<LinkPanel> (c); }

} // namespace cat

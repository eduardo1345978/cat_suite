#include "LinkPanel.h"

namespace catbox
{
namespace
{
const juce::Colour panelBg   { 0xff141518 };
const juce::Colour panelText { 0xffeeeeee };
const juce::Colour panelDim  { 0xff8b8e95 };
const juce::Colour panelLine { 0xff2c2e33 };
const juce::Colour okColour  { 0xffff8a3d };

void styleLabel (juce::Label& l, float size, bool bold, juce::Colour c)
{
    l.setFont (juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)));
    l.setColour (juce::Label::textColourId, c);
    l.setInterceptsMouseClicks (false, false);
    l.setJustificationType (juce::Justification::topLeft);
}
} // namespace

LinkPanel::LinkPanel (Controller& b, LinkClient& l) : box (b), link (l)
{
    styleLabel (title, 20.0f, true, panelText);
    title.setText ("Cat Box link", juce::dontSendNotification);
    styleLabel (status, 14.0f, false, panelDim);
    styleLabel (foundLabel, 13.0f, true, panelDim);
    foundLabel.setText ("Boxes on your network", juce::dontSendNotification);
    styleLabel (info, 13.0f, false, panelDim);
    info.setText ("The box and this PC must be on the same network (WiFi or cable). The box shows its address on "
                  "its PROJECT page.\n\nConnected, everything here plays the box and this PC stays silent. "
                  "Disconnect and Cat Box plays here again.", juce::dontSendNotification);

    address.setTextToShowWhenEmpty ("address, e.g. 192.168.1.23", panelDim);
    address.setFont (juce::Font (juce::FontOptions (14.0f)));
    address.onReturnKey = [this] { connectButton.triggerClick(); };
    connectButton.onClick = [this]
    {
        const auto a = address.getText().trim();
        if (a.isEmpty()) return;
        if (! link.connect (a)) status.setText ("No Cat Box answered at " + a + ". Is it on and on the same network?", juce::dontSendNotification);
        refresh();
    };
    disconnectButton.onClick = [this] { link.disconnect(); refresh(); };
    for (auto* c : { (juce::Component*) &title, (juce::Component*) &status, (juce::Component*) &foundLabel, (juce::Component*) &info,
                     (juce::Component*) &address, (juce::Component*) &connectButton, (juce::Component*) &disconnectButton })
        addAndMakeVisible (c);
    connectButton.setWantsKeyboardFocus (false);
    disconnectButton.setWantsKeyboardFocus (false);
    link.onChange = [this] { refresh(); };
    refresh();
    startTimerHz (2);
}

LinkPanel::~LinkPanel()
{
    stopTimer();
    link.onChange = nullptr;
}

void LinkPanel::paint (juce::Graphics& g)
{
    g.fillAll (panelBg);
    g.setColour (panelLine);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());
}

void LinkPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 16);
    title.setBounds (r.removeFromTop (30));
    status.setBounds (r.removeFromTop (54));
    r.removeFromTop (6);
    foundLabel.setBounds (r.removeFromTop (20));
    for (auto* b : foundButtons) b->setBounds (r.removeFromTop (34).reduced (0, 2));
    r.removeFromTop (8);
    address.setBounds (r.removeFromTop (30));
    r.removeFromTop (6);
    auto buttons = r.removeFromTop (30);
    connectButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).withTrimmedRight (4));
    disconnectButton.setBounds (buttons.withTrimmedLeft (4));
    r.removeFromTop (16);
    info.setBounds (r.removeFromTop (160));
}

void LinkPanel::refresh()
{
    if (link.isConnected())
    {
        status.setText ("Connected to the box at " + link.connectedTo() + ".", juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, okColour);
    }
    else if (! status.getText().startsWith ("No Cat Box"))
    {
        status.setText ("Not connected: Cat Box plays here on the PC.", juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, panelDim);
    }
    connectButton.setEnabled (! link.isConnected());
    disconnectButton.setEnabled (link.isConnected());

    const auto found = link.found();
    juce::String key;
    for (auto& f : found) key << f.address << ":" << f.port << ";";
    if (key != lastFoundKey)
    {
        lastFoundKey = key;
        foundButtons.clear();
        for (auto& f : found)
        {
            auto* b = foundButtons.add (new juce::TextButton ("Connect to " + f.name + "  (" + f.address + ")"));
            b->setWantsKeyboardFocus (false);
            const auto addr = f.address;
            const int port = f.port;
            b->onClick = [this, addr, port] { address.setText (addr); if (! link.connect (addr, port)) status.setText ("No Cat Box answered at " + addr + ".", juce::dontSendNotification); refresh(); };
            addAndMakeVisible (b);
        }
        foundLabel.setVisible (! found.empty());
        resized();
    }
    juce::ignoreUnused (box);
}
} // namespace catbox

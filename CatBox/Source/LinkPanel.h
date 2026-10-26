#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BoxLink.h"

namespace catbox
{
/**
    The PC app's side panel: find the box on the network and connect to it. Connected, the PC's
    knobs, buttons, pads and keys play the box and its screen shows the box.
*/
class LinkPanel : public juce::Component, private juce::Timer
{
public:
    LinkPanel (Controller&, LinkClient&);
    ~LinkPanel() override;

    static constexpr int panelWidth = 300;

    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String statusText() const { return status.getText(); }

private:
    void timerCallback() override { refresh(); }
    void refresh();

    Controller& box;
    LinkClient& link;
    juce::Label title, status, foundLabel, info;
    juce::OwnedArray<juce::TextButton> foundButtons;
    juce::TextEditor address;
    juce::TextButton connectButton { "Connect" }, disconnectButton { "Disconnect" };
    juce::String lastFoundKey;
};
} // namespace catbox

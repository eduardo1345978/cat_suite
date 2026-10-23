#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BoxLink.h"

namespace catbox
{
/**
    The PC app's side panel: find the box on the network and connect to it, and the preset
    library - every preset and scene with its name, on this PC and on the box: load, rename,
    and copy between them.
*/
class LinkPanel : public juce::Component, private juce::Timer
{
public:
    LinkPanel (BoxController&, LinkClient&);
    ~LinkPanel() override;

    static constexpr int panelWidth = 340;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Shows the presets of a page (0..3) or the scenes (-1). */
    void showPage (int pg) { pageBox.setSelectedItemIndex (pg < 0 ? NumPages : pg, juce::sendNotificationSync); }

    /** For the self test: what row i shows. */
    juce::String rowText (int i) const;

private:
    struct Row : public juce::Component
    {
        explicit Row (LinkPanel&, int index);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh();
        LinkPanel& owner;
        const int index;
        juce::Label boxName, pcName;
        juce::TextButton load { "Load" }, rename { "Name" }, toBox { "PC > Box" }, toPc { "Box > PC" };
    };

    void timerCallback() override;
    void refresh();
    int page() const;   // -1 = scenes
    void askName (int index);
    void copyToBox (int index);
    void copyToPc (int index);

    BoxController& box;
    LinkClient& link;
    juce::Label title, status, foundLabel, presetsLabel;
    juce::OwnedArray<juce::TextButton> foundButtons;
    juce::TextEditor address;
    juce::TextButton connectButton { "Connect" }, disconnectButton { "Disconnect" };
    juce::ComboBox pageBox;
    juce::Component rows;
    juce::Viewport rowsView;
    juce::OwnedArray<Row> rowItems;
    std::unique_ptr<juce::AlertWindow> nameWindow;
    juce::String lastFoundKey;
};
} // namespace catbox

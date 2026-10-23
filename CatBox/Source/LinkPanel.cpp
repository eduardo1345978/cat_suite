#include "LinkPanel.h"

namespace catbox
{
namespace
{
const juce::Colour panelBg   { 0xff15171d };
const juce::Colour panelText { 0xffe6e8ee };
const juce::Colour panelDim  { 0xff858b9b };
const juce::Colour panelLine { 0xff2a2e38 };
const juce::Colour okColour  { 0xff4fbf6a };

void styleLabel (juce::Label& l, float size, bool bold, juce::Colour c)
{
    l.setFont (juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)));
    l.setColour (juce::Label::textColourId, c);
    l.setInterceptsMouseClicks (false, false);
}
} // namespace

//==============================================================================
LinkPanel::Row::Row (LinkPanel& o, int i) : owner (o), index (i)
{
    styleLabel (boxName, 14.0f, true, panelText);
    styleLabel (pcName, 12.5f, false, panelDim);
    for (auto* c : { (juce::Component*) &boxName, (juce::Component*) &pcName, (juce::Component*) &load,
                     (juce::Component*) &rename, (juce::Component*) &toBox, (juce::Component*) &toPc })
        addAndMakeVisible (c);
    load.onClick   = [this] { owner.page() < 0 ? owner.box.loadScene (index) : owner.box.loadPreset (owner.page(), index); };
    rename.onClick = [this] { owner.askName (index); };
    toBox.onClick  = [this] { owner.copyToBox (index); };
    toPc.onClick   = [this] { owner.copyToPc (index); };
    for (auto* b : { &load, &rename, &toBox, &toPc }) b->setWantsKeyboardFocus (false);
}

void LinkPanel::Row::paint (juce::Graphics& g)
{
    g.setColour (panelLine);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    g.setColour (panelDim);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (juce::String (index + 1), 4, 4, 24, 20, juce::Justification::centredLeft);
}

void LinkPanel::Row::resized()
{
    auto r = getLocalBounds().reduced (4, 3);
    r.removeFromLeft (26);
    auto top = r.removeFromTop (20);
    boxName.setBounds (top);
    pcName.setBounds (r.removeFromTop (16));
    r.removeFromTop (2);
    const int w = r.getWidth() / 4;
    for (auto* b : { &load, &rename, &toBox, &toPc }) b->setBounds (r.removeFromLeft (w).reduced (2, 0));
}

void LinkPanel::Row::refresh()
{
    const int pg = owner.page();
    const bool linked = owner.box.isRemote();
    const auto local = owner.box.localPresetName (pg, index);
    const auto shown = owner.box.presetName (pg, index);   // the box's name while linked
    boxName.setText (shown.isNotEmpty() ? shown : juce::String ("empty"), juce::dontSendNotification);
    boxName.setColour (juce::Label::textColourId, shown.isNotEmpty() ? panelText : panelDim);
    pcName.setText (linked ? "on this PC: " + (local.isNotEmpty() ? local : juce::String ("empty")) : juce::String(), juce::dontSendNotification);
    load.setEnabled (shown.isNotEmpty());
    rename.setEnabled (shown.isNotEmpty());
    toBox.setEnabled (linked && local.isNotEmpty());
    toPc.setEnabled (linked && shown.isNotEmpty());
    toBox.setVisible (linked);
    toPc.setVisible (linked);
}

//==============================================================================
LinkPanel::LinkPanel (BoxController& b, LinkClient& l) : box (b), link (l)
{
    styleLabel (title, 20.0f, true, panelText);
    title.setText ("Cat Box link", juce::dontSendNotification);
    styleLabel (status, 13.5f, false, panelDim);
    styleLabel (foundLabel, 13.0f, true, panelDim);
    foundLabel.setText ("Boxes on your network", juce::dontSendNotification);
    styleLabel (presetsLabel, 16.0f, true, panelText);
    presetsLabel.setText ("Presets", juce::dontSendNotification);

    address.setTextToShowWhenEmpty ("address of the box, e.g. 192.168.1.23", panelDim);
    address.setFont (juce::Font (juce::FontOptions (14.0f)));
    address.onReturnKey = [this] { connectButton.triggerClick(); };
    connectButton.onClick = [this]
    {
        const auto a = address.getText().trim();
        if (a.isEmpty()) return;
        status.setText ("Connecting to " + a + " ...", juce::dontSendNotification);
        if (! link.connect (a)) status.setText ("No Cat Box answered at " + a + ". Is it on and on the same WiFi?", juce::dontSendNotification);
        refresh();
    };
    disconnectButton.onClick = [this] { link.disconnect(); refresh(); };

    pageBox.addItemList ({ "Drums", "Arp", "Bass", "FX", "Scenes" }, 1);
    pageBox.setSelectedItemIndex (0, juce::dontSendNotification);
    pageBox.onChange = [this] { refresh(); };

    for (auto* c : { (juce::Component*) &title, (juce::Component*) &status, (juce::Component*) &foundLabel,
                     (juce::Component*) &address, (juce::Component*) &connectButton, (juce::Component*) &disconnectButton,
                     (juce::Component*) &presetsLabel, (juce::Component*) &pageBox, (juce::Component*) &rowsView })
        addAndMakeVisible (c);
    for (auto* c : { (juce::Component*) &connectButton, (juce::Component*) &disconnectButton, (juce::Component*) &pageBox })
        c->setWantsKeyboardFocus (false);

    for (int i = 0; i < numPresetSlots; ++i) rows.addAndMakeVisible (rowItems.add (new Row (*this, i)));
    rowsView.setViewedComponent (&rows, false);
    rowsView.setScrollBarsShown (true, false);

    link.onChange = [this] { refresh(); };
    box.onNamesChanged = [this] { refresh(); };
    refresh();
    startTimerHz (2);
}

LinkPanel::~LinkPanel()
{
    stopTimer();
    link.onChange = nullptr;
    box.onNamesChanged = nullptr;
}

int LinkPanel::page() const
{
    const int i = pageBox.getSelectedItemIndex();
    return i >= NumPages || i < 0 ? -1 : i;
}

juce::String LinkPanel::rowText (int i) const
{
    return i >= 0 && i < rowItems.size() ? rowItems[i]->boxName.getText() : juce::String();
}

void LinkPanel::paint (juce::Graphics& g)
{
    g.fillAll (panelBg);
    g.setColour (panelLine);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());
}

void LinkPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 14);
    title.setBounds (r.removeFromTop (30));
    status.setBounds (r.removeFromTop (40));
    r.removeFromTop (6);
    foundLabel.setBounds (r.removeFromTop (20));
    for (auto* b : foundButtons) b->setBounds (r.removeFromTop (32).reduced (0, 2));
    r.removeFromTop (6);
    address.setBounds (r.removeFromTop (30));
    r.removeFromTop (6);
    auto buttons = r.removeFromTop (30);
    connectButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (0, 0).withTrimmedRight (4));
    disconnectButton.setBounds (buttons.withTrimmedLeft (4));
    r.removeFromTop (18);
    auto head = r.removeFromTop (30);
    presetsLabel.setBounds (head.removeFromLeft (90));
    pageBox.setBounds (head);
    r.removeFromTop (8);
    rowsView.setBounds (r);
    const int rowH = 66;
    rows.setSize (rowsView.getMaximumVisibleWidth(), rowH * numPresetSlots);
    for (int i = 0; i < rowItems.size(); ++i) rowItems[i]->setBounds (0, i * rowH, rows.getWidth(), rowH);
}

void LinkPanel::refresh()
{
    // connection
    if (link.isConnected())
    {
        status.setText ("Connected to the box at " + link.connectedTo() + ". The knobs, pads and buttons here play the box.",
                        juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, okColour);
    }
    else
    {
        status.setText ("Not connected: Cat Box plays here on the PC.", juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, panelDim);
    }
    connectButton.setEnabled (! link.isConnected());
    disconnectButton.setEnabled (link.isConnected());

    // boxes found on the network
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
            b->onClick = [this, addr, port] { address.setText (addr); if (! link.connect (addr, port)) status.setText ("The box did not answer.", juce::dontSendNotification); refresh(); };
            addAndMakeVisible (b);
        }
        foundLabel.setText (found.empty() ? "Boxes on your network: none found yet" : "Boxes on your network", juce::dontSendNotification);
        resized();
    }
    for (auto* b : foundButtons) b->setEnabled (! link.isConnected());

    presetsLabel.setText (link.isConnected() ? "On the box" : "Presets", juce::dontSendNotification);
    for (auto* row : rowItems) row->refresh();
}

void LinkPanel::timerCallback()
{
    refresh();
}

void LinkPanel::askName (int index)
{
    const int pg = page();
    nameWindow = std::make_unique<juce::AlertWindow> ("Name", "A name for " + juce::String (pg < 0 ? "scene " : "preset ") + juce::String (index + 1),
                                                      juce::MessageBoxIconType::NoIcon);
    nameWindow->addTextEditor ("name", box.presetName (pg, index));
    nameWindow->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<LinkPanel> safe (this);
    nameWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safe, pg, index] (int result)
    {
        if (safe == nullptr || safe->nameWindow == nullptr) return;
        const auto text = safe->nameWindow->getTextEditorContents ("name");
        if (result == 1) safe->box.renamePreset (pg, index, text);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->nameWindow.reset(); });
        safe->refresh();
    }), false);
}

void LinkPanel::copyToBox (int index)
{
    const int pg = page();
    const auto json = box.localPresetJson (pg, index);
    if (! json.isObject()) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("t", "putPreset");
    o->setProperty ("page", pg);
    o->setProperty ("slot", index);
    o->setProperty ("data", json);
    link.send (juce::var (o));
}

void LinkPanel::copyToPc (int index)
{
    const int pg = page();
    juce::Component::SafePointer<LinkPanel> safe (this);
    link.requestPreset (pg, index, [safe, pg, index] (const juce::var& json)
    {
        if (safe == nullptr) return;
        if (json.isObject() && safe->box.storeLocalPresetJson (pg, index, json))
            safe->status.setText ("Copied to this PC: " + safe->box.localPresetName (pg, index), juce::dontSendNotification);
        safe->refresh();
    });
}

} // namespace catbox

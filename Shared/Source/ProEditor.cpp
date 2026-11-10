#include "ProEditor.h"

#ifndef JucePlugin_Name
 #define JucePlugin_Name "Cat"
#endif

using namespace juce;

namespace cat
{
namespace
{
const Colour bg (0xff15171a), panel (0xff1d2024), panel2 (0xff23272c), edge (0xff2f343b), ink (0xffe6e9ee), dim (0xff8d949e),
             faint (0xff5d646e), accent (0xff4fd1c5), warm (0xffffb454), hot (0xffff5f5f);

Font mono (float h) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::plain)); }
Font sans (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }

String db (float g) { return g < 1.0e-5f ? String ("-inf") : String (Decibels::gainToDecibels (g), 1); }
}

void ProStats::measure (const AudioBuffer<float>& b, bool input) noexcept
{
    const int n = b.getNumSamples(), chans = jmin (2, b.getNumChannels());
    if (n <= 0 || chans <= 0) return;
    for (int ch = 0; ch < chans; ++ch)
    {
        const float pk = b.getMagnitude (ch, 0, n);
        if (input)
        {
            auto& p = ch == 0 ? inPeakL : inPeakR;
            p = jmax (pk, p.load() * 0.93f);
        }
        else
        {
            const float rms = b.getRMSLevel (ch, 0, n);
            auto& p = ch == 0 ? outPeakL : outPeakR;
            auto& r = ch == 0 ? outRmsL : outRmsR;
            p = jmax (pk, p.load() * 0.93f);
            r = r.load() * 0.8f + rms * 0.2f;
        }
    }
    if (chans == 1 && ! input) { outPeakR = outPeakL.load(); outRmsR = outRmsL.load(); }
}

//==============================================================================
class ProEditor::Laf : public LookAndFeel_V4
{
public:
    Laf()
    {
        setColour (ResizableWindow::backgroundColourId, bg);
        setColour (TextButton::buttonColourId, panel2);
        setColour (TextButton::buttonOnColourId, accent.withAlpha (0.35f));
        setColour (TextButton::textColourOffId, ink);
        setColour (TextButton::textColourOnId, ink);
        setColour (ComboBox::backgroundColourId, panel2);
        setColour (ComboBox::outlineColourId, edge);
        setColour (ComboBox::textColourId, ink);
        setColour (ComboBox::arrowColourId, dim);
        setColour (PopupMenu::backgroundColourId, panel);
        setColour (PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.3f));
        setColour (PopupMenu::textColourId, ink);
        setColour (TextEditor::backgroundColourId, panel2);
        setColour (TextEditor::outlineColourId, edge);
        setColour (TextEditor::focusedOutlineColourId, accent);
        setColour (TextEditor::textColourId, ink);
        setColour (Label::textColourId, ink);
        setColour (Label::textWhenEditingColourId, ink);
        setColour (Label::backgroundWhenEditingColourId, panel2);
        setColour (Label::outlineWhenEditingColourId, accent);
        setColour (ToggleButton::textColourId, dim);
        setColour (ToggleButton::tickColourId, accent);
        setColour (ToggleButton::tickDisabledColourId, faint);
        setColour (Slider::trackColourId, accent.withAlpha (0.55f));
        setColour (Slider::backgroundColourId, panel2);
        setColour (Slider::thumbColourId, ink);
        setColour (ScrollBar::thumbColourId, edge);
    }

    void drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider& s) override
    {
        const auto r = Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 3.0f, (float) w, 6.0f);
        g.setColour (panel2);
        g.fillRoundedRectangle (r, 3.0f);
        // filled from the start, or from the middle for -x .. +x ranges
        const double mid = s.getMinimum() < 0.0 && s.getMaximum() > 0.0 ? (double) s.getPositionOfValue (0.0) : (double) x;
        const float a = (float) jmin (mid, (double) pos), b = (float) jmax (mid, (double) pos);
        g.setColour (s.findColour (Slider::trackColourId));
        g.fillRoundedRectangle (Rectangle<float> (a, r.getY(), jmax (2.0f, b - a), r.getHeight()), 3.0f);
        g.setColour (s.isMouseOverOrDragging() ? Colours::white : ink);
        g.fillRoundedRectangle (pos - 3.0f, r.getY() - 4.0f, 6.0f, 14.0f, 2.0f);
    }
};

//==============================================================================
/** One parameter: group, name, its control, the exact value (click to type), a reset dot. */
class ProEditor::Row : public Component
{
public:
    Row (AudioProcessorParameter& p, const String& groupName) : param (p), group (groupName)
    {
        ranged = dynamic_cast<RangedAudioParameter*> (&p);
        const auto values = p.getAllValueStrings();
        if (p.isBoolean() && ranged != nullptr)
        {
            toggle = std::make_unique<ToggleButton>();
            addAndMakeVisible (*toggle);
            buttonAttachment = std::make_unique<ButtonParameterAttachment> (*ranged, *toggle);
        }
        else if (p.isDiscrete() && values.size() > 1 && values.size() <= 64 && ranged != nullptr)
        {
            combo = std::make_unique<ComboBox>();
            combo->addItemList (values, 1);
            addAndMakeVisible (*combo);
            comboAttachment = std::make_unique<ComboBoxParameterAttachment> (*ranged, *combo);
        }
        else if (ranged != nullptr)
        {
            slider = std::make_unique<Slider> (Slider::LinearHorizontal, Slider::NoTextBox);
            slider->setScrollWheelEnabled (true);
            slider->setDoubleClickReturnValue (true, (double) ranged->convertFrom0to1 (ranged->getDefaultValue()));
            addAndMakeVisible (*slider);
            sliderAttachment = std::make_unique<SliderParameterAttachment> (*ranged, *slider);
        }
        value.setEditable (false, true, false);
        value.setJustificationType (Justification::centredRight);
        value.setFont (mono (12.5f));
        value.setTooltip ("Double-click to type a value");
        value.onTextChange = [this]
        {
            const float v = param.getValueForText (value.getText());
            param.beginChangeGesture();
            param.setValueNotifyingHost (jlimit (0.0f, 1.0f, v));
            param.endChangeGesture();
            refresh();
        };
        addAndMakeVisible (value);
        reset.setButtonText (String::fromUTF8 ("\xe2\x86\xba"));
        reset.setTooltip ("Back to the default");
        reset.onClick = [this] { setToDefault(); };
        addAndMakeVisible (reset);
        refresh();
    }

    bool changed() const { return std::abs (param.getValue() - param.getDefaultValue()) > 1.0e-4f; }
    void setToDefault()
    {
        param.beginChangeGesture();
        param.setValueNotifyingHost (param.getDefaultValue());
        param.endChangeGesture();
        refresh();
    }
    String searchText() const { return (group + " " + param.getName (64) + " " + (ranged != nullptr ? ranged->getParameterID() : String())).toLowerCase(); }

    void refresh()
    {
        if (! value.isBeingEdited())
        {
            auto t = param.getCurrentValueAsText();
            const auto unit = param.getLabel();
            if (unit.isNotEmpty() && ! t.endsWith (unit)) t << " " << unit;
            value.setText (t, dontSendNotification);
        }
        const bool c = changed();
        if (c != wasChanged) { wasChanged = c; repaint(); }
        reset.setVisible (c);
    }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds();
        if (wasChanged) { g.setColour (accent.withAlpha (0.06f)); g.fillRect (r); }
        g.setColour (edge.withAlpha (0.6f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        g.setColour (wasChanged ? accent : faint);
        g.fillEllipse (8.0f, (float) getHeight() * 0.5f - 3.0f, 6.0f, 6.0f);
        r.removeFromLeft (22);
        if (group.isNotEmpty())
        {
            g.setColour (faint);
            g.setFont (sans (11.0f));
            g.drawText (group, r.removeFromLeft (110), Justification::centredLeft, true);
        }
        g.setColour (ink);
        g.setFont (sans (13.5f));
        g.drawText (param.getName (64), r.removeFromLeft (220), Justification::centredLeft, true);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 4);
        reset.setBounds (r.removeFromRight (26));
        r.removeFromRight (6);
        value.setBounds (r.removeFromRight (130));
        r.removeFromRight (10);
        auto control = r.removeFromRight (jmin (300, jmax (120, r.getWidth() - 340)));
        if (slider != nullptr) slider->setBounds (control);
        if (combo != nullptr) combo->setBounds (control.withSizeKeepingCentre (control.getWidth(), 24));
        if (toggle != nullptr) toggle->setBounds (control.withWidth (40));
    }

    AudioProcessorParameter& param;
    RangedAudioParameter* ranged = nullptr;
    String group;

private:
    std::unique_ptr<Slider> slider;
    std::unique_ptr<ComboBox> combo;
    std::unique_ptr<ToggleButton> toggle;
    std::unique_ptr<SliderParameterAttachment> sliderAttachment;
    std::unique_ptr<ComboBoxParameterAttachment> comboAttachment;
    std::unique_ptr<ButtonParameterAttachment> buttonAttachment;
    Label value;
    TextButton reset;
    bool wasChanged = false;
};

//==============================================================================
ProEditor::ProEditor (AudioProcessor& p, ProStats& s) : AudioProcessorEditor (p), proc (p), stats (s), laf (std::make_unique<Laf>())
{
    setLookAndFeel (laf.get());

    // the groups (from the parameter tree, when the plug-in has them)
    std::function<void (const AudioProcessorParameterGroup&, const String&)> walk = [&] (const AudioProcessorParameterGroup& gr, const String& prefix)
    {
        for (auto* node : gr)
        {
            if (auto* sub = node->getGroup()) walk (*sub, prefix.isEmpty() ? sub->getName() : prefix + " / " + sub->getName());
            else if (auto* prm = node->getParameter()) groupNames[prm] = prefix;
        }
    };
    walk (p.getParameterTree(), {});

    StringArray groupList;
    for (auto* prm : p.getParameters())
    {
        auto* row = rows.add (new Row (*prm, groupOf (prm)));
        list.addAndMakeVisible (row);
        if (row->group.isNotEmpty()) groupList.addIfNotAlreadyThere (row->group);
    }
    groups.addItem ("All groups", 1);
    groups.addItemList (groupList, 2);
    groups.setSelectedId (1, dontSendNotification);
    groups.onChange = [this] { rebuildList(); };
    groups.setVisible (groupList.size() > 1);
    addChildComponent (groups);

    // presets
    if (p.getNumPrograms() > 1)
    {
        for (int i = 0; i < p.getNumPrograms(); ++i) programs.addItem (String (i + 1) + "  " + p.getProgramName (i), i + 1);
        programs.setSelectedId (p.getCurrentProgram() + 1, dontSendNotification);
        programs.onChange = [this]
        {
            snapshot();
            proc.setCurrentProgram (programs.getSelectedId() - 1);
        };
        prevB.onClick = [this] { programs.setSelectedId (jmax (1, programs.getSelectedId() - 1)); };
        nextB.onClick = [this] { programs.setSelectedId (jmin (programs.getNumItems(), programs.getSelectedId() + 1)); };
        for (auto* c : std::initializer_list<Component*> { &programs, &prevB, &nextB }) addAndMakeVisible (c);
    }

    aB.setClickingTogglesState (false);
    bB.setClickingTogglesState (false);
    aB.setToggleState (true, dontSendNotification);
    aB.onClick = [this] { switchAB (0); };
    bB.onClick = [this] { switchAB (1); };
    copyAB.onClick = [this]
    {
        proc.getStateInformation (slots[slot]);
        slots[1 - slot] = slots[slot];
        copyAB.setButtonText (slot == 0 ? "A > B" : "B > A");
    };
    undoB.onClick = [this]
    {
        if (undoStack.empty()) return;
        auto m = undoStack.back();
        undoStack.pop_back();
        restore (m);
    };
    resetB.onClick = [this]
    {
        snapshot();
        for (auto* r : rows) r->setToDefault();
    };
    copyB.onClick = [this]
    {
        MemoryBlock m;
        proc.getStateInformation (m);
        SystemClipboard::copyTextToClipboard ("CATSUITE:" + String (JucePlugin_Name) + ":" + m.toBase64Encoding());
    };
    pasteB.onClick = [this]
    {
        const auto text = SystemClipboard::getTextFromClipboard();
        const String prefix = "CATSUITE:" + String (JucePlugin_Name) + ":";
        if (! text.startsWith (prefix)) return;
        MemoryBlock m;
        if (! m.fromBase64Encoding (text.substring (prefix.length()))) return;
        snapshot();
        restore (m);
    };
    aB.setTooltip ("Two settings to compare: A and B");
    copyAB.setTooltip ("Copy this setting to the other slot");
    undoB.setTooltip ("Undo the last reset / paste / preset / A-B change");
    copyB.setTooltip ("Copy the whole setting (to paste into another copy of this plug-in)");
    for (auto* b : { &aB, &bB, &copyAB, &undoB, &resetB, &copyB, &pasteB }) addAndMakeVisible (b);

    search.setTextToShowWhenEmpty ("Search parameters ...", faint);
    search.setIndents (10, 7);
    search.onTextChange = [this] { rebuildList(); };
    addAndMakeVisible (search);
    changedOnly.onClick = [this] { rebuildList(); };
    addAndMakeVisible (changedOnly);

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setResizable (true, true);
    setResizeLimits (780, 520, 2000, 1500);
    setSize (960, 700);
    rebuildList();
    startTimerHz (20);
}

ProEditor::~ProEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

String ProEditor::groupOf (const AudioProcessorParameter* p) const
{
    const auto it = groupNames.find (p);
    return it != groupNames.end() ? it->second : String();
}

void ProEditor::snapshot()
{
    MemoryBlock m;
    proc.getStateInformation (m);
    undoStack.push_back (m);
    if (undoStack.size() > 30) undoStack.erase (undoStack.begin());
}

void ProEditor::restore (const MemoryBlock& m)
{
    proc.setStateInformation (m.getData(), (int) m.getSize());
    proc.updateHostDisplay (AudioProcessor::ChangeDetails().withParameterInfoChanged (true));
    for (auto* r : rows) r->refresh();
}

void ProEditor::switchAB (int s)
{
    if (s == slot) return;
    snapshot();
    proc.getStateInformation (slots[slot]);
    slot = s;
    if (slots[slot].isEmpty()) slots[slot] = slots[1 - slot];
    else restore (slots[slot]);
    aB.setToggleState (slot == 0, dontSendNotification);
    bB.setToggleState (slot == 1, dontSendNotification);
    copyAB.setButtonText (slot == 0 ? "A > B" : "B > A");
}

void ProEditor::rebuildList()
{
    const auto q = search.getText().trim().toLowerCase();
    const auto g = groups.getSelectedId() > 1 ? groups.getText() : String();
    int y = 0;
    const int w = jmax (300, viewport.getWidth() - viewport.getScrollBarThickness());
    for (auto* r : rows)
    {
        const bool show = (q.isEmpty() || r->searchText().contains (q)) && (g.isEmpty() || r->group == g) && (! changedOnly.getToggleState() || r->changed());
        r->setVisible (show);
        if (show) { r->setBounds (0, y, w, 34); y += 34; }
    }
    list.setSize (w, jmax (y, 1));
}

void ProEditor::paint (Graphics& g)
{
    g.fillAll (bg);
    // title
    g.setColour (ink);
    g.setFont (sans (20.0f, true));
    const String name (JucePlugin_Name);
    const auto base = name.endsWith (" Pro") ? name.dropLastCharacters (4) : name;
    g.drawText (base, 16, 12, 260, 30, Justification::centredLeft);
    const int tw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), base);
    g.setColour (accent);
    g.fillRoundedRectangle ((float) (16 + tw + 10), 18.0f, 38.0f, 18.0f, 4.0f);
    g.setColour (bg);
    g.setFont (sans (11.0f, true));
    g.drawText ("PRO", 16 + tw + 10, 18, 38, 18, Justification::centred);

    // the numbers of the moment
    auto s = statsArea;
    g.setColour (panel);
    g.fillRoundedRectangle (s.toFloat(), 8.0f);
    s.reduce (12, 8);
    struct Box { String title, big, small; float meter; Colour c; };
    const float cpu = stats.cpu.load() * 100.0f, peakCpu = stats.cpuPeak.load() * 100.0f;
    const double sr = stats.sampleRate.load();
    const int lat = proc.getLatencySamples();
    const float oL = stats.outPeakL.load(), oR = stats.outPeakR.load();
    std::vector<Box> boxes {
        { "CPU", String (cpu, 1) + " %", "peak " + String (peakCpu, 1) + " %", jlimit (0.0f, 1.0f, cpu / 100.0f), cpu > 50.0f ? hot : (cpu > 20.0f ? warm : accent) },
        { "BLOCK", String (stats.blockMs.load(), 2) + " ms", String (stats.blockSize.load()) + " samples", -1.0f, accent },
        { "SAMPLE RATE", sr > 0.0 ? String (sr / 1000.0, 1) + " kHz" : String ("-"), "latency " + String (lat) + (sr > 0.0 ? " (" + String (lat * 1000.0 / sr, 1) + " ms)" : String()), -1.0f, accent },
        { "OUT PEAK L / R", db (oL) + " / " + db (oR), "RMS " + db (stats.outRmsL.load()) + " / " + db (stats.outRmsR.load()) + " dB",
          jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (jmax (oL, oR), -60.0f) + 60.0f) / 60.0f), jmax (oL, oR) > 0.99f ? hot : accent },
    };
    if (proc.getTotalNumInputChannels() > 0)
        boxes.push_back ({ "IN PEAK L / R", db (stats.inPeakL.load()) + " / " + db (stats.inPeakR.load()), "dB", -1.0f, accent });
    if (proc.acceptsMidi() || proc.producesMidi())
        boxes.push_back ({ "MIDI IN / OUT", String (roundToInt (midiInRate)) + " / " + String (roundToInt (midiOutRate)), "events a second", -1.0f, accent });
    boxes.push_back ({ "PARAMETERS", String (changedCount) + " / " + String (rows.size()), "changed / all", -1.0f, accent });
    const int bw = s.getWidth() / (int) boxes.size();
    for (size_t i = 0; i < boxes.size(); ++i)
    {
        auto b = s.removeFromLeft (bw).reduced (6, 0);
        const auto& x = boxes[i];
        g.setColour (faint);
        g.setFont (sans (10.5f, true));
        g.drawText (x.title, b.removeFromTop (14), Justification::centredLeft);
        g.setColour (ink);
        g.setFont (mono (16.0f));
        g.drawText (x.big, b.removeFromTop (22), Justification::centredLeft);
        g.setColour (dim);
        g.setFont (mono (11.0f));
        g.drawText (x.small, b.removeFromTop (14), Justification::centredLeft);
        if (x.meter >= 0.0f)
        {
            g.setColour (panel2);
            g.fillRoundedRectangle (b.removeFromTop (5).toFloat(), 2.0f);
            g.setColour (x.c);
            g.fillRoundedRectangle (Rectangle<float> ((float) b.getX(), (float) b.getY() - 5.0f, (float) b.getWidth() * x.meter, 5.0f), 2.0f);
        }
    }

    // column titles
    g.setColour (faint);
    g.setFont (sans (11.0f, true));
    const auto vp = viewport.getBounds();
    g.drawText ("PARAMETER", vp.getX() + 22, vp.getY() - 18, 200, 16, Justification::centredLeft);
    g.drawText ("VALUE", vp.getRight() - 200, vp.getY() - 18, 150, 16, Justification::centredRight);
}

void ProEditor::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    auto top = r.removeFromTop (36);
    top.removeFromLeft (260);
    for (auto* b : { &pasteB, &copyB, &resetB, &undoB }) { b->setBounds (top.removeFromRight (b == &resetB ? 80 : 62).reduced (2, 3)); }
    top.removeFromRight (10);
    copyAB.setBounds (top.removeFromRight (62).reduced (2, 3));
    bB.setBounds (top.removeFromRight (32).reduced (2, 3));
    aB.setBounds (top.removeFromRight (32).reduced (2, 3));
    top.removeFromRight (10);
    if (programs.isVisible())
    {
        nextB.setBounds (top.removeFromRight (28).reduced (2, 3));
        programs.setBounds (top.removeFromRight (jmin (260, top.getWidth() - 34)).reduced (2, 3));
        prevB.setBounds (top.removeFromRight (28).reduced (2, 3));
    }
    r.removeFromTop (8);
    statsArea = r.removeFromTop (76);
    r.removeFromTop (10);
    auto filter = r.removeFromTop (30);
    search.setBounds (filter.removeFromLeft (260));
    filter.removeFromLeft (10);
    if (groups.isVisible()) { groups.setBounds (filter.removeFromLeft (200)); filter.removeFromLeft (10); }
    changedOnly.setBounds (filter.removeFromLeft (140));
    r.removeFromTop (24);
    viewport.setBounds (r);
    rebuildList();
}

void ProEditor::timerCallback()
{
    int changed = 0;
    for (auto* r : rows)
    {
        if (r->isVisible()) r->refresh();
        changed += r->changed() ? 1 : 0;
    }
    if (changed != changedCount && changedOnly.getToggleState()) { changedCount = changed; rebuildList(); }
    changedCount = changed;

    const auto now = Time::getMillisecondCounter();
    if (lastTick != 0 && now - lastTick >= 500)
    {
        const float secs = (float) (now - lastTick) / 1000.0f;
        midiInRate = (float) (stats.midiIn.load() - lastMidiIn) / secs;
        midiOutRate = (float) (stats.midiOut.load() - lastMidiOut) / secs;
        lastMidiIn = stats.midiIn.load();
        lastMidiOut = stats.midiOut.load();
        lastTick = now;
    }
    else if (lastTick == 0) { lastTick = now; lastMidiIn = stats.midiIn.load(); lastMidiOut = stats.midiOut.load(); }
    if (programs.isVisible() && programs.getSelectedId() != proc.getCurrentProgram() + 1)
        programs.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    repaint (statsArea);
}
} // namespace cat

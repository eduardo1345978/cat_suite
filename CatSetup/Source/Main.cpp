// Cat Suite Setup: install, update and remove the Cats (see Setup.h).
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_cryptography/juce_cryptography.h>
#include "Setup.h"
#include "CatFonts.h"

using namespace juce;
using namespace setup;

namespace
{
const Colour bg0 (0xff12101c), bg1 (0xff1b1830), cardBg (0xff221f38), cardHover (0xff2a2646), line (0xff35304f),
             ink (0xfff1eefb), inkDim (0xff9d97b8), violet (0xff7c5cff), teal (0xff2ad6c8), okGreen (0xff4fd68a), orange (0xffffa94d), red (0xffff6b6b);

Font uiFont (float h, bool bold = false)
{
    static Typeface::Ptr regular = Typeface::createSystemTypefaceFor (CatFonts::SoraBold_ttf, (size_t) CatFonts::SoraBold_ttfSize);
    if (bold && regular != nullptr) return Font (FontOptions (regular).withHeight (h));
    return Font (FontOptions (h));
}

String initials (const Item& i)
{
    const auto n = i.name.replace ("Cat ", "").replace ("-Cat", "").replace (" app", "").replace (" library", "");
    return n.substring (0, 2).toUpperCase();
}

/** What a Cat is, for the sections and the filter chips. */
enum Group { Midi = 0, Instrument, Effect, Apps, Extras, numGroups };
const char* groupTitle (int g) { static const char* t[] { "MIDI CATS", "INSTRUMENTS", "EFFECTS", "APPS", "EXTRAS" }; return t[g]; }
const char* groupNote (int g)
{
    static const char* t[] { "they write notes for your synths", "they make their own sound", "for audio tracks", "programs on their own, no DAW needed", "" };
    return t[g];
}
const char* groupChip (int g) { static const char* t[] { "MIDI", "Instruments", "Effects", "Apps", "Extras" }; return t[g]; }
int groupOf (const Item& i)
{
    if (i.kind == Kind::App) return Apps;
    if (i.kind == Kind::Extra) return Extras;
    static const StringArray fx { "Echo-Cat", "Molten", "Swarm", "Vortex" };
    static const StringArray inst { "Sing-Cat", "Glide-Cat", "Talk-Cat", "Life-Cat", "Muse-Cat", "Glitch-Cat", "Negative" };
    if (fx.contains (i.id)) return Effect;
    if (inst.contains (i.id)) return Instrument;
    return Midi;
}
String exeName (const Item& i) { return i.id.fromFirstOccurrenceOf ("app:", false, false) + ".exe"; }

/** "2026-11-05 15:20 UTC" -> "today 16:20" / "yesterday 9:05" / "5 Nov 16:20" (local time). */
String friendlyBuilt (const String& built)
{
    const auto parts = StringArray::fromTokens (built, " -:", {});
    if (parts.size() < 5) return built;
    const Time utc (parts[0].getIntValue(), parts[1].getIntValue() - 1, parts[2].getIntValue(), parts[3].getIntValue(), parts[4].getIntValue(), 0, 0, false);
    const auto now = Time::getCurrentTime();
    const auto clock = utc.formatted ("%H:%M");
    const auto day = [] (Time x) { return x.formatted ("%Y-%m-%d"); };
    if (day (utc) == day (now)) return "today " + clock;
    if (day (utc) == day (now - RelativeTime::days (1))) return "yesterday " + clock;
    return utc.formatted ("%d %b ") + clock;
}

/** One Cat: icon, name, what it is, its state, and a tick. Click = pick / unpick. Installed apps get "Open". */
class Card : public Component, public SettableTooltipClient
{
public:
    Card (const Item& i, const Paths& p) : item (i), paths (p)
    {
        setRepaintsOnMouseActivity (true);
        setMouseCursor (MouseCursor::PointingHandCursor);
        setTooltip (i.name + ": " + i.blurb);
    }
    std::function<void()> onToggle;
    bool selected = false;
    State state = State::NotInstalled;
    bool busy = false;
    enum class Work { none, waiting, working, done, failed } work = Work::none;

    bool canOpen() const
    {
       #if JUCE_WINDOWS
        return item.kind == Kind::App && state != State::NotInstalled && paths.apps.getChildFile (exeName (item)).existsAsFile();
       #else
        return false;
       #endif
    }
    Rectangle<float> openArea() const { return getLocalBounds().toFloat().reduced (5.0f).reduced (14.0f, 12.0f).removeFromBottom (24.0f).removeFromRight (64.0f); }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (5.0f);
        g.setColour (isMouseOver() && ! busy ? cardHover : cardBg);
        g.fillRoundedRectangle (r, 14.0f);
        if (selected)
        {
            g.setGradientFill (ColourGradient (violet.withAlpha (0.14f), r.getX(), r.getY(), teal.withAlpha (0.05f), r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 14.0f);
        }
        g.setColour (selected ? violet : line);
        g.drawRoundedRectangle (r, 14.0f, selected ? 2.0f : 1.0f);

        // the icon: the Cat's own colour, two letters, cat ears
        auto icon = r.reduced (14.0f).removeFromLeft (52.0f).withSizeKeepingCentre (52.0f, 52.0f);
        const Colour c (item.colour);
        {
            Path ears;
            ears.addTriangle (icon.getX() + 6.0f, icon.getY() + 10.0f, icon.getX() + 10.0f, icon.getY() - 6.0f, icon.getX() + 22.0f, icon.getY() + 4.0f);
            ears.addTriangle (icon.getRight() - 6.0f, icon.getY() + 10.0f, icon.getRight() - 10.0f, icon.getY() - 6.0f, icon.getRight() - 22.0f, icon.getY() + 4.0f);
            g.setColour (c.darker (0.2f));
            g.fillPath (ears);
        }
        g.setGradientFill (ColourGradient (c.brighter (0.25f), icon.getX(), icon.getY(), c.darker (0.45f), icon.getRight(), icon.getBottom(), false));
        g.fillRoundedRectangle (icon, 14.0f);
        g.setColour (c.getPerceivedBrightness() > 0.6f ? Colour (0xff1a1626) : Colours::white);
        g.setFont (uiFont (19.0f, true));
        g.drawText (initials (item), icon, Justification::centred);
        if (item.kind == Kind::App)
        {
            auto tag = Rectangle<float> (26.0f, 14.0f).withCentre ({ icon.getCentreX(), icon.getBottom() - 1.0f });
            g.setColour (Colour (0xff1a1626));
            g.fillRoundedRectangle (tag, 7.0f);
            g.setColour (ink);
            g.setFont (uiFont (9.5f, true));
            g.drawText ("APP", tag, Justification::centred);
        }

        auto text = r.reduced (14.0f, 12.0f).withTrimmedLeft (66.0f);
        auto top = text.removeFromTop (24.0f);
        auto tick = top.removeFromRight (22.0f).withSizeKeepingCentre (20.0f, 20.0f);
        g.setColour (selected ? violet : Colours::transparentBlack);
        g.fillRoundedRectangle (tick, 6.0f);
        g.setColour (selected ? violet : inkDim.withAlpha (0.6f));
        g.drawRoundedRectangle (tick, 6.0f, 1.5f);
        if (selected)
        {
            Path p;
            p.startNewSubPath (tick.getX() + 5.0f, tick.getCentreY());
            p.lineTo (tick.getX() + 8.5f, tick.getBottom() - 5.0f);
            p.lineTo (tick.getRight() - 4.5f, tick.getY() + 5.0f);
            g.setColour (Colours::white);
            g.strokePath (p, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (ink);
        g.setFont (uiFont (16.0f, true));
        g.drawText (item.name, top, Justification::centredLeft, true);
        g.setColour (inkDim);
        g.setFont (uiFont (13.0f));
        g.drawFittedText (item.blurb, text.removeFromTop (34.0f).toNearestInt(), Justification::topLeft, 2);

        // the state (or what Setup is doing with it right now)
        String badge; Colour bc;
        switch (state)
        {
            case State::Installed:    badge = "Installed";        bc = okGreen; break;
            case State::Update:       badge = "Update ready";     bc = orange;  break;
            case State::NotInstalled: badge = "Not installed";    bc = inkDim;  break;
        }
        switch (work)
        {
            case Work::waiting: badge = "Waiting";  bc = inkDim; break;
            case Work::working: badge = "Working" + String ("...").substring (0, 1 + (int) (Time::getMillisecondCounter() / 300) % 3); bc = teal; break;
            case Work::done:    badge = String::fromUTF8 ("\xe2\x9c\x93 Done"); bc = okGreen; break;
            case Work::failed:  badge = "Didn't work"; bc = red; break;
            case Work::none: break;
        }
        g.setFont (uiFont (12.0f, true));
        const float bw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), badge) + 18.0f;
        auto b = text.removeFromBottom (20.0f).withWidth (bw);
        g.setColour (bc.withAlpha (0.16f));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (bc);
        g.drawText (badge, b, Justification::centred);

        if (canOpen() && work == Work::none)
        {
            const auto o = openArea();
            const bool over = isMouseOver() && o.contains (getMouseXYRelative().toFloat());
            g.setColour (over ? teal.withAlpha (0.3f) : teal.withAlpha (0.14f));
            g.fillRoundedRectangle (o, 12.0f);
            g.setColour (teal);
            g.setFont (uiFont (12.0f, true));
            g.drawText (String::fromUTF8 ("Open \xe2\x86\x97"), o, Justification::centred);
        }
    }

    void mouseUp (const MouseEvent& e) override
    {
        if (busy || ! getLocalBounds().contains (e.getPosition())) return;
        if (canOpen() && openArea().contains (e.position)) { paths.apps.getChildFile (exeName (item)).startAsProcess(); return; }
        if (onToggle) onToggle();
    }
    void mouseMove (const MouseEvent&) override { if (canOpen()) repaint(); }

    const Item& item;
    const Paths& paths;
};

/** A big rounded button. */
class Pill : public TextButton
{
public:
    Pill (const String& t, bool primary_, float fontH = 0.0f) : TextButton (t), primary (primary_), fh (fontH) {}
    void paintButton (Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float c = r.getHeight() * 0.5f;
        if (primary)
        {
            g.setGradientFill (ColourGradient (violet, r.getX(), r.getY(), teal, r.getRight(), r.getBottom(), false));
            g.setOpacity (isEnabled() ? (down ? 0.85f : 1.0f) : 0.35f);
            g.fillRoundedRectangle (r, c);
            if (over && isEnabled()) { g.setColour (Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle (r, c); }
        }
        else
        {
            g.setColour (getToggleState() ? violet.withAlpha (0.28f) : (over && isEnabled() ? cardHover : cardBg).withMultipliedAlpha (isEnabled() ? 1.0f : 0.5f));
            g.fillRoundedRectangle (r, c);
            g.setColour (getToggleState() ? violet : line);
            g.drawRoundedRectangle (r, c, 1.0f);
        }
        g.setColour (isEnabled() ? ink : inkDim.withAlpha (0.5f));
        g.setFont (uiFont (fh > 0.0f ? fh : (primary ? 16.0f : 14.0f), primary));
        g.drawText (getButtonText(), getLocalBounds(), Justification::centred);
    }
    bool primary;
    float fh;
};

/** A quiet text link ("Pick all"). */
class Link : public TextButton
{
public:
    explicit Link (const String& t) : TextButton (t) { setMouseCursor (MouseCursor::PointingHandCursor); }
    void paintButton (Graphics& g, bool over, bool) override
    {
        g.setColour (isEnabled() ? (over ? ink : violet.brighter (0.4f)) : inkDim.withAlpha (0.4f));
        g.setFont (uiFont (12.5f, true));
        g.drawText (getButtonText(), getLocalBounds(), Justification::centredRight);
    }
};
}

//==============================================================================
class SetupComponent : public Component, private Timer
{
public:
    explicit SetupComponent (Paths p, bool offline = false) : paths (std::move (p))
    {
        for (auto& i : catalog())
        {
            auto* c = cards.add (new Card (i, paths));
            c->onToggle = [this, c] { c->selected = ! c->selected; c->repaint(); updateButtons(); };
            list.addAndMakeVisible (c);
        }
        for (int gr = 0; gr < numGroups; ++gr)
        {
            auto* l = sectionLinks.add (new Link ("Pick all"));
            l->onClick = [this, gr]
            {
                bool all = true;
                for (auto* c : cards) if (groupOf (c->item) == gr && c->isVisible()) all &= c->selected;
                for (auto* c : cards) if (groupOf (c->item) == gr && c->isVisible()) c->selected = ! all;
                repaintCards();
            };
            list.addAndMakeVisible (l);
        }
        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        viewport.getVerticalScrollBar().setColour (ScrollBar::thumbColourId, line);
        addAndMakeVisible (viewport);

        // filter chips: All, MIDI, Instruments, Effects, Apps
        for (int k = -1; k < numGroups - 1; ++k)
        {
            auto* chip = chips.add (new Pill (k < 0 ? "All" : groupChip (k), false, 13.0f));
            chip->setClickingTogglesState (false);
            chip->setToggleState (k < 0, dontSendNotification);
            chip->onClick = [this, k] { filter = k; for (int j = 0; j < chips.size(); ++j) chips[j]->setToggleState (j - 1 == k, dontSendNotification); layoutCards(); repaint(); };
            addAndMakeVisible (chip);
        }
        search.setTextToShowWhenEmpty ("Search the Cats ...", inkDim.withAlpha (0.7f));
        search.setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
        search.setColour (TextEditor::outlineColourId, Colours::transparentBlack);
        search.setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
        search.onFocusLost = [this] { repaint(); };
        search.setColour (TextEditor::textColourId, ink);
        search.setFont (uiFont (14.0f));
        search.setIndents (18, 10);
        search.onTextChange = [this] { layoutCards(); repaint(); };
        search.onEscapeKey = [this] { search.clear(); layoutCards(); repaint(); };
        addAndMakeVisible (search);

        for (auto* b : { &installButton, &removeButton, &retryButton, &cubaseButton, &hubButton, &loopButton })
            addAndMakeVisible (b);
        for (auto* b : { &allLink, &updatesLink, &noneLink }) addAndMakeVisible (b);
        allLink.onClick = [this] { for (auto* c : cards) c->selected = c->item.kind != Kind::Extra || c->selected; repaintCards(); };
        updatesLink.onClick = [this] { for (auto* c : cards) c->selected = c->state == State::Update; repaintCards(); };
        noneLink.onClick = [this] { for (auto* c : cards) c->selected = false; repaintCards(); };
        installButton.onClick = [this] { start (Job::Install); };
        removeButton.onClick = [this]
        {
            int n = 0;
            for (auto* c : cards) n += c->selected && c->state != State::NotInstalled && c->item.kind != Kind::Extra;
            AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Remove " + String (n) + (n == 1 ? " Cat?" : " Cats?"),
                                          "They are deleted from this computer. Projects that use them will miss them (you can install them again any time).",
                                          "Remove", "Cancel", this, ModalCallbackFunction::create ([this] (int ok) { if (ok != 0) start (Job::Uninstall); }));
        };
        retryButton.onClick = [this] { check(); };
        cubaseButton.onClick = [this] { if (cubase.existsAsFile()) cubase.startAsProcess(); };
        hubButton.onClick = [this] { paths.apps.getChildFile ("Cat-Hub.exe").startAsProcess(); };
        loopButton.onClick = [this] { paths.apps.getChildFile ("Cat-Loop.exe").startAsProcess(); };

        loginToggle.setButtonText ("Check for updates when Windows starts");
        loginToggle.setColour (ToggleButton::textColourId, inkDim);
        loginToggle.setColour (ToggleButton::tickColourId, violet);
        loginToggle.setToggleState (checksAtLogin(), dontSendNotification);
        loginToggle.onClick = [this] { setCheckAtLogin (loginToggle.getToggleState(), paths.apps.getChildFile (setupExe)); };
       #if JUCE_WINDOWS
        addAndMakeVisible (loginToggle);
       #endif

        cubase = findCubase();
        setSize (1060, 780);
        if (! offline) check();
        startTimerHz (15);
    }

    ~SetupComponent() override { if (job != nullptr) job->stopThread (10000); }

    /** For tests / pictures: show this release without asking GitHub. */
    void showRelease (const Release& r) { release = r; checking = false; refreshStates (true); }
    Job* currentJob() { return job.get(); }
    void showFinished (int count) { finishedCount = count; finishedOk = true; finishedInstall = true; lastProgress = 1.0f; statusText = {}; updateButtons(); repaint(); }

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (bg1, 0.0f, 0.0f, bg0, 0.0f, (float) getHeight(), false));
        g.fillAll();
        g.setGradientFill (ColourGradient (violet.withAlpha (0.25f), 160.0f, 40.0f, violet.withAlpha (0.0f), 560.0f, 260.0f, true));
        g.fillRect (getLocalBounds());

        auto head = getLocalBounds().reduced (32, 24).removeFromTop (78);
        head.removeFromRight (300);                                  // the search box
        auto logo = head.removeFromLeft (64).withSizeKeepingCentre (60, 60).toFloat();
        g.setGradientFill (ColourGradient (violet, logo.getX(), logo.getY(), teal, logo.getRight(), logo.getBottom(), false));
        g.fillRoundedRectangle (logo, 16.0f);
        g.setColour (Colours::white);
        {
            Path cat;
            const auto c = logo.getCentre().translated (0.0f, 3.0f);
            cat.addEllipse (c.x - 15.0f, c.y - 12.0f, 30.0f, 26.0f);
            cat.addTriangle (c.x - 15.0f, c.y - 3.0f, c.x - 13.0f, c.y - 22.0f, c.x - 4.0f, c.y - 10.0f);
            cat.addTriangle (c.x + 15.0f, c.y - 3.0f, c.x + 13.0f, c.y - 22.0f, c.x + 4.0f, c.y - 10.0f);
            g.fillPath (cat);
            g.setColour (Colour (0xff463ca0));
            g.fillEllipse (c.x - 8.0f, c.y - 2.0f, 5.0f, 6.0f);
            g.fillEllipse (c.x + 3.0f, c.y - 2.0f, 5.0f, 6.0f);
        }
        head.removeFromLeft (16);
        g.setColour (ink);
        g.setFont (uiFont (32.0f, true));
        g.drawText ("Cat Suite", head.removeFromTop (44), Justification::bottomLeft);
        // the headline + a coloured chip (updates / up to date)
        auto line2 = head.removeFromTop (26);
        auto [text, chip, chipColour] = headline();
        g.setColour (inkDim);
        g.setFont (uiFont (14.5f));
        const int tw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text);
        g.drawText (text, line2, Justification::centredLeft);
        if (chip.isNotEmpty())
        {
            g.setFont (uiFont (12.0f, true));
            const float cw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), chip) + 20.0f;
            const Rectangle<float> cr ((float) line2.getX() + (float) tw + 12.0f, (float) line2.getCentreY() - 11.0f, cw, 22.0f);
            g.setColour (chipColour.withAlpha (0.18f));
            g.fillRoundedRectangle (cr, 11.0f);
            g.setColour (chipColour);
            g.drawText (chip, cr, Justification::centred);
        }

        // the search box's rounded frame, and "Select:" before the links
        {
            const auto sb = search.getBounds().toFloat().expanded (1.0f);
            g.setColour (cardBg);
            g.fillRoundedRectangle (sb, sb.getHeight() * 0.5f);
            g.setColour (search.hasKeyboardFocus (true) ? violet : line);
            g.drawRoundedRectangle (sb, sb.getHeight() * 0.5f, 1.0f);
            g.setColour (inkDim);
            g.setFont (uiFont (12.5f));
            g.drawText ("Select:", allLink.getX() - 60, allLink.getY(), 54, allLink.getHeight(), Justification::centredRight);
        }

        // section titles (they scroll with the list)
        g.saveState();
        g.reduceClipRegion (viewport.getBounds());
        for (auto& s : sectionTitles)
        {
            const int x = viewport.getX() + 8, y = viewport.getY() + s.y - viewport.getViewPositionY();
            g.setFont (uiFont (12.5f, true));
            g.setColour (ink.withAlpha (0.85f));
            g.drawText (s.title, x, y, 300, 22, Justification::centredLeft);
            int tx = x + GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s.title) + 10;
            const String n (s.count);
            g.setFont (uiFont (11.0f, true));
            const int nw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), n) + 14;
            g.setColour (line);
            g.fillRoundedRectangle ((float) tx, (float) y + 3.0f, (float) nw, 16.0f, 8.0f);
            g.setColour (inkDim);
            g.drawText (n, tx, y + 3, nw, 16, Justification::centred);
            tx += nw + 12;
            g.setFont (uiFont (13.0f));
            g.drawText (s.note, tx, y, 500, 22, Justification::centredLeft);
        }
        if (sectionTitles.empty())
        {
            g.setColour (inkDim);
            g.setFont (uiFont (15.0f));
            g.drawText ("No Cat is called \"" + search.getText() + "\"", viewport.getBounds().withHeight (80), Justification::centred);
        }
        g.restoreState();

        // the footer: progress and what is happening, or "all set"
        g.setColour (Colour (0xff17142a));
        g.fillRect (footer);
        g.setColour (line);
        g.drawHorizontalLine (footer.getY(), 0.0f, (float) getWidth());
        auto bar = statusArea.toFloat().removeFromBottom (8.0f);
        if (finishedOk && job == nullptr)
        {
            auto s = statusArea;
            g.setColour (okGreen);
            g.setFont (uiFont (21.0f, true));
            g.drawText (String::fromUTF8 ("\xe2\x9c\x93  ") + (finishedInstall ? "All set" : "Removed") + String::fromUTF8 (" \xe2\x80\x94 ")
                        + String (finishedCount) + (finishedCount == 1 ? " item" : " items"), s.removeFromTop (30), Justification::centredLeft);
            g.setColour (inkDim);
            g.setFont (uiFont (13.5f));
            g.drawFittedText (finishedInstall ? "Open your DAW and rescan the plug-ins (Cubase: Studio > VST Plug-in Manager > Rescan). The apps are in the Start menu."
                                              : "They are gone from this computer.", s, Justification::topLeft, 2, 1.0f);
            return;
        }
        if (job != nullptr || lastProgress > 0.0f)
        {
            g.setColour (line);
            g.fillRoundedRectangle (bar, 4.0f);
            g.setGradientFill (ColourGradient (violet, bar.getX(), 0.0f, teal, bar.getRight(), 0.0f, false));
            g.fillRoundedRectangle (bar.withWidth (jmax (8.0f, bar.getWidth() * lastProgress)), 4.0f);
        }
        g.setColour (statusColour);
        g.setFont (uiFont (14.0f));
        g.drawFittedText (statusText.isEmpty() ? selectionSummary() : statusText, statusArea.withTrimmedBottom (14), Justification::centredLeft, 2, 1.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        footer = r.removeFromBottom (128);
        search.setBounds (getWidth() - 32 - 280, 46, 280, 38);
        r.removeFromTop (116);
        auto row = r.removeFromTop (44).reduced (32, 5);
        for (auto* c : chips)
        {
            const int w = GlyphArrangement::getStringWidthInt (uiFont (13.0f), c->getButtonText()) + 34;
            c->setBounds (row.removeFromLeft (w));
            row.removeFromLeft (8);
        }
        noneLink.setBounds (row.removeFromRight (40));
        row.removeFromRight (14);
        updatesLink.setBounds (row.removeFromRight (62));
        row.removeFromRight (14);
        allLink.setBounds (row.removeFromRight (24));
        retryButton.setBounds (getWidth() - 32 - 140, 96, 140, 30);
        viewport.setBounds (r.reduced (26, 6));
        layoutCards();

        auto f = footer.reduced (32, 18);
        auto buttons = f.removeFromRight (440);
        auto row1 = buttons.removeFromTop (48);
        installButton.setBounds (row1.removeFromRight (230));
        row1.removeFromRight (10);
        removeButton.setBounds (row1);
        buttons.removeFromTop (10);
        auto row2 = buttons.removeFromTop (36);
        for (auto* b : { &cubaseButton, &hubButton, &loopButton })
            if (b->isVisible()) { b->setBounds (row2.removeFromRight (136)); row2.removeFromRight (8); }
        f.removeFromRight (24);
        statusArea = f.removeFromTop (62);
        loginToggle.setBounds (f.removeFromTop (30));
    }

private:
    std::tuple<String, String, Colour> headline() const
    {
        if (checking) return { "Looking for the newest Cats on GitHub ...", {}, inkDim };
        if (! release.ok) return { "Can't see the newest build", "offline?", orange };
        int updates = 0, installed = 0;
        for (auto* c : cards) { updates += c->state == State::Update; installed += c->state != State::NotInstalled; }
        const String s = "Newest build: " + friendlyBuilt (release.built);
        if (installed == 0) return { s, "pick what you want", violet.brighter (0.4f) };
        if (updates > 0) return { s, String (updates) + (updates == 1 ? " update ready" : " updates ready"), orange };
        return { s, String::fromUTF8 ("\xe2\x9c\x93 everything is up to date"), okGreen };
    }

    String selectionSummary() const
    {
        int picked = 0;
        for (auto* c : cards) picked += c->selected;
        if (! release.ok) return {};
        if (picked == 0) return "Click the Cats you want (they get a tick), then press Install.";
        return String (picked) + (picked == 1 ? " Cat picked." : " Cats picked.") + "  Plug-ins go to your VST3 folder, apps to the Start menu. No password needed.";
    }

    bool matches (const Card& c) const
    {
        const auto q = search.getText().trim();
        if (filter >= 0 && groupOf (c.item) != filter) return false;
        return q.isEmpty() || c.item.name.containsIgnoreCase (q) || c.item.blurb.containsIgnoreCase (q);
    }

    void layoutCards()
    {
        const int w = viewport.getWidth() - 14;
        const int cols = jmax (1, w / 330), cw = w / cols, ch = 104;
        int y = 0;
        sectionTitles.clear();
        for (int gr = 0; gr < numGroups; ++gr)
        {
            int k = 0;
            for (auto* c : cards)
                if (groupOf (c->item) == gr) c->setVisible (matches (*c));
            for (auto* c : cards) k += groupOf (c->item) == gr && c->isVisible();
            sectionLinks[gr]->setVisible (k > 1);
            if (k == 0) continue;
            sectionTitles.push_back ({ y + 4, groupTitle (gr), groupNote (gr), k });
            sectionLinks[gr]->setBounds (w - 140, y + 4, 132, 22);
            y += 30;
            k = 0;
            for (auto* c : cards)
            {
                if (groupOf (c->item) != gr || ! c->isVisible()) continue;
                c->setBounds ((k % cols) * cw, y + (k / cols) * ch, cw, ch);
                ++k;
            }
            y += ((k + cols - 1) / cols) * ch + 14;
        }
        list.setSize (w, jmax (y, 10));
    }

    void repaintCards()
    {
        for (int gr = 0; gr < numGroups; ++gr)
        {
            bool all = true;
            for (auto* c : cards) if (groupOf (c->item) == gr && c->isVisible()) all &= c->selected;
            sectionLinks[gr]->setButtonText (all ? "Unpick all" : "Pick all");
        }
        for (auto* c : cards) c->repaint();
        updateButtons();
        repaint (footer);
    }

    void check()
    {
        checking = true;
        retryButton.setVisible (false);
        statusText = "Asking GitHub for the newest build ...";
        statusColour = inkDim;
        repaint();
        SafePointer<SetupComponent> safe (this);
        Thread::launch ([safe]
        {
            auto r = fetchRelease();
            MessageManager::callAsync ([safe, r]
            {
                if (safe == nullptr) return;
                safe->release = r;
                safe->checking = false;
                safe->refreshStates (true);
            });
        });
    }

    void refreshStates (bool pickDefaults)
    {
        const auto state = Installed::load (paths.stateFile);
        bool any = false;
        for (auto* c : cards)
        {
            c->state = stateOf (c->item, state, release, paths);
            any |= c->state != State::NotInstalled && c->item.kind != Kind::Extra;
        }
        if (pickDefaults)
            for (auto* c : cards)
                c->selected = any ? c->state == State::Update                                       // updating: what's out of date
                                  : (c->item.kind == Kind::Plugin || c->item.id == "app:Cat-Hub" || c->item.id == "app:Cat-Loop");  // first time
        retryButton.setVisible (! release.ok && ! checking);
        if (! release.ok && ! checking) { statusText = "Can't reach GitHub: " + release.error + "  -  are you online?"; statusColour = orange; }
        else if (job == nullptr && statusText.startsWith ("Asking")) { statusText = {}; statusColour = inkDim; }
        repaintCards();
        repaint();
    }

    void updateButtons()
    {
        int install = 0, update = 0, remove = 0;
        for (auto* c : cards)
            if (c->selected)
            {
                if (c->state == State::NotInstalled) ++install;
                else if (c->state == State::Update) ++update;
                if (c->state != State::NotInstalled && c->item.kind != Kind::Extra) ++remove;
            }
        const bool idle = job == nullptr;
        String text = install > 0 && update > 0 ? "Install & update " + String (install + update)
                    : update > 0 ? "Update " + String (update)
                    : install > 0 ? "Install " + String (install)
                    : String ("Install");
        int reinstall = 0;
        for (auto* c : cards) reinstall += c->selected && c->state == State::Installed && c->item.kind != Kind::Extra;
        if (install + update == 0 && reinstall > 0) text = "Reinstall " + String (reinstall);
        if (! idle) text = "Working ...";
        installButton.setButtonText (text);
        installButton.setEnabled (idle && release.ok && install + update + reinstall > 0);
        removeButton.setButtonText (remove > 0 ? "Remove " + String (remove) : String ("Remove"));
        removeButton.setEnabled (idle && remove > 0);
        for (auto* b : { &allLink, &updatesLink, &noneLink }) b->setEnabled (idle);
        for (auto* l : sectionLinks) l->setEnabled (idle);
        search.setEnabled (idle);
        const bool wasCubase = cubaseButton.isVisible(), wasHub = hubButton.isVisible(), wasLoop = loopButton.isVisible();
        cubaseButton.setVisible (cubase.existsAsFile());
       #if JUCE_WINDOWS
        hubButton.setVisible (paths.apps.getChildFile ("Cat-Hub.exe").existsAsFile());
        loopButton.setVisible (paths.apps.getChildFile ("Cat-Loop.exe").existsAsFile());
       #else
        hubButton.setVisible (false);
        loopButton.setVisible (finishedOk);
       #endif
        if (wasCubase != cubaseButton.isVisible() || wasHub != hubButton.isVisible() || wasLoop != loopButton.isVisible()) resized();
        for (auto* c : cards) c->busy = ! idle;
    }

    void start (Job::Action a)
    {
        std::vector<String> ids;
        for (auto* c : cards)
            if (c->selected && (a == Job::Install || (c->state != State::NotInstalled && c->item.kind != Kind::Extra)))
                ids.push_back (c->item.id);
        if (ids.empty()) return;
        for (auto* c : cards) c->work = std::find (ids.begin(), ids.end(), c->item.id) != ids.end() ? Card::Work::waiting : Card::Work::none;
        job = std::make_unique<Job> (a, ids, release, paths);
        job->localSource = localSource;
        lastProgress = 0.0f;
        finishedOk = false;
        finishedInstall = a == Job::Install;
        statusColour = ink;
        job->startThread();
        updateButtons();
    }

    void timerCallback() override
    {
        if (job == nullptr) return;
        lastProgress = job->progress.load();
        statusText = job->getStatus();
        statusColour = job->waitingForClose.load() ? orange : ink;
        const auto current = job->getCurrent();
        const auto doneSoFar = job->getDone();
        for (auto* c : cards)
        {
            if (c->work == Card::Work::none) continue;
            if (doneSoFar.contains (c->item.id)) c->work = Card::Work::done;
            else if (c->item.id == current) c->work = Card::Work::working;
            c->repaint();
        }
        if (job->finished.load())
        {
            const bool bad = job->failed.load();
            const int count = job->getDone().size();
            for (auto* c : cards)
                if (c->work == Card::Work::waiting || c->work == Card::Work::working) c->work = bad ? Card::Work::failed : Card::Work::done;
            statusColour = bad ? orange : okGreen;
            job.reset();
            refreshStates (false);
            for (auto* c : cards) if (c->state == State::Installed) c->selected = false;
            if (! bad) { lastProgress = 1.0f; finishedOk = true; finishedCount = count; statusText = {}; }
            // the badges stay a moment, then the cards show their state again
            Timer::callAfterDelay (4000, [safe = SafePointer<SetupComponent> (this)] { if (safe != nullptr && safe->job == nullptr) { for (auto* c : safe->cards) { c->work = Card::Work::none; c->repaint(); } } });
        }
        updateButtons();
        repaint (footer);
    }

    static File findCubase()
    {
       #if JUCE_WINDOWS
        const File steinberg ("C:\\Program Files\\Steinberg");
        File best;
        for (auto& dir : steinberg.findChildFiles (File::findDirectories, false, "Cubase*"))
            for (auto& exe : dir.findChildFiles (File::findFiles, false, "Cubase*.exe"))
                if (! exe.getFileName().containsIgnoreCase ("uninst") && (best == File() || exe.getLastModificationTime() > best.getLastModificationTime()))
                    best = exe;
        return best;
       #else
        return {};
       #endif
    }

public:
    File localSource;     // (tests)

private:
    Paths paths;
    Release release;
    bool checking = false;
    OwnedArray<Card> cards;
    OwnedArray<Link> sectionLinks;
    OwnedArray<Pill> chips;
    int filter = -1;
    TextEditor search;
    Component list;
    Viewport viewport;
    struct Section { int y; String title, note; int count; };
    std::vector<Section> sectionTitles;
    Pill installButton { "Install", true }, removeButton { "Remove", false }, retryButton { "Try again", false, 13.0f },
         cubaseButton { "Open Cubase", false, 13.0f }, hubButton { "Start Cat Hub", false, 13.0f }, loopButton { "Start Cat Loop", false, 13.0f };
    Link allLink { "All" }, updatesLink { "Updates" }, noneLink { "None" };
    ToggleButton loginToggle;
    TooltipWindow tooltips { this, 600 };
    std::unique_ptr<Job> job;
    String statusText;
    Colour statusColour = inkDim;
    float lastProgress = 0.0f;
    bool finishedOk = false, finishedInstall = true;
    int finishedCount = 0;
    Rectangle<int> footer, statusArea;
    File cubase;
};

//==============================================================================
/** Started with Windows (--check): says nothing unless there is an update, then a small note. */
class UpdateNote : public Component
{
public:
    UpdateNote (int count, std::function<void()> open, std::function<void()> later) : n (count)
    {
        addAndMakeVisible (openButton);
        addAndMakeVisible (laterButton);
        openButton.onClick = std::move (open);
        laterButton.onClick = std::move (later);
        setSize (440, 170);
    }
    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (bg1, 0.0f, 0.0f, bg0, 0.0f, (float) getHeight(), false));
        g.fillAll();
        g.setColour (ink);
        g.setFont (uiFont (20.0f, true));
        g.drawText ("New Cats are ready", 24, 18, getWidth() - 48, 30, Justification::centredLeft);
        g.setColour (inkDim);
        g.setFont (uiFont (14.0f));
        g.drawText (String (n) + (n == 1 ? " of your Cats has" : " of your Cats have") + " an update.", 24, 50, getWidth() - 48, 24, Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (24).removeFromBottom (44);
        openButton.setBounds (r.removeFromRight (190));
        r.removeFromRight (10);
        laterButton.setBounds (r.removeFromRight (110));
    }
private:
    int n;
    Pill openButton { "Update now", true }, laterButton { "Later", false };
};

//==============================================================================
static int selfTest (const File& dir);

class SetupApp : public JUCEApplication
{
public:
    const String getApplicationName() override { return "Cat Suite Setup"; }
    const String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const String& commandLine) override
    {
        auto args = StringArray::fromTokens (commandLine, true);
        args.removeEmptyStrings();
        for (auto& a : args) a = a.unquoted();
        if (args.contains ("--selftest"))
        {
            const int i = args.indexOf ("--selftest");
            setApplicationReturnValue (selfTest (File (args[i + 1].isNotEmpty() ? args[i + 1] : File::getSpecialLocation (File::tempDirectory).getChildFile ("cat-setup-test").getFullPathName())));
            quit();
            return;
        }
        if (args.contains ("--check"))
        {
            // once a day, quietly
            const auto paths = Paths::forUser();
            auto state = Installed::load (paths.stateFile);
            const auto today = Time::getCurrentTime().formatted ("%Y-%m-%d");
            if (state.lastCheckDay == today || state.items.empty()) { quit(); return; }
            state.lastCheckDay = today;
            state.save (paths.stateFile);
            Thread::launch ([this, paths]
            {
                const auto r = fetchRelease();
                MessageManager::callAsync ([this, r, paths]
                {
                    int updates = 0;
                    const auto st = Installed::load (paths.stateFile);
                    for (auto& i : catalog()) updates += stateOf (i, st, r, paths) == State::Update;
                    if (updates == 0) { quit(); return; }
                    window = std::make_unique<Window> ("Cat Suite", new UpdateNote (updates, [this] { window.reset(); openMain(); }, [this] { quit(); }), false);
                });
            });
            return;
        }
        openMain();
    }

    void openMain()
    {
        auto* c = new SetupComponent (Paths::forUser());
        window = std::make_unique<Window> ("Cat Suite Setup", c, true);
    }

    void shutdown() override { window = nullptr; }
    void systemRequestedQuit() override { quit(); }

    struct Window : public DocumentWindow
    {
        Window (const String& name, Component* content, bool canResize) : DocumentWindow (name, bg0, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            setResizable (canResize, false);
            if (canResize) setResizeLimits (760, 600, 2400, 1600);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

private:
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (SetupApp)

//==============================================================================
// --selftest <folder>: a fake release (two zips like the real ones) installed into <folder>/root,
// then updated, then removed; GitHub's answer parsed; a broken download refused. Also draws the window.
static int selfTest (const File& dir)
{
    int fails = 0;
    auto out = dir.getChildFile ("setup-test.txt");
    String log;
    auto check = [&] (bool ok, const String& what) { log << (ok ? "ok   " : "FAIL ") << what << "\n"; if (! ok) ++fails; std::printf ("%s %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8()); };
    dir.deleteRecursively();
    dir.createDirectory();

    // the zips
    const auto src = dir.getChildFile ("release");
    {
        const auto stage = dir.getChildFile ("stage-suite");
        for (auto n : { "Glitch-Cat", "Hum-Cat", "Micro-Cat" })
        {
            stage.getChildFile ("VST3").getChildFile (String (n) + ".vst3").getChildFile ("Contents/x86_64-win").createDirectory();
            stage.getChildFile ("VST3").getChildFile (String (n) + ".vst3").getChildFile ("Contents/x86_64-win/" + String (n) + ".vst3").replaceWithText ("plug-in " + String (n));
        }
        stage.getChildFile ("THIRD-PARTY-NOTICES.txt").replaceWithText ("notices");
        stage.getChildFile ("licenses/MIT-VST3-SDK.txt").create();
        ZipFile::Builder b;
        for (auto& f : stage.findChildFiles (File::findFiles, true))
            b.addFile (f, 9, f.getRelativePathFrom (stage).replaceCharacter ('\\', '/'));
        src.createDirectory();
        FileOutputStream os (src.getChildFile (suiteZip));
        b.writeToStream (os, nullptr);
    }
    {
        const auto stage = dir.getChildFile ("stage-apps");
        stage.getChildFile ("Apps").createDirectory();
        stage.getChildFile ("Apps/Cat-Hub.exe").replaceWithText ("hub");
        stage.getChildFile ("Hub Cats/Chord-Cat.vst3").createDirectory();
        stage.getChildFile ("Hub Cats/Chord-Cat.vst3/x.txt").replaceWithText ("chord");
        ZipFile::Builder b;
        for (auto& f : stage.findChildFiles (File::findFiles, true))
            b.addFile (f, 9, f.getRelativePathFrom (stage).replaceCharacter ('\\', '/'));
        FileOutputStream os (src.getChildFile (appsZip));
        b.writeToStream (os, nullptr);
    }

    // GitHub's answer
    const char* json = R"J({"name":"Cat Suite - latest build (2026-11-05 15:20 UTC)","target_commitish":"main","published_at":"2026-10-21T20:26:12Z",
        "body":"text<br><br>Built from commit cf7f91eeec0bd5183fd440da33bbb78e2d6a8cad.",
        "assets":[{"name":"Cat-Suite-Windows.zip","size":100,"digest":"sha256:9B04","browser_download_url":"https://github.com/eduardo1345978/cat_suite/releases/download/latest/Cat-Suite-Windows.zip"},
                  {"name":"Cat-Sound-Apps-Windows.zip","size":200,"browser_download_url":"https://github.com/eduardo1345978/cat_suite/releases/download/latest/Cat-Sound-Apps-Windows.zip"},
                  {"name":"Evil.zip","size":1,"browser_download_url":"https://evil.example/x.zip"}]})J";
    auto rel = parseRelease (JSON::parse (String (json)));
    check (rel.ok && rel.commit == "cf7f91eeec0bd5183fd440da33bbb78e2d6a8cad" && rel.built == "2026-11-05 15:20 UTC", "reads the release: commit and date");
    check (rel.find (suiteZip) != nullptr && rel.find (suiteZip)->sha256 == "9b04" && rel.find ("Evil.zip") == nullptr, "takes files only from github.com, with their SHA-256");
    check (! parseRelease (JSON::parse (String ("{\"message\":\"Not Found\"}"))).ok, "a GitHub error is not a release");

    const auto paths = Paths::under (dir.getChildFile ("root"));
    auto runJob = [&] (Job::Action a, std::vector<String> ids, const Release& r)
    {
        Job job (a, std::move (ids), r, paths);
        job.localSource = src;
        job.startThread();
        while (! job.finished.load()) Thread::sleep (20);
        job.stopThread (1000);
        return std::make_pair (! job.failed.load(), job.getStatus());
    };

    // install
    const auto [ok1, st1] = runJob (Job::Install, { "Glitch-Cat", "Hum-Cat", "app:Cat-Hub" }, rel);
    check (ok1, "installs: " + st1);
    check (paths.vst3.getChildFile ("Glitch-Cat.vst3/Contents/x86_64-win/Glitch-Cat.vst3").loadFileAsString() == "plug-in Glitch-Cat", "the VST3 is in the VST3 folder");
    check (paths.apps.getChildFile ("Cat-Hub.exe").existsAsFile() && paths.apps.getChildFile ("Hub Cats/Chord-Cat.vst3/x.txt").existsAsFile(), "the Hub and its own Cats are in the apps folder");
    check (! paths.vst3.getChildFile ("Micro-Cat.vst3").exists(), "only what was picked");
    check (paths.apps.getChildFile ("THIRD-PARTY-NOTICES.txt").existsAsFile() && paths.apps.getChildFile ("licenses/MIT-VST3-SDK.txt").existsAsFile(),
           "the license files are installed with the programs");
    auto state = Installed::load (paths.stateFile);
    check (state.items["Glitch-Cat"] == rel.commit && state.items.size() == 3, "remembers what it installed and which build");
    check (stateOf (*findItem ("Glitch-Cat"), state, rel, paths) == State::Installed && stateOf (*findItem ("Micro-Cat"), state, rel, paths) == State::NotInstalled,
           "knows what is installed");

    // a newer build
    auto rel2 = rel;
    rel2.commit = "d2deb5e000000000000000000000000000000000";
    check (stateOf (*findItem ("Glitch-Cat"), state, rel2, paths) == State::Update, "a newer build: Update ready");
    paths.vst3.getChildFile ("Glitch-Cat.vst3/Contents/stale.txt").replaceWithText ("old file");
    const auto [ok2, st2] = runJob (Job::Install, { "Glitch-Cat" }, rel2);
    state = Installed::load (paths.stateFile);
    check (ok2 && state.items["Glitch-Cat"] == rel2.commit && state.items["Hum-Cat"] == rel.commit, "updates one Cat (the others keep their build)");
    check (! paths.vst3.getChildFile ("Glitch-Cat.vst3/Contents/stale.txt").exists(), "an update leaves no old files behind");

    // installed by the old .bat (no record): seen, and offered as an update
    paths.vst3.getChildFile ("Drum-Cat.vst3").createDirectory();
    check (stateOf (*findItem ("Drum-Cat"), state, rel2, paths) == State::Update, "a Cat from the old installer is seen and can be updated");

    // remove
    const auto [ok3, st3] = runJob (Job::Uninstall, { "Hum-Cat", "app:Cat-Hub" }, rel2);
    state = Installed::load (paths.stateFile);
    check (ok3 && ! paths.vst3.getChildFile ("Hum-Cat.vst3").exists() && ! paths.apps.getChildFile ("Cat-Hub.exe").exists()
               && ! paths.apps.getChildFile ("Hub Cats").exists() && state.items.count ("Hum-Cat") == 0 && state.items.count ("Glitch-Cat") == 1,
           "removes what was picked, keeps the rest");

    // a download that doesn't match GitHub's checksum is thrown away, nothing changes
    {
        Release bad = rel2;
        bad.commit = "eeee000000000000000000000000000000000000";
        bad.assets.clear();
        Release::Asset a;
        a.name = suiteZip;
        a.url = URL (src.getChildFile (suiteZip)).toString (false);
        a.size = src.getChildFile (suiteZip).getSize();
        a.sha256 = "00000000000000000000000000000000000000000000000000000000000000ff";
        bad.assets.push_back (a);
        Job job (Job::Install, { "Micro-Cat" }, bad, paths);
        job.startThread();
        while (! job.finished.load()) Thread::sleep (20);
        check (job.failed.load() && job.getStatus().contains ("checksum") && ! paths.vst3.getChildFile ("Micro-Cat.vst3").exists(),
               "a download with the wrong checksum is refused (" + job.getStatus() + ")");
        // and with the right one it goes in
        Release good = bad;
        good.assets[0].sha256 = SHA256 (src.getChildFile (suiteZip)).toHexString().toLowerCase();
        Job job2 (Job::Install, { "Micro-Cat" }, good, paths);
        job2.startThread();
        while (! job2.finished.load()) Thread::sleep (20);
        check (! job2.failed.load() && paths.vst3.getChildFile ("Micro-Cat.vst3").exists(), "the same download with the right checksum installs (" + job2.getStatus() + ")");
    }

    // a zip that tries to write outside its folder is refused
    {
        ZipFile::Builder b;
        const auto f = dir.getChildFile ("evil.txt");
        f.replaceWithText ("x");
        b.addFile (f, 9, "../../evil.txt");
        const auto evilDir = dir.getChildFile ("evil");
        evilDir.createDirectory();
        FileOutputStream os (evilDir.getChildFile (suiteZip));
        b.writeToStream (os, nullptr);
        os.flush();
        Job job (Job::Install, { "Glitch-Cat" }, rel, paths);
        job.localSource = evilDir;
        job.startThread();
        while (! job.finished.load()) Thread::sleep (20);
        check (job.failed.load() && ! dir.getChildFile ("root/evil.txt").exists() && ! dir.getParentDirectory().getChildFile ("evil.txt").exists(),
               "a zip with ../ paths is refused");
    }

    // the window, as it looks with this release (a picture to check)
    {
        auto c = std::make_unique<SetupComponent> (paths, true);
        c->showRelease (rel2);
        c->setSize (1060, 780);
        auto img = c->createComponentSnapshot (c->getLocalBounds());
        PNGImageFormat png;
        {
            FileOutputStream os (dir.getChildFile ("setup.png"));
            png.writeImageToStream (img, os);
        }
        c->showFinished (3);
        auto img2 = c->createComponentSnapshot (c->getLocalBounds());
        {
            FileOutputStream os (dir.getChildFile ("setup-done.png"));
            png.writeImageToStream (img2, os);
        }
        check (dir.getChildFile ("setup.png").getSize() > 1000 && dir.getChildFile ("setup-done.png").getSize() > 1000, "draws the window");
    }

    log << (fails == 0 ? "ALL PASSED" : String (fails) + " FAILED") << "\n";
    out.replaceWithText (log);
    std::printf ("%s\n", fails == 0 ? "SETUP CHECK PASSED" : "SETUP CHECK FAILED");
    return fails == 0 ? 0 : 1;
}

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
             ink (0xfff1eefb), inkDim (0xff9d97b8), violet (0xff7c5cff), teal (0xff2ad6c8), okGreen (0xff4fd68a), orange (0xffffa94d);

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

/** One Cat: icon, name, what it is, its state, and a tick. Click = pick / unpick. */
class Card : public Component
{
public:
    Card (const Item& i) : item (i) { setRepaintsOnMouseActivity (true); setMouseCursor (MouseCursor::PointingHandCursor); }
    std::function<void()> onToggle;
    bool selected = false;
    State state = State::NotInstalled;
    bool busy = false;

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (5.0f);
        g.setColour (isMouseOver() && ! busy ? cardHover : cardBg);
        g.fillRoundedRectangle (r, 14.0f);
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
        // the tick
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

        // the state
        String badge; Colour bc;
        switch (state)
        {
            case State::Installed:    badge = "Installed";        bc = okGreen; break;
            case State::Update:       badge = "Update ready";     bc = orange;  break;
            case State::NotInstalled: badge = "Not installed";    bc = inkDim;  break;
        }
        g.setFont (uiFont (12.0f, true));
        const float bw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), badge) + 18.0f;
        auto b = text.removeFromBottom (20.0f).withWidth (bw);
        g.setColour (bc.withAlpha (0.16f));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (bc);
        g.drawText (badge, b, Justification::centred);
    }

    void mouseUp (const MouseEvent& e) override
    {
        if (! busy && getLocalBounds().contains (e.getPosition()) && onToggle) onToggle();
    }

    const Item& item;
};

/** A big rounded button. */
class Pill : public TextButton
{
public:
    Pill (const String& t, bool primary_) : TextButton (t), primary (primary_) {}
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
            g.setColour ((over && isEnabled() ? cardHover : cardBg).withMultipliedAlpha (isEnabled() ? 1.0f : 0.5f));
            g.fillRoundedRectangle (r, c);
            g.setColour (line);
            g.drawRoundedRectangle (r, c, 1.0f);
        }
        g.setColour (isEnabled() ? ink : inkDim.withAlpha (0.5f));
        g.setFont (uiFont (primary ? 16.0f : 14.0f, primary));
        g.drawText (getButtonText(), getLocalBounds(), Justification::centred);
    }
    bool primary;
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
            auto* c = cards.add (new Card (i));
            c->onToggle = [this, c] { c->selected = ! c->selected; c->repaint(); updateButtons(); };
            list.addAndMakeVisible (c);
        }
        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        viewport.getVerticalScrollBar().setColour (ScrollBar::thumbColourId, line);
        addAndMakeVisible (viewport);

        for (auto* b : { &installButton, &removeButton, &allButton, &updatesButton, &noneButton, &retryButton, &cubaseButton, &hubButton })
            addAndMakeVisible (b);
        allButton.onClick = [this] { for (auto* c : cards) c->selected = c->item.kind != Kind::Extra || c->selected; repaintCards(); };
        updatesButton.onClick = [this] { for (auto* c : cards) c->selected = c->state == State::Update; repaintCards(); };
        noneButton.onClick = [this] { for (auto* c : cards) c->selected = false; repaintCards(); };
        installButton.onClick = [this] { start (Job::Install); };
        removeButton.onClick = [this] { start (Job::Uninstall); };
        retryButton.onClick = [this] { check(); };
        cubaseButton.onClick = [this] { if (cubase.existsAsFile()) cubase.startAsProcess(); };
        hubButton.onClick = [this] { paths.apps.getChildFile ("Cat-Hub.exe").startAsProcess(); };

        loginToggle.setButtonText ("Check for updates when Windows starts");
        loginToggle.setColour (ToggleButton::textColourId, inkDim);
        loginToggle.setColour (ToggleButton::tickColourId, violet);
        loginToggle.setToggleState (checksAtLogin(), dontSendNotification);
        loginToggle.onClick = [this] { setCheckAtLogin (loginToggle.getToggleState(), paths.apps.getChildFile (setupExe)); };
       #if JUCE_WINDOWS
        addAndMakeVisible (loginToggle);
       #endif

        cubase = findCubase();
        setSize (1060, 760);
        if (! offline) check();
        startTimerHz (15);
    }

    ~SetupComponent() override { if (job != nullptr) job->stopThread (10000); }

    /** For tests / pictures: show this release without asking GitHub. */
    void showRelease (const Release& r) { release = r; checking = false; refreshStates (true); }
    Job* currentJob() { return job.get(); }

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (bg1, 0.0f, 0.0f, bg0, 0.0f, (float) getHeight(), false));
        g.fillAll();
        // a soft glow behind the title
        g.setGradientFill (ColourGradient (violet.withAlpha (0.25f), 160.0f, 40.0f, violet.withAlpha (0.0f), 560.0f, 260.0f, true));
        g.fillRect (getLocalBounds());

        auto head = getLocalBounds().reduced (32, 24).removeFromTop (78);
        // logo
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
        g.setColour (inkDim);
        g.setFont (uiFont (15.0f));
        g.drawText (headline(), head, Justification::topLeft);

        // section titles
        g.setFont (uiFont (13.0f, true));
        g.setColour (inkDim);
        for (auto& [y, t] : sectionTitles)
            g.drawText (t, viewport.getX() + 6, viewport.getY() + y - viewport.getViewPositionY(), 600, 22, Justification::centredLeft);

        // the footer: progress and what is happening
        auto foot = footer;
        g.setColour (Colour (0xff17142a));
        g.fillRect (foot);
        g.setColour (line);
        g.drawHorizontalLine (foot.getY(), 0.0f, (float) getWidth());
        auto bar = statusArea.toFloat().removeFromBottom (8.0f);
        if (job != nullptr || lastProgress > 0.0f)
        {
            g.setColour (line);
            g.fillRoundedRectangle (bar, 4.0f);
            g.setGradientFill (ColourGradient (violet, bar.getX(), 0.0f, teal, bar.getRight(), 0.0f, false));
            g.fillRoundedRectangle (bar.withWidth (jmax (8.0f, bar.getWidth() * lastProgress)), 4.0f);
        }
        g.setColour (statusColour);
        g.setFont (uiFont (14.0f));
        g.drawFittedText (statusText, statusArea.withTrimmedBottom (14), Justification::centredLeft, 2);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        footer = r.removeFromBottom (128);
        r.removeFromTop (118);
        auto sel = r.removeFromTop (40).reduced (32, 4);
        allButton.setBounds (sel.removeFromLeft (110)); sel.removeFromLeft (8);
        updatesButton.setBounds (sel.removeFromLeft (150)); sel.removeFromLeft (8);
        noneButton.setBounds (sel.removeFromLeft (110));
        retryButton.setBounds (sel.removeFromRight (150));
        viewport.setBounds (r.reduced (26, 8));
        layoutCards();

        auto f = footer.reduced (32, 18);
        auto buttons = f.removeFromRight (420);
        auto row1 = buttons.removeFromTop (48);
        installButton.setBounds (row1.removeFromRight (230));
        row1.removeFromRight (10);
        removeButton.setBounds (row1);
        buttons.removeFromTop (10);
        auto row2 = buttons.removeFromTop (36);
        hubButton.setBounds (row2.removeFromRight (180));
        row2.removeFromRight (10);
        cubaseButton.setBounds (row2);
        f.removeFromRight (24);
        statusArea = f.removeFromTop (60);
        loginToggle.setBounds (f.removeFromTop (30));
    }

private:
    String headline() const
    {
        if (checking) return "Looking for the newest Cats on GitHub ...";
        if (! release.ok) return "Can't see the newest build: " + release.error;
        int updates = 0, installed = 0;
        for (auto* c : cards) { updates += c->state == State::Update; installed += c->state != State::NotInstalled; }
        String s = "Newest build: " + release.built + "   (" + release.commit.substring (0, 7) + ")";
        if (installed == 0) return s + "  -  pick what you want and press Install";
        return s + (updates > 0 ? "  -  " + String (updates) + (updates == 1 ? " update ready" : " updates ready") : String ("  -  everything is up to date"));
    }

    void layoutCards()
    {
        const int w = viewport.getWidth() - 14;
        const int cols = jmax (1, w / 330), cw = w / cols, ch = 104;
        int y = 0;
        sectionTitles.clear();
        for (auto kind : { Kind::Plugin, Kind::App, Kind::Extra })
        {
            static const char* titles[] = { "PLUG-INS FOR CUBASE (VST3)", "APPS", "EXTRAS" };
            sectionTitles.push_back ({ y + 4, titles[(int) kind] });
            y += 30;
            int k = 0;
            for (auto* c : cards)
            {
                if (c->item.kind != kind) continue;
                c->setBounds ((k % cols) * cw, y + (k / cols) * ch, cw, ch);
                ++k;
            }
            y += ((k + cols - 1) / cols) * ch + 14;
        }
        list.setSize (w, y);
    }

    void repaintCards() { for (auto* c : cards) c->repaint(); updateButtons(); }

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
                                  : (c->item.kind == Kind::Plugin || c->item.id == "app:Cat-Hub");  // first time: every plug-in + the Hub
        retryButton.setVisible (! release.ok && ! checking);
        if (! release.ok && ! checking) { statusText = "Can't reach the newest build: " + release.error; statusColour = orange; }
        else if (job == nullptr && statusText.startsWith ("Asking")) { statusText = "Ready."; statusColour = inkDim; }
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
                    : String ("Reinstall");
        int reinstall = 0;
        for (auto* c : cards) reinstall += c->selected && c->state == State::Installed && c->item.kind != Kind::Extra;
        if (install + update == 0 && reinstall > 0) text = "Reinstall " + String (reinstall);
        installButton.setButtonText (text);
        installButton.setEnabled (idle && release.ok && install + update + reinstall > 0);
        removeButton.setButtonText (remove > 0 ? "Remove " + String (remove) : String ("Remove"));
        removeButton.setEnabled (idle && remove > 0);
        for (auto* b : { &allButton, &updatesButton, &noneButton }) b->setEnabled (idle);
        cubaseButton.setVisible (cubase.existsAsFile());
        hubButton.setVisible (paths.apps.getChildFile ("Cat-Hub.exe").existsAsFile());
        for (auto* c : cards) c->busy = ! idle;
    }

    void start (Job::Action a)
    {
        std::vector<String> ids;
        for (auto* c : cards)
            if (c->selected && (a == Job::Install || (c->state != State::NotInstalled && c->item.kind != Kind::Extra)))
                ids.push_back (c->item.id);
        if (ids.empty()) return;
        job = std::make_unique<Job> (a, ids, release, paths);
        job->localSource = localSource;
        lastProgress = 0.0f;
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
        if (job->finished.load())
        {
            const bool bad = job->failed.load();
            statusColour = bad ? orange : okGreen;
            job.reset();
            refreshStates (false);
            for (auto* c : cards) if (c->state == State::Installed) c->selected = false;
            if (! bad) lastProgress = 1.0f;
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
    Component list;
    Viewport viewport;
    std::vector<std::pair<int, String>> sectionTitles;
    Pill installButton { "Install", true }, removeButton { "Remove", false }, allButton { "All", false }, updatesButton { "Updates only", false },
         noneButton { "None", false }, retryButton { "Try again", false }, cubaseButton { "Open Cubase", false }, hubButton { "Start Cat Hub", false };
    ToggleButton loginToggle;
    std::unique_ptr<Job> job;
    String statusText { "Ready." };
    Colour statusColour = inkDim;
    float lastProgress = 0.0f;
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
        c->setSize (1060, 760);
        auto img = c->createComponentSnapshot (c->getLocalBounds());
        PNGImageFormat png;
        FileOutputStream os (dir.getChildFile ("setup.png"));
        png.writeImageToStream (img, os);
        check (dir.getChildFile ("setup.png").getSize() > 1000, "draws the window");
    }

    log << (fails == 0 ? "ALL PASSED" : String (fails) + " FAILED") << "\n";
    out.replaceWithText (log);
    std::printf ("%s\n", fails == 0 ? "SETUP CHECK PASSED" : "SETUP CHECK FAILED");
    return fails == 0 ? 0 : 1;
}

// Cat Suite Setup: install, update and remove the Cats (see Setup.h).
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_cryptography/juce_cryptography.h>
#include "Setup.h"
#include "CatFonts.h"
#include <set>

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
Font monoFont (float h) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::plain)); }

String initials (const Item& i)
{
    const auto n = i.name.replace ("Cat ", "").replace ("-Cat", "").replace (" app", "").replace (" library", "").replace (" Pro", "");
    return n.substring (0, 2).toUpperCase();
}

/** The sections, in order. */
enum Group { Recommended = 0, Midi, Instrument, Effect, Apps, ProVersions, Extras, Legacy, numGroups };
const char* groupTitle (int g)
{
    static const char* t[] { "RECOMMENDED", "MIDI CATS", "INSTRUMENTS", "EFFECTS", "APPS", "PRO VERSIONS", "EXTRAS", "OLDER" };
    return t[g];
}
const char* groupNote (int g)
{
    static const char* t[] { "a good start", "they write notes for your synths", "they make their own sound", "for audio tracks",
                             "programs on their own, no DAW needed", "the same sound, a plain panel: every parameter, numbers, meters",
                             "", "the plug-ins as separate programs, and earlier builds" };
    return t[g];
}
const char* groupChip (int g) { static const char* t[] { "Recommended", "MIDI", "Instruments", "Effects", "Apps", "Pro", "Extras", "Older" }; return t[g]; }

/** The section a Cat lives in (RECOMMENDED shows some of them a second time). */
int homeOf (const Item& i)
{
    if (i.legacy) return Legacy;
    if (i.kind == Kind::App) return Apps;
    if (i.kind == Kind::Extra) return Extras;
    if (i.pro) return ProVersions;
    static const StringArray fx { "Echo-Cat", "Molten", "Swarm", "Vortex", "Crypt-FX" };
    static const StringArray inst { "Sing-Cat", "Glide-Cat", "Talk-Cat", "Life-Cat", "Muse-Cat", "Glitch-Cat", "Negative", "Meow-Cat", "Crypt" };
    if (fx.contains (i.id)) return Effect;
    if (inst.contains (i.id)) return Instrument;
    return Midi;
}

/** "2026-11-05 15:20 UTC" (or ISO) -> "today 16:20" / "yesterday 9:05" / "5 Nov 16:20" (local time). */
String friendlyTime (Time utc)
{
    const auto now = Time::getCurrentTime();
    const auto clock = utc.formatted ("%H:%M");
    const auto day = [] (Time x) { return x.formatted ("%Y-%m-%d"); };
    if (day (utc) == day (now)) return "today " + clock;
    if (day (utc) == day (now - RelativeTime::days (1))) return "yesterday " + clock;
    return utc.formatted ("%d %b ") + clock;
}
String friendlyBuilt (const String& built)
{
    const auto parts = StringArray::fromTokens (built, " -:T", {});
    if (parts.size() < 5) return built;
    const Time utc (parts[0].getIntValue(), parts[1].getIntValue() - 1, parts[2].getIntValue(), parts[3].getIntValue(), parts[4].getIntValue(), 0, 0, false);
    return friendlyTime (utc);
}
String bytes (int64 b)
{
    if (b <= 0) return "-";
    if (b < 1024 * 1024) return String (jmax (1, roundToInt ((double) b / 1024.0))) + " KB";
    if (b < 100 * 1048576) return String ((double) b / 1048576.0, 1) + " MB";
    return String (roundToInt ((double) b / 1048576.0)) + " MB";
}

/** What Setup knows about one Cat on this computer. */
struct Info
{
    File where;
    int64 size = 0;
    String commit, when, tag;
};

//==============================================================================
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
    explicit Link (const String& t, Justification j = Justification::centredRight) : TextButton (t), just (j) { setMouseCursor (MouseCursor::PointingHandCursor); }
    void paintButton (Graphics& g, bool over, bool) override
    {
        g.setColour (isEnabled() ? (over ? ink : violet.brighter (0.4f)) : inkDim.withAlpha (0.4f));
        g.setFont (uiFont (12.5f, true));
        g.drawText (getButtonText(), getLocalBounds(), just);
    }
    Justification just;
};

/** One Cat: a card (icon, name, blurb, state) or a row (with its numbers). Click = pick / unpick; (i) = details. */
class ItemView : public Component, public SettableTooltipClient
{
public:
    ItemView (const Item& i, const Paths& p) : item (i), paths (p)
    {
        setRepaintsOnMouseActivity (true);
        setMouseCursor (MouseCursor::PointingHandCursor);
        setTooltip (i.name + ": " + i.blurb);
    }
    std::function<void()> onToggle, onDetails;
    std::function<bool()> isPicked;
    bool compact = false, busy = false;
    State state = State::NotInstalled;
    Info info;
    enum class Work { none, waiting, working, done, failed } work = Work::none;

    bool canOpen() const
    {
       #if JUCE_WINDOWS
        return item.kind == Kind::App && state != State::NotInstalled && info.where.existsAsFile();
       #else
        return false;
       #endif
    }
    Rectangle<float> body() const { return getLocalBounds().toFloat().reduced (compact ? 2.0f : 5.0f); }
    Rectangle<float> infoArea() const
    {
        const auto r = body();
        return compact ? Rectangle<float> (r.getRight() - 30.0f, r.getCentreY() - 11.0f, 22.0f, 22.0f)
                       : Rectangle<float> (r.getRight() - 64.0f, r.getY() + 12.0f, 22.0f, 22.0f);
    }
    Rectangle<float> openArea() const
    {
        const auto r = body();
        return compact ? Rectangle<float> (r.getRight() - 100.0f, r.getCentreY() - 12.0f, 62.0f, 24.0f)
                       : r.reduced (14.0f, 12.0f).removeFromBottom (24.0f).removeFromRight (64.0f);
    }

    void badge (Graphics& g, Rectangle<float> area, bool alignLeft)
    {
        String b; Colour bc;
        switch (state)
        {
            case State::Installed:    b = "Installed";     bc = okGreen; break;
            case State::Update:       b = "Update ready";  bc = orange;  break;
            case State::NotInstalled: b = "Not installed"; bc = inkDim;  break;
        }
        switch (work)
        {
            case Work::waiting: b = "Waiting";  bc = inkDim; break;
            case Work::working: b = "Working" + String ("...").substring (0, 1 + (int) (Time::getMillisecondCounter() / 300) % 3); bc = teal; break;
            case Work::done:    b = String::fromUTF8 ("\xe2\x9c\x93 Done"); bc = okGreen; break;
            case Work::failed:  b = "Didn't work"; bc = red; break;
            case Work::none: break;
        }
        g.setFont (uiFont (12.0f, true));
        const float bw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), b) + 18.0f;
        auto r = alignLeft ? area.withWidth (bw) : area.withLeft (area.getRight() - bw);
        r = r.withSizeKeepingCentre (bw, 20.0f);
        g.setColour (bc.withAlpha (0.16f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (bc);
        g.drawText (b, r, Justification::centred);
    }

    void tickBox (Graphics& g, Rectangle<float> tick, bool picked)
    {
        g.setColour (picked ? violet : Colours::transparentBlack);
        g.fillRoundedRectangle (tick, 6.0f);
        g.setColour (picked ? violet : inkDim.withAlpha (0.6f));
        g.drawRoundedRectangle (tick, 6.0f, 1.5f);
        if (picked)
        {
            Path p;
            p.startNewSubPath (tick.getX() + 5.0f, tick.getCentreY());
            p.lineTo (tick.getX() + 8.5f, tick.getBottom() - 5.0f);
            p.lineTo (tick.getRight() - 4.5f, tick.getY() + 5.0f);
            g.setColour (Colours::white);
            g.strokePath (p, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }

    void infoButton (Graphics& g)
    {
        const auto a = infoArea();
        const bool over = isMouseOver() && a.contains (getMouseXYRelative().toFloat());
        g.setColour (over ? ink : inkDim.withAlpha (0.7f));
        g.drawEllipse (a.reduced (2.0f), 1.3f);
        g.setFont (uiFont (12.0f, true));
        g.drawText ("i", a, Justification::centred);
    }

    void paint (Graphics& g) override
    {
        const bool picked = isPicked && isPicked();
        auto r = body();
        g.setColour (isMouseOver() && ! busy ? cardHover : cardBg);
        g.fillRoundedRectangle (r, compact ? 8.0f : 14.0f);
        if (picked)
        {
            g.setGradientFill (ColourGradient (violet.withAlpha (0.14f), r.getX(), r.getY(), teal.withAlpha (0.05f), r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, compact ? 8.0f : 14.0f);
        }
        g.setColour (picked ? violet : line);
        g.drawRoundedRectangle (r, compact ? 8.0f : 14.0f, picked ? 2.0f : 1.0f);
        const Colour c (item.colour);

        if (compact)
        {
            // tick | dot | name | what it is | state | build | size | where | (open) (i)
            auto row = r.reduced (12.0f, 0.0f);
            tickBox (g, row.removeFromLeft (20.0f).withSizeKeepingCentre (18.0f, 18.0f), picked);
            row.removeFromLeft (12.0f);
            g.setColour (c);
            g.fillRoundedRectangle (row.removeFromLeft (10.0f).withSizeKeepingCentre (10.0f, 10.0f), 3.0f);
            row.removeFromLeft (10.0f);
            g.setColour (ink);
            g.setFont (uiFont (14.0f, true));
            g.drawText (item.name, row.removeFromLeft (150.0f), Justification::centredLeft, true);
            row.removeFromRight (100.0f);                                        // open + info
            auto where = row.removeFromRight (150.0f);
            auto size = row.removeFromRight (70.0f);
            auto build = row.removeFromRight (150.0f);
            auto st = row.removeFromRight (120.0f);
            g.setColour (inkDim);
            g.setFont (uiFont (12.5f));
            g.drawText (item.blurb, row.reduced (4.0f, 0.0f), Justification::centredLeft, true);
            badge (g, st, true);
            g.setFont (monoFont (11.5f));
            g.setColour (inkDim);
            if (state != State::NotInstalled)
            {
                const String b = (info.tag.isNotEmpty() ? info.tag.replace ("build-", "#") + " " : String())
                                 + (info.commit.isNotEmpty() ? info.commit.substring (0, 7) : String ("?"))
                                 + (info.when.isNotEmpty() ? "  " + friendlyTime (Time::fromISO8601 (info.when)) : String());
                g.drawText (b, build, Justification::centredLeft, true);
                g.drawText (bytes (info.size), size, Justification::centredRight, true);
                g.drawText (info.where.getParentDirectory().getFileName(), where.withTrimmedLeft (12.0f), Justification::centredLeft, true);
            }
        }
        else
        {
            auto icon = r.reduced (14.0f).removeFromLeft (52.0f).withSizeKeepingCentre (52.0f, 52.0f);
            if (! item.pro)
            {
                Path ears;
                ears.addTriangle (icon.getX() + 6.0f, icon.getY() + 10.0f, icon.getX() + 10.0f, icon.getY() - 6.0f, icon.getX() + 22.0f, icon.getY() + 4.0f);
                ears.addTriangle (icon.getRight() - 6.0f, icon.getY() + 10.0f, icon.getRight() - 10.0f, icon.getY() - 6.0f, icon.getRight() - 22.0f, icon.getY() + 4.0f);
                g.setColour (c.darker (0.2f));
                g.fillPath (ears);
                g.setGradientFill (ColourGradient (c.brighter (0.25f), icon.getX(), icon.getY(), c.darker (0.45f), icon.getRight(), icon.getBottom(), false));
                g.fillRoundedRectangle (icon, 14.0f);
                g.setColour (c.getPerceivedBrightness() > 0.6f ? Colour (0xff1a1626) : Colours::white);
            }
            else
            {
                // the Pro versions: no artwork, an outline and the letters
                g.setColour (Colour (0xff1a1826));
                g.fillRoundedRectangle (icon, 10.0f);
                g.setColour (c);
                g.drawRoundedRectangle (icon.reduced (1.0f), 10.0f, 1.5f);
            }
            g.setFont (uiFont (19.0f, true));
            g.drawText (initials (item), icon, Justification::centred);
            if (item.kind == Kind::App || item.pro)
            {
                auto tag = Rectangle<float> (30.0f, 14.0f).withCentre ({ icon.getCentreX(), icon.getBottom() - 1.0f });
                g.setColour (item.pro ? teal : Colour (0xff1a1626));
                g.fillRoundedRectangle (tag, 7.0f);
                g.setColour (item.pro ? Colour (0xff1a1626) : ink);
                g.setFont (uiFont (9.5f, true));
                g.drawText (item.pro ? "PRO" : "APP", tag, Justification::centred);
            }

            auto text = r.reduced (14.0f, 12.0f).withTrimmedLeft (66.0f);
            auto top = text.removeFromTop (24.0f);
            tickBox (g, top.removeFromRight (22.0f).withSizeKeepingCentre (20.0f, 20.0f), picked);
            top.removeFromRight (32.0f);                                          // the (i)
            g.setColour (ink);
            g.setFont (uiFont (16.0f, true));
            g.drawText (item.name, top, Justification::centredLeft, true);
            g.setColour (inkDim);
            g.setFont (uiFont (13.0f));
            g.drawFittedText (item.blurb, text.removeFromTop (34.0f).toNearestInt(), Justification::topLeft, 2);
            auto bottom = text.removeFromBottom (20.0f);
            badge (g, bottom, true);
            if (state != State::NotInstalled && work == Work::none && ! canOpen())
            {
                g.setColour (inkDim.withAlpha (0.8f));
                g.setFont (monoFont (11.0f));
                g.drawText ((info.tag.isNotEmpty() ? info.tag.replace ("build-", "#") + "  " : String()) + bytes (info.size), bottom, Justification::centredRight, true);
            }
        }
        infoButton (g);

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
        if (! getLocalBounds().contains (e.getPosition())) return;
        if (infoArea().expanded (4.0f).contains (e.position)) { if (onDetails) onDetails(); return; }
        if (busy) return;
        if (canOpen() && openArea().contains (e.position)) { info.where.startAsProcess(); return; }
        if (onToggle) onToggle();
    }
    void mouseMove (const MouseEvent&) override { repaint(); }

    const Item& item;
    const Paths& paths;
    int section = 0;
};

//==============================================================================
/** (i): everything about one Cat, and what you can do with it. */
class DetailsPanel : public Component
{
public:
    DetailsPanel (const Item& i, const Info& in, State st, const Release& newest, Settings& s, const Paths& p, std::function<void()> changed)
        : item (i), info (in), state (st), release (newest), settings (s), paths (p), onChange (std::move (changed))
    {
        openFolder.onClick = [this] { (info.where.exists() ? info.where : paths.vst3).revealToUser(); };
        addAndMakeVisible (openFolder);
        if (item.kind == Kind::App && state != State::NotInstalled)
        {
            desktop.setButtonText ("Shortcut on the Desktop");
            desktop.setToggleState (settings.wantsDesktop (item), dontSendNotification);
            desktop.setColour (ToggleButton::textColourId, ink);
            desktop.setColour (ToggleButton::tickColourId, violet);
            desktop.onClick = [this]
            {
                settings.desktop[item.id] = desktop.getToggleState();
                settings.save (paths.settingsFile);
                setDesktopShortcut (item, Installed::load (paths.stateFile), paths, desktop.getToggleState());
                if (onChange) onChange();
            };
            addAndMakeVisible (desktop);
        }
        setSize (420, item.kind == Kind::App ? 300 : 266);
    }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().reduced (18, 14);
        g.setColour (ink);
        g.setFont (uiFont (18.0f, true));
        g.drawText (item.name, r.removeFromTop (26), Justification::centredLeft);
        g.setColour (inkDim);
        g.setFont (uiFont (13.0f));
        g.drawText (item.blurb, r.removeFromTop (20), Justification::centredLeft, true);
        r.removeFromTop (8);
        auto rowOf = [&] (const String& k, const String& v)
        {
            auto row = r.removeFromTop (22);
            g.setColour (inkDim);
            g.setFont (uiFont (12.5f));
            g.drawText (k, row.removeFromLeft (120), Justification::centredLeft);
            g.setColour (ink);
            g.setFont (monoFont (12.0f));
            g.drawText (v, row, Justification::centredLeft, true);
        };
        const String st = state == State::Installed ? "installed, up to date" : state == State::Update ? "installed, an update is ready" : "not installed";
        rowOf ("State", st);
        if (state != State::NotInstalled)
        {
            rowOf ("Installed build", (info.tag.isNotEmpty() ? info.tag + "  " : String()) + (info.commit.isNotEmpty() ? info.commit.substring (0, 7) : String ("unknown (old installer)")));
            rowOf ("Installed on", info.when.isNotEmpty() ? friendlyTime (Time::fromISO8601 (info.when)) : String ("-"));
            rowOf ("Size", bytes (info.size));
            rowOf ("Where", info.where.getFullPathName());
        }
        rowOf ("Newest build", release.ok ? (release.tag.isNotEmpty() && release.tag != "latest" ? release.tag + "  " : String()) + release.commit.substring (0, 7) + "  " + friendlyBuilt (release.built) : String ("-"));
        if (release.change.isNotEmpty()) rowOf ("What changed", release.change);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18, 14).removeFromBottom (item.kind == Kind::App ? 66 : 32);
        if (desktop.isVisible()) { desktop.setBounds (r.removeFromTop (28)); r.removeFromTop (6); }
        openFolder.setBounds (r.removeFromTop (30).removeFromLeft (170));
    }

private:
    const Item& item;
    Info info;
    State state;
    Release release;
    Settings& settings;
    const Paths& paths;
    std::function<void()> onChange;
    Pill openFolder { "Show in folder", false, 13.0f };
    ToggleButton desktop;
};

//==============================================================================
/** The gear: channel, folders, shortcuts, the look. */
class SettingsPanel : public Component
{
public:
    SettingsPanel (Settings& s, const Paths& p, std::function<void (bool recheck)> changed) : settings (s), paths (p), onChange (std::move (changed))
    {
        for (auto* b : { &newest, &stable })
        {
            b->setRadioGroupId (7);
            b->setColour (ToggleButton::textColourId, ink);
            b->setColour (ToggleButton::tickColourId, violet);
            addAndMakeVisible (b);
        }
        newest.setToggleState (settings.channel != "stable", dontSendNotification);
        stable.setToggleState (settings.channel == "stable", dontSendNotification);
        newest.onClick = [this] { if (newest.getToggleState()) set ("latest"); };
        stable.onClick = [this] { if (stable.getToggleState()) set ("stable"); };

        for (auto* t : { &startMenu, &legacy, &login })
        {
            t->setColour (ToggleButton::textColourId, ink);
            t->setColour (ToggleButton::tickColourId, violet);
            addAndMakeVisible (t);
        }
        startMenu.setToggleState (settings.startMenu, dontSendNotification);
        startMenu.onClick = [this] { settings.startMenu = startMenu.getToggleState(); save (false); };
        legacy.setToggleState (settings.showLegacy, dontSendNotification);
        legacy.onClick = [this] { settings.showLegacy = legacy.getToggleState(); save (false); };
        login.setToggleState (checksAtLogin(), dontSendNotification);
        login.onClick = [this] { setCheckAtLogin (login.getToggleState(), paths.apps.getChildFile (setupExe)); };
       #if ! JUCE_WINDOWS
        login.setVisible (false);
       #endif

        for (int k = 0; k < 2; ++k)
        {
            auto& change = k == 0 ? vst3Change : appsChange;
            auto& reset = k == 0 ? vst3Reset : appsReset;
            change.onClick = [this, k]
            {
                chooser = std::make_unique<FileChooser> (k == 0 ? "Where the plug-ins go" : "Where the apps go", k == 0 ? paths.vst3 : paths.apps);
                chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this, k] (const FileChooser& fc)
                {
                    const auto d = fc.getResult();
                    if (d == File()) return;
                    (k == 0 ? settings.vst3Dir : settings.appsDir) = d.getFullPathName();
                    save (true);
                    repaint();
                });
            };
            reset.onClick = [this, k] { (k == 0 ? settings.vst3Dir : settings.appsDir) = {}; save (true); repaint(); };
            addAndMakeVisible (change);
            addAndMakeVisible (reset);
        }
        setSize (520, 440);
    }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().reduced (20, 16);
        g.setColour (ink);
        g.setFont (uiFont (18.0f, true));
        g.drawText ("Settings", r.removeFromTop (28), Justification::centredLeft);
        auto title = [&] (int y, const String& t, const String& note)
        {
            g.setColour (inkDim);
            g.setFont (uiFont (12.0f, true));
            g.drawText (t, 20, y, 300, 18, Justification::centredLeft);
            g.setFont (uiFont (12.0f));
            g.drawText (note, 20, y + 16, getWidth() - 40, 16, Justification::centredLeft, true);
        };
        title (54, "BUILDS", "Newest: every change, the moment it is ready. Stable: a build that has been played with first.");
        title (140, "FOLDERS", "Plug-ins: your DAW must look in this folder (Cubase finds the default one).");
        g.setColour (ink);
        g.setFont (monoFont (11.5f));
        g.drawText (folderText (settings.vst3Dir, paths.defaultVst3), 20, 176, 300, 26, Justification::centredLeft, true);
        g.drawText (folderText (settings.appsDir, paths.defaultApps), 20, 210, 300, 26, Justification::centredLeft, true);
        title (252, "SHORTCUTS AND MORE", "");
    }

    void resized() override
    {
        newest.setBounds (20, 92, 160, 26);
        stable.setBounds (190, 92, 160, 26);
        vst3Change.setBounds (getWidth() - 186, 176, 90, 26);
        vst3Reset.setBounds (getWidth() - 90, 176, 70, 26);
        appsChange.setBounds (getWidth() - 186, 210, 90, 26);
        appsReset.setBounds (getWidth() - 90, 210, 70, 26);
        startMenu.setBounds (20, 276, getWidth() - 40, 26);
        login.setBounds (20, 306, getWidth() - 40, 26);
        legacy.setBounds (20, 336, getWidth() - 40, 26);
    }

private:
    static String folderText (const String& custom, const File& def)
    {
        return custom.isNotEmpty() ? custom : def.getFullPathName() + "  (default)";
    }
    void set (const String& ch) { settings.channel = ch; save (true); }
    void save (bool recheck) { settings.save (paths.settingsFile); if (onChange) onChange (recheck); }

    Settings& settings;
    const Paths& paths;
    std::function<void (bool)> onChange;
    std::unique_ptr<FileChooser> chooser;
    ToggleButton newest { "Newest (every change)" }, stable { "Stable" };
    ToggleButton startMenu { "Start menu shortcuts for the apps" }, legacy { "Show the older things (separate plug-in apps, earlier builds)" },
                 login { "Check for updates when Windows starts" };
    Pill vst3Change { "Change", false, 12.5f }, vst3Reset { "Default", false, 12.5f }, appsChange { "Change", false, 12.5f }, appsReset { "Default", false, 12.5f };
};

//==============================================================================
/** OLDER: the earlier builds; install the picked Cats (or what is installed) from one of them. */
class BuildsPanel : public Component
{
public:
    std::function<void (const Release&)> onInstall;
    std::vector<Release> builds;
    String note { "Asking GitHub for the earlier builds ..." };
    OwnedArray<Pill> buttons;

    void setBuilds (std::vector<Release> b, const String& error)
    {
        builds = std::move (b);
        note = error.isNotEmpty() ? error : (builds.empty() ? "No earlier builds on GitHub yet (they are kept from now on: the last 10)." : String());
        buttons.clear();
        for (size_t k = 0; k < builds.size(); ++k)
        {
            auto* p = buttons.add (new Pill ("Install this build", false, 12.0f));
            p->onClick = [this, k] { if (onInstall) onInstall (builds[k]); };
            addAndMakeVisible (p);
        }
        setSize (getWidth(), heightFor());
        resized();
        repaint();
    }
    int heightFor() const { return 30 + (int) jmax ((size_t) 1, builds.size()) * 38; }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().reduced (6, 0);
        g.setColour (inkDim);
        g.setFont (uiFont (13.0f));
        g.drawText ("Something broke in the newest build? Pick the Cats above, then install them from an earlier build.", r.removeFromTop (26), Justification::centredLeft, true);
        if (note.isNotEmpty()) { g.drawText (note, r.removeFromTop (30), Justification::centredLeft, true); return; }
        for (auto& b : builds)
        {
            auto row = r.removeFromTop (38).reduced (0, 3);
            g.setColour (cardBg);
            g.fillRoundedRectangle (row.toFloat(), 8.0f);
            row.reduce (12, 0);
            g.setColour (ink);
            g.setFont (uiFont (13.5f, true));
            g.drawText (b.tag.replace ("build-", "Build #"), row.removeFromLeft (110), Justification::centredLeft);
            g.setColour (inkDim);
            g.setFont (monoFont (11.5f));
            g.drawText (friendlyBuilt (b.built) + "  " + b.commit.substring (0, 7), row.removeFromLeft (190), Justification::centredLeft);
            g.setFont (uiFont (12.5f));
            g.drawText (b.change, row.withTrimmedRight (160), Justification::centredLeft, true);
        }
    }
    void resized() override
    {
        for (int k = 0; k < buttons.size(); ++k)
            buttons[k]->setBounds (getWidth() - 160, 26 + k * 38 + 6, 148, 26);
    }
};
}

//==============================================================================
class SetupComponent : public Component, private Timer
{
public:
    explicit SetupComponent (Paths p, bool offline = false) : paths (std::move (p))
    {
        settings = Settings::load (paths.settingsFile);
        // every Cat in its section; the recommended ones a second time at the top
        for (auto& i : catalog())
        {
            for (int sec : { homeOf (i), i.recommended ? (int) Recommended : -1 })
            {
                if (sec < 0) continue;
                auto* v = views.add (new ItemView (i, paths));
                v->section = sec;
                v->isPicked = [this, id = i.id] { return picked.count (id) > 0; };
                v->onToggle = [this, id = i.id] { if (picked.count (id) > 0) picked.erase (id); else picked.insert (id); repaintCards(); };
                v->onDetails = [this, v] { showDetails (*v); };
                list.addAndMakeVisible (v);
            }
        }
        for (int gr = 0; gr < numGroups; ++gr)
        {
            auto* l = sectionLinks.add (new Link ("Pick all"));
            l->onClick = [this, gr]
            {
                bool all = true;
                for (auto* v : views) if (v->section == gr && v->isVisible()) all &= picked.count (v->item.id) > 0;
                for (auto* v : views) if (v->section == gr && v->isVisible()) { if (all) picked.erase (v->item.id); else picked.insert (v->item.id); }
                repaintCards();
            };
            list.addAndMakeVisible (l);
        }
        legacyLink.onClick = [this] { settings.showLegacy = ! settings.showLegacy; settings.save (paths.settingsFile); if (settings.showLegacy) loadBuilds(); layoutCards(); repaint(); };
        list.addAndMakeVisible (legacyLink);
        builds.onInstall = [this] (const Release& r) { installFrom (r); };
        list.addChildComponent (builds);
        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        viewport.getVerticalScrollBar().setColour (ScrollBar::thumbColourId, line);
        addAndMakeVisible (viewport);

        // filter chips
        const int chipGroups[] { -1, Recommended, Midi, Instrument, Effect, Apps, ProVersions };
        for (int k : chipGroups)
        {
            auto* chip = chips.add (new Pill (k < 0 ? "All" : groupChip (k), false, 13.0f));
            chip->setToggleState (k < 0, dontSendNotification);
            chip->onClick = [this, k] { filter = k; for (int j = 0; j < chips.size(); ++j) chips[j]->setToggleState (chipFilter[(size_t) j] == k, dontSendNotification); layoutCards(); repaint(); };
            addAndMakeVisible (chip);
            chipFilter.push_back (k);
        }
        // cards / list
        for (auto* b : { &cardsB, &listB }) { b->setClickingTogglesState (false); addAndMakeVisible (b); }
        cardsB.onClick = [this] { setListView (false); };
        listB.onClick = [this] { setListView (true); };
        cardsB.setToggleState (! settings.listView, dontSendNotification);
        listB.setToggleState (settings.listView, dontSendNotification);
        for (auto* v : views) v->compact = settings.listView;

        search.setTextToShowWhenEmpty ("Search the Cats ...", inkDim.withAlpha (0.7f));
        search.setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
        search.setColour (TextEditor::outlineColourId, Colours::transparentBlack);
        search.setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
        search.setColour (TextEditor::textColourId, ink);
        search.setFont (uiFont (14.0f));
        search.setIndents (18, 10);
        search.onTextChange = [this] { layoutCards(); repaint(); };
        search.onEscapeKey = [this] { search.clear(); layoutCards(); repaint(); };
        search.onFocusLost = [this] { repaint(); };
        addAndMakeVisible (search);
        gearB.onClick = [this] { showSettings(); };
        gearB.setTooltip ("Settings: Newest / Stable builds, folders, shortcuts");
        addAndMakeVisible (gearB);

        for (auto* b : { &installButton, &removeButton, &retryButton, &cubaseButton, &hubButton, &loopButton })
            addAndMakeVisible (b);
        for (auto* b : { &allLink, &updatesLink, &noneLink }) addAndMakeVisible (b);
        allLink.onClick = [this] { for (auto* v : views) if (v->item.kind != Kind::Extra && ! v->item.legacy && ! v->item.pro) picked.insert (v->item.id); repaintCards(); };
        updatesLink.onClick = [this] { picked.clear(); for (auto* v : views) if (v->state == State::Update) picked.insert (v->item.id); repaintCards(); };
        noneLink.onClick = [this] { picked.clear(); repaintCards(); };
        installButton.onClick = [this] { start (Job::Install, release); };
        removeButton.onClick = [this]
        {
            int n = 0;
            for (auto& id : picked) if (auto* i = findItem (id)) n += stateFor (id) != State::NotInstalled && i->kind != Kind::Extra;
            AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Remove " + String (n) + (n == 1 ? " Cat?" : " Cats?"),
                                          "They are deleted from this computer. Projects that use them will miss them (you can install them again any time).",
                                          "Remove", "Cancel", this, ModalCallbackFunction::create ([this] (int ok) { if (ok != 0) start (Job::Uninstall, release); }));
        };
        retryButton.onClick = [this] { check(); };
        cubaseButton.onClick = [this] { if (cubase.existsAsFile()) cubase.startAsProcess(); };
        hubButton.onClick = [this] { paths.apps.getChildFile ("Cat-Hub.exe").startAsProcess(); };
        loopButton.onClick = [this] { paths.apps.getChildFile ("Cat-Loop.exe").startAsProcess(); };

        cubase = findCubase();
        setSize (1100, 800);
        if (! offline) { check(); if (settings.showLegacy) loadBuilds(); }
        startTimerHz (15);
    }

    ~SetupComponent() override { if (job != nullptr) job->stopThread (10000); }

    /** For tests / pictures: show this release without asking GitHub. */
    void showRelease (const Release& r) { release = r; checking = false; refreshStates (true); }
    void showBuilds (std::vector<Release> b) { builds.setBuilds (std::move (b), {}); layoutCards(); }
    void setListView (bool on)
    {
        settings.listView = on;
        settings.save (paths.settingsFile);
        cardsB.setToggleState (! on, dontSendNotification);
        listB.setToggleState (on, dontSendNotification);
        for (auto* v : views) v->compact = on;
        layoutCards();
        repaint();
    }
    Job* currentJob() { return job.get(); }
    void showFinished (int count) { finishedCount = count; finishedOk = true; finishedInstall = true; lastProgress = 1.0f; statusText = {}; updateButtons(); repaint(); }

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (bg1, 0.0f, 0.0f, bg0, 0.0f, (float) getHeight(), false));
        g.fillAll();
        g.setGradientFill (ColourGradient (violet.withAlpha (0.25f), 160.0f, 40.0f, violet.withAlpha (0.0f), 560.0f, 260.0f, true));
        g.fillRect (getLocalBounds());

        auto head = getLocalBounds().reduced (32, 24).removeFromTop (78);
        head.removeFromRight (360);
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

        // the search box's rounded frame, "Select:", "View:"
        {
            const auto sb = search.getBounds().toFloat().expanded (1.0f);
            g.setColour (cardBg);
            g.fillRoundedRectangle (sb, sb.getHeight() * 0.5f);
            g.setColour (search.hasKeyboardFocus (true) ? violet : line);
            g.drawRoundedRectangle (sb, sb.getHeight() * 0.5f, 1.0f);
            g.setColour (inkDim);
            g.setFont (uiFont (12.5f));
            g.drawText ("Select:", allLink.getX() - 58, allLink.getY(), 52, allLink.getHeight(), Justification::centredRight);
        }

        // section titles (they scroll with the list)
        g.saveState();
        g.reduceClipRegion (viewport.getBounds());
        for (auto& s : sectionTitles)
        {
            const int x = viewport.getX() + 8, y = viewport.getY() + s.y - viewport.getViewPositionY();
            g.setFont (uiFont (12.5f, true));
            g.setColour (s.group == Recommended ? violet.brighter (0.5f) : (s.group == ProVersions ? teal : ink.withAlpha (0.85f)));
            g.drawText (s.title, x, y, 300, 22, Justification::centredLeft);
            int tx = x + GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s.title) + 10;
            if (s.count > 0)
            {
                const String n (s.count);
                g.setFont (uiFont (11.0f, true));
                const int nw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), n) + 14;
                g.setColour (line);
                g.fillRoundedRectangle ((float) tx, (float) y + 3.0f, (float) nw, 16.0f, 8.0f);
                g.setColour (inkDim);
                g.drawText (n, tx, y + 3, nw, 16, Justification::centred);
                tx += nw + 12;
            }
            g.setColour (inkDim);
            g.setFont (uiFont (13.0f));
            g.drawText (s.note, tx, y, 640, 22, Justification::centredLeft, true);
            if (s.group != Legacy && settings.listView && s.count > 0)
            {
                // the list's column titles
                const int right = viewport.getX() + list.getWidth() - 12;
                g.setFont (uiFont (10.5f, true));
                g.setColour (inkDim.withAlpha (0.7f));
                g.drawText ("WHERE", right - 238, y + 4, 150, 16, Justification::centredLeft);
                g.drawText ("SIZE", right - 320, y + 4, 70, 16, Justification::centredRight);
                g.drawText ("INSTALLED BUILD", right - 470, y + 4, 150, 16, Justification::centredLeft);
            }
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
        gearB.setBounds (getWidth() - 32 - 44, 46, 44, 38);
        search.setBounds (getWidth() - 32 - 44 - 10 - 280, 46, 280, 38);
        r.removeFromTop (116);
        auto row = r.removeFromTop (44).reduced (32, 5);
        for (auto* c : chips)
        {
            const int w = GlyphArrangement::getStringWidthInt (uiFont (13.0f), c->getButtonText()) + 30;
            c->setBounds (row.removeFromLeft (w));
            row.removeFromLeft (6);
        }
        listB.setBounds (row.removeFromRight (56));
        cardsB.setBounds (row.removeFromRight (64));
        row.removeFromRight (18);
        noneLink.setBounds (row.removeFromRight (40));
        row.removeFromRight (12);
        updatesLink.setBounds (row.removeFromRight (62));
        row.removeFromRight (12);
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
    }

private:
    std::tuple<String, String, Colour> headline() const
    {
        const String channel = settings.channel == "stable" ? "Stable build: " : "Newest build: ";
        if (checking) return { "Looking for the newest Cats on GitHub ...", {}, inkDim };
        if (! release.ok) return { "Can't see the build", "offline?", orange };
        int updates = 0, installed = 0;
        for (auto& [id, st] : states) { updates += st == State::Update; installed += st != State::NotInstalled; }
        const String s = channel + friendlyBuilt (release.built);
        if (installed == 0) return { s, "pick what you want", violet.brighter (0.4f) };
        if (updates > 0) return { s, String (updates) + (updates == 1 ? " update ready" : " updates ready"), orange };
        return { s, String::fromUTF8 ("\xe2\x9c\x93 everything is up to date"), okGreen };
    }

    State stateFor (const String& id) const { const auto it = states.find (id); return it != states.end() ? it->second : State::NotInstalled; }

    String selectionSummary() const
    {
        if (! release.ok) return {};
        if (picked.empty()) return "Click the Cats you want (they get a tick), then press Install.  (i) shows everything about one.";
        int64 size = 0;
        for (auto& id : picked) { const auto it = infos.find (id); if (it != infos.end()) size += it->second.size; }
        return String ((int) picked.size()) + (picked.size() == 1 ? " picked." : " picked.")
               + (size > 0 ? "  Installed now: " + bytes (size) + "." : String())
               + "  Plug-ins go to " + paths.vst3.getFullPathName() + ".";
    }

    bool matches (const ItemView& v) const
    {
        const auto q = search.getText().trim();
        if (v.item.legacy && ! settings.showLegacy) return false;
        if (q.isNotEmpty()) return v.section != Recommended && (v.item.name.containsIgnoreCase (q) || v.item.blurb.containsIgnoreCase (q));
        if (filter >= 0) return v.section == filter;
        return true;
    }

    void layoutCards()
    {
        const int w = viewport.getWidth() - 14;
        const bool compact = settings.listView;
        const int cols = compact ? 1 : jmax (1, w / 340), cw = w / cols, ch = compact ? 44 : 104;
        int y = 0;
        sectionTitles.clear();
        for (int gr = 0; gr < numGroups; ++gr)
        {
            int k = 0;
            for (auto* v : views)
                if (v->section == gr) { v->setVisible (matches (*v)); k += v->isVisible(); }
            sectionLinks[gr]->setVisible (k > 1);
            const bool legacySection = gr == Legacy && search.getText().isEmpty() && (filter < 0);
            if (legacySection)
            {
                int hidden = 0;
                for (auto* v : views) hidden += v->section == Legacy;
                sectionTitles.push_back ({ y + 4, gr, groupTitle (gr), groupNote (gr), 0 });
                legacyLink.setButtonText (settings.showLegacy ? "Hide" : "Show (" + String (hidden) + " + earlier builds)");
                legacyLink.setBounds (w - 260, y + 4, 252, 22);
                legacyLink.setVisible (true);
                sectionLinks[gr]->setVisible (false);
                y += 30;
                if (! settings.showLegacy) { builds.setVisible (false); continue; }
            }
            else if (gr == Legacy) legacyLink.setVisible (false);
            if (k == 0 && ! (legacySection && settings.showLegacy)) continue;
            if (! legacySection)
            {
                sectionTitles.push_back ({ y + 4, gr, groupTitle (gr), groupNote (gr), k });
                sectionLinks[gr]->setBounds (w - 140, y + 4, 132, 22);
                y += 30;
            }
            k = 0;
            for (auto* v : views)
            {
                if (v->section != gr || ! v->isVisible()) continue;
                v->setBounds ((k % cols) * cw, y + (k / cols) * ch, cw, ch);
                ++k;
            }
            y += ((k + cols - 1) / cols) * ch + 14;
            if (legacySection)
            {
                builds.setVisible (true);
                builds.setBounds (0, y, w, builds.heightFor());
                y += builds.getHeight() + 14;
            }
        }
        if (! (search.getText().isEmpty() && filter < 0)) builds.setVisible (false);
        list.setSize (w, jmax (y, 10));
    }

    void repaintCards()
    {
        for (int gr = 0; gr < numGroups; ++gr)
        {
            bool all = true;
            for (auto* v : views) if (v->section == gr && v->isVisible()) all &= picked.count (v->item.id) > 0;
            sectionLinks[gr]->setButtonText (all ? "Unpick all" : "Pick all");
        }
        for (auto* v : views) v->repaint();
        updateButtons();
        repaint (footer);
    }

    void check()
    {
        checking = true;
        retryButton.setVisible (false);
        statusText = "Asking GitHub for the " + String (settings.channel == "stable" ? "stable" : "newest") + " build ...";
        statusColour = inkDim;
        repaint();
        SafePointer<SetupComponent> safe (this);
        const auto channel = settings.channel;
        Thread::launch ([safe, channel]
        {
            auto r = fetchRelease (channel);
            MessageManager::callAsync ([safe, r]
            {
                if (safe == nullptr) return;
                safe->release = r;
                safe->checking = false;
                safe->refreshStates (true);
            });
        });
    }

    void loadBuilds()
    {
        SafePointer<SetupComponent> safe (this);
        Thread::launch ([safe]
        {
            auto b = fetchBuilds();
            MessageManager::callAsync ([safe, b]
            {
                if (safe == nullptr) return;
                safe->builds.setBuilds (b, {});
                safe->layoutCards();
            });
        });
    }

    void refreshStates (bool pickDefaults)
    {
        const auto state = Installed::load (paths.stateFile);
        bool any = false;
        states.clear();
        infos.clear();
        for (auto& i : catalog())
        {
            const auto st = stateOf (i, state, release, paths);
            states[i.id] = st;
            Info in;
            in.where = locate (i, state, paths);
            if (st != State::NotInstalled && i.kind != Kind::Extra) in.size = sizeOnDisk (in.where);
            if (auto it = state.items.find (i.id); it != state.items.end()) in.commit = it->second;
            if (auto it = state.when.find (i.id); it != state.when.end()) in.when = it->second;
            if (auto it = state.build.find (i.id); it != state.build.end()) in.tag = it->second;
            infos[i.id] = in;
            any |= st != State::NotInstalled && i.kind != Kind::Extra;
        }
        for (auto* v : views) { v->state = states[v->item.id]; v->info = infos[v->item.id]; }
        if (pickDefaults)
        {
            picked.clear();
            for (auto& i : catalog())
                if (any ? states[i.id] == State::Update                                              // updating: what's out of date
                        : ((i.kind == Kind::Plugin && ! i.pro) || i.id == "app:Cat-Hub" || i.id == "app:Cat-Loop"))   // first time
                    picked.insert (i.id);
        }
        retryButton.setVisible (! release.ok && ! checking);
        if (! release.ok && ! checking) { statusText = "Can't reach the build: " + release.error; statusColour = orange; }
        else if (job == nullptr && statusText.startsWith ("Asking")) { statusText = {}; statusColour = inkDim; }
        repaintCards();
        repaint();
    }

    void updateButtons()
    {
        int install = 0, update = 0, remove = 0, reinstall = 0;
        for (auto& id : picked)
        {
            const auto* i = findItem (id);
            if (i == nullptr) continue;
            const auto st = stateFor (id);
            if (st == State::NotInstalled) ++install;
            else if (st == State::Update) ++update;
            else if (i->kind != Kind::Extra) ++reinstall;
            if (st != State::NotInstalled && i->kind != Kind::Extra) ++remove;
        }
        const bool idle = job == nullptr;
        String text = install > 0 && update > 0 ? "Install & update " + String (install + update)
                    : update > 0 ? "Update " + String (update)
                    : install > 0 ? "Install " + String (install)
                    : String ("Install");
        if (install + update == 0 && reinstall > 0) text = "Reinstall " + String (reinstall);
        if (! idle) text = "Working ...";
        installButton.setButtonText (text);
        installButton.setEnabled (idle && release.ok && install + update + reinstall > 0);
        removeButton.setButtonText (remove > 0 ? "Remove " + String (remove) : String ("Remove"));
        removeButton.setEnabled (idle && remove > 0);
        for (auto* b : { &allLink, &updatesLink, &noneLink }) b->setEnabled (idle);
        for (auto* l : sectionLinks) l->setEnabled (idle);
        for (auto* b : builds.buttons) b->setEnabled (idle);
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
        for (auto* v : views) v->busy = ! idle;
    }

    void start (Job::Action a, const Release& from)
    {
        std::vector<String> ids;
        for (auto& i : catalog())
            if (picked.count (i.id) > 0 && (a == Job::Install || (stateFor (i.id) != State::NotInstalled && i.kind != Kind::Extra)))
                ids.push_back (i.id);
        if (ids.empty()) return;
        for (auto* v : views) v->work = std::find (ids.begin(), ids.end(), v->item.id) != ids.end() ? ItemView::Work::waiting : ItemView::Work::none;
        job = std::make_unique<Job> (a, ids, from, paths);
        job->localSource = localSource;
        lastProgress = 0.0f;
        finishedOk = false;
        finishedInstall = a == Job::Install;
        statusColour = ink;
        job->startThread();
        updateButtons();
    }

    /** OLDER: install the picked Cats (or, with none picked, everything installed) from an earlier build. */
    void installFrom (const Release& r)
    {
        if (picked.empty())
            for (auto& [id, st] : states)
                if (st != State::NotInstalled && findItem (id) != nullptr && findItem (id)->kind != Kind::Extra) picked.insert (id);
        if (picked.empty()) return;
        AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Install " + String ((int) picked.size()) + " from " + r.tag.replace ("build-", "build #") + "?",
                                      "The picked Cats go back to that build (" + friendlyBuilt (r.built) + "). Setup will then show them as \"Update ready\" - just don't update them until the problem is fixed.",
                                      "Install", "Cancel", this, ModalCallbackFunction::create ([this, r] (int ok) { if (ok != 0) start (Job::Install, r); }));
    }

    void showDetails (ItemView& v)
    {
        auto panel = std::make_unique<DetailsPanel> (v.item, infos[v.item.id], stateFor (v.item.id), release, settings, paths, [this] { refreshStates (false); });
        CallOutBox::launchAsynchronously (std::move (panel), v.getScreenBounds(), nullptr);
    }

    void showSettings()
    {
        auto panel = std::make_unique<SettingsPanel> (settings, paths, [this] (bool recheck)
        {
            const auto oldChannel = channelChecked;
            paths = Paths::forUser().vst3 != File() && localSource == File() ? Paths::forUser() : paths;
            if (recheck && settings.channel != oldChannel) { channelChecked = settings.channel; check(); }
            else refreshStates (false);
            if (settings.showLegacy && builds.builds.empty()) loadBuilds();
            layoutCards();
            repaint();
        });
        CallOutBox::launchAsynchronously (std::move (panel), gearB.getScreenBounds(), nullptr);
    }

    void timerCallback() override
    {
        if (job == nullptr) return;
        lastProgress = job->progress.load();
        statusText = job->getStatus();
        statusColour = job->waitingForClose.load() ? orange : ink;
        const auto current = job->getCurrent();
        const auto doneSoFar = job->getDone();
        for (auto* v : views)
        {
            if (v->work == ItemView::Work::none) continue;
            if (doneSoFar.contains (v->item.id)) v->work = ItemView::Work::done;
            else if (v->item.id == current) v->work = ItemView::Work::working;
            v->repaint();
        }
        if (job->finished.load())
        {
            const bool bad = job->failed.load();
            const int count = job->getDone().size();
            for (auto* v : views)
                if (v->work == ItemView::Work::waiting || v->work == ItemView::Work::working) v->work = bad ? ItemView::Work::failed : ItemView::Work::done;
            statusColour = bad ? orange : okGreen;
            job.reset();
            refreshStates (false);
            for (auto& [id, st] : states) if (st == State::Installed) picked.erase (id);
            if (! bad) { lastProgress = 1.0f; finishedOk = true; finishedCount = count; statusText = {}; }
            Timer::callAfterDelay (4000, [safe = SafePointer<SetupComponent> (this)] { if (safe != nullptr && safe->job == nullptr) { for (auto* v : safe->views) { v->work = ItemView::Work::none; v->repaint(); } } });
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
    Settings settings;
    String channelChecked { settings.channel };
    Release release;
    bool checking = false;
    OwnedArray<ItemView> views;
    std::set<String> picked;
    std::map<String, State> states;
    std::map<String, Info> infos;
    OwnedArray<Link> sectionLinks;
    Link legacyLink { "Show" };
    BuildsPanel builds;
    OwnedArray<Pill> chips;
    std::vector<int> chipFilter;
    int filter = -1;
    TextEditor search;
    Pill gearB { String::fromUTF8 ("\xe2\x9a\x99"), false, 18.0f }, cardsB { "Cards", false, 12.5f }, listB { "List", false, 12.5f };
    Component list;
    Viewport viewport;
    struct Section { int y; int group; String title, note; int count; };
    std::vector<Section> sectionTitles;
    Pill installButton { "Install", true }, removeButton { "Remove", false }, retryButton { "Try again", false, 13.0f },
         cubaseButton { "Open Cubase", false, 13.0f }, hubButton { "Start Cat Hub", false, 13.0f }, loopButton { "Start Cat Loop", false, 13.0f };
    Link allLink { "All" }, updatesLink { "Updates" }, noneLink { "None" };
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
        stage.getChildFile ("VST3 Pro/Glitch-Cat Pro.vst3/Contents/x86_64-win").createDirectory();
        stage.getChildFile ("VST3 Pro/Glitch-Cat Pro.vst3/Contents/x86_64-win/Glitch-Cat Pro.vst3").replaceWithText ("plug-in Glitch-Cat Pro");
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
        "tag_name":"latest",
        "body":"text<br><br>Change: Cat Loop: a 5-track live looper (like a Boss RC-505), Windows / Mac / Linux<br>Build: build-41<br>Built from commit cf7f91eeec0bd5183fd440da33bbb78e2d6a8cad.",
        "assets":[{"name":"Cat-Suite-Windows.zip","size":100,"digest":"sha256:9B04","browser_download_url":"https://github.com/eduardo1345978/cat_suite/releases/download/latest/Cat-Suite-Windows.zip"},
                  {"name":"Cat-Sound-Apps-Windows.zip","size":200,"browser_download_url":"https://github.com/eduardo1345978/cat_suite/releases/download/latest/Cat-Sound-Apps-Windows.zip"},
                  {"name":"Evil.zip","size":1,"browser_download_url":"https://evil.example/x.zip"}]})J";
    auto rel = parseRelease (JSON::parse (String (json)));
    check (rel.ok && rel.commit == "cf7f91eeec0bd5183fd440da33bbb78e2d6a8cad" && rel.built == "2026-11-05 15:20 UTC", "reads the release: commit and date");
    check (rel.tag == "build-41" && rel.change == "Cat Loop: a 5-track live looper", "reads which numbered build it is and what it changed (" + rel.tag + ", " + rel.change + ")");
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
    const auto [ok1, st1] = runJob (Job::Install, { "Glitch-Cat", "Hum-Cat", "app:Cat-Hub", "Glitch-Cat Pro" }, rel);
    check (ok1, "installs: " + st1);
    check (paths.vst3.getChildFile ("Glitch-Cat.vst3/Contents/x86_64-win/Glitch-Cat.vst3").loadFileAsString() == "plug-in Glitch-Cat", "the VST3 is in the VST3 folder");
    check (paths.apps.getChildFile ("Cat-Hub.exe").existsAsFile() && paths.apps.getChildFile ("Hub Cats/Chord-Cat.vst3/x.txt").existsAsFile(), "the Hub and its own Cats are in the apps folder");
    check (! paths.vst3.getChildFile ("Micro-Cat.vst3").exists(), "only what was picked");
    check (paths.vst3.getChildFile ("Glitch-Cat Pro.vst3/Contents/x86_64-win/Glitch-Cat Pro.vst3").loadFileAsString() == "plug-in Glitch-Cat Pro",
           "a Pro version comes from the zip's VST3 Pro folder");
    check (paths.apps.getChildFile ("THIRD-PARTY-NOTICES.txt").existsAsFile() && paths.apps.getChildFile ("licenses/MIT-VST3-SDK.txt").existsAsFile(),
           "the license files are installed with the programs");
    auto state = Installed::load (paths.stateFile);
    check (state.items["Glitch-Cat"] == rel.commit && state.items.size() == 4 && state.build["Glitch-Cat"] == "build-41"
           && state.where["Glitch-Cat"] == paths.vst3.getFullPathName() && state.when["Glitch-Cat"].isNotEmpty(),
           "remembers what it installed: which build, where, when");
    check (sizeOnDisk (locate (*findItem ("Glitch-Cat"), state, paths)) > 0, "knows how big it is on disk");
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

    // a folder of your own: installed there, found there, the old copy gone
    {
        auto moved = paths;
        moved.vst3 = dir.getChildFile ("root/MyPlugins");
        Job job (Job::Install, { "Hum-Cat" }, rel2, moved);
        job.localSource = src;
        job.startThread();
        while (! job.finished.load()) Thread::sleep (20);
        job.stopThread (1000);
        const auto st = Installed::load (paths.stateFile);
        check (moved.vst3.getChildFile ("Hum-Cat.vst3").isDirectory() && ! paths.vst3.getChildFile ("Hum-Cat.vst3").exists()
                   && stateOf (*findItem ("Hum-Cat"), st, rel2, paths) == State::Installed,
               "installs into a folder of your own (and moves it there), and finds it there");
    }

    // remove
    const auto [ok3, st3] = runJob (Job::Uninstall, { "Hum-Cat", "app:Cat-Hub" }, rel2);
    state = Installed::load (paths.stateFile);
    check (ok3 && ! dir.getChildFile ("root/MyPlugins/Hum-Cat.vst3").exists() && ! paths.apps.getChildFile ("Cat-Hub.exe").exists()
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
        c->setSize (1100, 800);
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
        // the list view, with the older things shown (and two earlier builds)
        Settings s = Settings::load (paths.settingsFile);
        s.showLegacy = true;
        s.listView = true;
        s.save (paths.settingsFile);
        auto c2 = std::make_unique<SetupComponent> (paths, true);
        c2->showRelease (rel2);
        auto b1 = rel2; b1.tag = "build-41"; b1.change = "Cat Loop: a 5-track live looper"; b1.built = "2026-11-09 13:20 UTC";
        auto b2 = rel2; b2.tag = "build-40"; b2.change = "Vortex: a new effect"; b2.built = "2026-11-08 09:05 UTC";
        c2->showBuilds ({ b1, b2 });
        c2->setSize (1100, 800);
        if (SystemStats::getEnvironmentVariable ("CAT_SETUP_TALL", {}).isNotEmpty())
        {
            c2->setSize (1100, 3200);
            auto tall = c2->createComponentSnapshot (c2->getLocalBounds());
            FileOutputStream os (dir.getChildFile ("setup-tall.png"));
            png.writeImageToStream (tall, os);
            c2->setSize (1100, 800);
        }
        auto img3 = c2->createComponentSnapshot (c2->getLocalBounds());
        {
            FileOutputStream os (dir.getChildFile ("setup-list.png"));
            png.writeImageToStream (img3, os);
        }
        s.listView = false;
        s.save (paths.settingsFile);
        check (Settings::load (paths.settingsFile).showLegacy && ! Settings::load (paths.settingsFile).listView, "the settings are saved and read back");
        check (dir.getChildFile ("setup.png").getSize() > 1000 && dir.getChildFile ("setup-done.png").getSize() > 1000 && dir.getChildFile ("setup-list.png").getSize() > 1000,
               "draws the window (cards, all set, the list with the older builds)");
    }

    log << (fails == 0 ? "ALL PASSED" : String (fails) + " FAILED") << "\n";
    out.replaceWithText (log);
    std::printf ("%s\n", fails == 0 ? "SETUP CHECK PASSED" : "SETUP CHECK FAILED");
    return fails == 0 ? 0 : 1;
}

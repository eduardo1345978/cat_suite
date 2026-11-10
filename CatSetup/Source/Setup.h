#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <map>
#include <vector>
#include <atomic>
#include <functional>

/**
    Cat Suite Setup: installs, updates and removes the Cats.

    Everything comes from the GitHub release "latest" of eduardo1345978/cat_suite:
        Cat-Suite-Windows.zip       VST3/<Name>.vst3 for every Cat, MTS-ESP/ (the MTS-ESP library installer)
        Cat-Sound-Apps-Windows.zip  Apps/<Name>.exe (Cat Hub, Cat Ocean, Cat Box, the sound Cats as apps), Hub Cats/
        Cat-Suite-Setup.exe         this program (it updates itself)
    The release says which commit it was built from; Setup remembers per Cat which commit it installed
    (<local app data>/Cat Suite/installed.json), so it knows what is out of date. Downloads are checked
    against GitHub's SHA-256 of every file.

    No administrator rights: VST3s go to <local app data>/Programs/Common/VST3 (Cubase finds them there),
    apps to <local app data>/Programs/Cat Suite, with Start menu (and Desktop) shortcuts.
*/
namespace setup
{
enum class Kind { Plugin, App, Extra };

struct Item
{
    juce::String id;          // = the file's name: "Glitch-Cat" -> Glitch-Cat.vst3 / Glitch-Cat.exe
    juce::String name, blurb;
    Kind kind;
    juce::uint32 colour;
    bool appVersion = false;  // an app (exe) - its id is the exe name
};
const std::vector<Item>& catalog();
const Item* findItem (const juce::String& id);

inline const char* const suiteZip = "Cat-Suite-Windows.zip";
inline const char* const appsZip = "Cat-Sound-Apps-Windows.zip";
inline const char* const setupExe = "Cat-Suite-Setup.exe";
inline const char* const mtsId = "MTS-ESP";

/** Where everything goes. */
struct Paths
{
    juce::File vst3, apps, stateFile, startMenu, desktop, temp;
    static Paths forUser();
    static Paths under (const juce::File& root);   // (tests)
};

//==============================================================================
struct Release
{
    struct Asset { juce::String name, url, sha256; juce::int64 size = 0; };
    bool ok = false;
    juce::String error, commit, title, built;
    std::vector<Asset> assets;
    const Asset* find (const juce::String& name) const;
};
/** The GitHub API's answer for a release -> Release. */
Release parseRelease (const juce::var& json);
/** Asks GitHub for the newest build (blocking, a few seconds). */
Release fetchRelease();

struct Installed
{
    std::map<juce::String, juce::String> items;   // id -> commit (empty = installed some other way, version unknown)
    juce::String lastCheckDay;
    static Installed load (const juce::File&);
    bool save (const juce::File&) const;
};

enum class State { NotInstalled, Installed, Update };
/** Installed is what is on disk (also from the old .bat installers), Update = on disk but not the newest. */
State stateOf (const Item&, const Installed&, const Release&, const Paths&);
bool isOnDisk (const Item&, const Paths&);

/** Programs that must be closed first (Cubase for plug-ins, the app itself for apps). Windows only; empty elsewhere. */
juce::StringArray runningBlockers (const std::vector<juce::String>& ids);

/** This program's own commit (set by the build), empty when built by hand. */
juce::String ownCommit();

//==============================================================================
/** Installs / uninstalls on a background thread. */
class Job : public juce::Thread
{
public:
    enum Action { Install, Uninstall };
    Job (Action, std::vector<juce::String> ids, Release, Paths);
    ~Job() override;

    /** For tests: take the zips from this folder instead of downloading them. */
    juce::File localSource;

    std::atomic<float> progress { 0.0f };
    std::atomic<bool> finished { false }, failed { false }, waitingForClose { false };
    juce::String getStatus() const { const juce::ScopedLock l (lock); return status; }
    juce::StringArray done;                         // what was installed / removed (read when finished)
    /** While it runs (any thread): the id it works on now, and what is finished so far. */
    juce::String getCurrent() const { const juce::ScopedLock l (lock); return current; }
    juce::StringArray getDone() const { const juce::ScopedLock l (lock); return done; }

private:
    void run() override;
    void setStatus (const juce::String&);
    bool download (const Release::Asset&, const juce::File&, float from, float to);
    bool getZip (const juce::String& name, juce::File& unpacked, float from, float to);
    bool installItem (const Item&, const juce::File& suite, const juce::File& apps, Installed&);
    bool uninstallItem (const Item&, Installed&);
    void shortcut (const juce::File& target, const juce::String& name, bool desktop);
    void installSelf();

    Action action;
    std::vector<juce::String> ids;
    Release release;
    Paths paths;
    juce::CriticalSection lock;
    juce::String status, current;
    void setCurrent (const juce::String& id) { const juce::ScopedLock l (lock); current = id; }
    void addDone (const juce::String& id) { const juce::ScopedLock l (lock); done.add (id); }
};

/** Start with Windows to check for updates (a "Run" entry for this user). */
void setCheckAtLogin (bool on, const juce::File& setupExe);
bool checksAtLogin();
} // namespace setup

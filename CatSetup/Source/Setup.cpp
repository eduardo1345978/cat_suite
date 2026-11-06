#include "Setup.h"
#include <juce_cryptography/juce_cryptography.h>

#ifndef CAT_COMMIT
 #define CAT_COMMIT ""
#endif

namespace setup
{
using namespace juce;

const std::vector<Item>& catalog()
{
    static const std::vector<Item> items {
        { "Chord-Cat",  "Chord-Cat",  "Chords and progressions",           Kind::Plugin, 0xffff8a2a },
        { "Patt-Cat",   "Patt-Cat",   "Patterns and arpeggios",            Kind::Plugin, 0xffffd23f },
        { "Bend-Cat",   "Bend-Cat",   "Pitch bends drawn as curves",       Kind::Plugin, 0xff9db4ff },
        { "Drum-Cat",   "Drum-Cat",   "Drum grooves",                      Kind::Plugin, 0xffd8452c },
        { "Orbit-Cat",  "Orbit-Cat",  "Planets that play notes",           Kind::Plugin, 0xffffd27a },
        { "Sing-Cat",   "Sing-Cat",   "A singing choir",                   Kind::Plugin, 0xffff3b4e },
        { "Bass-Cat",   "Bass-Cat",   "Acid and bass lines",               Kind::Plugin, 0xffc1272d },
        { "Glide-Cat",  "Glide-Cat",  "Gliding poly synth",                Kind::Plugin, 0xffff9c3a },
        { "Talk-Cat",   "Talk-Cat",   "A singer with lyrics",              Kind::Plugin, 0xff7dff8a },
        { "Life-Cat",   "Life-Cat",   "A living ecosystem that plays",     Kind::Plugin, 0xff5cffd6 },
        { "Muse-Cat",   "Muse-Cat",   "Writes whole songs",                Kind::Plugin, 0xff4fd1ff },
        { "Echo-Cat",   "Echo-Cat",   "Echo and shimmer",                  Kind::Plugin, 0xffc9561a },
        { "Micro-Cat",  "Micro-Cat",  "Microtonal tunings",                Kind::Plugin, 0xffd8b169 },
        { "Phase-Cat",  "Phase-Cat",  "Drifting, phasing rhythms",         Kind::Plugin, 0xffb88cff },
        { "Hum-Cat",    "Hum-Cat",    "Sing a tune, get a song",           Kind::Plugin, 0xffff6f4f },
        { "Glitch-Cat", "Glitch-Cat", "Every note falls apart into bugs",  Kind::Plugin, 0xff3dff9a },
        { "Molten",     "Molten",     "The sound melts (warm and fat)",    Kind::Plugin, 0xffff6a1f },
        { "Swarm",      "Swarm",      "The sound becomes living star dust", Kind::Plugin, 0xffb9a6ff },
        { "app:Cat-Hub",   "Cat Hub",   "Every Cat jamming together",       Kind::App, 0xff8fa3f0, true },
        { "app:Cat-Ocean", "Cat Ocean", "The endless ocean for YouTube",    Kind::App, 0xff2ce8f5, true },
        { "app:Cat-Box",   "Cat Box",   "The groovebox",                    Kind::App, 0xffd9dde3, true },
        { "app:Sing-Cat",  "Sing-Cat app",  "Play the choir on your keyboard", Kind::App, 0xffff3b4e, true },
        { "app:Talk-Cat",  "Talk-Cat app",  "Make it sing your words",         Kind::App, 0xff7dff8a, true },
        { "app:Glide-Cat", "Glide-Cat app", "The glide synth on its own",      Kind::App, 0xffff9c3a, true },
        { "app:Life-Cat",  "Life-Cat app",  "Watch the ecosystem play",        Kind::App, 0xff5cffd6, true },
        { "app:Muse-Cat",  "Muse-Cat app",  "Write songs without Cubase",      Kind::App, 0xff4fd1ff, true },
        { mtsId, "MTS-ESP library", "Lets Micro-Cat tune Vital, Surge, Serum ... (by ODDSound)", Kind::Extra, 0xffb5a98e },
    };
    return items;
}

const Item* findItem (const String& id)
{
    for (auto& i : catalog()) if (i.id == id) return &i;
    return nullptr;
}

static String fileNameOf (const Item& i) { return i.id.fromFirstOccurrenceOf ("app:", false, false).isNotEmpty() ? i.id.fromFirstOccurrenceOf ("app:", false, false) : i.id; }

//==============================================================================
Paths Paths::forUser()
{
    Paths p;
   #if JUCE_WINDOWS
    const auto local = File::getSpecialLocation (File::windowsLocalAppData);
    p.vst3 = local.getChildFile ("Programs").getChildFile ("Common").getChildFile ("VST3");
    p.apps = local.getChildFile ("Programs").getChildFile ("Cat Suite");
    p.stateFile = local.getChildFile ("Cat Suite").getChildFile ("installed.json");
    p.startMenu = File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Microsoft\\Windows\\Start Menu\\Programs\\Cat Suite");
   #else
    const auto home = File::getSpecialLocation (File::userHomeDirectory);
    p.vst3 = home.getChildFile (".vst3");
    p.apps = home.getChildFile (".local/share/Cat Suite/apps");
    p.stateFile = home.getChildFile (".local/share/Cat Suite/installed.json");
    p.startMenu = home.getChildFile (".local/share/Cat Suite/menu");
   #endif
    p.desktop = File::getSpecialLocation (File::userDesktopDirectory);
    p.temp = File::getSpecialLocation (File::tempDirectory).getChildFile ("Cat Suite Setup");
    return p;
}

Paths Paths::under (const File& root)
{
    Paths p;
    p.vst3 = root.getChildFile ("VST3");
    p.apps = root.getChildFile ("Apps");
    p.stateFile = root.getChildFile ("state/installed.json");
    p.startMenu = root.getChildFile ("Menu");
    p.desktop = root.getChildFile ("Desktop");
    p.temp = root.getChildFile ("temp");
    return p;
}

//==============================================================================
const Release::Asset* Release::find (const String& name) const
{
    for (auto& a : assets) if (a.name == name) return &a;
    return nullptr;
}

Release parseRelease (const var& json)
{
    Release r;
    if (! json.isObject()) { r.error = "GitHub sent something unexpected"; return r; }
    if (json.hasProperty ("message") && ! json.hasProperty ("assets")) { r.error = "GitHub: " + json["message"].toString(); return r; }
    r.title = json["name"].toString();
    const auto body = json["body"].toString();
    // "Built from commit <40 hex>"
    const auto at = body.indexOf ("Built from commit ");
    if (at >= 0)
        r.commit = body.substring (at + 18).initialSectionContainingOnly ("0123456789abcdef");
    if (r.commit.length() < 7) r.commit = json["target_commitish"].toString();
    // the build time from the title: "Cat Suite - latest build (2026-11-05 15:20 UTC)"
    r.built = r.title.fromFirstOccurrenceOf ("(", false, false).upToFirstOccurrenceOf (")", false, false);
    if (r.built.isEmpty()) r.built = json["published_at"].toString();
    if (auto* arr = json["assets"].getArray())
        for (auto& a : *arr)
        {
            Release::Asset as;
            as.name = a["name"].toString();
            as.url = a["browser_download_url"].toString();
            as.size = (int64) a["size"];
            const auto d = a["digest"].toString();
            if (d.startsWith ("sha256:")) as.sha256 = d.substring (7).toLowerCase();
            // only ever from GitHub
            if (as.name.isNotEmpty() && as.url.startsWith ("https://github.com/")) r.assets.push_back (as);
        }
    r.ok = r.commit.isNotEmpty() && r.find (suiteZip) != nullptr;
    if (! r.ok && r.error.isEmpty()) r.error = "the newest build isn't complete on GitHub yet - try again in a few minutes";
    return r;
}

Release fetchRelease()
{
    const URL url ("https://api.github.com/repos/eduardo1345978/cat_suite/releases/tags/latest");
    int status = 0;
    auto stream = url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                             .withExtraHeaders ("User-Agent: Cat-Suite-Setup\r\nAccept: application/vnd.github+json")
                                             .withConnectionTimeoutMs (15000)
                                             .withStatusCode (&status)
                                             .withNumRedirectsToFollow (5));
    if (stream == nullptr) { Release r; r.error = "no connection to GitHub (are you online?)"; return r; }
    const auto text = stream->readEntireStreamAsString();
    if (status != 0 && status != 200)
    {
        Release r;
        r.error = status == 403 ? "GitHub says: too many questions from this network - try again in an hour" : "GitHub answered " + String (status);
        return r;
    }
    return parseRelease (JSON::parse (text));
}

//==============================================================================
Installed Installed::load (const File& f)
{
    Installed s;
    const auto v = JSON::parse (f.loadFileAsString());
    if (auto* o = v["items"].getDynamicObject())
        for (auto& p : o->getProperties()) s.items[p.name.toString()] = p.value.toString();
    s.lastCheckDay = v["lastCheckDay"].toString();
    return s;
}

bool Installed::save (const File& f) const
{
    auto* items_ = new DynamicObject();
    for (auto& [k, v] : items) items_->setProperty (k, v);
    auto* root = new DynamicObject();
    root->setProperty ("items", var (items_));
    root->setProperty ("lastCheckDay", lastCheckDay);
    f.getParentDirectory().createDirectory();
    return f.replaceWithText (JSON::toString (var (root)));
}

bool isOnDisk (const Item& i, const Paths& p)
{
    switch (i.kind)
    {
        case Kind::Plugin: return p.vst3.getChildFile (i.id + ".vst3").exists();
        case Kind::App:    return p.apps.getChildFile (fileNameOf (i) + ".exe").existsAsFile();
        case Kind::Extra:
           #if JUCE_WINDOWS
            return File (SystemStats::getEnvironmentVariable ("CommonProgramFiles", "C:\\Program Files\\Common Files")).getChildFile ("MTS-ESP\\LIBMTS.dll").existsAsFile();
           #else
            return false;
           #endif
    }
    return false;
}

State stateOf (const Item& i, const Installed& s, const Release& r, const Paths& p)
{
    if (! isOnDisk (i, p)) return State::NotInstalled;
    if (i.kind == Kind::Extra) return State::Installed;          // not ours to update
    const auto it = s.items.find (i.id);
    if (! r.ok) return State::Installed;                          // offline: can't tell
    if (it == s.items.end() || it->second != r.commit) return State::Update;
    return State::Installed;
}

juce::StringArray runningBlockers (const std::vector<String>& ids)
{
    StringArray out;
   #if JUCE_WINDOWS
    ChildProcess p;
    if (! p.start ("tasklist /FO CSV /NH", ChildProcess::wantStdOut)) return out;
    const auto text = p.readAllProcessOutput().toLowerCase();
    p.waitForProcessToFinish (5000);
    bool plugins = false;
    for (auto& id : ids)
    {
        const auto* i = findItem (id);
        if (i == nullptr) continue;
        if (i->kind == Kind::Plugin) plugins = true;
        if (i->kind == Kind::App && text.contains ("\"" + fileNameOf (*i).toLowerCase() + ".exe\"")) out.addIfNotAlreadyThere (i->name);
    }
    if (plugins)
    {
        if (text.contains ("\"cubase")) out.addIfNotAlreadyThere ("Cubase");
        if (text.contains ("\"cat-hub.exe\"")) out.addIfNotAlreadyThere ("Cat Hub");
    }
   #else
    ignoreUnused (ids);
   #endif
    return out;
}

String ownCommit() { return CAT_COMMIT; }

//==============================================================================
Job::Job (Action a, std::vector<String> i, Release r, Paths p)
    : Thread ("Cat Suite Setup"), action (a), ids (std::move (i)), release (std::move (r)), paths (std::move (p)) {}

Job::~Job() { stopThread (10000); }

void Job::setStatus (const String& s) { const ScopedLock l (lock); status = s; }

bool Job::download (const Release::Asset& asset, const File& file, float from, float to)
{
    const auto& name = asset.name;
    setStatus ("Downloading " + name + " ...");
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    const auto mb = String ((double) asset.size / 1048576.0, 0);
    auto stream = URL (asset.url).createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                                         .withExtraHeaders ("User-Agent: Cat-Suite-Setup")
                                                         .withConnectionTimeoutMs (20000)
                                                         .withNumRedirectsToFollow (8));
    if (stream == nullptr) { setStatus ("Couldn't download " + name + " - are you online?"); return false; }
    int64 got = 0;
    {
        FileOutputStream out (file);
        if (! out.openedOk()) { setStatus ("Couldn't write " + file.getFullPathName()); return false; }
        HeapBlock<char> buf (1 << 16);
        while (! threadShouldExit())
        {
            const int n = stream->read (buf.get(), 1 << 16);
            if (n <= 0) break;
            out.write (buf.get(), (size_t) n);
            got += n;
            if (asset.size > 0)
            {
                progress = from + (to - from) * (float) jmin (1.0, (double) got / (double) asset.size);
                setStatus ("Downloading " + name + "  " + String ((double) got / 1048576.0, 0) + " / " + mb + " MB");
            }
        }
    }
    if (threadShouldExit()) { file.deleteFile(); return false; }
    if (asset.size > 0 && got != asset.size) { file.deleteFile(); setStatus ("The download of " + name + " broke off - try again"); return false; }
    // the file must be exactly the one GitHub built
    if (asset.sha256.isNotEmpty())
    {
        setStatus ("Checking " + name + " ...");
        if (SHA256 (file).toHexString().toLowerCase() != asset.sha256)
        {
            file.deleteFile();
            setStatus (name + " didn't arrive intact (checksum) - nothing was changed. Try again.");
            return false;
        }
    }
    return true;
}

bool Job::getZip (const String& name, File& unpacked, float from, float to)
{
    File zip;
    if (localSource != File())
        zip = localSource.getChildFile (name);
    else
    {
        const auto* asset = release.find (name);
        if (asset == nullptr) { setStatus ("The newest build has no " + name); return false; }
        zip = paths.temp.getChildFile (name);
        if (! download (*asset, zip, from, from + (to - from) * 0.8f)) return false;
    }
    if (! zip.existsAsFile()) { setStatus ("Missing " + name); return false; }
    setStatus ("Unpacking " + name + " ...");
    unpacked = paths.temp.getChildFile (File::createLegalFileName (name + " unpacked"));
    unpacked.deleteRecursively();
    unpacked.createDirectory();
    ZipFile z (zip);
    // never write outside the folder (a zip entry like ..\..\Windows\x)
    for (int i = 0; i < z.getNumEntries(); ++i)
        if (auto* e = z.getEntry (i); e != nullptr && (e->filename.contains ("../") || e->filename.contains ("..\\") || e->filename.startsWith ("..") || File::isAbsolutePath (e->filename)))
        {
            setStatus (name + " has a strange file in it - stopped for safety");
            return false;
        }
    const auto res = z.uncompressTo (unpacked, true);
    if (localSource == File()) zip.deleteFile();
    progress = to;
    if (res.failed()) { setStatus ("Couldn't unpack " + name + ": " + res.getErrorMessage()); return false; }
    return true;
}

void Job::shortcut (const File& target, const String& name, bool alsoDesktop)
{
   #if JUCE_WINDOWS
    paths.startMenu.createDirectory();
    target.createShortcut (name, paths.startMenu.getChildFile (name + ".lnk"));
    if (alsoDesktop) target.createShortcut (name, paths.desktop.getChildFile (name + ".lnk"));
   #else
    paths.startMenu.createDirectory();
    paths.startMenu.getChildFile (name + ".link").replaceWithText (target.getFullPathName());
    if (alsoDesktop) { paths.desktop.createDirectory(); paths.desktop.getChildFile (name + ".link").replaceWithText (target.getFullPathName()); }
   #endif
}

static File linkFile (const File& folder, const String& name)
{
   #if JUCE_WINDOWS
    return folder.getChildFile (name + ".lnk");
   #else
    return folder.getChildFile (name + ".link");
   #endif
}

bool Job::installItem (const Item& i, const File& suite, const File& apps, Installed& state)
{
    switch (i.kind)
    {
        case Kind::Plugin:
        {
            const auto src = suite.getChildFile ("VST3").getChildFile (i.id + ".vst3");
            if (! src.isDirectory()) { setStatus (i.name + " is not in this build"); return false; }
            const auto dst = paths.vst3.getChildFile (i.id + ".vst3");
            paths.vst3.createDirectory();
            dst.deleteRecursively();
            if (! src.copyDirectoryTo (dst)) { setStatus ("Couldn't copy " + i.name + " - is Cubase still open?"); return false; }
            break;
        }
        case Kind::App:
        {
            const auto exe = fileNameOf (i) + ".exe";
            const auto src = apps.getChildFile ("Apps").getChildFile (exe);
            if (! src.existsAsFile()) { setStatus (i.name + " is not in this build"); return false; }
            paths.apps.createDirectory();
            const auto dst = paths.apps.getChildFile (exe);
            if (! src.copyFileTo (dst)) { setStatus ("Couldn't copy " + i.name + " - is it still open?"); return false; }
            if (i.id == "app:Cat-Hub")                     // the Hub plays its own copies of the Cats
            {
                const auto hubCats = apps.getChildFile ("Hub Cats");
                if (hubCats.isDirectory())
                {
                    paths.apps.getChildFile ("Hub Cats").deleteRecursively();
                    hubCats.copyDirectoryTo (paths.apps.getChildFile ("Hub Cats"));
                }
            }
            shortcut (dst, i.name.replace (" app", ""), i.id == "app:Cat-Hub" || i.id == "app:Cat-Ocean" || i.id == "app:Cat-Box");
            break;
        }
        case Kind::Extra:
        {
            const auto exe = suite.getChildFile ("MTS-ESP").findChildFiles (File::findFiles, false, "libMTSWin*.exe");
            if (exe.isEmpty()) { setStatus ("The MTS-ESP installer is not in this build"); return false; }
            // its own installer (it asks Windows for permission); copy it out of the temp folder first
            const auto keep = paths.apps.getChildFile (exe[0].getFileName());
            paths.apps.createDirectory();
            exe[0].copyFileTo (keep);
           #if JUCE_WINDOWS
            keep.startAsProcess();
           #endif
            break;
        }
    }
    state.items[i.id] = release.commit;
    return true;
}

bool Job::uninstallItem (const Item& i, Installed& state)
{
    bool ok = true;
    switch (i.kind)
    {
        case Kind::Plugin:
            ok = ! paths.vst3.getChildFile (i.id + ".vst3").exists() || paths.vst3.getChildFile (i.id + ".vst3").deleteRecursively();
            break;
        case Kind::App:
        {
            const auto exe = paths.apps.getChildFile (fileNameOf (i) + ".exe");
            ok = ! exe.exists() || exe.deleteFile();
            if (i.id == "app:Cat-Hub") paths.apps.getChildFile ("Hub Cats").deleteRecursively();
            const auto n = i.name.replace (" app", "");
            linkFile (paths.startMenu, n).deleteFile();
            linkFile (paths.desktop, n).deleteFile();
            break;
        }
        case Kind::Extra:
            setStatus ("The MTS-ESP library is removed in Windows Settings > Apps");
            return true;
    }
    if (ok) state.items.erase (i.id);
    else setStatus ("Couldn't remove " + i.name + " - close Cubase and the Cat apps first");
    return ok;
}

void Job::installSelf()
{
    // keep Setup next to the apps (Start menu: "Cat Suite Setup") so updating is one click later
    paths.apps.createDirectory();
    const auto me = File::getSpecialLocation (File::currentExecutableFile);
    const auto dst = paths.apps.getChildFile (setupExe);
    const auto* asset = release.find (setupExe);
    const bool newer = asset != nullptr && ownCommit().isNotEmpty() && ownCommit() != release.commit && localSource == File();
    if (newer)
    {
        // a newer Setup is out: download it (the running one may be the one in Programs - Windows lets a running exe be renamed)
        const auto fresh = paths.temp.getChildFile (setupExe);
        if (download (*asset, fresh, progress.load(), progress.load()))
        {
            const auto old = dst.getSiblingFile ("Cat-Suite-Setup.old.exe");
            old.deleteFile();
            if (dst.exists()) dst.moveFileTo (old);
            fresh.moveFileTo (dst);
        }
    }
    else if (me != dst && me.getFileName().endsWithIgnoreCase (".exe"))
    {
        dst.deleteFile();
        me.copyFileTo (dst);
    }
    if (dst.existsAsFile()) shortcut (dst, "Cat Suite Setup", false);
}

void Job::run()
{
    auto state = Installed::load (paths.stateFile);
    std::vector<const Item*> items;
    for (auto& id : ids) if (auto* i = findItem (id)) items.push_back (i);

    if (action == Uninstall)
    {
        for (;;)
        {
            const auto blockers = runningBlockers (ids);
            if (blockers.isEmpty() || threadShouldExit()) break;
            waitingForClose = true;
            setStatus ("Please close " + blockers.joinIntoString (", ") + " - it continues by itself");
            Thread::sleep (1000);
        }
        waitingForClose = false;
        bool ok = true;
        for (size_t k = 0; k < items.size(); ++k)
        {
            setStatus ("Removing " + items[k]->name + " ...");
            if (uninstallItem (*items[k], state)) done.add (items[k]->id); else ok = false;
            progress = (float) (k + 1) / (float) items.size();
        }
        state.save (paths.stateFile);
        if (ok) setStatus ("Removed " + String (done.size()) + (done.size() == 1 ? " item." : " items."));
        failed = ! ok;
        finished = true;
        return;
    }

    bool needSuite = false, needApps = false;
    for (auto* i : items) { needSuite |= i->kind != Kind::App; needApps |= i->kind == Kind::App; }
    File suite, apps;
    const float split = needSuite && needApps ? 0.45f : 0.9f;
    if (needSuite && ! getZip (suiteZip, suite, 0.0f, needApps ? split : 0.9f)) { failed = true; finished = true; return; }
    if (needApps && ! getZip (appsZip, apps, needSuite ? split : 0.0f, 0.9f)) { failed = true; finished = true; return; }

    // Cubase (and the apps being replaced) must be closed
    for (;;)
    {
        const auto blockers = runningBlockers (ids);
        if (blockers.isEmpty() || threadShouldExit()) break;
        waitingForClose = true;
        setStatus ("Please close " + blockers.joinIntoString (", ") + " - Setup goes on by itself");
        Thread::sleep (1000);
    }
    waitingForClose = false;
    if (threadShouldExit()) { finished = true; return; }

    bool ok = true;
    for (size_t k = 0; k < items.size(); ++k)
    {
        setStatus ("Installing " + items[k]->name + " ...");
        if (installItem (*items[k], suite, apps, state)) done.add (items[k]->id); else ok = false;
        progress = 0.9f + 0.09f * (float) (k + 1) / (float) items.size();
    }
    state.save (paths.stateFile);
    // the licenses go with the programs
    for (auto& from : { suite, apps })
        if (from.isDirectory())
        {
            paths.apps.createDirectory();
            for (auto name : { "LICENSE.txt", "THIRD-PARTY-NOTICES.txt" })
                if (from.getChildFile (name).existsAsFile()) from.getChildFile (name).copyFileTo (paths.apps.getChildFile (name));
            if (from.getChildFile ("licenses").isDirectory())
            {
                paths.apps.getChildFile ("licenses").deleteRecursively();
                from.getChildFile ("licenses").copyDirectoryTo (paths.apps.getChildFile ("licenses"));
            }
        }
    installSelf();
    paths.temp.deleteRecursively();
    progress = 1.0f;
    if (ok) setStatus ("Done - " + String (done.size()) + (done.size() == 1 ? " item is" : " items are") + " up to date.");
    failed = ! ok;
    finished = true;
}

//==============================================================================
static const char* const runKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\Cat Suite Setup";

void setCheckAtLogin (bool on, const File& exe)
{
   #if JUCE_WINDOWS
    if (on) WindowsRegistry::setValue (runKey, "\"" + exe.getFullPathName() + "\" --check");
    else WindowsRegistry::deleteValue (runKey);
   #else
    ignoreUnused (on, exe);
   #endif
}

bool checksAtLogin()
{
   #if JUCE_WINDOWS
    return WindowsRegistry::valueExists (runKey);
   #else
    return false;
   #endif
}
} // namespace setup

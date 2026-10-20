// Cat Ocean: the admin console in the app ("/help" lists the commands).
#include "OceanSim.h"
#include "OceanFilter.h"
#include <algorithm>
#include <cmath>

namespace ocean
{
float Sim::daylightAt (float h)
{
    h = std::fmod (h + 24.0f, 24.0f);
    if (h < 5.0f || h >= 21.5f) return 0.0f;      // night
    if (h < 7.0f) return (h - 5.0f) / 2.0f;       // sunrise
    if (h < 19.0f) return 1.0f;                    // day
    return 1.0f - (h - 19.0f) / 2.5f;              // sunset
}

juce::String Sim::admin (const juce::String& line)
{
    auto text = line.trim();
    if (text.startsWithChar ('/') || text.startsWithChar ('!')) text = text.substring (1);
    juce::StringArray tok, raw;
    raw.addTokens (text, " ,", "");
    raw.removeEmptyStrings();
    if (raw.isEmpty()) return "type /help";
    for (auto& r : raw) tok.add (r.toLowerCase());
    const auto cmd = tok[0];
    tok.remove (0);
    raw.remove (0);
    const juce::String rest = raw.joinIntoString (" ");

    auto fishFor = [this] (const juce::String& who) -> Fish* { return fishOf (findViewer (who)); };
    auto describe = [this] (const Fish& f)
    {
        juce::String s;
        s << f.name << (f.viewer() ? " (" + f.ownerName + ")" : juce::String (" (wild)")) << ": LV" << f.level() << " " << f.xp << " XP, gen "
          << f.generation << ", " << f.babies << " babies, " << colourName (f.genes.colour1) << " " << bodyNames()[f.genes.body] << " "
          << patternNames()[f.genes.pattern] << ", voice " << voiceNames()[f.genes.voice] << ", melody " << f.genes.melodyString()
          << (f.genes.rare != RareNone ? ", " + rareNames()[f.genes.rare] : juce::String())
          << (species (f.genes.species) != nullptr ? ", species " + species (f.genes.species)->name : juce::String())
          << (f.present ? ", swimming" : ", asleep in the cave");
        return s;
    };

    if (cmd == "help" || cmd == "?")
        return "/stats  /fish @name  /viewers  /spawn 5  /feed 10  /give @name 50  /level @name 8  /rare @name gold|rainbow|ghost|glow|crystal|none\n"
               "/weather calm|rain|storm  /time day|night|sunset|sunrise|auto|0-24  /place reef|kelp|crystal|ship|cave|ice|lagoon|ruins\n"
               "/music aqua|lounge|crystal|bubble|...  /event storm|calm|feast|babyboom|rarehunt|plankton|migration|meteors  /vote start|end  /announce text  /party\n"
               "/today  /recap (show today's recap)  /testalert member|superchat|milestone  /cat (how the boat cat is)  /catvisit\n"
               "/species  /newspecies  /kick @name  /ban @name  /unban @name  /bans  /favourites  /requests  /clearrequests  /rename @name New Name  /resetname @name\n"
               "/sleepall  /wakeall  /clear food|bubbles|speech  /save   (chat commands like !fish also work here, as the host)";

    if (cmd == "stats")
    {
        int present = 0, viewersHere = 0, rare = 0;
        for (auto& f : fish) { if (f.present && ! f.dying) ++present; if (f.viewer() && f.present) ++viewersHere; if (f.genes.rare != RareNone) ++rare; }
        static const char* wn[] = { "calm", "rain", "storm" };
        juce::String s;
        s << present << " fish swimming (" << wildCount() << " wild, " << viewersHere << " viewers'), " << viewers.size() << " viewers ever, "
          << rare << " rare fish\n" << births << " babies, " << mutations << " mutations, " << speciesList.size() << " species, "
          << banned.size() << " banned\nplace " << placeName (place) << ", music " << styleName (style) << ", weather " << wn[juce::jlimit (0, 2, weather)]
          << ", " << (timeOverride >= 0 ? "time fixed at " + juce::String (timeOverride, 1) + "h" : juce::String ("real time"))
          << ", daylight " << juce::String ((int) std::lround (daylight * 100)) << "%, ocean age " << juce::String ((int) (time / 3600.0)) << "h";
        return s;
    }
    if (cmd == "fish" || cmd == "who")
    {
        if (raw.isEmpty()) return "/fish @name";
        if (auto* f = fishFor (raw[0])) return describe (*f);
        for (auto& f : fish) if (f.name.equalsIgnoreCase (rest)) return describe (f);
        return "no fish called " + rest;
    }
    if (cmd == "viewers")
    {
        juce::StringArray here;
        for (auto& f : fish) if (f.viewer() && f.present) here.add (f.name + " LV" + juce::String (f.level()));
        return here.isEmpty() ? juce::String ("no viewers' fish swimming") : juce::String (here.size()) + " here: " + here.joinIntoString (", ");
    }
    if (cmd == "spawn")
    {
        const int n = juce::jlimit (1, 20, tok.isEmpty() ? 1 : tok[0].getIntValue());
        for (int i = 0; i < n; ++i) spawnWild (true);
        return juce::String (n) + " wild fish swim in";
    }
    if (cmd == "feed")
    {
        const int n = juce::jlimit (1, 60, tok.isEmpty() ? 10 : tok[0].getIntValue());
        dropFood (n);
        return juce::String (n) + " food dropped";
    }
    if (cmd == "give" || cmd == "xp")
    {
        if (raw.size() < 2) return "/give @name 50";
        auto* f = fishFor (raw[0]);
        if (f == nullptr) return "no viewer " + raw[0];
        addXp (*f, juce::jlimit (1, 100000, raw[1].getIntValue()));
        return f->name + " is LV" + juce::String (f->level()) + " (" + juce::String (f->xp) + " XP)";
    }
    if (cmd == "level" || cmd == "setlevel")
    {
        if (raw.size() < 2) return "/level @name 8";
        auto* f = fishFor (raw[0]);
        if (f == nullptr) return "no viewer " + raw[0];
        const int target = juce::jlimit (1, 100, raw[1].getIntValue());
        if (xpForLevel (target) > f->xp) addXp (*f, xpForLevel (target) - f->xp);
        else f->xp = xpForLevel (target);
        return f->name + " is LV" + juce::String (f->level());
    }
    if (cmd == "rare")
    {
        if (raw.size() < 2) return "/rare @name gold";
        auto* f = fishFor (raw[0]);
        if (f == nullptr) return "no viewer " + raw[0];
        const auto w = tok[1];
        const int rr = w == "gold" || w == "golden" ? RareGold : w == "rainbow" ? RareRainbow : w == "ghost" ? RareGhost
                     : w == "glow" || w == "glowing" ? RareGlow : w == "crystal" ? RareCrystal : RareNone;
        f->genes.rare = rr;
        if (rr != RareNone) { announce (f->name + " became a " + rareNames()[rr] + " fish!", 0xfffee761); party (30); }
        return f->name + " is now " + (rr == RareNone ? juce::String ("a normal fish") : rareNames()[rr]);
    }
    if (cmd == "weather")
    {
        const auto w = tok.isEmpty() ? juce::String() : tok[0];
        if (w == "calm" || w == "sun" || w == "sunny") { weather = WeatherCalm; weatherTimer = 900.0f; }
        else if (w == "rain") { weather = WeatherRain; weatherTimer = 300.0f; }
        else if (w == "storm") { weather = WeatherStorm; weatherTimer = 180.0f; }
        else return "/weather calm|rain|storm";
        return "weather: " + w;
    }
    if (cmd == "time")
    {
        const auto w = tok.isEmpty() ? juce::String() : tok[0];
        if (w == "auto" || w == "real") timeOverride = -1.0f;
        else if (w == "day" || w == "noon") timeOverride = 12.0f;
        else if (w == "night" || w == "midnight") timeOverride = 0.0f;
        else if (w == "sunset" || w == "evening") timeOverride = 20.2f;
        else if (w == "sunrise" || w == "morning") timeOverride = 6.0f;
        else if (w.containsOnly ("0123456789.") && w.isNotEmpty()) timeOverride = juce::jlimit (0.0f, 23.99f, w.getFloatValue());
        else return "/time day|night|sunset|sunrise|auto|0-24";
        return timeOverride < 0 ? juce::String ("time follows the real clock") : "time fixed at " + juce::String (timeOverride, 1) + "h";
    }
    if (cmd == "place" || cmd == "goto")
    {
        const int p = findPlace (tok.joinIntoString (""));
        if (p < 0) return "/place reef|kelp|crystal|ship|cave|ice|lagoon|ruins";
        goToPlace (p);
        announce ("Off to the " + placeName (p) + "!", 0xff2ce8f5);
        return "going to the " + placeName (p);
    }
    if (cmd == "music" || cmd == "style")
    {
        const int st = findStyle (tok.joinIntoString (""));
        if (st < 0) return "/music aqua|lounge|crystal|bubble";
        styleRequest = st;
        return "music: " + styleName (st);
    }
    if (cmd == "event")
    {
        const int e = findEvent (tok.joinIntoString (""));
        if (e < 0) return "/event storm|calm|feast|babyboom|rarehunt|plankton|migration|meteors";
        announce (eventName (e) + "!", 0xff2ce8f5);
        applyEvent (e);
        return "event: " + eventName (e);
    }
    if (cmd == "vote")
    {
        if (! tok.isEmpty() && tok[0] == "end") { if (vote.active) vote.left = 0.0f; return "the vote ends now"; }
        if (! vote.active) voteTimer = 0.0f;
        return "a vote starts";
    }
    if (cmd == "announce" || cmd == "say")
    {
        if (rest.isEmpty()) return "/announce text";
        announce (rest.substring (0, 110), 0xffffffff);
        return "announced";
    }
    if (cmd == "party") { party (150); return "party!"; }
    if (cmd == "species")
    {
        if (speciesList.empty()) return "no species discovered yet";
        juce::StringArray l;
        for (auto& sp : speciesList)
        {
            int alive = 0;
            for (auto& f : fish) if (f.genes.species == sp.id) ++alive;
            l.add (sp.name + " (" + traitNames()[sp.trait] + ", " + juce::String (alive) + " alive)");
        }
        return l.joinIntoString ("\n");
    }
    if (cmd == "newspecies")
    {
        const int id = discoverSpeciesForTests();
        return id > 0 && species (id) != nullptr ? "discovered: " + species (id)->name : juce::String ("evolution needs at least two fish");
    }
    if (cmd == "favourites" || cmd == "favorites" || cmd == "favs")
    {
        if (favourites.empty()) return "no liked songs yet";
        std::vector<const Favourite*> top;
        for (auto& fv : favourites) top.push_back (&fv);
        std::sort (top.begin(), top.end(), [] (const Favourite* a, const Favourite* b) { return a->likers.size() > b->likers.size(); });
        juce::StringArray lines;
        for (size_t i = 0; i < juce::jmin ((size_t) 15, top.size()); ++i)
            lines.add (juce::String ((int) i + 1) + ". " + top[i]->title + "  (" + styleName (top[i]->style) + ", " + juce::String (top[i]->likers.size()) + " likes)");
        return lines.joinIntoString ("\n");
    }
    if (cmd == "today") return "today: " + dayRecapText (today);
    if (cmd == "recap") { showRecap(); return "showing today's recap for 45 s"; }
    if (cmd == "testalert")
    {
        const auto what = tok.size() < 2 ? juce::String ("member") : tok[1];
        if (what.startsWith ("super")) superChat ("test", "Test Viewer", "$5.00", "this is how a Super Chat looks");
        else if (what.startsWith ("mile")) { celebrateSubscribers (100, 100); return "a milestone alert (100 subscribers) - only a test"; }
        else memberJoined ("test", "Test Viewer", "");
        return "test alert shown";
    }
    if (cmd == "cat")
        return "boat cat: " + juce::String (cat.mood == 1 ? "napping" : "awake") + ", hunger " + juce::String (juce::roundToInt (cat.hunger * 100)) + "%, sleepy "
               + juce::String (juce::roundToInt (cat.sleepy * 100)) + "%, happy " + juce::String (juce::roundToInt (cat.happy * 100)) + "%, caught " + juce::String (cat.catches) + " things";
    if (cmd == "catvisit") { cat.nextVisitor = 0.0f; cat.visitor = VisitorNone; return "a visitor is coming to the boat"; }
    if (cmd == "requests")
    {
        if (requests.empty()) return "no song requests waiting";
        juce::StringArray lines;
        for (auto& rq : requests) lines.add (styleName (rq.style) + " (" + rq.by + ")");
        return "next: " + lines.joinIntoString (", ");
    }
    if (cmd == "clearrequests") { requests.clear(); return "song requests cleared"; }
    if (cmd == "bans")
    {
        const auto b = bannedNames();
        return b.isEmpty() ? juce::String ("nobody is banned") : "banned: " + b.joinIntoString (", ");
    }
    if (cmd == "sleepall")
    {
        int n = 0;
        for (auto& f : fish) if (f.viewer() && f.present) { sendAway (f); ++n; }
        return juce::String (n) + " viewers' fish went to sleep";
    }
    if (cmd == "wakeall")
    {
        int n = 0;
        for (auto& v : viewers) if (auto* f = findFish (v.second.fishId); f != nullptr && ! f->present && n < maxViewerFish) { bringBack (*f); v.second.lastMessage = time; ++n; }
        return juce::String (n) + " viewers' fish woke up";
    }
    if (cmd == "clear")
    {
        const auto w = tok.isEmpty() ? juce::String ("food") : tok[0];
        if (w == "food") food.clear();
        else if (w == "speech" || w == "bubbles") speech.clear();
        else if (w == "news") news.clear();
        else return "/clear food|speech|news";
        return "cleared " + w;
    }
    // the moderator commands work here too
    if (modCommand (cmd, tok, raw))
        return "done: " + cmd + " " + rest;
    return "unknown command - type /help";
}
} // namespace ocean

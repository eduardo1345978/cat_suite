// Cat Ocean's self test (Cat-Ocean --selftest <folder>): chat commands, the word filter, the
// YouTube chat reader, fish genetics, an hour of ocean life, the music, save / load, pictures and a
// test video through ffmpeg (when ffmpeg is installed).
#include <juce_gui_basics/juce_gui_basics.h>
#include "OceanEngine.h"
#include "OceanFilter.h"
#include <cstdio>
#include <set>

using namespace ocean;

namespace
{
int failures = 0;
juce::StringArray report;

void check (bool ok, const juce::String& what)
{
    report.add ((ok ? "ok    " : "FAIL  ") + what);
    std::printf ("%s %s\n", ok ? "ok   " : "FAIL ", what.toRawUTF8());
    std::fflush (stdout);
    if (! ok) ++failures;
}

void savePng (const Canvas& c, const juce::File& f)
{
    juce::Image img (juce::Image::ARGB, W * 2, H * 2, false);
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < H * 2; ++y)
            for (int x = 0; x < W * 2; ++x)
                bd.setPixelColour (x, y, juce::Colour (c.get (x / 2, y / 2)));
    }
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, out);
}

int distinctColours (const Canvas& c)
{
    std::set<juce::uint32> s;
    for (int i = 0; i < W * H; i += 7) s.insert (c.data()[i]);
    return (int) s.size();
}

Fish* fishNamed (Sim& sim, const juce::String& name)
{
    for (auto& f : sim.fish) if (f.name == name) return &f;
    return nullptr;
}

void testFilter()
{
    check (filter::isClean ("Miro") && filter::isClean ("Spice") && filter::isClean ("Picasso") && filter::isClean ("Document")
           , "word filter: normal names pass (Spice, Picasso), rude ones don't");
    check (! filter::isClean ("FuCk") && ! filter::isClean ("sh1t") && ! filter::isClean ("kurva") && ! filter::isClean ("k0k0t")
           && ! filter::isClean ("pica") && ! filter::isClean ("P!CA"), "word filter: catches English and Slovak words, also written with numbers");
    check (filter::cleanName (juce::String::fromUTF8 ("\xc5\xbd" "ofia \xc5\xa0" "tef\xc3\xa1nkov\xc3\xa1"), 1) == "Zofia Stefankova", "names: Slovak letters become plain ones for the pixel font");
    check (filter::cleanName ("fuck you", 5).startsWith ("Fish"), "names: a rude name becomes FishNNN");
    {
        bool allClean = true;
        for (int i = 0; i < 2000; ++i) allClean = allClean && filter::isClean (filter::neutralName (i));
        check (allClean, "names: the neutral FishNNN names are never rude themselves (no Fish127 = fishit)");
    }
    check (filter::cleanName ("ThisNameIsWayTooLongForAFish", 1).length() <= 16, "names: at most 16 letters");
}

void testChatReader()
{
    check (YouTubeChat::videoIdFrom ("https://www.youtube.com/watch?v=dQw4w9WgXcQ&t=10") == "dQw4w9WgXcQ", "chat: video id from a watch link");
    check (YouTubeChat::videoIdFrom ("https://youtu.be/dQw4w9WgXcQ") == "dQw4w9WgXcQ", "chat: video id from a youtu.be link");
    check (YouTubeChat::videoIdFrom ("https://www.youtube.com/live/abcDEF12345?si=x") == "abcDEF12345", "chat: video id from a /live/ link");
    check (YouTubeChat::channelPathFrom ("https://www.youtube.com/@CatOcean/streams") == "@CatOcean", "chat: channel from a link");
    check (YouTubeChat::channelPathFrom ("@miro.music") == "@miro.music", "chat: channel typed as @name");
    check (YouTubeChat::videoIdFrom ("@CatOcean").isEmpty(), "chat: a channel is not taken for a video");

    const juce::String page = R"(<script>var ytInitialData = {"contents":{"liveChatRenderer":{"continuations":[{"invalidationContinuationData":{"continuation":"ABC123","timeoutMs":5000}}],"actions":[{"addChatItemAction":{"item":{"liveChatTextMessageRenderer":{"id":"old1","message":{"runs":[{"text":"hi {}"}]}}}}}]}}};</script>"INNERTUBE_CLIENT_VERSION":"2.20241001.01.00")";
    juce::String cont;
    juce::StringArray seen;
    const auto data = juce::JSON::parse (YouTubeChat::jsonObjectAfter (page, "ytInitialData"));
    check (YouTubeChat::parseInitial (data, cont, seen) && cont == "ABC123" && seen.contains ("old1"), "chat: reads the chat page (continuation, old messages)");
    check (YouTubeChat::quotedValueAfter (page, "INNERTUBE_CLIENT_VERSION") == "2.20241001.01.00", "chat: finds the client version");
    {
        // "Top chat" hides messages: the reader switches to "Live chat" (all messages)
        const auto withMenu = juce::JSON::parse (R"({"contents":{"liveChatRenderer":{"continuations":[{"invalidationContinuationData":{"continuation":"TOP"}}],
            "header":{"liveChatHeaderRenderer":{"viewSelector":{"sortFilterSubMenuRenderer":{"subMenuItems":[
              {"title":"Top chat","selected":true,"continuation":{"reloadContinuationData":{"continuation":"TOPCHAT"}}},
              {"title":"Live chat","selected":false,"continuation":{"reloadContinuationData":{"continuation":"LIVECHAT"}}}]}}}}}}})");
        juce::String c2;
        juce::StringArray seen2;
        check (YouTubeChat::parseInitial (withMenu, c2, seen2) && c2 == "LIVECHAT", "chat: reads Live chat (every message), not Top chat (" + c2 + ")");
    }

    const juce::String answer = R"({"continuationContents":{"liveChatContinuation":{"continuations":[{"timedContinuationData":{"continuation":"NEXT","timeoutMs":2500}}],
        "actions":[{"addChatItemAction":{"item":{"liveChatTextMessageRenderer":{"id":"m1","authorExternalChannelId":"UCabc","authorName":{"simpleText":"Miro"},
        "message":{"runs":[{"text":"!fish Miro "},{"emoji":{"emojiId":"x","shortcuts":[":cat:"]}},{"text":" blue"}]},
        "authorBadges":[{"liveChatAuthorBadgeRenderer":{"icon":{"iconType":"MODERATOR"},"tooltip":"Moderator"}}]}}}},
        {"addChatItemAction":{"item":{"liveChatPaidMessageRenderer":{"id":"m2","authorExternalChannelId":"UCdef","authorName":{"simpleText":"Ana"},"message":{"runs":[{"text":"!feed"}]}}}}},
        {"addLiveChatTickerItemAction":{}}]}}})";
    check (YouTubeChat::parseSubscribers (R"(..."subscriberCountText":{"accessibility":{"accessibilityData":{"label":"1.23K subscribers"}},"simpleText":"1.23K subscribers"}...)") == 1230
           && YouTubeChat::parseSubscribers (R"("subscriberCountText":{"simpleText":"987 subscribers"})") == 987
           && YouTubeChat::parseSubscribers ("no count here") == -1, "chat: reads the subscriber count (987, 1.23K)");
    std::vector<YouTubeChat::Message> msgs;
    int wait = 0;
    juce::String next;
    const bool ok = YouTubeChat::parseChat (juce::JSON::parse (answer), msgs, next, wait);
    check (ok && msgs.size() == 2 && msgs[0].authorName == "Miro" && msgs[0].authorId == "UCabc" && msgs[0].text == "!fish Miro :cat: blue"
           && msgs[1].text == "!feed" && next == "NEXT" && wait == 2500, "chat: reads new messages (text, emoji, super chat) and the next poll");
    check (msgs.size() == 2 && msgs[0].moderator && ! msgs[1].moderator, "chat: knows who is a moderator");
    {
        const auto paid = juce::JSON::parse (R"({"continuationContents":{"liveChatContinuation":{"actions":[
            {"addChatItemAction":{"item":{"liveChatPaidMessageRenderer":{"id":"p1","authorExternalChannelId":"UCp","authorName":{"simpleText":"Rich"},"purchaseAmountText":{"simpleText":"$5.00"}}}}},
            {"addChatItemAction":{"item":{"liveChatMembershipItemRenderer":{"id":"mb1","authorExternalChannelId":"UCm","authorName":{"simpleText":"Newbie"},"headerSubtext":{"runs":[{"text":"Welcome to Cat Ocean!"}]}}}}}]}}})");
        std::vector<YouTubeChat::Message> pm;
        juce::String nx;
        int w2 = 0;
        YouTubeChat::parseChat (paid, pm, nx, w2);
        check (pm.size() == 2 && pm[0].kind == 1 && pm[0].amount == "$5.00" && pm[1].kind == 3 && pm[1].authorName == "Newbie",
               "chat: reads Super Chats (with the amount) and new members");
    }
}

void testGenes()
{
    juce::Random r (5);
    int rare = 0, mutated = 0, n = 20000;
    Genes a = Genes::random (r), b = Genes::random (r);
    for (int i = 0; i < n; ++i)
    {
        bool m = false;
        const auto c = Genes::breed (a, b, r, &m);
        rare += c.rare != RareNone ? 1 : 0;
        mutated += m ? 1 : 0;
    }
    check (rare > n / 200 && rare < n / 40, "genes: rare babies are rare (" + juce::String (rare) + " of " + juce::String (n) + ")");
    check (mutated > n / 20 && mutated < n / 2, "genes: some babies mutate (" + juce::String (mutated) + " of " + juce::String (n) + ")");
    Genes g = Genes::random (r);
    g.setMelody ("1353");
    check (g.melodyString() == "1353" && g.melodyLength == 4, "genes: !melody 1353 is kept as typed");
    const auto back = Genes::fromTree (g.toTree());
    check (back.toTree().isEquivalentTo (g.toTree()), "genes: save / load gives the same fish");
}

void testCommandsAndLife (const juce::File& outDir)
{
    const auto saveFile = outDir.getChildFile ("ocean-test.xml");
    saveFile.deleteFile();
    Engine engine (saveFile);
    auto& sim = engine.simForTests();
    auto& music = engine.musicForTests();
    engine.paintEvery = 1;
    sim.timeOverride = 12.0f;   // pictures at noon (the real clock could say night)
    sim.daylight = 1.0f;
    engine.runFrames (30);
    savePng (engine.canvasForTests(), outDir.getChildFile ("01-start.png"));
    check (distinctColours (engine.canvasForTests()) > 40, "picture: the ocean is drawn (" + juce::String (distinctColours (engine.canvasForTests())) + " colours)");

    // a viewer joins
    engine.say ("UC1", "Miro", "!fish Miro blue stripes cat");
    engine.runFrames (90);
    auto M = [&sim] { return fishNamed (sim, "Miro"); };   // always fresh: the fish list moves when fish are born
    check (M() != nullptr && M()->viewer() && M()->present, "chat: !fish makes a viewer fish that swims in");
    if (M() != nullptr)
        check (M()->genes.colour1 == findColour ("blue") && M()->genes.pattern == PatStripes && M()->genes.body != BodyCat,
               "chat: !fish Miro blue stripes cat sets the look (the cat body waits for LV5)");
    if (M() != nullptr) M()->xp = 1000;   // a long-time viewer: everything unlocked
    engine.say ("UC1", "Miro", "!body cat");
    engine.runFrames (90);
    check (M() != nullptr && M()->genes.body == BodyCat && M()->level() >= 8, "levels: LV" + juce::String (M() != nullptr ? M()->level() : 0) + " may have a cat body");

    // more commands (with the cooldown between them)
    auto cmd = [&] (const juce::String& id, const juce::String& text) { engine.runFrames (270); engine.say (id, "Viewer", text); engine.runFrames (2); };
    cmd ("UC1", "!color red yellow white");
    cmd ("UC1", "!hat crown");
    cmd ("UC1", "!eyes big");
    cmd ("UC1", "!tail veil");
    cmd ("UC1", "!fin sail");
    cmd ("UC1", "!voice bell");
    cmd ("UC1", "!melody 1353");
    cmd ("UC1", "!mood hyper");
    cmd ("UC1", "!notes long");
    cmd ("UC1", "!size big");
    
    if (M() != nullptr)
    {
        const auto& g = M()->genes;
        check (g.colour1 == findColour ("red") && g.colour2 == findColour ("yellow") && g.colour3 == findColour ("white"), "chat: !color sets body, pattern and fin colours");
        check (g.accessory == AccCrown && g.eyes == EyesBig && g.tail == TailVeil && g.fin == FinSail, "chat: !hat !eyes !tail !fin");
        check (g.voice == VoiceBell && g.melodyString() == "1353" && g.mood() == MoodHyper && g.noteLength == 1 && g.size == 2,
               "chat: !voice !melody !mood !notes !size");
    }
    engine.runFrames (270);
    const int before = sim.ignoredCommands;
    engine.say ("UC1", "Miro", "!hat glasses");
    engine.say ("UC1", "Miro", "!hat bow");
    engine.runFrames (3);
    check (sim.ignoredCommands == before + 1 && M() != nullptr && M()->genes.accessory == AccGlasses, "chat: a second command right away is ignored (cooldown)");
    engine.say ("UC2", "fuck", "!fish");
    engine.say ("UC3", "Ana", "!fish kurva");
    engine.runFrames (2);
    bool rude = false;
    for (auto& f : sim.fish) rude = rude || ! filter::isClean (f.name);
    {
        juce::String rudeName;
        for (auto& f : sim.fish) if (! filter::isClean (f.name)) rudeName = f.name;
        check (! rude && sim.fishOf ("UC2") != nullptr && sim.fishOf ("UC3") != nullptr,
               "chat: rude names are replaced (" + (rudeName.isNotEmpty() ? "rude: " + rudeName : juce::String ("none rude"))
               + ", UC2 " + (sim.fishOf ("UC2") != nullptr ? sim.fishOf ("UC2")->name : juce::String ("no fish"))
               + ", UC3 " + (sim.fishOf ("UC3") != nullptr ? sim.fishOf ("UC3")->name : juce::String ("no fish")) + ")");
    }
    // a new viewer can't wear a crown yet
    engine.runFrames (90);
    engine.say ("UC3", "Ana", "!hat crown");
    engine.runFrames (2);
    check (sim.fishOf ("UC3") != nullptr && sim.fishOf ("UC3")->genes.accessory != AccCrown, "levels: the crown waits for LV8");
    const int xpBefore = sim.fishOf ("UC3") != nullptr ? sim.fishOf ("UC3")->xp : 0;
    engine.runFrames (30 * 65);
    check (sim.fishOf ("UC3") != nullptr && sim.fishOf ("UC3")->xp > xpBefore, "levels: swimming earns XP");

    // emotes
    engine.say ("UC1", "Miro", "!jump");
    engine.runFrames (2);
    const float yBefore = M() != nullptr ? M()->y : 0.0f;
    engine.runFrames (12);
    check (M() != nullptr && M()->y < yBefore - 10.0f, "emotes: !jump jumps");
    engine.say ("UC1", "Miro", "!dance");
    engine.runFrames (2);
    check (M() != nullptr && M()->emote != EmoteDance, "spam: emotes need a pause (20 s)");
    engine.runFrames (30 * 21);
    engine.say ("UC1", "Miro", "!dance");
    engine.runFrames (20);
    check (M() != nullptr && M()->emote == EmoteDance, "emotes: !dance dances (emote " + juce::String (M() != nullptr ? M()->emote : -1) + ")");
    savePng (engine.canvasForTests(), outDir.getChildFile ("02b-dance.png"));
    engine.runFrames (120);

    // moderators
    engine.say ("UCmod", "Mod", "!rename @Ana Bubbles", true);
    engine.runFrames (2);
    check (sim.fishOf ("UC3") != nullptr && sim.fishOf ("UC3")->name == "Bubbles", "mods: !rename @name");
    engine.say ("UCx", "Troll", "!ban @Miro");
    engine.runFrames (2);
    check (sim.fishOf ("UC1") != nullptr, "mods: viewers can't use mod commands");
    engine.say ("UCmod", "Mod", "!ban @Ana", true);
    engine.runFrames (2);
    check (sim.fishOf ("UC3") == nullptr && sim.isBanned ("UC3"), "mods: !ban removes the fish");
    engine.say ("UC3", "Ana", "!fish Ana");
    engine.runFrames (2);
    check (sim.fishOf ("UC3") == nullptr, "mods: banned people get no fish");
    engine.say ("UCmod", "Mod", "!unban @Ana", true);
    engine.runFrames (90);
    engine.say ("UC3", "Ana", "!fish Ana");
    engine.runFrames (2);
    check (sim.fishOf ("UC3") != nullptr, "mods: !unban lets them back");
    engine.say ("host", "Host", "!kick @Miro", true);
    engine.runFrames (2);
    check (M() != nullptr && M()->leaving, "mods: !kick sends a fish home (the host can do it too)");
    engine.say ("UC1", "Miro", "back!");
    engine.runFrames (2);
    

    // places
    for (auto* where : { "kelp", "crystal", "ship", "cave", "ice", "lagoon", "ruins", "volcano", "jelly", "open", "reef" })
    {
        engine.say ("UCmod", "Mod", juce::String ("!place ") + where, true);
        engine.runFrames (30 * 5);
        savePng (engine.canvasForTests(), outDir.getChildFile (juce::String ("09-place-") + where + ".png"));
        check (sim.place == findPlace (where), juce::String ("places: the ocean moves to the ") + placeName (findPlace (where)));
    }

    // spam: too many commands in a minute -> a 2 minute nap
    {
        engine.say ("UCspam", "Spammy", "!fish Spammy");
        for (int i = 0; i < 14; ++i) { engine.runFrames (100); engine.say ("UCspam", "Spammy", "!info " + juce::String (i)); }
        engine.runFrames (30 * 10);
        auto* sp = sim.fishOf ("UCspam");
        const int colourBefore = sp != nullptr ? sp->genes.colour1 : -1;
        engine.say ("UCspam", "Spammy", colourBefore == findColour ("red") ? "!color blue" : "!color red");
        engine.runFrames (2);
        check (sp != nullptr && sp->genes.colour1 == colourBefore, "spam: a viewer who floods commands is muted for a while");
        engine.say ("UCspam", "Spammy", "same same");
        engine.runFrames (2);
        const int ign = sim.ignoredCommands;
        engine.say ("UCspam", "Spammy", "same same");
        engine.runFrames (2);
        check (sim.ignoredCommands > ign, "spam: the same message twice is ignored");
    }

    // speech bubbles
    engine.say ("UC1", "Miro", "hello everyone!");
    engine.runFrames (2);
    check (! sim.speech.empty() && sim.speech.back().text == "hello everyone!", "chat: a normal message becomes a speech bubble");
    engine.say ("UC1", "Miro", "you are a kokot");
    engine.runFrames (2);
    check (sim.speech.empty() || sim.speech.back().text == "hello everyone!", "chat: rude messages get no bubble");
    engine.runFrames (20);
    savePng (engine.canvasForTests(), outDir.getChildFile ("02c-speech.png"));

    // the admin console
    engine.console ("/give @Miro 50");
    engine.console ("/rare @Miro gold");
    engine.console ("/stats");
    engine.runFrames (2);
    check (M() != nullptr && M()->genes.rare == RareGold, "admin: /rare @name gold");
    check (engine.consoleText().contains ("fish swimming") && engine.consoleText().contains ("LV"), "admin: /give and /stats answer in the console");
    engine.console ("/spawn 3");
    const int wildBefore = sim.wildCount();
    engine.runFrames (2);
    check (sim.wildCount() >= wildBefore + 2, "admin: /spawn adds wild fish");
    engine.console ("/nonsense");
    engine.runFrames (1);
    check (engine.consoleText().contains ("unknown command"), "admin: unknown commands say so");
    engine.console ("/kick @Miro");
    engine.runFrames (2);
    check (M() != nullptr && M()->leaving, "admin: mod commands work in the console");
    engine.say ("UC1", "Miro", "back");
    engine.runFrames (2);

    // day and night
    engine.console ("/time night");
    engine.runFrames (30 * 8);
    check (sim.daylight < 0.1f, "time: /time night makes it night");
    savePng (engine.canvasForTests(), outDir.getChildFile ("11-night.png"));
    engine.console ("/time sunset");
    engine.runFrames (30 * 8);
    savePng (engine.canvasForTests(), outDir.getChildFile ("12-sunset.png"));
    engine.console ("/time day");
    engine.runFrames (30 * 8);
    check (sim.daylight > 0.9f, "time: and day again");
    check (std::abs (Sim::daylightAt (3.0f)) < 0.01f && Sim::daylightAt (12.0f) > 0.99f && Sim::daylightAt (20.0f) > 0.2f && Sim::daylightAt (20.0f) < 0.8f,
           "time: the real clock gives night at 3, day at 12, sunset at 20");

    // a new species and a subscriber party
    const int sp = sim.discoverSpeciesForTests();
    check (sp > 0 && sim.species (sp) != nullptr && sim.species (sp)->trait != TraitNone,
           "evolution: a new species gets a name and a new trait (" + (sim.species (sp) != nullptr ? sim.species (sp)->name : juce::String()) + ")");
    sim.celebrateSubscribers (124, 1);
    engine.runFrames (20);
    check (! sim.confetti.empty(), "subscribers: a new subscriber makes a party");
    savePng (engine.canvasForTests(), outDir.getChildFile ("10-party.png"));

    // alerts: a subscriber milestone, a new member (their fish gets a crown), a Super Chat
    sim.alerts.clear();
    sim.celebrateSubscribers (100, 2);
    check (! sim.alerts.empty() && sim.alerts.back().title.contains ("100 SUBSCRIBERS"), "alerts: a subscriber milestone (100) gets a big alert");
    sim.memberJoined ("UC1", "Miro", "hi!");
    check (M() != nullptr && M()->genes.accessory == AccCrown && sim.alerts.back().title.contains ("MEMBER"), "alerts: a new member is welcomed and their fish gets a crown");
    sim.superChat ("UC1", "Miro", "EUR 5.00", "love this ocean");
    engine.runFrames (30);
    check (sim.alerts.back().title.contains ("SUPER CHAT") && sim.alerts.back().text == "love this ocean" && sim.fireworks > 0.0f, "alerts: a Super Chat shows its message with fireworks");
    savePng (engine.canvasForTests(), outDir.getChildFile ("13-alert.png"));
    engine.runFrames (30 * 25);
    check (sim.alerts.empty(), "alerts: they go away by themselves");

    // the boat cat: pets, food, it fishes things up and talks
    sim.cat.hunger = 0.9f;
    engine.say ("UC1", "Miro", "!feedcat");
    engine.runFrames (100);
    check (sim.cat.hunger < 0.6f && sim.cat.say.contains ("Miro"), "boat cat: !feedcat feeds it (it says thank you)");
    engine.say ("UC1", "Miro", "!pet");
    engine.runFrames (5);
    check (sim.cat.hearts > 0.0f, "boat cat: !pet makes it purr (hearts)");

    // the new events
    for (auto* ev : { "plankton", "migration", "meteors" })
    {
        engine.say ("UCmod", "Mod", juce::String ("!event ") + ev, true);
        engine.runFrames (100);
    }
    check (sim.plankton > 0.0f && sim.migration > 0.0f && sim.meteors > 0.0f, "events: plankton bloom, migration and meteor shower start");
    engine.say ("UC1", "Miro", "!wish");
    engine.runFrames (3);
    check (sim.wishers.count ("UC1") == 1, "events: !wish during a meteor shower");
    savePng (engine.canvasForTests(), outDir.getChildFile ("14-events.png"));

    engine.say ("UC4", "Nobody", "!hat crown");
    engine.runFrames (2);
    check (sim.fishOf ("UC4") == nullptr, "chat: other commands need !fish first");
    engine.runFrames (12);
    savePng (engine.canvasForTests(), outDir.getChildFile ("02-viewers.png"));

    // a crowd: 60 viewers, at most 40 fish of viewers on screen
    for (int i = 0; i < 60; ++i) engine.say ("UCcrowd" + juce::String (i), "Cat" + juce::String (i), "!fish");
    engine.runFrames (60);
    check (sim.viewerFishOnScreen() <= maxViewerFish, "crowd: at most " + juce::String (maxViewerFish) + " viewer fish on screen (" + juce::String (sim.viewerFishOnScreen()) + ")");
    engine.runFrames (30 * 20);
    savePng (engine.canvasForTests(), outDir.getChildFile ("03-crowd.png"));

    // a vote
    {
        bool started = false;
        for (int i = 0; i < 30 * 900 && ! started; ++i) { engine.runFrames (1); started = sim.vote.active; }
        check (started, "vote: a vote starts after a while");
        if (started)
        {
            for (int i = 0; i < 10; ++i) engine.say ("UCcrowd" + juce::String (i), "x", "!vote 2");
            engine.runFrames (2);
            check (sim.vote.counts.size() == 3 && sim.vote.counts[1] == 10, "vote: !vote 2 is counted once per viewer");
            savePng (engine.canvasForTests(), outDir.getChildFile ("04-vote.png"));
            const int kind = sim.vote.kind, choice = sim.vote.options[1];
            engine.runFrames (30 * 95);
            if (kind == 0) check (sim.getStyle() == choice, "vote: the ocean plays the music people voted for (" + styleName (choice) + ")");
            else if (kind == 1) check (sim.place == choice, "vote: the ocean moved where people voted (" + placeName (choice) + ")");
            else check (true, "vote: event " + eventName (choice) + " happened");
        }
    }

    // an hour of life (painting only now and then to be quick)
    engine.paintEvery = 15;
    double peakMax = 0, rmsSum = 0;
    int frames = 0, weatherSeen = 0;
    const int startPieces = music.pieces(), startSongs = music.songsStarted;
    int crossfadeFrames = 0, silentFrames = 0;
    juce::StringArray titles;
    for (int minute = 0; minute < 60; ++minute)
    {
        // a few viewers keep chatting, Miro's owner too
        if (minute % 5 == 0) engine.say ("UC1", "Miro", "hello");
        if (minute == 1) engine.say ("UC1", "Miro", "!request night jazz");
        if (minute == 4) { engine.say ("UC1", "Miro", "!like"); engine.runFrames (100); engine.say ("UC1", "Miro", "!like"); }
        for (int s = 0; s < 60; ++s)
        {
            engine.runFrames (30);
            peakMax = juce::jmax (peakMax, (double) engine.lastPeak());
            rmsSum += engine.lastPeak(); ++frames;
            weatherSeen |= 1 << sim.weather;
            if (music.isCrossfading()) ++crossfadeFrames;
            if (engine.lastPeak() < 0.002f) ++silentFrames;
            titles.addIfNotAlreadyThere (music.currentTitle());
        }
    }
    engine.paintEvery = 1;
    engine.runFrames (1);
    savePng (engine.canvasForTests(), outDir.getChildFile ("05-after-an-hour.png"));
    int gen2 = 0, juniors = 0, maxGen = 1;
    for (auto& f : sim.fish) { if (f.generation >= 2) ++gen2; if (f.name.contains ("Jr.")) ++juniors; maxGen = juce::jmax (maxGen, f.generation); }
    check (sim.births > 5, "life: fish have babies (" + juce::String (sim.births) + " in the test, " + juce::String (sim.mutations) + " mutations)");
    check (gen2 > 0, "life: there are fish of generation 2 or later (up to " + juce::String (maxGen) + ")");
    check (juniors > 0 || sim.births < 20, "life: babies of viewer fish are named after them (" + juce::String (juniors) + " Jr.)");
    check (sim.wildCount() >= minWild - 2 && sim.wildCount() <= maxWild + 1, "life: the ocean stays full but not crowded (" + juce::String (sim.wildCount()) + " wild fish)");
    
    check (M() != nullptr && M()->present, "viewers: a fish whose owner keeps chatting stays");
    int crowdHere = 0;
    for (int i = 0; i < 60; ++i) if (auto* f = sim.fishOf ("UCcrowd" + juce::String (i)); f != nullptr && f->present) ++crowdHere;
    check (crowdHere == 0, "viewers: fish of viewers who went quiet for 20 minutes swam home (" + juce::String (crowdHere) + " still here)");
    engine.say ("UCcrowd5", "Cat5", "I'm back");
    engine.runFrames (2);
    check (sim.fishOf ("UCcrowd5") != nullptr && sim.fishOf ("UCcrowd5")->present, "viewers: writing in chat brings your fish back");

    check (peakMax > 0.05 && peakMax <= 1.0, "music: plays and never clips (peak " + juce::String (peakMax, 3) + ")");
    check (music.fishNotes > 500, "music: the fish sing (" + juce::String (music.fishNotes) + " notes)");
    check (music.songsStarted - startSongs >= 3 && titles.size() >= 3,
           "music: new songs keep coming (" + juce::String (music.songsStarted - startSongs) + " in an hour: " + titles.joinIntoString (", ").substring (0, 120) + ")");
    check (music.requestsPlayed >= 1, "music: a !request plays as the next song (" + juce::String (music.requestsPlayed) + " played)");
    {
        int likes = 0;
        for (auto& fv : sim.favourites) likes += fv.likers.size();
        check (sim.favourites.size() >= 1 && likes == 1, "music: !like saves the song as a favourite, one like per viewer (" + juce::String (likes) + " likes)");
        // a liked song comes back: the same settings write the same song
        if (! sim.favourites.empty())
        {
            const auto fav = sim.favourites.front();
            cat::muse::Settings a; a.seed = fav.seed; a.genre = fav.genre; a.mood = fav.mood; a.key = fav.key; a.endless = true;
            check (cat::muse::compose (a).title == fav.title, "music: a favourite can be played again (\"" + fav.title + "\")");
        }
    }
    check (crossfadeFrames > 0, "music: songs crossfade into each other (" + juce::String (crossfadeFrames) + " s of crossfade)");
    check (silentFrames < 3, "music: never a silent gap (" + juce::String (silentFrames) + " silent seconds)");
    check (music.outOfKey * 50 < music.fishNotes + 1, "music: fish sing in the song's key (" + juce::String (music.outOfKey) + " out of key)");
    {
        const auto& st = styleInfo (sim.getStyle());
        check (music.currentBpm() >= st.bpmLo - 0.5 && music.currentBpm() <= st.bpmHi + 0.5,
               "music: always relaxed (" + juce::String (music.currentBpm(), 0) + " bpm, " + styleName (sim.getStyle()) + ")");
    }
    check (music.whales() > 3, "ambience: whales sing far away now and then (" + juce::String (music.whales()) + " in an hour)");
    check (music.pieces() > startPieces + 10, "music: the song keeps going, piece after piece (" + juce::String (music.pieces() - startPieces) + " new pieces)");

    // genes pull the music: make every fish like A at 120 bpm
    for (auto& f : sim.fish) { f.genes.key = 9; f.genes.tempo = 128.0f; }
    for (int s = 0; s < 60 * 12 && music.currentKey() != 9; ++s) engine.runFrames (30);
    check (music.currentKey() == 9, "music: the key moves to the one the fish like (now " + cat::muse::keyName (music.currentKey()) + ")");
    const double bpmBefore = music.currentBpm();
    for (int s = 0; s < 60 * 6; ++s) engine.runFrames (30);
    const double want = sim.targetTempo();
    check (std::abs (music.currentBpm() - want) <= 3.5 || std::abs (music.currentBpm() - want) < std::abs (bpmBefore - want),
           "music: the tempo moves towards what the fish like (" + juce::String (want, 0) + " wanted, "
           + juce::String (bpmBefore, 0) + " -> " + juce::String (music.currentBpm(), 0) + " bpm)");
    check ((weatherSeen & 2) != 0 || (weatherSeen & 4) != 0, "weather: it rains or storms now and then");

    // a storm picture
    sim.weather = WeatherStorm;
    sim.weatherLevel = 1.0f;
    sim.lightning = 0.6f;
    engine.runFrames (1);
    savePng (engine.canvasForTests(), outDir.getChildFile ("06-storm.png"));

    // save and load
    engine.saveNow();
    {
        Engine again (saveFile);
        auto& s2 = again.simForTests();
        check (s2.fish.size() >= sim.fish.size() - 2 && s2.fishOf ("UC1") != nullptr && s2.fishOf ("UC1")->name == "Miro"
               && s2.fishOf ("UC1")->genes.accessory == sim.fishOf ("UC1")->genes.accessory,
               "save / load: the ocean and every viewer's fish come back (" + juce::String ((int) s2.fish.size()) + " fish)");
        check (s2.fishOf ("UC1") != nullptr && ! s2.fishOf ("UC1")->present, "save / load: viewer fish wait in the cave until their owner chats");
        again.say ("UC1", "Miro", "hi");
        again.runFrames (2);
        check (again.simForTests().fishOf ("UC1")->present, "save / load: ... and come back when they do");
    }
}

void testAllLooks (const juce::File& outDir)
{
    // every body, tail, fin, eyes, pattern, hat and rare kind on one picture
    Canvas c;
    for (int i = 0; i < W * H; ++i) c.data()[i] = 0xff1b72a8;
    juce::Random r (3);
    int n = 0;
    for (int body = 0; body < NumBodies; ++body)
        for (int k = 0; k < 8; ++k)
        {
            Fish f;
            f.id = ++n;
            f.genes = Genes::random (r);
            f.genes.body = body;
            f.genes.size = 2;
            f.genes.tail = k % NumTails;
            f.genes.fin = k % NumFins;
            f.genes.eyes = k % NumEyes;
            f.genes.pattern = (k + body) % NumPatterns;
            f.genes.accessory = k % NumAccessories;
            f.genes.rare = k < 6 ? k : 0;
            f.genes.trait = (k + body) % NumTraits;
            f.x = 30.0f + k * 56.0f;
            f.y = 25.0f + body * 42.0f;
            f.facing = (k & 1) ? -1 : 1;
            f.wiggle = k * 0.7f;
            f.singFlash = k == 3 ? 0.8f : 0.0f;
            Painter::drawFish (c, f, 1.0f);
        }
    savePng (c, outDir.getChildFile ("07-all-looks.png"));

    // the wave while singing, frame by frame
    Canvas w;
    for (int i = 0; i < W * H; ++i) w.data()[i] = 0xff1b72a8;
    for (int k = 0; k < 8; ++k)
    {
        Fish f;
        f.id = 500 + k;
        f.genes = Genes::random (r);
        f.genes.size = 2;
        f.genes.body = k % 2 == 0 ? BodySlim : BodyCat;
        f.x = 40.0f + k * 55.0f; f.y = 135.0f;
        f.singFlash = 1.0f - k / 8.0f;
        Painter::drawFish (w, f, 0.0f);
    }
    savePng (w, outDir.getChildFile ("08-wave.png"));
    check (distinctColours (c) > 60, "picture: every kind of fish can be drawn");
}

void testVideo (const juce::File& outDir)
{
    const auto ff = Streamer::findFFmpeg ({});
    if (! ff.existsAsFile()) { check (true, "video: (ffmpeg not installed here, skipped)"); return; }
    const auto file = outDir.getChildFile ("test-video.mp4");
    file.deleteFile();
    Engine engine ({});
    engine.runFrames (5);
    Streamer s;
    check (s.start (ff, file.getFullPathName(), W, H, fps, sampleRate), "video: ffmpeg starts");
    // 6 s of ocean, as fast as ffmpeg takes it
    std::vector<float> l ((size_t) samplesPerFrame), rr ((size_t) samplesPerFrame);
    for (int i = 0; i < fps * 6; ++i)
    {
        engine.runFrames (1);
        const juce::int64 before = s.sentFrames();
        for (int tries = 0; tries < 2000 && s.sentFrames() == before && ! s.hasFailed(); ++tries)
        {
            for (int k = 0; k < samplesPerFrame; ++k) { l[(size_t) k] = 0.2f * std::sin (k * 0.05f); rr[(size_t) k] = l[(size_t) k]; }
            s.push (engine.canvasForTests().data(), l.data(), rr.data(), samplesPerFrame);
            if (s.sentFrames() == before) juce::Thread::sleep (5);
        }
    }
    s.stop();
    check (! s.hasFailed() && file.getSize() > 30000, "video: a 6 s test video was written (" + juce::String (file.getSize() / 1024) + " KB) " + s.getLog().trim().substring (0, 200));
    juce::ChildProcess probe;
    if (probe.start (juce::StringArray { ff.getSiblingFile ("ffprobe").getFullPathName(), "-v", "error", "-show_entries", "stream=codec_type,width,height,duration",
                                         "-of", "csv=p=0", file.getFullPathName() }))
    {
        const auto out = probe.readAllProcessOutput();
        check (out.contains ("video,1920,1080") || out.contains ("1920,1080,video") || out.contains ("1920,1080"), "video: 1920 x 1080 with sound (" + out.trim().replace ("\n", " | ") + ")");
    }
}
} // namespace

/** --places <dir>: a quick picture of every place, by day and by night (for checking the look). */
int renderPlaces (const juce::File& outDir)
{
    outDir.createDirectory();
    const auto saveFile = outDir.getChildFile ("places.xml");
    saveFile.deleteFile();
    Engine engine (saveFile);
    auto& sim = engine.simForTests();
    engine.paintEvery = 1;
    for (int night = 0; night < 2; ++night)
    {
        sim.timeOverride = night ? 23.0f : 12.0f;
        sim.daylight = night ? 0.0f : 1.0f;
        for (int p = 0; p < NumPlaces; ++p)
        {
            static const char* words[NumPlaces] = { "reef", "kelp", "crystal", "ship", "cave", "ice", "lagoon", "ruins", "volcano", "jelly", "open" };
            engine.say ("UCmod", "Mod", "!place " + juce::String (words[p]), true);
            engine.runFrames (30 * 5);
            savePng (engine.canvasForTests(), outDir.getChildFile (juce::String ("place-") + juce::String (p) + (night ? "-night" : "-day") + ".png"));
        }
    }
    return 0;
}

int runSelfTest (const juce::File& outDir)
{
    outDir.createDirectory();
    testFilter();
    testChatReader();
    testGenes();
    testAllLooks (outDir);
    testCommandsAndLife (outDir);
    testVideo (outDir);
    report.add (failures == 0 ? "CAT OCEAN SELF TEST PASSED" : "CAT OCEAN SELF TEST FAILED (" + juce::String (failures) + ")");
    std::printf ("%s\n", report[report.size() - 1].toRawUTF8());
    outDir.getChildFile ("ocean-test.txt").replaceWithText (report.joinIntoString ("\n"));
    return failures;
}

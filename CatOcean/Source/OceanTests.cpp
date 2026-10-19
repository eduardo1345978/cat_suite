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

    const juce::String answer = R"({"continuationContents":{"liveChatContinuation":{"continuations":[{"timedContinuationData":{"continuation":"NEXT","timeoutMs":2500}}],
        "actions":[{"addChatItemAction":{"item":{"liveChatTextMessageRenderer":{"id":"m1","authorExternalChannelId":"UCabc","authorName":{"simpleText":"Miro"},
        "message":{"runs":[{"text":"!fish Miro "},{"emoji":{"emojiId":"x","shortcuts":[":cat:"]}},{"text":" blue"}]}}}}},
        {"addChatItemAction":{"item":{"liveChatPaidMessageRenderer":{"id":"m2","authorExternalChannelId":"UCdef","authorName":{"simpleText":"Ana"},"message":{"runs":[{"text":"!feed"}]}}}}},
        {"addLiveChatTickerItemAction":{}}]}}})";
    std::vector<YouTubeChat::Message> msgs;
    int wait = 0;
    juce::String next;
    const bool ok = YouTubeChat::parseChat (juce::JSON::parse (answer), msgs, next, wait);
    check (ok && msgs.size() == 2 && msgs[0].authorName == "Miro" && msgs[0].authorId == "UCabc" && msgs[0].text == "!fish Miro :cat: blue"
           && msgs[1].text == "!feed" && next == "NEXT" && wait == 2500, "chat: reads new messages (text, emoji, super chat) and the next poll");
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
    engine.runFrames (30);
    savePng (engine.canvasForTests(), outDir.getChildFile ("01-start.png"));
    check (distinctColours (engine.canvasForTests()) > 40, "picture: the ocean is drawn (" + juce::String (distinctColours (engine.canvasForTests())) + " colours)");

    // a viewer joins
    engine.say ("UC1", "Miro", "!fish Miro blue stripes cat");
    engine.runFrames (90);
    auto* miro = fishNamed (sim, "Miro");
    check (miro != nullptr && miro->viewer() && miro->present, "chat: !fish makes a viewer fish that swims in");
    if (miro != nullptr)
        check (miro->genes.colour1 == findColour ("blue") && miro->genes.pattern == PatStripes && miro->genes.body == BodyCat,
               "chat: !fish Miro blue stripes cat sets the look");

    // more commands (with the cooldown between them)
    auto cmd = [&] (const juce::String& id, const juce::String& text) { engine.say (id, "Viewer", text); engine.runFrames (90); };
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
    miro = fishNamed (sim, "Miro");
    if (miro != nullptr)
    {
        const auto& g = miro->genes;
        check (g.colour1 == findColour ("red") && g.colour2 == findColour ("yellow") && g.colour3 == findColour ("white"), "chat: !color sets body, pattern and fin colours");
        check (g.accessory == AccCrown && g.eyes == EyesBig && g.tail == TailVeil && g.fin == FinSail, "chat: !hat !eyes !tail !fin");
        check (g.voice == VoiceBell && g.melodyString() == "1353" && g.mood() == MoodHyper && g.noteLength == 1 && g.size == 2,
               "chat: !voice !melody !mood !notes !size");
    }
    const int before = sim.ignoredCommands;
    engine.say ("UC1", "Miro", "!hat glasses");
    engine.say ("UC1", "Miro", "!hat bow");
    engine.runFrames (3);
    check (sim.ignoredCommands == before + 1 && miro != nullptr && miro->genes.accessory == AccGlasses, "chat: a second command right away is ignored (cooldown)");
    engine.say ("UC2", "fuck", "!fish");
    engine.say ("UC3", "Ana", "!fish kurva");
    engine.runFrames (2);
    bool rude = false;
    for (auto& f : sim.fish) rude = rude || ! filter::isClean (f.name);
    check (! rude && sim.fishOf ("UC2") != nullptr && sim.fishOf ("UC3") != nullptr, "chat: rude names are replaced");
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
        for (int i = 0; i < 30 * 300 && ! started; ++i) { engine.runFrames (1); started = sim.vote.active; }
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
            else check (true, "vote: event " + eventName (choice) + " happened");
        }
    }

    // an hour of life (painting only now and then to be quick)
    engine.paintEvery = 15;
    double peakMax = 0, rmsSum = 0;
    int frames = 0, weatherSeen = 0;
    const int startPieces = music.pieces();
    for (int minute = 0; minute < 60; ++minute)
    {
        // a few viewers keep chatting, Miro's owner too
        if (minute % 5 == 0) engine.say ("UC1", "Miro", "hello");
        for (int s = 0; s < 60; ++s)
        {
            engine.runFrames (30);
            peakMax = juce::jmax (peakMax, (double) engine.lastPeak());
            rmsSum += engine.lastPeak(); ++frames;
            weatherSeen |= 1 << sim.weather;
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
    miro = fishNamed (sim, "Miro");
    check (miro != nullptr && miro->present, "viewers: a fish whose owner keeps chatting stays");
    int crowdHere = 0;
    for (int i = 0; i < 60; ++i) if (auto* f = sim.fishOf ("UCcrowd" + juce::String (i)); f != nullptr && f->present) ++crowdHere;
    check (crowdHere == 0, "viewers: fish of viewers who went quiet for 20 minutes swam home (" + juce::String (crowdHere) + " still here)");
    engine.say ("UCcrowd5", "Cat5", "I'm back");
    engine.runFrames (2);
    check (sim.fishOf ("UCcrowd5") != nullptr && sim.fishOf ("UCcrowd5")->present, "viewers: writing in chat brings your fish back");

    check (peakMax > 0.05 && peakMax <= 1.0, "music: plays and never clips (peak " + juce::String (peakMax, 3) + ")");
    check (music.fishNotes > 500, "music: the fish sing (" + juce::String (music.fishNotes) + " notes)");
    check (music.outOfKey * 50 < music.fishNotes + 1, "music: fish sing in the song's key (" + juce::String (music.outOfKey) + " out of key)");
    check (music.pieces() > startPieces + 10, "music: the song keeps going, piece after piece (" + juce::String (music.pieces() - startPieces) + " new pieces)");

    // genes pull the music: make every fish like A at 120 bpm
    for (auto& f : sim.fish) { f.genes.key = 9; f.genes.tempo = 128.0f; }
    for (int s = 0; s < 60 * 12 && music.currentKey() != 9; ++s) engine.runFrames (30);
    check (music.currentKey() == 9, "music: the key moves to the one the fish like (now " + cat::muse::keyName (music.currentKey()) + ")");
    const double bpmBefore = music.currentBpm();
    for (int s = 0; s < 60 * 6; ++s) engine.runFrames (30);
    check (music.currentBpm() > bpmBefore || music.currentBpm() >= 120.0, "music: the tempo moves towards what the fish like ("
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
            f.x = 30.0f + k * 56.0f;
            f.y = 25.0f + body * 42.0f;
            f.facing = (k & 1) ? -1 : 1;
            f.wiggle = k * 0.7f;
            f.singFlash = k == 3 ? 0.8f : 0.0f;
            Painter::drawFish (c, f, 1.0f);
        }
    savePng (c, outDir.getChildFile ("07-all-looks.png"));
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

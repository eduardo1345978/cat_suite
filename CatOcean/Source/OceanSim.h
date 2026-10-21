#pragma once

#include "OceanGenes.h"
#include <map>
#include <set>
#include <deque>
#include <vector>

/**
    Cat Ocean: the living ocean. Wild fish swim, eat, have babies and grow old; every viewer can
    claim one fish (it swims in when they write in chat and swims away when they go quiet).
    Runs on one thread (the stream clock); chat messages are handed to chat().
*/
namespace ocean
{
constexpr int W = 480, H = 270;                   // the pixel canvas (shown 4x bigger: 1920 x 1080)
constexpr float surfaceY = 40.0f, floorY = 244.0f;
constexpr int maxWild = 34, minWild = 16, maxViewerFish = 40;
constexpr double viewerIdleSeconds = 20.0 * 60.0; // a viewer's fish swims away after 20 quiet minutes

enum Weather { WeatherCalm = 0, WeatherRain, WeatherStorm };

/** The music: always relaxing, four flavours of ambient. */
enum MusicStyle { StyleDreamy = 0, StyleLoFi, StyleDeepSea, StyleSunrise, StyleGlass, StyleBossa, StylePixel, StyleStrings,
                  StyleCity, StyleIsland, StyleFolk, StyleDreamHaze, StyleFunky, StyleNightJazz, NumStyles };
struct StyleInfo { const char* name; const char* word; int genre, mood; float bpmLo, bpmHi; bool drums; };
const StyleInfo& styleInfo (int style);
juce::String styleName (int style);
int findStyle (const juce::String& word);         // -1 if none

/** Where the ocean is (chat votes where it goes next). */
enum Place { PlaceReef = 0, PlaceKelp, PlaceDeep, PlaceWreck, PlaceCave, PlaceIce, PlaceLagoon, PlaceRuins,
             PlaceVolcano, PlaceJelly, PlaceOpenSea, NumPlaces };
juce::String placeName (int place);
int findPlace (const juce::String& word);

enum Emote { EmoteNone = 0, EmoteDance, EmoteSpin, EmoteJump };

struct Fish
{
    int id = 0;
    Genes genes;
    juce::String name;                            // shown above named fish
    juce::String ownerId, ownerName;              // viewer fish only
    bool viewer() const noexcept { return ownerId.isNotEmpty(); }

    float x = 0, y = 0, vx = 0, vy = 0;
    float targetX = 0, targetY = 0, retarget = 0;
    float age = 0;                                // seconds lived (in the ocean)
    float lifespan = 1800;                        // wild fish only
    float energy = 0.8f, breedCooldown = 60;
    float growth = 1.0f;                          // babies start small
    float wiggle = 0, singFlash = 0, hop = 0;
    int facing = 1;
    bool present = true, arriving = false, leaving = false, dying = false;
    float fade = 1.0f;
    int generation = 1, parentA = 0, parentB = 0, familyId = 0, babies = 0;
    juce::String familyName;                      // the viewer name the family started from (or "")
    int melodyPos = 0;
    double bornAt = 0;                            // ocean time
    // viewers' fish: experience and level
    int xp = 0;
    int level() const noexcept;
    // emotes (!dance !spin !jump)
    int emote = EmoteNone;
    float emoteTime = 0;
    bool emoteLeaped = false;   // jump: swims up first, then leaps out once
};

/** XP needed for a level (level 1 = 0 XP). */
int xpForLevel (int level);

struct Food { float x = 0, y = 0, vy = 10, life = 90; };
struct Toast { juce::String text; int fishId = 0; float x = 0, y = 0, life = 4.0f; juce::uint32 colour = 0xffffffff; };
struct Announcement { juce::String text; float life = 8.0f; juce::uint32 colour = 0xffffffff; };
struct Confetti { float x, y, vx, vy, life; juce::uint32 colour; };
/** What a viewer wrote in chat, shown in a bubble above their fish. */
struct Speech { int fishId = 0; juce::String text; float life = 6.0f; };

struct Vote
{
    bool active = false;
    int kind = 0;                                 // 0 = music style, 1 = place, 2 = event
    std::vector<int> options;
    std::vector<int> counts;
    std::map<juce::String, int> voters;
    float left = 0;                               // seconds
};
enum Event { EventStorm = 0, EventCalm, EventFeast, EventBabyBoom, EventRareHunt, EventPlankton, EventMigration, EventMeteors, NumEvents };
juce::String eventName (int e);
int findEvent (const juce::String& word);

/** A species that evolution discovered (its fish carry its trait). */
/** What is playing now (the music fills it in), so viewers can !like it. */
struct NowPlaying { juce::String title; int style = 0, genre = 0, mood = 0, key = 0; juce::uint32 seed = 0; double bpm = 0; };
/** A song viewers liked: it can come back later (the same settings write the same song). */
struct Favourite
{
    juce::String title;
    int style = 0, genre = 0, mood = 0, key = 0;
    juce::uint32 seed = 0;
    double bpm = 0;
    juce::StringArray likers;   // channel ids, one like each
    double lastPlayed = -1e9;
};
/** A song request from chat: a style, played as the next song. */
struct SongRequest { int style = 0; juce::String by; };

struct Species { int id = 0; juce::String name; int trait = 0; double discoveredAt = 0; juce::String firstFish; };

/** The cat in the boat on the surface. */
/** Things the boat cat can fish up. */
enum CatchItem { CatchNone = -1, CatchBoot = 0, CatchBottle, CatchCan, CatchStarfish, CatchFish, CatchDuck, CatchShell, CatchTreasure, NumCatchItems };
/** Who visits the boat now and then. */
enum Visitor { VisitorNone = 0, VisitorGull, VisitorGreyCat, VisitorDog, NumVisitors };

struct BoatCat
{
    float x = 120, vx = 4;
    int mood = 0;                                 // 0 fishing, 1 sleeping, 2 waving
    float moodTime = 0, wave = 0;
    juce::String waveAt;                          // who it waves at
    // needs: 0 .. 1 (fed and petted by chat: !feedcat, !pet)
    float hunger = 0.25f, sleepy = 0.0f, happy = 0.7f;
    // what it just caught (shown over the boat for a few seconds)
    int item = CatchNone;
    float itemTime = 0, nextCatch = 75;
    // what it says (a bubble over the boat)
    juce::String say;
    float sayTime = 0, nextTalk = 45;
    // a visitor on the boat
    int visitor = VisitorNone;
    float visitorTime = 0, nextVisitor = 240;
    float hearts = 0;                             // > 0: hearts float up (petted / fed)
    int catches = 0;
};
juce::String catchName (int item);

/** A big alert in the middle of the screen: new members, Super Chats, subscriber milestones. */
struct Alert { juce::String title, text; juce::uint32 colour = 0xfffee761; float life = 9.0f; int kind = 0; };

/** Today's numbers (for the end-of-day recap and the "most active today" board). */
struct DayStats
{
    juce::String date;                            // "2026-10-20"
    int messages = 0, newFish = 0, births = 0, viewersSeen = 0, superChats = 0, members = 0, newSubscribers = 0;
    std::map<juce::String, int> chatters;         // name -> messages today
};

struct Board { juce::String title; juce::StringArray lines; bool empty = false; };

class Music;
class Sim
{
    friend class Music;   // announces each new song
public:
    explicit Sim (juce::uint32 seed = 1);

    /** Moves the ocean on by dt seconds (the stream clock calls it 30 times a second). */
    void step (float dt);

    /** One chat message. Commands start with '!'; any message brings the writer's fish back.
        moderator: the channel owner or a moderator (they may use the mod commands). */
    void chat (const juce::String& authorId, const juce::String& authorName, const juce::String& text, bool moderator = false);

    /** The admin console (the app): "/help" lists everything. Returns what to print. */
    juce::String admin (const juce::String& line);
    float timeOverride = -1.0f;                   // admin "/time": hours, or -1 = the real clock
    static float daylightAt (float hours);

    /** The channel got more subscribers: a little party (a big one at 10, 25, 50, 100, 250, 500, 1000 ...). */
    void celebrateSubscribers (int newCount, int gained);
    /** Somebody became a member (or reached a member milestone): a golden welcome, their fish gets a crown. */
    void memberJoined (const juce::String& authorId, const juce::String& name, const juce::String& text);
    /** A Super Chat / Super Sticker: a big bubble with the message and fireworks (more for bigger amounts). */
    void superChat (const juce::String& authorId, const juce::String& name, const juce::String& amount, const juce::String& text);
    std::deque<Alert> alerts;
    float fireworks = 0;                          // seconds of fireworks left

    // today
    DayStats today;
    float recap = 0;                              // > 0: the end-of-day recap is on screen
    DayStats recapStats;
    juce::String dayRecapText (const DayStats&) const;
    void showRecap();

    juce::ValueTree save() const;
    void load (const juce::ValueTree&);

    // what the music listens to
    float targetTempo() const;                    // average of the fish on screen (inside the style's range)
    int targetKey() const;                        // the key most fish like (weighted)
    int takeStyleRequest() { const int s = styleRequest; styleRequest = -1; return s; }

    // song requests and favourites
    NowPlaying nowPlaying;
    std::vector<Favourite> favourites;
    std::deque<SongRequest> requests;
    static constexpr int maxRequests = 5;
    static constexpr double requestCooldown = 600.0, favouritesCooldown = 60.0;
    bool takeSongRequest (SongRequest& out) { if (requests.empty()) return false; out = requests.front(); requests.pop_front(); return true; }
    /** A liked song to play again now and then (not one played in the last hour), or nullptr. */
    const Favourite* pickFavourite (juce::Random&);
    void markFavouritePlayed (juce::uint32 seed, int style);
    int likeNowPlaying (const juce::String& viewerId);   // returns the song's likes, or -1 if nothing to like / liked already
    int getStyle() const noexcept { return style; }
    void setStyle (int s) noexcept { style = juce::jlimit (0, NumStyles - 1, s); styleTimer = 0.0f; }

    Board board (int which) const;                // 0 oldest, 1 families, 2 rare, 3 most babies, 4 species, 5 levels, 6 most active today
    static constexpr int numBoards = 7;
    int wildCount() const;
    int viewerFishOnScreen() const;
    int viewerCount() const { return (int) viewers.size(); }
    Fish* findFish (int id);
    const Fish* findFish (int id) const;
    Fish* fishOf (const juce::String& ownerId);
    const Species* species (int id) const;
    juce::StringArray bannedNames() const;
    bool isBanned (const juce::String& ownerId) const { return banned.count (ownerId) > 0; }

    // state (read by the painter on the same thread)
    std::vector<Fish> fish;
    std::vector<Food> food;
    std::vector<Toast> toasts;
    std::vector<Confetti> confetti;
    std::vector<Speech> speech;
    float daylight = 1.0f;                        // 0 night .. 1 day (from the real clock; the engine sets it)
    float clockHours = 12.0f;
    std::deque<Announcement> news;
    std::vector<Species> speciesList;
    Vote vote;
    BoatCat cat;
    int weather = WeatherCalm;
    float weatherLevel = 0;                       // 0 calm .. 0.5 rain .. 1 storm (smoothed)
    float lightning = 0;
    int place = PlaceReef, nextPlace = -1;
    float placeFade = 0;                          // 0 .. 1 .. 0 while the ocean moves to a new place
    double time = 0;                              // seconds of ocean life
    float babyBoom = 0, rareHunt = 0;             // seconds left of those events
    float plankton = 0, migration = 0, meteors = 0;
    std::set<juce::String> wishers;               // who made a wish in this meteor shower
    int births = 0, mutations = 0;

    /** Tests: evolution discovers a species right now (returns its id). */
    int discoverSpeciesForTests();
    /** Test hook: how many commands got ignored by the cooldown / filter. */
    int ignoredCommands = 0;

private:
    struct Viewer
    {
        juce::String name;
        int fishId = 0;
        double lastMessage = -1e9, lastCommand = -1e9, lastFeed = -1e9, lastXpMessage = -1e9;
        // anti-spam: every kind of command has its own pause, and too many in a minute mutes the viewer for a while
        double lastEmote = -1e9, lastLook = -1e9, lastName = -1e9, lastSpeech = -1e9, mutedUntil = -1e9;
        std::deque<double> recent;
        juce::String lastText;
        double lastTextAt = -1e9;
        bool warned = false;
        double lastRequest = -1e9, lastPet = -1e9;
    };

    void stepFish (Fish&, float dt);
    void stepWeather (float dt);
    void stepVote (float dt);
    void stepCat (float dt);
    void catSay (const juce::String&, float seconds = 6.0f);
    void stepDay();
    void stepXp (float dt);
    void tryBreeding (float dt);
    void keepPopulation (float dt);
    void dropFood (int count, float x = -1);
    void startVote();
    void finishVote();
    void applyEvent (int e);
    void goToPlace (int p);
    void addXp (Fish&, int amount);
    void party (int count);
    Fish& spawnWild (bool fromSide);
    Fish& newViewerFish (const juce::String& ownerId, const juce::String& name);
    void bringBack (Fish&);
    void sendAway (Fish&);
    void toast (const Fish&, const juce::String&, juce::uint32 colour = 0xffffffff);
    void announce (const juce::String&, juce::uint32 colour = 0xffffffff);
    void command (Viewer&, const juce::String& ownerId, const juce::String& text, bool moderator);
    bool modCommand (const juce::String& cmd, const juce::StringArray& tok, const juce::StringArray& raw);
    bool applyWord (Fish&, const juce::String& word, juce::String& changed, juce::String& locked);
    juce::String findViewer (const juce::String& nameOrMention) const;   // owner id or ""
    void maybeNewSpecies (Fish& baby, const Fish& a, const Fish& b, bool mutated);

    std::map<juce::String, Viewer> viewers;
    std::set<juce::String> banned;
    std::map<juce::String, juce::String> bannedNamesById;
    juce::Random rng;
    int nextId = 1, nextSpecies = 1;
    float weatherTimer = 600, foodTimer = 120, voteTimer = 180, wildTimer = 0, xpTimer = 0;
    int voteKindNext = 0;
    bool newSpeciesBorn = false;
    double lastHelp = -1e9, lastFeedAnyone = -1e9, lastCatFed = -1e9, lastToday = -1e9;
    int lastPeriod = -1;   // part of the day the cat last talked about
    bool spamCheck (Viewer&, const juce::String& text, bool isCommand);
    int style = StyleDreamy, styleRequest = -1;
    double lastFavouritesShown = -1e9, lastSongShown = -1e9;
    float styleTimer = 0.0f;   // the music drifts to another style after a while
};
} // namespace ocean

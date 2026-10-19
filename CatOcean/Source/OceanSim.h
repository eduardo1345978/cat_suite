#pragma once

#include "OceanGenes.h"
#include <map>
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
enum MusicStyle { StyleAmbient = 0, StyleLoFi, StyleJazz, StyleHouse, StyleCinematic, StylePop, StyleTrap, StyleRock, NumStyles };
juce::String styleName (int style);
int styleGenre (int style);                       // -> cat::muse::Genre

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
};

struct Food { float x = 0, y = 0, vy = 10, life = 90; };
struct Toast { juce::String text; int fishId = 0; float x = 0, y = 0, life = 4.0f; juce::uint32 colour = 0xffffffff; };
struct Announcement { juce::String text; float life = 8.0f; juce::uint32 colour = 0xffffffff; };

struct Vote
{
    bool active = false;
    int kind = 0;                                 // 0 = music style, 1 = event
    std::vector<int> options;                     // style index or event index
    std::vector<int> counts;
    std::map<juce::String, int> voters;
    float left = 0;                               // seconds
};
enum Event { EventStorm = 0, EventCalm, EventFeast, EventBabyBoom, EventRareHunt, NumEvents };
juce::String eventName (int e);

struct Board { juce::String title; juce::StringArray lines; bool empty = false; };

class Sim
{
public:
    explicit Sim (juce::uint32 seed = 1);

    /** Moves the ocean on by dt seconds (the stream clock calls it 30 times a second). */
    void step (float dt);

    /** One chat message. Commands start with '!'; any message brings the writer's fish back. */
    void chat (const juce::String& authorId, const juce::String& authorName, const juce::String& text);

    juce::ValueTree save() const;
    void load (const juce::ValueTree&);

    // what the music listens to
    float targetTempo() const;                    // average of the fish on screen
    int targetKey() const;                        // the key most fish like (weighted)
    int takeStyleRequest() { const int s = styleRequest; styleRequest = -1; return s; }
    int getStyle() const noexcept { return style; }
    void setStyle (int s) noexcept { style = juce::jlimit (0, NumStyles - 1, s); }

    Board board (int which) const;                // 0 oldest, 1 families, 2 rare, 3 most babies
    static constexpr int numBoards = 4;
    int wildCount() const;
    int viewerFishOnScreen() const;
    int viewerCount() const { return (int) viewers.size(); }
    Fish* findFish (int id);
    const Fish* findFish (int id) const;
    Fish* fishOf (const juce::String& ownerId);

    // state (read by the painter on the same thread)
    std::vector<Fish> fish;
    std::vector<Food> food;
    std::vector<Toast> toasts;
    std::deque<Announcement> news;
    Vote vote;
    int weather = WeatherCalm;
    float weatherLevel = 0;                       // 0 calm .. 0.5 rain .. 1 storm (smoothed)
    float lightning = 0;
    double time = 0;                              // seconds of ocean life
    float babyBoom = 0, rareHunt = 0;             // seconds left of those events
    int births = 0, mutations = 0;

    /** Test hook: how many commands got ignored by the cooldown / filter. */
    int ignoredCommands = 0;

private:
    struct Viewer { juce::String name; int fishId = 0; double lastMessage = -1e9, lastCommand = -1e9, lastFeed = -1e9; };

    void stepFish (Fish&, float dt);
    void stepWeather (float dt);
    void stepVote (float dt);
    void tryBreeding (float dt);
    void keepPopulation (float dt);
    void dropFood (int count, float x = -1);
    void startVote();
    void finishVote();
    void applyEvent (int e);
    Fish& spawnWild (bool fromSide);
    Fish& newViewerFish (const juce::String& ownerId, const juce::String& name);
    void bringBack (Fish&);
    void sendAway (Fish&);
    void toast (const Fish&, const juce::String&, juce::uint32 colour = 0xffffffff);
    void announce (const juce::String&, juce::uint32 colour = 0xffffffff);
    void command (Viewer&, const juce::String& ownerId, const juce::String& text);
    bool applyWord (Fish&, const juce::String& word, juce::String& changed);

    std::map<juce::String, Viewer> viewers;
    juce::Random rng;
    int nextId = 1;
    float weatherTimer = 600, foodTimer = 120, voteTimer = 180, wildTimer = 0;
    int voteKindNext = 0;
    int style = StyleAmbient, styleRequest = -1;
};
} // namespace ocean

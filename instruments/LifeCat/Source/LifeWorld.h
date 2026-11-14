#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <juce_core/juce_core.h>

/**
    Life-Cat world: a small sandbox ecosystem.

      Plants      grow by themselves (more on meadows, in warm zones, in spring and rain)
      Grazers     eat plants, flee hunters
      Hunters     eat grazers, swarms and pollinators
      Drifters    live on light, float calmly, sing long notes
      Pollinators visit flowers, drink nectar and spread new plants
      Scavengers  eat what dies (and leftovers), play percussion
      Giants      slow, huge, rare; eat plants as they walk; deep drones
      Swarms      move as a flock and sing arpeggios together
      Singers     birds / whales: roam far, gather, and sing whole melodic phrases
      Drummers    woodpeckers / crabs: tap out steady rhythms (percussion)
      Builders    beavers / corals: plant seeds, lay meadow trails and build nests

    Every creature carries genes (0..1). Babies get a blend of both parents' genes plus a
    small mutation, so body, behaviour and SOUND slowly evolve over the generations.
    The world has day and night, seasons, weather, gravity and wind, and terrain you paint
    (water, rock, meadow, light). It runs on the audio thread with a fixed time step and its
    own random generator, so the same seed gives the same life (and the same music).
*/
namespace lifecat
{
constexpr int maxCreatures = 128;
constexpr int maxPlants = 260;
constexpr int maxNests = 16;
constexpr int maxCorpses = 48;
constexpr int maxWalls = 48;
constexpr int maxZones = 16;
constexpr int historyLength = 240;
constexpr int terrainSize = 32;

enum class Role : uint8_t { Grazer = 0, Hunter, Drifter, Pollinator, Scavenger, Giant, Swarm, Singer, Drummer, Builder };
constexpr int numRoles = 10;

enum Gene
{
    // body / behaviour
    gSpeed, gSize, gSight, gFertility, gLifespan, gHue,
    // sound
    gTimbre,    // 0 soft pad .. 0.5 bell .. 1 texture
    gBright,    // dark .. bright
    gHarm,      // harmonic colour (FM ratio / detune / resonance)
    gAttack,    // fast .. slow
    gRelease,   // short .. long
    gRegister,  // low .. high octave
    gPitch,     // which note / chord tone
    gRhythm,    // how often it sings (2 bars .. 1/16)
    gDensity,   // chance to sing when its turn comes
    gVibrato, gShimmer, gWobble, gPan,
    // v2: body / behaviour
    gSocial,    // likes to stay with its own kind
    gNight,     // night owl: awake at night instead of by day
    gHardy,     // copes with winter, snow and cold zones
    gSwim,      // moves well through water
    gPattern,   // spots / stripes (look only)
    gGlow,      // glows in the dark (look + brighter at night)
    // v2: sound
    gInterval,  // sometimes sings two notes together
    gGlide,     // slides into its notes
    gAccent,    // dynamics: plain .. strongly accented
    gEcho,      // sends its voice into the echo
    numGenes
};
constexpr int numGenesV1 = 19;

juce::StringArray geneNames();
bool isSoundGene (int gene) noexcept;

struct Genes
{
    std::array<float, numGenes> g {};
    float operator[] (int i) const noexcept { return g[(size_t) i]; }
    float& operator[] (int i) noexcept { return g[(size_t) i]; }
};

struct Creature
{
    bool alive = false;
    uint32_t id = 0;
    Role role = Role::Grazer;
    float x = 0.5f, y = 0.5f, vx = 0.0f, vy = 0.0f, heading = 0.0f;
    float energy = 0.6f, age = 0.0f, breedCooldown = 0.0f;
    Genes genes;
    bool favourite = false;
    int generation = 0;
    uint32_t parentA = 0, parentB = 0;
    float singFlash = 0.0f;       // 1 when it sings, fades (for the display)
    float freePhase = 0.0f;       // free (unquantized) singing clock, beats
    float activity = 1.0f;        // 1 awake .. 0 asleep (day / night)
    uint32_t group = 0;           // swarm flock id (smallest id in the flock)
    int groupRank = 0;            // place in the flock (for arpeggios)
    float visitCooldown = 0.0f;   // pollinators: time until the next flower
    float blink = 0.0f;           // eyes closing (display)
    uint32_t nest = 0;            // builders: their nest (index + 1)
    float phraseTimer = 0.0f;     // singers: time until the next phrase
};

/** kind: 0 plant, 1 fruit (dropped by a tree), 2 mushroom (grows from what died, glows at night) */
struct Plant { bool alive = false; float x = 0.5f, y = 0.5f, size = 0.3f; uint8_t kind = 0; };
/** Fruit trees are not eaten: they keep dropping fruit around them. */
struct Tree { bool alive = false; float x = 0.5f, y = 0.5f, size = 0.2f, fruitTimer = 3.0f; };
constexpr int maxTrees = 12;

/** World events you can trigger (the EVENTS buttons). */
enum class WorldEvent : uint8_t { Meteors = 0, Flood, Drought, Bloom, Migration, Plague };
constexpr int numWorldEvents = 6;
struct Corpse { bool alive = false; float x = 0.5f, y = 0.5f, food = 0.5f, age = 0.0f; Role role = Role::Grazer; };
struct Wall { float x1 = 0, y1 = 0, x2 = 0, y2 = 0; };
/** Built by builders: plants grow around it, builders rest and breed there. */
struct Nest { bool alive = false; float x = 0.5f, y = 0.5f, size = 0.0f; uint32_t owner = 0; };

enum class ZoneType : uint8_t { Warm = 0, Cold, Current };
struct Zone { ZoneType type = ZoneType::Warm; float x = 0.5f, y = 0.5f, r = 0.1f, dx = 0.0f, dy = 0.0f; };

enum class Terrain : uint8_t { Ground = 0, Water, Rock, Meadow, Light, Sand, Lava };
constexpr int numTerrains = 7;

enum class Weather : uint8_t { Clear = 0, Rain, Snow, Fog, Storm };
constexpr int numWeathers = 5;

/** Per kind of creature (the SPECIES tab). */
struct SpeciesSettings
{
    float speed = 1.0f;          // multiplier
    int maxCount = 20;           // 0 = this kind is not allowed
};

/** How life behaves (LIFE + WORLD tabs). */
struct LifeSettings
{
    float speed = 1.0f;          // overall life speed
    float foodGrowth = 0.5f;     // plants per second, scaled
    float mutation = 0.25f;      // 0 stable .. 1 wild
    float metabolism = 0.5f;     // how fast energy is used
    float breeding = 0.5f;       // how eagerly they breed
    float lifespan = 1.0f;       // multiplier on natural lifespan
    int maxPopulation = 64;
    bool immigration = true;     // newcomers arrive when a kind dies out

    std::array<SpeciesSettings, numRoles> species;

    // world physics
    float gravity = 0.0f;        // 0..1: heavy creatures sink to the bottom
    float wind = 0.0f;           // 0..1
    float windDirection = 0.0f;  // radians (0 = to the right)
    float dayLength = 0.0f;      // seconds of world time per day, 0 = always day
    float seasonLength = 0.0f;   // seconds per season, 0 = fixed season
    int fixedSeason = 0;         // 0 spring 1 summer 2 autumn 3 winter (when seasons are off)
    int weatherMode = 1;         // 0 auto, else 1 + Weather

    LifeSettings();
};

/** Small deterministic random generator (xorshift32). */
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    void seed (uint32_t v) noexcept { s = v ? v : 0x9e3779b9u; }
    uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }          // 0..1
    float bi() noexcept { return uni() * 2.0f - 1.0f; }                                    // -1..1
    float gauss() noexcept { return (uni() + uni() + uni() + uni() - 2.0f) * 0.866f; }     // ~N(0,0.5)
};

/** kind: 0 born, 1 died, 2 eaten, 3 pollinated (a new plant), 4 scavenged, 5 nest built, 6 burnt (lava / meteor), 7 meteor impact, 8 fruit fell */
struct Event { uint8_t kind; float x, y; Role role; };

/** Fixed facts about each kind of creature. */
struct RoleInfo
{
    float speed, weight, burn, ready, cooldown, lifespan, radius;
    bool flies, prey;
    int defaultMax;
};
const RoleInfo& roleInfo (Role) noexcept;

class World
{
public:
    World();

    void reset (uint32_t seed, const std::array<int, numRoles>& counts, int plants);
    void step (float dt, const LifeSettings&);

    // gardener
    uint32_t addCreature (Role, float x, float y, const Genes* genes = nullptr, int generation = 0);
    void removeNear (float x, float y, float radius);
    void addFood (float x, float y, int count, float spread, uint8_t kind = 0);
    void addTree (float x, float y);
    void triggerEvent (WorldEvent, float x, float y);
    void addWall (float x1, float y1, float x2, float y2);
    void addZone (ZoneType, float x, float y, float r, float dx, float dy);
    void removeZoneOrWallNear (float x, float y, float radius);
    void clearWallsAndZones();
    void paintTerrain (float x, float y, float radius, Terrain);
    void clearTerrain();
    bool breed (uint32_t a, uint32_t b);
    uint32_t clone (uint32_t id);
    void setFavourite (uint32_t id, bool on);
    void setGenes (uint32_t id, const Genes&);
    void removeId (uint32_t id);

    Creature* find (uint32_t id) noexcept;
    int count (Role) const noexcept;
    int population() const noexcept;
    int plantCount() const noexcept;
    int corpseCount() const noexcept;
    int nestCount() const noexcept;
    int roleCap (Role, const LifeSettings&) const noexcept;
    Terrain terrainAt (float x, float y) const noexcept;
    static float radiusOf (const Creature&) noexcept;

    Genes randomGenes (Role);
    Genes blend (const Genes& a, const Genes& b, float mutation);

    // state
    std::array<Creature, maxCreatures> creatures;
    std::array<Plant, maxPlants> plants;
    std::array<Corpse, maxCorpses> corpses;
    std::array<Nest, maxNests> nests;
    std::array<Tree, maxTrees> trees;
    int lastEvent = -1;                // last world event (for the screen), and how long ago
    float lastEventAge = 1000.0f;
    std::array<Wall, maxWalls> walls;
    int numWalls = 0;
    std::array<Zone, maxZones> zones;
    int numZones = 0;
    std::array<uint8_t, terrainSize * terrainSize> terrain {};
    uint32_t seedValue = 1, nextId = 1;
    int maxGeneration = 0, births = 0, deaths = 0;
    double time = 0.0;
    Rng rng;

    // environment (read by the sound and the screen)
    float dayPhase = 0.4f;         // 0 midnight .. 0.25 morning .. 0.5 noon .. 0.75 evening
    float light = 1.0f;            // 0 night .. 1 day
    float season = 1.0f;           // 0 spring .. 1 summer .. 2 autumn .. 3 winter (continuous, wraps at 4)
    float temperature = 0.0f;      // -1 cold .. 1 warm
    Weather weather = Weather::Clear, weatherTarget = Weather::Clear;
    float weatherAmount = 0.0f;    // 0..1 (fades in / out)
    float weatherTimer = 0.0f;

    // population history (one sample every 2 s) for the graph
    std::array<std::array<uint8_t, numRoles>, historyLength> history {};      // counts (max 128 fit in a byte)
    int historyPos = 0;
    float historyTimer = 0.0f;

    // recent births / deaths for the display (and Cat Link)
    std::array<Event, 32> events {};
    int eventPos = 0;
    uint32_t eventCount = 0;

private:
    void pushEvent (uint8_t kind, float x, float y, Role role) noexcept;
    void die (Creature&, uint8_t kind, float leftover) noexcept;
    void collideWalls (Creature&) const noexcept;
    void collideRock (Creature&, float oldX, float oldY) const noexcept;
    float zoneFactor (float x, float y, ZoneType) const noexcept;
    void zoneCurrent (float x, float y, float& dx, float& dy) const noexcept;
    void updateEnvironment (float sdt, const LifeSettings&);
    void updateFlocks();
    bool plantAllowedAt (float x, float y) noexcept;
    float immigrationTimer = 0.0f;
};

} // namespace lifecat

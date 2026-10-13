#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <juce_core/juce_core.h>

/**
    Life-Cat world: a small ecosystem.

      Plants   grow by themselves (more in warm zones and where you drop food)
      Grazers  eat plants, flee hunters
      Hunters  eat grazers
      Drifters live on light, float calmly, sing long notes

    Every creature carries genes (0..1). Babies get a blend of both parents' genes plus a
    small mutation, so body, behaviour and SOUND slowly evolve over the generations.
    The world runs on the audio thread with a fixed time step and its own random generator,
    so the same seed gives the same life (and the same music).
*/
namespace lifecat
{
constexpr int maxCreatures = 64;
constexpr int maxPlants = 160;
constexpr int maxWalls = 48;
constexpr int maxZones = 16;
constexpr int historyLength = 240;

enum class Role : uint8_t { Grazer = 0, Hunter, Drifter };
constexpr int numRoles = 3;

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
    numGenes
};

juce::StringArray geneNames();

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
};

struct Plant { bool alive = false; float x = 0.5f, y = 0.5f, size = 0.3f; };
struct Wall { float x1 = 0, y1 = 0, x2 = 0, y2 = 0; };

enum class ZoneType : uint8_t { Warm = 0, Cold, Current };
struct Zone { ZoneType type = ZoneType::Warm; float x = 0.5f, y = 0.5f, r = 0.1f, dx = 0.0f, dy = 0.0f; };

/** How life behaves (the LIFE tab). */
struct LifeSettings
{
    float speed = 1.0f;        // overall life speed
    float foodGrowth = 0.5f;   // plants per second, scaled
    float mutation = 0.25f;    // 0 stable .. 1 wild
    float metabolism = 0.5f;   // how fast energy is used
    float breeding = 0.5f;     // how eagerly they breed
    float lifespan = 1.0f;     // multiplier on natural lifespan
    int maxPopulation = 64;
    bool immigration = true;   // newcomers arrive when the world gets empty
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

struct Event { uint8_t kind; float x, y; Role role; };   // kind: 0 born, 1 died, 2 eaten

class World
{
public:
    World();

    void reset (uint32_t seed, int grazers, int hunters, int drifters, int plants);
    void step (float dt, const LifeSettings&);

    // gardener
    uint32_t addCreature (Role, float x, float y, const Genes* genes = nullptr, int generation = 0);
    void removeNear (float x, float y, float radius);
    void addFood (float x, float y, int count, float spread);
    void addWall (float x1, float y1, float x2, float y2);
    void addZone (ZoneType, float x, float y, float r, float dx, float dy);
    void removeZoneOrWallNear (float x, float y, float radius);
    void clearWallsAndZones();
    bool breed (uint32_t a, uint32_t b);
    uint32_t clone (uint32_t id);
    void setFavourite (uint32_t id, bool on);
    void removeId (uint32_t id);

    Creature* find (uint32_t id) noexcept;
    int count (Role) const noexcept;
    int population() const noexcept;
    int plantCount() const noexcept;
    int roleCap (Role, const LifeSettings&) const noexcept;

    Genes randomGenes (Role);
    Genes blend (const Genes& a, const Genes& b, float mutation);

    // state
    std::array<Creature, maxCreatures> creatures;
    std::array<Plant, maxPlants> plants;
    std::array<Wall, maxWalls> walls;
    int numWalls = 0;
    std::array<Zone, maxZones> zones;
    int numZones = 0;
    uint32_t seedValue = 1, nextId = 1;
    int maxGeneration = 0, births = 0, deaths = 0;
    double time = 0.0;
    Rng rng;

    // population history (one sample every 2 s) for the graph
    std::array<std::array<uint8_t, numRoles>, historyLength> history {};
    int historyPos = 0;
    float historyTimer = 0.0f;

    // recent births / deaths for the display
    std::array<Event, 32> events {};
    int eventPos = 0;

private:
    void pushEvent (uint8_t kind, float x, float y, Role role) noexcept;
    void collideWalls (Creature&) const noexcept;
    float zoneFactor (float x, float y, ZoneType) const noexcept;
    void zoneCurrent (float x, float y, float& dx, float& dy) const noexcept;
    float immigrationTimer = 0.0f;
};

} // namespace lifecat

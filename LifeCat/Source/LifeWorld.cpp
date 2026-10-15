#include "LifeWorld.h"
#include <algorithm>

namespace lifecat
{
juce::StringArray geneNames()
{
    return { "Speed", "Size", "Sight", "Fertility", "Lifespan", "Colour",
             "Timbre", "Brightness", "Harmonics", "Attack", "Release", "Register", "Pitch", "Rhythm", "Density",
             "Vibrato", "Shimmer", "Wobble", "Pan",
             "Social", "Night Owl", "Hardy", "Swimming", "Pattern", "Glow",
             "Interval", "Glide", "Accent", "Echo" };
}

bool isSoundGene (int g) noexcept
{
    return (g >= gTimbre && g <= gPan) || (g >= gInterval && g <= gEcho);
}

const RoleInfo& roleInfo (Role r) noexcept
{
    //                              speed  weight burn   ready  cooldown life  radius   flies  prey   max
    static const RoleInfo info[numRoles] = {
        /* Grazer     */ { 1.00f, 0.80f, 0.85f, 0.62f,  8.0f, 1.0f,  0.000f, false, true,  30 },
        /* Hunter     */ { 1.15f, 0.80f, 1.50f, 0.75f, 14.0f, 1.0f,  0.004f, false, false, 10 },
        /* Drifter    */ { 0.35f, 0.05f, 0.60f, 0.75f, 14.0f, 1.0f,  0.000f, true,  false, 14 },
        /* Pollinator */ { 1.35f, 0.15f, 0.90f, 0.70f, 10.0f, 0.8f, -0.004f, true,  true,  10 },
        /* Scavenger  */ { 0.70f, 0.90f, 0.70f, 0.70f, 16.0f, 1.3f,  0.000f, false, false,  6 },
        /* Giant      */ { 0.30f, 1.00f, 0.55f, 0.90f, 60.0f, 3.0f,  0.022f, false, false,  3 },
        /* Swarm      */ { 1.10f, 0.30f, 0.80f, 0.60f,  7.0f, 0.6f, -0.006f, false, true,  24 },
        /* Singer     */ { 0.90f, 0.30f, 0.75f, 0.75f, 20.0f, 1.6f,  0.004f, true,  false,  6 },
        /* Drummer    */ { 0.80f, 0.70f, 0.80f, 0.70f, 12.0f, 1.1f,  0.000f, false, true,   8 },
        /* Builder    */ { 0.70f, 0.80f, 0.80f, 0.72f, 16.0f, 1.3f,  0.002f, false, true,   8 },
    };
    return info[juce::jlimit (0, numRoles - 1, (int) r)];
}

LifeSettings::LifeSettings()
{
    for (int i = 0; i < numRoles; ++i) species[(size_t) i].maxCount = roleInfo ((Role) i).defaultMax;
}

namespace
{
    float clamp01 (float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    float dist2 (float ax, float ay, float bx, float by) noexcept { const float dx = ax - bx, dy = ay - by; return dx * dx + dy * dy; }
    float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
    constexpr float twoPi = 6.2831853f;

    /** Seasonal value from a 4-entry table (spring, summer, autumn, winter), smoothly wrapped. */
    float seasonal (const float (&t)[4], float season) noexcept
    {
        const float s = std::fmod (season + 4.0f, 4.0f);
        const int a = (int) s, b = (a + 1) % 4;
        return lerp (t[a], t[b], s - (float) a);
    }
}

float World::radiusOf (const Creature& c) noexcept
{
    return std::max (0.004f, 0.010f + 0.016f * c.genes[gSize] + roleInfo (c.role).radius);
}

World::World()
{
    reset (1, { 10, 3, 4, 0, 0, 0, 0 }, 40);
}

Genes World::randomGenes (Role role)
{
    Genes g;
    for (auto& v : g.g) v = rng.uni();
    auto range = [this, &g] (int gene, float lo, float hi) { g[gene] = lo + (hi - lo) * rng.uni(); };
    // a little character per role
    switch (role)
    {
        case Role::Grazer:     range (gTimbre, 0.25f, 0.75f); range (gRhythm, 0.35f, 0.85f); range (gRegister, 0.35f, 0.75f); range (gNight, 0.0f, 0.4f); break;
        case Role::Hunter:     range (gTimbre, 0.1f, 0.6f);   range (gRhythm, 0.15f, 0.55f); range (gRegister, 0.05f, 0.4f); range (gSpeed, 0.4f, 1.0f); range (gNight, 0.3f, 1.0f); break;
        case Role::Drifter:    range (gTimbre, 0.55f, 1.0f);  range (gRhythm, 0.0f, 0.35f);  range (gRegister, 0.55f, 1.0f); range (gAttack, 0.5f, 1.0f);
                               range (gRelease, 0.6f, 1.0f);  range (gNight, 0.5f, 1.0f);    range (gGlow, 0.5f, 1.0f); break;
        case Role::Pollinator: range (gTimbre, 0.4f, 0.6f);   range (gRhythm, 0.6f, 0.9f);   range (gRegister, 0.7f, 1.0f); range (gAttack, 0.0f, 0.2f);
                               range (gRelease, 0.0f, 0.35f); range (gBright, 0.5f, 1.0f);   range (gNight, 0.0f, 0.25f); break;
        case Role::Scavenger:  range (gTimbre, 0.75f, 1.0f);  range (gRhythm, 0.65f, 1.0f);  range (gRegister, 0.3f, 0.6f); range (gAttack, 0.0f, 0.1f);
                               range (gRelease, 0.0f, 0.25f); range (gNight, 0.2f, 0.8f); break;
        case Role::Giant:      range (gTimbre, 0.0f, 0.3f);   range (gRhythm, 0.0f, 0.08f);  range (gRegister, 0.0f, 0.15f); range (gAttack, 0.6f, 1.0f);
                               range (gRelease, 0.7f, 1.0f);  range (gSize, 0.7f, 1.0f);     range (gSpeed, 0.0f, 0.3f); range (gLifespan, 0.6f, 1.0f); break;
        case Role::Swarm:      range (gTimbre, 0.3f, 0.6f);   range (gRhythm, 0.8f, 1.0f);   range (gRegister, 0.4f, 0.8f); range (gRelease, 0.1f, 0.4f);
                               range (gSocial, 0.7f, 1.0f);   range (gSize, 0.0f, 0.4f);     range (gSwim, 0.6f, 1.0f); break;
        case Role::Singer:     range (gTimbre, 0.15f, 0.55f); range (gRhythm, 0.45f, 0.75f); range (gRegister, 0.45f, 0.9f); range (gAttack, 0.05f, 0.4f);
                               range (gRelease, 0.3f, 0.7f);  range (gGlide, 0.3f, 0.9f);    range (gVibrato, 0.4f, 1.0f); range (gSocial, 0.4f, 0.9f); break;
        case Role::Drummer:    range (gTimbre, 0.7f, 1.0f);   range (gRhythm, 0.55f, 0.9f);  range (gRegister, 0.1f, 0.6f); range (gAttack, 0.0f, 0.05f);
                               range (gRelease, 0.0f, 0.3f);  range (gDensity, 0.6f, 1.0f);  range (gAccent, 0.4f, 1.0f); break;
        case Role::Builder:    range (gTimbre, 0.35f, 0.65f); range (gRhythm, 0.3f, 0.6f);   range (gRegister, 0.2f, 0.55f); range (gAttack, 0.0f, 0.15f);
                               range (gRelease, 0.15f, 0.5f); range (gSize, 0.4f, 0.8f);     range (gHardy, 0.4f, 1.0f); break;
    }
    // the newer sound genes start gentle, evolution can push them
    range (gInterval, 0.0f, 0.5f);
    range (gGlide, 0.0f, 0.5f);
    range (gEcho, 0.0f, 0.5f);
    return g;
}

Genes World::blend (const Genes& a, const Genes& b, float mutation)
{
    Genes c;
    for (int i = 0; i < numGenes; ++i)
    {
        const float w = 0.25f + 0.5f * rng.uni();                        // a random mix of both parents
        float v = a[i] * w + b[i] * (1.0f - w);
        v += rng.gauss() * (0.02f + 0.16f * mutation);                    // small drift
        if (rng.uni() < 0.01f + 0.06f * mutation) v = rng.uni();          // rare big mutation
        c[i] = clamp01 (v);
    }
    // colour wraps around the colour wheel
    c[gHue] = std::fmod (c[gHue] + 1.0f, 1.0f);
    return c;
}

void World::reset (uint32_t seed, const std::array<int, numRoles>& counts, int numPlants)
{
    seedValue = seed;
    rng.seed (seed * 2654435761u + 12345u);
    for (auto& c : creatures) c.alive = false;
    for (auto& p : plants) p.alive = false;
    for (auto& k : corpses) k.alive = false;
    for (auto& n : nests) n.alive = false;
    terrain.fill (0);
    nextId = 1;
    maxGeneration = births = deaths = 0;
    time = 0.0;
    historyPos = 0;
    historyTimer = 0.0f;
    for (auto& h : history) h.fill (0);
    eventPos = 0;
    eventCount = 0;
    for (auto& e : events) e.kind = 255;
    immigrationTimer = 0.0f;
    dayPhase = 0.4f;
    light = 1.0f;
    season = 1.0f;
    weather = weatherTarget = Weather::Clear;
    weatherAmount = 0.0f;
    weatherTimer = 20.0f;

    for (int r = 0; r < numRoles; ++r)
        for (int i = 0; i < counts[(size_t) r]; ++i)
            addCreature ((Role) r, 0.1f + 0.8f * rng.uni(), 0.1f + 0.8f * rng.uni());
    for (int i = 0; i < numPlants && i < maxPlants; ++i)
    {
        auto& p = plants[(size_t) i];
        p.alive = true;
        p.x = 0.04f + 0.92f * rng.uni();
        p.y = 0.04f + 0.92f * rng.uni();
        p.size = 0.3f + 0.7f * rng.uni();
    }
}

uint32_t World::addCreature (Role role, float x, float y, const Genes* genes, int generation)
{
    for (auto& c : creatures)
    {
        if (c.alive) continue;
        c = Creature();
        c.alive = true;
        c.id = nextId++;
        c.role = role;
        c.x = juce::jlimit (0.01f, 0.99f, x);
        c.y = juce::jlimit (0.01f, 0.99f, y);
        c.heading = rng.uni() * twoPi;
        c.genes = genes != nullptr ? *genes : randomGenes (role);
        c.energy = 0.6f;
        c.age = 0.0f;
        c.breedCooldown = 6.0f;
        c.generation = generation;
        c.freePhase = rng.uni() * 4.0f;
        c.group = c.id;
        maxGeneration = std::max (maxGeneration, generation);
        pushEvent (0, c.x, c.y, role);
        return c.id;
    }
    return 0;
}

Creature* World::find (uint32_t id) noexcept
{
    if (id == 0) return nullptr;
    for (auto& c : creatures) if (c.alive && c.id == id) return &c;
    return nullptr;
}

int World::count (Role r) const noexcept
{
    int n = 0;
    for (const auto& c : creatures) n += (c.alive && c.role == r) ? 1 : 0;
    return n;
}

int World::population() const noexcept
{
    int n = 0;
    for (const auto& c : creatures) n += c.alive ? 1 : 0;
    return n;
}

int World::plantCount() const noexcept
{
    int n = 0;
    for (const auto& p : plants) n += p.alive ? 1 : 0;
    return n;
}

int World::nestCount() const noexcept
{
    int n = 0;
    for (const auto& k : nests) n += k.alive ? 1 : 0;
    return n;
}

int World::corpseCount() const noexcept
{
    int n = 0;
    for (const auto& k : corpses) n += k.alive ? 1 : 0;
    return n;
}

void World::pushEvent (uint8_t kind, float x, float y, Role role) noexcept
{
    events[(size_t) eventPos] = { kind, x, y, role };
    eventPos = (eventPos + 1) % (int) events.size();
    ++eventCount;
}

void World::die (Creature& c, uint8_t kind, float leftover) noexcept
{
    c.alive = false;
    ++deaths;
    pushEvent (kind, c.x, c.y, c.role);
    if (leftover > 0.05f)
        for (auto& k : corpses)
            if (! k.alive)
            {
                k = { true, c.x, c.y, leftover, 0.0f, c.role };
                break;
            }
}

Terrain World::terrainAt (float x, float y) const noexcept
{
    const int cx = juce::jlimit (0, terrainSize - 1, (int) (x * (float) terrainSize));
    const int cy = juce::jlimit (0, terrainSize - 1, (int) (y * (float) terrainSize));
    return (Terrain) terrain[(size_t) (cy * terrainSize + cx)];
}

void World::paintTerrain (float x, float y, float radius, Terrain t)
{
    for (int cy = 0; cy < terrainSize; ++cy)
        for (int cx = 0; cx < terrainSize; ++cx)
        {
            const float px = ((float) cx + 0.5f) / (float) terrainSize, py = ((float) cy + 0.5f) / (float) terrainSize;
            if (dist2 (px, py, x, y) <= radius * radius + 0.25f / (float) (terrainSize * terrainSize))
                terrain[(size_t) (cy * terrainSize + cx)] = (uint8_t) t;
        }
    // nothing grows inside rock or lava
    if (t == Terrain::Rock || t == Terrain::Lava)
        for (auto& p : plants)
            if (p.alive && terrainAt (p.x, p.y) == t) p.alive = false;
}

void World::clearTerrain() { terrain.fill (0); }

bool World::plantAllowedAt (float x, float y) noexcept
{
    switch (terrainAt (x, y))
    {
        case Terrain::Rock:
        case Terrain::Lava:   return false;
        case Terrain::Sand:   return rng.uni() < 0.12f;
        case Terrain::Water:  return rng.uni() < 0.25f;
        case Terrain::Meadow: return true;
        case Terrain::Light:  return rng.uni() < 0.9f;
        case Terrain::Ground:
        default:              return rng.uni() < 0.55f;
    }
}

void World::removeNear (float x, float y, float radius)
{
    for (auto& c : creatures)
        if (c.alive && dist2 (c.x, c.y, x, y) < radius * radius)
        {
            c.alive = false;
            ++deaths;
            pushEvent (1, c.x, c.y, c.role);
        }
    for (auto& p : plants)
        if (p.alive && dist2 (p.x, p.y, x, y) < radius * radius) p.alive = false;
    for (auto& k : corpses)
        if (k.alive && dist2 (k.x, k.y, x, y) < radius * radius) k.alive = false;
    for (auto& n : nests)
        if (n.alive && dist2 (n.x, n.y, x, y) < radius * radius) n.alive = false;
}

void World::removeId (uint32_t id)
{
    if (auto* c = find (id)) { c->alive = false; ++deaths; pushEvent (1, c->x, c->y, c->role); }
}

void World::addFood (float x, float y, int n, float spread)
{
    for (int k = 0; k < n; ++k)
        for (auto& p : plants)
            if (! p.alive)
            {
                p.alive = true;
                p.x = juce::jlimit (0.01f, 0.99f, x + spread * rng.bi());
                p.y = juce::jlimit (0.01f, 0.99f, y + spread * rng.bi());
                p.size = 0.6f + 0.4f * rng.uni();
                break;
            }
}

void World::addWall (float x1, float y1, float x2, float y2)
{
    if (numWalls >= maxWalls) return;
    walls[(size_t) numWalls++] = { x1, y1, x2, y2 };
}

void World::addZone (ZoneType t, float x, float y, float r, float dx, float dy)
{
    if (numZones >= maxZones) return;
    zones[(size_t) numZones++] = { t, x, y, r, dx, dy };
}

void World::removeZoneOrWallNear (float x, float y, float radius)
{
    for (int i = numZones; --i >= 0;)
        if (dist2 (zones[(size_t) i].x, zones[(size_t) i].y, x, y) < std::pow (zones[(size_t) i].r + radius, 2.0f))
        {
            zones[(size_t) i] = zones[(size_t) numZones - 1];
            --numZones;
        }
    for (int i = numWalls; --i >= 0;)
    {
        const auto& w = walls[(size_t) i];
        const float vx = w.x2 - w.x1, vy = w.y2 - w.y1;
        const float t = juce::jlimit (0.0f, 1.0f, ((x - w.x1) * vx + (y - w.y1) * vy) / std::max (1.0e-6f, vx * vx + vy * vy));
        if (dist2 (w.x1 + t * vx, w.y1 + t * vy, x, y) < radius * radius)
        {
            walls[(size_t) i] = walls[(size_t) numWalls - 1];
            --numWalls;
        }
    }
}

void World::clearWallsAndZones() { numWalls = numZones = 0; }

bool World::breed (uint32_t a, uint32_t b)
{
    auto* pa = find (a);
    auto* pb = find (b);
    if (pa == nullptr || pb == nullptr || pa == pb) return false;
    const auto genes = blend (pa->genes, pb->genes, 0.25f);
    const uint32_t id = addCreature (pa->role, 0.5f * (pa->x + pb->x), 0.5f * (pa->y + pb->y), &genes,
                                     std::max (pa->generation, pb->generation) + 1);
    if (auto* c = find (id)) { c->parentA = a; c->parentB = b; ++births; }
    return id != 0;
}

uint32_t World::clone (uint32_t id)
{
    auto* p = find (id);
    if (p == nullptr) return 0;
    const auto genes = p->genes;
    const uint32_t nid = addCreature (p->role, p->x + 0.03f * rng.bi(), p->y + 0.03f * rng.bi(), &genes, p->generation);
    if (auto* c = find (nid)) { c->parentA = id; ++births; }
    return nid;
}

void World::setFavourite (uint32_t id, bool on)
{
    if (auto* c = find (id)) c->favourite = on;
}

void World::setGenes (uint32_t id, const Genes& g)
{
    if (auto* c = find (id)) c->genes = g;
}

float World::zoneFactor (float x, float y, ZoneType t) const noexcept
{
    float f = 0.0f;
    for (int i = 0; i < numZones; ++i)
    {
        const auto& z = zones[(size_t) i];
        if (z.type != t) continue;
        const float d = std::sqrt (dist2 (x, y, z.x, z.y));
        if (d < z.r) f = std::max (f, 1.0f - d / z.r);
    }
    return f;
}

void World::zoneCurrent (float x, float y, float& dx, float& dy) const noexcept
{
    for (int i = 0; i < numZones; ++i)
    {
        const auto& z = zones[(size_t) i];
        if (z.type != ZoneType::Current) continue;
        const float d = std::sqrt (dist2 (x, y, z.x, z.y));
        if (d < z.r)
        {
            const float w = 1.0f - d / z.r;
            dx += z.dx * w;
            dy += z.dy * w;
        }
    }
}

void World::collideWalls (Creature& c) const noexcept
{
    const float r = radiusOf (c);
    for (int i = 0; i < numWalls; ++i)
    {
        const auto& w = walls[(size_t) i];
        const float vx = w.x2 - w.x1, vy = w.y2 - w.y1;
        const float len2 = std::max (1.0e-6f, vx * vx + vy * vy);
        const float t = juce::jlimit (0.0f, 1.0f, ((c.x - w.x1) * vx + (c.y - w.y1) * vy) / len2);
        const float px = w.x1 + t * vx, py = w.y1 + t * vy;
        const float dx = c.x - px, dy = c.y - py;
        const float d = std::sqrt (dx * dx + dy * dy);
        if (d < r + 0.004f && d > 1.0e-6f)
        {
            const float nx = dx / d, ny = dy / d;
            c.x = px + nx * (r + 0.004f);
            c.y = py + ny * (r + 0.004f);
            const float vn = c.vx * nx + c.vy * ny;
            if (vn < 0.0f) { c.vx -= 1.8f * vn * nx; c.vy -= 1.8f * vn * ny; }
        }
    }
}

void World::collideRock (Creature& c, float oldX, float oldY) const noexcept
{
    if (roleInfo (c.role).flies || terrainAt (c.x, c.y) != Terrain::Rock) return;
    // slide along the rock if one axis is free, else bounce back
    if (terrainAt (c.x, oldY) != Terrain::Rock) { c.y = oldY; c.vy = -0.5f * c.vy; }
    else if (terrainAt (oldX, c.y) != Terrain::Rock) { c.x = oldX; c.vx = -0.5f * c.vx; }
    else { c.x = oldX; c.y = oldY; c.vx = -0.5f * c.vx; c.vy = -0.5f * c.vy; c.heading += 3.14159f; }
}

int World::roleCap (Role r, const LifeSettings& s) const noexcept
{
    return std::max (0, std::min ({ maxCreatures, s.maxPopulation, s.species[(size_t) r].maxCount }));
}

void World::updateEnvironment (float sdt, const LifeSettings& s)
{
    // day and night
    if (s.dayLength > 0.5f)
    {
        dayPhase = std::fmod (dayPhase + sdt / s.dayLength, 1.0f);
        const float raw = 0.5f - 0.5f * std::cos (twoPi * dayPhase);      // 0 midnight, 1 noon
        light = clamp01 ((raw - 0.22f) / 0.56f);
    }
    else
    {
        dayPhase = 0.45f;
        light = 1.0f;
    }

    // seasons
    if (s.seasonLength > 0.5f) season = std::fmod (season + sdt / s.seasonLength, 4.0f);
    else
    {
        const float target = (float) juce::jlimit (0, 3, s.fixedSeason);
        float d = std::remainder (target - season, 4.0f);
        season = std::fmod (season + d * std::min (1.0f, sdt * 0.5f) + 4.0f, 4.0f);
    }
    temperature = std::cos ((season - 1.0f) * 1.5707963f);               // summer +1, winter -1

    // weather
    if (s.weatherMode > 0) weatherTarget = (Weather) juce::jlimit (0, numWeathers - 1, s.weatherMode - 1);
    else
    {
        weatherTimer -= sdt;
        if (weatherTimer <= 0.0f)
        {
            weatherTimer = 30.0f + 40.0f * rng.uni();
            //                                   clear  rain   snow   fog    storm
            static const float odds[4][5] = { { 0.50f, 0.35f, 0.00f, 0.15f, 0.00f },     // spring
                                              { 0.65f, 0.15f, 0.00f, 0.00f, 0.20f },     // summer
                                              { 0.35f, 0.35f, 0.00f, 0.25f, 0.05f },     // autumn
                                              { 0.35f, 0.00f, 0.45f, 0.20f, 0.00f } };   // winter
            const int si = juce::jlimit (0, 3, (int) (season + 0.5f) % 4);
            float u = rng.uni();
            int pick = 0;
            for (int i = 0; i < numWeathers; ++i) { u -= odds[si][i]; if (u <= 0.0f) { pick = i; break; } }
            weatherTarget = (Weather) pick;
        }
    }
    if (weatherTarget != weather)
    {
        weatherAmount -= sdt / 4.0f;
        if (weatherAmount <= 0.0f) { weatherAmount = 0.0f; weather = weatherTarget; }
    }
    else if (weather != Weather::Clear) weatherAmount = std::min (1.0f, weatherAmount + sdt / 4.0f);
    else weatherAmount = 0.0f;
}

void World::updateFlocks()
{
    // flocks = swarm members connected within a short distance; id = smallest member id
    for (auto& c : creatures) if (c.alive) c.group = c.id;
    for (int pass = 0; pass < 4; ++pass)
        for (auto& a : creatures)
        {
            if (! a.alive || a.role != Role::Swarm) continue;
            for (const auto& b : creatures)
                if (b.alive && b.role == Role::Swarm && b.group < a.group && dist2 (a.x, a.y, b.x, b.y) < 0.13f * 0.13f)
                    a.group = b.group;
        }
    for (auto& a : creatures)
    {
        if (! a.alive || a.role != Role::Swarm) continue;
        int rank = 0;
        for (const auto& b : creatures)
            if (b.alive && b.role == Role::Swarm && b.group == a.group && b.id < a.id) ++rank;
        a.groupRank = rank;
    }
}

void World::step (float dt, const LifeSettings& s)
{
    const float sdt = dt * s.speed;
    if (sdt <= 0.0f) return;
    time += sdt;
    updateEnvironment (sdt, s);

    static const float growthBySeason[4] = { 1.4f, 1.0f, 0.6f, 0.15f };
    const float wAmt = weatherAmount;
    const bool rain = weather == Weather::Rain || weather == Weather::Storm;
    const float weatherGrowth = rain ? 1.0f + 0.8f * wAmt : (weather == Weather::Snow ? 1.0f - 0.7f * wAmt : 1.0f);
    const float growth = seasonal (growthBySeason, season) * weatherGrowth;
    const float winterChill = std::max (0.0f, -temperature) * 0.7f + (weather == Weather::Snow ? 0.5f * wAmt : 0.0f);
    const float fog = weather == Weather::Fog ? wAmt : 0.0f;
    const float gust = weather == Weather::Storm ? wAmt * (0.6f + 0.4f * std::sin ((float) time * 1.7f)) : 0.0f;
    const float windX = (s.wind + gust) * std::cos (s.windDirection), windY = (s.wind + gust) * std::sin (s.windDirection);
    const bool dayCycle = s.dayLength > 0.5f;

    // ---- plants grow (and wither in winter)
    float expected = (0.4f + 6.0f * s.foodGrowth) * growth * sdt;
    while (expected > 0.0f)
    {
        if (rng.uni() < std::min (1.0f, expected) && plantCount() < maxPlants * 3 / 4)
        {
            for (int tries = 0; tries < 6; ++tries)
            {
                float x = 0.03f + 0.94f * rng.uni(), y = 0.03f + 0.94f * rng.uni();
                // warm zones are fertile: prefer them
                if (numZones > 0 && rng.uni() < 0.5f)
                    for (int i = 0; i < numZones; ++i)
                        if (zones[(size_t) i].type == ZoneType::Warm && rng.uni() < 0.6f)
                        {
                            const auto& z = zones[(size_t) i];
                            x = juce::jlimit (0.02f, 0.98f, z.x + z.r * rng.bi() * 0.8f);
                            y = juce::jlimit (0.02f, 0.98f, z.y + z.r * rng.bi() * 0.8f);
                            break;
                        }
                // builders' nests are gardens too
                if (rng.uni() < 0.3f)
                    for (const auto& n : nests)
                        if (n.alive && n.size > 0.5f && rng.uni() < 0.5f)
                        {
                            x = juce::jlimit (0.02f, 0.98f, n.x + 0.08f * rng.bi());
                            y = juce::jlimit (0.02f, 0.98f, n.y + 0.08f * rng.bi());
                            break;
                        }
                if (plantAllowedAt (x, y)) { addFood (x, y, 1, 0.0f); break; }
            }
        }
        expected -= 1.0f;
    }
    for (auto& p : plants)
    {
        if (! p.alive) continue;
        p.size = std::min (1.0f, p.size + 0.05f * growth * sdt);
        if (winterChill > 0.4f)
        {
            p.size -= 0.015f * winterChill * sdt;
            if (p.size < 0.08f) p.alive = false;
        }
    }

    // ---- what dies feeds the ground
    for (auto& k : corpses)
    {
        if (! k.alive) continue;
        k.age += sdt;
        if (k.food <= 0.02f) { k.alive = false; continue; }
        if (k.age > 25.0f)
        {
            k.alive = false;
            if (rng.uni() < 0.7f && plantAllowedAt (k.x, k.y)) addFood (k.x, k.y, 1 + (rng.uni() < 0.4f ? 1 : 0), 0.02f);
        }
    }

    // nests: grow while their builder is home, crumble without one
    for (auto& n : nests)
    {
        if (! n.alive) continue;
        const auto* owner = find (n.owner);
        if (owner == nullptr) { n.size -= 0.01f * sdt; if (n.size <= 0.0f) n.alive = false; }
        else if (dist2 (owner->x, owner->y, n.x, n.y) < 0.05f * 0.05f) n.size = std::min (1.0f, n.size + 0.08f * sdt);
    }

    updateFlocks();

    // ---- creatures
    for (auto& c : creatures)
    {
        if (! c.alive) continue;
        const auto& g = c.genes;
        const auto& info = roleInfo (c.role);
        const auto& sp = s.species[(size_t) c.role];
        const float cold = std::min (1.0f, std::max (zoneFactor (c.x, c.y, ZoneType::Cold), winterChill) * (1.0f - 0.7f * g[gHardy]));
        const float warm = zoneFactor (c.x, c.y, ZoneType::Warm);
        const Terrain here = terrainAt (c.x, c.y);

        // awake by day (or by night for night owls)
        c.activity = dayCycle ? juce::jlimit (0.12f, 1.0f, lerp (light, 1.0f - light, g[gNight]) * 1.2f) : 1.0f;

        float terrainSpeed = 1.0f;
        if (here == Terrain::Water && ! info.flies)
            terrainSpeed = c.role == Role::Swarm ? 1.1f : 0.35f + 0.65f * g[gSwim];
        if (here == Terrain::Sand && ! info.flies) terrainSpeed = 0.55f + 0.2f * g[gHardy];
        const float maxSpeed = (0.025f + 0.11f * g[gSpeed]) * info.speed * sp.speed
                             * (1.0f - 0.55f * cold) * (1.0f + 0.25f * warm) * (0.3f + 0.7f * c.activity) * terrainSpeed;
        const float sight = (0.07f + 0.25f * g[gSight]) * (1.0f - 0.5f * fog);
        float ax = 0.0f, ay = 0.0f;

        // wander
        c.heading += rng.bi() * 3.0f * sdt;
        ax += std::cos (c.heading) * 0.5f;
        ay += std::sin (c.heading) * 0.5f;

        // keep away from lava (fliers too: it is hot up there)
        for (int k = 0; k < 4; ++k)
        {
            const float a = c.heading + (float) k * 1.5707963f;
            const float px = c.x + std::cos (a) * 0.045f, py = c.y + std::sin (a) * 0.045f;
            if (terrainAt (px, py) == Terrain::Lava) { ax -= 2.5f * std::cos (a); ay -= 2.5f * std::sin (a); }
        }

        // blinking (and eyes shut while asleep)
        if (rng.uni() < 0.35f * sdt) c.blink = 1.0f;
        c.blink = std::max (0.0f, c.blink - 7.0f * sdt);

        auto seek = [&] (float tx, float ty, float weight)
        {
            const float d = std::sqrt (dist2 (c.x, c.y, tx, ty)) + 1.0e-5f;
            ax += weight * (tx - c.x) / d;
            ay += weight * (ty - c.y) / d;
            return d;
        };
        auto flee = [&] (Role from, float weight)
        {
            for (const auto& h : creatures)
                if (h.alive && h.role == from)
                {
                    const float d2 = dist2 (c.x, c.y, h.x, h.y);
                    if (d2 < sight * sight)
                    {
                        const float d = std::sqrt (d2) + 1.0e-5f;
                        ax += weight * (c.x - h.x) / d;
                        ay += weight * (c.y - h.y) / d;
                    }
                }
        };
        auto nearestPlant = [&] (float range, float minSize) -> int
        {
            float best = range * range;
            int bi = -1;
            for (int i = 0; i < maxPlants; ++i)
                if (plants[(size_t) i].alive && plants[(size_t) i].size >= minSize)
                {
                    const float d = dist2 (c.x, c.y, plants[(size_t) i].x, plants[(size_t) i].y);
                    if (d < best) { best = d; bi = i; }
                }
            return bi;
        };

        switch (c.role)
        {
            case Role::Grazer:
            {
                const int bi = nearestPlant (sight, 0.0f);
                if (bi >= 0 && c.energy < 0.95f)
                {
                    auto& p = plants[(size_t) bi];
                    if (seek (p.x, p.y, 2.0f) < radiusOf (c) + 0.008f)
                    {
                        p.alive = false;
                        c.energy = std::min (1.2f, c.energy + 0.18f + 0.12f * p.size);
                    }
                }
                flee (Role::Hunter, 3.0f);
                break;
            }
            case Role::Hunter:
            {
                float best = sight * sight;
                Creature* prey = nullptr;
                for (auto& o : creatures)
                    if (o.alive && roleInfo (o.role).prey)
                    {
                        // fast fliers are hard to catch: hunters prefer the easier prey
                        const float d = dist2 (c.x, c.y, o.x, o.y) * (o.role == Role::Pollinator ? 4.0f : (o.role == Role::Swarm ? 1.5f : 1.0f));
                        if (d < best) { best = d; prey = &o; }
                    }
                if (prey != nullptr && c.energy < 0.8f)          // only hunts when hungry
                {
                    const float d = seek (prey->x, prey->y, 2.6f);
                    if (d < radiusOf (c) + radiusOf (*prey) && c.breedCooldown < 10.0f)
                    {
                        // not every pounce succeeds (and favourites almost always escape)
                        const float agility = prey->role == Role::Pollinator ? 0.5f : (prey->role == Role::Swarm ? 0.7f : 1.0f);
                        const float chance = prey->favourite ? 0.01f : (0.25f + 0.25f * (c.genes[gSpeed] - prey->genes[gSpeed] + 0.5f)) * agility;
                        if (rng.uni() < chance * sdt * 30.0f)
                        {
                            c.energy = std::min (1.3f, c.energy + 0.45f + 0.2f * prey->genes[gSize]);
                            die (*prey, 2, 0.15f + 0.1f * prey->genes[gSize]);
                        }
                    }
                }
                flee (Role::Giant, 1.5f);
                break;
            }
            case Role::Drifter:
            {
                // drifters float together loosely
                float cx = 0.0f, cy = 0.0f;
                int n = 0;
                for (const auto& o : creatures)
                    if (o.alive && o.role == Role::Drifter && &o != &c && dist2 (c.x, c.y, o.x, o.y) < 4.0f * sight * sight)
                    { cx += o.x; cy += o.y; ++n; }
                if (n > 0) { ax += 0.6f * (cx / (float) n - c.x) / sight; ay += 0.6f * (cy / (float) n - c.y) / sight; }
                const float lit = here == Terrain::Light ? 0.025f : 0.0f;
                c.energy = std::min (1.1f, c.energy + (0.018f + 0.03f * warm + lit) * (0.3f + 0.7f * (light + g[gGlow] * (1.0f - light) * 0.6f)) * sdt);
                break;
            }
            case Role::Pollinator:
            {
                c.visitCooldown -= sdt;
                if (c.visitCooldown <= 0.0f && c.energy < 1.0f)
                {
                    const int bi = nearestPlant (sight * 1.5f, 0.45f);
                    if (bi >= 0)
                    {
                        auto& p = plants[(size_t) bi];
                        if (seek (p.x, p.y, 2.2f) < radiusOf (c) + 0.01f)
                        {
                            c.energy = std::min (1.15f, c.energy + 0.1f + 0.1f * p.size);
                            p.size -= 0.2f;
                            if (p.size < 0.1f) p.alive = false;
                            c.visitCooldown = 1.5f + 2.0f * rng.uni();
                            c.heading = rng.uni() * twoPi;
                            // carry pollen: a new plant appears nearby
                            if (rng.uni() < 0.55f && plantCount() < maxPlants * 3 / 4)
                            {
                                const float nx = juce::jlimit (0.02f, 0.98f, p.x + 0.07f * rng.bi()), ny = juce::jlimit (0.02f, 0.98f, p.y + 0.07f * rng.bi());
                                if (plantAllowedAt (nx, ny)) { addFood (nx, ny, 1, 0.0f); pushEvent (3, nx, ny, c.role); }
                            }
                        }
                    }
                }
                if (here == Terrain::Light) c.energy = std::min (1.1f, c.energy + 0.01f * sdt);
                flee (Role::Hunter, 2.5f);
                break;
            }
            case Role::Scavenger:
            {
                float best = (sight * 1.6f) * (sight * 1.6f);
                Corpse* food = nullptr;
                for (auto& k : corpses)
                    if (k.alive)
                    {
                        const float d = dist2 (c.x, c.y, k.x, k.y);
                        if (d < best) { best = d; food = &k; }
                    }
                if (food != nullptr && c.energy < 1.1f)
                {
                    if (seek (food->x, food->y, 2.4f) < radiusOf (c) + 0.012f)
                    {
                        const float take = std::min (food->food, 0.35f * sdt);
                        food->food -= take;
                        c.energy = std::min (1.25f, c.energy + 0.9f * take);
                        if (food->food <= 0.02f) { food->alive = false; pushEvent (4, food->x, food->y, c.role); }
                    }
                }
                else if (c.energy < 0.45f)
                {
                    // leftovers: nibble a plant now and then
                    const int bi = nearestPlant (sight, 0.0f);
                    if (bi >= 0)
                    {
                        auto& p = plants[(size_t) bi];
                        if (seek (p.x, p.y, 1.5f) < radiusOf (c) + 0.008f) { p.alive = false; c.energy += 0.08f + 0.05f * p.size; }
                    }
                }
                break;
            }
            case Role::Giant:
            {
                // crunches every plant it walks over
                const float reach = radiusOf (c) + 0.012f;
                for (auto& p : plants)
                    if (p.alive && dist2 (c.x, c.y, p.x, p.y) < reach * reach)
                    {
                        p.alive = false;
                        c.energy = std::min (1.3f, c.energy + 0.05f + 0.05f * p.size);
                    }
                if (c.energy < 0.75f)
                {
                    const int bi = nearestPlant (sight * 1.3f, 0.3f);
                    if (bi >= 0) seek (plants[(size_t) bi].x, plants[(size_t) bi].y, 1.2f);
                }
                break;
            }
            case Role::Swarm:
            {
                // flocking: keep apart, fly the same way, stay together
                float cx = 0.0f, cy = 0.0f, avx = 0.0f, avy = 0.0f;
                int n = 0;
                for (const auto& o : creatures)
                {
                    if (! o.alive || o.role != Role::Swarm || &o == &c) continue;
                    const float d2 = dist2 (c.x, c.y, o.x, o.y);
                    if (d2 > 0.12f * 0.12f) continue;
                    cx += o.x; cy += o.y; avx += o.vx; avy += o.vy; ++n;
                    if (d2 < 0.022f * 0.022f)
                    {
                        const float d = std::sqrt (d2) + 1.0e-5f;
                        ax += 1.6f * (c.x - o.x) / d;
                        ay += 1.6f * (c.y - o.y) / d;
                    }
                }
                if (n > 0)
                {
                    const float social = 0.6f + 1.4f * g[gSocial];
                    ax += social * (cx / (float) n - c.x) / 0.12f;
                    ay += social * (cy / (float) n - c.y) / 0.12f;
                    const float vl = std::sqrt (avx * avx + avy * avy) + 1.0e-6f;
                    ax += social * avx / vl;
                    ay += social * avy / vl;
                }
                if (c.energy < 0.85f)
                {
                    const int bi = nearestPlant (sight, 0.0f);
                    if (bi >= 0)
                    {
                        auto& p = plants[(size_t) bi];
                        if (seek (p.x, p.y, 1.4f) < radiusOf (c) + 0.01f)
                        {
                            p.size -= 0.25f;
                            if (p.size < 0.1f) p.alive = false;
                            c.energy = std::min (1.1f, c.energy + 0.09f);
                        }
                    }
                }
                flee (Role::Hunter, 2.6f);
                break;
            }
            default: break;
        }

        switch (c.role)
        {
            case Role::Singer:
            {
                // roam, rest in the light, nibble plants (birds eat seeds, whales krill)
                if (c.energy < 0.7f)
                {
                    const int bi = nearestPlant (sight * 1.4f, 0.0f);
                    if (bi >= 0)
                    {
                        auto& p = plants[(size_t) bi];
                        if (seek (p.x, p.y, 1.6f) < radiusOf (c) + 0.01f)
                        {
                            p.size -= 0.3f;
                            if (p.size < 0.1f) p.alive = false;
                            c.energy = std::min (1.2f, c.energy + 0.12f);
                        }
                    }
                }
                c.energy = std::min (1.15f, c.energy + (here == Terrain::Light ? 0.012f : 0.004f) * light * sdt);
                flee (Role::Hunter, 1.5f);
                break;
            }
            case Role::Drummer:
            {
                const int bi = nearestPlant (sight, 0.0f);
                if (bi >= 0 && c.energy < 0.9f)
                {
                    auto& p = plants[(size_t) bi];
                    if (seek (p.x, p.y, 1.8f) < radiusOf (c) + 0.008f)
                    {
                        p.alive = false;
                        c.energy = std::min (1.2f, c.energy + 0.16f + 0.1f * p.size);
                    }
                }
                // drummers like hard ground: rocks to tap on
                if (here == Terrain::Rock || here == Terrain::Sand) c.energy = std::min (1.2f, c.energy + 0.003f * sdt);
                flee (Role::Hunter, 2.8f);
                break;
            }
            case Role::Builder:
            {
                Nest* home = c.nest > 0 && c.nest <= (uint32_t) maxNests && nests[(size_t) c.nest - 1].alive ? &nests[(size_t) c.nest - 1] : nullptr;
                if (home == nullptr) c.nest = 0;
                if (home == nullptr && c.energy > 0.7f && c.age > 4.0f
                    && here != Terrain::Water && here != Terrain::Lava && here != Terrain::Rock)
                {
                    bool crowded = false;
                    for (const auto& n : nests) crowded = crowded || (n.alive && dist2 (n.x, n.y, c.x, c.y) < 0.12f * 0.12f);
                    if (! crowded)
                        for (int i = 0; i < maxNests; ++i)
                            if (! nests[(size_t) i].alive)
                            {
                                nests[(size_t) i] = { true, c.x, c.y, 0.05f, c.id };
                                c.nest = (uint32_t) i + 1;
                                c.energy -= 0.15f;
                                pushEvent (5, c.x, c.y, c.role);
                                break;
                            }
                }
                const int bi = nearestPlant (sight, 0.0f);
                if (bi >= 0 && c.energy < 0.85f)
                {
                    auto& p = plants[(size_t) bi];
                    if (seek (p.x, p.y, 1.8f) < radiusOf (c) + 0.008f)
                    {
                        p.alive = false;
                        c.energy = std::min (1.2f, c.energy + 0.15f + 0.1f * p.size);
                    }
                }
                else if (home != nullptr && (c.energy > 0.85f || c.activity < 0.5f))
                {
                    // go home, plant seeds around it
                    if (seek (home->x, home->y, 1.6f) < 0.04f && rng.uni() < 0.25f * sdt && plantCount() < maxPlants * 3 / 4)
                    {
                        const float nx = juce::jlimit (0.02f, 0.98f, home->x + 0.07f * rng.bi()), ny = juce::jlimit (0.02f, 0.98f, home->y + 0.07f * rng.bi());
                        if (plantAllowedAt (nx, ny)) { addFood (nx, ny, 1, 0.0f); c.energy -= 0.03f; pushEvent (3, nx, ny, c.role); }
                    }
                }
                // meadow trails where builders walk
                if ((here == Terrain::Ground || here == Terrain::Sand) && rng.uni() < 0.35f * sdt)
                {
                    const int cx = juce::jlimit (0, terrainSize - 1, (int) (c.x * (float) terrainSize));
                    const int cy = juce::jlimit (0, terrainSize - 1, (int) (c.y * (float) terrainSize));
                    terrain[(size_t) (cy * terrainSize + cx)] = (uint8_t) Terrain::Meadow;
                }
                flee (Role::Hunter, 2.6f);
                break;
            }
            default: break;
        }

        // the social ones stay near their own kind (swarms do this above)
        if (c.role != Role::Swarm && g[gSocial] > 0.35f)
        {
            float cx = 0.0f, cy = 0.0f;
            int n = 0;
            for (const auto& o : creatures)
                if (o.alive && o.role == c.role && &o != &c && dist2 (c.x, c.y, o.x, o.y) < sight * sight)
                { cx += o.x; cy += o.y; ++n; }
            if (n > 0)
            {
                const float w = (g[gSocial] - 0.35f) * 1.4f;
                ax += w * (cx / (float) n - c.x) / sight;
                ay += w * (cy / (float) n - c.y) / sight;
            }
        }
        // everybody makes way for giants
        if (c.role != Role::Giant)
            for (const auto& o : creatures)
                if (o.alive && o.role == Role::Giant)
                {
                    const float r = radiusOf (o) + 0.03f;
                    const float d2 = dist2 (c.x, c.y, o.x, o.y);
                    if (d2 < r * r)
                    {
                        const float d = std::sqrt (d2) + 1.0e-5f;
                        ax += 2.5f * (c.x - o.x) / d;
                        ay += 2.5f * (c.y - o.y) / d;
                    }
                }

        // ready to breed: look for a partner of the same kind
        if (c.breedCooldown <= 0.0f && c.energy >= info.ready && c.age >= 6.0f)
        {
            float best = (sight * 2.0f) * (sight * 2.0f);
            const Creature* mate = nullptr;
            for (const auto& o : creatures)
                if (o.alive && &o != &c && o.role == c.role && o.age >= 6.0f && o.breedCooldown <= 0.0f)
                {
                    const float d = dist2 (c.x, c.y, o.x, o.y);
                    if (d < best) { best = d; mate = &o; }
                }
            if (mate != nullptr) seek (mate->x, mate->y, 1.6f);
        }

        // edges push back
        if (c.x < 0.05f) ax += 2.0f; if (c.x > 0.95f) ax -= 2.0f;
        if (c.y < 0.05f) ay += 2.0f; if (c.y > 0.95f) ay -= 2.0f;

        // steer
        const float alen = std::sqrt (ax * ax + ay * ay) + 1.0e-6f;
        const float steer = 1.0f - std::exp (-4.0f * sdt);
        c.vx += (ax / alen * maxSpeed - c.vx) * steer;
        c.vy += (ay / alen * maxSpeed - c.vy) * steer;

        // currents, wind and gravity carry them along
        float cdx = 0.0f, cdy = 0.0f;
        zoneCurrent (c.x, c.y, cdx, cdy);
        const float drag = 1.15f - info.weight;
        const float oldX = c.x, oldY = c.y;
        c.x += (c.vx + cdx * 0.08f + windX * 0.07f * drag) * sdt;
        c.y += (c.vy + cdy * 0.08f + windY * 0.07f * drag + s.gravity * info.weight * 0.07f) * sdt;
        c.x = juce::jlimit (0.01f, 0.99f, c.x);
        c.y = juce::jlimit (0.01f, 0.99f, c.y);
        collideWalls (c);
        collideRock (c, oldX, oldY);
        if (std::abs (c.vx) + std::abs (c.vy) > 1.0e-4f)
        {
            // turn towards the direction of travel (the short way round)
            const float d = std::remainder (std::atan2 (c.vy, c.vx) - c.heading, twoPi);
            c.heading = std::remainder (c.heading + 0.2f * d, twoPi);
        }

        // metabolism (sleeping saves energy, cold costs energy)
        const float burn = (0.004f + 0.022f * g[gSpeed] * g[gSpeed] + 0.010f * g[gSize]) * (0.4f + 1.2f * s.metabolism)
                         * info.burn * (1.0f + 0.6f * cold) * (0.45f + 0.55f * c.activity) * (1.0f - 0.3f * warm);
        c.energy -= burn * sdt;
        if (here == Terrain::Lava && ! info.flies) c.energy -= 0.35f * sdt;
        c.age += sdt;
        c.breedCooldown -= sdt * (0.5f + s.breeding) * (1.0f + 0.5f * warm);
        c.singFlash = std::max (0.0f, c.singFlash - 3.0f * dt);

        const float lifespan = (35.0f + 110.0f * g[gLifespan]) * s.lifespan * info.lifespan;
        if (c.energy <= 0.0f) die (c, here == Terrain::Lava ? 6 : 1, here == Terrain::Lava ? 0.0f : 0.2f + 0.25f * g[gSize] + (c.role == Role::Giant ? 0.6f : 0.0f));
        else if (! c.favourite && c.age > lifespan) die (c, 1, 0.35f + 0.3f * g[gSize] + (c.role == Role::Giant ? 0.8f : 0.0f));
    }

    // ---- breeding
    const int preyCount = count (Role::Grazer) + count (Role::Swarm) + count (Role::Pollinator);
    for (auto& a : creatures)
    {
        const auto& info = roleInfo (a.role);
        if (! a.alive || a.breedCooldown > 0.0f || a.energy < info.ready || a.age < 6.0f) continue;
        if (population() >= std::min (maxCreatures, s.maxPopulation)) break;
        if (count (a.role) >= roleCap (a.role, s)) continue;
        // role balance: hunters only breed when there is plenty of prey
        if (a.role == Role::Hunter && preyCount < 3 * (count (Role::Hunter) + 1)) continue;
        for (auto& b : creatures)
        {
            if (&a == &b || ! b.alive || b.role != a.role || b.breedCooldown > 0.0f || b.energy < info.ready - 0.15f || b.age < 6.0f) continue;
            const float reach = 0.07f + radiusOf (a) + radiusOf (b) - 0.03f;
            if (dist2 (a.x, a.y, b.x, b.y) > reach * reach) continue;
            float chance = (0.3f + 0.7f * 0.5f * (a.genes[gFertility] + b.genes[gFertility])) * (0.3f + s.breeding);
            if (a.role == Role::Giant) chance *= 0.35f;
            if (rng.uni() > chance) { a.breedCooldown = 2.0f; break; }
            const auto genes = blend (a.genes, b.genes, s.mutation);
            const uint32_t id = addCreature (a.role, 0.5f * (a.x + b.x), 0.5f * (a.y + b.y), &genes, std::max (a.generation, b.generation) + 1);
            if (auto* child = find (id)) { child->parentA = a.id; child->parentB = b.id; child->energy = 0.5f; ++births; }
            a.energy -= 0.3f; b.energy -= 0.3f;
            a.breedCooldown = b.breedCooldown = info.cooldown;
            break;
        }
    }
    // a lonely creature can bud (so a species does not vanish just because it met nobody)
    for (auto& a : creatures)
        if (a.alive && a.energy > 1.0f && a.breedCooldown <= 0.0f && count (a.role) <= 2 && population() < s.maxPopulation && count (a.role) < roleCap (a.role, s))
        {
            const auto genes = blend (a.genes, a.genes, s.mutation);
            const uint32_t id = addCreature (a.role, a.x + 0.02f, a.y + 0.02f, &genes, a.generation + 1);
            if (auto* child = find (id)) { child->parentA = a.id; ++births; }
            a.energy -= 0.4f;
            a.breedCooldown = std::max (15.0f, roleInfo (a.role).cooldown);
        }

    // ---- immigration keeps the world from going silent
    if (s.immigration)
    {
        immigrationTimer += sdt;
        if (immigrationTimer > 8.0f && population() < std::min (maxCreatures, s.maxPopulation))
        {
            const int plantsNow = plantCount();
            auto wants = [&] (Role r)
            {
                if (roleCap (r, s) <= 0) return false;
                const int n = count (r);
                switch (r)
                {
                    case Role::Grazer:     return n < 2;
                    case Role::Drifter:    return n < 1;
                    case Role::Hunter:     return n < 1 && preyCount > 8;
                    case Role::Pollinator: return n < 1 && plantsNow > 20;
                    case Role::Swarm:      return n < 3 && plantsNow > 20;
                    case Role::Scavenger:  return n < 1 && population() > 10;
                    case Role::Giant:      return n < 1 && plantsNow > 40 && time > 30.0;
                    case Role::Singer:     return n < 1 && time > 20.0;
                    case Role::Drummer:    return n < 1 && plantsNow > 15;
                    case Role::Builder:    return n < 1 && plantsNow > 15;
                }
                return false;
            };
            // every kind that is missing gets a fair chance (grazers first: everything depends on them)
            Role wanted[numRoles];
            int numWanted = 0;
            for (int r = 0; r < numRoles; ++r) if (wants ((Role) r)) wanted[numWanted++] = (Role) r;
            if (numWanted > 0)
            {
                const Role r = wants (Role::Grazer) ? Role::Grazer : wanted[std::min (numWanted - 1, (int) (rng.uni() * (float) numWanted))];
                immigrationTimer = 0.0f;
                const float edge = rng.uni();
                const float y0 = 0.1f + 0.8f * rng.uni();
                const int arrivals = r == Role::Swarm ? 3 : (r == Role::Pollinator || r == Role::Grazer ? 2 : 1);
                for (int k = 0; k < arrivals; ++k)
                    addCreature (r, edge < 0.5f ? 0.03f : 0.97f, juce::jlimit (0.05f, 0.95f, y0 + 0.03f * (float) k));
            }
        }
    }

    // ---- history
    historyTimer += sdt;
    if (historyTimer >= 2.0f)
    {
        historyTimer = 0.0f;
        auto& h = history[(size_t) historyPos];
        for (int r = 0; r < numRoles; ++r) h[(size_t) r] = (uint8_t) count ((Role) r);
        historyPos = (historyPos + 1) % historyLength;
    }
}

} // namespace lifecat

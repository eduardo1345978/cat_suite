#include "LifeWorld.h"
#include <algorithm>

namespace lifecat
{
juce::StringArray geneNames()
{
    return { "Speed", "Size", "Sight", "Fertility", "Lifespan", "Colour",
             "Timbre", "Brightness", "Harmonics", "Attack", "Release", "Register", "Pitch", "Rhythm", "Density",
             "Vibrato", "Shimmer", "Wobble", "Pan" };
}

namespace
{
    float clamp01 (float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    float dist2 (float ax, float ay, float bx, float by) noexcept { const float dx = ax - bx, dy = ay - by; return dx * dx + dy * dy; }
    float radiusOf (const Creature& c) noexcept { return 0.010f + 0.016f * c.genes[gSize] + (c.role == Role::Hunter ? 0.004f : 0.0f); }
}

World::World()
{
    reset (1, 10, 3, 4, 40);
}

Genes World::randomGenes (Role role)
{
    Genes g;
    for (auto& v : g.g) v = rng.uni();
    // a little character per role
    switch (role)
    {
        case Role::Grazer:  g[gTimbre] = 0.25f + 0.5f * rng.uni(); g[gRhythm] = 0.35f + 0.5f * rng.uni(); g[gRegister] = 0.35f + 0.4f * rng.uni(); break;
        case Role::Hunter:  g[gTimbre] = 0.1f + 0.5f * rng.uni();  g[gRhythm] = 0.15f + 0.4f * rng.uni(); g[gRegister] = 0.05f + 0.35f * rng.uni(); g[gSpeed] = 0.4f + 0.6f * rng.uni(); break;
        case Role::Drifter: g[gTimbre] = 0.55f + 0.45f * rng.uni(); g[gRhythm] = 0.0f + 0.35f * rng.uni(); g[gRegister] = 0.55f + 0.45f * rng.uni(); g[gAttack] = 0.5f + 0.5f * rng.uni(); g[gRelease] = 0.6f + 0.4f * rng.uni(); break;
    }
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

void World::reset (uint32_t seed, int grazers, int hunters, int drifters, int numPlants)
{
    seedValue = seed;
    rng.seed (seed * 2654435761u + 12345u);
    for (auto& c : creatures) c.alive = false;
    for (auto& p : plants) p.alive = false;
    nextId = 1;
    maxGeneration = births = deaths = 0;
    time = 0.0;
    historyPos = 0;
    historyTimer = 0.0f;
    for (auto& h : history) h = { 0, 0, 0 };
    eventPos = 0;
    for (auto& e : events) e.kind = 255;
    immigrationTimer = 0.0f;

    for (int i = 0; i < grazers; ++i) addCreature (Role::Grazer, 0.1f + 0.8f * rng.uni(), 0.1f + 0.8f * rng.uni());
    for (int i = 0; i < hunters; ++i) addCreature (Role::Hunter, 0.1f + 0.8f * rng.uni(), 0.1f + 0.8f * rng.uni());
    for (int i = 0; i < drifters; ++i) addCreature (Role::Drifter, 0.1f + 0.8f * rng.uni(), 0.1f + 0.8f * rng.uni());
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
        c.heading = rng.uni() * 6.2832f;
        c.genes = genes != nullptr ? *genes : randomGenes (role);
        c.energy = 0.6f;
        c.age = 0.0f;
        c.breedCooldown = 6.0f;
        c.generation = generation;
        c.freePhase = rng.uni() * 4.0f;
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

void World::pushEvent (uint8_t kind, float x, float y, Role role) noexcept
{
    events[(size_t) eventPos] = { kind, x, y, role };
    eventPos = (eventPos + 1) % (int) events.size();
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

int World::roleCap (Role r, const LifeSettings& s) const noexcept
{
    const int m = std::min (maxCreatures, s.maxPopulation);
    switch (r)
    {
        case Role::Grazer:  return std::max (2, m * 6 / 10);
        case Role::Hunter:  return std::max (1, m * 2 / 10);
        case Role::Drifter: return std::max (1, m * 3 / 10);
    }
    return m;
}

void World::step (float dt, const LifeSettings& s)
{
    const float sdt = dt * s.speed;
    if (sdt <= 0.0f) return;
    time += sdt;

    // ---- plants grow
    const float growthPerSec = 0.4f + 6.0f * s.foodGrowth;
    float expected = growthPerSec * sdt;
    while (expected > 0.0f)
    {
        if (rng.uni() < std::min (1.0f, expected))
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
            if (plantCount() < maxPlants * 3 / 4) addFood (x, y, 1, 0.0f);
        }
        expected -= 1.0f;
    }
    for (auto& p : plants) if (p.alive) p.size = std::min (1.0f, p.size + 0.05f * sdt);

    // ---- creatures
    const int pop = population();
    for (auto& c : creatures)
    {
        if (! c.alive) continue;
        const auto& g = c.genes;
        const float cold = zoneFactor (c.x, c.y, ZoneType::Cold), warm = zoneFactor (c.x, c.y, ZoneType::Warm);
        const float maxSpeed = (0.025f + 0.11f * g[gSpeed]) * (c.role == Role::Hunter ? 1.15f : (c.role == Role::Drifter ? 0.35f : 1.0f))
                             * (1.0f - 0.55f * cold) * (1.0f + 0.25f * warm);
        const float sight = 0.07f + 0.25f * g[gSight];
        float ax = 0.0f, ay = 0.0f;

        // wander
        c.heading += rng.bi() * 3.0f * sdt;
        ax += std::cos (c.heading) * 0.5f;
        ay += std::sin (c.heading) * 0.5f;

        if (c.role == Role::Grazer)
        {
            // nearest plant
            float best = sight * sight;
            int bi = -1;
            for (int i = 0; i < maxPlants; ++i)
                if (plants[(size_t) i].alive)
                {
                    const float d = dist2 (c.x, c.y, plants[(size_t) i].x, plants[(size_t) i].y);
                    if (d < best) { best = d; bi = i; }
                }
            if (bi >= 0 && c.energy < 0.95f)
            {
                const auto& p = plants[(size_t) bi];
                const float d = std::sqrt (best) + 1.0e-5f;
                ax += 2.0f * (p.x - c.x) / d;
                ay += 2.0f * (p.y - c.y) / d;
                if (d < radiusOf (c) + 0.008f)
                {
                    plants[(size_t) bi].alive = false;
                    c.energy = std::min (1.2f, c.energy + 0.18f + 0.12f * p.size);
                }
            }
            // flee hunters
            for (const auto& h : creatures)
                if (h.alive && h.role == Role::Hunter)
                {
                    const float d2 = dist2 (c.x, c.y, h.x, h.y);
                    if (d2 < sight * sight)
                    {
                        const float d = std::sqrt (d2) + 1.0e-5f;
                        ax += 3.0f * (c.x - h.x) / d;
                        ay += 3.0f * (c.y - h.y) / d;
                    }
                }
        }
        else if (c.role == Role::Hunter)
        {
            float best = sight * sight;
            Creature* prey = nullptr;
            for (auto& o : creatures)
                if (o.alive && o.role == Role::Grazer)
                {
                    const float d = dist2 (c.x, c.y, o.x, o.y);
                    if (d < best) { best = d; prey = &o; }
                }
            if (prey != nullptr && c.energy < 0.8f)          // only hunts when hungry
            {
                const float d = std::sqrt (best) + 1.0e-5f;
                ax += 2.6f * (prey->x - c.x) / d;
                ay += 2.6f * (prey->y - c.y) / d;
                if (d < radiusOf (c) + radiusOf (*prey) && c.breedCooldown < 10.0f)
                {
                    // not every pounce succeeds (and favourites almost always escape)
                    const float chance = prey->favourite ? 0.01f : 0.25f + 0.25f * (c.genes[gSpeed] - prey->genes[gSpeed] + 0.5f);
                    if (rng.uni() < chance * sdt * 30.0f)
                    {
                        prey->alive = false;
                        ++deaths;
                        pushEvent (2, prey->x, prey->y, prey->role);
                        c.energy = std::min (1.3f, c.energy + 0.45f + 0.2f * prey->genes[gSize]);
                    }
                }
            }
        }
        else
        {
            // drifters: drift together loosely, live on light (more in warm zones)
            float cx = 0.0f, cy = 0.0f;
            int n = 0;
            for (const auto& o : creatures)
                if (o.alive && o.role == Role::Drifter && &o != &c && dist2 (c.x, c.y, o.x, o.y) < sight * sight)
                { cx += o.x; cy += o.y; ++n; }
            if (n > 0) { ax += 0.6f * (cx / (float) n - c.x) / sight; ay += 0.6f * (cy / (float) n - c.y) / sight; }
            c.energy = std::min (1.1f, c.energy + (0.018f + 0.03f * warm) * sdt);
        }

        // ready to breed: look for a partner of the same kind
        const float ready = c.role == Role::Grazer ? 0.62f : 0.75f;
        if (c.breedCooldown <= 0.0f && c.energy >= ready && c.age >= 6.0f)
        {
            float best = (sight * 2.0f) * (sight * 2.0f);
            const Creature* mate = nullptr;
            for (const auto& o : creatures)
                if (o.alive && &o != &c && o.role == c.role && o.age >= 6.0f && o.breedCooldown <= 0.0f)
                {
                    const float d = dist2 (c.x, c.y, o.x, o.y);
                    if (d < best) { best = d; mate = &o; }
                }
            if (mate != nullptr)
            {
                const float d = std::sqrt (best) + 1.0e-5f;
                ax += 1.6f * (mate->x - c.x) / d;
                ay += 1.6f * (mate->y - c.y) / d;
            }
        }

        // edges push back
        if (c.x < 0.05f) ax += 2.0f; if (c.x > 0.95f) ax -= 2.0f;
        if (c.y < 0.05f) ay += 2.0f; if (c.y > 0.95f) ay -= 2.0f;

        // steer
        const float alen = std::sqrt (ax * ax + ay * ay) + 1.0e-6f;
        const float steer = 1.0f - std::exp (-4.0f * sdt);
        c.vx += (ax / alen * maxSpeed - c.vx) * steer;
        c.vy += (ay / alen * maxSpeed - c.vy) * steer;
        float cdx = 0.0f, cdy = 0.0f;
        zoneCurrent (c.x, c.y, cdx, cdy);
        c.x += (c.vx + cdx * 0.08f) * sdt;
        c.y += (c.vy + cdy * 0.08f) * sdt;
        c.x = juce::jlimit (0.01f, 0.99f, c.x);
        c.y = juce::jlimit (0.01f, 0.99f, c.y);
        collideWalls (c);
        if (std::abs (c.vx) + std::abs (c.vy) > 1.0e-4f)
        {
            // turn towards the direction of travel (the short way round)
            const float d = std::remainder (std::atan2 (c.vy, c.vx) - c.heading, 6.2831853f);
            c.heading = std::remainder (c.heading + 0.2f * d, 6.2831853f);
        }

        // metabolism
        const float burn = (0.004f + 0.022f * g[gSpeed] * g[gSpeed] + 0.010f * g[gSize]) * (0.4f + 1.2f * s.metabolism)
                         * (c.role == Role::Drifter ? 0.6f : (c.role == Role::Hunter ? 1.5f : 0.85f)) * (1.0f - 0.5f * cold);
        c.energy -= burn * sdt;
        c.age += sdt;
        c.breedCooldown -= sdt * (0.5f + s.breeding) * (1.0f + 0.5f * warm);
        c.singFlash = std::max (0.0f, c.singFlash - 3.0f * dt);

        const float lifespan = (35.0f + 110.0f * g[gLifespan]) * s.lifespan;
        if (c.energy <= 0.0f || (! c.favourite && c.age > lifespan))
        {
            c.alive = false;
            ++deaths;
            pushEvent (1, c.x, c.y, c.role);
        }
    }

    // ---- breeding
    for (auto& a : creatures)
    {
        const float need = a.role == Role::Grazer ? 0.62f : 0.75f;
        if (! a.alive || a.breedCooldown > 0.0f || a.energy < need || a.age < 6.0f) continue;
        if (population() >= std::min (maxCreatures, s.maxPopulation)) break;
        if (count (a.role) >= roleCap (a.role, s)) continue;
        // role balance: hunters only breed when there is plenty of prey
        if (a.role == Role::Hunter && count (Role::Grazer) < 3 * (count (Role::Hunter) + 1)) continue;
        for (auto& b : creatures)
        {
            if (&a == &b || ! b.alive || b.role != a.role || b.breedCooldown > 0.0f || b.energy < need - 0.15f || b.age < 6.0f) continue;
            if (dist2 (a.x, a.y, b.x, b.y) > 0.07f * 0.07f) continue;
            const float chance = (0.3f + 0.7f * 0.5f * (a.genes[gFertility] + b.genes[gFertility])) * (0.3f + s.breeding);
            if (rng.uni() > chance) { a.breedCooldown = 2.0f; break; }
            const auto genes = blend (a.genes, b.genes, s.mutation);
            const uint32_t id = addCreature (a.role, 0.5f * (a.x + b.x), 0.5f * (a.y + b.y), &genes, std::max (a.generation, b.generation) + 1);
            if (auto* child = find (id)) { child->parentA = a.id; child->parentB = b.id; child->energy = 0.5f; ++births; }
            a.energy -= 0.3f; b.energy -= 0.3f;
            a.breedCooldown = b.breedCooldown = a.role == Role::Grazer ? 8.0f : 14.0f;
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
            a.breedCooldown = 15.0f;
        }

    // ---- immigration keeps the world from going silent
    if (s.immigration)
    {
        immigrationTimer += sdt;
        const bool needGrazer = count (Role::Grazer) < 2, needDrifter = count (Role::Drifter) < 1, needHunter = count (Role::Hunter) < 1 && count (Role::Grazer) > 8;
        if (immigrationTimer > 8.0f && (needGrazer || needDrifter || needHunter) && population() < maxCreatures)
        {
            immigrationTimer = 0.0f;
            const Role r = needGrazer ? Role::Grazer : (needDrifter ? Role::Drifter : Role::Hunter);
            const float edge = rng.uni();
            addCreature (r, edge < 0.5f ? 0.03f : 0.97f, 0.1f + 0.8f * rng.uni());
        }
    }
    juce::ignoreUnused (pop);

    // ---- history
    historyTimer += sdt;
    if (historyTimer >= 2.0f)
    {
        historyTimer = 0.0f;
        history[(size_t) historyPos] = { (uint8_t) count (Role::Grazer), (uint8_t) count (Role::Hunter), (uint8_t) count (Role::Drifter) };
        historyPos = (historyPos + 1) % historyLength;
    }
}

} // namespace lifecat

#include "OceanSim.h"
#include <algorithm>
#include <cmath>

namespace ocean
{
const StyleInfo& styleInfo (int s)
{
    // cat::muse genres: Pop 0, LoFi 1, Trap 2, House 3, Ambient 4, Jazz 5, Rock 6, Cinematic 7
    // moods: Happy 0, Sad 1, Dark 2, Dreamy 3, Epic 4, Chill 5
    static const StyleInfo styles[NumStyles] = {
        { "Dreamy ambient", "dreamy",  4, 3, 60.0f, 80.0f, true  },
        { "Lo-fi chill",    "lofi",    1, 5, 70.0f, 86.0f, true  },
        { "Deep sea",       "deep",    4, 1, 54.0f, 70.0f, false },
        { "Sunrise piano",  "sunrise", 4, 0, 62.0f, 82.0f, false },
    };
    return styles[juce::jlimit (0, NumStyles - 1, s)];
}
juce::String styleName (int s) { return styleInfo (s).name; }
int findStyle (const juce::String& w)
{
    const auto x = w.toLowerCase().removeCharacters ("-_ ");
    if (x == "dreamy" || x == "ambient" || x == "dream") return StyleDreamy;
    if (x == "lofi" || x == "chill" || x == "lofichill") return StyleLoFi;
    if (x == "deep" || x == "deepsea" || x == "sea") return StyleDeepSea;
    if (x == "sunrise" || x == "piano" || x == "sunrisepiano") return StyleSunrise;
    return -1;
}
juce::String placeName (int p)
{
    static const char* n[NumPlaces] = { "Coral reef", "Kelp forest", "Deep trench", "Shipwreck" };
    return n[juce::jlimit (0, NumPlaces - 1, p)];
}
int findPlace (const juce::String& w)
{
    const auto x = w.toLowerCase().removeCharacters ("-_ ");
    if (x == "reef" || x == "coral" || x == "coralreef") return PlaceReef;
    if (x == "kelp" || x == "forest" || x == "kelpforest") return PlaceKelp;
    if (x == "trench" || x == "abyss" || x == "deeptrench" || x == "dark") return PlaceDeep;
    if (x == "wreck" || x == "ship" || x == "shipwreck") return PlaceWreck;
    return -1;
}
juce::String eventName (int e)
{
    static const char* n[NumEvents] = { "Storm", "Calm seas", "Feast", "Baby boom", "Rare hunt" };
    return n[juce::jlimit (0, NumEvents - 1, e)];
}
int findEvent (const juce::String& w)
{
    const auto x = w.toLowerCase().removeCharacters ("-_ ");
    if (x == "storm") return EventStorm;
    if (x == "calm" || x == "calmseas") return EventCalm;
    if (x == "feast" || x == "food") return EventFeast;
    if (x == "babyboom" || x == "babies") return EventBabyBoom;
    if (x == "rarehunt" || x == "rare") return EventRareHunt;
    return -1;
}
int xpForLevel (int level) { const int l = juce::jmax (1, level) - 1; return 10 * l * l; }
int Fish::level() const noexcept { return 1 + (int) std::floor (std::sqrt (juce::jmax (0, xp) / 10.0)); }

namespace
{
    float dist (float ax, float ay, float bx, float by) { return std::hypot (ax - bx, ay - by); }

    juce::String formatAge (double seconds)
    {
        const int m = (int) (seconds / 60.0);
        if (m < 60) return juce::String (m) + "m";
        const int h = m / 60;
        if (h < 48) return juce::String (h) + "h " + juce::String (m % 60) + "m";
        return juce::String (h / 24) + "d " + juce::String (h % 24) + "h";
    }

    juce::String label (const Fish& f)
    {
        juce::String s;
        if (f.genes.rare != RareNone) s << rareNames()[f.genes.rare] << " ";
        if (f.name.isNotEmpty()) return s + f.name;
        return s + colourName (f.genes.colour1) + " " + bodyNames()[f.genes.body] + " fish";
    }

    /** Species names: two syllables that sound like a creature. */
    juce::String speciesNameFor (juce::Random& r, int trait)
    {
        static const char* a[] = { "Glim", "Purr", "Bub", "Shim", "Lum", "Coral", "Zephy", "Marl", "Nim", "Twink", "Velv", "Sorb", "Fizz", "Mew", "Plum" };
        static const char* b[] = { "fin", "tail", "gill", "scale", "whisk", "drift", "gleam", "paw", "bloom", "wisp" };
        static const char* t[NumTraits] = { "", "Whiskered", "Lantern", "Twin-tailed", "Starry", "Frilled", "Horned" };
        juce::String n = juce::String (t[juce::jlimit (0, NumTraits - 1, trait)]) + " " + a[r.nextInt (15)] + b[r.nextInt (10)];
        return n.trim();
    }
}

//==============================================================================
Sim::Sim (juce::uint32 seed) : rng ((juce::int64) seed)
{
    for (int i = 0; i < 22; ++i)
    {
        auto& f = spawnWild (false);
        f.age = rng.nextFloat() * 600.0f;
        f.breedCooldown = 30.0f + rng.nextFloat() * 120.0f;
    }
    weatherTimer = 480.0f + rng.nextFloat() * 300.0f;
}

Fish* Sim::findFish (int id)
{
    for (auto& f : fish) if (f.id == id) return &f;
    return nullptr;
}
const Fish* Sim::findFish (int id) const
{
    for (auto& f : fish) if (f.id == id) return &f;
    return nullptr;
}
Fish* Sim::fishOf (const juce::String& ownerId)
{
    auto it = viewers.find (ownerId);
    return it == viewers.end() ? nullptr : findFish (it->second.fishId);
}

int Sim::wildCount() const
{
    int n = 0;
    for (auto& f : fish) if (! f.viewer() && ! f.dying) ++n;
    return n;
}
int Sim::viewerFishOnScreen() const
{
    int n = 0;
    for (auto& f : fish) if (f.viewer() && f.present && ! f.leaving) ++n;
    return n;
}

Fish& Sim::spawnWild (bool fromSide)
{
    Fish f;
    f.id = nextId++;
    f.genes = Genes::random (rng);
    f.familyId = f.id;
    f.lifespan = 1500.0f + rng.nextFloat() * 1200.0f;
    f.bornAt = time;
    f.energy = 0.6f + rng.nextFloat() * 0.3f;
    f.breedCooldown = 90.0f;
    if (fromSide)
    {
        f.x = rng.nextBool() ? -25.0f : (float) W + 25.0f;
        f.arriving = true;
    }
    else f.x = 20.0f + rng.nextFloat() * (W - 40);
    f.y = surfaceY + 15.0f + rng.nextFloat() * (floorY - surfaceY - 30.0f);
    f.targetX = f.x < 0 ? 60.0f : f.x > W ? (float) W - 60.0f : f.x;
    f.targetY = f.y;
    f.facing = f.x < W / 2 ? 1 : -1;
    fish.push_back (f);
    return fish.back();
}

Fish& Sim::newViewerFish (const juce::String& ownerId, const juce::String& name)
{
    Fish f;
    f.id = nextId++;
    f.genes = Genes::random (rng);
    if (rng.nextFloat() < 0.01f) f.genes.rare = 1 + rng.nextInt (NumRares - 1);
    f.ownerId = ownerId;
    f.ownerName = name;
    f.name = name;
    f.familyId = f.id;
    f.familyName = name;
    f.bornAt = time;
    f.present = false;
    fish.push_back (f);
    return fish.back();
}

void Sim::bringBack (Fish& f)
{
    if (f.present && ! f.leaving) return;
    if (! f.present)
    {
        f.x = rng.nextBool() ? -25.0f : (float) W + 25.0f;
        f.y = surfaceY + 20.0f + rng.nextFloat() * (floorY - surfaceY - 40.0f);
        f.vx = f.vy = 0;
        f.targetX = f.x < 0 ? 70.0f : (float) W - 70.0f;
        f.targetY = f.y;
        f.facing = f.x < 0 ? 1 : -1;
    }
    if (! f.present || f.leaving)
    {
        // the cat in the boat waves hello
        cat.mood = 2; cat.moodTime = 0; cat.wave = 0; cat.waveAt = f.name;
    }
    f.present = true;
    f.arriving = true;
    f.leaving = false;
    f.fade = 1.0f;

    // too many viewer fish: the one quiet for the longest swims home first
    if (viewerFishOnScreen() > maxViewerFish)
    {
        Fish* quiet = nullptr;
        double oldest = 1e18;
        for (auto& v : viewers)
            if (auto* g = findFish (v.second.fishId))
                if (g->present && ! g->leaving && g->id != f.id && v.second.lastMessage < oldest)
                { oldest = v.second.lastMessage; quiet = g; }
        if (quiet != nullptr) sendAway (*quiet);
    }
}

void Sim::sendAway (Fish& f)
{
    if (! f.present || f.leaving) return;
    f.leaving = true;
    f.arriving = false;
    f.targetX = f.x < W / 2 ? -45.0f : (float) W + 45.0f;
}

void Sim::toast (const Fish& f, const juce::String& text, juce::uint32 colour)
{
    for (auto& t : toasts) if (t.fishId == f.id) { t.text = text; t.life = 4.0f; t.colour = colour; return; }
    Toast t;
    t.text = text; t.fishId = f.id; t.x = f.x; t.y = f.y; t.colour = colour;
    toasts.push_back (t);
}

void Sim::announce (const juce::String& text, juce::uint32 colour)
{
    news.push_back ({ text, 9.0f, colour });
    while (news.size() > 3) news.pop_front();
}

void Sim::dropFood (int count, float atX)
{
    for (int i = 0; i < count && food.size() < 120; ++i)
    {
        Food p;
        p.x = atX >= 0 ? juce::jlimit (10.0f, (float) W - 10.0f, atX + (rng.nextFloat() - 0.5f) * 40.0f) : 15.0f + rng.nextFloat() * (W - 30);
        p.y = surfaceY - 4.0f - rng.nextFloat() * 6.0f;
        p.vy = 7.0f + rng.nextFloat() * 6.0f;
        p.life = 70.0f + rng.nextFloat() * 30.0f;
        food.push_back (p);
    }
}

//==============================================================================
void Sim::step (float dt)
{
    time += dt;
    for (auto& f : fish) stepFish (f, dt);
    fish.erase (std::remove_if (fish.begin(), fish.end(), [] (const Fish& f) { return f.dying && f.fade <= 0.0f; }), fish.end());

    for (auto& p : food)
    {
        if (p.y < floorY - 2.0f) p.y += p.vy * dt;
        p.x += std::sin ((float) time * 1.3f + p.y * 0.1f) * 3.0f * dt;
        p.life -= dt;
    }
    food.erase (std::remove_if (food.begin(), food.end(), [] (const Food& p) { return p.life <= 0.0f; }), food.end());

    for (auto& t : toasts)
    {
        t.life -= dt;
        if (auto* f = findFish (t.fishId)) { t.x = f->x; t.y = f->y; }
    }
    toasts.erase (std::remove_if (toasts.begin(), toasts.end(), [] (const Toast& t) { return t.life <= 0.0f; }), toasts.end());
    for (auto& n : news) n.life -= dt;
    while (! news.empty() && news.front().life <= 0.0f) news.pop_front();

    // viewers who went quiet: their fish swims home
    for (auto& v : viewers)
        if (time - v.second.lastMessage > viewerIdleSeconds)
            if (auto* f = findFish (v.second.fishId))
                if (f->present && ! f->leaving) sendAway (*f);

    for (auto& c : confetti) { c.vy += 30.0f * dt; c.x += c.vx * dt; c.y += c.vy * dt; c.vx *= 0.99f; c.life -= dt; }
    confetti.erase (std::remove_if (confetti.begin(), confetti.end(), [] (const Confetti& c) { return c.life <= 0.0f || c.y > H; }), confetti.end());

    // moving to a new place: fade out, swap, fade in
    if (nextPlace >= 0)
    {
        placeFade = juce::jmin (1.0f, placeFade + dt * 0.5f);
        if (placeFade >= 1.0f) { place = nextPlace; nextPlace = -1; }
    }
    else placeFade = juce::jmax (0.0f, placeFade - dt * 0.5f);

    stepWeather (dt);
    stepVote (dt);
    stepCat (dt);
    stepXp (dt);
    tryBreeding (dt);
    keepPopulation (dt);
    babyBoom = juce::jmax (0.0f, babyBoom - dt);
    rareHunt = juce::jmax (0.0f, rareHunt - dt);

    foodTimer -= dt;
    if (foodTimer <= 0.0f)
    {
        dropFood (5 + rng.nextInt (5));
        foodTimer = 150.0f + rng.nextFloat() * 120.0f;
    }
}

void Sim::stepFish (Fish& f, float dt)
{
    if (! f.present) return;
    f.singFlash = juce::jmax (0.0f, f.singFlash - dt * 1.6f);   // ~0.6 s: the wave and the note
    if (f.dying)
    {
        f.vy = -9.0f; f.vx *= 0.95f;
        f.y += f.vy * dt; f.x += f.vx * dt;
        f.fade -= dt / 4.0f;
        return;
    }
    f.age += dt;
    f.growth = juce::jmin (1.0f, f.growth + dt / 90.0f);
    f.energy = juce::jmax (0.0f, f.energy - dt / 700.0f);
    f.breedCooldown = juce::jmax (0.0f, f.breedCooldown - dt);

    // emotes: the fish does its move instead of swimming around
    if (f.emote != EmoteNone)
    {
        f.emoteTime += dt;
        const float t = f.emoteTime;
        f.vx *= 0.9f;
        switch (f.emote)
        {
            case EmoteDance: f.vy = std::sin (t * 12.0f) * 22.0f; f.vx = std::sin (t * 6.0f) * 14.0f; if (t > 3.0f) f.emote = EmoteNone; break;
            case EmoteSpin:  if (((int) (t * 10.0f)) % 2 == 0) f.facing = 1; else f.facing = -1; f.vy *= 0.9f; if (t > 2.0f) f.emote = EmoteNone; break;
            case EmoteJump:
                if (t < 0.05f) f.vy = -95.0f;
                f.vy += 140.0f * dt;                                  // gravity (out of the water too)
                if (t > 2.5f || (t > 0.3f && f.y > surfaceY + 20.0f && f.vy > 0.0f)) f.emote = EmoteNone;
                break;
            default: f.emote = EmoteNone; break;
        }
        f.x += f.vx * dt; f.y += f.vy * dt;
        f.x = juce::jlimit (6.0f, (float) W - 6.0f, f.x);
        f.y = juce::jlimit (6.0f, floorY - 4.0f, f.y);
        f.wiggle += dt * 10.0f;
        if (f.emote == EmoteNone) { f.targetX = f.x; f.targetY = juce::jmax (surfaceY + 10.0f, f.y); f.vy = juce::jmax (0.0f, f.vy); }
        return;
    }

    const auto& g = f.genes;
    const float len = g.lengthPixels() * (0.5f + 0.5f * f.growth);
    const float maxSpeed = (10.0f + 38.0f * g.speed) * (f.energy < 0.12f ? 0.6f : 1.0f) * (0.7f + 0.3f * f.growth);

    // where to go
    if (f.leaving) { f.targetY = f.y; }
    else if (f.arriving)
    {
        if (f.x > 30.0f && f.x < W - 30.0f) f.arriving = false;
    }
    else
    {
        f.retarget -= dt;
        if (f.retarget <= 0.0f || dist (f.x, f.y, f.targetX, f.targetY) < 6.0f)
        {
            f.targetX = 15.0f + rng.nextFloat() * (W - 30);
            const float top = surfaceY + 10.0f, bottom = floorY - 8.0f;
            f.targetY = g.shy > 0.6f && rng.nextFloat() < 0.7f ? bottom - rng.nextFloat() * 40.0f
                                                               : top + rng.nextFloat() * (bottom - top);
            f.retarget = (3.0f + rng.nextFloat() * 6.0f) * (1.4f - g.speed * 0.8f);
        }
        // hungry: the nearest food
        if (f.energy < 0.85f && ! food.empty())
        {
            int best = -1; float bd = 150.0f;
            for (int i = 0; i < (int) food.size(); ++i)
            {
                const float d = dist (f.x, f.y, food[(size_t) i].x, food[(size_t) i].y);
                if (d < bd) { bd = d; best = i; }
            }
            if (best >= 0)
            {
                f.targetX = food[(size_t) best].x; f.targetY = food[(size_t) best].y;
                if (bd < 3.0f + len * 0.35f)
                {
                    f.energy = juce::jmin (1.0f, f.energy + 0.3f);
                    food.erase (food.begin() + best);
                }
            }
        }
    }
    float ty = f.targetY;
    if (! f.leaving && ! f.arriving) ty += weatherLevel * weatherLevel * 45.0f;   // storms push fish deeper

    float dx = f.targetX - f.x, dy = ty - f.y;
    const float dl = juce::jmax (0.001f, std::hypot (dx, dy));
    float sx = dx / dl * maxSpeed, sy = dy / dl * maxSpeed * 0.7f;

    // friends and personal space
    float cx = 0, cy = 0, ax = 0, ay = 0, px = 0, py = 0; int n = 0, crowd = 0;
    for (auto& o : fish)
    {
        if (&o == &f || ! o.present || o.dying) continue;
        const float d = dist (f.x, f.y, o.x, o.y);
        if (d < 40.0f)
        {
            const float w = (o.familyId == f.familyId || o.genes.colour1 == g.colour1) ? 2.0f : 1.0f;
            cx += o.x * w; cy += o.y * w; ax += o.vx * w; ay += o.vy * w; n += (int) w;
            if (d < 26.0f) ++crowd;
            const float space = 0.6f * (len + o.genes.lengthPixels());
            if (d < space && d > 0.01f) { px += (f.x - o.x) / d * (space - d); py += (f.y - o.y) / d * (space - d); }
        }
    }
    float accX = (sx - f.vx) * 1.4f, accY = (sy - f.vy) * 1.4f;
    if (n > 0 && ! f.leaving && ! f.arriving)
    {
        accX += ((cx / n - f.x) * 0.6f + (ax / n - f.vx) * 0.5f) * g.social;
        accY += ((cy / n - f.y) * 0.6f + (ay / n - f.vy) * 0.5f) * g.social;
    }
    accX += px * 3.0f; accY += py * 3.0f;
    if (g.shy > 0.6f && crowd > 2) { accX += (f.x - cx / juce::jmax (1, n)) * g.shy * 2.0f; }

    f.vx += accX * dt; f.vy += accY * dt;
    // a hop now and then
    f.hop = juce::jmax (0.0f, f.hop - dt);
    if (g.bounce > 0.4f && f.hop <= 0.0f && rng.nextFloat() < g.bounce * dt * 0.6f) { f.vy -= 18.0f + g.bounce * 15.0f; f.hop = 1.0f; }
    const float sp = std::hypot (f.vx, f.vy), cap = maxSpeed * 1.3f;
    if (sp > cap) { f.vx *= cap / sp; f.vy *= cap / sp; }

    f.x += f.vx * dt; f.y += f.vy * dt;
    if (! f.leaving && ! f.arriving) f.x = juce::jlimit (6.0f, (float) W - 6.0f, f.x);
    if (f.y < surfaceY + 5.0f) { f.y = surfaceY + 5.0f; f.vy = std::abs (f.vy) * 0.3f; }
    if (f.y > floorY - 4.0f) { f.y = floorY - 4.0f; f.vy = -std::abs (f.vy) * 0.3f; }
    if (f.vx > 2.5f) f.facing = 1; else if (f.vx < -2.5f) f.facing = -1;
    f.wiggle += dt * (3.0f + g.speed * 7.0f) * (0.35f + juce::jmin (1.0f, sp / juce::jmax (1.0f, maxSpeed)));

    if (f.leaving && (f.x < -35.0f || f.x > W + 35.0f)) { f.present = false; f.leaving = false; }

    if (! f.viewer() && f.age > f.lifespan)
    {
        f.dying = true;
        if (f.name.isNotEmpty() || f.genes.rare != RareNone)
            announce (label (f) + " lived a long life and swam to the stars", 0xffc0a7f0);
    }
}

void Sim::tryBreeding (float dt)
{
    if (wildCount() >= maxWild) return;
    const float chance = 0.035f * (babyBoom > 0.0f ? 6.0f : 1.0f);
    auto ready = [] (const Fish& f) { return f.present && ! f.dying && ! f.leaving && ! f.arriving && f.growth >= 1.0f
                                             && f.age > 90.0f && f.energy > 0.5f && f.breedCooldown <= 0.0f; };
    for (size_t i = 0; i < fish.size(); ++i)
    {
        if (! ready (fish[i])) continue;
        for (size_t j = i + 1; j < fish.size(); ++j)
        {
            if (! ready (fish[j])) continue;
            auto& a = fish[i]; auto& b = fish[j];
            if (dist (a.x, a.y, b.x, b.y) > 16.0f || rng.nextFloat() >= chance * dt) continue;

            bool mutated = false;
            Fish baby;
            baby.id = nextId++;
            baby.genes = Genes::breed (a.genes, b.genes, rng, &mutated);
            if (rareHunt > 0.0f && baby.genes.rare == RareNone && rng.nextFloat() < 0.05f) { baby.genes.rare = 1 + rng.nextInt (NumRares - 1); mutated = true; }
            baby.genes.accessory = AccNone;
            baby.x = (a.x + b.x) * 0.5f; baby.y = (a.y + b.y) * 0.5f;
            baby.targetX = baby.x; baby.targetY = baby.y;
            baby.growth = 0.45f;
            baby.energy = 0.7f;
            baby.breedCooldown = 200.0f;
            baby.lifespan = 1500.0f + rng.nextFloat() * 1200.0f;
            baby.generation = juce::jmax (a.generation, b.generation) + 1;
            baby.parentA = a.id; baby.parentB = b.id;
            baby.bornAt = time;
            maybeNewSpecies (baby, a, b, mutated);
            const Fish& root = a.familyName.isNotEmpty() ? a : b.familyName.isNotEmpty() ? b : a;
            baby.familyId = root.familyId;
            baby.familyName = root.familyName;
            if (baby.familyName.isNotEmpty())
            {
                int n = 0;
                for (auto& o : fish) if (o.familyId == baby.familyId && ! o.viewer()) ++n;
                baby.name = baby.familyName + " Jr." + (n > 0 ? " " + juce::String (n + 1) : juce::String());
            }
            a.energy -= 0.25f; b.energy -= 0.25f;
            a.breedCooldown = b.breedCooldown = 240.0f;
            ++a.babies; ++b.babies;
            ++births;
            if (mutated) ++mutations;

            const juce::String an = label (a), bn = label (b);
            const bool named = a.name.isNotEmpty() || b.name.isNotEmpty();
            fish.push_back (baby);   // a / b may move now
            if (newSpeciesBorn)
            {
                newSpeciesBorn = false;
                if (auto* sp = species (baby.genes.species))
                    announce ("NEW SPECIES DISCOVERED: " + sp->name + "! (generation " + juce::String (baby.generation) + ")", 0xff9ef0c2);
                party (40);
            }
            else if (baby.genes.rare != RareNone)
                announce ("A " + rareNames()[baby.genes.rare] + " fish was born! (" + an + " + " + bn + ")", 0xfffee761);
            else if (named)
                announce (an + " and " + bn + " had a baby: " + (baby.name.isNotEmpty() ? baby.name : juce::String ("a little fish")), 0xff9ef0c2);
            return;   // one birth per step is plenty
        }
    }
}

void Sim::keepPopulation (float dt)
{
    wildTimer -= dt;
    if (wildTimer > 0.0f) return;
    const int wild = wildCount();
    if (wild < minWild) { spawnWild (true); wildTimer = 8.0f; }
    else if (wild < 26 && rng.nextFloat() < 0.3f) { spawnWild (true); wildTimer = 60.0f; }
    else wildTimer = 30.0f;
}

void Sim::stepWeather (float dt)
{
    weatherTimer -= dt;
    if (weatherTimer <= 0.0f)
    {
        const float u = rng.nextFloat();
        if (weather == WeatherCalm)       { weather = u < 0.7f ? WeatherRain : WeatherStorm; }
        else if (weather == WeatherRain)  { weather = u < 0.35f ? WeatherStorm : WeatherCalm; }
        else                              { weather = WeatherRain; }
        weatherTimer = weather == WeatherCalm ? 600.0f + rng.nextFloat() * 500.0f
                     : weather == WeatherRain ? 200.0f + rng.nextFloat() * 200.0f : 120.0f + rng.nextFloat() * 120.0f;
        if (weather == WeatherStorm) announce ("A storm is coming! Fish dive deeper...", 0xfff6757a);
    }
    const float target = weather == WeatherCalm ? 0.0f : weather == WeatherRain ? 0.5f : 1.0f;
    weatherLevel += (target - weatherLevel) * juce::jmin (1.0f, dt * 0.08f);
    lightning = juce::jmax (0.0f, lightning - dt * 3.0f);
    if (weatherLevel > 0.8f && rng.nextFloat() < dt * 0.12f) lightning = 1.0f;
}

void Sim::stepVote (float dt)
{
    if (vote.active)
    {
        vote.left -= dt;
        if (vote.left <= 0.0f) finishVote();
        return;
    }
    voteTimer -= dt;
    if (voteTimer <= 0.0f) startVote();
}

void Sim::startVote()
{
    vote = {};
    vote.active = true;
    vote.kind = voteKindNext;
    voteKindNext = (voteKindNext + 1) % 3;
    vote.left = 90.0f;
    std::vector<int> pool;
    if (vote.kind == 0) { for (int s = 0; s < NumStyles; ++s) if (s != style) pool.push_back (s); }
    else if (vote.kind == 1) { for (int p = 0; p < NumPlaces; ++p) if (p != place) pool.push_back (p); }
    else for (int e = 0; e < NumEvents; ++e) pool.push_back (e);
    for (int i = 0; i < 3 && ! pool.empty(); ++i)
    {
        const int k = rng.nextInt ((int) pool.size());
        vote.options.push_back (pool[(size_t) k]);
        pool.erase (pool.begin() + k);
    }
    vote.counts.assign (vote.options.size(), 0);
    announce (vote.kind == 0 ? "VOTE: what music next? Type !vote 1, 2 or 3"
              : vote.kind == 1 ? "VOTE: where should the ocean go? Type !vote 1, 2 or 3"
                               : "VOTE: what happens next? Type !vote 1, 2 or 3", 0xff2ce8f5);
}

void Sim::finishVote()
{
    vote.active = false;
    voteTimer = 600.0f;
    int best = -1, bestCount = 0;
    for (int i = 0; i < (int) vote.counts.size(); ++i)
        if (vote.counts[(size_t) i] > bestCount) { bestCount = vote.counts[(size_t) i]; best = i; }
    if (best < 0) { announce ("Nobody voted - the fish decide for themselves", 0xff8b9bb4); return; }
    const int choice = vote.options[(size_t) best];
    if (vote.kind == 0)
    {
        styleRequest = choice;
        announce ("The ocean voted: " + styleName (choice) + "!", 0xff2ce8f5);
    }
    else if (vote.kind == 1)
    {
        announce ("The ocean voted: off to the " + placeName (choice) + "!", 0xff2ce8f5);
        goToPlace (choice);
    }
    else
    {
        announce ("The ocean voted: " + eventName (choice) + "!", 0xff2ce8f5);
        applyEvent (choice);
    }
}

void Sim::applyEvent (int e)
{
    switch (e)
    {
        case EventStorm:    weather = WeatherStorm; weatherTimer = 180.0f; break;
        case EventCalm:     weather = WeatherCalm;  weatherTimer = 600.0f; break;
        case EventFeast:    dropFood (35); break;
        case EventBabyBoom: babyBoom = 150.0f; for (auto& f : fish) f.breedCooldown = juce::jmin (f.breedCooldown, 10.0f); break;
        case EventRareHunt: rareHunt = 240.0f; break;
        default: break;
    }
}

//==============================================================================
float Sim::targetTempo() const
{
    double sum = 0, w = 0;
    for (auto& f : fish)
        if (f.present && ! f.dying) { const double k = f.viewer() ? 2.0 : 1.0; sum += f.genes.tempo * k; w += k; }
    const float gene = w > 0 ? (float) (sum / w) : 90.0f;          // 60 .. 140
    const auto& st = styleInfo (style);
    return st.bpmLo + (st.bpmHi - st.bpmLo) * juce::jlimit (0.0f, 1.0f, (gene - 60.0f) / 80.0f);
}

int Sim::targetKey() const
{
    std::array<double, 12> votes {};
    for (auto& f : fish)
        if (f.present && ! f.dying) votes[(size_t) f.genes.key] += f.viewer() ? 2.0 : 1.0;
    int best = 0;
    for (int k = 1; k < 12; ++k) if (votes[(size_t) k] > votes[(size_t) best]) best = k;
    return best;
}

Board Sim::board (int which) const
{
    Board b;
    std::vector<const Fish*> all;
    for (auto& f : fish) if (! f.dying) all.push_back (&f);
    switch (which)
    {
        case 0:
        {
            b.title = "OLDEST FISH";
            std::sort (all.begin(), all.end(), [] (auto* x, auto* y) { return x->age > y->age; });
            for (size_t i = 0; i < all.size() && i < 3; ++i) b.lines.add (label (*all[i]) + "  " + formatAge (all[i]->age));
            break;
        }
        case 1:
        {
            b.title = "BIGGEST FAMILIES";
            std::map<int, std::pair<juce::String, int>> fam;
            for (auto* f : all) if (f->familyName.isNotEmpty()) { auto& e = fam[f->familyId]; e.first = f->familyName; ++e.second; }
            std::vector<std::pair<juce::String, int>> list;
            for (auto& e : fam) list.push_back (e.second);
            std::sort (list.begin(), list.end(), [] (auto& x, auto& y) { return x.second > y.second; });
            for (size_t i = 0; i < list.size() && i < 3; ++i) b.lines.add (list[i].first + " family  " + juce::String (list[i].second));
            break;
        }
        case 2:
        {
            b.title = "RARE FISH";
            int n = 0;
            for (auto* f : all) if (f->genes.rare != RareNone) { if (n < 3) b.lines.add (label (*f)); ++n; }
            if (n > 3) b.lines.set (2, b.lines[2] + "  +" + juce::String (n - 2));
            break;
        }
        case 4:
        {
            b.title = "SPECIES DISCOVERED (" + juce::String ((int) speciesList.size()) + ")";
            for (int i = (int) speciesList.size() - 1; i >= 0 && b.lines.size() < 3; --i)
            {
                int alive = 0;
                for (auto* f : all) if (f->genes.species == speciesList[(size_t) i].id) ++alive;
                b.lines.add (speciesList[(size_t) i].name + "  " + juce::String (alive) + " alive");
            }
            if (b.lines.isEmpty()) { b.lines.add ("evolution is working on it..."); b.empty = true; }
            break;
        }
        case 5:
        {
            b.title = "HIGHEST LEVELS";
            std::vector<const Fish*> v;
            for (auto* f : all) if (f->viewer()) v.push_back (f);
            std::sort (v.begin(), v.end(), [] (auto* x, auto* y) { return x->xp > y->xp; });
            for (size_t i = 0; i < v.size() && i < 3; ++i) b.lines.add (v[i]->name + "  LV" + juce::String (v[i]->level()));
            break;
        }
        default:
        {
            b.title = "MOST BABIES";
            std::sort (all.begin(), all.end(), [] (auto* x, auto* y) { return x->babies > y->babies; });
            for (size_t i = 0; i < all.size() && i < 3; ++i) if (all[i]->babies > 0) b.lines.add (label (*all[i]) + "  " + juce::String (all[i]->babies));
            break;
        }
    }
    if (b.lines.isEmpty()) { b.lines.add (which == 2 ? "none yet - keep watching!" : "type !fish to join"); b.empty = true; }
    return b;
}

//==============================================================================
juce::ValueTree Sim::save() const
{
    juce::ValueTree t ("OCEAN");
    t.setProperty ("version", 1, nullptr);
    t.setProperty ("time", time, nullptr);
    t.setProperty ("nextId", nextId, nullptr);
    t.setProperty ("weather", weather, nullptr);
    t.setProperty ("style", style, nullptr);
    t.setProperty ("births", births, nullptr);
    t.setProperty ("mutations", mutations, nullptr);
    t.setProperty ("place", nextPlace >= 0 ? nextPlace : place, nullptr);
    t.setProperty ("nextSpecies", nextSpecies, nullptr);
    for (auto& sp : speciesList)
    {
        juce::ValueTree x ("SPECIES");
        x.setProperty ("id", sp.id, nullptr); x.setProperty ("name", sp.name, nullptr);
        x.setProperty ("trait", sp.trait, nullptr); x.setProperty ("at", sp.discoveredAt, nullptr);
        x.setProperty ("first", sp.firstFish, nullptr);
        t.addChild (x, -1, nullptr);
    }
    for (auto& b : banned)
    {
        juce::ValueTree x ("BANNED");
        x.setProperty ("id", b, nullptr);
        auto it = bannedNamesById.find (b);
        x.setProperty ("name", it != bannedNamesById.end() ? it->second : juce::String(), nullptr);
        t.addChild (x, -1, nullptr);
    }
    for (auto& f : fish)
    {
        if (f.dying) continue;
        juce::ValueTree x ("FISH");
        x.setProperty ("id", f.id, nullptr);
        x.setProperty ("name", f.name, nullptr);
        x.setProperty ("owner", f.ownerId, nullptr);
        x.setProperty ("ownerName", f.ownerName, nullptr);
        x.setProperty ("x", f.x, nullptr); x.setProperty ("y", f.y, nullptr);
        x.setProperty ("age", f.age, nullptr); x.setProperty ("life", f.lifespan, nullptr);
        x.setProperty ("energy", f.energy, nullptr); x.setProperty ("growth", f.growth, nullptr);
        x.setProperty ("gen", f.generation, nullptr);
        x.setProperty ("pa", f.parentA, nullptr); x.setProperty ("pb", f.parentB, nullptr);
        x.setProperty ("family", f.familyId, nullptr); x.setProperty ("familyName", f.familyName, nullptr);
        x.setProperty ("babies", f.babies, nullptr);
        x.setProperty ("present", f.present && ! f.leaving, nullptr);
        x.setProperty ("born", f.bornAt, nullptr);
        x.setProperty ("xp", f.xp, nullptr);
        x.addChild (f.genes.toTree(), -1, nullptr);
        t.addChild (x, -1, nullptr);
    }
    for (auto& v : viewers)
    {
        juce::ValueTree x ("VIEWER");
        x.setProperty ("id", v.first, nullptr);
        x.setProperty ("name", v.second.name, nullptr);
        x.setProperty ("fish", v.second.fishId, nullptr);
        t.addChild (x, -1, nullptr);
    }
    return t;
}

void Sim::load (const juce::ValueTree& t)
{
    if (! t.hasType ("OCEAN")) return;
    fish.clear(); food.clear(); toasts.clear(); news.clear(); viewers.clear();
    speciesList.clear(); banned.clear(); bannedNamesById.clear(); confetti.clear();
    time = (double) t.getProperty ("time", 0.0);
    nextId = juce::jmax (1, (int) t.getProperty ("nextId", 1));
    weather = juce::jlimit (0, 2, (int) t.getProperty ("weather", 0));
    weatherLevel = weather * 0.5f;
    style = juce::jlimit (0, NumStyles - 1, (int) t.getProperty ("style", 0));
    place = juce::jlimit (0, NumPlaces - 1, (int) t.getProperty ("place", 0));
    nextPlace = -1; placeFade = 0;
    nextSpecies = juce::jmax (1, (int) t.getProperty ("nextSpecies", 1));
    births = t.getProperty ("births", 0);
    mutations = t.getProperty ("mutations", 0);
    for (auto x : t)
    {
        if (x.hasType ("FISH"))
        {
            Fish f;
            f.id = x.getProperty ("id");
            f.name = x.getProperty ("name").toString();
            f.ownerId = x.getProperty ("owner").toString();
            f.ownerName = x.getProperty ("ownerName").toString();
            f.x = x.getProperty ("x", 100.0f); f.y = x.getProperty ("y", 120.0f);
            f.targetX = f.x; f.targetY = f.y;
            f.age = x.getProperty ("age", 0.0f); f.lifespan = x.getProperty ("life", 1800.0f);
            f.energy = x.getProperty ("energy", 0.8f); f.growth = x.getProperty ("growth", 1.0f);
            f.generation = x.getProperty ("gen", 1);
            f.parentA = x.getProperty ("pa", 0); f.parentB = x.getProperty ("pb", 0);
            f.familyId = x.getProperty ("family", f.id); f.familyName = x.getProperty ("familyName").toString();
            f.babies = x.getProperty ("babies", 0);
            f.bornAt = x.getProperty ("born", 0.0);
            f.xp = x.getProperty ("xp", 0);
            // viewer fish wait at home until their owner writes again
            f.present = f.viewer() ? false : (bool) x.getProperty ("present", true);
            f.genes = Genes::fromTree (x.getChildWithName ("GENES"));
            f.breedCooldown = 30.0f;
            fish.push_back (f);
            nextId = juce::jmax (nextId, f.id + 1);
        }
        else if (x.hasType ("SPECIES"))
        {
            Species sp;
            sp.id = x.getProperty ("id"); sp.name = x.getProperty ("name").toString();
            sp.trait = x.getProperty ("trait", 0); sp.discoveredAt = x.getProperty ("at", 0.0);
            sp.firstFish = x.getProperty ("first").toString();
            speciesList.push_back (sp);
            nextSpecies = juce::jmax (nextSpecies, sp.id + 1);
        }
        else if (x.hasType ("BANNED"))
        {
            const auto id = x.getProperty ("id").toString();
            banned.insert (id);
            bannedNamesById[id] = x.getProperty ("name").toString();
        }
        else if (x.hasType ("VIEWER"))
        {
            Viewer v;
            v.name = x.getProperty ("name").toString();
            v.fishId = x.getProperty ("fish");
            viewers[x.getProperty ("id").toString()] = v;
        }
    }
    if (wildCount() < minWild)
        while (wildCount() < minWild) spawnWild (false);
}
//==============================================================================
const Species* Sim::species (int id) const
{
    for (auto& sp : speciesList) if (sp.id == id) return &sp;
    return nullptr;
}

juce::StringArray Sim::bannedNames() const
{
    juce::StringArray out;
    for (auto& b : banned) { auto it = bannedNamesById.find (b); out.add (it != bannedNamesById.end() && it->second.isNotEmpty() ? it->second : b); }
    return out;
}

void Sim::maybeNewSpecies (Fish& baby, const Fish& a, const Fish& b, bool mutated)
{
    // evolution: deep in a family tree, a mutation now and then starts a whole new species
    if (! mutated || baby.generation < 4) return;
    const float chance = 0.18f + 0.04f * (float) juce::jmin (10, baby.generation - 4);
    if (rng.nextFloat() >= chance) return;
    std::vector<int> unused;
    for (int t = 1; t < NumTraits; ++t)
    {
        bool seen = false;
        for (auto& sp : speciesList) seen = seen || sp.trait == t;
        if (! seen) unused.push_back (t);
    }
    Species sp;
    sp.id = nextSpecies++;
    sp.trait = ! unused.empty() ? unused[(size_t) rng.nextInt ((int) unused.size())] : 1 + rng.nextInt (NumTraits - 1);
    sp.name = speciesNameFor (rng, sp.trait);
    sp.discoveredAt = time;
    sp.firstFish = a.name.isNotEmpty() ? a.name + "'s baby" : b.name.isNotEmpty() ? b.name + "'s baby" : juce::String ("a wild fish");
    speciesList.push_back (sp);
    baby.genes.species = sp.id;
    baby.genes.trait = sp.trait;
    newSpeciesBorn = true;
}

void Sim::goToPlace (int p)
{
    p = juce::jlimit (0, NumPlaces - 1, p);
    if (p == place && nextPlace < 0) return;
    nextPlace = p;
}

void Sim::party (int count)
{
    static const juce::uint32 cols[] = { 0xffe43b44, 0xfffee761, 0xff63c74d, 0xff2ce8f5, 0xfff6757a, 0xffc0a7f0, 0xffffffff };
    for (int i = 0; i < count && confetti.size() < 400; ++i)
        confetti.push_back ({ rng.nextFloat() * W, -rng.nextFloat() * 30.0f, (rng.nextFloat() - 0.5f) * 40.0f, rng.nextFloat() * 20.0f,
                              5.0f + rng.nextFloat() * 3.0f, cols[rng.nextInt (7)] });
}

void Sim::celebrateSubscribers (int newCount, int gained)
{
    announce (gained > 1 ? juce::String (gained) + " NEW SUBSCRIBERS! THANK YOU! (" + juce::String (newCount) + ")"
                         : "NEW SUBSCRIBER! THANK YOU! (" + juce::String (newCount) + " subscribers)", 0xfffee761);
    party (120);
    dropFood (12);
    cat.mood = 2; cat.moodTime = 0; cat.waveAt = "new friend";
}

void Sim::addXp (Fish& f, int amount)
{
    if (! f.viewer() || amount <= 0) return;
    const int before = f.level();
    f.xp += amount;
    const int now = f.level();
    if (now <= before) return;
    // what the new level unlocks
    juce::StringArray unlocked;
    const struct { int kind; juce::StringArray names; } kinds[] = {
        { KindBody, bodyNames() }, { KindPattern, patternNames() }, { KindTail, tailNames() }, { KindFin, finNames() },
        { KindEyes, eyeNames() }, { KindHat, accessoryNames() }, { KindVoice, voiceNames() } };
    for (auto& k : kinds)
        for (int i = 0; i < k.names.size(); ++i)
            if (unlockLevel (k.kind, i) > before && unlockLevel (k.kind, i) <= now) unlocked.add (k.names[i]);
    announce (f.name + " reached LV" + juce::String (now) + "!" + (unlocked.isEmpty() ? juce::String() : "  unlocked: " + unlocked.joinIntoString (", ")), 0xff9ef0c2);
    toast (f, "LEVEL UP! LV" + juce::String (now), 0xfffee761);
}

void Sim::stepXp (float dt)
{
    // a viewer's fish earns 1 XP for every minute it swims in the ocean
    xpTimer += dt;
    if (xpTimer < 60.0f) return;
    xpTimer -= 60.0f;
    for (auto& f : fish)
        if (f.viewer() && f.present && ! f.leaving) addXp (f, 1);
}

void Sim::stepCat (float dt)
{
    // the cat in the boat drifts along, fishes, naps, and waves at fish that arrive
    cat.moodTime += dt;
    cat.x += cat.vx * dt;
    if (cat.x < 40.0f) cat.vx = std::abs (cat.vx);
    if (cat.x > W - 120.0f) cat.vx = -std::abs (cat.vx);
    if (cat.mood == 2)
    {
        cat.wave += dt;
        if (cat.moodTime > 3.5f) { cat.mood = 0; cat.moodTime = 0; }
    }
    else if (cat.moodTime > 40.0f + rng.nextFloat() * 40.0f)
    {
        cat.mood = cat.mood == 0 ? 1 : 0;   // fishing <-> napping
        cat.moodTime = 0;
        cat.vx = (rng.nextBool() ? 1.0f : -1.0f) * (2.0f + rng.nextFloat() * 4.0f);
    }
}
int Sim::discoverSpeciesForTests()
{
    if (fish.size() < 2) return 0;
    for (int tries = 0; tries < 1000; ++tries)
    {
        Fish baby;
        baby.id = nextId++;
        baby.genes = Genes::breed (fish[0].genes, fish[1].genes, rng);
        baby.generation = 12;
        baby.x = W * 0.5f; baby.y = 140.0f; baby.targetX = baby.x; baby.targetY = baby.y;
        maybeNewSpecies (baby, fish[0], fish[1], true);
        if (newSpeciesBorn)
        {
            newSpeciesBorn = false;
            fish.push_back (baby);
            if (auto* sp = species (fish.back().genes.species))
                announce ("NEW SPECIES DISCOVERED: " + sp->name + "!", 0xff9ef0c2);
            party (40);
            return fish.back().genes.species;
        }
    }
    return 0;
}
} // namespace ocean

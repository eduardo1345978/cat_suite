#include "OceanSim.h"
#include "OceanFilter.h"
#include <algorithm>
#include <cmath>

namespace ocean
{
const StyleInfo& styleInfo (int s)
{
    // cat::muse genres: Pop 0, LoFi 1, Trap 2, House 3, Ambient 4, Jazz 5, Rock 6, Cinematic 7, Aero 8, Synthwave 9,
    // Bossa 10, Chiptune 11, Orchestral 12, City pop 13, Reggae 14, Folk 15, Dream pop 16, Funk 17
    // moods: Happy 0, Sad 1, Dark 2, Dreamy 3, Epic 4, Chill 5
    static const StyleInfo styles[NumStyles] = {
        // Frutiger Aero: bright, airy, glassy - major keys, pads, bells, soft beats
        { "Aqua dream",   "aqua",    4, 0, 58.0f, 72.0f, false },
        { "Sky lounge",   "lounge",  1, 0, 64.0f, 78.0f, true  },
        { "Crystal deep", "crystal", 4, 3, 50.0f, 62.0f, false },
        { "Bubble pop",   "bubble",  4, 0, 68.0f, 80.0f, false },
        { "Glass garden",   "glass",   8, 3, 62.0f, 76.0f, false },   // Frutiger Aero genre, glass bells
        { "Bossa breeze",   "bossa",  10, 5, 70.0f, 84.0f, true  },
        { "Pixel lullaby",  "pixel",  11, 3, 62.0f, 74.0f, false },   // a soft chiptune lullaby
        { "Strings at dusk", "strings", 12, 3, 52.0f, 66.0f, false },
        { "City lights",    "city",   13, 5, 70.0f, 84.0f, true  },   // City pop, soft and glossy
        { "Island drift",   "island", 14, 0, 62.0f, 74.0f, true  },   // a lazy reggae one-drop
        { "Folk tide",      "folk",   15, 5, 64.0f, 78.0f, false },   // strummed and warm
        { "Dream haze",     "haze",   16, 3, 60.0f, 74.0f, true  },   // dream pop
        { "Funky reef",     "funky",  17, 5, 76.0f, 88.0f, true  },   // gentle funk
        { "Night jazz",     "jazz",    5, 5, 58.0f, 72.0f, true  },   // brushes and e-piano
    };
    return styles[juce::jlimit (0, NumStyles - 1, s)];
}
juce::String styleName (int s) { return styleInfo (s).name; }
int findStyle (const juce::String& w)
{
    const auto x = w.toLowerCase().removeCharacters ("-_ ");
    if (x == "aqua" || x == "dream" || x == "aquadream" || x == "dreamy" || x == "ambient") return StyleDreamy;
    if (x == "lounge" || x == "sky" || x == "skylounge" || x == "lofi" || x == "chill") return StyleLoFi;
    if (x == "crystal" || x == "crystaldeep" || x == "deep" || x == "deepsea") return StyleDeepSea;
    if (x == "bubble" || x == "pop" || x == "bubblepop" || x == "sunrise") return StyleSunrise;
    if (x == "glass" || x == "garden" || x == "glassgarden" || x == "aero") return StyleGlass;
    if (x == "bossa" || x == "breeze" || x == "bossabreeze" || x == "bossanova") return StyleBossa;
    if (x == "pixel" || x == "lullaby" || x == "pixellullaby" || x == "chip" || x == "chiptune") return StylePixel;
    if (x == "strings" || x == "dusk" || x == "stringsatdusk" || x == "orchestra" || x == "orchestral") return StyleStrings;
    if (x == "city" || x == "lights" || x == "citylights" || x == "citypop") return StyleCity;
    if (x == "island" || x == "drift" || x == "islanddrift" || x == "reggae") return StyleIsland;
    if (x == "folk" || x == "tide" || x == "folktide" || x == "acoustic") return StyleFolk;
    if (x == "haze" || x == "dreamhaze" || x == "dreampop" || x == "shoegaze") return StyleDreamHaze;
    if (x == "funky" || x == "funk" || x == "funkyreef" || x == "groove") return StyleFunky;
    if (x == "jazz" || x == "night" || x == "nightjazz" || x == "smooth") return StyleNightJazz;
    return -1;
}
juce::String placeName (int p)
{
    static const char* n[NumPlaces] = { "Aqua reef", "Emerald kelp", "Crystal deep", "Sunken ship",
                                        "Coral cave", "Ice ocean", "Tropical lagoon", "Sunken ruins",
                                        "Volcano vents", "Jellyfish bay", "Open sea", "Underwater city", "Deep trench",
                                        "Coral castle", "Sky ocean" };
    return n[juce::jlimit (0, NumPlaces - 1, p)];
}
int findPlace (const juce::String& w)
{
    const auto x = w.toLowerCase().removeCharacters ("-_ ");
    if (x == "reef" || x == "coral" || x == "aqua" || x == "aquareef") return PlaceReef;
    if (x == "kelp" || x == "forest" || x == "emerald" || x == "emeraldkelp") return PlaceKelp;
    if (x == "trench" || x == "abyss" || x == "deeptrench" || x == "dark") return PlaceTrench;
    if (x == "deep" || x == "crystal" || x == "crystaldeep") return PlaceDeep;
    if (x == "city" || x == "underwatercity" || x == "atlantis" || x == "town") return PlaceCity;
    if (x == "castle" || x == "coralcastle" || x == "palace") return PlaceCastle;
    if (x == "sky" || x == "skyocean" || x == "islands" || x == "floating" || x == "clouds") return PlaceSkyOcean;
    if (x == "wreck" || x == "ship" || x == "shipwreck" || x == "sunken" || x == "sunkenship") return PlaceWreck;
    if (x == "cave" || x == "coralcave" || x == "grotto") return PlaceCave;
    if (x == "ice" || x == "iceocean" || x == "arctic" || x == "snow") return PlaceIce;
    if (x == "lagoon" || x == "tropical" || x == "tropicallagoon" || x == "beach") return PlaceLagoon;
    if (x == "ruins" || x == "sunkenruins" || x == "temple") return PlaceRuins;
    if (x == "volcano" || x == "vents" || x == "volcanovents" || x == "lava" || x == "vent") return PlaceVolcano;
    if (x == "jelly" || x == "jellyfish" || x == "jellybay" || x == "jellyfishbay" || x == "bay") return PlaceJelly;
    if (x == "open" || x == "opensea" || x == "sea" || x == "blue" || x == "whale") return PlaceOpenSea;
    return -1;
}
juce::String eventName (int e)
{
    static const char* n[NumEvents] = { "Storm", "Calm seas", "Feast", "Baby boom", "Rare hunt", "Plankton bloom", "Migration", "Meteor shower" };
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
    if (x == "plankton" || x == "bloom" || x == "planktonbloom" || x == "glow") return EventPlankton;
    if (x == "migration" || x == "school" || x == "turtles") return EventMigration;
    if (x == "meteors" || x == "meteor" || x == "meteorshower" || x == "stars" || x == "shootingstars") return EventMeteors;
    return -1;
}
juce::String catchName (int item)
{
    static const char* n[NumCatchItems] = { "an old boot", "a bottle with a message", "a tin can", "a starfish", "a little fish", "a rubber duck", "a pretty shell", "TREASURE" };
    return item >= 0 && item < NumCatchItems ? juce::String (n[item]) : juce::String();
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
    // the floor's residents: two crabs and an octopus
    for (int i = 0; i < 2; ++i)
    {
        Critter cr;
        cr.kind = CritterCrab; cr.id = ++critterIds;
        cr.x = 60.0f + rng.nextFloat() * 360.0f; cr.y = floorY - 3.0f; cr.dir = rng.nextBool() ? 1 : -1;
        cr.timer = 2.0f + rng.nextFloat() * 5.0f; cr.hue = rng.nextFloat();
        critters.push_back (cr);
    }
    Critter oct;
    oct.kind = CritterOctopus; oct.id = ++critterIds;
    oct.x = 330.0f; oct.y = floorY - 6.0f; oct.timer = 40.0f + rng.nextFloat() * 50.0f; oct.hue = 0.3f;
    critters.push_back (oct);
    nextWhale = 600.0f + rng.nextFloat() * 900.0f;
    nextDolphins = 300.0f + rng.nextFloat() * 600.0f;
    nextMermaid = 3.0f * 3600.0f + rng.nextFloat() * 6.0f * 3600.0f;   // very rare: once in 3 - 9 hours
}

int Sim::season() const
{
    if (seasonSetting >= 0) return seasonSetting;
    const int month = juce::Time::getCurrentTime().getMonth();   // 0 = January
    return month == 9 ? SeasonHalloween : month == 11 ? SeasonChristmas : SeasonNone;
}

bool Sim::mermaidSinging (float* x) const
{
    for (auto& c : critters)
        if (c.kind == CritterMermaid && c.state == 1) { if (x != nullptr) *x = c.x; return true; }
    return false;
}

void Sim::spawnVisitor (int kind)
{
    Critter v;
    v.id = ++critterIds;
    v.kind = kind;
    v.dir = rng.nextBool() ? 1 : -1;
    switch (kind)
    {
        case CritterWhale:
            v.x = v.dir > 0 ? -110.0f : (float) W + 110.0f;
            v.y = 150.0f + rng.nextFloat() * 40.0f; v.vx = 9.0f * (float) v.dir;
            ++whaleVisits;
            announce ("A WHALE SWIMS BY", 0xff9fd8ff);
            critters.push_back (v);
            break;
        case CritterDolphin:
            for (int k = 0; k < 3; ++k)
            {
                auto d = v;
                d.id = ++critterIds;
                d.x = (v.dir > 0 ? -30.0f - 26.0f * (float) k : (float) W + 30.0f + 26.0f * (float) k);
                d.y = surfaceY + 10.0f + (float) k * 4.0f; d.vx = 46.0f * (float) v.dir; d.t = (float) k * 0.7f;
                critters.push_back (d);
            }
            ++dolphinVisits;
            announce ("DOLPHINS! THEY JUMP ALONG THE SURFACE", 0xff9fd8ff);
            break;
        case CritterMermaid:
            v.x = v.dir > 0 ? -20.0f : (float) W + 20.0f;
            v.y = 110.0f; v.vx = 34.0f * (float) v.dir; v.state = 0;
            critters.push_back (v);
            break;
        default: break;
    }
}

void Sim::stepCritters (float dt)
{
    nextWhale -= dt; nextDolphins -= dt; nextMermaid -= dt;
    if (nextWhale <= 0.0f) { spawnVisitor (CritterWhale); nextWhale = 900.0f + rng.nextFloat() * 1500.0f; }
    if (nextDolphins <= 0.0f) { spawnVisitor (CritterDolphin); nextDolphins = 600.0f + rng.nextFloat() * 1200.0f; }
    if (nextMermaid <= 0.0f) { spawnVisitor (CritterMermaid); nextMermaid = 3.0f * 3600.0f + rng.nextFloat() * 6.0f * 3600.0f; }

    for (auto& c : critters)
    {
        c.t += dt;
        switch (c.kind)
        {
            case CritterCrab:
                // scuttles sideways, stops, looks around, walks again
                c.timer -= dt;
                if (c.timer <= 0.0f)
                {
                    c.state = c.state == 0 ? 1 : 0;
                    c.timer = c.state == 0 ? 2.0f + rng.nextFloat() * 4.0f : 1.5f + rng.nextFloat() * 5.0f;
                    if (c.state == 0 && rng.nextFloat() < 0.4f) c.dir = -c.dir;
                }
                if (c.state == 0) c.x += 9.0f * (float) c.dir * dt;
                if (c.x < 10.0f) { c.x = 10.0f; c.dir = 1; }
                if (c.x > (float) W - 10.0f) { c.x = (float) W - 10.0f; c.dir = -1; }
                c.y = floorY - 3.0f;
                break;
            case CritterOctopus:
            {
                // sits and slowly changes colour; now and then jets up and floats back down
                c.hue = std::fmod (c.hue + dt * 0.01f, 1.0f);
                c.timer -= dt;
                const float home = floorY - 6.0f;
                if (c.state == 0 && c.timer <= 0.0f) { c.state = 1; c.vy = -55.0f; c.vx = (rng.nextFloat() - 0.5f) * 20.0f; c.timer = 50.0f + rng.nextFloat() * 70.0f; }
                if (c.state == 1)
                {
                    c.vy += 22.0f * dt;                   // slows, then sinks back
                    c.vy = juce::jmin (c.vy, 12.0f);
                    c.x += c.vx * dt; c.y += c.vy * dt;
                    c.vx *= 0.98f;
                    c.x = juce::jlimit (20.0f, (float) W - 20.0f, c.x);
                    if (c.y >= home && c.vy > 0.0f) { c.y = home; c.state = 0; c.vx = c.vy = 0.0f; }
                }
                break;
            }
            case CritterWhale:
                c.x += c.vx * dt;
                c.y += std::sin (c.t * 0.25f) * 1.5f * dt;
                break;
            case CritterDolphin:
                c.x += c.vx * dt;
                break;
            case CritterMermaid:
                if (c.state == 0)
                {
                    c.x += c.vx * dt;
                    if ((c.dir > 0 && c.x >= (float) W * 0.5f) || (c.dir < 0 && c.x <= (float) W * 0.5f))
                    {
                        c.state = 1; c.timer = 28.0f;
                        ++mermaidSightings;
                        announce ("A MERMAID CAT! SHE SINGS FOR EVERYONE: +10 XP FOR EVERY FISH", 0xffffb3e6);
                        for (auto& f : fish) if (f.viewer() && f.present && ! f.leaving) addXp (f, 10);
                        for (int k = 0; k < 40; ++k)
                            confetti.push_back ({ c.x, c.y, (rng.nextFloat() - 0.5f) * 60.0f, -rng.nextFloat() * 50.0f, 2.5f, k % 2 == 0 ? 0xffffb3e6u : 0xff9ff3ffu });
                    }
                }
                else if (c.state == 1)
                {
                    c.timer -= dt;
                    c.y = 110.0f + std::sin (c.t * 0.9f) * 6.0f;
                    if (c.timer <= 0.0f) { c.state = 2; c.dir = -c.dir; c.vx = 30.0f * (float) c.dir; }
                }
                else c.x += c.vx * dt;
                break;
            default: break;
        }
    }
    // visitors that have swum off the screen go
    critters.erase (std::remove_if (critters.begin(), critters.end(), [] (const Critter& c)
    {
        const bool visitor = c.kind == CritterWhale || c.kind == CritterDolphin || c.kind == CritterMermaid;
        const float margin = c.kind == CritterWhale ? 130.0f : 40.0f;
        const bool goingOut = c.kind != CritterMermaid || c.state == 2;
        return visitor && goingOut && c.t > 3.0f && (c.x < -margin || c.x > (float) W + margin);
    }), critters.end());
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
    ++today.newFish;
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
    for (auto& sp : speech) sp.life -= dt;
    speech.erase (std::remove_if (speech.begin(), speech.end(), [] (const Speech& sp) { return sp.life <= 0.0f; }), speech.end());
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
    // if nobody picks music for a long time, it drifts to another calm style by itself
    styleTimer += dt;
    if (styleTimer > 1500.0f && ! vote.active && styleRequest < 0)
    {
        int next = style;
        while (next == style) next = rng.nextInt (NumStyles);
        styleRequest = next;
        styleTimer = 0.0f;
        announce ("The music drifts to " + styleName (next) + "...", 0xff9ff3ff);
    }
    tryBreeding (dt);
    keepPopulation (dt);
    babyBoom = juce::jmax (0.0f, babyBoom - dt);
    rareHunt = juce::jmax (0.0f, rareHunt - dt);
    // plankton: fish nibble it and grow a little faster
    if (plankton > 0.0f)
        for (auto& f : fish) if (f.present && ! f.dying) f.energy = juce::jmin (1.0f, f.energy + dt * 0.01f);
    plankton = juce::jmax (0.0f, plankton - dt);
    migration = juce::jmax (0.0f, migration - dt);
    meteors = juce::jmax (0.0f, meteors - dt);
    fireworks = juce::jmax (0.0f, fireworks - dt);
    for (auto& a : alerts) a.life -= dt;
    while (! alerts.empty() && alerts.front().life <= 0.0f) alerts.pop_front();
    recap = juce::jmax (0.0f, recap - dt);
    stepDay();
    stepCritters (dt);

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
            case EmoteSpin:  f.vy *= 0.9f; if (t > 2.0f) f.emote = EmoteNone; break;   // the turn is drawn (the fish squeezes round)
            case EmoteJump:
                if (t < 0.05f) f.emoteLeaped = false;
                if (! f.emoteLeaped)
                {
                    // swim up to just under the surface, then leap out
                    f.vy = juce::jlimit (-120.0f, 0.0f, (surfaceY + 8.0f - f.y) * 4.0f);
                    if (f.y < surfaceY + 12.0f || t > 2.5f) { f.emoteLeaped = true; f.vy = -85.0f; f.emoteTime = 10.0f; }
                }
                else
                {
                    f.vy += 150.0f * dt;                              // gravity (out of the water too)
                    if (t > 13.0f || (t > 10.3f && f.y > surfaceY + 16.0f && f.vy > 0.0f)) f.emote = EmoteNone;
                }
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
            ++today.births;
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
        case EventPlankton: plankton = 180.0f; babyBoom = juce::jmax (babyBoom, 90.0f); dropFood (10);
                            catSay ("Ooh, the water is glowing!"); break;
        case EventMigration: migration = 100.0f; catSay ("Look! A big school is swimming by!"); break;
        case EventMeteors:  meteors = 150.0f; wishers.clear(); catSay ("Shooting stars! Type !wish"); break;
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
        case 6:
        {
            b.title = "MOST ACTIVE TODAY";
            std::vector<std::pair<juce::String, int>> list (today.chatters.begin(), today.chatters.end());
            std::sort (list.begin(), list.end(), [] (auto& x, auto& y) { return x.second > y.second; });
            for (size_t i = 0; i < list.size() && i < 3; ++i) b.lines.add (list[i].first + "  " + juce::String (list[i].second) + " msgs");
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
const Favourite* Sim::pickFavourite (juce::Random& r)
{
    std::vector<const Favourite*> pool;
    for (auto& f : favourites) if (! f.likers.isEmpty() && time - f.lastPlayed > 3600.0) pool.push_back (&f);
    if (pool.empty()) return nullptr;
    // songs with more likes come back more often
    int total = 0;
    for (auto* f : pool) total += f->likers.size();
    int pick = r.nextInt (juce::jmax (1, total));
    for (auto* f : pool) { pick -= f->likers.size(); if (pick < 0) return f; }
    return pool.back();
}

void Sim::markFavouritePlayed (juce::uint32 seed, int st)
{
    for (auto& f : favourites) if (f.seed == seed && f.style == st) f.lastPlayed = time;
}

int Sim::likeNowPlaying (const juce::String& viewerId)
{
    if (nowPlaying.title.isEmpty() || nowPlaying.seed == 0) return -1;
    Favourite* fav = nullptr;
    for (auto& f : favourites) if (f.seed == nowPlaying.seed && f.style == nowPlaying.style) fav = &f;
    if (fav == nullptr)
    {
        if (favourites.size() >= 200)   // keep the list small: forget the least liked
        {
            auto least = std::min_element (favourites.begin(), favourites.end(), [] (const Favourite& a, const Favourite& b) { return a.likers.size() < b.likers.size(); });
            favourites.erase (least);
        }
        Favourite f;
        f.title = nowPlaying.title; f.style = nowPlaying.style; f.genre = nowPlaying.genre; f.mood = nowPlaying.mood;
        f.key = nowPlaying.key; f.seed = nowPlaying.seed; f.bpm = nowPlaying.bpm; f.lastPlayed = time;
        favourites.push_back (f);
        fav = &favourites.back();
    }
    if (fav->likers.contains (viewerId)) return -1;
    fav->likers.add (viewerId);
    return fav->likers.size();
}

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
    for (auto& fv : favourites)
    {
        juce::ValueTree x ("FAVOURITE");
        x.setProperty ("title", fv.title, nullptr); x.setProperty ("style", fv.style, nullptr);
        x.setProperty ("genre", fv.genre, nullptr); x.setProperty ("mood", fv.mood, nullptr);
        x.setProperty ("key", fv.key, nullptr); x.setProperty ("seed", (juce::int64) fv.seed, nullptr);
        x.setProperty ("bpm", fv.bpm, nullptr); x.setProperty ("likers", fv.likers.joinIntoString (","), nullptr);
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
    favourites.clear(); requests.clear();
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
        if (x.hasType ("FAVOURITE"))
        {
            Favourite fv;
            fv.title = x.getProperty ("title").toString();
            fv.style = juce::jlimit (0, NumStyles - 1, (int) x.getProperty ("style", 0));
            fv.genre = x.getProperty ("genre", 0); fv.mood = x.getProperty ("mood", 0); fv.key = x.getProperty ("key", 0);
            fv.seed = (juce::uint32) (juce::int64) x.getProperty ("seed", 0);
            fv.bpm = x.getProperty ("bpm", 70.0);
            fv.likers = juce::StringArray::fromTokens (x.getProperty ("likers").toString(), ",", "");
            fv.likers.removeEmptyStrings();
            if (fv.seed != 0) favourites.push_back (fv);
            continue;
        }
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
    today.newSubscribers += gained;
    // a milestone: 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000 ...
    for (int m : { 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000, 25000, 50000, 100000 })
        if (newCount >= m && newCount - gained < m)
        {
            alerts.push_back ({ juce::String (m) + " SUBSCRIBERS!", "Thank you all - the whole ocean is partying!", 0xfffee761, 12.0f, 3 });
            fireworks = 12.0f;
            party (300);
            dropFood (30);
            catSay (juce::String (m) + " FRIENDS! I'm so happy!", 8.0f);
            cat.hearts = 6.0f;
            return;
        }
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

void Sim::catSay (const juce::String& text, float seconds)
{
    cat.say = text;
    cat.sayTime = seconds;
    cat.nextTalk = 70.0f + rng.nextFloat() * 110.0f;
}

void Sim::stepCat (float dt)
{
    // the cat in the boat drifts along, fishes, naps, waves at fish that arrive, talks, gets hungry and sleepy
    cat.moodTime += dt;
    cat.x += cat.vx * dt;
    if (cat.x < 40.0f) cat.vx = std::abs (cat.vx);
    if (cat.x > W - 120.0f) cat.vx = -std::abs (cat.vx);
    cat.hunger = juce::jmin (1.0f, cat.hunger + dt / 1500.0f);                       // hungry again after ~25 minutes
    cat.sleepy = cat.mood == 1 ? juce::jmax (0.0f, cat.sleepy - dt / 120.0f) : juce::jmin (1.0f, cat.sleepy + dt / 900.0f);
    cat.happy = juce::jlimit (0.0f, 1.0f, cat.happy + dt * (cat.hunger > 0.75f ? -1.0f / 900.0f : 1.0f / 1800.0f));
    cat.sayTime = juce::jmax (0.0f, cat.sayTime - dt);
    cat.itemTime = juce::jmax (0.0f, cat.itemTime - dt);
    cat.hearts = juce::jmax (0.0f, cat.hearts - dt);
    if (cat.itemTime <= 0.0f) cat.item = CatchNone;

    if (cat.mood == 2)
    {
        cat.wave += dt;
        if (cat.moodTime > 3.5f) { cat.mood = 0; cat.moodTime = 0; }
    }
    else if (cat.mood == 1)
    {
        if (cat.sleepy <= 0.0f || cat.moodTime > 150.0f) { cat.mood = 0; cat.moodTime = 0; catSay ("*yawn* ... good nap!"); }
    }
    else if (cat.sleepy >= 1.0f || (cat.moodTime > 60.0f && cat.sleepy > 0.6f && rng.nextFloat() < dt * 0.02f))
    {
        cat.mood = 1;   // nap time
        cat.moodTime = 0;
        cat.vx = (rng.nextBool() ? 1.0f : -1.0f) * (2.0f + rng.nextFloat() * 4.0f);
        catSay ("zzz...", 3.0f);
    }

    // fishing: every minute or two something bites
    if (cat.mood == 0)
    {
        cat.nextCatch -= dt;
        if (cat.nextCatch <= 0.0f)
        {
            cat.nextCatch = 60.0f + rng.nextFloat() * 90.0f;
            const float r = rng.nextFloat();
            cat.item = r < 0.03f ? CatchTreasure : r < 0.2f ? CatchFish : r < 0.33f ? CatchBoot : r < 0.46f ? CatchBottle
                     : r < 0.58f ? CatchCan : r < 0.72f ? CatchStarfish : r < 0.86f ? CatchShell : CatchDuck;
            cat.itemTime = 7.0f;
            ++cat.catches;
            switch (cat.item)
            {
                case CatchTreasure:
                {
                    catSay ("TREASURE!!! Gold for everyone!", 7.0f);
                    announce ("The boat cat fished up TREASURE! Every fish here gets 20 XP", 0xfffee761);
                    for (auto& f : fish) if (f.viewer() && f.present && ! f.leaving) addXp (f, 20);
                    party (80);
                    cat.happy = 1.0f;
                    break;
                }
                case CatchFish:
                    catSay (cat.hunger > 0.4f ? "A snack! *munch*" : "A little fish... I'll let you go!");
                    if (cat.hunger > 0.4f) cat.hunger = juce::jmax (0.0f, cat.hunger - 0.3f);
                    break;
                case CatchBottle:
                {
                    static const char* notes[] = { "\"Be kind to every fish\"", "\"The treasure is under the boat\"", "\"Drink water, take breaks\"",
                                                   "\"Hello from a faraway island!\"", "\"You are doing great\"", "\"Feed the cat\" - signed, the cat" };
                    catSay (juce::String ("A message: ") + notes[rng.nextInt (6)], 8.0f);
                    break;
                }
                case CatchBoot:     catSay (rng.nextBool() ? "A boot. Again." : "Who keeps losing boots?!"); break;
                case CatchCan:      catSay ("A tin can... I'll recycle it."); break;
                case CatchStarfish: catSay ("Hello little star! Back you go."); break;
                case CatchDuck:     catSay ("Squeak! A rubber duck!"); break;
                default:            catSay ("Ooh, " + catchName (cat.item) + "!"); break;
            }
        }
    }

    // talking now and then: about the weather, the time, how it feels, or a viewer
    cat.nextTalk -= dt;
    if (cat.nextTalk <= 0.0f && cat.sayTime <= 0.0f && cat.mood != 1)
    {
        juce::StringArray lines;
        if (cat.hunger > 0.75f) { lines.add ("I'm SO hungry... !feedcat please?"); lines.add ("My tummy is rumbling... !feedcat"); }
        else if (cat.happy < 0.35f) lines.add ("Nobody pets me... !pet");
        else
        {
            if (weather == WeatherStorm) lines.add ("What a storm! Hold on, fish!");
            else if (weather == WeatherRain) lines.add ("I don't like getting wet...");
            if (daylight < 0.2f) { lines.add ("Look at all the stars tonight"); lines.add ("The fish glow so pretty at night"); }
            else if (daylight < 0.9f) lines.add ("What a sunset...");
            else lines.add ("Lovely day for fishing!");
            lines.add ("Type !fish to get your own fish!");
            lines.add ("Have you liked this song? !like");
            lines.add ("I caught " + juce::String (cat.catches) + " things today!");
            int here = 0;
            juce::String someone;
            for (auto& f : fish) if (f.viewer() && f.present && ! f.leaving) { ++here; if (rng.nextInt (here) == 0) someone = f.name; }
            if (someone.isNotEmpty()) { lines.add ("Hi " + someone + "! Nice fins!"); lines.add (someone + ", want to go fishing with me?"); }
            if (plankton > 0.0f) lines.add ("The plankton tastes like stars");
        }
        catSay (lines[rng.nextInt (lines.size())]);
    }

    // visitors: a seagull, the grey cat from the next island, or a dog
    if (cat.visitor != VisitorNone)
    {
        cat.visitorTime -= dt;
        if (cat.visitorTime <= 0.0f) { cat.visitor = VisitorNone; cat.nextVisitor = 300.0f + rng.nextFloat() * 400.0f; }
    }
    else if ((cat.nextVisitor -= dt) <= 0.0f && weather != WeatherStorm)
    {
        cat.visitor = 1 + rng.nextInt (NumVisitors - 1);
        cat.visitorTime = 50.0f + rng.nextFloat() * 40.0f;
        static const char* hello[] = { "", "A seagull! Don't steal my fish!", "My friend from the next island is here!", "Woof? Who let the dog on the boat?" };
        catSay (hello[cat.visitor]);
    }
}

void Sim::stepDay()
{
    // a new day (by the real clock): show yesterday's recap, start counting again
    const auto date = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
    if (today.date.isEmpty()) { today.date = date; return; }
    if (date == today.date) return;
    recapStats = today;
    if (recapStats.messages > 0 || recapStats.newFish > 0) recap = 60.0f;
    today = DayStats();
    today.date = date;
}

void Sim::showRecap()
{
    recapStats = today;
    recap = 45.0f;
}

juce::String Sim::dayRecapText (const DayStats& d) const
{
    juce::String t;
    t << d.messages << " messages, " << d.newFish << " new fish, " << d.births << " babies";
    if (d.superChats > 0) t << ", " << d.superChats << " Super Chats";
    if (d.members > 0) t << ", " << d.members << " new members";
    if (d.newSubscribers > 0) t << ", " << d.newSubscribers << " new subscribers";
    return t;
}

void Sim::memberJoined (const juce::String& authorId, const juce::String& name, const juce::String& text)
{
    const auto who = filter::cleanName (name, (int) (authorId.hashCode() & 0x7fffffff));
    ++today.members;
    alerts.push_back ({ "WELCOME, NEW MEMBER!", who + (text.isNotEmpty() ? " - " + filter::cleanName (text, 1) : juce::String()), 0xfffee761, 10.0f, 1 });
    party (160);
    fireworks = juce::jmax (fireworks, 6.0f);
    cat.hearts = 4.0f;
    catSay ("Welcome to the family, " + who + "!", 7.0f);
    if (auto* f = fishOf (authorId))
    {
        f->genes.accessory = AccCrown;          // members wear a crown, whatever their level
        addXp (*f, 50);
        bringBack (*f);
    }
}

void Sim::superChat (const juce::String& authorId, const juce::String& name, const juce::String& amount, const juce::String& text)
{
    const auto who = filter::cleanName (name, (int) (authorId.hashCode() & 0x7fffffff));
    ++today.superChats;
    // bigger amounts, a bigger show (the number, whatever the currency)
    const double value = amount.retainCharacters ("0123456789.,").replaceCharacter (',', '.').getDoubleValue();
    const float show = (float) juce::jlimit (1.0, 5.0, 1.0 + std::log10 (juce::jmax (1.0, value)) * 1.6);
    juce::String said;
    for (auto c : text) { c = filter::fold (c); if (c >= 32 && c < 127) said << juce::String::charToString (c); }
    said = said.trim().substring (0, 60);
    if (! filter::isClean (said)) said = {};
    alerts.push_back ({ "SUPER CHAT " + amount.toUpperCase() + " - THANK YOU " + who.toUpperCase() + "!", said, 0xff2ce8f5, 8.0f + show * 2.0f, 2 });
    party ((int) (60 * show));
    fireworks = juce::jmax (fireworks, 3.0f * show);
    cat.hearts = 3.0f;
    catSay ("Thank you " + who + "!!!", 6.0f);
    if (auto* f = fishOf (authorId)) addXp (*f, (int) (10 * show));
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

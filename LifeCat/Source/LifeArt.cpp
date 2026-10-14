#include "LifeArt.h"

namespace lifeart
{
using namespace lifecat;
using juce::Colour;
using juce::Path;
using juce::AffineTransform;

namespace
{
    constexpr float twoPi = 6.2831853f;
    float clamp01 (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v); }

    juce::PathStrokeType roundStroke (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); }

    void softGlow (juce::Graphics& g, juce::Point<float> c, float radius, Colour col, float alpha)
    {
        if (alpha <= 0.003f || radius <= 0.5f) return;
        g.setGradientFill (juce::ColourGradient (col.withAlpha (clamp01 (alpha)), c.x, c.y, col.withAlpha (0.0f), c.x + radius, c.y, true));
        g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
    }

    /** Spots or stripes from the Pattern gene, clipped to the body. */
    void pattern (juce::Graphics& g, const Path& body, const AffineTransform& tf, const Creature& c, Colour ink)
    {
        const float pv = c.genes[gPattern];
        if (pv < 0.33f) return;
        juce::Graphics::ScopedSaveState ss (g);
        auto clip = body;
        clip.applyTransform (tf);
        g.reduceClipRegion (clip);
        Path marks;
        if (pv > 0.66f)
            for (int k = -3; k <= 3; ++k) marks.addRectangle ((float) k * 0.42f - 0.1f, -2.0f, 0.16f, 4.0f);          // stripes
        else
        {
            juce::Random rnd ((juce::int64) c.id * 77);
            for (int k = 0; k < 6; ++k) marks.addEllipse (-1.0f + 2.0f * rnd.nextFloat(), -0.8f + 1.6f * rnd.nextFloat(), 0.32f, 0.32f);
        }
        g.setColour (ink);
        g.fillPath (marks, tf);
    }
}

//==============================================================================
juce::String roleName (int theme, Role role, bool plural)
{
    static const char* one[4][numRoles] = {
        { "Beetle", "Frog",   "Firefly", "Bee",      "Snail",   "Tortoise",   "Ant" },
        { "Fish",   "Shark",  "Jelly",   "Seahorse", "Crab",    "Whale",      "Sardine" },
        { "Saucer", "Comet",  "Star",    "Seeder",   "Junkbot", "Star Whale", "Spark" },
        { "Cell",   "Amoeba", "Diatom",  "Spore",    "Phage",   "Colony",     "Bacterium" } };
    static const char* many[4][numRoles] = {
        { "Beetles", "Frogs",   "Fireflies", "Bees",      "Snails",   "Tortoises",   "Ants" },
        { "Fish",    "Sharks",  "Jellies",   "Seahorses", "Crabs",    "Whales",      "Sardines" },
        { "Saucers", "Comets",  "Stars",     "Seeders",   "Junkbots", "Star Whales", "Sparks" },
        { "Cells",   "Amoebas", "Diatoms",   "Spores",    "Phages",   "Colonies",    "Bacteria" } };
    const int t = juce::jlimit (0, 3, theme), r = juce::jlimit (0, numRoles - 1, (int) role);
    return plural ? many[t][r] : one[t][r];
}

Colour roleColour (int theme, Role role)
{
    static const juce::uint32 cols[4][numRoles] = {
        { 0xff8fd18a, 0xffd98b6a, 0xfff2df7a, 0xfff5c542, 0xffc9a27a, 0xff9aa36a, 0xffc0705a },
        { 0xff7cc6e0, 0xff9aa6c2, 0xffd59ae6, 0xfff0a868, 0xffe8735a, 0xff6f8fb8, 0xffcfe0ea },
        { 0xffa9b8ff, 0xffffa86b, 0xfffff0b0, 0xff7fe0c8, 0xffb0b8c8, 0xff8a7cff, 0xffffe070 },
        { 0xff9fe0b8, 0xffe58aa8, 0xffb7a6f0, 0xfff0d070, 0xffa0d0f0, 0xffe0a0d8, 0xffc8f08a } };
    return Colour (cols[juce::jlimit (0, 3, theme)][juce::jlimit (0, numRoles - 1, (int) role)]);
}

Colour creatureColour (int theme, const Creature& c)
{
    return roleColour (theme, c.role).withRotatedHue ((c.genes[gHue] - 0.5f) * 0.22f)
                                     .withMultipliedBrightness (0.85f + 0.3f * c.genes[gBright]);
}

juce::String terrainName (int theme, Terrain t)
{
    static const char* n[4][numTerrains] = {
        { "Ground", "Water",   "Rock",     "Meadow",    "Sunlight" },
        { "Sand",   "Deep",    "Reef",     "Seagrass",  "Sunbeam" },
        { "Void",   "Nebula",  "Asteroid", "Stardust",  "Starlight" },
        { "Gel",    "Fluid",   "Crystal",  "Nutrients", "UV Light" } };
    return n[juce::jlimit (0, 3, theme)][juce::jlimit (0, numTerrains - 1, (int) t)];
}

Colour terrainColour (int theme, Terrain t)
{
    static const juce::uint32 c[4][numTerrains] = {
        { 0x00000000, 0xff2f6f9a, 0xff6e665c, 0xff4f9c45, 0xffffe08a },
        { 0x00000000, 0xff06213a, 0xffa58066, 0xff3f8f6a, 0xffbfe8ff },
        { 0x00000000, 0xff5a3a8a, 0xff6a6470, 0xffd8b87a, 0xfffff4c8 },
        { 0x00000000, 0xff3a5a7a, 0xffa8d8f0, 0xffd8c060, 0xffc890ff } };
    return Colour (c[juce::jlimit (0, 3, theme)][juce::jlimit (0, numTerrains - 1, (int) t)]);
}

juce::String weatherName (Weather w)
{
    static const char* n[] = { "Clear", "Rain", "Snow", "Fog", "Storm" };
    return n[juce::jlimit (0, numWeathers - 1, (int) w)];
}

juce::String seasonName (float season)
{
    static const char* n[] = { "Spring", "Summer", "Autumn", "Winter" };
    return n[juce::jlimit (0, 3, (int) (std::fmod (season + 4.5f, 4.0f)))];
}

juce::String clockText (float dayPhase)
{
    const int minutes = (int) (std::fmod (dayPhase + 1.0f, 1.0f) * 24.0f * 60.0f);
    return juce::String (minutes / 60).paddedLeft ('0', 2) + ":" + juce::String ((minutes % 60) / 10 * 10).paddedLeft ('0', 2);
}

float maturity (const Creature& c) noexcept
{
    return juce::jmin (1.0f, 0.45f + c.age / 36.0f);
}

//==============================================================================
void drawBackground (juce::Graphics& g, juce::Rectangle<float> a, int theme, double time)
{
    const float ft = (float) time;
    juce::Random rnd (1234 + theme);
    switch (theme)
    {
        case 0:   // garden: deep green with dappled light
        {
            g.setGradientFill (juce::ColourGradient (Colour (0xff1b3522), a.getCentreX(), a.getY(), Colour (0xff0c1a11), a.getCentreX(), a.getBottom(), false));
            g.fillRect (a);
            for (int i = 0; i < 16; ++i)
            {
                const float x = a.getX() + rnd.nextFloat() * a.getWidth(), y = a.getY() + rnd.nextFloat() * a.getHeight();
                const float r = juce::jmin (a.getWidth(), a.getHeight()) * (0.05f + 0.08f * rnd.nextFloat()) * (1.0f + 0.08f * std::sin (ft * 0.3f + (float) i));
                softGlow (g, { x, y }, r, Colour (0xffc8f0a0), 0.07f);
            }
            break;
        }
        case 1:   // ocean: blue depth, light rays, rising bubbles
        {
            g.setGradientFill (juce::ColourGradient (Colour (0xff10405e), a.getCentreX(), a.getY(), Colour (0xff04101d), a.getCentreX(), a.getBottom(), false));
            g.fillRect (a);
            for (int i = 0; i < 6; ++i)
            {
                const float x = a.getX() + a.getWidth() * (0.05f + 0.18f * (float) i) + 20.0f * std::sin (ft * 0.15f + (float) i);
                Path ray;
                ray.startNewSubPath (x, a.getY());
                ray.lineTo (x + a.getWidth() * 0.05f, a.getY());
                ray.lineTo (x + a.getWidth() * 0.18f, a.getBottom());
                ray.lineTo (x + a.getWidth() * 0.09f, a.getBottom());
                ray.closeSubPath();
                g.setGradientFill (juce::ColourGradient (Colour (0x169fd8ff), x, a.getY(), Colour (0x009fd8ff), x, a.getBottom(), false));
                g.fillPath (ray);
            }
            for (int i = 0; i < 28; ++i)
            {
                const float x0 = rnd.nextFloat(), speed = 0.02f + 0.04f * rnd.nextFloat(), ph = rnd.nextFloat();
                const float y = 1.0f - std::fmod (ph + ft * speed, 1.0f);
                const float x = x0 + 0.01f * std::sin (ft * 2.0f + (float) i);
                const float r = 1.0f + 2.5f * rnd.nextFloat();
                g.setColour (Colour (0x40bfe8ff));
                g.drawEllipse (a.getX() + x * a.getWidth() - r, a.getY() + y * a.getHeight() - r, 2.0f * r, 2.0f * r, 0.8f);
            }
            break;
        }
        case 2:   // space: black, nebula glow, twinkling stars
        {
            g.setColour (Colour (0xff05050c));
            g.fillRect (a);
            const juce::uint32 neb[] = { 0x2a6a3fa0, 0x223f6aa0, 0x1ca04f6a };
            for (int i = 0; i < 3; ++i)
            {
                const float x = a.getX() + a.getWidth() * (0.25f + 0.25f * (float) i), y = a.getY() + a.getHeight() * (0.3f + 0.2f * (float) (i % 2));
                softGlow (g, { x, y }, a.getWidth() * 0.35f, Colour (neb[i]).withAlpha (1.0f), (float) Colour (neb[i]).getAlpha() / 255.0f);
            }
            for (int i = 0; i < 180; ++i)
            {
                const float x = a.getX() + rnd.nextFloat() * a.getWidth(), y = a.getY() + rnd.nextFloat() * a.getHeight();
                const float tw = 0.5f + 0.5f * std::sin (ft * (0.5f + rnd.nextFloat() * 2.0f) + (float) i);
                const float s = rnd.nextFloat() < 0.1f ? 1.8f : 1.0f;
                g.setColour (juce::Colours::white.withAlpha (0.12f + 0.45f * tw * rnd.nextFloat()));
                g.fillEllipse (x, y, s, s);
            }
            break;
        }
        default:  // cells: a petri dish under the microscope
        {
            g.setColour (Colour (0xff120f16));
            g.fillRect (a);
            const auto c = a.getCentre();
            const float rx = a.getWidth() * 0.49f, ry = a.getHeight() * 0.49f;
            g.setGradientFill (juce::ColourGradient (Colour (0xff2c2133), c.x, c.y, Colour (0xff17121c), c.x + juce::jmax (rx, ry), c.y, true));
            g.fillEllipse (c.x - rx, c.y - ry, 2.0f * rx, 2.0f * ry);
            g.setColour (Colour (0x10ffffff));
            for (float x = a.getX(); x < a.getRight(); x += a.getWidth() / 12.0f) g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
            for (float y = a.getY(); y < a.getBottom(); y += a.getHeight() / 12.0f) g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
            g.setColour (Colour (0x40d8b8e8));
            g.drawEllipse (c.x - rx, c.y - ry, 2.0f * rx, 2.0f * ry, 2.0f);
            g.setColour (Colour (0x12ffffff));
            g.fillEllipse (c.x - rx * 0.75f, c.y - ry * 0.85f, rx * 0.6f, ry * 0.25f);
            break;
        }
    }
}

juce::Image renderTerrain (const World& w, int theme, int size)
{
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);
    const float cell = (float) size / (float) terrainSize;
    juce::Random rnd (99);
    for (int pass = 1; pass < numTerrains; ++pass)
        for (int cy = 0; cy < terrainSize; ++cy)
            for (int cx = 0; cx < terrainSize; ++cx)
            {
                const auto t = (Terrain) w.terrain[(size_t) (cy * terrainSize + cx)];
                if ((int) t != pass) continue;
                const float x = ((float) cx + 0.5f) * cell, y = ((float) cy + 0.5f) * cell;
                const juce::uint32 h = (juce::uint32) (cx * 7349 + cy * 1931 + pass * 17);
                const float jitter = (float) (h % 100) / 100.0f;
                const auto col = terrainColour (theme, t);
                switch (t)
                {
                    case Terrain::Water:
                    {
                        const float r = cell * 0.95f;
                        g.setColour (col.withAlpha (theme == 1 ? 0.7f : 0.85f));
                        g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
                        g.setColour (juce::Colours::white.withAlpha (0.06f));
                        if (jitter < 0.3f) g.drawLine (x - r * 0.4f, y - r * 0.2f, x + r * 0.2f, y - r * 0.2f, 1.0f);
                        break;
                    }
                    case Terrain::Rock:
                    {
                        const float r = cell * (0.7f + 0.25f * jitter);
                        Path stone;
                        stone.addPolygon ({ x, y }, 6 + (int) (jitter * 3.0f), r, jitter * 3.0f);
                        stone = stone.createPathWithRoundedCorners (r * 0.35f);
                        g.setColour (col.darker (0.3f));
                        g.fillPath (stone, AffineTransform::translation (cell * 0.08f, cell * 0.1f));
                        g.setColour (col);
                        g.fillPath (stone);
                        g.setColour (col.brighter (0.4f).withAlpha (0.5f));
                        g.fillEllipse (x - r * 0.4f, y - r * 0.5f, r * 0.5f, r * 0.3f);
                        break;
                    }
                    case Terrain::Meadow:
                    {
                        const float r = cell * 0.85f;
                        g.setColour (col.withAlpha (0.45f));
                        g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
                        g.setColour (col.brighter (0.3f).withAlpha (0.8f));
                        for (int k = 0; k < 3; ++k)
                        {
                            const float bx = x + (float) (k - 1) * cell * 0.3f + jitter * 2.0f;
                            g.drawLine (bx, y + cell * 0.3f, bx + cell * 0.1f, y - cell * 0.25f, 1.2f);
                        }
                        break;
                    }
                    case Terrain::Light:
                    {
                        softGlow (g, { x, y }, cell * 1.3f, col, 0.35f);
                        break;
                    }
                    case Terrain::Ground:
                    default: break;
                }
            }
    return img;
}

void drawPlant (juce::Graphics& g, juce::Point<float> c, float s, float growth, int idx, const Scene& sc)
{
    const float ft = (float) sc.time;
    // autumn turns leaves golden, winter frosts them
    const float autumn = clamp01 (1.0f - std::abs (std::fmod (sc.season + 4.0f, 4.0f) - 2.0f));
    const float winter = clamp01 (1.0f - std::abs (std::fmod (sc.season + 4.0f, 4.0f) - 3.0f));
    switch (sc.theme)
    {
        case 0:   // leaves and a flower
        {
            const auto leaf = Colour (0xff5fae5a).interpolatedWith (Colour (0xffd8a040), autumn * 0.8f).interpolatedWith (Colour (0xffd8e8e0), winter * 0.6f);
            for (int k = 0; k < 3; ++k)
            {
                Path l;
                l.addEllipse (0.0f, -s * 0.35f, s * 1.3f, s * 0.7f);
                l.applyTransform (AffineTransform::rotation ((float) k * 2.094f + (float) idx + 0.15f * std::sin (ft * 0.8f + (float) idx)).translated (c));
                g.setColour (leaf.withAlpha (0.88f));
                g.fillPath (l);
            }
            if (growth > 0.6f && winter < 0.5f)
            {
                g.setColour (Colour (idx % 3 == 0 ? 0xfff0a0c0 : (idx % 3 == 1 ? 0xfff5e08a : 0xffffffff)));
                g.fillEllipse (c.x - s * 0.38f, c.y - s * 0.38f, s * 0.76f, s * 0.76f);
                g.setColour (Colour (0xffd89a30));
                g.fillEllipse (c.x - s * 0.14f, c.y - s * 0.14f, s * 0.28f, s * 0.28f);
            }
            break;
        }
        case 1:   // kelp sprig swaying
        {
            Path kelp;
            kelp.startNewSubPath (c.x, c.y + s);
            const float sway = std::sin (ft * 1.2f + (float) idx) * s * 0.6f;
            kelp.quadraticTo (c.x + sway, c.y, c.x + sway * 1.5f, c.y - s * 1.6f);
            g.setColour (Colour (0xff4fb08a).interpolatedWith (Colour (0xffa08040), autumn * 0.5f).withAlpha (0.85f));
            g.strokePath (kelp, roundStroke (juce::jmax (1.5f, s * 0.45f)));
            g.setColour (Colour (0x6090f0c0));
            g.fillEllipse (c.x + sway * 1.5f - s * 0.3f, c.y - s * 1.9f, s * 0.6f, s * 0.6f);
            break;
        }
        case 2:   // stardust
        {
            const float tw = 0.7f + 0.3f * std::sin (ft * 2.0f + (float) idx);
            softGlow (g, c, s * 1.7f, Colour (0xffffd890), 0.8f * tw);
            g.setColour (juce::Colours::white.withAlpha (0.9f * tw));
            g.fillEllipse (c.x - 1.0f, c.y - 1.0f, 2.0f, 2.0f);
            break;
        }
        default:  // nutrient granule
        {
            Path hex;
            hex.addPolygon (c, 6, s * 0.75f, (float) idx);
            g.setColour (Colour (0xffe8d070).withAlpha (0.78f));
            g.fillPath (hex);
            g.setColour (Colour (0x60ffffff));
            g.fillEllipse (c.x - s * 0.2f, c.y - s * 0.35f, s * 0.3f, s * 0.3f);
            break;
        }
    }
}

void drawCorpse (juce::Graphics& g, juce::Point<float> c, float r, const Corpse& k, const Scene& sc)
{
    const float fade = clamp01 (1.0f - k.age / 25.0f) * clamp01 (k.food * 3.0f);
    const auto col = Colour (sc.theme == 2 ? 0xff9aa0b0 : (sc.theme == 3 ? 0xffb8a0c0 : 0xffb8ab98)).withAlpha (0.25f + 0.45f * fade);
    const float s = r * (0.6f + 0.6f * clamp01 (k.food));
    if (sc.theme == 2)   // space debris
    {
        Path p;
        p.addPolygon (c, 5, s, (float) k.age * 0.2f);
        g.setColour (col);
        g.fillPath (p);
        return;
    }
    // a little skeleton: spine and ribs
    g.setColour (col);
    Path bones;
    bones.startNewSubPath (c.x - s, c.y);
    bones.lineTo (c.x + s, c.y);
    for (int i = -1; i <= 1; ++i)
    {
        bones.startNewSubPath (c.x + (float) i * s * 0.45f, c.y - s * 0.5f);
        bones.lineTo (c.x + (float) i * s * 0.45f, c.y + s * 0.5f);
    }
    g.strokePath (bones, roundStroke (juce::jmax (1.0f, s * 0.18f)));
    g.fillEllipse (c.x + s * 0.8f, c.y - s * 0.35f, s * 0.7f, s * 0.7f);
}

//==============================================================================
void drawCreature (juce::Graphics& g, const Creature& c, juce::Point<float> pos, float r, const Scene& sc)
{
    const float ft = (float) sc.time;
    const float flash = clamp01 (c.singFlash);
    const float vitality = 0.5f + 0.5f * clamp01 (c.energy);
    const bool asleep = c.activity < 0.35f;
    const auto base = creatureColour (sc.theme, c);
    const auto body = base.interpolatedWith (juce::Colours::white, flash * 0.45f).withAlpha (vitality * (asleep ? 0.85f : 1.0f));
    const auto dark = base.darker (0.9f).withAlpha (vitality);
    const auto ink = base.darker (1.6f).withAlpha (0.55f * vitality);
    const float speed = std::sqrt (c.vx * c.vx + c.vy * c.vy);
    const float heading = speed > 1.0e-3f ? std::atan2 (c.vy, c.vx) : c.heading;
    const float phase = ft * (4.0f + 10.0f * c.genes[gSpeed]) * (asleep ? 0.2f : 1.0f) + (float) c.id;
    const auto tf = AffineTransform::scale (r).rotated (heading).translated (pos);
    const auto upright = AffineTransform::scale (r).translated (pos);
    const float eye = 0.16f + 0.1f * c.genes[gNight];       // night owls have big eyes

    // soft shadow (not in space) and a faint wake behind fast movers
    const bool flies = roleInfo (c.role).flies;
    if (sc.theme != 2)
    {
        const float lift = flies ? 0.9f : 0.35f;
        g.setColour (juce::Colours::black.withAlpha ((flies ? 0.14f : 0.28f) * vitality));
        g.fillEllipse (pos.x - r * 1.1f + r * lift, pos.y - r * 0.75f + r * (lift + 0.2f), r * 2.2f, r * 1.5f);
    }
    if (speed > 0.02f && sc.theme != 2)
    {
        const float len = r * juce::jmin (5.0f, speed * 40.0f);
        const float bx = pos.x - std::cos (heading) * len, by = pos.y - std::sin (heading) * len;
        Path wake;
        wake.startNewSubPath (pos);
        wake.lineTo (bx, by);
        g.setGradientFill (juce::ColourGradient (base.withAlpha (0.16f), pos.x, pos.y, base.withAlpha (0.0f), bx, by, false));
        g.strokePath (wake, roundStroke (r * 0.9f));
    }

    auto eyes = [&] (float x, float y1, float y2, const AffineTransform& t)
    {
        Path e, p;
        const float es = eye * (asleep ? 1.0f : 1.0f);
        if (asleep)
        {
            e.startNewSubPath (x - es, y1); e.lineTo (x + es, y1);
            e.startNewSubPath (x - es, y2); e.lineTo (x + es, y2);
            g.setColour (dark);
            g.strokePath (e, juce::PathStrokeType (r * 0.08f), t);
            return;
        }
        e.addEllipse (x - es, y1 - es, es * 2.0f, es * 2.0f);
        e.addEllipse (x - es, y2 - es, es * 2.0f, es * 2.0f);
        p.addEllipse (x - es * 0.2f, y1 - es * 0.5f, es, es);
        p.addEllipse (x - es * 0.2f, y2 - es * 0.5f, es, es);
        g.setColour (juce::Colours::white.withAlpha (vitality));
        g.fillPath (e, t);
        g.setColour (juce::Colours::black.withAlpha (vitality));
        g.fillPath (p, t);
    };

    Path p, d;
    switch (sc.theme * numRoles + (int) c.role)
    {
        // ================= GARDEN =================
        case 0:   // beetle
        {
            Path legs;
            for (int k = 0; k < 3; ++k)
            {
                const float lx = -0.5f + 0.5f * (float) k, sw = 0.25f * std::sin (phase + (float) k * 2.0f);
                legs.startNewSubPath (lx, -0.6f); legs.lineTo (lx + sw, -1.15f);
                legs.startNewSubPath (lx, 0.6f);  legs.lineTo (lx - sw, 1.15f);
            }
            g.setColour (dark); g.strokePath (legs, juce::PathStrokeType (r * 0.1f), tf);
            p.addEllipse (-1.0f, -0.75f, 1.9f, 1.5f);
            g.setColour (body); g.fillPath (p, tf);
            pattern (g, p, tf, c, ink);
            d.addEllipse (0.65f, -0.4f, 0.7f, 0.8f);
            g.setColour (dark); g.fillPath (d, tf);
            Path line; line.startNewSubPath (-1.0f, 0.0f); line.lineTo (0.65f, 0.0f);
            g.strokePath (line, juce::PathStrokeType (r * 0.1f), tf);
            break;
        }
        case 1:   // frog
        {
            const float kick = 0.25f * std::sin (phase * 0.5f);
            d.addEllipse (-1.35f - kick, -1.05f, 0.8f, 0.45f); d.addEllipse (-1.35f - kick, 0.6f, 0.8f, 0.45f);
            g.setColour (body.darker (0.25f)); g.fillPath (d, tf);
            p.addEllipse (-1.0f, -0.85f, 2.0f, 1.7f);
            g.setColour (body); g.fillPath (p, tf);
            pattern (g, p, tf, c, ink);
            eyes (0.55f, -0.55f, 0.55f, tf);
            break;
        }
        case 2:   // firefly
        {
            const float flap = 0.35f * std::sin (phase * 2.0f);
            d.addEllipse (-0.6f, -1.1f - flap, 0.9f, 0.9f); d.addEllipse (-0.6f, 0.2f + flap, 0.9f, 0.9f);
            g.setColour (juce::Colours::white.withAlpha (0.35f * vitality)); g.fillPath (d, tf);
            p.addEllipse (-0.7f, -0.35f, 1.5f, 0.7f);
            g.setColour (body); g.fillPath (p, tf);
            break;
        }
        case 3:   // bee: striped, buzzing wings
        {
            const float flap = 0.4f * std::sin (phase * 3.0f);
            d.addEllipse (-0.4f, -1.2f - flap * 0.3f, 0.8f, 0.9f); d.addEllipse (-0.4f, 0.3f + flap * 0.3f, 0.8f, 0.9f);
            g.setColour (juce::Colours::white.withAlpha (0.45f * vitality)); g.fillPath (d, tf);
            p.addEllipse (-0.9f, -0.55f, 1.8f, 1.1f);
            g.setColour (body); g.fillPath (p, tf);
            {
                juce::Graphics::ScopedSaveState ss (g);
                auto clip = p; clip.applyTransform (tf); g.reduceClipRegion (clip);
                Path stripes;
                for (int k = 0; k < 3; ++k) stripes.addRectangle (-0.55f + 0.42f * (float) k, -1.0f, 0.18f, 2.0f);
                g.setColour (Colour (0xff2a2014).withAlpha (vitality)); g.fillPath (stripes, tf);
            }
            Path head; head.addEllipse (0.7f, -0.35f, 0.6f, 0.7f);
            g.setColour (Colour (0xff2a2014).withAlpha (vitality)); g.fillPath (head, tf);
            break;
        }
        case 4:   // snail: spiral shell
        {
            Path foot; foot.addRoundedRectangle (-1.1f, -0.35f, 2.4f, 0.7f, 0.35f);
            g.setColour (Colour (0xffc8b8a0).withAlpha (vitality)); g.fillPath (foot, tf);
            Path stalks; stalks.startNewSubPath (1.0f, -0.1f); stalks.lineTo (1.5f, -0.45f); stalks.startNewSubPath (1.0f, 0.1f); stalks.lineTo (1.5f, 0.45f);
            g.strokePath (stalks, juce::PathStrokeType (r * 0.1f), tf);
            p.addEllipse (-0.9f, -0.85f, 1.6f, 1.7f);
            g.setColour (body); g.fillPath (p, tf);
            Path spiral;
            for (int k = 0; k <= 30; ++k)
            {
                const float a = (float) k * 0.35f, rad = 0.75f * (1.0f - (float) k / 34.0f);
                const float x = -0.1f + std::cos (a) * rad, y = std::sin (a) * rad;
                if (k == 0) spiral.startNewSubPath (x, y); else spiral.lineTo (x, y);
            }
            g.setColour (dark); g.strokePath (spiral, juce::PathStrokeType (r * 0.1f), tf);
            break;
        }
        case 5:   // tortoise: domed shell with plates
        {
            const float step = 0.12f * std::sin (phase * 0.4f);
            Path legs;
            for (float lx : { -0.55f, 0.45f })
            {
                legs.addEllipse (lx - 0.2f + step, -1.15f, 0.45f, 0.45f);
                legs.addEllipse (lx - 0.2f - step, 0.7f, 0.45f, 0.45f);
            }
            legs.addEllipse (0.85f, -0.3f, 0.6f, 0.6f);
            g.setColour (Colour (0xffa8a070).withAlpha (vitality)); g.fillPath (legs, tf);
            p.addEllipse (-1.0f, -0.9f, 1.9f, 1.8f);
            g.setColour (body.darker (0.15f)); g.fillPath (p, tf);
            Path plates;
            plates.addPolygon ({ -0.05f, 0.0f }, 6, 0.38f, 0.0f);
            for (int k = 0; k < 6; ++k) plates.addPolygon ({ -0.05f + 0.62f * std::cos ((float) k * 1.047f), 0.6f * std::sin ((float) k * 1.047f) }, 6, 0.24f, 0.0f);
            g.setColour (body.brighter (0.25f)); g.fillPath (plates, tf);
            g.setColour (ink); g.strokePath (plates, juce::PathStrokeType (r * 0.04f), tf);
            break;
        }
        case 6:   // ant: three segments
        {
            Path legs;
            for (int k = 0; k < 3; ++k)
            {
                const float sw = 0.3f * std::sin (phase * 1.5f + (float) k * 2.1f);
                legs.startNewSubPath (-0.1f, 0.0f); legs.lineTo (-0.6f + 0.5f * (float) k + sw, -1.0f);
                legs.startNewSubPath (-0.1f, 0.0f); legs.lineTo (-0.6f + 0.5f * (float) k - sw, 1.0f);
            }
            g.setColour (dark); g.strokePath (legs, juce::PathStrokeType (r * 0.12f), tf);
            p.addEllipse (-1.3f, -0.45f, 0.9f, 0.9f);
            p.addEllipse (-0.45f, -0.3f, 0.6f, 0.6f);
            p.addEllipse (0.2f, -0.38f, 0.75f, 0.76f);
            g.setColour (body); g.fillPath (p, tf);
            break;
        }
        // ================= OCEAN =================
        case 7:   // fish
        {
            const float wag = 0.35f * std::sin (phase);
            p.addEllipse (-0.9f, -0.5f, 1.9f, 1.0f);
            p.addTriangle (-0.75f, 0.0f, -1.5f, -0.6f + wag, -1.5f, 0.6f + wag);
            g.setColour (body); g.fillPath (p, tf);
            pattern (g, p, tf, c, ink);
            d.addEllipse (0.45f - eye, -0.25f - eye * 0.5f, eye * 1.6f, eye * 1.6f);
            g.setColour (dark); g.fillPath (d, tf);
            break;
        }
        case 8:   // shark (from above)
        {
            const float wag = 0.25f * std::sin (phase * 0.7f);
            p.startNewSubPath (1.35f, 0.0f);
            p.quadraticTo (0.4f, -0.5f, -1.0f, -0.18f);
            p.lineTo (-1.6f, -0.55f + wag); p.lineTo (-1.35f, 0.0f); p.lineTo (-1.6f, 0.55f + wag); p.lineTo (-1.0f, 0.18f);
            p.quadraticTo (0.4f, 0.5f, 1.35f, 0.0f);
            p.closeSubPath();
            p.addTriangle (0.2f, -0.3f, -0.35f, -1.0f, -0.3f, -0.25f);
            p.addTriangle (0.2f, 0.3f, -0.35f, 1.0f, -0.3f, 0.25f);
            g.setColour (body); g.fillPath (p, tf);
            pattern (g, p, tf, c, ink);
            d.addEllipse (0.75f, -0.28f, 0.16f, 0.16f); d.addEllipse (0.75f, 0.12f, 0.16f, 0.16f);
            g.setColour (dark); g.fillPath (d, tf);
            break;
        }
        case 9:   // jellyfish (always upright)
        {
            const float pulse = 1.0f + 0.12f * std::sin (phase * 0.35f);
            p.addCentredArc (0.0f, 0.0f, 1.1f * pulse, 1.0f / pulse, 0.0f, -1.5708f, 1.5708f, true);
            p.closeSubPath();
            g.setColour (body.withMultipliedAlpha (0.8f)); g.fillPath (p, upright);
            for (int k = 0; k < 4; ++k)
            {
                const float x = -0.7f + 0.47f * (float) k;
                d.startNewSubPath (x, 0.05f);
                d.quadraticTo (x + 0.35f * std::sin (phase * 0.4f + (float) k), 1.0f, x + 0.2f * std::sin (phase * 0.4f + (float) k + 1.0f), 1.9f);
            }
            g.setColour (body.withMultipliedAlpha (0.6f));
            g.strokePath (d, roundStroke (r * 0.12f), upright);
            break;
        }
        case 10:  // seahorse (upright, curled tail)
        {
            const float bob = 0.1f * std::sin (phase * 0.6f);
            const auto t2 = AffineTransform::scale (r * (c.vx < 0.0f ? -1.0f : 1.0f), r).translated (pos.x, pos.y + bob * r);
            p.startNewSubPath (0.2f, -1.3f);
            p.quadraticTo (0.9f, -1.2f, 0.7f, -0.9f);     // snout
            p.lineTo (0.35f, -0.85f);
            p.quadraticTo (0.7f, -0.2f, 0.35f, 0.4f);     // belly
            p.quadraticTo (0.1f, 1.0f, 0.4f, 1.2f);       // tail
            p.quadraticTo (0.7f, 1.25f, 0.55f, 0.95f);
            p.quadraticTo (-0.3f, 1.1f, -0.25f, 0.3f);
            p.quadraticTo (-0.45f, -0.6f, 0.2f, -1.3f);
            p.closeSubPath();
            g.setColour (body); g.fillPath (p, t2);
            pattern (g, p, t2, c, ink);
            d.addEllipse (0.2f, -1.1f, 0.18f, 0.18f);
            g.setColour (dark); g.fillPath (d, t2);
            break;
        }
        case 11:  // crab: claws and legs
        {
            const auto side = AffineTransform::scale (r).rotated (heading + 1.5708f).translated (pos);   // crabs walk sideways
            Path legs;
            for (int k = 0; k < 3; ++k)
            {
                const float lx = -0.5f + 0.5f * (float) k, sw = 0.2f * std::sin (phase * 1.5f + (float) k);
                legs.startNewSubPath (lx, 0.3f); legs.lineTo (lx + sw - 0.3f, 1.1f);
                legs.startNewSubPath (-lx, 0.3f); legs.lineTo (-lx - sw + 0.3f, 1.1f);
            }
            g.setColour (dark); g.strokePath (legs, juce::PathStrokeType (r * 0.12f), side);
            const float pinch = 0.15f * std::sin (phase * 0.8f);
            Path claws;
            claws.addEllipse (-1.45f, -1.05f - pinch, 0.6f, 0.5f); claws.addEllipse (0.85f, -1.05f - pinch, 0.6f, 0.5f);
            claws.startNewSubPath (-0.6f, -0.3f); claws.lineTo (-1.15f, -0.75f);
            claws.startNewSubPath (0.6f, -0.3f); claws.lineTo (1.15f, -0.75f);
            g.setColour (body); g.fillPath (claws, side); g.strokePath (claws, juce::PathStrokeType (r * 0.15f), side);
            p.addEllipse (-0.95f, -0.6f, 1.9f, 1.2f);
            g.fillPath (p, side);
            pattern (g, p, side, c, ink);
            eyes (-0.35f, 0.0f, 0.0f, AffineTransform::scale (r).translated (pos.x, pos.y - r * 0.6f));
            break;
        }
        case 12:  // whale: big body, tail fluke, flippers
        {
            const float wag = 0.25f * std::sin (phase * 0.25f);
            p.startNewSubPath (1.3f, 0.0f);
            p.cubicTo (1.2f, -0.75f, -0.6f, -0.7f, -1.1f, -0.15f);
            p.lineTo (-1.65f, -0.55f + wag); p.lineTo (-1.45f, 0.0f); p.lineTo (-1.65f, 0.55f + wag); p.lineTo (-1.1f, 0.15f);
            p.cubicTo (-0.6f, 0.7f, 1.2f, 0.75f, 1.3f, 0.0f);
            p.closeSubPath();
            p.addEllipse (0.0f, -1.0f, 0.55f, 0.35f);
            p.addEllipse (0.0f, 0.65f, 0.55f, 0.35f);
            g.setColour (body); g.fillPath (p, tf);
            pattern (g, p, tf, c, ink);
            Path belly; belly.addEllipse (-0.6f, -0.25f, 1.6f, 0.5f);
            g.setColour (body.brighter (0.35f).withAlpha (0.5f * vitality)); g.fillPath (belly, tf);
            d.addEllipse (0.75f, -0.4f, 0.14f, 0.14f); d.addEllipse (0.75f, 0.26f, 0.14f, 0.14f);
            g.setColour (dark); g.fillPath (d, tf);
            // a spout now and then
            if (std::fmod (ft * 0.15f + (float) c.id * 0.37f, 1.0f) < 0.12f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                for (int k = 0; k < 4; ++k) g.fillEllipse (pos.x - r * 0.3f + (float) k * r * 0.2f, pos.y - r * (1.2f + 0.3f * (float) k), r * 0.25f, r * 0.25f);
            }
            break;
        }
        case 13:  // sardine: small silver fish
        {
            const float wag = 0.4f * std::sin (phase * 1.6f);
            p.addEllipse (-0.9f, -0.32f, 1.9f, 0.64f);
            p.addTriangle (-0.8f, 0.0f, -1.4f, -0.45f + wag, -1.4f, 0.45f + wag);
            g.setColour (body.interpolatedWith (juce::Colours::white, 0.3f)); g.fillPath (p, tf);
            Path line; line.startNewSubPath (-0.8f, 0.0f); line.lineTo (0.8f, 0.0f);
            g.setColour (Colour (0xff4070a0).withAlpha (0.6f * vitality)); g.strokePath (line, juce::PathStrokeType (r * 0.12f), tf);
            break;
        }
        // ================= SPACE =================
        case 14:  // saucer
        {
            const auto tilt = AffineTransform::scale (r).rotated (juce::jlimit (-0.4f, 0.4f, c.vx * 6.0f)).translated (pos);
            d.addEllipse (-0.55f, -0.85f, 1.1f, 0.9f);
            g.setColour (Colour (0xa0c8f0ff)); g.fillPath (d, tilt);
            p.addEllipse (-1.25f, -0.35f, 2.5f, 0.75f);
            g.setColour (body); g.fillPath (p, tilt);
            Path lights;
            for (int k = 0; k < 3; ++k)
                if (std::fmod (ft * 3.0f + (float) k * 0.33f, 1.0f) < 0.5f) lights.addEllipse (-0.75f + 0.6f * (float) k, -0.05f, 0.25f, 0.25f);
            g.setColour (Colour (0xfffff3a0)); g.fillPath (lights, tilt);
            break;
        }
        case 15:  // comet with a tail
        {
            const float tail = r * (3.0f + 6.0f * juce::jmin (1.0f, speed * 8.0f));
            const float bx = pos.x - std::cos (heading) * tail, by = pos.y - std::sin (heading) * tail;
            Path tp;
            const float nx = -std::sin (heading) * r * 0.9f, ny = std::cos (heading) * r * 0.9f;
            tp.startNewSubPath (pos.x + nx, pos.y + ny); tp.lineTo (bx, by); tp.lineTo (pos.x - nx, pos.y - ny); tp.closeSubPath();
            g.setGradientFill (juce::ColourGradient (base.withAlpha (0.7f * vitality), pos.x, pos.y, base.withAlpha (0.0f), bx, by, false));
            g.fillPath (tp);
            softGlow (g, pos, r * 2.2f, base, 0.4f + 0.4f * flash);
            g.setColour (body.interpolatedWith (juce::Colours::white, 0.5f));
            g.fillEllipse (pos.x - r * 0.8f, pos.y - r * 0.8f, r * 1.6f, r * 1.6f);
            break;
        }
        case 16:  // star
        {
            softGlow (g, pos, r * (2.6f + 0.6f * std::sin (phase * 0.25f)), base, 0.3f + 0.5f * flash);
            p.addStar ({}, 5, 0.5f, 1.35f, ft * 0.4f + (float) c.id);
            g.setColour (body); g.fillPath (p, upright);
            break;
        }
        case 17:  // seeder: probe with solar panels
        {
            p.addRectangle (-0.35f, -0.35f, 0.7f, 0.7f);
            Path panels; panels.addRectangle (-0.3f, -1.4f, 0.6f, 0.9f); panels.addRectangle (-0.3f, 0.5f, 0.6f, 0.9f);
            g.setColour (Colour (0xff3a5a9a).withAlpha (vitality)); g.fillPath (panels, tf);
            g.setColour (Colour (0xff9ab8f0).withAlpha (0.6f * vitality)); g.strokePath (panels, juce::PathStrokeType (r * 0.05f), tf);
            g.setColour (body); g.fillPath (p, tf);
            Path dish; dish.addCentredArc (0.55f, 0.0f, 0.3f, 0.3f, 0.0f, 0.0f, 3.1416f, true);
            g.strokePath (dish, juce::PathStrokeType (r * 0.12f), tf);
            if (std::fmod (ft * 1.5f + (float) c.id, 1.0f) < 0.3f) { g.setColour (Colour (0xff80ffb0)); g.fillEllipse (pos.x - r * 0.15f, pos.y - r * 0.15f, r * 0.3f, r * 0.3f); }
            break;
        }
        case 18:  // junkbot: boxy robot with grabber arms
        {
            const float grab = 0.2f * std::sin (phase * 0.9f);
            Path arms;
            arms.startNewSubPath (0.3f, -0.5f); arms.lineTo (1.1f + grab, -0.7f); arms.lineTo (1.35f + grab, -0.45f);
            arms.startNewSubPath (0.3f, 0.5f); arms.lineTo (1.1f + grab, 0.7f); arms.lineTo (1.35f + grab, 0.45f);
            g.setColour (dark); g.strokePath (arms, juce::PathStrokeType (r * 0.14f), tf);
            p.addRoundedRectangle (-0.8f, -0.7f, 1.4f, 1.4f, 0.25f);
            g.setColour (body); g.fillPath (p, tf);
            pattern (g, p, tf, c, ink);
            Path visor; visor.addRoundedRectangle (0.1f, -0.45f, 0.35f, 0.9f, 0.1f);
            g.setColour (Colour (0xffff6a50).withAlpha ((0.5f + 0.5f * std::sin (ft * 4.0f)) * vitality)); g.fillPath (visor, tf);
            break;
        }
        case 19:  // star whale: glowing whale with constellation dots
        {
            const float wag = 0.25f * std::sin (phase * 0.25f);
            softGlow (g, pos, r * 2.4f, base, 0.18f + 0.3f * flash);
            p.startNewSubPath (1.3f, 0.0f);
            p.cubicTo (1.2f, -0.75f, -0.6f, -0.7f, -1.1f, -0.15f);
            p.lineTo (-1.65f, -0.55f + wag); p.lineTo (-1.45f, 0.0f); p.lineTo (-1.65f, 0.55f + wag); p.lineTo (-1.1f, 0.15f);
            p.cubicTo (-0.6f, 0.7f, 1.2f, 0.75f, 1.3f, 0.0f);
            p.closeSubPath();
            g.setColour (body.withMultipliedAlpha (0.55f)); g.fillPath (p, tf);
            g.setColour (body); g.strokePath (p, juce::PathStrokeType (r * 0.06f), tf);
            Path stars;
            juce::Random rnd ((juce::int64) c.id);
            juce::Point<float> prev;
            for (int k = 0; k < 6; ++k)
            {
                const juce::Point<float> s (-0.9f + 1.9f * rnd.nextFloat(), -0.35f + 0.7f * rnd.nextFloat());
                stars.addEllipse (s.x - 0.07f, s.y - 0.07f, 0.14f, 0.14f);
                if (k > 0) { stars.startNewSubPath (prev); stars.lineTo (s); }
                prev = s;
            }
            g.setColour (juce::Colours::white.withAlpha (0.8f * vitality));
            g.fillPath (stars, tf);
            g.strokePath (stars, juce::PathStrokeType (r * 0.02f), tf);
            break;
        }
        case 20:  // sparks: tiny bright points with short trails
        {
            const float tl = r * (2.0f + 6.0f * juce::jmin (1.0f, speed * 8.0f));
            const float bx = pos.x - std::cos (heading) * tl, by = pos.y - std::sin (heading) * tl;
            g.setGradientFill (juce::ColourGradient (base.withAlpha (0.6f), pos.x, pos.y, base.withAlpha (0.0f), bx, by, false));
            Path tr; tr.startNewSubPath (pos); tr.lineTo (bx, by);
            g.strokePath (tr, roundStroke (r * 0.7f));
            softGlow (g, pos, r * 2.0f, base, 0.5f + 0.4f * flash);
            g.setColour (juce::Colours::white.withAlpha (vitality));
            g.fillEllipse (pos.x - r * 0.45f, pos.y - r * 0.45f, r * 0.9f, r * 0.9f);
            break;
        }
        // ================= CELLS =================
        case 21:  // cell
        {
            p.addEllipse (-1.0f, -1.0f, 2.0f, 2.0f);
            g.setColour (body.withMultipliedAlpha (0.4f)); g.fillPath (p, tf);
            pattern (g, p, tf, c, ink);
            g.setColour (body); g.strokePath (p, juce::PathStrokeType (r * 0.13f), tf);
            d.addEllipse (0.0f, -0.35f, 0.7f, 0.7f);
            g.setColour (dark.brighter (0.3f)); g.fillPath (d, tf);
            break;
        }
        case 22:  // amoeba reaching forward
        {
            for (int k = 0; k <= 28; ++k)
            {
                const float th = twoPi * (float) k / 28.0f;
                const float rad = 1.0f + 0.16f * std::sin (3.0f * th + phase * 0.3f) + 0.35f * juce::jmax (0.0f, std::cos (th));
                if (k == 0) p.startNewSubPath (std::cos (th) * rad, std::sin (th) * rad); else p.lineTo (std::cos (th) * rad, std::sin (th) * rad);
            }
            p.closeSubPath();
            p = p.createPathWithRoundedCorners (0.3f);
            g.setColour (body.withMultipliedAlpha (0.55f)); g.fillPath (p, tf);
            pattern (g, p, tf, c, ink);
            g.setColour (body); g.strokePath (p, juce::PathStrokeType (r * 0.1f), tf);
            d.addEllipse (-0.4f, -0.3f, 0.6f, 0.6f);
            g.setColour (dark); g.fillPath (d, tf);
            break;
        }
        case 23:  // diatom wheel
        {
            const auto spin = AffineTransform::scale (r).rotated (ft * 0.6f + (float) c.id).translated (pos);
            p.addEllipse (-1.0f, -1.0f, 2.0f, 2.0f);
            p.addEllipse (-0.5f, -0.5f, 1.0f, 1.0f);
            for (int k = 0; k < 8; ++k)
            {
                const float th = twoPi * (float) k / 8.0f;
                p.startNewSubPath (std::cos (th) * 0.5f, std::sin (th) * 0.5f);
                p.lineTo (std::cos (th), std::sin (th));
            }
            g.setColour (body); g.strokePath (p, juce::PathStrokeType (r * 0.12f), spin);
            break;
        }
        case 24:  // spore: spiky ball
        {
            const auto spin = AffineTransform::scale (r).rotated (ft * 0.4f + (float) c.id).translated (pos);
            p.addStar ({}, 10, 0.65f, 1.05f, 0.0f);
            g.setColour (body); g.fillPath (p, spin);
            d.addEllipse (-0.35f, -0.35f, 0.7f, 0.7f);
            g.setColour (body.brighter (0.5f)); g.fillPath (d, spin);
            break;
        }
        case 25:  // phage: hexagon head, tail, legs
        {
            Path legs;
            for (int k = 0; k < 3; ++k)
            {
                const float sw = 0.25f * std::sin (phase + (float) k);
                legs.startNewSubPath (-0.9f, 0.0f); legs.lineTo (-1.3f + sw, -0.7f + 0.7f * (float) k);
            }
            legs.startNewSubPath (-0.2f, 0.0f); legs.lineTo (-0.9f, 0.0f);
            g.setColour (dark); g.strokePath (legs, juce::PathStrokeType (r * 0.12f), tf);
            p.addPolygon ({ 0.35f, 0.0f }, 6, 0.65f, 0.0f);
            g.setColour (body); g.fillPath (p, tf);
            pattern (g, p, tf, c, ink);
            g.setColour (ink); g.strokePath (p, juce::PathStrokeType (r * 0.06f), tf);
            break;
        }
        case 26:  // colony: a cluster of cells
        {
            juce::Random rnd ((juce::int64) c.id * 13);
            for (int k = 0; k < 7; ++k)
            {
                const float a = (float) k * 0.9f + 0.2f * std::sin (phase * 0.2f + (float) k);
                const float rad = k == 0 ? 0.0f : 0.6f;
                const float cs = k == 0 ? 0.55f : 0.42f + 0.1f * rnd.nextFloat();
                p.addEllipse (std::cos (a) * rad - cs, std::sin (a) * rad - cs, cs * 2.0f, cs * 2.0f);
            }
            g.setColour (body.withMultipliedAlpha (0.45f)); g.fillPath (p, tf);
            g.setColour (body); g.strokePath (p, juce::PathStrokeType (r * 0.05f), tf);
            break;
        }
        default:  // bacterium: capsule with a whipping flagellum
        {
            Path fl;
            fl.startNewSubPath (-0.8f, 0.0f);
            for (int k = 1; k <= 8; ++k) fl.lineTo (-0.8f - 0.15f * (float) k, 0.25f * std::sin (phase * 2.0f + (float) k));
            g.setColour (dark); g.strokePath (fl, juce::PathStrokeType (r * 0.08f), tf);
            p.addRoundedRectangle (-0.85f, -0.4f, 1.7f, 0.8f, 0.4f);
            g.setColour (body); g.fillPath (p, tf);
            pattern (g, p, tf, c, ink);
            break;
        }
    }

    // sleeping creatures dream a little "z"
    if (asleep && std::fmod (ft * 0.5f + (float) c.id * 0.31f, 1.0f) < 0.5f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::FontOptions (juce::jmax (8.0f, r * 1.1f), juce::Font::bold));
        g.drawText ("z", juce::Rectangle<float> (pos.x + r * 0.6f, pos.y - r * 2.2f, r * 1.5f, r * 1.5f), juce::Justification::centred);
    }
}

void drawGlow (juce::Graphics& g, const Creature& c, juce::Point<float> pos, float r, const Scene& sc)
{
    const float flash = clamp01 (c.singFlash);
    const auto base = creatureColour (sc.theme, c);
    const float night = 1.0f - sc.light;
    // glow gene: shines in the dark (fireflies, jellies, stars always a little)
    const bool natural = c.role == Role::Drifter || (sc.theme == 2 && (c.role == Role::Hunter || c.role == Role::Swarm));
    const float glow = juce::jmax (c.genes[gGlow] * night, natural ? 0.35f + 0.5f * night : 0.0f);
    if (glow > 0.05f) softGlow (g, pos, r * (2.4f + 1.2f * glow), base.brighter (0.4f), 0.45f * glow);

    // song: a ring that expands while the note sounds, and a note that floats up
    if (flash > 0.01f)
    {
        const float rr = r * (1.6f + (1.0f - flash) * 2.6f);
        g.setColour (base.brighter (0.6f).withAlpha (flash * 0.7f));
        g.drawEllipse (pos.x - rr, pos.y - rr, rr * 2.0f, rr * 2.0f, 1.5f);
        softGlow (g, pos, r * 3.0f, base, flash * 0.35f);
        if (flash > 0.05f)
        {
            const float rise = (1.0f - flash) * r * 5.0f;
            const float nx = pos.x + r * 1.2f, ny = pos.y - r * 1.6f - rise;
            const float ns = juce::jmax (3.0f, r * 0.55f);
            Path note;
            note.addEllipse (nx - ns, ny - ns * 0.7f, ns * 2.0f, ns * 1.4f);
            note.addRectangle (nx + ns * 0.75f, ny - ns * 3.2f, ns * 0.35f, ns * 3.2f);
            note.addTriangle (nx + ns * 1.1f, ny - ns * 3.2f, nx + ns * 2.3f, ny - ns * 2.2f, nx + ns * 1.1f, ny - ns * 2.4f);
            g.setColour (base.brighter (0.8f).withAlpha (juce::jmin (1.0f, flash * 1.4f)));
            g.fillPath (note);
        }
    }
}

void drawSky (juce::Graphics& g, juce::Rectangle<float> a, const World& w, const Scene& sc)
{
    const float night = 1.0f - w.light;
    // dusk and dawn glow warm, night is deep blue
    const float twilight = clamp01 (1.0f - std::abs (w.light - 0.45f) / 0.35f) * (w.light < 0.999f ? 1.0f : 0.0f);
    if (twilight > 0.01f)
    {
        g.setColour (Colour (0xffff9a50).withAlpha (0.12f * twilight));
        g.fillRect (a);
    }
    if (night > 0.01f)
    {
        g.setColour (Colour (sc.theme == 2 ? 0xff000008 : 0xff050a22).withAlpha (0.62f * night));
        g.fillRect (a);
    }
    // winter frost
    const float winter = clamp01 (1.0f - std::abs (std::fmod (w.season + 4.0f, 4.0f) - 3.0f)) * 0.8f
                       + (w.weather == Weather::Snow ? 0.4f * w.weatherAmount : 0.0f);
    if (winter > 0.02f && sc.theme != 2)
    {
        g.setGradientFill (juce::ColourGradient (Colour (0xffe8f4ff).withAlpha (0.0f), a.getCentreX(), a.getY(),
                                                 Colour (0xffe8f4ff).withAlpha (0.22f * juce::jmin (1.0f, winter)), a.getCentreX(), a.getBottom(), false));
        g.fillRect (a);
    }
}

void drawWeather (juce::Graphics& g, juce::Rectangle<float> a, const World& w, const Scene& sc, float windX)
{
    const float amt = w.weatherAmount;
    if (amt < 0.01f) return;
    const float ft = (float) sc.time;
    juce::Random rnd (4242);
    switch (w.weather)
    {
        case Weather::Rain:
        case Weather::Storm:
        {
            const bool storm = w.weather == Weather::Storm;
            const int n = (int) ((storm ? 220.0f : 140.0f) * amt);
            const float slant = 0.15f + windX * 0.5f + (storm ? 0.3f : 0.0f);
            g.setColour (Colour (sc.theme == 2 ? 0xffb0c8ff : 0xffcfe4ff).withAlpha (0.35f));
            for (int i = 0; i < n; ++i)
            {
                const float speed = 0.9f + 0.5f * rnd.nextFloat();
                const float y = std::fmod (rnd.nextFloat() + ft * speed, 1.0f);
                const float x = std::fmod (rnd.nextFloat() + y * slant + 2.0f, 1.0f);
                const float px = a.getX() + x * a.getWidth(), py = a.getY() + y * a.getHeight();
                const float len = a.getHeight() * 0.025f;
                g.drawLine (px, py, px - slant * len, py - len, 1.0f);
            }
            if (storm)
            {
                // lightning: a short white flash every few seconds
                const float ph = std::fmod (ft * 0.23f, 1.0f);
                if (ph < 0.025f) { g.setColour (juce::Colours::white.withAlpha (0.35f * amt * (1.0f - ph / 0.025f))); g.fillRect (a); }
            }
            break;
        }
        case Weather::Snow:
        {
            const int n = (int) (160.0f * amt);
            for (int i = 0; i < n; ++i)
            {
                const float speed = 0.04f + 0.06f * rnd.nextFloat();
                const float y = std::fmod (rnd.nextFloat() + ft * speed, 1.0f);
                const float x = std::fmod (rnd.nextFloat() + 0.02f * std::sin (ft * 1.3f + (float) i) + windX * y * 0.3f + 2.0f, 1.0f);
                const float s = 1.5f + 2.5f * rnd.nextFloat();
                g.setColour (juce::Colours::white.withAlpha (0.55f + 0.3f * rnd.nextFloat()));
                g.fillEllipse (a.getX() + x * a.getWidth() - s * 0.5f, a.getY() + y * a.getHeight() - s * 0.5f, s, s);
            }
            break;
        }
        case Weather::Fog:
        {
            for (int i = 0; i < 9; ++i)
            {
                const float x = std::fmod (rnd.nextFloat() + ft * 0.01f * (1.0f + rnd.nextFloat()), 1.2f) - 0.1f;
                const float y = rnd.nextFloat();
                softGlow (g, { a.getX() + x * a.getWidth(), a.getY() + y * a.getHeight() }, a.getWidth() * (0.25f + 0.2f * rnd.nextFloat()),
                          Colour (0xffdde4ea), 0.32f * amt);
            }
            g.setColour (Colour (0xffdde4ea).withAlpha (0.12f * amt));
            g.fillRect (a);
            break;
        }
        case Weather::Clear:
        default: break;
    }
}

void drawIcon (juce::Graphics& g, juce::Rectangle<float> box, int theme, Role role)
{
    Creature c;
    c.alive = true;
    c.role = role;
    c.id = 7;
    c.energy = 1.0f;
    c.age = 100.0f;
    c.heading = 0.0f;
    for (auto& v : c.genes.g) v = 0.5f;
    c.genes[gPattern] = 0.0f;
    c.genes[gNight] = 0.0f;
    Scene sc;
    sc.theme = theme;
    sc.time = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float r = juce::jmin (box.getWidth(), box.getHeight()) * (role == Role::Giant ? 0.3f : 0.28f);
    drawCreature (g, c, box.getCentre(), r, sc);
}

} // namespace lifeart

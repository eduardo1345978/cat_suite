#include "PluginEditor.h"
#include "MidiExport.h"

namespace C = cat::Colours;
using namespace lifecat;

namespace
{
    double now() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

    constexpr float twoPi = 6.2831853f;

    const char* toolTips[] = {
        "Inspect: click a creature to see (and hear) its genes. Right-click works with every tool.",
        "Food: click or drag to scatter food. Creatures gather where you feed them.",
        "Add a plant-eater (grazer) where you click",
        "Add a hunter where you click (it eats grazers)",
        "Add a drifter where you click (lives on light, sings long notes)",
        "Remove: click or drag over creatures to take them out",
        "Wall: drag to build a wall. Walls split the world into separate tribes.",
        "Warm zone: click or drag out a circle. Plants grow faster, life is busier.",
        "Cold zone: click or drag out a circle. Life slows down and saves energy.",
        "Current: drag in the direction the water / wind should flow",
        "Erase: click a wall or zone to remove it",
    };

    juce::String toolName (GardenTool t, int theme)
    {
        switch (t)
        {
            case GardenTool::Inspect: return "Inspect";
            case GardenTool::Food:    return "Food";
            case GardenTool::Grazer:  return "+ " + LifeViewState::roleName (theme, Role::Grazer);
            case GardenTool::Hunter:  return "+ " + LifeViewState::roleName (theme, Role::Hunter);
            case GardenTool::Drifter: return "+ " + LifeViewState::roleName (theme, Role::Drifter);
            case GardenTool::Remove:  return "Remove";
            case GardenTool::Wall:    return "Wall";
            case GardenTool::Warm:    return "Warm";
            case GardenTool::Cold:    return "Cold";
            case GardenTool::Current: return "Current";
            case GardenTool::Erase:   return "Erase";
        }
        return {};
    }

    juce::String toolHint (GardenTool t)
    {
        switch (t)
        {
            case GardenTool::Inspect: return "Click a creature to inspect it";
            case GardenTool::Food:    return "Click or drag to drop food";
            case GardenTool::Grazer:
            case GardenTool::Hunter:
            case GardenTool::Drifter: return "Click to add";
            case GardenTool::Remove:  return "Click or drag to remove";
            case GardenTool::Wall:    return "Drag to build a wall";
            case GardenTool::Warm:
            case GardenTool::Cold:    return "Click, or drag out the size";
            case GardenTool::Current: return "Drag in the flow direction";
            case GardenTool::Erase:   return "Click a wall or zone to erase it";
        }
        return {};
    }
}

//==============================================================================
juce::String LifeViewState::roleName (int theme, Role role, bool plural)
{
    static const char* singular[4][3] = { { "Beetle", "Frog", "Firefly" }, { "Fish", "Shark", "Jelly" },
                                          { "Saucer", "Comet", "Star" },   { "Cell", "Amoeba", "Diatom" } };
    static const char* many[4][3] = { { "Beetles", "Frogs", "Fireflies" }, { "Fish", "Sharks", "Jellies" },
                                      { "Saucers", "Comets", "Stars" },    { "Cells", "Amoebas", "Diatoms" } };
    const int t = juce::jlimit (0, 3, theme), r = juce::jlimit (0, 2, (int) role);
    return plural ? many[t][r] : singular[t][r];
}

juce::Colour LifeViewState::roleColour (int theme, Role role)
{
    static const juce::uint32 cols[4][3] = {
        { 0xff8fd18a, 0xffd98b6a, 0xfff2df7a },      // garden: green beetles, orange frogs, yellow fireflies
        { 0xff7cc6e0, 0xff9aa6c2, 0xffd59ae6 },      // ocean: blue fish, grey sharks, pink jellies
        { 0xffa9b8ff, 0xffffa86b, 0xfffff0b0 },      // space: lavender saucers, fiery comets, pale stars
        { 0xff9fe0b8, 0xffe58aa8, 0xffb7a6f0 },      // cells
    };
    return juce::Colour (cols[juce::jlimit (0, 3, theme)][juce::jlimit (0, 2, (int) role)]);
}

juce::Colour LifeViewState::creatureColour (int theme, const Creature& c)
{
    return roleColour (theme, c.role).withRotatedHue ((c.genes[gHue] - 0.5f) * 0.22f)
                                     .withMultipliedBrightness (0.85f + 0.3f * c.genes[gBright]);
}

void LifeViewState::say (const juce::String& text)
{
    message = text;
    messageTime = now();
}

//==============================================================================
WorldView::WorldView (LifeCatProcessor& p, LifeViewState& v) : processor (p), view (v)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

juce::Rectangle<float> WorldView::area() const { return getLocalBounds().toFloat().reduced (2.0f); }

juce::Point<float> WorldView::toWorld (juce::Point<float> s) const
{
    const auto a = area();
    return { juce::jlimit (0.0f, 1.0f, (s.x - a.getX()) / a.getWidth()), juce::jlimit (0.0f, 1.0f, (s.y - a.getY()) / a.getHeight()) };
}

juce::Point<float> WorldView::toScreen (float x, float y) const
{
    const auto a = area();
    return { a.getX() + x * a.getWidth(), a.getY() + y * a.getHeight() };
}

float WorldView::creatureRadius (const Creature& c) const
{
    const float base = area().getWidth() * (0.011f + 0.011f * c.genes[gSize]);
    return base * (c.role == Role::Hunter ? 1.25f : 1.0f);
}

uint32_t WorldView::creatureAt (const World& w, juce::Point<float> p, float radius)
{
    uint32_t best = 0;
    float bestD = radius * radius;
    for (const auto& c : w.creatures)
    {
        if (! c.alive) continue;
        const float dx = c.x - p.x, dy = c.y - p.y, d = dx * dx + dy * dy;
        if (d < bestD) { bestD = d; best = c.id; }
    }
    return best;
}

void WorldView::tick()
{
    const auto& w = view.world;
    if (w.time < lastWorldTime) lastEventPos = w.eventPos;           // a new world: ignore its old events
    lastWorldTime = w.time;

    const int n = (int) w.events.size();
    int fresh = ((w.eventPos - lastEventPos) % n + n) % n;
    const double t = now();
    for (int k = fresh; k > 0; --k)
    {
        const auto& e = w.events[(size_t) (((w.eventPos - k) % n + n) % n)];
        if (fx.size() < 64) fx.push_back ({ e.x, e.y, e.kind, e.role, t });
    }
    lastEventPos = w.eventPos;
    fx.erase (std::remove_if (fx.begin(), fx.end(), [t] (const Fx& f) { return t - f.t0 > 1.2; }), fx.end());
}

//------------------------------------------------------------------------------
void WorldView::paint (juce::Graphics& g)
{
    const auto a = area();
    const double t = now();

    juce::Path clip;
    clip.addRoundedRectangle (a, 10.0f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (clip);
        drawBackground (g, a, t);
        if (view.valid)
        {
            drawZones (g, t);
            drawPlants (g, t);
            drawWalls (g);
            // drifters float above, hunters on top of grazers
            for (auto role : { Role::Grazer, Role::Hunter, Role::Drifter })
                for (const auto& c : view.world.creatures)
                    if (c.alive && c.role == role) drawCreature (g, c, t);
            drawEffects (g, t);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, a.getCentreX(), a.getCentreY(),
                                                 juce::Colours::black.withAlpha (0.45f), a.getX(), a.getY(), true));
        g.fillRect (a);
        drawGesture (g);
        drawHud (g, a, t);
    }
    g.setColour (C::outline);
    g.drawRoundedRectangle (a, 10.0f, 1.2f);
}

void WorldView::drawBackground (juce::Graphics& g, juce::Rectangle<float> a, double time) const
{
    const float ft = (float) time;
    juce::Random rnd (1234 + view.theme);
    switch (view.theme)
    {
        case 0:   // garden pond: deep green with dappled light and lily pads
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17301f), a.getCentreX(), a.getY(),
                                                     juce::Colour (0xff0b1810), a.getCentreX(), a.getBottom(), false));
            g.fillRect (a);
            for (int i = 0; i < 14; ++i)
            {
                const float x = a.getX() + rnd.nextFloat() * a.getWidth(), y = a.getY() + rnd.nextFloat() * a.getHeight();
                const float r = a.getWidth() * (0.05f + 0.08f * rnd.nextFloat());
                const float wob = 1.0f + 0.08f * std::sin (ft * 0.3f + (float) i);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0x10c8f0a0), x, y, juce::Colour (0x00c8f0a0), x + r * wob, y, true));
                g.fillEllipse (x - r * wob, y - r * wob, 2.0f * r * wob, 2.0f * r * wob);
            }
            for (int i = 0; i < 6; ++i)
            {
                const float x = a.getX() + rnd.nextFloat() * a.getWidth(), y = a.getY() + rnd.nextFloat() * a.getHeight();
                const float r = a.getWidth() * (0.018f + 0.02f * rnd.nextFloat());
                juce::Path pad;
                pad.addPieSegment (x - r, y - r, 2.0f * r, 2.0f * r, 0.35f + (float) i, twoPi + (float) i, 0.0f);
                g.setColour (juce::Colour (0x2a5a9a50));
                g.fillPath (pad);
            }
            break;
        }
        case 1:   // ocean: blue depth, light rays, rising bubbles
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0f3550), a.getCentreX(), a.getY(),
                                                     juce::Colour (0xff050f1c), a.getCentreX(), a.getBottom(), false));
            g.fillRect (a);
            for (int i = 0; i < 5; ++i)
            {
                const float x = a.getX() + a.getWidth() * (0.1f + 0.2f * (float) i) + 20.0f * std::sin (ft * 0.15f + (float) i);
                juce::Path ray;
                ray.startNewSubPath (x, a.getY());
                ray.lineTo (x + a.getWidth() * 0.06f, a.getY());
                ray.lineTo (x + a.getWidth() * 0.2f, a.getBottom());
                ray.lineTo (x + a.getWidth() * 0.1f, a.getBottom());
                ray.closeSubPath();
                g.setGradientFill (juce::ColourGradient (juce::Colour (0x149fd8ff), x, a.getY(), juce::Colour (0x009fd8ff), x, a.getBottom(), false));
                g.fillPath (ray);
            }
            for (int i = 0; i < 24; ++i)
            {
                const float x0 = rnd.nextFloat(), speed = 0.02f + 0.04f * rnd.nextFloat(), ph = rnd.nextFloat();
                const float y = 1.0f - std::fmod (ph + ft * speed, 1.0f);
                const float x = x0 + 0.01f * std::sin (ft * 2.0f + (float) i);
                const float r = 1.0f + 2.5f * rnd.nextFloat();
                g.setColour (juce::Colour (0x40bfe8ff));
                g.drawEllipse (a.getX() + x * a.getWidth() - r, a.getY() + y * a.getHeight() - r, 2.0f * r, 2.0f * r, 0.8f);
            }
            break;
        }
        case 2:   // space: black, nebula glow, twinkling stars
        {
            g.fillAll (juce::Colour (0xff06060d));
            const juce::uint32 neb[] = { 0x2a6a3fa0, 0x223f6aa0, 0x1ca04f6a };
            for (int i = 0; i < 3; ++i)
            {
                const float x = a.getX() + a.getWidth() * (0.25f + 0.25f * (float) i), y = a.getY() + a.getHeight() * (0.3f + 0.2f * (float) (i % 2));
                const float r = a.getWidth() * 0.35f;
                g.setGradientFill (juce::ColourGradient (juce::Colour (neb[i]), x, y, juce::Colour (neb[i]).withAlpha (0.0f), x + r, y, true));
                g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
            }
            for (int i = 0; i < 140; ++i)
            {
                const float x = a.getX() + rnd.nextFloat() * a.getWidth(), y = a.getY() + rnd.nextFloat() * a.getHeight();
                const float tw = 0.5f + 0.5f * std::sin (ft * (0.5f + rnd.nextFloat() * 2.0f) + (float) i);
                const float s = rnd.nextFloat() < 0.1f ? 1.8f : 1.0f;
                g.setColour (juce::Colours::white.withAlpha (0.15f + 0.45f * tw * rnd.nextFloat()));
                g.fillEllipse (x, y, s, s);
            }
            break;
        }
        default:  // cells: a petri dish under the microscope
        {
            g.fillAll (juce::Colour (0xff120f16));
            const auto c = a.getCentre();
            const float r = juce::jmin (a.getWidth(), a.getHeight()) * 0.49f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2030), c.x, c.y, juce::Colour (0xff17121c), c.x + r, c.y, true));
            g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
            g.setColour (juce::Colour (0x10ffffff));
            for (float x = a.getX(); x < a.getRight(); x += a.getWidth() / 12.0f) g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
            for (float y = a.getY(); y < a.getBottom(); y += a.getHeight() / 12.0f) g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
            g.setColour (juce::Colour (0x40d8b8e8));
            g.drawEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r, 2.0f);
            g.setColour (juce::Colour (0x12ffffff));
            g.fillEllipse (c.x - r * 0.75f, c.y - r * 0.85f, r * 0.6f, r * 0.25f);
            break;
        }
    }
}

void WorldView::drawZones (juce::Graphics& g, double time) const
{
    const auto& w = view.world;
    const float aw = area().getWidth();
    for (int i = 0; i < w.numZones; ++i)
    {
        const auto& z = w.zones[(size_t) i];
        const auto c = toScreen (z.x, z.y);
        const float r = z.r * aw;
        if (z.type == ZoneType::Current)
        {
            g.setColour (juce::Colour (0x1470d0e0));
            g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
            const float len = std::sqrt (z.dx * z.dx + z.dy * z.dy);
            const float ux = len > 1.0e-4f ? z.dx / len : 1.0f, uy = len > 1.0e-4f ? z.dy / len : 0.0f;
            for (int k = 0; k < 7; ++k)
            {
                const float lane = ((float) k - 3.0f) / 3.5f;
                const float ph = std::fmod ((float) time * 0.35f + (float) k * 0.37f, 1.0f);
                const float along = (ph * 2.0f - 1.0f) * r * std::sqrt (juce::jmax (0.0f, 1.0f - lane * lane));
                const float px = c.x - uy * lane * r * 0.8f + ux * along, py = c.y + ux * lane * r * 0.8f + uy * along;
                const float alpha = std::sin (ph * 3.14159f) * 0.6f;
                g.setColour (juce::Colour (0xff8ee0f0).withAlpha (alpha));
                juce::Path arrow;
                arrow.startNewSubPath (px - ux * 9.0f, py - uy * 9.0f);
                arrow.lineTo (px, py);
                arrow.lineTo (px - ux * 4.0f - uy * 3.5f, py - uy * 4.0f + ux * 3.5f);
                arrow.startNewSubPath (px, py);
                arrow.lineTo (px - ux * 4.0f + uy * 3.5f, py - uy * 4.0f - ux * 3.5f);
                g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
        else
        {
            const auto col = z.type == ZoneType::Warm ? juce::Colour (0xffffa050) : juce::Colour (0xff70b0ff);
            const float pulse = 1.0f + 0.03f * std::sin ((float) time * 1.3f + (float) i);
            g.setGradientFill (juce::ColourGradient (col.withAlpha (0.24f), c.x, c.y, col.withAlpha (0.0f), c.x + r * pulse, c.y, true));
            g.fillEllipse (c.x - r * pulse, c.y - r * pulse, 2.0f * r * pulse, 2.0f * r * pulse);
            g.setColour (col.withAlpha (0.25f));
            const float dash[] = { 4.0f, 5.0f };
            juce::Path ring;
            ring.addEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
            juce::Path dashed;
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, ring, dash, 2);
            g.fillPath (dashed);
        }
    }
}

void WorldView::drawWalls (juce::Graphics& g) const
{
    const auto& w = view.world;
    static const juce::uint32 cols[] = { 0xff6b6152, 0xff3d4f5e, 0xff6fd8ff, 0xffd9a0c8 };
    const auto col = juce::Colour (cols[juce::jlimit (0, 3, view.theme)]);
    const float thick = juce::jmax (4.0f, area().getWidth() * 0.009f);
    for (int i = 0; i < w.numWalls; ++i)
    {
        const auto& wl = w.walls[(size_t) i];
        const auto p1 = toScreen (wl.x1, wl.y1), p2 = toScreen (wl.x2, wl.y2);
        if (view.theme == 2)   // force field
        {
            g.setColour (col.withAlpha (0.18f));
            g.drawLine ({ p1, p2 }, thick * 2.2f);
        }
        g.setColour (col);
        juce::Path line;
        line.startNewSubPath (p1);
        line.lineTo (p2);
        g.strokePath (line, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col.brighter (0.4f).withAlpha (0.5f));
        g.strokePath (line, juce::PathStrokeType (thick * 0.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void WorldView::drawPlants (juce::Graphics& g, double time) const
{
    const auto& w = view.world;
    const float aw = area().getWidth();
    const float ft = (float) time;
    int idx = 0;
    for (const auto& pl : w.plants)
    {
        ++idx;
        if (! pl.alive) continue;
        const auto c = toScreen (pl.x, pl.y);
        const float s = aw * (0.0035f + 0.007f * juce::jlimit (0.0f, 1.0f, pl.size));
        switch (view.theme)
        {
            case 0:   // three leaves and a little flower
            {
                for (int k = 0; k < 3; ++k)
                {
                    juce::Path leaf;
                    leaf.addEllipse (0.0f, -s * 0.35f, s * 1.3f, s * 0.7f);
                    leaf.applyTransform (juce::AffineTransform::rotation ((float) k * 2.094f + (float) idx).translated (c));
                    g.setColour (juce::Colour (0xff5fae5a).withAlpha (0.85f));
                    g.fillPath (leaf);
                }
                if (pl.size > 0.6f)
                {
                    g.setColour (juce::Colour (idx % 3 == 0 ? 0xfff0a0c0 : 0xfff5e08a));
                    g.fillEllipse (c.x - s * 0.35f, c.y - s * 0.35f, s * 0.7f, s * 0.7f);
                }
                break;
            }
            case 1:   // kelp sprig swaying
            {
                juce::Path kelp;
                kelp.startNewSubPath (c.x, c.y + s);
                const float sway = std::sin (ft * 1.2f + (float) idx) * s * 0.6f;
                kelp.quadraticTo (c.x + sway, c.y, c.x + sway * 1.5f, c.y - s * 1.6f);
                g.setColour (juce::Colour (0xff4fb08a).withAlpha (0.8f));
                g.strokePath (kelp, juce::PathStrokeType (juce::jmax (1.5f, s * 0.45f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (0x5090f0c0));
                g.fillEllipse (c.x + sway * 1.5f - s * 0.3f, c.y - s * 1.9f, s * 0.6f, s * 0.6f);
                break;
            }
            case 2:   // stardust
            {
                const float tw = 0.7f + 0.3f * std::sin (ft * 2.0f + (float) idx);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffd890).withAlpha (0.8f * tw), c.x, c.y,
                                                         juce::Colour (0x00ffd890), c.x + s * 1.6f, c.y, true));
                g.fillEllipse (c.x - s * 1.6f, c.y - s * 1.6f, s * 3.2f, s * 3.2f);
                g.setColour (juce::Colours::white.withAlpha (0.9f * tw));
                g.fillEllipse (c.x - 1.0f, c.y - 1.0f, 2.0f, 2.0f);
                break;
            }
            default:  // nutrient granule
            {
                juce::Path hex;
                hex.addPolygon (c, 6, s * 0.75f, (float) idx);
                g.setColour (juce::Colour (0xffe8d070).withAlpha (0.75f));
                g.fillPath (hex);
                g.setColour (juce::Colour (0x60ffffff));
                g.fillEllipse (c.x - s * 0.2f, c.y - s * 0.35f, s * 0.3f, s * 0.3f);
                break;
            }
        }
    }
}

void WorldView::drawCreature (juce::Graphics& g, const Creature& c, double time) const
{
    const auto pos = toScreen (c.x, c.y);
    const float r = creatureRadius (c);
    const float ft = (float) time;
    const float flash = juce::jlimit (0.0f, 1.0f, c.singFlash);
    const float vitality = 0.45f + 0.55f * juce::jlimit (0.0f, 1.0f, c.energy);
    const auto base = LifeViewState::creatureColour (view.theme, c);
    const auto body = base.interpolatedWith (juce::Colours::white, flash * 0.5f).withAlpha (vitality);
    const auto dark = base.darker (0.9f).withAlpha (vitality);
    const float speed = std::sqrt (c.vx * c.vx + c.vy * c.vy);
    const float heading = speed > 1.0e-3f ? std::atan2 (c.vy, c.vx) : c.heading;
    const float phase = ft * (4.0f + 10.0f * c.genes[gSpeed]) + (float) c.id;
    const auto tf = juce::AffineTransform::scale (r).rotated (heading).translated (pos);
    const auto upright = juce::AffineTransform::scale (r).translated (pos);

    // song: a ring that expands while the note sounds
    if (flash > 0.01f)
    {
        const float rr = r * (1.6f + (1.0f - flash) * 2.6f);
        g.setColour (base.brighter (0.6f).withAlpha (flash * 0.7f));
        g.drawEllipse (pos.x - rr, pos.y - rr, rr * 2.0f, rr * 2.0f, 1.5f);
        g.setGradientFill (juce::ColourGradient (base.withAlpha (flash * 0.35f), pos.x, pos.y, base.withAlpha (0.0f), pos.x + r * 3.0f, pos.y, true));
        g.fillEllipse (pos.x - r * 3.0f, pos.y - r * 3.0f, r * 6.0f, r * 6.0f);
    }

    auto glow = [&] (float radius, float alpha)
    {
        g.setGradientFill (juce::ColourGradient (base.withAlpha (alpha), pos.x, pos.y, base.withAlpha (0.0f), pos.x + radius, pos.y, true));
        g.fillEllipse (pos.x - radius, pos.y - radius, radius * 2.0f, radius * 2.0f);
    };

    // soft shadow (not in space) and a faint wake behind fast movers
    if (view.theme != 2 && c.role != Role::Drifter)
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f * vitality));
        g.fillEllipse (pos.x - r * 1.1f + r * 0.35f, pos.y - r * 0.8f + r * 0.55f, r * 2.2f, r * 1.6f);
    }
    if (speed > 0.02f && view.theme != 2)
    {
        const float len = r * juce::jmin (5.0f, speed * 40.0f);
        const float bx = pos.x - std::cos (heading) * len, by = pos.y - std::sin (heading) * len;
        juce::Path wake;
        wake.startNewSubPath (pos);
        wake.lineTo (bx, by);
        g.setGradientFill (juce::ColourGradient (base.withAlpha (0.18f), pos.x, pos.y, base.withAlpha (0.0f), bx, by, false));
        g.strokePath (wake, juce::PathStrokeType (r * 0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Path p, details;
    switch (view.theme * 3 + (int) c.role)
    {
        case 0:   // garden grazer: beetle
        {
            juce::Path legs;
            for (int k = 0; k < 3; ++k)
            {
                const float lx = -0.5f + 0.5f * (float) k, sw = 0.25f * std::sin (phase + (float) k * 2.0f);
                legs.startNewSubPath (lx, -0.6f); legs.lineTo (lx + sw, -1.15f);
                legs.startNewSubPath (lx, 0.6f);  legs.lineTo (lx - sw, 1.15f);
            }
            g.setColour (dark); g.strokePath (legs, juce::PathStrokeType (r * 0.1f), tf);
        }
            p.addEllipse (-1.0f, -0.75f, 1.9f, 1.5f);
            p.addEllipse (0.65f, -0.4f, 0.7f, 0.8f);
            g.setColour (body); g.fillPath (p, tf);
            details.startNewSubPath (-1.0f, 0.0f); details.lineTo (0.75f, 0.0f);
            g.setColour (dark); g.strokePath (details, juce::PathStrokeType (r * 0.12f), tf);
            details.clear();
            details.addEllipse (-0.55f, -0.5f, 0.35f, 0.3f); details.addEllipse (-0.55f, 0.2f, 0.35f, 0.3f);
            details.addEllipse (0.05f, -0.45f, 0.25f, 0.22f); details.addEllipse (0.05f, 0.23f, 0.25f, 0.22f);
            g.fillPath (details, tf);
            break;
        case 1:   // garden hunter: frog
        {
            const float kick = 0.25f * std::sin (phase * 0.5f);
            details.addEllipse (-1.35f - kick, -1.05f, 0.8f, 0.45f); details.addEllipse (-1.35f - kick, 0.6f, 0.8f, 0.45f);
            g.setColour (body.darker (0.25f)); g.fillPath (details, tf);
            p.addEllipse (-1.0f, -0.85f, 2.0f, 1.7f);
            g.setColour (body); g.fillPath (p, tf);
            juce::Path eyes, pupils;
            eyes.addEllipse (0.3f, -0.85f, 0.55f, 0.55f); eyes.addEllipse (0.3f, 0.3f, 0.55f, 0.55f);
            pupils.addEllipse (0.5f, -0.72f, 0.25f, 0.28f); pupils.addEllipse (0.5f, 0.43f, 0.25f, 0.28f);
            g.setColour (juce::Colours::white.withAlpha (vitality)); g.fillPath (eyes, tf);
            g.setColour (juce::Colours::black.withAlpha (vitality)); g.fillPath (pupils, tf);
            break;
        }
        case 2:   // garden drifter: firefly
        {
            glow (r * (2.4f + 1.2f * std::sin (phase * 0.3f)), 0.25f + 0.45f * flash);
            const float flap = 0.35f * std::sin (phase * 2.0f);
            details.addEllipse (-0.6f, -1.1f - flap, 0.9f, 0.9f); details.addEllipse (-0.6f, 0.2f + flap, 0.9f, 0.9f);
            g.setColour (juce::Colours::white.withAlpha (0.35f * vitality)); g.fillPath (details, tf);
            p.addEllipse (-0.7f, -0.35f, 1.5f, 0.7f);
            g.setColour (body); g.fillPath (p, tf);
            break;
        }
        case 3:   // ocean grazer: fish
        {
            const float wag = 0.35f * std::sin (phase);
            p.addEllipse (-0.9f, -0.5f, 1.9f, 1.0f);
            p.addTriangle (-0.75f, 0.0f, -1.5f, -0.6f + wag, -1.5f, 0.6f + wag);
            g.setColour (body); g.fillPath (p, tf);
            details.addEllipse (0.45f, -0.25f, 0.22f, 0.22f);
            g.setColour (dark); g.fillPath (details, tf);
            break;
        }
        case 4:   // ocean hunter: shark (from above)
        {
            const float wag = 0.25f * std::sin (phase * 0.7f);
            p.startNewSubPath (1.35f, 0.0f);
            p.quadraticTo (0.4f, -0.5f, -1.0f, -0.18f);
            p.lineTo (-1.6f, -0.55f + wag);
            p.lineTo (-1.35f, 0.0f);
            p.lineTo (-1.6f, 0.55f + wag);
            p.lineTo (-1.0f, 0.18f);
            p.quadraticTo (0.4f, 0.5f, 1.35f, 0.0f);
            p.closeSubPath();
            p.addTriangle (0.2f, -0.3f, -0.35f, -1.0f, -0.3f, -0.25f);
            p.addTriangle (0.2f, 0.3f, -0.35f, 1.0f, -0.3f, 0.25f);
            g.setColour (body); g.fillPath (p, tf);
            details.addEllipse (0.75f, -0.28f, 0.16f, 0.16f); details.addEllipse (0.75f, 0.12f, 0.16f, 0.16f);
            g.setColour (dark); g.fillPath (details, tf);
            break;
        }
        case 5:   // ocean drifter: jellyfish (always upright)
        {
            glow (r * 2.4f, 0.18f + 0.4f * flash);
            const float pulse = 1.0f + 0.12f * std::sin (phase * 0.35f);
            p.addCentredArc (0.0f, 0.0f, 1.1f * pulse, 1.0f / pulse, 0.0f, -1.5708f, 1.5708f, true);
            p.closeSubPath();
            g.setColour (body.withMultipliedAlpha (0.8f)); g.fillPath (p, upright);
            for (int k = 0; k < 4; ++k)
            {
                const float x = -0.7f + 0.47f * (float) k;
                details.startNewSubPath (x, 0.05f);
                details.quadraticTo (x + 0.35f * std::sin (phase * 0.4f + (float) k), 1.0f, x + 0.2f * std::sin (phase * 0.4f + (float) k + 1.0f), 1.9f);
            }
            g.setColour (body.withMultipliedAlpha (0.6f));
            g.strokePath (details, juce::PathStrokeType (r * 0.12f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), upright);
            break;
        }
        case 6:   // space grazer: saucer
        {
            const auto tilt = juce::AffineTransform::scale (r).rotated (juce::jlimit (-0.4f, 0.4f, c.vx * 6.0f)).translated (pos);
            details.addEllipse (-0.55f, -0.85f, 1.1f, 0.9f);
            g.setColour (juce::Colour (0xa0c8f0ff)); g.fillPath (details, tilt);
            p.addEllipse (-1.25f, -0.35f, 2.5f, 0.75f);
            g.setColour (body); g.fillPath (p, tilt);
            juce::Path lights;
            for (int k = 0; k < 3; ++k)
                if (std::fmod (ft * 3.0f + (float) k * 0.33f, 1.0f) < 0.5f) lights.addEllipse (-0.75f + 0.6f * (float) k, -0.05f, 0.25f, 0.25f);
            g.setColour (juce::Colour (0xfffff3a0)); g.fillPath (lights, tilt);
            break;
        }
        case 7:   // space hunter: comet with a tail
        {
            const float tail = r * (3.0f + 6.0f * juce::jmin (1.0f, speed * 8.0f));
            const float bx = pos.x - std::cos (heading) * tail, by = pos.y - std::sin (heading) * tail;
            juce::Path tp;
            const float nx = -std::sin (heading) * r * 0.9f, ny = std::cos (heading) * r * 0.9f;
            tp.startNewSubPath (pos.x + nx, pos.y + ny);
            tp.lineTo (bx, by);
            tp.lineTo (pos.x - nx, pos.y - ny);
            tp.closeSubPath();
            g.setGradientFill (juce::ColourGradient (base.withAlpha (0.7f * vitality), pos.x, pos.y, base.withAlpha (0.0f), bx, by, false));
            g.fillPath (tp);
            glow (r * 2.2f, 0.4f + 0.4f * flash);
            g.setColour (body.interpolatedWith (juce::Colours::white, 0.5f));
            g.fillEllipse (pos.x - r * 0.8f, pos.y - r * 0.8f, r * 1.6f, r * 1.6f);
            break;
        }
        case 8:   // space drifter: star
        {
            glow (r * (2.6f + 0.6f * std::sin (phase * 0.25f)), 0.3f + 0.5f * flash);
            p.addStar ({}, 5, 0.5f, 1.35f, ft * 0.4f + (float) c.id);
            g.setColour (body); g.fillPath (p, upright);
            break;
        }
        case 9:   // cells grazer: round cell with nucleus
        {
            p.addEllipse (-1.0f, -1.0f, 2.0f, 2.0f);
            g.setColour (body.withMultipliedAlpha (0.4f)); g.fillPath (p, tf);
            g.setColour (body); g.strokePath (p, juce::PathStrokeType (r * 0.13f), tf);
            details.addEllipse (0.0f, -0.35f, 0.7f, 0.7f);
            g.setColour (dark.brighter (0.3f)); g.fillPath (details, tf);
            break;
        }
        case 10:  // cells hunter: amoeba reaching forward
        {
            for (int k = 0; k <= 28; ++k)
            {
                const float th = twoPi * (float) k / 28.0f;
                const float rad = 1.0f + 0.16f * std::sin (3.0f * th + phase * 0.3f) + 0.35f * juce::jmax (0.0f, std::cos (th));
                const float x = std::cos (th) * rad, y = std::sin (th) * rad;
                if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            p.closeSubPath();
            p = p.createPathWithRoundedCorners (0.3f);
            g.setColour (body.withMultipliedAlpha (0.55f)); g.fillPath (p, tf);
            g.setColour (body); g.strokePath (p, juce::PathStrokeType (r * 0.1f), tf);
            details.addEllipse (-0.4f, -0.3f, 0.6f, 0.6f);
            g.setColour (dark); g.fillPath (details, tf);
            break;
        }
        default:  // cells drifter: diatom wheel
        {
            glow (r * 2.0f, 0.15f + 0.4f * flash);
            const auto spin = juce::AffineTransform::scale (r).rotated (ft * 0.6f + (float) c.id).translated (pos);
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
    }

    if (flash > 0.05f)
    {
        // a little note floats up from the singer
        const float rise = (1.0f - flash) * r * 5.0f;
        const float nx = pos.x + r * 1.2f, ny = pos.y - r * 1.6f - rise;
        const float ns = juce::jmax (3.0f, r * 0.55f);
        juce::Path note;
        note.addEllipse (nx - ns, ny - ns * 0.7f, ns * 2.0f, ns * 1.4f);
        note.addRectangle (nx + ns * 0.75f, ny - ns * 3.2f, ns * 0.35f, ns * 3.2f);
        note.addTriangle (nx + ns * 1.1f, ny - ns * 3.2f, nx + ns * 2.3f, ny - ns * 2.2f, nx + ns * 1.1f, ny - ns * 2.4f);
        g.setColour (base.brighter (0.8f).withAlpha (juce::jmin (1.0f, flash * 1.4f)));
        g.fillPath (note);
    }

    if (c.favourite)
    {
        juce::Path heart;
        const float hs = juce::jmax (4.0f, r * 0.55f);
        const float hx = pos.x + r * 0.9f, hy = pos.y - r * 1.5f;
        heart.startNewSubPath (hx, hy + hs * 0.9f);
        heart.cubicTo (hx - hs * 1.3f, hy, hx - hs * 0.5f, hy - hs * 0.9f, hx, hy - hs * 0.2f);
        heart.cubicTo (hx + hs * 0.5f, hy - hs * 0.9f, hx + hs * 1.3f, hy, hx, hy + hs * 0.9f);
        g.setColour (juce::Colour (0xffff7a9a));
        g.fillPath (heart);
    }
    if (c.id == view.selected)
    {
        const float rr = r * 2.0f + 3.0f;
        juce::Path ring, dashed;
        ring.addEllipse (pos.x - rr, pos.y - rr, rr * 2.0f, rr * 2.0f);
        const float dash[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.6f).createDashedStroke (dashed, ring, dash, 2, juce::AffineTransform::rotation (ft, pos.x, pos.y));
        g.setColour (view.pairing ? juce::Colour (0xffff9ac0) : juce::Colours::white);
        g.fillPath (dashed);
    }
}

void WorldView::drawEffects (juce::Graphics& g, double time)
{
    for (const auto& f : fx)
    {
        const float age = (float) (time - f.t0) / 1.2f;
        if (age < 0.0f || age > 1.0f) continue;
        const auto pos = toScreen (f.x, f.y);
        const auto col = LifeViewState::roleColour (view.theme, f.role);
        if (f.kind == 0)   // born: sparkles flying out
        {
            for (int k = 0; k < 6; ++k)
            {
                const float th = twoPi * (float) k / 6.0f + 0.3f;
                const float d = 4.0f + age * 18.0f;
                g.setColour (col.brighter (0.5f).withAlpha (1.0f - age));
                g.fillEllipse (pos.x + std::cos (th) * d - 1.5f, pos.y + std::sin (th) * d - 1.5f, 3.0f, 3.0f);
            }
        }
        else               // died / eaten: a fading ghost ring
        {
            const float rr = 4.0f + age * 10.0f;
            g.setColour ((f.kind == 2 ? juce::Colour (0xffff8070) : C::textDim).withAlpha (0.6f * (1.0f - age)));
            g.drawEllipse (pos.x - rr, pos.y - rr, rr * 2.0f, rr * 2.0f, 1.2f);
        }
    }
}

void WorldView::drawGesture (juce::Graphics& g) const
{
    const float aw = area().getWidth();
    if (dragging)
    {
        const auto p1 = toScreen (downW.x, downW.y), p2 = toScreen (curW.x, curW.y);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        switch (view.tool)
        {
            case GardenTool::Wall:
                g.drawLine ({ p1, p2 }, juce::jmax (4.0f, aw * 0.009f));
                break;
            case GardenTool::Warm:
            case GardenTool::Cold:
            {
                const float r = juce::jlimit (0.06f, 0.4f, downW.getDistanceFrom (curW)) * aw;
                g.setColour ((view.tool == GardenTool::Warm ? juce::Colour (0xffffa050) : juce::Colour (0xff70b0ff)).withAlpha (0.6f));
                g.drawEllipse (p1.x - r, p1.y - r, 2.0f * r, 2.0f * r, 1.5f);
                break;
            }
            case GardenTool::Current:
            {
                juce::Path arrow;
                arrow.addArrow ({ p1, p2 }, 3.0f, 14.0f, 12.0f);
                g.setColour (juce::Colour (0xff8ee0f0).withAlpha (0.8f));
                g.fillPath (arrow);
                const float r = 0.15f * aw;
                g.drawEllipse (p1.x - r, p1.y - r, 2.0f * r, 2.0f * r, 1.0f);
                break;
            }
            default: break;
        }
    }
    else if (hovering && view.valid)
    {
        const auto p = toScreen (hoverW.x, hoverW.y);
        if (view.tool == GardenTool::Remove || view.tool == GardenTool::Erase)
        {
            const float r = (view.tool == GardenTool::Remove ? 0.04f : 0.05f) * aw;
            g.setColour (juce::Colour (0x60ff8080));
            g.drawEllipse (p.x - r, p.y - r, 2.0f * r, 2.0f * r, 1.2f);
        }
        else if (view.tool == GardenTool::Inspect)
        {
            if (const auto id = creatureAt (view.world, hoverW, 0.05f))
                for (const auto& c : view.world.creatures)
                    if (c.alive && c.id == id)
                    {
                        const auto cp = toScreen (c.x, c.y);
                        const float r = creatureRadius (c) * 2.0f + 2.0f;
                        g.setColour (juce::Colours::white.withAlpha (0.35f));
                        g.drawEllipse (cp.x - r, cp.y - r, 2.0f * r, 2.0f * r, 1.0f);
                    }
        }
    }
}

void WorldView::drawHud (juce::Graphics& g, juce::Rectangle<float> a, double time) const
{
    auto top = a.reduced (10.0f, 8.0f).removeFromTop (16.0f);
    g.setFont (cat::catFont (11.0f));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    juce::String left = toolName (view.tool, view.theme) + ": " + toolHint (view.tool);
    if (view.pairing) left = "Pick a partner for #" + juce::String (view.selected) + " (right-click to cancel)";
    g.drawText (left, top, juce::Justification::centredLeft);

    if (! view.valid)
    {
        g.setFont (cat::catFont (14.0f));
        g.setColour (C::textDim);
        g.drawText ("Waking up the world...", a, juce::Justification::centred);
        return;
    }
    if (! view.running)
    {
        g.setFont (cat::catFont (13.0f, true));
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawText ("Life paused", top, juce::Justification::centredRight);
    }
    else if (! view.singing)
    {
        g.setFont (cat::catFont (11.0f));
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawText ("Silent: press Play (or turn on Play When Stopped)", top, juce::Justification::centredRight);
    }
    if (view.message.isNotEmpty() && time - view.messageTime < 2.5)
    {
        const float alpha = (float) juce::jlimit (0.0, 1.0, 2.5 - (time - view.messageTime));
        auto box = a.withSizeKeepingCentre (260.0f, 30.0f).withBottomY (a.getBottom() - 14.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f * alpha));
        g.fillRoundedRectangle (box, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.setFont (cat::catFont (13.0f, true));
        g.drawText (view.message, box, juce::Justification::centred);
    }
}

//------------------------------------------------------------------------------
void WorldView::mouseDown (const juce::MouseEvent& e)
{
    const auto w = toWorld (e.position);
    downW = curW = w;
    dragging = false;
    using Cmd = LifeCatProcessor::Command;

    // right-click: inspect with any tool (and cancel pairing)
    if (e.mods.isPopupMenu())
    {
        view.pairing = false;
        view.selected = creatureAt (view.world, w, 0.05f);
        if (onSelectionChanged) onSelectionChanged();
        repaint();
        return;
    }

    Cmd c;
    c.x = w.x; c.y = w.y;
    switch (view.tool)
    {
        case GardenTool::Inspect:
        {
            const auto id = creatureAt (view.world, w, 0.05f);
            if (view.pairing && id != 0 && id != view.selected)
            {
                c.type = Cmd::Breed; c.id = view.selected; c.id2 = id;
                processor.send (c);
                view.pairing = false;
                view.say ("A baby is born!");
            }
            else
            {
                view.pairing = false;
                view.selected = id;
            }
            if (onSelectionChanged) onSelectionChanged();
            break;
        }
        case GardenTool::Food:
            c.type = Cmd::Food; c.r = 6.0f; processor.send (c); lastDrop = now();
            break;
        case GardenTool::Grazer:
        case GardenTool::Hunter:
        case GardenTool::Drifter:
            c.type = Cmd::Add; c.role = (int) view.tool - (int) GardenTool::Grazer; processor.send (c);
            break;
        case GardenTool::Remove:
            c.type = Cmd::Remove; c.r = 0.04f; processor.send (c); lastDrop = now();
            break;
        case GardenTool::Erase:
            c.type = Cmd::EraseZone; c.r = 0.05f; processor.send (c);
            break;
        case GardenTool::Wall:
        case GardenTool::Warm:
        case GardenTool::Cold:
        case GardenTool::Current:
            dragging = true;
            break;
    }
    repaint();
}

void WorldView::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    curW = toWorld (e.position);
    hoverW = curW;
    using Cmd = LifeCatProcessor::Command;
    const double t = now();
    if (view.tool == GardenTool::Food && t - lastDrop > 0.07)
    {
        Cmd c; c.type = Cmd::Food; c.x = curW.x; c.y = curW.y; c.r = 2.0f;
        processor.send (c);
        lastDrop = t;
    }
    else if (view.tool == GardenTool::Remove && t - lastDrop > 0.05)
    {
        Cmd c; c.type = Cmd::Remove; c.x = curW.x; c.y = curW.y; c.r = 0.04f;
        processor.send (c);
        lastDrop = t;
    }
    repaint();
}

void WorldView::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging) return;
    dragging = false;
    curW = toWorld (e.position);
    using Cmd = LifeCatProcessor::Command;
    Cmd c;
    c.x = downW.x; c.y = downW.y;
    const float dist = downW.getDistanceFrom (curW);
    switch (view.tool)
    {
        case GardenTool::Wall:
            if (dist > 0.02f)
            {
                c.type = Cmd::Wall; c.x2 = curW.x; c.y2 = curW.y;
                processor.send (c);
                if (view.world.numWalls >= maxWalls) view.say ("No room for more walls");
            }
            break;
        case GardenTool::Warm:
        case GardenTool::Cold:
            c.type = Cmd::Zone; c.zone = view.tool == GardenTool::Warm ? 0 : 1;
            c.r = dist < 0.02f ? 0.12f : juce::jlimit (0.06f, 0.4f, dist);
            processor.send (c);
            if (view.world.numZones >= maxZones) view.say ("No room for more zones");
            break;
        case GardenTool::Current:
        {
            c.type = Cmd::Zone; c.zone = 2; c.r = 0.15f;
            const float dx = curW.x - downW.x, dy = curW.y - downW.y;
            if (dist > 0.02f) { c.x2 = dx / dist; c.y2 = dy / dist; } else { c.x2 = 1.0f; c.y2 = 0.0f; }
            processor.send (c);
            break;
        }
        default: break;
    }
    repaint();
}

void WorldView::mouseMove (const juce::MouseEvent& e) { hovering = true; hoverW = toWorld (e.position); }
void WorldView::mouseExit (const juce::MouseEvent&)   { hovering = false; }

//==============================================================================
Inspector::Inspector (LifeCatProcessor& p, LifeViewState& v) : processor (p), view (v)
{
    using Cmd = LifeCatProcessor::Command;
    listen.setTooltip ("Hear this creature sing one note");
    favourite.setTooltip ("Favourites never die of old age and nearly always escape hunters");
    cloneButton.setTooltip ("An exact copy (same genes, same sound)");
    pair.setTooltip ("Breed: then click a partner. The baby blends both parents' genes plus a little mutation.");
    remove.setTooltip ("Take this creature out of the world");
    favourite.setClickingTogglesState (false);

    listen.onClick = [this] { Cmd c; c.type = Cmd::Listen; c.id = view.selected; processor.send (c); };
    favourite.onClick = [this]
    {
        Cmd c; c.type = Cmd::Favourite; c.id = view.selected; c.flag = ! shown.favourite; processor.send (c);
        view.say (c.flag ? "Favourite - it will live on" : "No longer a favourite");
    };
    cloneButton.onClick = [this] { Cmd c; c.type = Cmd::Clone; c.id = view.selected; processor.send (c); view.say ("Cloned!"); };
    pair.onClick = [this] { view.pairing = ! view.pairing; view.tool = GardenTool::Inspect; };
    remove.onClick = [this]
    {
        Cmd c; c.type = Cmd::RemoveId; c.id = view.selected; processor.send (c);
        view.selected = 0; view.pairing = false; refresh();
    };
    for (auto* b : { &listen, &favourite, &cloneButton, &pair, &remove }) addAndMakeVisible (b);
    refresh();
}

void Inspector::refresh()
{
    found = false;
    if (view.selected != 0)
        for (const auto& c : view.world.creatures)
            if (c.alive && c.id == view.selected) { shown = c; found = true; break; }
    for (auto* b : { &listen, &favourite, &cloneButton, &pair, &remove }) b->setEnabled (found);
    favourite.setButtonText (found && shown.favourite ? "Unfavourite" : "Favourite");
    pair.setButtonText (view.pairing ? "Cancel" : "Pair...");
    repaint();
}

void Inspector::resized()
{
    auto r = getLocalBounds().reduced (8).removeFromBottom (26);
    const int w = r.getWidth() / 5;
    for (auto* b : { &listen, &favourite, &cloneButton, &pair, &remove }) b->setBounds (r.removeFromLeft (w).reduced (2, 0));
}

void Inspector::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (C::panel);
    g.fillRoundedRectangle (b, 8.0f);
    auto r = b.reduced (10.0f, 8.0f);
    r.removeFromBottom (32.0f);

    if (view.selected == 0 || ! found)
    {
        g.setColour (C::textDim);
        g.setFont (cat::catFont (13.0f));
        const juce::String text = view.selected == 0
            ? "Click a creature (Inspect tool, or right-click) to see its genes.\n\nBody genes shape how it lives, sound genes shape how it sings. "
              "Babies blend their parents' genes and mutate a little, so the music evolves."
            : "#" + juce::String (view.selected) + " is gone - it died or was eaten.";
        g.drawFittedText (text, r.toNearestInt(), juce::Justification::centred, 6);
        return;
    }

    const auto& c = shown;
    const auto col = LifeViewState::creatureColour (view.theme, c);
    auto head = r.removeFromTop (20.0f);
    g.setColour (col);
    g.fillEllipse (head.removeFromLeft (14.0f).withSizeKeepingCentre (12.0f, 12.0f));
    head.removeFromLeft (6.0f);
    g.setColour (C::text);
    g.setFont (cat::catFont (14.0f, true));
    g.drawText (LifeViewState::roleName (view.theme, c.role) + "  #" + juce::String (c.id), head, juce::Justification::centredLeft);
    g.setFont (cat::catFont (11.0f));
    g.setColour (C::textDim);
    juce::String info = "generation " + juce::String (c.generation) + "   age " + juce::String (juce::roundToInt (c.age)) + " s";
    if (c.parentA != 0) info << "   parents #" << (int) c.parentA << (c.parentB != 0 ? " x #" + juce::String (c.parentB) : juce::String (" (clone)"));
    g.drawText (info, head, juce::Justification::centredRight);

    auto en = r.removeFromTop (16.0f);
    g.drawText ("Energy", en.removeFromLeft (64.0f), juce::Justification::centredLeft);
    auto bar = en.reduced (0.0f, 4.0f);
    g.setColour (C::raised);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (c.energy < 0.25f ? C::red : C::green);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, c.energy)), 3.0f);
    r.removeFromTop (4.0f);

    // gene bars: life & rhythm (left column), sound (right)
    const auto names = geneNames();
    static const int leftGenes[] = { gSpeed, gSize, gSight, gFertility, gLifespan, gHue, gPitch, gRhythm, gDensity };
    static const int rightGenes[] = { gTimbre, gBright, gHarm, gAttack, gRelease, gRegister, gVibrato, gShimmer, gWobble, gPan };
    const float colW = (r.getWidth() - 14.0f) / 2.0f;
    const float rowH = juce::jmin (14.0f, r.getHeight() / 11.0f);
    auto column = [&] (const int* genes, int count, float x, const juce::String& title, juce::Colour barColour)
    {
        g.setFont (cat::catFont (10.0f, true));
        g.setColour (C::text.withAlpha (0.7f));
        g.drawText (title, juce::Rectangle<float> (x, r.getY(), colW, rowH), juce::Justification::centredLeft);
        g.setFont (cat::catFont (10.5f));
        for (int k = 0; k < count; ++k)
        {
            const int gi = genes[k];
            auto cell = juce::Rectangle<float> (x, r.getY() + (float) (k + 1) * rowH, colW, rowH);
            g.setColour (C::textDim);
            g.drawText (names[gi], cell.removeFromLeft (66.0f), juce::Justification::centredLeft);
            auto gb = cell.reduced (0.0f, rowH * 0.3f);
            g.setColour (C::raised);
            g.fillRoundedRectangle (gb, 2.0f);
            g.setColour (barColour.withAlpha (0.9f));
            g.fillRoundedRectangle (gb.withWidth (juce::jmax (2.0f, gb.getWidth() * juce::jlimit (0.0f, 1.0f, c.genes[gi]))), 2.0f);
        }
    };
    column (leftGenes, 9, r.getX(), "LIFE & RHYTHM", C::slate);
    column (rightGenes, 10, r.getX() + colW + 14.0f, "SOUND", col);
}

//==============================================================================
void PopulationGraph::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (C::panel);
    g.fillRoundedRectangle (b, 8.0f);
    auto r = b.reduced (10.0f, 6.0f);
    const auto& w = view.world;

    auto stats = r.removeFromTop (16.0f);
    g.setFont (cat::catFont (11.0f));
    float x = stats.getX();
    for (int k = 0; k < numRoles; ++k)
    {
        const auto role = (Role) k;
        const juce::String s = juce::String (view.valid ? w.count (role) : 0) + " " + LifeViewState::roleName (view.theme, role, true);
        g.setColour (LifeViewState::roleColour (view.theme, role));
        g.fillEllipse (x, stats.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (C::text);
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) + 16.0f;
        g.drawText (s, juce::Rectangle<float> (x + 9.0f, stats.getY(), tw, stats.getHeight()), juce::Justification::centredLeft);
        x += tw + 6.0f;
    }
    g.setColour (C::textDim);
    g.drawText ("gen " + juce::String (w.maxGeneration) + "   born " + juce::String (w.births) + "   gone " + juce::String (w.deaths),
                stats, juce::Justification::centredRight);

    r.removeFromTop (4.0f);
    g.setColour (C::background);
    g.fillRoundedRectangle (r, 4.0f);
    if (! view.valid) return;

    int peak = 8;
    for (const auto& h : w.history) peak = juce::jmax (peak, (int) h[0], (int) h[1], (int) h[2]);
    const auto plot = r.reduced (4.0f, 4.0f);
    for (int k = 0; k < numRoles; ++k)
    {
        juce::Path line;
        for (int i = 0; i < historyLength; ++i)
        {
            const int idx = (w.historyPos + i) % historyLength;
            const float v = (float) w.history[(size_t) idx][(size_t) k] / (float) peak;
            const float px = plot.getX() + plot.getWidth() * (float) i / (float) (historyLength - 1);
            const float py = plot.getBottom() - v * plot.getHeight();
            if (i == 0) line.startNewSubPath (px, py); else line.lineTo (px, py);
        }
        g.setColour (LifeViewState::roleColour (view.theme, (Role) k).withAlpha (0.9f));
        g.strokePath (line, juce::PathStrokeType (1.5f));
    }
    g.setColour (C::textDim.withAlpha (0.6f));
    g.setFont (cat::catFont (9.5f));
    g.drawText ("last 8 min", plot, juce::Justification::topRight);
}

//==============================================================================
LifeCatEditor::LifeCatEditor (LifeCatProcessor& p)
    : processor (p),
      presetBar (p.presetManager),
      theme (p.apvts, "theme", "World", &p.midiLearn, 44),
      running (p.apvts, "running", "Life", &p.midiLearn),
      surpriseControls (p, p.surpriseUndo, [&p] { p.surprise(); }),
      worldView (p, view),
      inspector (p, view),
      graph (view),
      pitchMode (p.apvts, "pitchMode", "Notes", &p.midiLearn, 48),
      key (p.apvts, "key", "Key", &p.midiLearn, 34),
      scale (p.apvts, "scale", "Scale", &p.midiLearn, 40),
      grid (p.apvts, "grid", "Grid", &p.midiLearn, 34),
      quantize (p.apvts, "quantize", "On the Beat", &p.midiLearn),
      followGroove (p.apvts, "followGroove", "Follow Drum", &p.midiLearn),
      playStopped (p.apvts, "playStopped", "Play When Stopped", &p.midiLearn),
      swing (p.apvts, "swing", "Swing", &p.midiLearn, 48),
      density (p.apvts, "density", "Density", &p.midiLearn, 56),
      noteLength (p.apvts, "noteLength", "Length", &p.midiLearn, 50),
      lifeSpeed (p.apvts, "lifeSpeed", "Speed", &p.midiLearn, 50),
      foodGrowth (p.apvts, "foodGrowth", "Food", &p.midiLearn, 44),
      mutation (p.apvts, "mutation", "Mutation", &p.midiLearn, 62),
      hunger (p.apvts, "metabolism", "Hunger", &p.midiLearn, 52),
      breeding (p.apvts, "breeding", "Breeding", &p.midiLearn, 62),
      lifespan (p.apvts, "lifespan", "Lifespan", &p.midiLearn, 60),
      immigration (p.apvts, "immigration", "Newcomers", &p.midiLearn),
      ownSound (p.apvts, "sound", "Own Sound", &p.midiLearn),
      brightness (p.apvts, "brightness", "Bright", &p.midiLearn, 50),
      attack (p.apvts, "attack", "Attack", &p.midiLearn, 50),
      release (p.apvts, "release", "Release", &p.midiLearn, 56),
      reverbMix (p.apvts, "reverbMix", "Reverb", &p.midiLearn, 54),
      reverbSize (p.apvts, "reverbSize", "Size", &p.midiLearn, 40),
      delayMix (p.apvts, "delayMix", "Delay", &p.midiLearn, 48),
      delayFb (p.apvts, "delayFb", "Feedback", &p.midiLearn, 64),
      width (p.apvts, "width", "Width", &p.midiLearn, 48),
      volume (p.apvts, "volume", "Volume", &p.midiLearn, 56),
      delayTime (p.apvts, "delayTime", "Time", &p.midiLearn, 40),
      midiOut (p.apvts, "midiOut", "MIDI Out", &p.midiLearn),
      midiChannels (p.apvts, "midiChannels", "", &p.midiLearn, 0),
      keyboard (p.noteMonitor, 24, 108),
      catView (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); }, cat::CatStyle::Life),
      bootScreen (p.noteMonitor, [&p] { return p.uiPpq.load(); }, [&p] { return p.uiTransportRunning.load(); },
                  "LIFE", "-cat", "A TINY ECOSYSTEM THAT MAKES MUSIC", cat::CatStyle::Life)
{
    setLookAndFeel (&lnf);

    // header
    theme.getCombo().setTooltip ("Garden pond, Ocean, Space or Cells: changes the look AND the sound");
    running.setTooltip ("Life on / off (freezes the world; nothing sings while paused)");
    newWorldButton.setTooltip ("A fresh world of the same kind (new random seed)");
    newWorldButton.onClick = [this] { newWorld(); };
    dragButton.setTooltip ("Drag the last 8 bars the creatures sang onto a Cubase MIDI track");
    dragButton.onDragOut = [this]
    {
        const auto seq = processor.takeRecording();
        if (seq.getNumEvents() == 0) { view.say ("Nothing sung yet"); return; }
        cat::midiexport::dragFileOut (cat::midiexport::writeMidiFile (seq, processor.getHostBpm(), "Life-Cat"), this);
    };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &theme, &running, &newWorldButton, &surpriseControls, &dragButton })
        addAndMakeVisible (c);

    // tools
    toolsLabel.setText ("GARDENER", juce::dontSendNotification);
    toolsLabel.setFont (cat::catFont (11.0f, true));
    toolsLabel.setColour (juce::Label::textColourId, C::textDim);
    toolsLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (toolsLabel);
    for (int i = 0; i < numGardenTools; ++i)
    {
        auto* b = toolButtons.add (new juce::TextButton());
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4711);
        b->setTooltip (toolTips[i]);
        b->onClick = [this, i] { if (toolButtons[i]->getToggleState()) setTool ((GardenTool) i); };
        addAndMakeVisible (b);
    }
    toolButtons[0]->setToggleState (true, juce::dontSendNotification);
    updateToolNames();

    worldView.onSelectionChanged = [this] { inspector.refresh(); };
    addAndMakeVisible (worldView);
    addAndMakeVisible (inspector);
    addAndMakeVisible (graph);

    // tabs
    const char* tabNames[] = { "MUSIC", "LIFE", "SOUND", "MIDI" };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = tabButtons.add (new juce::TextButton (tabNames[i]));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4712);
        b->onClick = [this, i] { showTab (i); };
        addAndMakeVisible (b);
    }

    lowNote = cat::Stepper::forParameter (p.apvts, "lowNote", "Low", [] (int v) { return cat::cubaseNoteName (v); });
    highNote = cat::Stepper::forParameter (p.apvts, "highNote", "High", [] (int v) { return cat::cubaseNoteName (v); });
    maxNotes = cat::Stepper::forParameter (p.apvts, "maxNotes", "Max Notes");
    maxPop = cat::Stepper::forParameter (p.apvts, "maxPop", "Max Creatures");
    lowNote->setTooltip ("Lowest note the creatures may sing");
    highNote->setTooltip ("Highest note the creatures may sing");
    maxNotes->setTooltip ("How many creatures may sing at the same grid step");
    maxPop->setTooltip ("The world never holds more creatures than this");

    pitchMode.getCombo().setTooltip ("Position: higher in the world = higher note.  Species: chord tones per kind of creature.  "
                                     "Held chord: they sing the notes you hold (or Chord-Cat sends).");
    quantize.setTooltip ("On: creatures sing on the grid (with swing).  Off: each floats on its own clock.");
    followGroove.setTooltip ("Take the swing from Drum-Cat (Cat Link)");
    playStopped.setTooltip ("Keep singing when the host transport is stopped (own clock at the host tempo)");
    density.getSlider().setTooltip ("How often creatures actually sing when their turn comes");
    mutation.getSlider().setTooltip ("How much babies differ from their parents (wild mutation = fast evolution)");
    hunger.getSlider().setTooltip ("How fast creatures burn energy");
    immigration.setTooltip ("When a kind dies out, newcomers arrive from outside");
    clearWalls.onClick = [this] { LifeCatProcessor::Command c; c.type = LifeCatProcessor::Command::ClearZones; processor.send (c); };
    ownSound.setTooltip ("Life-Cat's own voices (sound shaped by each creature's genes). Turn off to only send MIDI.");

    midiInfo.setFont (cat::catFont (12.0f));
    midiInfo.setColour (juce::Label::textColourId, C::textDim);
    midiInfo.setJustificationType (juce::Justification::topLeft);
    midiInfo.setText ("MIDI Out sends every note the creatures sing, so you can play any instrument with the ecosystem "
                      "(in Cubase: route this track's MIDI output to another instrument track).\n\n"
                      "Channels by kind: grazers on 1, hunters on 2, drifters on 3, for three different instruments.\n\n"
                      "Notes you play IN are used by the 'Held chord' note mode. Drag MIDI (top right) exports the last 8 bars.",
                      juce::dontSendNotification);

    tabContent[0] = { &pitchMode, &key, &scale, lowNote.get(), highNote.get(), &quantize, &grid, &swing, &density, maxNotes.get(),
                      &noteLength, &followGroove, &playStopped };
    tabContent[1] = { &lifeSpeed, &foodGrowth, &mutation, &hunger, &breeding, &lifespan, maxPop.get(), &immigration, &clearWalls };
    tabContent[2] = { &ownSound, &brightness, &attack, &release, &reverbMix, &reverbSize, &delayMix, &delayTime, &delayFb, &width, &volume };
    tabContent[3] = { &midiOut, &midiChannels, &midiInfo };
    for (auto& list : tabContent)
        for (auto* c : list) addChildComponent (c);
    tabButtons[0]->setToggleState (true, juce::dontSendNotification);
    showTab (0);

    keyboard.setColourForChannel ([this] (int ch)
    {
        return ch >= 1 && ch <= 3 ? LifeViewState::roleColour (view.theme, (Role) (ch - 1)) : C::accent;
    });
    addAndMakeVisible (keyboard);
    addAndMakeVisible (catView);
    addAndMakeVisible (bootScreen);

    setSize (1180, 770);
    timerCallback();
    startTimerHz (30);
}

LifeCatEditor::~LifeCatEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LifeCatEditor::setTool (GardenTool t)
{
    view.tool = t;
    if (t != GardenTool::Inspect) view.pairing = false;
    worldView.repaint();
}

void LifeCatEditor::showTab (int i)
{
    currentTab = juce::jlimit (0, 3, i);
    for (int k = 0; k < 4; ++k)
        for (auto* c : tabContent[(size_t) k]) c->setVisible (k == currentTab);
    resized();
}

void LifeCatEditor::updateToolNames()
{
    for (int i = 0; i < numGardenTools; ++i) toolButtons[i]->setButtonText (toolName ((GardenTool) i, view.theme));
}

void LifeCatEditor::newWorld()
{
    LifeCatProcessor::Command c;
    c.type = LifeCatProcessor::Command::NewWorld;
    c.seed = (uint32_t) juce::Random::getSystemRandom().nextInt (1 << 30) + 1u;
    const int pi = processor.currentPreset.load();
    const auto& list = LifeCatProcessor::presets();
    if (pi >= 0 && pi < (int) list.size())
    {
        const auto& pr = list[(size_t) pi];
        c.counts[0] = pr.grazers; c.counts[1] = pr.hunters; c.counts[2] = pr.drifters; c.counts[3] = pr.plants;
        c.layout = pr.layout;
    }
    processor.send (c);
    view.selected = 0;
    view.pairing = false;
    view.say ("A new world begins");
}

void LifeCatEditor::timerCallback()
{
    double ppq = 0.0;
    bool singing = false;
    if (processor.getSnapshot (view.world, ppq, singing))
    {
        view.valid = true;
        view.ppq = ppq;
        view.singing = singing;
    }
    view.theme = juce::roundToInt (processor.apvts.getRawParameterValue ("theme")->load());
    view.running = processor.apvts.getRawParameterValue ("running")->load() > 0.5f;
    if (view.theme != lastTheme)
    {
        lastTheme = view.theme;
        updateToolNames();
        keyboard.repaint();
    }
    // the tool buttons follow the view (Pair... switches to Inspect)
    if (! toolButtons[(int) view.tool]->getToggleState())
        toolButtons[(int) view.tool]->setToggleState (true, juce::dontSendNotification);

    worldView.tick();
    worldView.repaint();
    if (++tickCount % 3 == 0)
    {
        inspector.refresh();
        graph.repaint();
    }
}

void LifeCatEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::background);
    cat::CatLookAndFeel::drawLogo (g, juce::Rectangle<float> (14.0f, 8.0f, 160.0f, 32.0f), "LIFE", "-cat");
}

void LifeCatEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto item = [] (juce::Component& c, float w, float h = 30.0f) { return juce::FlexItem (c).withWidth (w).withHeight (h).withMargin ({ 0, 3, 0, 3 }); };
    auto row = [] (juce::Rectangle<int> area, std::initializer_list<juce::FlexItem> items)
    {
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        for (auto& it : items) fb.items.add (it);
        fb.performLayout (area);
    };

    {
        auto top = r.removeFromTop (40);
        auto logo = top.removeFromLeft (150);
        catView.setBounds (logo.removeFromRight (40).expanded (0, 6));
        row (top, { item (presetBar, 320), item (theme, 160), item (running, 56), item (newWorldButton, 92), juce::FlexItem().withFlex (1.0f),
                    item (surpriseControls, 122), item (dragButton, 92) });
    }
    r.removeFromTop (8);
    keyboard.setBounds (r.removeFromBottom (44));
    r.removeFromBottom (8);

    // tools column
    auto tools = r.removeFromLeft (100);
    toolsLabel.setBounds (tools.removeFromTop (18));
    const int th = juce::jmin (36, (tools.getHeight() - 30) / numGardenTools);
    for (int i = 0; i < numGardenTools; ++i)
    {
        if (i == 1 || i == 2 || i == 5 || i == 6 || i == 10) tools.removeFromTop (6);    // groups
        toolButtons[i]->setBounds (tools.removeFromTop (th).reduced (0, 2));
    }
    r.removeFromLeft (8);

    // world (square)
    const int side = juce::jmin (r.getHeight(), r.getWidth() - 400);
    worldView.setBounds (r.removeFromLeft (side).withSizeKeepingCentre (side, side));
    r.removeFromLeft (10);

    // right column
    inspector.setBounds (r.removeFromTop (232));
    r.removeFromTop (8);
    auto tabs = r.removeFromTop (28);
    for (int i = 0; i < 4; ++i) tabButtons[i]->setBounds (tabs.removeFromLeft (tabs.getWidth() / (4 - i)).reduced (2, 0));
    r.removeFromTop (8);
    graph.setBounds (r.removeFromBottom (112));
    r.removeFromBottom (8);

    auto content = r;
    const int half = content.getWidth() / 2;
    auto line = [&content] { auto l = content.removeFromTop (30); content.removeFromTop (5); return l; };
    auto two = [&] (juce::Component& a, juce::Component& b)
    {
        auto l = line();
        a.setBounds (l.removeFromLeft (half).reduced (2, 1));
        b.setBounds (l.reduced (2, 1));
    };
    switch (currentTab)
    {
        case 0:
            pitchMode.setBounds (line().reduced (2, 1));
            two (key, scale);
            two (*lowNote, *highNote);
            two (quantize, grid);
            two (swing, density);
            two (*maxNotes, noteLength);
            two (followGroove, playStopped);
            break;
        case 1:
            two (lifeSpeed, foodGrowth);
            two (mutation, hunger);
            two (breeding, lifespan);
            two (*maxPop, immigration);
            clearWalls.setBounds (line().removeFromLeft (half).reduced (2, 1));
            break;
        case 2:
            two (ownSound, brightness);
            two (attack, release);
            two (reverbMix, reverbSize);
            two (delayMix, delayTime);
            two (delayFb, width);
            volume.setBounds (line().removeFromLeft (half).reduced (2, 1));
            break;
        default:
            two (midiOut, midiChannels);
            midiInfo.setBounds (content.reduced (2));
            break;
    }

    bootScreen.setBounds (getLocalBounds());
}

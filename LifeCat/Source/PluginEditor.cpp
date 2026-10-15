#include "PluginEditor.h"
#include "MidiExport.h"

namespace C = cat::Colours;
using namespace lifecat;

namespace
{
    double now() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }
    constexpr float twoPi = 6.2831853f;

    const char* toolTips[] = {
        "Inspect: click a creature to see (and hear) its genes. Right-click works with every tool. Wheel = zoom, middle-drag = move around.",
        "Food: click or drag to scatter food. Creatures gather where you feed them.",
        "Add: pick an animal below, then click in the world",
        "Remove: click or drag over creatures, plants and remains to take them out",
        "Wall: drag to build a wall. Walls split the world into separate tribes.",
        "Warm zone: click or drag out a circle. Plants grow faster, life is busier.",
        "Cold zone: click or drag out a circle. Life slows down and saves energy.",
        "Current: drag in the direction the water / wind should flow",
        "Paint terrain: pick water, rock, meadow, light (or rub out) below, then paint",
        "Erase: click a wall or zone to remove it",
    };

    juce::String toolName (GardenTool t)
    {
        static const char* n[] = { "Inspect", "Food", "Add", "Remove", "Wall", "Warm", "Cold", "Current", "Paint", "Erase", "Place" };
        return n[juce::jlimit (0, 10, (int) t)];
    }

    juce::String toolHint (GardenTool t, const LifeViewState& v)
    {
        switch (t)
        {
            case GardenTool::Inspect: return "Click a creature to inspect it";
            case GardenTool::Food:    return "Click or drag to drop food";
            case GardenTool::Add:     return "Click to add a " + lifeart::roleName (v.theme, v.addRole);
            case GardenTool::Remove:  return "Click or drag to remove";
            case GardenTool::Wall:    return "Drag to build a wall";
            case GardenTool::Warm:
            case GardenTool::Cold:    return "Click, or drag out the size";
            case GardenTool::Current: return "Drag in the flow direction";
            case GardenTool::Paint:   return v.paintType == Terrain::Ground ? juce::String ("Rub out painted terrain") : "Paint " + lifeart::terrainName (v.theme, v.paintType);
            case GardenTool::Erase:   return "Click a wall or zone to erase it";
            case GardenTool::PlaceLab:return "Click to place your lab " + lifeart::roleName (v.theme, v.labRole);
        }
        return {};
    }

    lifecat::World& scratchWorld()
    {
        static lifecat::World w;          // message thread only: random genes for the lab
        return w;
    }

    const int labLeft[] = { gSpeed, gSize, gSight, gFertility, gLifespan, gHue, gSocial, gNight, gHardy, gSwim, gPattern, gGlow, gRhythm, gDensity };
    const int labRight[] = { gTimbre, gBright, gHarm, gAttack, gRelease, gRegister, gPitch, gVibrato, gShimmer, gWobble, gPan, gInterval, gGlide, gAccent, gEcho };
}

//==============================================================================
void LifeViewState::say (const juce::String& text)
{
    message = text;
    messageTime = now();
}

const Creature* LifeViewState::selectedCreature() const noexcept
{
    if (selected == 0) return nullptr;
    for (const auto& c : world.creatures) if (c.alive && c.id == selected) return &c;
    return nullptr;
}

//==============================================================================
WorldView::WorldView (LifeCatProcessor& p, LifeViewState& v) : processor (p), view (v)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

juce::Rectangle<float> WorldView::area() const { return getLocalBounds().toFloat().reduced (2.0f); }
float WorldView::unitPixels() const { const auto a = area(); return juce::jmin (a.getWidth(), a.getHeight()) * view.zoom; }

juce::Point<float> WorldView::toWorld (juce::Point<float> s) const
{
    const auto a = area();
    const float x = ((s.x - a.getX()) / a.getWidth() - 0.5f) / view.zoom + view.camX;
    const float y = ((s.y - a.getY()) / a.getHeight() - 0.5f) / view.zoom + view.camY;
    return { juce::jlimit (0.0f, 1.0f, x), juce::jlimit (0.0f, 1.0f, y) };
}

juce::Point<float> WorldView::toScreen (float x, float y) const
{
    const auto a = area();
    return { a.getX() + ((x - view.camX) * view.zoom + 0.5f) * a.getWidth(), a.getY() + ((y - view.camY) * view.zoom + 0.5f) * a.getHeight() };
}

float WorldView::creatureRadius (const Creature& c) const
{
    return juce::jmax (3.5f, World::radiusOf (c) * unitPixels() * 0.95f * lifeart::maturity (c));
}

void WorldView::clampCamera()
{
    view.zoom = juce::jlimit (1.0f, 6.0f, view.zoom);
    const float h = 0.5f / view.zoom;
    view.camX = juce::jlimit (h, 1.0f - h, view.camX);
    view.camY = juce::jlimit (h, 1.0f - h, view.camY);
}

void WorldView::resetCamera()
{
    view.zoom = 1.0f;
    view.camX = view.camY = 0.5f;
    view.follow = false;
    repaint();
}

void WorldView::zoomAt (juce::Point<float> screen, float factor)
{
    const auto before = toWorld (screen);
    view.zoom = juce::jlimit (1.0f, 6.0f, view.zoom * factor);
    const auto a = area();
    // keep the point under the mouse where it is
    view.camX = before.x - ((screen.x - a.getX()) / a.getWidth() - 0.5f) / view.zoom;
    view.camY = before.y - ((screen.y - a.getY()) / a.getHeight() - 0.5f) / view.zoom;
    clampCamera();
    repaint();
}

juce::Rectangle<float> WorldView::zoomButton (int index) const
{
    const auto a = area();
    return { a.getX() + 10.0f + (float) index * 30.0f, a.getBottom() - 34.0f, 26.0f, 24.0f };
}

uint32_t WorldView::creatureAt (const World& w, juce::Point<float> p, float radius)
{
    uint32_t best = 0;
    float bestD = 1.0e9f;
    for (const auto& c : w.creatures)
    {
        if (! c.alive) continue;
        const float rr = radius + World::radiusOf (c);
        const float dx = c.x - p.x, dy = c.y - p.y, d = dx * dx + dy * dy;
        if (d < rr * rr && d < bestD) { bestD = d; best = c.id; }
    }
    return best;
}

void WorldView::resized() {}

void WorldView::tick()
{
    const auto& w = view.world;
    if (w.time < lastWorldTime) lastEventPos = w.eventPos;           // a new world: ignore its old events
    lastWorldTime = w.time;

    const int n = (int) w.events.size();
    const int fresh = ((w.eventPos - lastEventPos) % n + n) % n;
    const double t = now();
    for (int k = fresh; k > 0; --k)
    {
        const auto& e = w.events[(size_t) (((w.eventPos - k) % n + n) % n)];
        if (fx.size() < 80) fx.push_back ({ e.x, e.y, e.kind, e.role, t });
    }
    lastEventPos = w.eventPos;
    fx.erase (std::remove_if (fx.begin(), fx.end(), [t] (const Fx& f) { return t - f.t0 > 1.2; }), fx.end());

    // camera follows the selected creature
    if (view.follow)
    {
        if (const auto* c = view.selectedCreature())
        {
            if (view.zoom < 2.0f) view.zoom = 2.5f;
            view.camX += (c->x - view.camX) * 0.15f;
            view.camY += (c->y - view.camY) * 0.15f;
            clampCamera();
        }
        else view.follow = false;
    }

    // terrain picture: redraw only when it changes
    uint32_t h = 2166136261u;
    for (auto v : w.terrain) h = (h ^ v) * 16777619u;
    if (h != terrainHash || view.theme != terrainTheme || ! terrainImage.isValid())
    {
        terrainHash = h;
        terrainTheme = view.theme;
        terrainImage = lifeart::renderTerrain (w, view.theme, 512);
    }
}

//------------------------------------------------------------------------------
void WorldView::paint (juce::Graphics& g)
{
    const auto a = area();
    const double t = now();
    const auto& w = view.world;

    lifeart::Scene sc;
    sc.theme = view.theme;
    sc.time = t;
    sc.light = w.light;
    sc.season = w.season;
    sc.pixelsPerUnit = unitPixels();

    juce::Path clip;
    clip.addRoundedRectangle (a, 10.0f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (clip);
        lifeart::drawBackground (g, a, view.theme, t);

        if (view.valid)
        {
            // terrain image covers the whole world square
            if (terrainImage.isValid())
            {
                const auto o = toScreen (0.0f, 0.0f);
                const auto e = toScreen (1.0f, 1.0f);
                g.drawImageTransformed (terrainImage, juce::AffineTransform::scale ((e.x - o.x) / (float) terrainImage.getWidth(),
                                                                                    (e.y - o.y) / (float) terrainImage.getHeight()).translated (o));
            }
            drawZones (g, t);

            const float units = unitPixels();
            int idx = 0;
            for (const auto& pl : w.plants)
            {
                ++idx;
                if (! pl.alive) continue;
                const auto c = toScreen (pl.x, pl.y);
                if (! a.expanded (20.0f).contains (c)) continue;
                lifeart::drawPlant (g, c, units * (0.0035f + 0.007f * juce::jlimit (0.0f, 1.0f, pl.size)), pl.size, idx, sc);
            }
            for (const auto& n : w.nests)
                if (n.alive) lifeart::drawNest (g, toScreen (n.x, n.y), units * 0.028f, n, sc);
            for (const auto& k : w.corpses)
                if (k.alive) lifeart::drawCorpse (g, toScreen (k.x, k.y), units * 0.012f, k, sc);
            drawLava (g, units, t);
            drawWalls (g);

            // giants under the small ones, fliers on top
            for (auto role : { Role::Giant, Role::Builder, Role::Grazer, Role::Drummer, Role::Scavenger, Role::Swarm, Role::Hunter,
                               Role::Pollinator, Role::Singer, Role::Drifter })
                for (const auto& c : w.creatures)
                    if (c.alive && c.role == role)
                    {
                        const auto pos = toScreen (c.x, c.y);
                        const float r = creatureRadius (c);
                        if (a.expanded (r * 4.0f).contains (pos)) lifeart::drawCreature (g, c, pos, r, sc);
                    }

            drawMotes (g, a, t);
            lifeart::drawSky (g, a, w, sc);
            for (const auto& c : w.creatures)
                if (c.alive) lifeart::drawGlow (g, c, toScreen (c.x, c.y), creatureRadius (c), sc);

            // favourite hearts + selection ring (on top of the night)
            for (const auto& c : w.creatures)
            {
                if (! c.alive) continue;
                const auto pos = toScreen (c.x, c.y);
                const float r = creatureRadius (c);
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
                    juce::PathStrokeType (1.6f).createDashedStroke (dashed, ring, dash, 2, juce::AffineTransform::rotation ((float) t, pos.x, pos.y));
                    g.setColour (view.pairing ? juce::Colour (0xffff9ac0) : juce::Colours::white);
                    g.fillPath (dashed);
                }
            }
            drawEffects (g, t);
            const float windX = processor.apvts.getRawParameterValue ("wind")->load()
                              * std::cos (juce::degreesToRadians (processor.apvts.getRawParameterValue ("windDir")->load()));
            lifeart::drawWeather (g, a, w, sc, windX);
        }
        // a soft vignette
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, a.getCentreX(), a.getCentreY(),
                                                 juce::Colours::black.withAlpha (0.42f), a.getX(), a.getY(), true));
        g.fillRect (a);
        drawGesture (g);
        drawHud (g, a, t);
    }
    g.setColour (C::outline);
    g.drawRoundedRectangle (a, 10.0f, 1.2f);
}

void WorldView::drawLava (juce::Graphics& g, float units, double time) const
{
    // painted lava breathes: a slow pulsing glow over each lava cell
    const auto& w = view.world;
    const auto col = lifeart::terrainColour (view.theme, Terrain::Lava);
    const float cell = units / (float) terrainSize;
    for (int cy = 0; cy < terrainSize; ++cy)
        for (int cx = 0; cx < terrainSize; ++cx)
        {
            if (w.terrain[(size_t) (cy * terrainSize + cx)] != (uint8_t) Terrain::Lava) continue;
            const auto c = toScreen (((float) cx + 0.5f) / (float) terrainSize, ((float) cy + 0.5f) / (float) terrainSize);
            const float pulse = 0.5f + 0.5f * std::sin ((float) time * 1.7f + (float) (cx * 3 + cy * 5));
            const float r = cell * (1.1f + 0.4f * pulse);
            g.setGradientFill (juce::ColourGradient (col.brighter (0.5f).withAlpha (0.18f + 0.22f * pulse), c.x, c.y, col.withAlpha (0.0f), c.x + r, c.y, true));
            g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
        }
}

void WorldView::drawMotes (juce::Graphics& g, juce::Rectangle<float> a, double time) const
{
    // drifting specks that make the world feel alive: pollen, plankton, stardust, vesicles
    static const juce::uint32 cols[] = { 0xfffff2b0, 0xffb8f0ff, 0xffffffff, 0xffe0c8ff };
    const auto col = juce::Colour (cols[juce::jlimit (0, 3, view.theme)]);
    juce::Random rnd (777 + view.theme);
    const float ft = (float) time;
    const float night = 1.0f - view.world.light;
    for (int i = 0; i < 46; ++i)
    {
        const float sx = rnd.nextFloat(), sy = rnd.nextFloat(), sp = 0.004f + 0.012f * rnd.nextFloat(), ph = rnd.nextFloat() * 6.28f;
        float x = std::fmod (sx + ft * sp + 0.02f * std::sin (ft * 0.4f + ph), 1.0f);
        float y = std::fmod (sy + (view.theme == 1 ? -ft * sp * 0.8f + 1.0f : 0.015f * std::sin (ft * 0.3f + ph * 2.0f) + 1.0f), 1.0f);
        const auto p = toScreen (x, y);
        if (! a.contains (p)) continue;
        const float tw = 0.5f + 0.5f * std::sin (ft * (1.0f + rnd.nextFloat() * 2.0f) + ph);
        const float s = (1.0f + 1.6f * rnd.nextFloat()) * juce::jmin (2.0f, std::sqrt (view.zoom));
        g.setColour (col.withAlpha ((0.12f + 0.25f * tw) * (0.6f + 0.6f * night)));
        g.fillEllipse (p.x - s * 0.5f, p.y - s * 0.5f, s, s);
    }
}

void WorldView::drawZones (juce::Graphics& g, double time) const
{
    const auto& w = view.world;
    const float aw = unitPixels();
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
                g.setColour (juce::Colour (0xff8ee0f0).withAlpha (std::sin (ph * 3.14159f) * 0.6f));
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
            juce::Path ring, dashed;
            ring.addEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
            const float dash[] = { 4.0f, 5.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, ring, dash, 2);
            g.setColour (col.withAlpha (0.25f));
            g.fillPath (dashed);
        }
    }
}

void WorldView::drawWalls (juce::Graphics& g) const
{
    const auto& w = view.world;
    static const juce::uint32 cols[] = { 0xff6b6152, 0xff3d4f5e, 0xff6fd8ff, 0xffd9a0c8 };
    const auto col = juce::Colour (cols[juce::jlimit (0, 3, view.theme)]);
    const float thick = juce::jmax (4.0f, unitPixels() * 0.009f);
    for (int i = 0; i < w.numWalls; ++i)
    {
        const auto& wl = w.walls[(size_t) i];
        const auto p1 = toScreen (wl.x1, wl.y1), p2 = toScreen (wl.x2, wl.y2);
        if (view.theme == 2)
        {
            g.setColour (col.withAlpha (0.18f));
            g.drawLine ({ p1, p2 }, thick * 2.2f);
        }
        juce::Path line;
        line.startNewSubPath (p1);
        line.lineTo (p2);
        g.setColour (col);
        g.strokePath (line, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col.brighter (0.4f).withAlpha (0.5f));
        g.strokePath (line, juce::PathStrokeType (thick * 0.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void WorldView::drawEffects (juce::Graphics& g, double time)
{
    for (const auto& f : fx)
    {
        const float age = (float) (time - f.t0) / 1.2f;
        if (age < 0.0f || age > 1.0f) continue;
        const auto pos = toScreen (f.x, f.y);
        const auto col = lifeart::roleColour (view.theme, f.role);
        if (f.kind == 0 || f.kind == 3)   // born / a new plant: sparkles flying out
        {
            for (int k = 0; k < 6; ++k)
            {
                const float th = twoPi * (float) k / 6.0f + 0.3f;
                const float d = 4.0f + age * (f.kind == 3 ? 10.0f : 18.0f);
                g.setColour ((f.kind == 3 ? juce::Colour (0xfff5e08a) : col.brighter (0.5f)).withAlpha (1.0f - age));
                g.fillEllipse (pos.x + std::cos (th) * d - 1.5f, pos.y + std::sin (th) * d - 1.5f, 3.0f, 3.0f);
            }
        }
        else if (f.kind == 5)             // nest built: a little burst of twigs / bubbles
        {
            for (int k = 0; k < 8; ++k)
            {
                const float th = twoPi * (float) k / 8.0f;
                const float d = 3.0f + age * 14.0f;
                g.setColour (juce::Colour (0xffd8b080).withAlpha (1.0f - age));
                g.drawLine (pos.x + std::cos (th) * d, pos.y + std::sin (th) * d, pos.x + std::cos (th) * (d + 4.0f), pos.y + std::sin (th) * (d + 4.0f), 1.4f);
            }
        }
        else if (f.kind == 6)             // burnt in lava: rising embers
        {
            juce::Random rnd ((juce::int64) (f.t0 * 1000.0));
            for (int k = 0; k < 7; ++k)
            {
                const float ex = pos.x + (rnd.nextFloat() - 0.5f) * 14.0f, ey = pos.y - age * (16.0f + 20.0f * rnd.nextFloat());
                g.setColour (juce::Colour (0xffffa040).withAlpha (1.0f - age));
                g.fillEllipse (ex - 1.5f, ey - 1.5f, 3.0f, 3.0f);
            }
        }
        else                              // died / eaten / eaten up: a fading ghost ring
        {
            const float rr = 4.0f + age * 10.0f;
            g.setColour ((f.kind == 2 ? juce::Colour (0xffff8070) : C::textDim).withAlpha (0.6f * (1.0f - age)));
            g.drawEllipse (pos.x - rr, pos.y - rr, rr * 2.0f, rr * 2.0f, 1.2f);
        }
    }
}

void WorldView::drawGesture (juce::Graphics& g) const
{
    const float aw = unitPixels();
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
    if (hovering && view.valid && ! panning)
    {
        const auto p = toScreen (hoverW.x, hoverW.y);
        if (view.tool == GardenTool::Remove || view.tool == GardenTool::Erase || view.tool == GardenTool::Paint)
        {
            const float r = (view.tool == GardenTool::Erase ? 0.05f : view.brush) * aw;
            auto col = juce::Colour (0x60ff8080);
            if (view.tool == GardenTool::Paint)
                col = view.paintType == Terrain::Ground ? juce::Colours::white.withAlpha (0.5f) : lifeart::terrainColour (view.theme, view.paintType).withAlpha (0.8f);
            g.setColour (col);
            g.drawEllipse (p.x - r, p.y - r, 2.0f * r, 2.0f * r, 1.4f);
        }
        else if (view.tool == GardenTool::Inspect)
        {
            if (const auto id = creatureAt (view.world, hoverW, 0.03f / view.zoom))
                for (const auto& c : view.world.creatures)
                    if (c.alive && c.id == id)
                    {
                        const auto cp = toScreen (c.x, c.y);
                        const float r = creatureRadius (c) * 2.0f + 2.0f;
                        g.setColour (juce::Colours::white.withAlpha (0.35f));
                        g.drawEllipse (cp.x - r, cp.y - r, 2.0f * r, 2.0f * r, 1.0f);
                        g.setFont (cat::catFont (11.0f));
                        g.drawText (lifeart::roleName (view.theme, c.role) + " #" + juce::String (c.id), juce::Rectangle<float> (cp.x - 60.0f, cp.y + r + 2.0f, 120.0f, 14.0f),
                                    juce::Justification::centred);
                    }
        }
        else if (view.tool == GardenTool::Add || view.tool == GardenTool::PlaceLab)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.setOpacity (0.5f);
            lifeart::drawIcon (g, juce::Rectangle<float> (40.0f, 40.0f).withCentre (p), view.theme, view.tool == GardenTool::Add ? view.addRole : view.labRole);
        }
    }
}

void WorldView::drawHud (juce::Graphics& g, juce::Rectangle<float> a, double time) const
{
    auto top = a.reduced (10.0f, 8.0f).removeFromTop (16.0f);
    g.setFont (cat::catFont (11.0f));
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    juce::String left = toolName (view.tool) + ": " + toolHint (view.tool, view);
    if (view.pairing) left = "Pick a partner for #" + juce::String (view.selected) + " (right-click to cancel)";
    g.drawText (left, top, juce::Justification::centredLeft);

    if (! view.valid)
    {
        g.setFont (cat::catFont (14.0f));
        g.setColour (C::textDim);
        g.drawText ("Waking up the world...", a, juce::Justification::centred);
        return;
    }
    // the sky in words: time, season, weather
    const auto& w = view.world;
    juce::String sky;
    if (processor.apvts.getRawParameterValue ("dayLength")->load() > 0.5f) sky << lifeart::clockText (w.dayPhase) << (w.light < 0.3f ? " night" : "") << "   ";
    sky << lifeart::seasonName (w.season);
    if (w.weather != Weather::Clear && w.weatherAmount > 0.1f) sky << "   " << lifeart::weatherName (w.weather);
    if (! view.running) sky = "Life paused   " + sky;
    else if (! view.singing) sky = "Silent: press Play (or Play When Stopped)   " + sky;
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    if (view.selected == 0) g.drawText (sky, top, juce::Justification::centredRight);

    // zoom buttons
    const char* labels[] = { "-", "+", "Fit" };
    for (int i = 0; i < 3; ++i)
    {
        const auto b = zoomButton (i);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (cat::catFont (i == 2 ? 10.0f : 14.0f, true));
        g.drawText (labels[i], b, juce::Justification::centred);
    }
    if (view.zoom > 1.01f || view.follow)
    {
        g.setFont (cat::catFont (11.0f));
        g.setColour (juce::Colours::white.withAlpha (0.65f));
        juce::String z = juce::String (view.zoom, 1) + "x";
        if (view.follow) z << "   following #" << (int) view.selected;
        g.drawText (z, zoomButton (2).translated (34.0f, 0.0f).withWidth (200.0f), juce::Justification::centredLeft);
    }

    if (view.message.isNotEmpty() && time - view.messageTime < 2.5)
    {
        const float alpha = (float) juce::jlimit (0.0, 1.0, 2.5 - (time - view.messageTime));
        auto box = a.withSizeKeepingCentre (280.0f, 30.0f).withBottomY (a.getBottom() - 14.0f);
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
    // zoom buttons
    for (int i = 0; i < 3; ++i)
        if (zoomButton (i).contains (e.position))
        {
            if (i == 2) resetCamera();
            else zoomAt (area().getCentre(), i == 0 ? 1.0f / 1.5f : 1.5f);
            return;
        }

    // pan: middle button or Alt
    if (e.mods.isMiddleButtonDown() || e.mods.isAltDown())
    {
        panning = true;
        panStart = e.position;
        panCamX = view.camX;
        panCamY = view.camY;
        view.follow = false;
        return;
    }

    const auto w = toWorld (e.position);
    downW = curW = w;
    dragging = false;
    using Cmd = LifeCatProcessor::Command;

    // right-click: inspect with any tool (and cancel pairing)
    if (e.mods.isPopupMenu())
    {
        view.pairing = false;
        view.selected = creatureAt (view.world, w, 0.03f / view.zoom);
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
            const auto id = creatureAt (view.world, w, 0.03f / view.zoom);
            if (id == 0 && view.zoom > 1.01f && ! view.pairing)
            {
                // empty space while zoomed in: drag to look around
                panning = true;
                panStart = e.position;
                panCamX = view.camX;
                panCamY = view.camY;
                view.follow = false;
                return;
            }
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
                if (id != view.selected) view.follow = false;
                view.selected = id;
            }
            if (onSelectionChanged) onSelectionChanged();
            break;
        }
        case GardenTool::Food:
            c.type = Cmd::Food; c.r = 6.0f; processor.send (c); lastDrop = now();
            break;
        case GardenTool::Add:
            c.type = Cmd::Add; c.role = (int) view.addRole; processor.send (c);
            break;
        case GardenTool::PlaceLab:
            c.type = Cmd::AddGenes; c.role = (int) view.labRole; c.genes = view.labGenes; processor.send (c);
            view.say ("Your " + lifeart::roleName (view.theme, view.labRole) + " joins the world");
            break;
        case GardenTool::Remove:
            c.type = Cmd::Remove; c.r = view.brush; processor.send (c); lastDrop = now();
            break;
        case GardenTool::Paint:
            c.type = Cmd::Paint; c.r = view.brush; c.terrain = (int) view.paintType; processor.send (c); lastDrop = now();
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
    if (panning)
    {
        const auto a = area();
        view.camX = panCamX - (e.position.x - panStart.x) / a.getWidth() / view.zoom;
        view.camY = panCamY - (e.position.y - panStart.y) / a.getHeight() / view.zoom;
        clampCamera();
        repaint();
        return;
    }
    if (e.mods.isPopupMenu()) return;
    curW = toWorld (e.position);
    hoverW = curW;
    hovering = true;
    using Cmd = LifeCatProcessor::Command;
    const double t = now();
    if (view.tool == GardenTool::Food && t - lastDrop > 0.07)
    {
        Cmd c; c.type = Cmd::Food; c.x = curW.x; c.y = curW.y; c.r = 2.0f;
        processor.send (c);
        lastDrop = t;
    }
    else if ((view.tool == GardenTool::Remove || view.tool == GardenTool::Paint) && t - lastDrop > 0.03)
    {
        Cmd c; c.x = curW.x; c.y = curW.y; c.r = view.brush;
        c.type = view.tool == GardenTool::Remove ? Cmd::Remove : Cmd::Paint;
        c.terrain = (int) view.paintType;
        processor.send (c);
        lastDrop = t;
    }
    repaint();
}

void WorldView::mouseUp (const juce::MouseEvent& e)
{
    if (panning) { panning = false; return; }
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

void WorldView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& d)
{
    const float amount = d.deltaY != 0.0f ? d.deltaY : d.deltaX;
    if (std::abs (amount) < 1.0e-4f) return;
    if (amount < 0.0f) view.follow = false;
    zoomAt (e.position, std::pow (1.6f, amount * 2.0f));
}

void WorldView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isMiddleButtonDown()) resetCamera();
}

//==============================================================================
InspectorCard::InspectorCard (LifeCatProcessor& p, LifeViewState& v) : processor (p), view (v)
{
    using Cmd = LifeCatProcessor::Command;
    listen.setTooltip ("Hear this creature sing one note");
    favourite.setTooltip ("Favourites never die of old age and nearly always escape hunters");
    cloneButton.setTooltip ("An exact copy (same genes, same sound)");
    pair.setTooltip ("Breed: then click a partner. The baby blends both parents' genes plus a little mutation.");
    follow.setTooltip ("Zoom in and follow this creature around");
    toLab.setTooltip ("Copy its genes into the Gene Lab to edit them");
    remove.setTooltip ("Take this creature out of the world");

    listen.onClick = [this] { Cmd c; c.type = Cmd::Listen; c.id = view.selected; processor.send (c); };
    favourite.onClick = [this]
    {
        Cmd c; c.type = Cmd::Favourite; c.id = view.selected; c.flag = ! shown.favourite; processor.send (c);
        view.say (c.flag ? "Favourite - it will live on" : "No longer a favourite");
    };
    cloneButton.onClick = [this] { Cmd c; c.type = Cmd::Clone; c.id = view.selected; processor.send (c); view.say ("Cloned!"); };
    pair.onClick = [this] { view.pairing = ! view.pairing; view.tool = GardenTool::Inspect; };
    follow.onClick = [this] { view.follow = ! view.follow; if (! view.follow) { view.zoom = 1.0f; view.camX = view.camY = 0.5f; } };
    toLab.onClick = [this] { if (onToLab) onToLab(); };
    remove.onClick = [this]
    {
        Cmd c; c.type = Cmd::RemoveId; c.id = view.selected; processor.send (c);
        view.selected = 0; view.pairing = false; view.follow = false; refresh();
    };
    close.onClick = [this] { view.selected = 0; view.pairing = false; view.follow = false; refresh(); };
    for (auto* b : { &listen, &favourite, &cloneButton, &pair, &follow, &toLab, &remove, &close }) addAndMakeVisible (b);
    refresh();
}

void InspectorCard::refresh()
{
    found = false;
    if (const auto* c = view.selectedCreature()) { shown = *c; found = true; }
    setVisible (view.selected != 0);
    for (auto* b : { &listen, &favourite, &cloneButton, &pair, &follow, &toLab, &remove }) b->setEnabled (found);
    favourite.setButtonText (found && shown.favourite ? "Unfavourite" : "Favourite");
    pair.setButtonText (view.pairing ? "Cancel" : "Pair...");
    follow.setButtonText (view.follow ? "Stop" : "Follow");
    repaint();
}

void InspectorCard::resized()
{
    auto r = getLocalBounds().reduced (8);
    close.setBounds (r.getRight() - 20, r.getY(), 20, 18);
    auto buttons = r.removeFromBottom (52);
    auto row1 = buttons.removeFromTop (24), row2 = buttons.removeFromBottom (24);
    const int w1 = row1.getWidth() / 4, w2 = row2.getWidth() / 3;
    for (auto* b : { &listen, &favourite, &cloneButton, &pair }) b->setBounds (row1.removeFromLeft (w1).reduced (2, 0));
    for (auto* b : { &follow, &toLab, &remove }) b->setBounds (row2.removeFromLeft (w2).reduced (2, 0));
}

void InspectorCard::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (C::panel.withAlpha (0.93f));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (C::outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);
    auto r = b.reduced (10.0f, 8.0f);
    r.removeFromBottom (56.0f);

    if (! found)
    {
        g.setColour (C::textDim);
        g.setFont (cat::catFont (13.0f));
        g.drawFittedText ("#" + juce::String (view.selected) + " is gone - it died or was eaten.", r.toNearestInt(), juce::Justification::centred, 3);
        return;
    }

    const auto& c = shown;
    const auto col = lifeart::creatureColour (view.theme, c);
    auto head = r.removeFromTop (34.0f);
    lifeart::drawIcon (g, head.removeFromLeft (34.0f), view.theme, c.role);
    head.removeFromLeft (6.0f);
    g.setColour (C::text);
    g.setFont (cat::catFont (14.0f, true));
    g.drawText (lifeart::roleName (view.theme, c.role) + "  #" + juce::String (c.id), head.removeFromTop (18.0f).withTrimmedRight (24.0f), juce::Justification::centredLeft);
    g.setFont (cat::catFont (10.5f));
    g.setColour (C::textDim);
    juce::String info = "gen " + juce::String (c.generation) + "   age " + juce::String (juce::roundToInt (c.age)) + " s";
    if (c.parentA != 0) info << "   parents #" << (int) c.parentA << (c.parentB != 0 ? " x #" + juce::String (c.parentB) : juce::String (" (clone)"));
    if (c.activity < 0.35f) info << "   asleep";
    g.drawText (info, head, juce::Justification::centredLeft);

    auto en = r.removeFromTop (14.0f);
    g.drawText ("Energy", en.removeFromLeft (52.0f), juce::Justification::centredLeft);
    auto bar = en.reduced (0.0f, 3.0f);
    g.setColour (C::raised);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (c.energy < 0.25f ? C::red : C::green);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, c.energy)), 3.0f);
    r.removeFromTop (4.0f);

    // genes in two columns: life (left) and sound (right)
    const auto names = geneNames();
    const float colW = (r.getWidth() - 12.0f) / 2.0f;
    const float rowH = juce::jmin (13.0f, r.getHeight() / 16.0f);
    auto column = [&] (const int* genes, int count, float x, const juce::String& title, juce::Colour barColour)
    {
        g.setFont (cat::catFont (9.5f, true));
        g.setColour (C::text.withAlpha (0.7f));
        g.drawText (title, juce::Rectangle<float> (x, r.getY(), colW, rowH), juce::Justification::centredLeft);
        g.setFont (cat::catFont (9.5f));
        for (int k = 0; k < count; ++k)
        {
            const int gi = genes[k];
            auto cell = juce::Rectangle<float> (x, r.getY() + (float) (k + 1) * rowH, colW, rowH);
            g.setColour (C::textDim);
            g.drawText (names[gi], cell.removeFromLeft (60.0f), juce::Justification::centredLeft);
            auto gb = cell.reduced (0.0f, rowH * 0.28f);
            g.setColour (C::raised);
            g.fillRoundedRectangle (gb, 2.0f);
            g.setColour (barColour.withAlpha (0.9f));
            g.fillRoundedRectangle (gb.withWidth (juce::jmax (2.0f, gb.getWidth() * juce::jlimit (0.0f, 1.0f, c.genes[gi]))), 2.0f);
        }
    };
    column (labLeft, 14, r.getX(), "LIFE & BODY", C::slate);
    column (labRight, 15, r.getX() + colW + 12.0f, "SOUND", col);
}

//==============================================================================
ToolPanel::ToolPanel (LifeViewState& v) : view (v)
{
    for (int i = 0; i < numGardenTools; ++i)
    {
        auto* b = tools.add (new juce::TextButton (toolName ((GardenTool) i)));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4711);
        b->setTooltip (toolTips[i]);
        b->onClick = [this, i] { if (tools[i]->getToggleState() && onTool) onTool ((GardenTool) i); };
        addAndMakeVisible (b);
    }
    tools[0]->setToggleState (true, juce::dontSendNotification);
    startTimerHz (10);
}

void ToolPanel::updateNames() { repaint(); }

void ToolPanel::timerCallback()
{
    const int t = (int) view.tool;
    for (int i = 0; i < numGardenTools; ++i)
    {
        const bool on = i == t;
        if (tools[i]->getToggleState() != on) tools[i]->setToggleState (on, juce::dontSendNotification);
    }
    repaint (contextArea);
}

int ToolPanel::numChoices() const
{
    if (view.tool == GardenTool::Add) return numRoles;
    if (view.tool == GardenTool::Paint) return numTerrains;
    return 0;
}

juce::Rectangle<float> ToolPanel::choiceBox (int index) const
{
    const auto a = contextArea.toFloat().withTrimmedTop (18.0f);
    if (view.tool == GardenTool::Add)
    {
        const float w = a.getWidth() / 2.0f, h = 46.0f;
        return { a.getX() + (float) (index % 2) * w, a.getY() + (float) (index / 2) * h, w, h };
    }
    return { a.getX(), a.getY() + (float) index * 26.0f, a.getWidth(), 24.0f };
}

juce::Rectangle<float> ToolPanel::brushBar() const
{
    const auto a = contextArea.toFloat();
    return { a.getX() + 4.0f, a.getBottom() - 22.0f, a.getWidth() - 8.0f, 14.0f };
}

void ToolPanel::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (18);
    for (int i = 0; i < numGardenTools; ++i)
    {
        if (i == 2 || i == 4 || i == 8) r.removeFromTop (5);
        tools[i]->setBounds (r.removeFromTop (28).reduced (0, 2));
    }
    r.removeFromTop (8);
    contextArea = r;
}

void ToolPanel::paint (juce::Graphics& g)
{
    g.setFont (cat::catFont (11.0f, true));
    g.setColour (C::textDim);
    g.drawText ("GARDENER", getLocalBounds().removeFromTop (16), juce::Justification::centred);

    const auto a = contextArea.toFloat();
    g.setColour (C::panel);
    g.fillRoundedRectangle (a, 8.0f);
    g.setFont (cat::catFont (10.0f, true));
    g.setColour (C::textDim);
    const char* title = view.tool == GardenTool::Add ? "ANIMAL" : (view.tool == GardenTool::Paint ? "TERRAIN" : (view.tool == GardenTool::PlaceLab ? "LAB" : ""));
    g.drawText (title, a.withHeight (16.0f), juce::Justification::centred);

    if (view.tool == GardenTool::Add)
        for (int i = 0; i < numRoles; ++i)
        {
            auto box = choiceBox (i).reduced (2.0f);
            const bool on = (int) view.addRole == i;
            g.setColour (on ? C::accent.withAlpha (0.25f) : C::raised);
            g.fillRoundedRectangle (box, 5.0f);
            if (on) { g.setColour (C::accent); g.drawRoundedRectangle (box, 5.0f, 1.2f); }
            lifeart::drawIcon (g, box.withTrimmedBottom (12.0f), view.theme, (Role) i);
            g.setColour (C::text);
            g.setFont (cat::catFont (9.5f));
            g.drawFittedText (lifeart::roleName (view.theme, (Role) i), box.removeFromBottom (13.0f).toNearestInt(), juce::Justification::centred, 1);
        }
    else if (view.tool == GardenTool::Paint)
        for (int i = 0; i < numTerrains; ++i)
        {
            const auto box = choiceBox (i).reduced (2.0f, 1.0f);
            const bool on = (int) view.paintType == i;
            g.setColour (on ? C::accent.withAlpha (0.25f) : C::raised);
            g.fillRoundedRectangle (box, 5.0f);
            if (on) { g.setColour (C::accent); g.drawRoundedRectangle (box, 5.0f, 1.2f); }
            auto sw = box.reduced (4.0f).removeFromLeft (18.0f);
            g.setColour (i == 0 ? C::background : lifeart::terrainColour (view.theme, (Terrain) i));
            g.fillRoundedRectangle (sw, 3.0f);
            g.setColour (C::text);
            g.setFont (cat::catFont (10.5f));
            g.drawText (i == 0 ? juce::String ("Rub out") : lifeart::terrainName (view.theme, (Terrain) i), box.withTrimmedLeft (28.0f), juce::Justification::centredLeft);
        }
    else if (view.tool == GardenTool::PlaceLab)
    {
        lifeart::drawIcon (g, a.withTrimmedTop (18.0f).withHeight (60.0f), view.theme, view.labRole);
        g.setColour (C::textDim);
        g.setFont (cat::catFont (10.5f));
        g.drawFittedText ("Click in the world to place your lab creature", a.withTrimmedTop (80.0f).withHeight (48.0f).reduced (4.0f).toNearestInt(),
                          juce::Justification::centredTop, 3);
    }

    if (view.tool == GardenTool::Paint || view.tool == GardenTool::Remove)
    {
        const auto bb = brushBar();
        g.setColour (C::textDim);
        g.setFont (cat::catFont (9.5f));
        g.drawText ("Brush size", bb.translated (0.0f, -13.0f).withHeight (12.0f), juce::Justification::centredLeft);
        g.setColour (C::raised);
        g.fillRoundedRectangle (bb, 4.0f);
        g.setColour (C::accent.withAlpha (0.8f));
        g.fillRoundedRectangle (bb.withWidth (bb.getWidth() * (view.brush - 0.02f) / 0.16f), 4.0f);
    }
}

void ToolPanel::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < numChoices(); ++i)
        if (choiceBox (i).contains (e.position))
        {
            if (view.tool == GardenTool::Add) view.addRole = (Role) i;
            else view.paintType = (Terrain) i;
            repaint();
            return;
        }
    mouseDrag (e);
}

void ToolPanel::mouseDrag (const juce::MouseEvent& e)
{
    if ((view.tool == GardenTool::Paint || view.tool == GardenTool::Remove) && brushBar().expanded (0.0f, 6.0f).contains (e.position))
    {
        const auto bb = brushBar();
        view.brush = 0.02f + 0.16f * juce::jlimit (0.0f, 1.0f, (e.position.x - bb.getX()) / bb.getWidth());
        repaint();
    }
}

//==============================================================================
SpeciesPanel::SpeciesPanel (LifeCatProcessor& p, LifeViewState& v) : processor (p), view (v)
{
    auto& s = p.apvts;
    for (int r = 0; r < numRoles; ++r)
    {
        auto id = [r] (const char* w) { return "sp" + juce::String (r) + w; };
        auto& row = rows[(size_t) r];
        row.sing = std::make_unique<cat::ParamToggle> (s, id ("Sing"), "Sing", &p.midiLearn);
        row.volume = std::make_unique<cat::BarSlider> (s, id ("Vol"), "Vol", &p.midiLearn, 30);
        row.speed = std::make_unique<cat::BarSlider> (s, id ("Speed"), "Speed", &p.midiLearn, 40);
        row.sound = std::make_unique<cat::ParamCombo> (s, id ("Sound"), "", &p.midiLearn, 0);
        row.octave = cat::Stepper::forParameter (s, id ("Oct"), "Oct", [] (int x) { return x > 0 ? "+" + juce::String (x) : juce::String (x); });
        row.max = cat::Stepper::forParameter (s, id ("Max"), "Max");
        row.sing->setTooltip ("Let this kind sing (off = it still lives, but is silent)");
        row.sound->getCombo().setTooltip ("Natural = this kind's own sound; Gene Mix = only its genes decide; or force a sound type");
        row.max->setTooltip ("Most of this kind the world will hold (0 = none: they do not breed or arrive)");
        row.speed->getSlider().setTooltip ("How fast this kind moves");
        for (auto* c : std::initializer_list<juce::Component*> { row.sing.get(), row.volume.get(), row.speed.get(), row.sound.get(), row.octave.get(), row.max.get() })
            addAndMakeVisible (c);
    }
}

void SpeciesPanel::resized()
{
    auto r = getLocalBounds();
    const int rowH = juce::jmin (74, r.getHeight() / numRoles);
    for (int i = 0; i < numRoles; ++i)
    {
        auto row = r.removeFromTop (rowH).reduced (4, 3);
        auto line1 = row.removeFromTop (row.getHeight() / 2).reduced (0, 1);
        auto line2 = row.reduced (0, 1);
        auto& rw = rows[(size_t) i];
        line1.removeFromLeft (150);
        rw.sing->setBounds (line1.removeFromLeft (56).reduced (2, 0));
        rw.sound->setBounds (line1.reduced (2, 0));
        const int q = line2.getWidth() / 4;
        rw.volume->setBounds (line2.removeFromLeft (q + 14).reduced (2, 0));
        rw.octave->setBounds (line2.removeFromLeft (q - 10).reduced (2, 0));
        rw.speed->setBounds (line2.removeFromLeft (q + 4).reduced (2, 0));
        rw.max->setBounds (line2.reduced (2, 0));
    }
}

void SpeciesPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    const int rowH = juce::jmin (74, r.getHeight() / numRoles);
    for (int i = 0; i < numRoles; ++i)
    {
        auto row = r.removeFromTop (rowH).reduced (0, 2);
        g.setColour (i % 2 == 0 ? C::panel : C::panelLight);
        g.fillRoundedRectangle (row.toFloat(), 6.0f);
        auto head = row.reduced (4, 0).removeFromTop (row.getHeight() / 2).withWidth (150).toFloat();
        lifeart::drawIcon (g, head.removeFromLeft (34.0f), view.theme, (Role) i);
        head.removeFromLeft (4.0f);
        g.setColour (C::text);
        g.setFont (cat::catFont (12.5f, true));
        g.drawText (lifeart::roleName (view.theme, (Role) i, true), head.removeFromTop (head.getHeight() * 0.6f), juce::Justification::bottomLeft);
        g.setColour (C::textDim);
        g.setFont (cat::catFont (10.0f));
        g.drawText (juce::String (view.valid ? view.world.count ((Role) i) : 0) + " alive", head, juce::Justification::topLeft);
    }
}

//==============================================================================
GeneLab::GeneLab (LifeCatProcessor& p, LifeViewState& v) : processor (p), view (v)
{
    using Cmd = LifeCatProcessor::Command;
    for (int r = 0; r < numRoles; ++r) kind.addItem (lifeart::roleName (view.theme, (Role) r), r + 1);
    kind.setSelectedId (1, juce::dontSendNotification);
    kind.onChange = [this] { view.labRole = (Role) juce::jlimit (0, numRoles - 1, kind.getSelectedId() - 1); repaint(); };
    addAndMakeVisible (kind);

    auto& w = scratchWorld();
    w.rng.seed ((uint32_t) juce::Random::getSystemRandom().nextInt() | 1u);
    view.labGenes = w.randomGenes (view.labRole);

    fromSelected.setTooltip ("Copy the genes of the creature you inspected");
    randomise.setTooltip ("Random genes for this kind");
    mutate.setTooltip ("Nudge every gene a little, like a baby would get");
    listen.setTooltip ("Hear how a creature with these genes sings");
    apply.setTooltip ("Give these genes to the selected creature (it keeps living, but sings and moves differently)");
    place.setTooltip ("Then click in the world to drop a creature with these genes in");

    fromSelected.onClick = [this] { if (const auto* c = view.selectedCreature()) loadFrom (*c); else view.say ("Select a creature first"); };
    randomise.onClick = [this] { view.labGenes = scratchWorld().randomGenes (view.labRole); repaint(); };
    mutate.onClick = [this] { view.labGenes = scratchWorld().blend (view.labGenes, view.labGenes, 0.5f); repaint(); };
    listen.onClick = [this] { Cmd c; c.type = Cmd::ListenGenes; c.role = (int) view.labRole; c.genes = view.labGenes; processor.send (c); };
    apply.onClick = [this]
    {
        if (view.selectedCreature() == nullptr) { view.say ("Select a creature first"); return; }
        Cmd c; c.type = Cmd::SetGenes; c.id = view.selected; c.genes = view.labGenes; processor.send (c);
        view.say ("New genes for #" + juce::String (view.selected));
    };
    place.onClick = [this] { if (onPlace) onPlace(); };
    for (auto* b : { &fromSelected, &randomise, &mutate, &listen, &apply, &place }) addAndMakeVisible (b);
}

void GeneLab::loadFrom (const Creature& c)
{
    view.labGenes = c.genes;
    view.labRole = c.role;
    kind.setSelectedId ((int) c.role + 1, juce::dontSendNotification);
    repaint();
}

void GeneLab::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (28);
    kind.setBounds (top.removeFromLeft (180).reduced (0, 2));
    for (int i = 0; i < kind.getNumItems(); ++i) kind.changeItemText (i + 1, lifeart::roleName (view.theme, (Role) i));
    auto buttons = r.removeFromBottom (60);
    auto b1 = buttons.removeFromTop (28), b2 = buttons.removeFromBottom (28);
    const int w1 = b1.getWidth() / 3, w2 = b2.getWidth() / 3;
    for (auto* b : { &fromSelected, &randomise, &mutate }) b->setBounds (b1.removeFromLeft (w1).reduced (2, 1));
    for (auto* b : { &listen, &apply, &place }) b->setBounds (b2.removeFromLeft (w2).reduced (2, 1));
    barsArea = r.reduced (0, 6);
}

juce::Rectangle<float> GeneLab::barArea (int gene) const
{
    const auto a = barsArea.toFloat();
    const float colW = (a.getWidth() - 14.0f) / 2.0f;
    const float rowH = juce::jmin (19.0f, a.getHeight() / 16.0f);
    for (int k = 0; k < 14; ++k) if (labLeft[k] == gene) return { a.getX(), a.getY() + (float) (k + 1) * rowH, colW, rowH };
    for (int k = 0; k < 15; ++k) if (labRight[k] == gene) return { a.getX() + colW + 14.0f, a.getY() + (float) (k + 1) * rowH, colW, rowH };
    return {};
}

void GeneLab::paint (juce::Graphics& g)
{
    const auto names = geneNames();
    const auto a = barsArea.toFloat();
    const float colW = (a.getWidth() - 14.0f) / 2.0f;
    const float rowH = juce::jmin (19.0f, a.getHeight() / 16.0f);
    g.setFont (cat::catFont (10.0f, true));
    g.setColour (C::text.withAlpha (0.7f));
    g.drawText ("LIFE & BODY", juce::Rectangle<float> (a.getX(), a.getY(), colW, rowH), juce::Justification::centredLeft);
    g.drawText ("SOUND", juce::Rectangle<float> (a.getX() + colW + 14.0f, a.getY(), colW, rowH), juce::Justification::centredLeft);
    lifeart::drawIcon (g, juce::Rectangle<float> ((float) getWidth() - 40.0f, 0.0f, 36.0f, 28.0f), view.theme, view.labRole);
    g.setFont (cat::catFont (10.5f));
    g.setColour (C::textDim);
    g.drawText ("Drag the bars to design a creature", juce::Rectangle<float> (190.0f, 0.0f, (float) getWidth() - 236.0f, 28.0f), juce::Justification::centredLeft);
    const auto col = lifeart::roleColour (view.theme, view.labRole);
    for (int gi = 0; gi < numGenes; ++gi)
    {
        auto cell = barArea (gi);
        g.setColour (C::textDim);
        g.setFont (cat::catFont (10.0f));
        g.drawText (names[gi], cell.removeFromLeft (64.0f), juce::Justification::centredLeft);
        auto gb = cell.reduced (0.0f, rowH * 0.22f);
        g.setColour (C::raised);
        g.fillRoundedRectangle (gb, 3.0f);
        g.setColour ((isSoundGene (gi) ? col : C::slate).withAlpha (0.9f));
        g.fillRoundedRectangle (gb.withWidth (juce::jmax (3.0f, gb.getWidth() * view.labGenes[gi])), 3.0f);
    }
}

void GeneLab::setFromMouse (juce::Point<float> p)
{
    if (dragGene < 0) return;
    auto cell = barArea (dragGene);
    cell.removeFromLeft (64.0f);
    view.labGenes[dragGene] = juce::jlimit (0.0f, 1.0f, (p.x - cell.getX()) / cell.getWidth());
    repaint();
}

void GeneLab::mouseDown (const juce::MouseEvent& e)
{
    dragGene = -1;
    for (int gi = 0; gi < numGenes; ++gi)
        if (barArea (gi).contains (e.position)) { dragGene = gi; break; }
    setFromMouse (e.position);
}

void GeneLab::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }

//==============================================================================
void PopulationGraph::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (C::panel);
    g.fillRoundedRectangle (b, 8.0f);
    auto r = b.reduced (10.0f, 6.0f);
    const auto& w = view.world;

    auto stats = r.removeFromTop (28.0f);
    g.setFont (cat::catFont (10.0f));
    float x = stats.getX(), y = stats.getY();
    for (int k = 0; k < numRoles; ++k)
    {
        const auto role = (Role) k;
        const int n = view.valid ? w.count (role) : 0;
        const juce::String s = juce::String (n) + " " + lifeart::roleName (view.theme, role, true);
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) + 14.0f;
        if (x + tw > stats.getRight()) { x = stats.getX(); y += 14.0f; }
        g.setColour (lifeart::roleColour (view.theme, role).withAlpha (n > 0 ? 1.0f : 0.35f));
        g.fillEllipse (x, y + 4.0f, 6.0f, 6.0f);
        g.setColour (n > 0 ? C::text : C::textDim);
        g.drawText (s, juce::Rectangle<float> (x + 9.0f, y, tw, 14.0f), juce::Justification::centredLeft);
        x += tw;
    }
    r.removeFromTop (2.0f);
    auto info = r.removeFromBottom (14.0f);
    g.setColour (C::textDim);
    g.drawText ("plants " + juce::String (w.plantCount()) + "   generation " + juce::String (w.maxGeneration) + "   born " + juce::String (w.births)
                + "   gone " + juce::String (w.deaths), info, juce::Justification::centredLeft);
    g.drawText ("last 8 min", info, juce::Justification::centredRight);

    g.setColour (C::background);
    g.fillRoundedRectangle (r, 4.0f);
    if (! view.valid) return;
    int peak = 8;
    for (const auto& h : w.history) for (auto v : h) peak = juce::jmax (peak, (int) v);
    const auto plot = r.reduced (4.0f, 3.0f);
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
        g.setColour (lifeart::roleColour (view.theme, (Role) k).withAlpha (0.9f));
        g.strokePath (line, juce::PathStrokeType (1.4f));
    }
}

//==============================================================================
LifeCatEditor::LifeCatEditor (LifeCatProcessor& p)
    : processor (p),
      presetBar (p.presetManager),
      theme (p.apvts, "theme", "World", &p.midiLearn, 44),
      running (p.apvts, "running", "Life", &p.midiLearn),
      surpriseControls (p, p.surpriseUndo, [&p] { p.surprise(); }),
      toolPanel (view),
      worldView (p, view),
      inspector (p, view),
      graph (view),
      speciesPanel (p, view),
      geneLab (p, view),
      gravity (p.apvts, "gravity", "Gravity", &p.midiLearn, 58),
      wind (p.apvts, "wind", "Wind", &p.midiLearn, 44),
      windDir (p.apvts, "windDir", "Direction", &p.midiLearn, 64),
      ambience (p.apvts, "ambience", "Weather Sound", &p.midiLearn, 94),
      dayNightSound (p.apvts, "dayNightSound", "Day/Night Colour", &p.midiLearn, 104),
      dayLength (p.apvts, "dayLength", "Day / Night", &p.midiLearn, 80),
      seasonLength (p.apvts, "seasonLength", "Seasons", &p.midiLearn, 80),
      season (p.apvts, "season", "Season", &p.midiLearn, 80),
      weather (p.apvts, "weather", "Weather", &p.midiLearn, 80),
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
    bigButton.setClickingTogglesState (true);
    bigButton.setToggleState (processor.bigView, juce::dontSendNotification);
    bigButton.setTooltip ("Let the world fill the window (the settings slide away)");
    bigButton.onClick = [this] { setBigView (bigButton.getToggleState()); };
    dragButton.setTooltip ("Drag the last 8 bars the creatures sang onto a Cubase MIDI track");
    dragButton.onDragOut = [this]
    {
        const auto seq = processor.takeRecording();
        if (seq.getNumEvents() == 0) { view.say ("Nothing sung yet"); return; }
        cat::midiexport::dragFileOut (cat::midiexport::writeMidiFile (seq, processor.getHostBpm(), "Life-Cat"), this);
    };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &theme, &running, &newWorldButton, &bigButton, &surpriseControls, &dragButton })
        addAndMakeVisible (c);

    toolPanel.onTool = [this] (GardenTool t) { setTool (t); };
    addAndMakeVisible (toolPanel);
    worldView.onSelectionChanged = [this] { inspector.refresh(); };
    addAndMakeVisible (worldView);
    inspector.onToLab = [this]
    {
        if (const auto* c = view.selectedCreature()) geneLab.loadFrom (*c);
        if (processor.bigView) setBigView (false);
        tabButtons[2]->setToggleState (true, juce::dontSendNotification);
        showTab (2);
    };
    addChildComponent (inspector);
    addAndMakeVisible (graph);
    geneLab.onPlace = [this] { setTool (GardenTool::PlaceLab); };

    // tabs
    const char* tabNames[] = { "SPECIES", "WORLD", "LAB", "MUSIC", "SOUND", "LIFE", "MIDI" };
    for (int i = 0; i < numTabs; ++i)
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
    ownSound.setTooltip ("Life-Cat's own voices (sound shaped by each creature's genes). Turn off to only send MIDI.");
    gravity.getSlider().setTooltip ("Heavy creatures sink to the bottom (an aquarium); fliers and drifters barely notice");
    wind.getSlider().setTooltip ("Blows light creatures along (and you can hear it)");
    dayLength.getCombo().setTooltip ("Day and night: most creatures sleep at night (night owls are awake), the sound gets darker");
    seasonLength.getCombo().setTooltip ("Spring, summer, autumn, winter in turn: plants boom in spring and freeze in winter");
    weather.getCombo().setTooltip ("Rain grows plants, snow and storms are hard, fog hides prey. Auto changes with the seasons.");
    ambience.getSlider().setTooltip ("How loud you hear the weather (rain, wind, snow)");
    dayNightSound.getSlider().setTooltip ("How much night, winter and fog change the sound (darker, more reverb)");
    clearWalls.onClick = [this] { LifeCatProcessor::Command c; c.type = LifeCatProcessor::Command::ClearZones; processor.send (c); };
    clearTerrain.onClick = [this] { LifeCatProcessor::Command c; c.type = LifeCatProcessor::Command::ClearTerrain; processor.send (c); };
    skyLabel.setFont (cat::catFont (12.0f));
    skyLabel.setColour (juce::Label::textColourId, C::textDim);
    skyLabel.setJustificationType (juce::Justification::topLeft);

    midiInfo.setFont (cat::catFont (12.0f));
    midiInfo.setColour (juce::Label::textColourId, C::textDim);
    midiInfo.setJustificationType (juce::Justification::topLeft);
    midiInfo.setText ("MIDI Out sends every note the creatures sing, so you can play any instrument with the ecosystem "
                      "(in Cubase: route this track's MIDI output to another instrument track).\n\n"
                      "One channel per kind: grazers 1, hunters 2, drifters 3, pollinators 4, scavengers 5, giants 6, swarms 7.\n\n"
                      "Notes you play IN are used by the 'Held chord' note mode. Drag MIDI (top right) exports the last 8 bars.",
                      juce::dontSendNotification);

    tabContent[0] = { &speciesPanel };
    tabContent[1] = { &gravity, &wind, &windDir, &dayLength, &seasonLength, &season, &weather, &ambience, &dayNightSound, &clearWalls, &clearTerrain, &skyLabel };
    tabContent[2] = { &geneLab };
    tabContent[3] = { &pitchMode, &key, &scale, lowNote.get(), highNote.get(), &quantize, &grid, &swing, &density, maxNotes.get(),
                      &noteLength, &followGroove, &playStopped };
    tabContent[4] = { &ownSound, &brightness, &attack, &release, &reverbMix, &reverbSize, &delayMix, &delayTime, &delayFb, &width, &volume };
    tabContent[5] = { &lifeSpeed, &foodGrowth, &mutation, &hunger, &breeding, &lifespan, maxPop.get(), &immigration };
    tabContent[6] = { &midiOut, &midiChannels, &midiInfo };
    for (auto& list : tabContent)
        for (auto* c : list) addChildComponent (c);
    tabButtons[0]->setToggleState (true, juce::dontSendNotification);

    keyboard.setColourForChannel ([this] (int ch)
    {
        return ch >= 1 && ch <= numRoles ? lifeart::roleColour (view.theme, (Role) (ch - 1)) : C::accent;
    });
    addAndMakeVisible (keyboard);
    addAndMakeVisible (catView);
    addAndMakeVisible (bootScreen);

    setSize (1280, 820);
    showTab (0);
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
    toolPanel.repaint();
    worldView.repaint();
}

void LifeCatEditor::setBigView (bool on)
{
    processor.bigView = on;
    bigButton.setToggleState (on, juce::dontSendNotification);
    showTab (currentTab);
}

void LifeCatEditor::showTab (int i)
{
    currentTab = juce::jlimit (0, numTabs - 1, i);
    const bool panel = ! processor.bigView;
    for (int k = 0; k < numTabs; ++k)
    {
        tabButtons[k]->setVisible (panel);
        for (auto* c : tabContent[(size_t) k]) c->setVisible (panel && k == currentTab);
    }
    graph.setVisible (panel);
    resized();
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
        c.counts = list[(size_t) pi].counts;
        c.plants = list[(size_t) pi].plants;
        c.layout = list[(size_t) pi].layout;
    }
    processor.send (c);
    view.selected = 0;
    view.pairing = false;
    view.follow = false;
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
        toolPanel.updateNames();
        geneLab.resized();
        speciesPanel.repaint();
        keyboard.repaint();
    }

    worldView.tick();
    worldView.repaint();
    ++tickCount;
    if (tickCount % 3 == 0)
    {
        inspector.refresh();
        graph.repaint();
        const auto& w = view.world;
        juce::String sky = "Now:  " + lifeart::seasonName (w.season);
        if (processor.apvts.getRawParameterValue ("dayLength")->load() > 0.5f)
            sky << ",  " << lifeart::clockText (w.dayPhase) << (w.light < 0.3f ? " (night)" : (w.light < 0.9f ? " (twilight)" : " (day)"));
        sky << ",  " << lifeart::weatherName (w.weather) << "\nPlants " << w.plantCount() << ",  remains " << w.corpseCount();
        skyLabel.setText (sky, juce::dontSendNotification);
    }
    if (tickCount % 15 == 0 && currentTab == 0) speciesPanel.repaint();
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
        row (top, { item (presetBar, 310), item (theme, 160), item (running, 56), item (newWorldButton, 92), item (bigButton, 80),
                    juce::FlexItem().withFlex (1.0f), item (surpriseControls, 122), item (dragButton, 92) });
    }
    r.removeFromTop (8);
    keyboard.setBounds (r.removeFromBottom (40));
    r.removeFromBottom (8);

    toolPanel.setBounds (r.removeFromLeft (104));
    r.removeFromLeft (8);

    if (processor.bigView)
    {
        worldView.setBounds (r);
    }
    else
    {
        const int side = juce::jmin (r.getHeight(), r.getWidth() - 440);
        worldView.setBounds (r.removeFromLeft (side).withSizeKeepingCentre (side, side));
        r.removeFromLeft (10);

        auto tabs = r.removeFromTop (28);
        for (int i = 0; i < numTabs; ++i) tabButtons[i]->setBounds (tabs.removeFromLeft (tabs.getWidth() / (numTabs - i)).reduced (1, 0));
        r.removeFromTop (8);
        graph.setBounds (r.removeFromBottom (130));
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
            case 0: speciesPanel.setBounds (content); break;
            case 1:
                two (gravity, wind);
                windDir.setBounds (line().removeFromLeft (half).reduced (2, 1));
                two (dayLength, seasonLength);
                two (season, weather);
                two (ambience, dayNightSound);
                two (clearWalls, clearTerrain);
                content.removeFromTop (6);
                skyLabel.setBounds (content.removeFromTop (40).reduced (4, 0));
                break;
            case 2: geneLab.setBounds (content); break;
            case 3:
                pitchMode.setBounds (line().reduced (2, 1));
                two (key, scale);
                two (*lowNote, *highNote);
                two (quantize, grid);
                two (swing, density);
                two (*maxNotes, noteLength);
                two (followGroove, playStopped);
                break;
            case 4:
                two (ownSound, brightness);
                two (attack, release);
                two (reverbMix, reverbSize);
                two (delayMix, delayTime);
                two (delayFb, width);
                volume.setBounds (line().removeFromLeft (half).reduced (2, 1));
                break;
            case 5:
                two (lifeSpeed, foodGrowth);
                two (mutation, hunger);
                two (breeding, lifespan);
                two (*maxPop, immigration);
                break;
            default:
                two (midiOut, midiChannels);
                midiInfo.setBounds (content.reduced (2));
                break;
        }
    }

    const auto wb = worldView.getBounds();
    inspector.setBounds (juce::Rectangle<int> (wb.getRight() - 340, wb.getY() + 30, 330, juce::jmin (360, wb.getHeight() - 80)));
    bootScreen.setBounds (getLocalBounds());
}

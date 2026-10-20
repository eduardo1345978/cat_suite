#include "OceanArt.h"
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <algorithm>

namespace ocean
{
namespace
{
    constexpr float pi = 3.14159265f;

    inline int cr (Colour c) noexcept { return (int) ((c >> 16) & 255); }
    inline int cg (Colour c) noexcept { return (int) ((c >> 8) & 255); }
    inline int cb (Colour c) noexcept { return (int) (c & 255); }
    inline Colour rgb (int r, int g, int b) noexcept
    {
        return 0xff000000u | ((Colour) juce::jlimit (0, 255, r) << 16) | ((Colour) juce::jlimit (0, 255, g) << 8) | (Colour) juce::jlimit (0, 255, b);
    }
    inline Colour mixc (Colour a, Colour b, float t) noexcept
    {
        return rgb ((int) (cr (a) + (cr (b) - cr (a)) * t), (int) (cg (a) + (cg (b) - cg (a)) * t), (int) (cb (a) + (cb (b) - cb (a)) * t));
    }
    inline Colour shade (Colour c, float k) noexcept { return rgb ((int) (cr (c) * k), (int) (cg (c) * k), (int) (cb (c) * k)); }
    inline Colour light (Colour c, float t) noexcept { return mixc (c, 0xffffffffu, t); }
    inline Colour hsv (float h, float s, float v) noexcept
    {
        h = h - std::floor (h);
        const float i = std::floor (h * 6.0f), f = h * 6.0f - i, p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
        float r, g, b;
        switch ((int) i % 6) { case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break;
                               case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; break; }
        return rgb ((int) (r * 255), (int) (g * 255), (int) (b * 255));
    }
    inline juce::uint32 hash (juce::uint32 x) noexcept
    {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
    }
    inline float hashf (juce::uint32 x) noexcept { return (float) (hash (x) & 0xffffff) / 16777216.0f; }

    const int bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

    // 3 x 5 font: 5 rows of 3 bits
    juce::uint16 glyph (juce::juce_wchar c) noexcept
    {
        auto g = [] (int a, int b, int c2, int d, int e) { return (juce::uint16) ((a << 12) | (b << 9) | (c2 << 6) | (d << 3) | e); };
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        switch (c)
        {
            case 'A': return g (2, 5, 7, 5, 5);  case 'B': return g (6, 5, 6, 5, 6);  case 'C': return g (3, 4, 4, 4, 3);
            case 'D': return g (6, 5, 5, 5, 6);  case 'E': return g (7, 4, 6, 4, 7);  case 'F': return g (7, 4, 6, 4, 4);
            case 'G': return g (3, 4, 5, 5, 3);  case 'H': return g (5, 5, 7, 5, 5);  case 'I': return g (7, 2, 2, 2, 7);
            case 'J': return g (1, 1, 1, 5, 2);  case 'K': return g (5, 5, 6, 5, 5);  case 'L': return g (4, 4, 4, 4, 7);
            case 'M': return g (5, 7, 7, 5, 5);  case 'N': return g (6, 5, 5, 5, 5);  case 'O': return g (2, 5, 5, 5, 2);
            case 'P': return g (6, 5, 6, 4, 4);  case 'Q': return g (2, 5, 5, 6, 3);  case 'R': return g (6, 5, 6, 5, 5);
            case 'S': return g (3, 4, 2, 1, 6);  case 'T': return g (7, 2, 2, 2, 2);  case 'U': return g (5, 5, 5, 5, 7);
            case 'V': return g (5, 5, 5, 5, 2);  case 'W': return g (5, 5, 7, 7, 5);  case 'X': return g (5, 5, 2, 5, 5);
            case 'Y': return g (5, 5, 2, 2, 2);  case 'Z': return g (7, 1, 2, 4, 7);
            case '0': return g (7, 5, 5, 5, 7);  case '1': return g (2, 6, 2, 2, 7);  case '2': return g (6, 1, 2, 4, 7);
            case '3': return g (6, 1, 2, 1, 6);  case '4': return g (5, 5, 7, 1, 1);  case '5': return g (7, 4, 6, 1, 6);
            case '6': return g (3, 4, 6, 5, 2);  case '7': return g (7, 1, 2, 2, 2);  case '8': return g (2, 5, 2, 5, 2);
            case '9': return g (2, 5, 3, 1, 6);
            case '!': return g (2, 2, 2, 0, 2);  case '.': return g (0, 0, 0, 0, 2);  case ',': return g (0, 0, 0, 2, 4);
            case ':': return g (0, 2, 0, 2, 0);  case '-': return g (0, 0, 7, 0, 0);  case '+': return g (0, 2, 7, 2, 0);
            case '\'': return g (2, 2, 0, 0, 0); case '#': return g (5, 7, 5, 7, 5);  case '?': return g (6, 1, 2, 0, 2);
            case '/': return g (1, 1, 2, 4, 4);  case '(': return g (1, 2, 2, 2, 1);  case ')': return g (4, 2, 2, 2, 4);
            case '&': return g (2, 5, 2, 5, 3);  case '_': return g (0, 0, 0, 0, 7);  case '%': return g (5, 1, 2, 4, 5);
            case '"': return g (5, 5, 0, 0, 0);  case '=': return g (0, 7, 0, 7, 0);  case '*': return g (0, 5, 2, 5, 0);
            case '@': return g (2, 5, 7, 4, 3);  case '>': return g (4, 2, 1, 2, 4);  case '<': return g (1, 2, 4, 2, 1);
            case '[': return g (3, 2, 2, 2, 3);  case ']': return g (6, 2, 2, 2, 6);
            default: return 0;
        }
    }
}

//==============================================================================
void Canvas::blend (int x, int y, Colour c, float a) noexcept
{
    if ((unsigned) x >= (unsigned) W || (unsigned) y >= (unsigned) H || a <= 0.0f) return;
    auto& p = pixels[(size_t) (y * W + x)];
    p = a >= 1.0f ? (c | 0xff000000u) : mixc (p, c, a);
}

void Canvas::add (int x, int y, Colour c, float k) noexcept
{
    if ((unsigned) x >= (unsigned) W || (unsigned) y >= (unsigned) H || k <= 0.0f) return;
    auto& p = pixels[(size_t) (y * W + x)];
    p = rgb (cr (p) + (int) (cr (c) * k), cg (p) + (int) (cg (c) * k), cb (p) + (int) (cb (c) * k));
}

void Canvas::fillRect (int x, int y, int w, int h, Colour c, float a) noexcept
{
    for (int j = juce::jmax (0, y); j < juce::jmin (H, y + h); ++j)
        for (int i = juce::jmax (0, x); i < juce::jmin (W, x + w); ++i)
            blend (i, j, c, a);
}

int Canvas::textWidth (const juce::String& s, int scale) noexcept { return s.length() * 4 * scale - (s.isEmpty() ? 0 : scale); }

int Canvas::text (const juce::String& s, int x, int y, Colour c, int scale, Colour outline, bool withOutline) noexcept
{
    auto drawGlyphs = [&] (int ox, int oy, Colour col)
    {
        int cx = x + ox;
        for (auto ch : s)
        {
            const auto gl = glyph (ch);
            for (int row = 0; row < 5; ++row)
                for (int col2 = 0; col2 < 3; ++col2)
                    if (gl & (1 << ((4 - row) * 3 + (2 - col2))))
                        for (int a = 0; a < scale; ++a)
                            for (int b = 0; b < scale; ++b)
                                set (cx + col2 * scale + a, y + oy + row * scale + b, col);
            cx += 4 * scale;
        }
    };
    if (withOutline)
        for (int oy = -1; oy <= 1; ++oy)
            for (int ox = -1; ox <= 1; ++ox)
                if (ox != 0 || oy != 0) drawGlyphs (ox, oy, outline);
    drawGlyphs (0, 0, c);
    return textWidth (s, scale);
}

//==============================================================================
namespace
{
    // water (top, middle, deep), sand (top, bottom) and rocks for every place
    struct PlaceLook { Colour top, mid, deep, sandTop, sandLow, rock; float rays; };
    const PlaceLook& look (int place)
    {
        static const PlaceLook looks[NumPlaces] = {
            { 0xff52e3ea, 0xff1c9fd6, 0xff0b4f93, 0xfff6ead0, 0xffd6bd8c, 0xff7aa6c4, 1.3f },   // aqua reef
            { 0xff74eab8, 0xff1fa58a, 0xff0b5a5c, 0xffe8efc0, 0xffb4c482, 0xff6fa59a, 1.1f },   // emerald kelp
            { 0xff2d8fd0, 0xff124c8e, 0xff051c40, 0xff3c5c94, 0xff1a2c58, 0xff2f4c80, 0.0f },   // crystal deep
            { 0xff5fd6de, 0xff2589b0, 0xff0f4672, 0xfff0dfb6, 0xffc4a878, 0xff6f90ae, 0.8f },   // sunken ship
            { 0xff6a5fd0, 0xff35307a, 0xff120f33, 0xff8a6fa8, 0xff3d2f5c, 0xff4a3d6e, 0.0f },   // coral cave
            { 0xffbff4ff, 0xff5fbfe6, 0xff1d5c99, 0xfff4fbff, 0xffc6dbe8, 0xff9fb8cc, 1.0f },   // ice ocean
            { 0xff6ff2e0, 0xff22c4cc, 0xff0f7aa0, 0xfffff4d6, 0xffefd9a4, 0xff9ab6a0, 1.5f },   // tropical lagoon
            { 0xff4fc8d6, 0xff1f7fa8, 0xff0d3f66, 0xffe8dcc0, 0xffb9a77e, 0xffc9c2b0, 0.9f },   // sunken ruins
            { 0xff6fa9b8, 0xff2f5568, 0xff141c28, 0xff5a5052, 0xff2a2426, 0xff3c3436, 0.4f },   // volcano vents: warm murky water, ash
            { 0xff8fe4ff, 0xff4a7fd8, 0xff1d236e, 0xffe2d8f6, 0xffaa9ccc, 0xff8e84bc, 1.0f },   // jellyfish bay: lavender sand
            { 0xff40d4ff, 0xff1572cc, 0xff051f58, 0xff1c4a86, 0xff0b2552, 0xff2a5a8e, 1.6f },   // open sea: no beach, just blue
        };
        return looks[juce::jlimit (0, NumPlaces - 1, place)];
    }
}

Painter::Painter()
{
    // the sea floor: rocks, coral, seaweed (the same every time, one set per place)
    juce::Random r (77);
    for (int i = 0; i < 9; ++i)
        rocks.push_back ({ 10 + r.nextInt (W - 20), 10 + r.nextInt (22), 5 + r.nextInt (9), 0 });
    for (int i = 0; i < 26; ++i)
    {
        const bool kelp = r.nextFloat() < 0.6f;
        weedSets[PlaceReef].push_back ({ 6.0f + r.nextFloat() * (W - 12), kelp ? 22 + r.nextInt (40) : 6 + r.nextInt (10),
                                         kelp ? mixc (0xff2f7a3d, 0xff58b04a, r.nextFloat()) : mixc (0xfff6757a, 0xffff9a5c, r.nextFloat()),
                                         r.nextFloat() * 6.0f });
    }
    for (int i = 0; i < 44; ++i)   // a forest of tall kelp
    {
        const bool tall = i < 34;
        weedSets[PlaceKelp].push_back ({ 4.0f + r.nextFloat() * (W - 8), tall ? 70 + r.nextInt (120) : 8 + r.nextInt (14),
                                         mixc (0xff3f7a2a, 0xff8fb04a, r.nextFloat()), r.nextFloat() * 6.0f });
    }
    for (int i = 0; i < 16; ++i)   // glowing plants in the dark
    {
        static const Colour glows[] = { 0xff2ce8f5, 0xffc06bff, 0xff7dffb0, 0xffff7ad9 };
        weedSets[PlaceDeep].push_back ({ 6.0f + r.nextFloat() * (W - 12), 8 + r.nextInt (22), glows[r.nextInt (4)], r.nextFloat() * 6.0f, true });
    }
    for (int i = 0; i < 14; ++i)   // glowing crystals and corals in the cave
    {
        static const Colour cave[] = { 0xffff7ad9, 0xffc06bff, 0xff2ce8f5, 0xffffb36b };
        weedSets[PlaceCave].push_back ({ 6.0f + r.nextFloat() * (W - 12), 6 + r.nextInt (16), cave[r.nextInt (4)], r.nextFloat() * 6.0f, i % 2 == 0 });
    }
    for (int i = 0; i < 6; ++i)    // a little pale sea grass under the ice
        weedSets[PlaceIce].push_back ({ 6.0f + r.nextFloat() * (W - 12), 6 + r.nextInt (12), mixc (0xff8fd9c0, 0xffd8f8ee, r.nextFloat()), r.nextFloat() * 6.0f });
    for (int i = 0; i < 30; ++i)   // bright corals and sea grass in the lagoon
    {
        static const Colour lag[] = { 0xffff6b8a, 0xffffb347, 0xffb06bff, 0xff4fd17a, 0xff5fe3ff };
        weedSets[PlaceLagoon].push_back ({ 6.0f + r.nextFloat() * (W - 12), 6 + r.nextInt (18), lag[r.nextInt (5)], r.nextFloat() * 6.0f });
    }
    for (int i = 0; i < 16; ++i)
        weedSets[PlaceRuins].push_back ({ 6.0f + r.nextFloat() * (W - 12), 10 + r.nextInt (34), mixc (0xff3f8a4a, 0xff9ab04a, r.nextFloat()), r.nextFloat() * 6.0f });
    for (int i = 0; i < 18; ++i)
        weedSets[PlaceWreck].push_back ({ 6.0f + r.nextFloat() * (W - 12), 10 + r.nextInt (30), mixc (0xff4d6b2a, 0xff8a8a3a, r.nextFloat()), r.nextFloat() * 6.0f });
    for (int i = 0; i < 10; ++i)   // dark, tough weeds near the vents
        weedSets[PlaceVolcano].push_back ({ 6.0f + r.nextFloat() * (W - 12), 6 + r.nextInt (12), mixc (0xff4a5a2a, 0xff7a6a3a, r.nextFloat()), r.nextFloat() * 6.0f });
    for (int i = 0; i < 14; ++i)   // pale glowing anemones in the jellyfish bay
        weedSets[PlaceJelly].push_back ({ 6.0f + r.nextFloat() * (W - 12), 5 + r.nextInt (9), mixc (0xffffb6e6, 0xffc8b6ff, r.nextFloat()), r.nextFloat() * 6.0f, i % 3 == 0 });
    sandNoise.resize ((size_t) W);
    for (auto& n : sandNoise) n = r.nextInt (1000);
}

void Painter::paint (Canvas& c, const Sim& sim, const MusicInfo& music, float dt)
{
    clock += dt;
    const float t = clock;
    background (c, sim, t);
    if (sim.place != PlaceCave) sky (c, sim, t);
    decor (c, sim, t);
    school (c, sim, t);
    if (sim.place != PlaceCave) boat (c, sim, t);

    // food
    for (auto& p : sim.food)
    {
        const int x = (int) p.x, y = (int) p.y;
        c.set (x, y, 0xffc9852b); c.set (x + 1, y, 0xffa86a1e); c.set (x, y + 1, 0xffa86a1e); c.set (x + 1, y + 1, 0xff7c4c16);
        c.set (x, y, 0xfff0b860);
    }

    // fish, small ones in front
    std::vector<const Fish*> order;
    for (auto& f : sim.fish) if (f.present) order.push_back (&f);
    std::sort (order.begin(), order.end(), [] (auto* a, auto* b) { return a->genes.lengthPixels() * a->growth > b->genes.lengthPixels() * b->growth; });
    for (auto* f : order) drawFish (c, *f, (float) sim.time);
    if (sim.place == PlaceDeep || sim.place == PlaceCave || sim.daylight < 0.4f)
        for (auto* f : order)   // in the dark, every fish glows a little
            for (int y = -8; y <= 8; ++y)
                for (int x = -8; x <= 8; ++x)
                {
                    const float d = std::sqrt ((float) (x * x + y * y)) / 8.0f;
                    if (d < 1.0f) c.add ((int) f->x + x, (int) f->y + y, colourARGB (f->genes.colour1), 0.10f * (1.0f - d) * f->fade);
                }

    glow (c, sim, t);
    bubbles (c, sim, dt);
    // little light sparkles drifting in the water
    for (int i = 0; i < 18; ++i)
    {
        const float phase = std::sin (t * 2.2f + i * 1.7f);
        if (phase < 0.75f) continue;
        const int sx = (int) (hashf ((juce::uint32) i * 41 + 9) * W + std::sin (t * 0.3f + i) * 12.0f);
        const int sy = (int) (surfaceY + 8.0f + hashf ((juce::uint32) i * 23 + 4) * 120.0f);
        const float a = (phase - 0.75f) * 4.0f * (1.0f - sim.weatherLevel);
        c.blend (sx, sy, 0xffffffff, a); c.blend (sx - 1, sy, 0xffffffff, a * 0.5f); c.blend (sx + 1, sy, 0xffffffff, a * 0.5f);
        c.blend (sx, sy - 1, 0xffffffff, a * 0.5f); c.blend (sx, sy + 1, 0xffffffff, a * 0.5f);
    }
    floor (c, sim, t);
    weather (c, sim, t);

    // what the fish say, then names: labels never cover each other (a crowd shows the
    // viewers' names first; other names only where there is room)
    std::vector<juce::Rectangle<int>> used;
    auto place = [&] (const juce::String& text, int cx, int y, Colour col, bool always)
    {
        const int w = Canvas::textWidth (text);
        juce::Rectangle<int> r (juce::jlimit (2, W - 2 - w, cx - w / 2) - 1, y - 1, w + 2, 7);
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            bool free = r.getY() > 40;
            for (auto& u : used) free = free && ! u.intersects (r);
            if (free) { used.push_back (r); c.text (text, r.getX() + 1, r.getY() + 1, col); return; }
            r.translate (0, -7);
        }
        if (always) { r.translate (0, 14); used.push_back (r); c.text (text, r.getX() + 1, juce::jmax (41, r.getY() + 1), col); }
    };
    for (auto& sp : sim.speech)
    {
        const auto* f = sim.findFish (sp.fishId);
        if (f == nullptr || ! f->present) continue;
        const auto text = sp.text.toUpperCase();
        const int w = Canvas::textWidth (text) + 6, h = 9;
        const float len = f->genes.lengthPixels() * (0.5f + 0.5f * f->growth);
        juce::Rectangle<int> r (juce::jlimit (2, W - 2 - w, (int) f->x - w / 2), juce::jmax (42, (int) (f->y - len * 0.45f) - 22), w, h);
        for (int tries = 0; tries < 3; ++tries)
        {
            bool free = true;
            for (auto& u : used) free = free && ! u.intersects (r);
            if (free) break;
            r.translate (0, -9);
        }
        used.push_back (r);
        const float a = juce::jmin (1.0f, sp.life);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const bool corner = (x == 0 || x == w - 1) && (y == 0 || y == h - 1);
                if (corner) continue;
                const bool edge = x == 0 || x == w - 1 || y == 0 || y == h - 1;
                c.blend (r.getX() + x, r.getY() + y, edge ? 0xff6fc3ec : (y < h / 2 ? 0xffffffff : 0xffe8f8ff), a * (edge ? 0.9f : 0.92f));
            }
        // the little tail pointing at the fish
        const int tx = juce::jlimit (r.getX() + 2, r.getRight() - 3, (int) f->x);
        c.blend (tx, r.getBottom(), 0xffffffff, a); c.blend (tx + 1, r.getBottom(), 0xff6fc3ec, a); c.blend (tx, r.getBottom() + 1, 0xff6fc3ec, a);
        c.text (text, r.getX() + 3, r.getY() + 2, 0xff0b2f55, 1, 0, false);
    }
    for (auto& tst : sim.toasts)
    {
        const auto* f = sim.findFish (tst.fishId);
        const float len = f != nullptr ? f->genes.lengthPixels() * (0.5f + 0.5f * f->growth) : 14.0f;
        const int rise = (int) ((4.0f - tst.life) * 2.0f);
        place (tst.text.toUpperCase().substring (0, 40), (int) tst.x, (int) (tst.y - len * 0.45f) - 17 - rise, tst.colour, true);
    }
    for (int pass = 0; pass < 2; ++pass)
        for (auto* f : order)
        {
            if (f->dying || f->name.isEmpty() || f->viewer() != (pass == 0)) continue;
            // wild named fish (babies of viewers' fish) only show their name for a while after they're born
            if (! f->viewer() && sim.time - f->bornAt > 30.0) continue;
            const float len = f->genes.lengthPixels() * (0.5f + 0.5f * f->growth);
            const Colour col = f->genes.rare != RareNone ? 0xffffd34d : f->viewer() ? 0xffffffff : 0xff9ef0c2;
            const auto label = f->viewer() ? f->name.toUpperCase() + " LV" + juce::String (f->level()) : f->name.toUpperCase();
            place (label, (int) f->x, (int) (f->y - len * 0.45f) - 9, col, pass == 0);
        }

    // confetti (new subscribers, new species)
    for (auto& cf : sim.confetti)
    {
        const float a = juce::jmin (1.0f, cf.life);
        c.blend ((int) cf.x, (int) cf.y, cf.colour, a);
        c.blend ((int) cf.x + (((int) (cf.life * 8.0f)) & 1), (int) cf.y + 1, cf.colour, a);
    }

    // moving to a new place: the picture fades out and in
    if (sim.placeFade > 0.0f)
    {
        c.fillRect (0, 0, W, H, 0xff02040a, juce::jmin (0.95f, sim.placeFade));
        if (sim.nextPlace >= 0)
        {
            const auto text = "SWIMMING TO THE " + placeName (sim.nextPlace).toUpperCase() + " ...";
            c.text (text, W / 2 - Canvas::textWidth (text, 2) / 2, H / 2 - 5, 0xff2ce8f5, 2);
        }
    }

    overlay (c, sim, music, t);
}

//==============================================================================
void Painter::background (Canvas& c, const Sim& sim, float t)
{
    const float storm = sim.weatherLevel;
    // day and night from the real clock: blue sky, a pink-orange sunset, a starry night
    const float day = juce::jlimit (0.0f, 1.0f, sim.daylight), dusk = day > 0.0f && day < 1.0f ? 1.0f - std::abs (2.0f * day - 1.0f) : 0.0f;
    Colour skyTop = mixc (0xff070b26, 0xff2b8ff0, day), skyLow = mixc (0xff1d2a5c, 0xffc8f0ff, day);
    skyTop = mixc (skyTop, 0xff4b4fa8, dusk * 0.7f);
    skyLow = mixc (skyLow, 0xffff9f7a, dusk * 0.8f);
    skyTop = mixc (skyTop, shade (0xff2e3442, 0.4f + 0.6f * day), storm);
    skyLow = mixc (skyLow, shade (0xff5d6676, 0.4f + 0.6f * day), storm);
    const auto& lk = look (sim.place);
    const float darkWater = (1.0f - day) * 0.75f;
    // at night the water turns a deep moonlit blue (not just a darker, greyer version of the day colour)
    auto nightOf = [] (Colour c, Colour moon, float keep) { return mixc (moon, shade (c, 0.35f), keep); };
    const Colour waterTop = mixc (mixc (lk.top, shade (lk.top, 0.55f), storm), nightOf (lk.top, 0xff1a3c78, 0.35f), darkWater),
                 waterMid = mixc (mixc (lk.mid, shade (lk.mid, 0.55f), storm), nightOf (lk.mid, 0xff0e2654, 0.35f), darkWater),
                 waterDeep = mixc (mixc (lk.deep, shade (lk.deep, 0.5f), storm), nightOf (lk.deep, 0xff050f2a, 0.4f), darkWater);

    for (int x = 0; x < W; ++x)
    {
        const float surf = surfaceY + std::sin (x * 0.07f + t * 1.4f) * (1.0f + storm * 2.5f) + std::sin (x * 0.19f - t * 2.1f) * 0.6f;
        for (int y = 0; y < H; ++y)
        {
            const float d = bayer[y & 3][x & 3] / 16.0f;
            if (y < surf)
            {
                const float k = std::floor ((float) y / surfaceY * 8.0f + d) / 8.0f;
                c.set (x, y, mixc (skyTop, skyLow, juce::jlimit (0.0f, 1.0f, k)));
            }
            else
            {
                const float depth = (y - surfaceY) / (H - surfaceY);
                const float k = juce::jlimit (0.0f, 1.0f, std::floor (depth * 12.0f + d) / 12.0f);
                c.set (x, y, k < 0.5f ? mixc (waterTop, waterMid, k * 2.0f) : mixc (waterMid, waterDeep, (k - 0.5f) * 2.0f));
                if (y < (int) surf + 1) c.blend (x, y, 0xffffffff, 0.55f);   // foam line
            }
        }
    }

    // stars and the moon at night
    if (day < 1.0f)
    {
        const float a = (1.0f - day) * (1.0f - storm);
        for (int i = 0; i < 70; ++i)
        {
            const int sx = (int) (hashf ((juce::uint32) i * 19 + 11) * W), sy = (int) (hashf ((juce::uint32) i * 7 + 3) * (surfaceY - 8.0f));
            const float tw = 0.6f + 0.4f * std::sin (t * (1.0f + (i % 5) * 0.4f) + i);
            c.blend (sx, sy, 0xffffffff, a * tw);
        }
        for (int y = -7; y <= 7; ++y)
            for (int x = -7; x <= 7; ++x)
            {
                const int r2 = x * x + y * y;
                if (r2 > 49) continue;
                const bool shadow = (x + 3) * (x + 3) + (y - 2) * (y - 2) < 30;   // a crescent
                if (! shadow) c.blend (250 + x, 14 + y, 0xfff2f4ff, a);
            }
    }
    // the sun (sinks lower at sunrise / sunset)
    const int sunY = 15 + (int) ((1.0f - day) * 24.0f);
    if (storm < 0.6f && day > 0.0f)
    {
        const float a = (1.0f - storm / 0.6f) * juce::jmin (1.0f, day * 2.0f);
        const Colour sunCol = mixc (0xffff9a4a, 0xfffff2a8, day);
        for (int y = -9; y <= 9; ++y)
            for (int x = -9; x <= 9; ++x)
                if (sunY + y >= (int) surfaceY - 1) continue;
                else if (x * x + y * y <= 64) c.blend (420 + x, sunY + y, sunCol, a);
                else if (x * x + y * y <= 81) c.blend (420 + x, sunY + y, sunCol, a * 0.35f);
    }
    // lens flare from the sun (Frutiger Aero!)
    if (storm < 0.6f && day > 0.5f)
    {
        const float a = (1.0f - storm / 0.6f) * (day - 0.5f) * 2.0f;
        static const Colour flare[] = { 0xff9ff3ff, 0xfffff2a8, 0xffffb6e6, 0xffb6ffcf };
        for (int k = 1; k <= 4; ++k)
        {
            const int fx = 420 - k * 46, fy = 15 + k * 30, r = 3 + (k % 2) * 3;
            for (int y = -r; y <= r; ++y)
                for (int x = -r; x <= r; ++x)
                {
                    const float d = std::sqrt ((float) (x * x + y * y)) / (float) r;
                    if (d <= 1.0f) c.add (fx + x, fy + y, flare[k - 1], a * 0.10f * (d > 0.7f ? 1.6f : 0.8f));
                }
        }
    }
    // islands on the horizon: green, snowy at the ice ocean, with palm trees at the lagoon
    const bool snowy = sim.place == PlaceIce;
    for (int x = 0; x < W; ++x)
    {
        const float hill = 5.0f + 4.0f * std::sin (x * 0.035f + 0.6f) + 2.5f * std::sin (x * 0.09f);
        if (hill < 3.0f) continue;
        const int top = (int) (surfaceY - hill);
        for (int y = top; y < (int) surfaceY - 1; ++y)
        {
            const float v = (float) (y - top) / juce::jmax (1.0f, hill);
            Colour g = snowy ? mixc (0xfff4fbff, 0xffaac8de, v) : mixc (0xff7ee05a, 0xff3c9a3c, v);
            g = shade (mixc (g, 0xff59707a, storm), 0.35f + 0.65f * day);
            c.set (x, y, y == top ? light (g, 0.4f * day) : g);
        }
    }
    if (sim.place == PlaceLagoon)
        for (int p = 0; p < 6; ++p)
        {
            const int px = 30 + p * 83 + (p % 2) * 17;
            const float hill = 5.0f + 4.0f * std::sin (px * 0.035f + 0.6f) + 2.5f * std::sin (px * 0.09f);
            if (hill < 4.0f) continue;
            const int base = (int) (surfaceY - hill);
            const Colour trunk = shade (0xff9a6a34, 0.4f + 0.6f * day), leaf = shade (0xff3fbf4a, 0.4f + 0.6f * day);
            for (int k = 0; k < 12; ++k) c.set (px + k / 5, base - k, trunk);
            const int tx = px + 2, ty = base - 12;
            for (int k = 1; k <= 6; ++k)
            {
                c.set (tx + k, ty + k / 2, leaf); c.set (tx - k, ty + k / 2, leaf);
                c.set (tx + k / 2, ty - 1 + k / 3, leaf); c.set (tx - k / 2, ty - 1 + k / 3, leaf);
            }
        }
    // clouds
    const int clouds = 3 + (int) (storm * 6);
    for (int i = 0; i < clouds; ++i)
    {
        const float speed = 3.0f + hashf ((juce::uint32) i * 7 + 1) * 4.0f;
        const int cx = (int) std::fmod (hashf ((juce::uint32) i * 13 + 5) * (W + 80) + t * speed, (float) (W + 80)) - 40;
        const int cy = 6 + (int) (hashf ((juce::uint32) i * 3 + 2) * 20);
        const Colour col = shade (mixc (mixc (0xfff4f8ff, 0xffffc2a8, dusk * 0.6f), 0xff7a8290, storm), 0.35f + 0.65f * day);
        for (int b = 0; b < 4; ++b)
        {
            const int bx = cx + b * 7 - 10, by = cy - (b == 1 || b == 2 ? 3 : 0), r = b == 1 || b == 2 ? 6 : 4;
            for (int y = -r; y <= r; ++y)
                for (int x = -r - 2; x <= r + 2; ++x)
                    if (x * x * 0.6f + y * y * 1.4f <= r * r && by + y < surfaceY - 2) c.set (bx + x, by + y, y > r / 2 ? shade (col, 0.85f) : col);
        }
    }

    // above the water: a rainbow after a shower, birds on calm days, lightning in storms
    if (sim.weatherLevel > 0.35f) lastRainAt = t;
    {
        const float since = t - lastRainAt;
        if (since > 2.0f && since < 150.0f && day > 0.45f && storm < 0.3f)
        {
            const float a = juce::jmin (1.0f, (since - 2.0f) / 10.0f) * juce::jmin (1.0f, (150.0f - since) / 30.0f) * 0.35f * day;
            static const Colour bands[] = { 0xffff5a5a, 0xffffa64a, 0xfffff05a, 0xff5aff7a, 0xff5ab4ff, 0xffa05aff };
            const float cx = W * 0.32f, cy = surfaceY + 26.0f;
            for (int y = 0; y < (int) surfaceY - 1; ++y)
                for (int x = 0; x < W; ++x)
                {
                    const float d = std::sqrt ((x - cx) * (x - cx) + (y - cy) * (y - cy) * 1.6f);
                    const int band = (int) (68.0f - d);
                    if (band >= 0 && band < 6) c.blend (x, y, bands[band], a);
                }
        }
    }
    if (day > 0.35f && storm < 0.5f && sim.place != PlaceCave)
        for (int i = 0; i < 5; ++i)
        {
            const float speed = 9.0f + hashf ((juce::uint32) i * 31 + 7) * 8.0f;
            const int bx = (int) std::fmod (hashf ((juce::uint32) i * 17 + 3) * (W + 60) + t * speed + i * 40.0f, (float) (W + 60)) - 30;
            const int by = 6 + (int) (hashf ((juce::uint32) i * 5 + 9) * 16.0f + 2.0f * std::sin (t * 0.7f + i));
            const int flap = std::sin (t * 9.0f + i * 1.3f) > 0.0f ? -1 : 0;
            const Colour bc = shade (0xff2a3442, 0.6f + 0.4f * day);
            c.set (bx, by, bc); c.set (bx - 1, by - 1 - flap, bc); c.set (bx + 1, by - 1 - flap, bc);
            c.set (bx - 2, by - 1 - 2 * flap, bc); c.set (bx + 2, by - 1 - 2 * flap, bc);
        }
    if (storm > 0.6f && sim.place != PlaceCave)
    {
        const float slot = std::floor (t * 1.1f), frac = t * 1.1f - slot;
        const auto h = hash ((juce::uint32) slot * 2654435761u);
        if (h % 6 == 0 && frac < 0.12f)
        {
            const float a = (1.0f - frac / 0.12f) * (storm - 0.6f) * 2.5f;
            for (int y = 0; y < (int) surfaceY; ++y) for (int x = 0; x < W; ++x) c.add (x, y, 0xffd8e4ff, a * 0.35f);
            int lx = 40 + (int) (h % (juce::uint32) (W - 80));
            for (int y = 2; y < (int) surfaceY - 1; ++y)
            {
                if (y % 4 == 0) lx += (int) ((h >> (y % 16)) % 3) - 1;
                c.blend (lx, y, 0xffffffff, a); c.blend (lx + 1, y, 0xffcfe0ff, a * 0.6f);
            }
        }
    }

    // sun rays in calm water
    if (storm < 0.8f && lk.rays > 0.0f && day > 0.2f)
    {
        const float a = 0.07f * lk.rays * (1.0f - storm / 0.8f) * day;
        for (int y = (int) surfaceY + 3; y < (int) floorY; ++y)
        {
            const float depth = (y - surfaceY) / (floorY - surfaceY);
            for (int x = 0; x < W; ++x)
            {
                const float p = std::fmod (x + y * 0.55f + t * 5.0f + 1000.0f, 96.0f);
                if (p < 16.0f) c.add (x, y, 0xffffffff, a * (1.0f - depth) * (p < 3 || p > 13 ? 0.5f : 1.0f));
            }
        }
    }

    // the far reef: two soft layers of hills fading into the water (far = paler, near = deeper), so the
    // ocean has depth instead of one flat band
    if (sim.place != PlaceCave)
    {
        const float ph = (float) sim.place * 1.7f;
        for (int x = 0; x < W; ++x)
        {
            const float far = 30.0f + 12.0f * std::sin (x * 0.013f + ph) + 7.0f * std::sin (x * 0.041f + 2.0f * ph) + 3.0f * std::sin (x * 0.11f + ph);
            const int topF = (int) (floorY - far);
            for (int y = topF; y < (int) floorY; ++y)
            {
                const float k = (float) (y - topF) / juce::jmax (1.0f, far);
                c.blend (x, y, mixc (waterMid, waterDeep, 0.5f), 0.16f + 0.14f * k);
            }
            const float nearH = 16.0f + 7.0f * std::sin (x * 0.027f + 4.0f + ph) + 4.0f * std::sin (x * 0.083f + ph)
                                + 2.5f * std::abs (std::sin (x * 0.31f + ph));                      // little coral bumps on top
            const int topN = (int) (floorY - nearH);
            for (int y = topN; y < (int) floorY; ++y)
            {
                const float k = (float) (y - topN) / juce::jmax (1.0f, nearH);
                c.blend (x, y, waterDeep, 0.24f + 0.2f * k);
            }
            if (topN >= 0) c.blend (x, topN, light (waterMid, 0.3f), 0.18f * day);   // a lit edge
        }
    }
}

void Painter::floor (Canvas& c, const Sim& sim, float t)
{
    // seaweed and coral
    const auto& lk = look (sim.place);
    for (auto& w : weedSets[(size_t) juce::jlimit (0, NumPlaces - 1, sim.place)])
    {
        const bool kelp = w.height > 18;
        if (w.glow)
        {
            // glowing plants: a soft light, brighter tips
            for (int y = -10; y <= 10; ++y)
                for (int x = -10; x <= 10; ++x)
                {
                    const float d = std::sqrt ((float) (x * x + y * y)) / 10.0f;
                    if (d < 1.0f) c.add ((int) w.x + x, (int) floorY + 2 - w.height + y, w.colour, 0.07f * (1.0f - d) * (0.7f + 0.3f * std::sin (t * 1.5f + w.phase)));
                }
        }
        for (int k = 0; k < w.height; ++k)
        {
            const float sway = std::sin (t * 1.1f + w.phase + k * 0.18f) * k * (kelp ? 0.09f : 0.04f) * (1.0f + sim.weatherLevel);
            const int x = (int) (w.x + sway), y = (int) floorY + 2 - k;
            const Colour col = k % 6 < 3 ? w.colour : shade (w.colour, 0.85f);
            c.set (x, y, col);
            if (kelp)
            {
                // a thicker stem with leaf blades on alternating sides, lit on one side
                c.set (x + 1, y, shade (col, 0.78f));
                if (k > 4 && k % 7 == 3)
                {
                    const int side = (k / 7) % 2 == 0 ? 1 : -1;
                    const Colour leaf = light (w.colour, 0.15f);
                    for (int j = 1; j <= 4; ++j)
                        c.set (x + (side > 0 ? 1 : 0) + side * j, y - j / 2, j == 4 ? light (leaf, 0.2f) : leaf);
                    c.set (x + (side > 0 ? 1 : 0) + side * 2, y - 2, shade (leaf, 0.85f));
                }
            }
            else if (k % 3 == 1) { c.set (x - 1 - k / 4, y, col); c.set (x + 1 + k / 4, y, col); }
        }
    }
    // sand
    for (int x = 0; x < W; ++x)
    {
        const int top = (int) (floorY + 3.0f * std::sin (x * 0.03f) + 2.0f * std::sin (x * 0.11f + 1.0f));
        for (int y = top; y < H; ++y)
        {
            const float depth = (float) (y - top) / (float) (H - top + 1);
            Colour col = mixc (lk.sandTop, lk.sandLow, depth);
            const auto h = hash ((juce::uint32) (x * 1031 + y * 7919));
            if (h % 11 == 0) col = shade (col, 0.85f);
            else if (h % 17 == 0) col = light (col, 0.25f);
            if (y == top) col = light (col, 0.3f);
            else if (y > top + 2 && std::sin (x * 0.16f + y * 1.1f + std::sin (x * 0.03f) * 3.0f) > 0.93f) col = shade (col, 0.9f);   // ripples in the sand
            if (y > top + 3 && h % 997 == 0)                                                                                        // a shell or a pebble
                col = (h >> 12) % 3 == 0 ? 0xfffff0f4 : ((h >> 12) % 3 == 1 ? 0xfff2b7a8 : shade (lk.rock, 0.9f));
            col = shade (col, 0.45f + 0.55f * sim.daylight);
            // dancing light from the surface (caustics)
            if (lk.rays > 0.0f)
            {
                const float v = std::sin (x * 0.21f + t * 1.3f) + std::sin (y * 0.5f - t * 0.9f + x * 0.07f) + std::sin ((x + y) * 0.13f + t * 0.7f);
                if (v > 2.1f) col = light (col, 0.35f * (1.0f - sim.weatherLevel) * sim.daylight);
            }
            c.set (x, y, col);
        }
    }
    // rocks: round boulders half sunk in the sand, lit from above, with a darker side and a soft shadow
    const float lit = 0.45f + 0.55f * sim.daylight;
    for (size_t ri = 0; ri < rocks.size(); ++ri)
    {
        const auto& r = rocks[ri];
        if (std::abs (r.x - 440) < 40) continue;                     // keep the home cave clear
        const Colour base = mixc (lk.rock, light (lk.rock, 0.12f), (float) (r.x % 7) / 7.0f);
        const int baseY = (int) floorY + 5;
        for (int x = -r.w / 2 - 2; x <= r.w / 2 + 3; ++x)             // shadow on the sand
            c.blend (r.x + x, baseY, 0xff000000, 0.18f);
        for (int y = 0; y < r.h; ++y)
            for (int x = 0; x < r.w; ++x)
            {
                const float nx = (x - r.w * 0.5f + 0.5f) / (r.w * 0.5f), ny = (float) y / (float) r.h;   // ny: 0 bottom .. 1 top
                const float bump = 0.08f * std::sin ((float) x * 1.7f + (float) ri);
                if (nx * nx + ny * ny > 1.0f + bump) continue;
                const float light01 = juce::jlimit (0.0f, 1.0f, 0.55f + 0.5f * ny - 0.45f * nx);
                Colour col = mixc (shade (base, 0.62f), light (base, 0.22f), light01);
                const auto h = hash ((juce::uint32) (ri * 977 + (size_t) x * 31 + (size_t) y * 7));
                if (h % 9 == 0) col = shade (col, 0.9f);                       // speckles
                const bool edge = nx * nx + ny * ny > 0.82f + bump;
                if (edge) col = ny > 0.55f && nx < 0.3f ? light (col, 0.25f) : shade (col, 0.8f);
                // a cap that suits the place: moss, snow, coral or glowing crystals
                if (ny > 0.78f)
                {
                    if (sim.place == PlaceIce) col = mixc (0xfff4fbff, col, 0.15f);
                    else if (sim.place == PlaceReef || sim.place == PlaceKelp || sim.place == PlaceRuins || sim.place == PlaceWreck)
                        col = (h % 3 == 0) ? mixc (col, 0xff5aa04a, 0.6f) : col;
                    else if (sim.place == PlaceLagoon && h % 4 == 0) col = 0xffff8aa0;
                }
                c.set (r.x + x - r.w / 2, baseY - 1 - y, shade (col, lit));
            }
        if ((sim.place == PlaceDeep || sim.place == PlaceCave) && ri % 2 == 0)   // a little crystal glowing on top
        {
            const int cx = r.x + (int) (ri % 3) - 1, cy = baseY - r.h - 1;
            const Colour g = ri % 4 == 0 ? 0xff2ce8f5 : 0xffc06bff;
            for (int k = 0; k < 4; ++k) c.set (cx, cy - k, k == 3 ? light (g, 0.6f) : g);
            c.set (cx + 1, cy - 1, light (g, 0.3f));
            for (int y = -5; y <= 5; ++y) for (int x = -5; x <= 5; ++x)
            {
                const float d = std::sqrt ((float) (x * x + y * y)) / 5.0f;
                if (d < 1.0f) c.add (cx + x, cy - 2 + y, g, 0.12f * (1.0f - d));
            }
        }
    }
    // the cave where viewers' fish sleep while they're away: a rocky arch in the place's own stone, dark inside,
    // with a warm little lamp over the door
    {
        const int hx = 440, hy = (int) floorY + 6;
        const Colour stone = lk.rock;
        for (int y = 0; y < 34; ++y)
            for (int x = -31; x <= 31; ++x)
            {
                const float nx = x / 31.0f, ny = y / 34.0f;
                const float rough = 0.05f * std::sin (x * 0.9f) + 0.04f * std::sin (y * 1.3f + x * 0.4f);
                if (nx * nx + ny * ny > 1.0f + rough) continue;
                const float hole = (x / 13.0f) * (x / 13.0f) + (y / 17.0f) * (y / 17.0f);
                Colour col;
                if (hole < 1.0f) col = mixc (0xff1a2240, 0xff070a16, juce::jlimit (0.0f, 1.0f, 1.0f - hole + ny * 0.5f));   // the dark inside
                else
                {
                    const float l01 = juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * ny - 0.4f * nx);
                    col = mixc (shade (stone, 0.55f), light (stone, 0.15f), l01);
                    const auto h = hash ((juce::uint32) ((x + 40) * 131 + y * 17));
                    if (h % 7 == 0) col = shade (col, 0.88f);
                    if (hole < 1.25f) col = shade (col, 0.7f);                          // the rim of the door
                    if (ny > 0.86f && sim.place != PlaceCave && sim.place != PlaceDeep)
                        col = sim.place == PlaceIce ? mixc (0xfff4fbff, col, 0.1f) : mixc (col, 0xff5aa04a, h % 2 == 0 ? 0.65f : 0.3f);
                }
                c.set (hx + x, hy - y, shade (col, hole < 1.0f ? 1.0f : lit));
            }
        // the lamp
        const int lx = hx + 12, ly = hy - 24;
        const float flick = 0.85f + 0.15f * std::sin (t * 7.0f) * std::sin (t * 3.1f);
        for (int y = -7; y <= 7; ++y) for (int x = -7; x <= 7; ++x)
        {
            const float d = std::sqrt ((float) (x * x + y * y)) / 7.0f;
            if (d < 1.0f) c.add (lx + x, ly + y, 0xffffd27a, 0.22f * (1.0f - d) * flick);
        }
        c.set (lx, ly - 2, 0xff5a4a3a); c.set (lx, ly - 1, 0xffffe9a8); c.set (lx, ly, 0xffffd27a); c.set (lx - 1, ly, 0xffffc25a); c.set (lx + 1, ly, 0xffffc25a);
    }
    int sleeping = 0;
    for (auto& f : sim.fish) if (f.viewer() && ! f.present) ++sleeping;
    if (sleeping > 0)
    {
        const auto z = juce::String::repeatedString ("Z", 1 + (int) std::fmod (t * 1.5f, 3.0f));
        c.text (z, 444, (int) floorY - 12 - (int) std::fmod (t * 3.0f, 4.0f), 0xffc0a7f0);
        c.text (juce::String (sleeping) + " ASLEEP", 418, (int) floorY + 9, 0xffc0a7f0);
    }
}

void Painter::bubbles (Canvas& c, const Sim& sim, float dt)
{
    // vents on the floor, fish breathing, more in storms
    const float rate = 2.5f + sim.weatherLevel * 4.0f;
    if (rng.nextFloat() < rate * dt)
        bubbleList.push_back ({ (float) (60 + 130 * rng.nextInt (3) + rng.nextInt (6)), floorY,
                                rng.nextFloat() < 0.15f ? 3.0f + rng.nextFloat() * 1.5f : 0.8f + rng.nextFloat() * 1.6f, 12.0f + rng.nextFloat() * 10.0f, rng.nextFloat() * 6 });
    for (auto& f : sim.fish)
        if (f.present && ! f.dying && rng.nextFloat() < 0.06f * dt)
            bubbleList.push_back ({ f.x + f.facing * f.genes.lengthPixels() * 0.5f, f.y - 2.0f, 0.6f + rng.nextFloat(), 10.0f + rng.nextFloat() * 6.0f, rng.nextFloat() * 6 });
    for (auto& b : bubbleList)
    {
        b.y -= b.speed * dt;
        b.wobble += dt * 3.0f;
        b.x += std::sin (b.wobble) * 4.0f * dt;
    }
    bubbleList.erase (std::remove_if (bubbleList.begin(), bubbleList.end(), [] (const Bubble& b) { return b.y < surfaceY + 2.0f; }), bubbleList.end());
    if (bubbleList.size() > 300) bubbleList.erase (bubbleList.begin(), bubbleList.begin() + 50);
    for (auto& b : bubbleList)
    {
        const int x = (int) b.x, y = (int) b.y;
        if (b.r < 1.4f) c.blend (x, y, 0xffd8f6ff, 0.7f);
        else if (b.r < 2.8f)
        {
            c.blend (x - 1, y, 0xffd8f6ff, 0.75f); c.blend (x + 1, y, 0xffd8f6ff, 0.75f);
            c.blend (x, y - 1, 0xffd8f6ff, 0.75f); c.blend (x, y + 1, 0xffd8f6ff, 0.75f);
            c.blend (x, y, 0xffffffff, 0.15f); c.set (x - 1, y - 1, 0xffffffff);
        }
        else
        {
            // a big glossy bubble: a rim, a light inside and a white shine
            const int r = (int) b.r;
            for (int yy = -r; yy <= r; ++yy)
                for (int xx = -r; xx <= r; ++xx)
                {
                    const float d = std::sqrt ((float) (xx * xx + yy * yy));
                    if (d > r + 0.4f) continue;
                    if (d > r - 0.8f) c.blend (x + xx, y + yy, 0xffe6faff, 0.8f);
                    else c.blend (x + xx, y + yy, yy > 0 ? 0xff9ff3ff : 0xffffffff, yy > 0 ? 0.16f : 0.08f);
                }
            c.set (x - r / 2, y - r / 2, 0xffffffff); c.set (x - r / 2 + 1, y - r / 2, 0xffffffff); c.set (x - r / 2, y - r / 2 + 1, 0xffffffff);
        }
    }
}

void Painter::weather (Canvas& c, const Sim& sim, float t)
{
    const float lvl = sim.weatherLevel;
    if (lvl > 0.2f)
    {
        // rain in the sky and rings on the water
        const int drops = (int) (lvl * 160);
        for (int i = 0; i < drops; ++i)
        {
            const float x0 = hashf ((juce::uint32) i * 31 + 7) * (W + 40);
            const float y = std::fmod (hashf ((juce::uint32) i * 17 + 3) * 60.0f + t * (140.0f + lvl * 80.0f), surfaceY + 6.0f);
            const int x = (int) (x0 - y * 0.25f * lvl);
            for (int k = 0; k < 3; ++k) c.blend (x - (int) (k * 0.25f * lvl), (int) y - k, 0xffcfe3f0, 0.55f);
        }
        for (int i = 0; i < (int) (lvl * 25); ++i)
        {
            const auto h = hash ((juce::uint32) (i * 977 + (int) (t * 6.0f) * 131));
            c.blend ((int) (h % W), (int) surfaceY + 1 + (int) ((h >> 12) % 2), 0xffffffff, 0.8f);
        }
    }
    if (lvl > 0.6f)
        for (int y = (int) surfaceY; y < H; ++y)
            for (int x = 0; x < W; ++x)
                c.blend (x, y, 0xff050a18, (lvl - 0.6f) * 0.45f);
    if (sim.lightning > 0.0f)
    {
        // a bolt in the sky, then the whole ocean flashes
        juce::uint32 seed = (juce::uint32) (sim.time * 3.0);
        int x = 80 + (int) (hash (seed) % 320), y = 0;
        while (y < (int) surfaceY)
        {
            c.set (x, y, 0xffffffff); c.set (x + 1, y, 0xffe9f0ff);
            ++y;
            seed = hash (seed);
            if (seed % 3 == 0) x += (seed & 8) ? 1 : -1;
        }
        for (int yy = 0; yy < H; ++yy)
            for (int xx = 0; xx < W; ++xx)
                c.add (xx, yy, 0xffb0c4ff, sim.lightning * 0.35f);
    }
}

//==============================================================================
void Painter::drawFish (Canvas& c, const Fish& f, float time)
{
    constexpr int SW = 120, SH = 80, CX = 60, CY = 40;
    static thread_local std::vector<Colour> spr;
    spr.assign ((size_t) (SW * SH), 0);
    auto put = [&] (int x, int y, Colour col) { if ((unsigned) (x + CX) < (unsigned) SW && (unsigned) (y + CY) < (unsigned) SH) spr[(size_t) ((y + CY) * SW + x + CX)] = col | 0xff000000u; };

    const auto& g = f.genes;
    Colour c1 = colourARGB (g.colour1), c2 = colourARGB (g.colour2), c3 = colourARGB (g.colour3);
    switch (g.rare)
    {
        case RareGold:    c1 = 0xffffcf3d; c2 = 0xfff59a1b; c3 = 0xffffeb9a; break;
        case RareGhost:   c1 = light (c1, 0.55f); c2 = light (c2, 0.55f); c3 = light (c3, 0.6f); break;
        case RareGlow:    c1 = light (c1, 0.25f); c2 = light (c2, 0.35f); c3 = light (c3, 0.4f); break;
        case RareCrystal: c1 = 0xff9feeff; c2 = 0xffffffff; c3 = 0xff63c8ff; break;
        default: break;
    }
    const float len = g.lengthPixels() * (0.5f + 0.5f * f.growth);
    const int L = juce::jmax (6, (int) std::lround (len));
    float Hmax;
    switch (g.body)
    {
        case BodyRound:  Hmax = L * 0.42f; break;
        case BodyLong:   Hmax = L * 0.17f; break;
        case BodyPuffer: Hmax = L * 0.44f; break;
        case BodyCat:    Hmax = L * 0.36f; break;
        case BodyFlat:   Hmax = L * 0.55f; break;
        default:         Hmax = L * 0.26f; break;
    }
    auto halfH = [&] (float u) -> float
    {
        float s;
        switch (g.body)
        {
            case BodyRound:  s = std::sqrt (std::max (0.0f, std::sin (pi * u))); break;
            case BodyLong:   s = std::min (1.0f, 3.0f * std::sin (pi * u)); break;
            case BodyPuffer: s = std::pow (std::max (0.0f, std::sin (pi * u)), 0.45f); break;
            case BodyCat:    s = std::pow (std::max (0.0f, std::sin (pi * (0.05f + 0.9f * u))), 0.55f); break;
            case BodyFlat:   s = std::pow (std::max (0.0f, std::sin (pi * u)), 1.1f); break;
            default:         s = std::sqrt (std::max (0.0f, std::sin (pi * (0.1f + 0.85f * u)))); break;
        }
        return std::max (0.8f, Hmax * s);
    };
    const int bx0 = -L / 2;
    const float amp = g.body == BodyLong ? 1.6f : g.body == BodyRound || g.body == BodyPuffer ? 0.6f : 1.0f;
    auto wig = [&] (float u) { return std::sin (f.wiggle * 2.0f - u * 2.6f) * (1.0f - u) * amp; };
    const Colour outline = mixc (shade (c1, 0.32f), 0xff10142a, 0.4f);

    // tail
    const float tl = std::max (3.0f, L * (g.tail == TailVeil ? 0.55f : 0.32f));
    const float tailShift = wig (0.0f);
    for (int k = 1; k <= (int) tl; ++k)
    {
        const float kk = (float) k, swing = tailShift + std::sin (f.wiggle * 2.0f + 0.6f) * kk / tl * 1.6f;
        float th;
        switch (g.tail)
        {
            case TailRound: th = std::sqrt (std::max (0.0f, tl * tl * 0.36f - (kk - tl * 0.45f) * (kk - tl * 0.45f))) + 0.5f; break;
            case TailVeil:  th = 1.0f + kk * Hmax * 0.08f; break;
            default:        th = 0.6f + kk * (Hmax * 0.95f / tl); break;
        }
        for (int dy = -(int) th; dy <= (int) th; ++dy)
        {
            if (g.tail == TailFork && kk > tl * 0.4f && std::abs (dy) < th * 0.45f) continue;
            const float wave = g.tail == TailVeil ? std::sin (f.wiggle * 1.3f - kk * 0.6f) * kk * 0.2f : 0.0f;
            put (bx0 - k, (int) std::lround (dy + swing + wave), (k + dy) % 3 == 0 ? shade (c3, 0.85f) : c3);
        }
    }
    // dorsal fin
    for (int i = 0; i < L; ++i)
    {
        const float u = (i + 0.5f) / L;
        if (u < 0.28f || u > 0.68f || g.fin == FinNone) continue;
        const float v = (u - 0.28f) / 0.4f;
        float fh;
        switch (g.fin)
        {
            case FinSail:  fh = Hmax * 0.85f * std::pow (std::sin (pi * v), 0.7f) * (1.2f - v * 0.5f); break;
            case FinSpiky: fh = (i % 3 == 0) ? 3.5f : 1.2f; break;
            default:       fh = 2.2f * std::sin (pi * v); break;
        }
        const int top = (int) std::lround (-halfH (u) + wig (u));
        for (int k = 1; k <= (int) fh; ++k) put (bx0 + i, top - k, shade (c3, k == (int) fh ? 0.8f : 1.0f));
    }
    // body
    const juce::uint32 spotSeed = (juce::uint32) f.id * 2654435761u;
    for (int i = 0; i < L; ++i)
    {
        const float u = (i + 0.5f) / L, h = halfH (u), w = wig (u);
        const int y0 = (int) std::lround (-h + w), y1 = (int) std::lround (h + w);
        for (int y = y0; y <= y1; ++y)
        {
            const float dy = (float) y - w;
            Colour col = c1;
            switch (g.pattern)
            {
                case PatStripes:  if (u > 0.12f && u < 0.86f && (i % 4) < 2) col = c2; break;
                case PatSpots:    if (std::abs (dy) < h - 1.0f && hash (spotSeed + (juce::uint32) (i * 31 + (y + 50) * 7)) % 6 == 0) col = c2; break;
                case PatGradient: col = mixc (c1, c2, juce::jlimit (0.0f, 1.0f, (dy + h) / (2.0f * h))); break;
                case PatChecker:  if (((i / 2) + ((y + 40) / 2)) & 1) col = c2; break;
                case PatBelly:    if (dy > h * 0.1f) col = c2; break;
                default: break;
            }
            if (g.rare == RareRainbow) col = hsv (i * 0.06f + time * 0.25f, 0.65f, 1.0f);
            if (dy < -h * 0.55f) col = light (col, 0.5f);          // a glossy top
            else if (dy < -h * 0.2f) col = light (col, 0.18f);
            else if (dy > h * 0.55f) col = shade (col, 0.8f);
            put (bx0 + i, y, col);
        }
    }
    // a shine spot, like glass
    {
        const float us = 0.7f;
        put (bx0 + (int) (us * L), (int) std::lround (-halfH (us) * 0.6f + wig (us)), 0xffffffff);
        if (L >= 16) put (bx0 + (int) (us * L) - 1, (int) std::lround (-halfH (us) * 0.6f + wig (us)), 0xfff0fbff);
    }
    // a small side fin that flaps
    {
        const float u = 0.58f;
        const int fx = bx0 + (int) (u * L), fy = (int) std::lround (halfH (u) * 0.25f + wig (u));
        if (f.singFlash > 0.0f)
        {
            // singing: the side fin goes up and waves (twice, left - right)
            const float phase = (1.0f - f.singFlash) * 6.2831853f * 2.0f;
            const int sway = (int) std::lround (std::sin (phase) * 1.4f);
            const Colour fin = light (c3, 0.25f);
            put (fx, fy, shade (c3, 0.85f));
            put (fx, fy - 1, fin);
            put (fx + sway / 2, fy - 2, fin);
            put (fx + sway, fy - 3, fin);
            put (fx + sway, fy - 4, light (c3, 0.5f));
            if (L >= 17) { put (fx + sway + 1, fy - 4, light (c3, 0.5f)); put (fx + 1, fy - 1, fin); }
        }
        else
        {
            const int flap = (int) std::lround (std::sin (f.wiggle * 3.0f) * 1.2f);
            put (fx - 1, fy + 1, shade (c3, 0.8f)); put (fx - 2, fy + 1 + flap, shade (c3, 0.7f)); put (fx - 1, fy + 2 + flap, shade (c3, 0.7f));
        }
    }
    // cat ears
    if (g.body == BodyCat)
        for (float ue : { 0.62f, 0.84f })
        {
            const int ex = bx0 + (int) (ue * L), top = (int) std::lround (-halfH (ue) + wig (ue));
            for (int k = 1; k <= 3; ++k)
                for (int dx = -(3 - k); dx <= 1 - (k - 1) / 2 && dx <= (3 - k); ++dx)
                    put (ex + dx, top - k + 1, k == 1 && dx == 0 ? light (0xfff6757a, 0.2f) : c1);
        }

    // outline everything drawn so far
    {
        static thread_local std::vector<Colour> copy;
        copy = spr;
        auto had = [&] (int x, int y) { return (unsigned) (x + CX) < (unsigned) SW && (unsigned) (y + CY) < (unsigned) SH && copy[(size_t) ((y + CY) * SW + x + CX)] != 0; };
        for (int y = -CY + 1; y < SH - CY - 1; ++y)
            for (int x = -CX + 1; x < SW - CX - 1; ++x)
                if (! had (x, y) && (had (x - 1, y) || had (x + 1, y) || had (x, y - 1) || had (x, y + 1)))
                    put (x, y, outline);
    }

    // eye and mouth
    const float ue = g.body == BodyCat ? 0.8f : 0.78f;
    const int ex = bx0 + (int) (ue * L), ey = (int) std::lround (-halfH (ue) * 0.25f - 0.5f + wig (ue));
    const bool large = L >= 17;
    switch (g.eyes)
    {
        case EyesBig:
            for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) put (ex + x, ey + y, 0xffffffff);
            put (ex + 1, ey, 0xff10142a); if (large) put (ex + 1, ey + 1, 0xff10142a);
            break;
        case EyesSleepy:
            put (ex - 1, ey, 0xff10142a); put (ex, ey, 0xff10142a); if (large) put (ex + 1, ey, 0xff10142a);
            break;
        case EyesStar:
            put (ex, ey, 0xff10142a); put (ex + 1, ey, 0xff10142a); put (ex, ey + 1, 0xff10142a); put (ex + 1, ey + 1, 0xff10142a);
            put (ex, ey, ((int) (time * 4.0f + f.id) & 1) ? 0xffffffff : 0xfffee761);
            break;
        default:
            put (ex, ey, 0xff10142a); if (large) { put (ex + 1, ey, 0xff10142a); put (ex, ey - 1, 0xffffffff); }
            break;
    }
    put (bx0 + L - 1, ey + 2, shade (c1, 0.45f));   // mouth
    if (g.body == BodyCat)
    {
        put (bx0 + L, ey + 1, 0xffd0d4e0); put (bx0 + L + 1, ey + 1, 0xffd0d4e0); put (bx0 + L + 2, ey, 0xffd0d4e0);
        put (bx0 + L, ey + 2, 0xffd0d4e0); put (bx0 + L + 1, ey + 3, 0xffd0d4e0);
        put (bx0 + L - 1, ey + 1, 0xfff6757a);   // nose
    }
    if (g.body == BodyPuffer)
        for (int i = 1; i < L - 1; i += 3)
        {
            const float u = (i + 0.5f) / L, h = halfH (u), w = wig (u);
            put (bx0 + i, (int) std::lround (-h + w) - 2, outline);
            put (bx0 + i, (int) std::lround (h + w) + 2, outline);
        }

    // accessories
    const float ua = 0.7f;
    const int hx = bx0 + (int) (ua * L), top = (int) std::lround (-halfH (ua) + wig (ua)) - 1;
    const Colour ink = 0xff10142a;
    switch (g.accessory)
    {
        case AccHat:
            for (int x = -2; x <= 2; ++x) put (hx + x, top, ink);
            for (int y = 1; y <= 3; ++y) for (int x = -1; x <= 1; ++x) put (hx + x, top - y, y == 1 ? 0xffe43b44 : 0xff262b44);
            break;
        case AccGlasses:
            for (int lens : { 0, -4 })
            {
                for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x)
                    put (ex + lens + x, ey + y, (x == 0 && y == 0) ? 0xff2ce8f5 : ink);
            }
            put (ex - 2, ey - 1, ink);
            break;
        case AccCrown:
            for (int x = -2; x <= 2; ++x) put (hx + x, top, 0xffffcf3d);
            for (int x : { -2, 0, 2 }) { put (hx + x, top - 1, 0xffffcf3d); put (hx + x, top - 2, x == 0 ? 0xffe43b44 : 0xffffe9a0); }
            break;
        case AccScarf:
        {
            const float us = 0.56f;
            const int sx = bx0 + (int) (us * L), h = (int) halfH (us), w = (int) std::lround (wig (us));
            for (int y = -h; y <= h; ++y) { put (sx, y + w, 0xffe43b44); put (sx + 1, y + w, (y & 1) ? 0xffe43b44 : 0xfffee761); }
            put (sx - 1, h + w + 1, 0xffe43b44); put (sx - 2, h + w + 2, 0xffe43b44); put (sx - 2, h + w + 3, 0xfffee761);
            break;
        }
        case AccBow:
            put (hx - 2, top, 0xfff6757a); put (hx - 2, top - 2, 0xfff6757a); put (hx - 1, top - 1, 0xfff6757a);
            put (hx, top - 1, 0xffd6338a);
            put (hx + 1, top - 1, 0xfff6757a); put (hx + 2, top, 0xfff6757a); put (hx + 2, top - 2, 0xfff6757a);
            break;
        case AccLamp:
        {
            const int lx = bx0 + (int) (0.85f * L);
            put (lx, top, 0xff3a3f5a); put (lx + 1, top - 1, 0xff3a3f5a); put (lx + 2, top - 2, 0xff3a3f5a); put (lx + 3, top - 2, 0xff3a3f5a);
            put (lx + 4, top - 1, 0xfffff2a8); put (lx + 5, top - 1, 0xfffff2a8); put (lx + 4, top, 0xfffee761); put (lx + 5, top, 0xfffee761);
            break;
        }
        case AccHeadphones:
            put (hx - 2, top + 1, ink); put (hx - 2, top, ink); put (hx - 1, top - 1, ink); put (hx, top - 1, ink);
            put (hx + 1, top - 1, ink); put (hx + 2, top, ink); put (hx + 2, top + 1, ink);
            put (hx - 3, top + 2, 0xff2ce8f5); put (hx - 2, top + 2, 0xff2ce8f5); put (hx + 2, top + 2, 0xff2ce8f5); put (hx + 3, top + 2, 0xff2ce8f5);
            break;
        default: break;
    }

    // what only a new species has
    switch (g.trait)
    {
        case TraitWhiskers:
            for (int k = 1; k <= 5; ++k) { put (bx0 + L + k, ey + 1 - k / 2, 0xffe8ecf4); put (bx0 + L + k, ey + 2 + k / 2, 0xffe8ecf4); }
            break;
        case TraitAntenna:
        {
            const int ax = bx0 + (int) (0.8f * L), ay = (int) std::lround (-halfH (0.8f) + wig (0.8f));
            for (int k = 1; k <= 4; ++k) put (ax + k / 2, ay - k, shade (c3, 0.7f));
            put (ax + 2, ay - 5, 0xff9ff8ff); put (ax + 3, ay - 5, 0xff9ff8ff); put (ax + 2, ay - 6, 0xffffffff);
            break;
        }
        case TraitTwinTail:
            for (int k = 1; k <= (int) tl; ++k)
            {
                const int up = (int) std::lround (-Hmax * 0.6f - k * 0.5f + tailShift), down = (int) std::lround (Hmax * 0.6f + k * 0.5f + tailShift);
                put (bx0 - k + 1, up, light (c3, 0.2f)); put (bx0 - k + 1, down, light (c3, 0.2f));
            }
            break;
        case TraitStars:
            for (int i = 2; i < L - 2; i += 3)
            {
                const float u = (i + 0.5f) / L;
                const int y = (int) std::lround (wig (u) + ((i * 7) % 5 - 2) * halfH (u) * 0.25f);
                put (bx0 + i, y, ((int) (time * 3.0f) + i) % 4 == 0 ? 0xffffffff : 0xfffee761);
            }
            break;
        case TraitFrill:
        {
            const float u = 0.64f;
            const int fx = bx0 + (int) (u * L), h = (int) halfH (u) + 1, w = (int) std::lround (wig (u));
            for (int y = -h - 1; y <= h + 1; ++y) if ((y & 1) == 0) { put (fx, y + w, light (c2, 0.3f)); put (fx - 1, y + w, c2); }
            break;
        }
        case TraitHorn:
        {
            const int hx2 = bx0 + (int) (0.86f * L), hy = (int) std::lround (-halfH (0.86f) + wig (0.86f));
            put (hx2, hy - 1, 0xffffe9a0); put (hx2 + 1, hy - 2, 0xffffcf3d); put (hx2 + 1, hy - 3, 0xffffe9a0); put (hx2 + 2, hy - 4, 0xffffffff);
            break;
        }
        default: break;
    }

    // to the canvas (flipped when swimming left)
    float alpha = f.fade * (g.rare == RareGhost ? 0.5f + 0.15f * std::sin (time * 2.0f) : 1.0f);
    // a little hop while it sings
    const float hopUp = f.singFlash > 0.0f ? std::sin (f.singFlash * 3.14159265f) * 1.5f : 0.0f;
    const int px = (int) std::lround (f.x), py = (int) std::lround (f.y + std::sin (time * 1.5f + f.id) * 0.6f - hopUp);
    for (int y = 0; y < SH; ++y)
        for (int x = 0; x < SW; ++x)
        {
            const Colour col = spr[(size_t) (y * SW + x)];
            if (col == 0) continue;
            const int sx = x - CX, sy = y - CY;
            c.blend (px + (f.facing >= 0 ? sx : -sx), py + sy, col, alpha);
        }

    // glows and sparkles
    if (g.rare == RareGlow || g.accessory == AccLamp)
    {
        const int gx = g.accessory == AccLamp ? px + f.facing * (bx0 + (int) (0.85f * L) + 4) : px, gy = g.accessory == AccLamp ? py + top - 1 : py;
        const float r = g.rare == RareGlow ? L * 0.9f : 6.0f;
        const Colour gc = g.rare == RareGlow ? c1 : 0xfffff2a8;
        for (int y = -(int) r; y <= (int) r; ++y)
            for (int x = -(int) r; x <= (int) r; ++x)
            {
                const float d = std::sqrt ((float) (x * x + y * y)) / r;
                if (d < 1.0f) c.add (gx + x, gy + y, gc, 0.18f * (1.0f - d) * f.fade);
            }
    }
    if (g.rare == RareGold || g.rare == RareCrystal || g.rare == RareRainbow)
        for (int i = 0; i < 3; ++i)
        {
            const auto h = hash ((juce::uint32) (f.id * 97 + i * 13 + (int) (time * 5.0f)));
            if (h % 3 != 0) continue;
            const int sx = px + (int) (h % (juce::uint32) (L + 6)) - (L + 6) / 2, sy = py + (int) ((h >> 8) % (juce::uint32) (2 * (int) Hmax + 6)) - (int) Hmax - 3;
            c.set (sx, sy, 0xffffffff); c.blend (sx - 1, sy, 0xffffffff, 0.5f); c.blend (sx + 1, sy, 0xffffffff, 0.5f);
            c.blend (sx, sy - 1, 0xffffffff, 0.5f); c.blend (sx, sy + 1, 0xffffffff, 0.5f);
        }

    // ripples: rings of light spread out from a fish while it sings (two rings, one after the other)
    if (f.singFlash > 0.0f)
    {
        const Colour rc = light (c1, 0.6f);
        for (int ring = 0; ring < 2; ++ring)
        {
            const float t = (1.0f - f.singFlash) - 0.22f * (float) ring;   // 0 .. 1 as the ring grows
            if (t <= 0.0f) continue;
            const float r = (float) L * 0.45f + 3.0f + t * ((float) L * 0.7f + 9.0f);
            const float a = (1.0f - t) * (ring == 0 ? 0.55f : 0.35f) * f.fade;
            const int steps = juce::jmax (16, (int) (r * 6.3f));
            int lastX = 1 << 20, lastY = 1 << 20;
            for (int k = 0; k < steps; ++k)
            {
                const float ang = 6.2831853f * (float) k / (float) steps;
                const int rx = px + (int) std::lround (std::cos (ang) * r), ry = py + (int) std::lround (std::sin (ang) * r * 0.55f);   // flat, like on water
                if (rx == lastX && ry == lastY) continue;
                lastX = rx; lastY = ry;
                c.blend (rx, ry, rc, a);
            }
        }
    }

    // a note floats up while it sings
    if (f.singFlash > 0.0f)
    {
        const int nx = px + f.facing * (L / 2 + 2), ny = py - (int) Hmax - 3 - (int) ((1.0f - f.singFlash) * 7.0f);
        const Colour nc = light (c3, 0.35f);
        const float a = juce::jmin (1.0f, f.singFlash * 1.5f) * f.fade;
        c.blend (nx, ny, nc, a); c.blend (nx + 1, ny, nc, a); c.blend (nx, ny + 1, nc, a); c.blend (nx + 1, ny + 1, nc, a);
        c.blend (nx + 1, ny - 1, nc, a); c.blend (nx + 1, ny - 2, nc, a); c.blend (nx + 2, ny - 3, nc, a);
    }
}

//==============================================================================
void Painter::overlay (Canvas& c, const Sim& sim, const MusicInfo& m, float dt)
{
    // glossy glass panels (Frutiger Aero): rounded, aqua, a shine on the top half, a light rim
    auto panel = [&c] (int x, int y, int w, int h)
    {
        constexpr int r = 4;
        auto inside = [&] (int i, int j)
        {
            const int cx = i < r ? r - i : (i >= w - r ? i - (w - r - 1) : 0);
            const int cy = j < r ? r - j : (j >= h - r ? j - (h - r - 1) : 0);
            return cx * cx + cy * cy <= r * r;
        };
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
            {
                if (! inside (i, j)) continue;
                const float v = (float) j / (float) juce::jmax (1, h - 1);
                c.blend (x + i, y + j, mixc (0xff4fc4ee, 0xff0a3f7c, v), 0.66f);
                if (v < 0.45f) c.blend (x + i, y + j, 0xffffffff, 0.05f + 0.22f * (1.0f - v / 0.45f));
                const bool edge = ! inside (i - 1, j) || ! inside (i + 1, j) || ! inside (i, j - 1) || ! inside (i, j + 1);
                if (edge) c.blend (x + i, y + j, j < h / 2 ? 0xffffffff : 0xff9ff3ff, j < h / 2 ? 0.75f : 0.5f);
            }
    };
    juce::ignoreUnused (dt);

    // now playing
    {
        juce::String l1 = m.title.isNotEmpty() ? "NOW PLAYING: " + m.title.toUpperCase() : juce::String ("NOW PLAYING"), l2 = m.style + "  " + m.key + " " + m.scale + "  " + juce::String (m.bpm) + " BPM",
                           l3 = "PART " + juce::String (m.piece + 1) + "  " + m.section + "   " + m.place,
                           l4 = "FISH PULL IT TO " + juce::String ((int) std::lround (m.targetBpm)) + " BPM " + m.targetKey;
        const int w = juce::jmax (Canvas::textWidth (l2), Canvas::textWidth (l4)) + 10;
        while (l1.length() > 12 && Canvas::textWidth (l1) > w - 10) l1 = l1.dropLastCharacters (1);
        panel (4, 4, w, 31);
        c.text (l1, 9, 8, 0xff2ce8f5, 1, 0xff06264a, true);
        c.text (l2.toUpperCase(), 9, 15, 0xffffffff, 1, 0xff06264a, true);
        c.text (l3.toUpperCase(), 9, 22, 0xffc0a7f0, 1, 0xff06264a, true);
        c.text (l4.toUpperCase(), 9, 29 - 1, 0xff9ef0c2, 1, 0xff06264a, true);
    }

    // leaderboard (changes every 10 s)
    boardTimer += 1.0f / 30.0f;
    if (boardTimer > 10.0f) { boardTimer = 0; boardIndex = (boardIndex + 1) % Sim::numBoards; }
    {
        const auto b = sim.board (boardIndex);
        int w = Canvas::textWidth (b.title) + 10;
        for (auto& l : b.lines) w = juce::jmax (w, Canvas::textWidth (l.toUpperCase()) + 10);
        juce::String stats = "FISH " + juce::String ((int) std::count_if (sim.fish.begin(), sim.fish.end(), [] (auto& f) { return f.present && ! f.dying; }))
                           + "  BIRTHS " + juce::String (sim.births);
        static const char* wn[] = { "CALM", "RAIN", "STORM" };
        stats << "  " << wn[juce::jlimit (0, 2, sim.weather)];
        w = juce::jmax (w, Canvas::textWidth (stats) + 10);
        const int x = W - 4 - w, h = 12 + 7 * b.lines.size() + 8;
        boardLeft = x; boardBottom = 4 + h;
        panel (x, 4, w, h);
        c.text (b.title, x + 5, 8, 0xfffee761, 1, 0xff06264a, true);
        for (int i = 0; i < b.lines.size(); ++i)
            c.text ((b.empty ? juce::String() : juce::String (i + 1) + ".") + b.lines[i].toUpperCase().substring (0, 30), x + 5, 15 + 7 * i, 0xffffffff, 1, 0xff06264a, true);
        c.text (stats, x + 5, 4 + h - 8, 0xff8b9bb4, 1, 0xff06264a, true);
    }

    // the vote
    if (sim.vote.active)
    {
        const int w = 170, x = W / 2 - w / 2, y = 46, h = 14 + 8 * (int) sim.vote.options.size() + 4;
        panel (x, y, w, h);
        const juce::String title = sim.vote.kind == 0 ? "VOTE: NEXT MUSIC" : "VOTE: WHAT HAPPENS?";
        c.text (title, x + 5, y + 4, 0xff2ce8f5, 1, 0xff06264a, true);
        const auto secs = juce::String ((int) std::ceil (sim.vote.left)) + "S";
        c.text (secs, x + w - 5 - Canvas::textWidth (secs), y + 4, 0xff8b9bb4, 1, 0xff06264a, true);
        int total = 0;
        for (auto n : sim.vote.counts) total += n;
        for (size_t i = 0; i < sim.vote.options.size(); ++i)
        {
            const int oy = y + 13 + 8 * (int) i;
            const auto name = (sim.vote.kind == 0 ? styleName (sim.vote.options[i]) : eventName (sim.vote.options[i])).toUpperCase();
            c.text ("!VOTE " + juce::String ((int) i + 1) + " " + name, x + 5, oy, 0xffffffff, 1, 0xff06264a, true);
            const int bw = total > 0 ? (int) (50.0f * sim.vote.counts[i] / total) : 0;
            c.fillRect (x + w - 62, oy, bw, 5, 0xff2ce8f5, 0.9f);
            c.fillRect (x + w - 62 + bw, oy, 50 - bw, 5, 0xff2a2f4e, 0.8f);
            c.text (juce::String (sim.vote.counts[i]), x + w - 9, oy, 0xffffffff, 1, 0xff06264a, true);
        }
    }

    // bottom line: news, otherwise a tip
    static const char* tips[] = {
        "TYPE !FISH IN CHAT TO GET YOUR OWN FISH",
        "!COLOR RED BLUE   !PATTERN STRIPES   !BODY CAT",
        "!HAT CROWN   !EYES BIG   !TAIL VEIL   !FIN SAIL",
        "!VOICE BELL   !MELODY 1353   !RHYTHM BUSY   !NOTES LONG",
        "!DANCE   !SPIN   !JUMP   !FEED   !LEVEL   !NAME MIRO",
        "CHATTING AND WATCHING LEVEL UP YOUR FISH: NEW HATS, VOICES AND LOOKS UNLOCK",
        "AFTER MANY GENERATIONS, EVOLUTION CAN DISCOVER A WHOLE NEW SPECIES",
        "YOUR FISH SWIMS IN WHEN YOU CHAT AND SLEEPS IN THE CAVE WHEN YOU'RE AWAY",
        "FISH HAVE BABIES. RARE ONES ARE GOLDEN, RAINBOW, GHOST, GLOWING OR CRYSTAL",
        "THE MUSIC FOLLOWS THE FISH: THEIR GENES PULL THE TEMPO AND THE KEY",
        "THE BOAT CAT GETS HUNGRY: !FEEDCAT   AND LOVES PETS: !PET",
        "!REQUEST JAZZ PLAYS YOUR STYLE NEXT   !LIKE A SONG AND IT COMES BACK",
        "!TODAY SHOWS TODAY'S OCEAN NUMBERS",
    };
    tipTimer += 1.0f / 30.0f;
    if (tipTimer > 9.0f) { tipTimer = 0; tip = (tip + 1) % (int) (sizeof (tips) / sizeof (tips[0])); }
    for (int y = H - 11; y < H; ++y)
    {
        const float v = (float) (y - (H - 11)) / 10.0f;
        for (int x = 0; x < W; ++x)
        {
            c.blend (x, y, mixc (0xff4fc4ee, 0xff0a3f7c, v), 0.7f);
            if (v < 0.5f) c.blend (x, y, 0xffffffff, 0.2f * (1.0f - v * 2.0f));
            if (y == H - 11) c.blend (x, y, 0xffffffff, 0.7f);
        }
    }
    juce::String line = tips[tip];
    Colour col = 0xffffffff;
    if (! sim.news.empty()) { line = sim.news.back().text.toUpperCase(); col = sim.news.back().colour; }
    line = line.substring (0, 118);
    c.text (line, W / 2 - Canvas::textWidth (line) / 2, H - 8, col, 1, 0xff06264a, true);

    // a big alert in the middle: new members, Super Chats, subscriber milestones
    if (! sim.alerts.empty())
    {
        const auto& a = sim.alerts.front();
        const float in = juce::jmin (1.0f, (9.0f + 20.0f - a.life) * 0.0f + 1.0f);
        juce::ignoreUnused (in);
        const auto title = a.title.toUpperCase().substring (0, 60), text = a.text.toUpperCase().substring (0, 70);
        const int w = juce::jmax (Canvas::textWidth (title, 2), Canvas::textWidth (text)) + 20;
        const int h = text.isNotEmpty() ? 34 : 24;
        const int bob = (int) std::lround (std::sin (clock * 3.0f) * 1.5f);
        const int x = W / 2 - w / 2, y = 70 + bob;
        panel (x, y, w, h);
        // a golden / aqua rim that shimmers
        for (int i = 0; i < w; ++i)
        {
            const float sh = 0.5f + 0.5f * std::sin (clock * 6.0f + i * 0.15f);
            c.blend (x + i, y - 1, a.colour, 0.5f + 0.5f * sh); c.blend (x + i, y + h, a.colour, 0.5f + 0.5f * sh);
        }
        c.text (title, W / 2 - Canvas::textWidth (title, 2) / 2, y + 6, a.colour, 2, 0xff06264a, true);
        if (text.isNotEmpty()) c.text (text, W / 2 - Canvas::textWidth (text) / 2, y + 24, 0xffffffff, 1, 0xff06264a, true);
    }

    // the end-of-day recap
    if (sim.recap > 0.0f)
    {
        const auto& d = sim.recapStats;
        juce::StringArray lines;
        lines.add ("TODAY IN THE OCEAN" + (d.date.isNotEmpty() ? "  (" + d.date + ")" : juce::String()));
        lines.add (juce::String (d.messages) + " MESSAGES   " + juce::String (d.viewersSeen) + " VIEWERS   " + juce::String (d.newFish) + " NEW FISH");
        lines.add (juce::String (d.births) + " BABIES   " + juce::String (d.newSubscribers) + " NEW SUBSCRIBERS   " + juce::String (d.members) + " NEW MEMBERS");
        std::vector<std::pair<juce::String, int>> top (d.chatters.begin(), d.chatters.end());
        std::sort (top.begin(), top.end(), [] (auto& p, auto& q) { return p.second > q.second; });
        juce::String best = "TOP CHATTERS: ";
        for (size_t i = 0; i < juce::jmin ((size_t) 3, top.size()); ++i) best << (i > 0 ? ", " : "") << top[i].first.toUpperCase() << " (" << top[i].second << ")";
        if (! top.empty()) lines.add (best);
        if (! sim.favourites.empty())
        {
            const Favourite* fav = &sim.favourites.front();
            for (auto& f : sim.favourites) if (f.likers.size() > fav->likers.size()) fav = &f;
            lines.add ("MOST LIKED SONG: " + fav->title.toUpperCase() + " (" + juce::String (fav->likers.size()) + ")");
        }
        lines.add ("THANK YOU FOR SWIMMING WITH US!");
        int w = 0;
        for (auto& l : lines) w = juce::jmax (w, Canvas::textWidth (l));
        w += 20;
        const int h = 8 + lines.size() * 8, x = W / 2 - w / 2, y = 112;
        panel (x, y, w, h);
        for (int i = 0; i < lines.size(); ++i)
            c.text (lines[i], W / 2 - Canvas::textWidth (lines[i]) / 2, y + 5 + i * 8, i == 0 ? 0xfffee761 : (i == lines.size() - 1 ? 0xff9ff3ff : 0xffffffff), 1, 0xff06264a, true);
    }
}

//==============================================================================
void Painter::sky (Canvas& c, const Sim& sim, float t)
{
    // shooting stars (a meteor shower)
    if (sim.meteors > 0.0f)
    {
        const float a = juce::jmin (1.0f, sim.meteors / 10.0f) * (0.4f + 0.6f * (1.0f - sim.daylight));
        for (int i = 0; i < 6; ++i)
        {
            const float period = 1.6f + (i % 3) * 0.7f;
            const float life = std::fmod (t + i * 0.83f, period) / period;
            if (life > 0.45f) continue;
            const auto h = hash ((juce::uint32) i * 977 + (juce::uint32) ((t + i * 0.83f) / period));
            const float sx = (float) (h % (juce::uint32) W), sy = 2.0f + (float) ((h >> 10) % 18u);
            const float k = life / 0.45f;
            const int hx = (int) (sx - k * 60.0f), hy = (int) (sy + k * 22.0f);
            for (int j = 0; j < 14; ++j)
            {
                const int px = hx + j * 3 / 1, py = hy - j * 22 / 60;
                if (py < (int) surfaceY - 2) c.blend (px, py, j == 0 ? 0xffffffff : 0xffcfe4ff, a * (1.0f - j / 14.0f) * (1.0f - k * 0.5f));
            }
        }
    }
    // fireworks (alerts, milestones)
    if (sim.fireworks > 0.0f)
    {
        static const Colour cols[] = { 0xffff5a7a, 0xfffee761, 0xff5affc8, 0xff7ab8ff, 0xffd08aff, 0xffffa64a };
        for (int i = 0; i < 4; ++i)
        {
            const float period = 1.7f + i * 0.31f;
            const float life = std::fmod (t + i * 0.57f, period) / period;
            const auto h = hash ((juce::uint32) i * 131 + (juce::uint32) ((t + i * 0.57f) / period) * 7);
            const int cx = 40 + (int) (h % (juce::uint32) (W - 80)), cy = 8 + (int) ((h >> 8) % 22u);
            const Colour col = cols[(h >> 4) % 6];
            if (life < 0.2f)   // the rocket going up
            {
                const int ry = (int) surfaceY - (int) ((float) ((int) surfaceY - cy) * life / 0.2f);
                c.blend (cx, ry, 0xffffffff, 0.9f);
                continue;
            }
            const float k = (life - 0.2f) / 0.8f;
            for (int j = 0; j < 16; ++j)
            {
                const float ang = j * 0.3927f;
                const float r = k * 16.0f;
                const int px = cx + (int) std::lround (std::cos (ang) * r), py = cy + (int) std::lround (std::sin (ang) * r * 0.8f + k * k * 6.0f);
                if (py < (int) surfaceY - 1) { c.blend (px, py, col, 1.0f - k); c.add (px, py, col, 0.3f * (1.0f - k)); }
            }
        }
    }
}

void Painter::school (Canvas& c, const Sim& sim, float t)
{
    if (sim.migration <= 0.0f) return;
    // a big school of silver fish and three sea turtles crossing the ocean
    const float total = 100.0f, p = (total - sim.migration) / total;   // 0 .. 1
    const float head = -80.0f + p * (W + 200.0f);
    for (int i = 0; i < 70; ++i)
    {
        const float ox = hashf ((juce::uint32) i * 13 + 1) * 90.0f, oy = hashf ((juce::uint32) i * 7 + 3) * 50.0f;
        const float x = head - ox + std::sin (t * 2.0f + i) * 2.0f;
        const float y = 120.0f + oy + std::sin (t * 1.3f + x * 0.03f) * 8.0f;
        const int ix = (int) x, iy = (int) y;
        const bool flash = std::sin (t * 3.0f + i) > 0.85f;
        c.set (ix, iy, flash ? 0xffffffff : 0xffbcd4e6); c.set (ix - 1, iy, 0xff8fb0c8); c.set (ix + 1, iy, 0xffdfeef8);
        c.set (ix - 2, iy - 1, 0xff8fb0c8); c.set (ix - 2, iy + 1, 0xff8fb0c8);
    }
    for (int k = 0; k < 3; ++k)
    {
        const float x = head - 30.0f - k * 45.0f, y = 90.0f + k * 22.0f + std::sin (t * 0.8f + k) * 4.0f;
        const int tx = (int) x, ty = (int) y;
        const float flap = std::sin (t * 2.5f + k);
        for (int yy = -4; yy <= 4; ++yy) for (int xx = -7; xx <= 7; ++xx)   // the shell
            if (xx * xx / 49.0f + yy * yy / 16.0f <= 1.0f)
                c.set (tx + xx, ty + yy, (std::abs (xx) + std::abs (yy)) % 4 == 0 ? 0xff4a6a2a : (yy < -1 ? 0xff7aa04a : 0xff5f8a3a));
        for (int j = 0; j < 3; ++j) { c.set (tx + 8 + j, ty - 1, 0xff9ac07a); c.set (tx + 8 + j, ty, 0xff9ac07a); }   // head
        c.set (tx + 10, ty - 1, 0xff10121c);
        const int fy = (int) std::lround (flap * 3.0f);
        for (int j = 0; j < 4; ++j) { c.set (tx + 2 - j, ty - 5 - j + fy * j / 3, 0xff8ab06a); c.set (tx + 2 - j, ty + 5 + j - fy * j / 3, 0xff8ab06a); }
    }
}

void Painter::glow (Canvas& c, const Sim& sim, float t)
{
    if (sim.plankton <= 0.0f) return;
    // a plankton bloom: the water fills with tiny glowing specks, brighter at night
    const float a = juce::jmin (1.0f, sim.plankton / 15.0f) * (0.35f + 0.65f * (1.0f - sim.daylight * 0.7f));
    for (int i = 0; i < 260; ++i)
    {
        const float x = std::fmod (hashf ((juce::uint32) i * 17 + 5) * W + t * (2.0f + (i % 5)) + std::sin (t * 0.7f + i) * 4.0f, (float) W);
        const float y = surfaceY + 6.0f + hashf ((juce::uint32) i * 29 + 1) * (floorY - surfaceY - 12.0f) + std::sin (t * 0.5f + i * 0.3f) * 3.0f;
        const float tw = 0.5f + 0.5f * std::sin (t * (1.5f + (i % 7) * 0.3f) + i);
        const Colour col = i % 3 == 0 ? 0xff7affd8 : (i % 3 == 1 ? 0xff5ad8ff : 0xffb6ff8a);
        c.add ((int) x, (int) y, col, a * (0.3f + 0.7f * tw));
        if (tw > 0.8f) { c.add ((int) x + 1, (int) y, col, a * 0.4f); c.add ((int) x, (int) y + 1, col, a * 0.4f); }
    }
}
//==============================================================================
void Painter::decor (Canvas& c, const Sim& sim, float t)
{
    switch (sim.place)
    {
        case PlaceWreck:
        {
            // a sunken ship lying on the sand, a little tilted, with portholes and a broken mast
            const int x0 = 150, y0 = (int) floorY + 2;
            for (int x = 0; x < 150; ++x)
            {
                const float u = x / 150.0f;
                const int top = y0 - 26 - (int) (u * 10.0f) + (x > 120 ? (x - 120) / 2 : 0);
                const int bottom = y0 - (int) (std::sin (u * 3.14159f) * 6.0f);
                for (int y = top; y <= bottom; ++y)
                {
                    Colour col = ((y - top) % 6 == 0) ? 0xff5a3a24 : (x % 23 == 0 ? 0xff4a2f1d : 0xff7a5134);
                    if (y == top) col = 0xff9a6a44;
                    c.blend (x0 + x, y, col, 0.95f);
                }
                if (x % 24 == 12) for (int k = -2; k <= 2; ++k) for (int j = -2; j <= 2; ++j)
                    if (k * k + j * j <= 4) c.set (x0 + x + k, top + 9 + j, (k * k + j * j <= 1) ? 0xff0f1a26 : 0xffc9a25a);
            }
            for (int y = 0; y < 70; ++y)   // the broken mast
                c.set (x0 + 60 + y / 6, y0 - 36 - y, y % 9 == 0 ? 0xff4a2f1d : 0xff6b4630);
            for (int y = 0; y < 18; ++y) for (int x = 0; x < 10 - y / 3; ++x)   // a torn sail
                c.blend (x0 + 63 + y / 6 + x, y0 - 100 + y, 0xffd8ccb0, 0.75f);
            // the treasure chest (it shines)
            const int tx = 345, ty = (int) floorY - 1;
            c.fillRect (tx, ty - 7, 14, 8, 0xff7a4a22);
            c.fillRect (tx, ty - 10, 14, 3, 0xff9a6a34);
            c.fillRect (tx, ty - 7, 14, 1, 0xffffcf3d);
            c.set (tx + 7, ty - 5, 0xffffcf3d);
            for (int i = 0; i < 3; ++i)
            {
                const auto h = hash ((juce::uint32) (i * 31 + (int) (t * 4.0f)));
                if (h % 3 == 0) c.set (tx + (int) (h % 14), ty - 11 - (int) ((h >> 8) % 4), 0xffffffff);
            }
            break;
        }
        case PlaceKelp:
        {
            // far kelp in the haze
            for (int k = 0; k < 30; ++k)
            {
                const float kx = hashf ((juce::uint32) k * 17 + 3) * W;
                const int hgt = 90 + (int) (hashf ((juce::uint32) k * 5 + 1) * 110);
                for (int j = 0; j < hgt; ++j)
                {
                    const float sway = std::sin (t * 0.7f + k + j * 0.05f) * j * 0.05f;
                    c.blend ((int) (kx + sway), (int) floorY - j, 0xff1d4a3a, 0.35f);
                }
            }
            break;
        }
        case PlaceCave:
        {
            // the cave roof hides the sky: rock with stalactites and glowing crystals
            for (int x = 0; x < W; ++x)
            {
                const float edge = surfaceY + 6.0f + 6.0f * std::sin (x * 0.08f) + 4.0f * std::sin (x * 0.23f + 1.0f);
                const int drip = (hash ((juce::uint32) x / 6 * 977) % 5 == 0) ? (int) (8 + hash ((juce::uint32) x / 6 * 31) % 14 - std::abs ((x % 6) - 3) * 3) : 0;
                for (int y = 0; y < (int) edge + juce::jmax (0, drip); ++y)
                {
                    Colour col = mixc (0xff2a2140, 0xff4a3d6e, (float) y / edge);
                    if (hash ((juce::uint32) (x * 131 + y * 17)) % 13 == 0) col = shade (col, 0.8f);
                    c.set (x, y, col);
                }
            }
            for (int i = 0; i < 9; ++i)
            {
                const int gx = 20 + (int) (hashf ((juce::uint32) i * 5 + 1) * (W - 40)), gy = 8 + (int) (hashf ((juce::uint32) i * 9 + 2) * 30);
                static const Colour cols[] = { 0xffff7ad9, 0xff2ce8f5, 0xffc06bff };
                const Colour col = cols[i % 3];
                for (int k = 0; k < 4; ++k) { c.set (gx, gy + k, col); c.set (gx + 1, gy + k + 1, light (col, 0.3f)); }
                for (int y = -7; y <= 7; ++y) for (int x = -7; x <= 7; ++x)
                {
                    const float d = std::sqrt ((float) (x * x + y * y)) / 7.0f;
                    if (d < 1.0f) c.add (gx + x, gy + 2 + y, col, 0.08f * (1.0f - d) * (0.7f + 0.3f * std::sin (t * 1.3f + i)));
                }
            }
            break;
        }
        case PlaceIce:
        {
            // ice floes on the surface and the hidden part of an iceberg
            for (int i = 0; i < 5; ++i)
            {
                const float fx = std::fmod (hashf ((juce::uint32) i * 13 + 1) * (W + 120) + t * (1.0f + i * 0.3f), (float) (W + 120)) - 60.0f;
                const int w = 26 + (int) (hashf ((juce::uint32) i * 7 + 5) * 40);
                const int y = (int) surfaceY - 2;
                for (int x = 0; x < w; ++x)
                    for (int k = 0; k < 5; ++k)
                    {
                        if ((x < 2 || x > w - 3) && k > 2) continue;
                        c.set ((int) fx + x, y + k, k == 0 ? 0xffffffff : k < 3 ? 0xffe6f7ff : 0xffbfe6f5);
                    }
                if (i == 2)   // an iceberg under this floe
                    for (int k = 0; k < 60; ++k)
                    {
                        const int half = (int) (w * 0.5f * (1.0f - k / 60.0f)) + 4;
                        for (int x = -half; x <= half; ++x) c.blend ((int) fx + w / 2 + x, y + 5 + k, 0xffd8f6ff, 0.28f);
                    }
            }
            // a few snowflakes above the water
            for (int i = 0; i < 40; ++i)
            {
                const float x = std::fmod (hashf ((juce::uint32) i * 37 + 2) * W + t * 3.0f + std::sin (t + i) * 3.0f, (float) W);
                const float y = std::fmod (hashf ((juce::uint32) i * 11 + 9) * surfaceY + t * (4.0f + i % 4), surfaceY);
                c.blend ((int) x, (int) y, 0xffffffff, 0.8f);
            }
            break;
        }
        case PlaceLagoon:
        {
            // starfish and shells on the bright sand
            static const Colour stars[] = { 0xffff7a4a, 0xffff5f8a, 0xffffc04a };
            for (int i = 0; i < 6; ++i)
            {
                const int sx = 30 + (int) (hashf ((juce::uint32) i * 23 + 4) * (W - 60)), sy = (int) floorY + 6 + (int) (hashf ((juce::uint32) i * 3 + 1) * 10);
                const Colour col = stars[i % 3];
                c.set (sx, sy, col); c.set (sx - 1, sy, col); c.set (sx + 1, sy, col); c.set (sx, sy - 1, col); c.set (sx, sy - 2, col);
                c.set (sx - 2, sy + 1, col); c.set (sx + 2, sy + 1, col); c.set (sx, sy, light (col, 0.4f));
            }
            break;
        }
        case PlaceVolcano:
        {
            // three vents: dark cones with glowing mouths, hot water shimmering up, cracks of lava in the floor
            const int vents[] = { 90, 250, 370 };
            const int heights[] = { 26, 38, 20 };
            for (int k = 0; k < 3; ++k)
            {
                const int vx = vents[k], vh = heights[k], base = (int) floorY + 4;
                for (int y = 0; y < vh; ++y)
                {
                    const int half = 3 + (vh - y) * 14 / vh;
                    for (int x = -half; x <= half; ++x)
                    {
                        Colour col = x < -half / 3 ? 0xff4a4042 : (x > half / 2 ? 0xff241e20 : 0xff362e30);
                        if (y > vh - 4 && std::abs (x) < 3) col = mixc (0xffff8a2a, 0xffffd25a, (float) (y - (vh - 4)) / 4.0f);   // the glowing mouth
                        c.set (vx + x, base - y, col);
                    }
                }
                // glow and rising heat
                const int my = base - vh;
                for (int y = -10; y <= 10; ++y) for (int x = -10; x <= 10; ++x)
                {
                    const float d = std::sqrt ((float) (x * x + y * y)) / 10.0f;
                    if (d < 1.0f) c.add (vx + x, my + y, 0xffff7a2a, 0.22f * (1.0f - d) * (0.8f + 0.2f * std::sin (t * 4.0f + k)));
                }
                for (int i = 0; i < 6; ++i)
                {
                    const float life = std::fmod (t * 0.5f + i / 6.0f + k * 0.37f, 1.0f);
                    const int px = vx + (int) (std::sin (t * 2.0f + i * 1.7f) * (2.0f + life * 6.0f)), py = my - (int) (life * 70.0f);
                    c.blend (px, py, life < 0.3f ? 0xffffb25a : 0xffc8e8ff, 0.5f * (1.0f - life));
                }
            }
            // lava cracks
            for (int x = 0; x < W; ++x)
            {
                const float v = std::sin (x * 0.11f) + std::sin (x * 0.037f + 1.0f);
                if (v > 1.55f) { const float g = 0.6f + 0.4f * std::sin (t * 2.0f + x * 0.2f); c.blend (x, (int) floorY + 8 + (x % 3), 0xffff6a1a, 0.7f * g); }
            }
            break;
        }
        case PlaceJelly:
        {
            // jellyfish drifting: soft glowing bells that pulse, trailing tentacles
            static const Colour jc[] = { 0xffff9ad8, 0xffc89aff, 0xff9ad8ff, 0xffffc89a };
            for (int i = 0; i < 9; ++i)
            {
                const float sp = 2.0f + hashf ((juce::uint32) i * 13 + 1) * 3.0f;
                const float jx = std::fmod (hashf ((juce::uint32) i * 7 + 2) * (W + 40) + t * sp * 0.6f, (float) (W + 40)) - 20.0f;
                const float pulse = 0.5f + 0.5f * std::sin (t * 1.8f + i * 1.1f);
                const float jy = 70.0f + hashf ((juce::uint32) i * 3 + 5) * 120.0f + std::sin (t * 0.4f + i) * 12.0f - pulse * 3.0f;
                const Colour col = jc[i % 4];
                const int rx = 5 + (int) (hashf ((juce::uint32) i * 11) * 4.0f), ry = rx - 1 + (int) (pulse * 1.5f);
                for (int y = -ry; y <= 0; ++y)
                    for (int x = -rx; x <= rx; ++x)
                        if ((float) (x * x) / (float) (rx * rx) + (float) (y * y) / (float) (ry * ry) <= 1.0f)
                            c.blend ((int) jx + x, (int) jy + y, y < -ry / 2 ? light (col, 0.4f) : col, 0.55f);
                for (int k = -rx + 1; k < rx; k += 2)   // tentacles
                    for (int y = 1; y < 14 + (k & 3) * 3; ++y)
                        c.blend ((int) jx + k + (int) std::lround (std::sin (t * 2.0f + y * 0.4f + k) * 1.2f), (int) jy + y, col, 0.35f * (1.0f - y / 24.0f));
                for (int y = -12; y <= 8; ++y) for (int x = -12; x <= 12; ++x)   // their soft light
                {
                    const float d = std::sqrt ((float) (x * x + y * y)) / 12.0f;
                    if (d < 1.0f) c.add ((int) jx + x, (int) jy + y, col, 0.06f * (1.0f - d) * (0.6f + 0.4f * pulse) * (1.4f - sim.daylight * 0.6f));
                }
            }
            break;
        }
        case PlaceOpenSea:
        {
            // the open sea: a big whale gliding far away, and little schools of fish turning together
            {
                const float wx = std::fmod (t * 6.0f, (float) (W + 260)) - 130.0f, wy = 150.0f + std::sin (t * 0.2f) * 10.0f;
                const Colour wc = mixc (0xff0c3a78, 0xff1a5aa0, 0.3f);
                for (int y = -14; y <= 14; ++y)
                    for (int x = -60; x <= 60; ++x)
                    {
                        const float u = (x + 60) / 120.0f;
                        const float half = 14.0f * std::sin (juce::jlimit (0.0f, 1.0f, u * 1.05f) * 3.14159f) * (u < 0.8f ? 1.0f : 0.6f);
                        if (std::abs ((float) y) <= half) c.blend ((int) wx + x, (int) wy + y, wc, 0.45f);
                    }
                for (int k = 0; k < 12; ++k)   // the tail
                {
                    const int tx = (int) wx - 60 - k, ty = (int) wy + (int) (std::sin (t * 1.2f) * k * 0.3f);
                    c.blend (tx, ty - k / 2, wc, 0.45f); c.blend (tx, ty + k / 2, wc, 0.45f);
                }
            }
            for (int s = 0; s < 3; ++s)
            {
                const float cx = std::fmod (hashf ((juce::uint32) s * 19 + 4) * W + t * (8.0f + s * 3.0f), (float) (W + 60)) - 30.0f;
                const float cy = 90.0f + s * 45.0f + std::sin (t * 0.5f + s) * 15.0f;
                for (int i = 0; i < 14; ++i)
                {
                    const float a = i * 0.45f + t * 0.8f;
                    const int fx = (int) (cx + std::cos (a) * (6 + i % 5)), fy = (int) (cy + std::sin (a * 1.3f) * (4 + i % 3));
                    c.blend (fx, fy, 0xffcfe8ff, 0.7f); c.blend (fx - 1, fy, 0xff9fc8f0, 0.5f);
                }
            }
            break;
        }
        case PlaceRuins:
        {
            // columns of a sunken city, an arch, and a big stone cat head (lit like everything else, mossy, and
            // seen through a little water so they sit in the scene)
            const float lit = 0.45f + 0.55f * sim.daylight;
            auto st = [&] (Colour col) { return shade (mixc (col, 0xff3f8fb0, 0.18f), lit); };
            const Colour stone = st (0xffd9d1bd), stoneLight = st (0xffeee6d2), stoneDark = st (0xffa69d86), moss = st (0xff5f9a4a);
            const int bases[] = { 70, 120, 300, 352 };
            const int heights[] = { 80, 54, 92, 92 };
            for (int k = 0; k < 4; ++k)
            {
                const int x0 = bases[k], hgt = heights[k];
                for (int y = 0; y < hgt; ++y)
                    for (int x = 0; x < 10; ++x)
                    {
                        Colour col = x == 0 || x == 9 ? stoneDark : (x < 3 ? stoneLight : (x > 6 ? shade (stone, 0.88f) : stone));
                        if (x % 3 == 1 && x > 0 && x < 9) col = shade (col, 0.93f);                 // fluting
                        const auto h = hash ((juce::uint32) (k * 977 + x * 31 + y * 7));
                        if (y < 10 && h % 3 == 0) col = moss;                                          // moss at the foot
                        else if (h % 41 == 0) col = stoneDark;                                         // cracks
                        c.set (x0 + x, (int) floorY + 2 - y, col);
                    }
                // capital (top) and base
                for (int x = -2; x < 12; ++x)
                {
                    c.set (x0 + x, (int) floorY + 2 - hgt, stoneLight); c.set (x0 + x, (int) floorY + 1 - hgt, stone); c.set (x0 + x, (int) floorY - hgt, stoneDark);
                    c.set (x0 + x, (int) floorY + 3, stoneDark);
                }
            }
            // the arch resting on the two tall columns at the right
            for (int x = -2; x < 64; ++x)
            {
                const float u = (x + 2) / 65.0f;
                const int y = (int) (std::sin (u * 3.14159f) * 16.0f);
                for (int k = 0; k < 6; ++k)
                    c.set (300 + x, (int) floorY - 92 - y - k + 1, k == 0 ? stoneDark : (k == 5 ? stoneLight : stone));
            }
            // a fallen column piece lying in the sand
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 30; ++x)
                    c.set (150 + x, (int) floorY + 3 - y, y == 7 ? stoneLight : (y == 0 ? stoneDark : (x % 6 == 0 ? shade (stone, 0.9f) : stone)));
            // the stone cat head
            const int hx = 220, hy = (int) floorY - 6;
            for (int y = -12; y <= 10; ++y)
                for (int x = -14; x <= 14; ++x)
                    if (x * x / 196.0f + y * y / 144.0f <= 1.0f) c.set (hx + x, hy + y, x > 6 ? stoneDark : (x < -6 && y < 0 ? stoneLight : stone));
            for (int k = 0; k < 7; ++k) { c.set (hx - 11 + k / 2, hy - 10 - k, stone); c.set (hx + 11 - k / 2, hy - 10 - k, stoneDark); }
            c.fillRect (hx - 7, hy - 3, 3, 3, st (0xff4a4538)); c.fillRect (hx + 4, hy - 3, 3, 3, st (0xff4a4538));
            c.set (hx - 1, hy + 2, st (0xff8f6a5a)); c.set (hx, hy + 2, st (0xff8f6a5a));
            break;
        }
        case PlaceDeep:
        {
            // marine snow drifting down
            for (int i = 0; i < 90; ++i)
            {
                const float x = hashf ((juce::uint32) i * 13 + 7) * W + std::sin (t * 0.4f + i) * 4.0f;
                const float y = std::fmod (hashf ((juce::uint32) i * 29 + 3) * H + t * (3.0f + (i % 5)), (float) H);
                if (y > surfaceY + 4.0f) c.blend ((int) x, (int) y, 0xffcfe3ff, 0.45f);
            }
            break;
        }
        default: break;
    }
}

void Painter::boat (Canvas& c, const Sim& sim, float t)
{
    const auto& cat = sim.cat;
    const float storm = sim.weatherLevel;
    const int bx = (int) cat.x;
    auto surfAt = [&] (float x) { return surfaceY + std::sin (x * 0.07f + t * 1.4f) * (1.0f + storm * 2.5f) + std::sin (x * 0.19f - t * 2.1f) * 0.6f; };
    const int sy = (int) std::lround (surfAt ((float) bx));
    const float rock = std::sin (t * 1.3f) * (0.6f + storm * 1.5f);
    auto tilt = [&] (int dx) { return (int) std::lround (dx * rock * 0.06f); };
    const Colour ink = 0xff1b1f33;

    // the fishing line goes into the water (behind the boat)
    if (cat.mood == 0)
    {
        const int tipX = bx + 13, tipY = sy - 17 + tilt (13);
        const int bob = (int) std::lround (surfAt ((float) tipX + 4.0f));
        for (int y = tipY; y < bob; ++y) c.blend (tipX + (y - tipY) / 6, y, 0xffe8ecf4, 0.6f);
        c.set (tipX + (bob - tipY) / 6, bob, 0xffe43b44); c.set (tipX + (bob - tipY) / 6, bob - 1, 0xffffffff);
        for (int y = bob + 1; y < bob + 30; ++y) c.blend (tipX + (bob - tipY) / 6, y, 0xffe8ecf4, 0.35f);
    }
    // the hull
    for (int r = 0; r < 6; ++r)
        for (int dx = -14 + r; dx <= 14 - r; ++dx)
        {
            const Colour col = r == 0 ? 0xffb8794a : r == 5 ? 0xff5a3a24 : (dx % 5 == 0 ? 0xff7a4a2a : 0xff8f563b);
            c.set (bx + dx, sy - 3 + r + tilt (dx), col);
        }
    for (int dx = -14; dx <= 14; ++dx) c.set (bx + dx, sy - 4 + tilt (dx), ink);

    // the cat (orange, white muzzle)
    const Colour fur = 0xfff7922e, furDark = 0xffc96a1c;
    const int cx = bx - 2, base = sy - 5 + tilt (-2);
    if (cat.mood == 1)
    {
        // napping, curled up
        for (int y = 0; y < 3; ++y) for (int x = -4; x <= 4; ++x) if (std::abs (x) + y < 6) c.set (cx + x, base - y, y == 0 ? furDark : fur);
        c.set (cx + 4, base - 3, fur); c.set (cx + 5, base - 3, fur); c.set (cx + 3, base - 4, fur); c.set (cx + 5, base - 4, fur);   // head + ears
        c.set (cx + 4, base - 2, ink);   // closed eye
        c.text ("Z", cx + 6, base - 10 - (int) std::fmod (t * 2.0f, 4.0f), 0xffc0a7f0, 1, 0xff06264a, true);
    }
    else
    {
        // sitting: body, head with ears, tail
        for (int y = 0; y < 5; ++y) for (int x = -3; x <= 2; ++x) c.set (cx + x, base - y, x == 2 || y == 0 ? furDark : fur);
        for (int y = 0; y < 4; ++y) for (int x = -2; x <= 2; ++x) c.set (cx + x + 1, base - 5 - y, fur);
        c.set (cx - 1, base - 9, fur); c.set (cx + 3, base - 9, fur); c.set (cx - 1, base - 10, furDark); c.set (cx + 3, base - 10, furDark);   // ears
        c.set (cx, base - 7, ink); c.set (cx + 2, base - 7, ink);   // eyes
        c.set (cx + 1, base - 6, 0xfff6757a);                        // nose
        c.set (cx + 3, base - 6, 0xffffffff);                        // muzzle
        for (int k = 0; k < 4; ++k) c.set (cx - 4 - k / 2, base - 1 - k, k == 3 ? furDark : fur);   // tail
        if (cat.mood == 0)
        {
            // the rod in its paws
            for (int k = 0; k < 14; ++k) c.set (cx + 3 + k * 13 / 14, base - 3 - (k * 9) / 14 + tilt (k), 0xff6b4630);
        }
        else
        {
            // waving hello
            const bool up = std::sin (cat.wave * 12.0f) > 0.0f;
            c.set (cx + 4, base - 4, fur); c.set (cx + 5, base - 6 + (up ? 0 : 1), fur); c.set (cx + (up ? 6 : 5), base - 8 + (up ? 0 : 1), fur);
            if (cat.waveAt.isNotEmpty())
            {
                const auto text = "HI " + cat.waveAt.toUpperCase().substring (0, 14) + "!";
                c.text (text, juce::jlimit (2, W - 2 - Canvas::textWidth (text), cx - Canvas::textWidth (text) / 2), base - 20, 0xffffffff);
            }
        }
        if (storm > 0.3f)
        {
            // an umbrella in the rain
            for (int dx = -7; dx <= 7; ++dx)
            {
                const int h = 4 - (dx * dx) / 12;
                for (int k = 0; k < juce::jmax (1, h); ++k) c.set (cx + 1 + dx, base - 14 - k, (dx / 3) % 2 == 0 ? 0xffe43b44 : 0xfffee761);
            }
            for (int k = 0; k < 5; ++k) c.set (cx + 1, base - 13 + k, ink);
        }
    }

    // a visitor on the bow
    if (cat.visitor != VisitorNone)
    {
        const int vx = bx + 9, vy = sy - 5 + tilt (9);
        if (cat.visitor == VisitorGull)
        {
            for (int y = 0; y < 3; ++y) for (int x = -2; x <= 2; ++x) c.set (vx + x, vy - y, y == 0 ? 0xffc8ccd4 : 0xfff4f6fa);
            c.set (vx + 2, vy - 3, 0xfff4f6fa); c.set (vx + 3, vy - 3, 0xfff4f6fa); c.set (vx + 3, vy - 4, 0xfff4f6fa);   // head
            c.set (vx + 4, vy - 3, 0xffffb02e); c.set (vx + 3, vy - 4, ink);                                           // beak, eye
            c.set (vx - 2, vy - 2, 0xff8a92a0); c.set (vx - 3, vy - 1, 0xff8a92a0);                                   // wing
        }
        else if (cat.visitor == VisitorGreyCat)
        {
            const Colour g = 0xffa8acb8, gd = 0xff7a7e8c;
            for (int y = 0; y < 4; ++y) for (int x = -2; x <= 1; ++x) c.set (vx + x, vy - y, x == 1 || y == 0 ? gd : g);
            for (int y = 0; y < 3; ++y) for (int x = -1; x <= 2; ++x) c.set (vx + x, vy - 4 - y, g);
            c.set (vx - 1, vy - 7, gd); c.set (vx + 2, vy - 7, gd); c.set (vx, vy - 5, ink); c.set (vx + 2, vy - 5, ink);
            for (int k = 0; k < 3; ++k) c.set (vx - 3 - k / 2, vy - 1 - k, g);
        }
        else
        {
            const Colour d = 0xffc8925a, dd = 0xff8a5a32;
            for (int y = 0; y < 3; ++y) for (int x = -3; x <= 1; ++x) c.set (vx + x, vy - y, y == 0 ? dd : d);
            for (int y = 0; y < 3; ++y) for (int x = 0; x <= 3; ++x) c.set (vx + x, vy - 3 - y, d);
            c.set (vx + 4, vy - 4, dd); c.set (vx, vy - 6, dd); c.set (vx + 1, vy - 7, dd);   // nose, floppy ear
            c.set (vx + 2, vy - 5, ink);
            if (std::sin (t * 10.0f) > 0.0f) c.set (vx - 4, vy - 2, d); else c.set (vx - 4, vy - 3, d);   // wagging tail
        }
    }

    // what it caught, bobbing over the boat
    if (cat.item != CatchNone && cat.itemTime > 0.0f)
    {
        const int ix = cx + 4, iy = base - 22 - (int) std::lround (std::sin (t * 3.0f) * 1.5f);
        switch (cat.item)
        {
            case CatchBoot:     c.fillRect (ix - 2, iy - 4, 3, 5, 0xff5a3a24); c.fillRect (ix - 2, iy, 6, 2, 0xff5a3a24); c.set (ix - 1, iy - 4, 0xff7a5a3a); break;
            case CatchBottle:   c.fillRect (ix - 1, iy - 5, 3, 7, 0xff7ad8a8, 0.8f); c.set (ix, iy - 6, 0xff8a6a4a); c.fillRect (ix, iy - 3, 1, 3, 0xfff4e8c8); break;
            case CatchCan:      c.fillRect (ix - 2, iy - 3, 4, 5, 0xffa8b0bc); c.fillRect (ix - 2, iy - 1, 4, 1, 0xffe43b44); break;
            case CatchStarfish: for (int k = -2; k <= 2; ++k) { c.set (ix + k, iy, 0xffff8a5a); c.set (ix, iy + k, 0xffff8a5a); } c.set (ix - 1, iy - 1, 0xffff8a5a); c.set (ix + 1, iy - 1, 0xffff8a5a); break;
            case CatchFish:     c.fillRect (ix - 2, iy - 1, 4, 3, 0xff7ab8ff); c.set (ix - 3, iy - 1, 0xff5a98df); c.set (ix - 3, iy + 1, 0xff5a98df); c.set (ix + 1, iy - 1, ink); break;
            case CatchDuck:     c.fillRect (ix - 2, iy - 1, 5, 3, 0xfffee761); c.fillRect (ix + 1, iy - 3, 2, 2, 0xfffee761); c.set (ix + 3, iy - 2, 0xffffa02e); c.set (ix + 1, iy - 3, ink); break;
            case CatchShell:    for (int y = 0; y < 3; ++y) for (int x = -2; x <= 2; ++x) if (std::abs (x) <= 2 - y / 2) c.set (ix + x, iy - y, x % 2 == 0 ? 0xffffd8e8 : 0xffffb6d0); break;
            case CatchTreasure:
            {
                c.fillRect (ix - 3, iy - 3, 7, 5, 0xff8a5a2a); c.fillRect (ix - 3, iy - 4, 7, 2, 0xffb07a3a); c.set (ix, iy - 2, 0xfffee761);
                for (int k = 0; k < 5; ++k) { const float a2 = t * 2.0f + k * 1.26f; c.add (ix + (int) (std::cos (a2) * 6.0f), iy - 2 + (int) (std::sin (a2) * 4.0f), 0xfffee761, 0.8f); }
                break;
            }
            default: break;
        }
    }

    // hearts when petted or fed
    if (cat.hearts > 0.0f)
        for (int k = 0; k < 3; ++k)
        {
            const float life = std::fmod (t * 0.8f + k * 0.33f, 1.0f);
            const int hx = cx + 2 + (int) (std::sin (t * 2.0f + k * 2.0f) * 4.0f), hy = base - 12 - (int) (life * 14.0f);
            const float a = juce::jmin (1.0f, cat.hearts) * (1.0f - life);
            c.blend (hx - 1, hy, 0xffff5a8a, a); c.blend (hx + 1, hy, 0xffff5a8a, a); c.blend (hx, hy + 1, 0xffff5a8a, a);
            c.blend (hx - 1, hy + 1, 0xffff5a8a, a); c.blend (hx + 1, hy + 1, 0xffff5a8a, a); c.blend (hx, hy + 2, 0xffff5a8a, a);
        }

    // a speech bubble
    if (cat.say.isNotEmpty() && cat.sayTime > 0.0f && cat.mood != 2)
    {
        const auto text = cat.say.toUpperCase().substring (0, 44);
        const int tw = Canvas::textWidth (text);
        int x0 = juce::jlimit (2, W - tw - 8, cx - tw / 2), y0 = base - 30;
        if (y0 - 3 < boardBottom && x0 + tw + 4 > boardLeft)     // keep clear of the leaderboard
            x0 = juce::jmax (2, boardLeft - tw - 6);
        const float a = juce::jmin (1.0f, cat.sayTime * 2.0f);
        c.fillRect (x0 - 3, y0 - 2, tw + 6, 9, 0xfff4fbff, 0.92f * a);
        c.fillRect (x0 - 3, y0 - 3, tw + 6, 1, 0xffffffff, 0.7f * a);
        for (int k = 0; k < 3; ++k) c.blend (cx + 1 - k, y0 + 7 + k, 0xfff4fbff, 0.92f * a);   // the tail of the bubble
        c.text (text, x0, y0, 0xff1b2a4a, 1, 0xfff4fbff, false);
    }
    // hungry: a little fish-bone thought
    else if (cat.hunger > 0.75f && cat.mood == 0)
    {
        const int hx = cx + 6, hy = base - 16;
        for (int k = 0; k < 5; ++k) c.set (hx + k, hy, 0xffe8ecf4);
        c.set (hx + 1, hy - 1, 0xffe8ecf4); c.set (hx + 1, hy + 1, 0xffe8ecf4); c.set (hx + 3, hy - 1, 0xffe8ecf4); c.set (hx + 3, hy + 1, 0xffe8ecf4);
        c.set (hx + 5, hy - 1, 0xffe8ecf4); c.set (hx + 5, hy + 1, 0xffe8ecf4);
    }
}
} // namespace ocean

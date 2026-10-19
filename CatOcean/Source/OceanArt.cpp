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
            { 0xff35c2cf, 0xff1b72a8, 0xff0c2756, 0xffe3c48a, 0xffa9844f, 0xff55607a, 1.0f },   // coral reef
            { 0xff4cc3a4, 0xff1f7468, 0xff0b3135, 0xffb9b77a, 0xff6f6a43, 0xff4d5f58, 0.8f },   // kelp forest
            { 0xff16426a, 0xff0b1d3a, 0xff02060f, 0xff2a3352, 0xff10142a, 0xff222a44, 0.0f },   // deep trench
            { 0xff3aa7b5, 0xff1e5e78, 0xff0e2a40, 0xffc9b07e, 0xff7e6845, 0xff4c5566, 0.5f },   // shipwreck
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
    for (int i = 0; i < 18; ++i)
        weedSets[PlaceWreck].push_back ({ 6.0f + r.nextFloat() * (W - 12), 10 + r.nextInt (30), mixc (0xff4d6b2a, 0xff8a8a3a, r.nextFloat()), r.nextFloat() * 6.0f });
    sandNoise.resize ((size_t) W);
    for (auto& n : sandNoise) n = r.nextInt (1000);
}

void Painter::paint (Canvas& c, const Sim& sim, const MusicInfo& music, float dt)
{
    clock += dt;
    const float t = clock;
    background (c, sim, t);
    decor (c, sim, t);
    boat (c, sim, t);

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
    if (sim.place == PlaceDeep)
        for (auto* f : order)   // in the dark, every fish glows a little
            for (int y = -8; y <= 8; ++y)
                for (int x = -8; x <= 8; ++x)
                {
                    const float d = std::sqrt ((float) (x * x + y * y)) / 8.0f;
                    if (d < 1.0f) c.add ((int) f->x + x, (int) f->y + y, colourARGB (f->genes.colour1), 0.10f * (1.0f - d) * f->fade);
                }

    bubbles (c, sim, dt);
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
    const Colour skyTop = mixc (0xff6fc3f0, 0xff2e3442, storm), skyLow = mixc (0xffcff1fb, 0xff5d6676, storm);
    const auto& lk = look (sim.place);
    const Colour waterTop = mixc (lk.top, shade (lk.top, 0.55f), storm), waterMid = mixc (lk.mid, shade (lk.mid, 0.55f), storm),
                 waterDeep = mixc (lk.deep, shade (lk.deep, 0.5f), storm);

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

    // sun or a dark sky
    if (storm < 0.6f)
    {
        const float a = 1.0f - storm / 0.6f;
        for (int y = -9; y <= 9; ++y)
            for (int x = -9; x <= 9; ++x)
                if (x * x + y * y <= 64) c.blend (420 + x, 15 + y, 0xfffff2a8, a);
                else if (x * x + y * y <= 81) c.blend (420 + x, 15 + y, 0xfffff2a8, a * 0.35f);
    }
    // clouds
    const int clouds = 3 + (int) (storm * 6);
    for (int i = 0; i < clouds; ++i)
    {
        const float speed = 3.0f + hashf ((juce::uint32) i * 7 + 1) * 4.0f;
        const int cx = (int) std::fmod (hashf ((juce::uint32) i * 13 + 5) * (W + 80) + t * speed, (float) (W + 80)) - 40;
        const int cy = 6 + (int) (hashf ((juce::uint32) i * 3 + 2) * 20);
        const Colour col = mixc (0xfff4f8ff, 0xff7a8290, storm);
        for (int b = 0; b < 4; ++b)
        {
            const int bx = cx + b * 7 - 10, by = cy - (b == 1 || b == 2 ? 3 : 0), r = b == 1 || b == 2 ? 6 : 4;
            for (int y = -r; y <= r; ++y)
                for (int x = -r - 2; x <= r + 2; ++x)
                    if (x * x * 0.6f + y * y * 1.4f <= r * r && by + y < surfaceY - 2) c.set (bx + x, by + y, y > r / 2 ? shade (col, 0.85f) : col);
        }
    }

    // sun rays in calm water
    if (storm < 0.8f && lk.rays > 0.0f)
    {
        const float a = 0.07f * lk.rays * (1.0f - storm / 0.8f);
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

    // far rocks (silhouettes)
    for (int x = 0; x < W; ++x)
    {
        const float hgt = 18.0f + 10.0f * std::sin (x * 0.021f + 1.3f) + 6.0f * std::sin (x * 0.057f);
        for (int y = (int) (floorY - hgt); y < (int) floorY; ++y)
            c.blend (x, y, waterDeep, 0.35f);
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
            if (kelp) { if (k % 5 == 2) c.set (x + 1, y, light (w.colour, 0.2f)); if (k % 5 == 4) c.set (x - 1, y, shade (w.colour, 0.75f)); }
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
            c.set (x, y, col);
        }
    }
    // rocks
    for (auto& r : rocks)
    {
        for (int y = 0; y < r.h; ++y)
            for (int x = 0; x < r.w; ++x)
            {
                const float nx = (x - r.w * 0.5f) / (r.w * 0.5f), ny = (float) (r.h - y) / (float) r.h;
                if (nx * nx + ny * ny * 0.9f > 1.0f) continue;
                Colour col = mixc (lk.rock, light (lk.rock, 0.15f), (float) (r.x % 7) / 7.0f);
                if (ny > 0.7f && nx < 0.2f) col = light (col, 0.2f);
                else if (nx > 0.4f) col = shade (col, 0.8f);
                c.set (r.x + x - r.w / 2, (int) floorY + 4 - y, col);
            }
    }
    // the cave where viewers' fish sleep while they're away
    for (int y = 0; y < 30; ++y)
        for (int x = -28; x <= 28; ++x)
        {
            const float nx = x / 28.0f, ny = y / 30.0f;
            if (nx * nx + ny * ny > 1.0f) continue;
            const bool hole = (x / 14.0f) * (x / 14.0f) + (y / 16.0f) * (y / 16.0f) < 1.0f;
            c.set (440 + x, (int) floorY + 6 - y, hole ? 0xff0a0e1c : (x > 10 ? 0xff3c4458 : 0xff4f5a73));
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
        bubbleList.push_back ({ (float) (60 + 130 * rng.nextInt (3) + rng.nextInt (6)), floorY, 0.8f + rng.nextFloat() * 1.6f, 12.0f + rng.nextFloat() * 10.0f, rng.nextFloat() * 6 });
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
        else
        {
            c.blend (x - 1, y, 0xffd8f6ff, 0.75f); c.blend (x + 1, y, 0xffd8f6ff, 0.75f);
            c.blend (x, y - 1, 0xffd8f6ff, 0.75f); c.blend (x, y + 1, 0xffd8f6ff, 0.75f);
            c.blend (x, y, 0xffffffff, 0.15f); c.set (x - 1, y - 1, 0xffffffff);
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
            if (dy < -h * 0.5f) col = light (col, 0.25f);
            else if (dy > h * 0.55f) col = shade (col, 0.8f);
            put (bx0 + i, y, col);
        }
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
    auto panel = [&c] (int x, int y, int w, int h)
    {
        c.fillRect (x, y, w, h, 0xff0b1020, 0.72f);
        for (int i = x; i < x + w; ++i) { c.blend (i, y, 0xff2ce8f5, 0.45f); c.blend (i, y + h - 1, 0xff2ce8f5, 0.25f); }
        for (int j = y; j < y + h; ++j) { c.blend (x, j, 0xff2ce8f5, 0.35f); c.blend (x + w - 1, j, 0xff2ce8f5, 0.35f); }
    };
    juce::ignoreUnused (dt);

    // now playing
    {
        const juce::String l1 = "NOW PLAYING", l2 = m.style + "  " + m.key + " " + m.scale + "  " + juce::String (m.bpm) + " BPM",
                           l3 = "PART " + juce::String (m.piece + 1) + "  " + m.section + "   " + m.place,
                           l4 = "FISH PULL IT TO " + juce::String ((int) std::lround (m.targetBpm)) + " BPM " + m.targetKey;
        const int w = juce::jmax (Canvas::textWidth (l2), Canvas::textWidth (l4)) + 10;
        panel (4, 4, w, 31);
        c.text (l1, 9, 8, 0xff2ce8f5, 1, 0, false);
        c.text (l2.toUpperCase(), 9, 15, 0xffffffff, 1, 0, false);
        c.text (l3.toUpperCase(), 9, 22, 0xffc0a7f0, 1, 0, false);
        c.text (l4.toUpperCase(), 9, 29 - 1, 0xff9ef0c2, 1, 0, false);
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
        panel (x, 4, w, h);
        c.text (b.title, x + 5, 8, 0xfffee761, 1, 0, false);
        for (int i = 0; i < b.lines.size(); ++i)
            c.text ((b.empty ? juce::String() : juce::String (i + 1) + ".") + b.lines[i].toUpperCase().substring (0, 30), x + 5, 15 + 7 * i, 0xffffffff, 1, 0, false);
        c.text (stats, x + 5, 4 + h - 8, 0xff8b9bb4, 1, 0, false);
    }

    // the vote
    if (sim.vote.active)
    {
        const int w = 170, x = W / 2 - w / 2, y = 46, h = 14 + 8 * (int) sim.vote.options.size() + 4;
        panel (x, y, w, h);
        const juce::String title = sim.vote.kind == 0 ? "VOTE: NEXT MUSIC" : "VOTE: WHAT HAPPENS?";
        c.text (title, x + 5, y + 4, 0xff2ce8f5, 1, 0, false);
        const auto secs = juce::String ((int) std::ceil (sim.vote.left)) + "S";
        c.text (secs, x + w - 5 - Canvas::textWidth (secs), y + 4, 0xff8b9bb4, 1, 0, false);
        int total = 0;
        for (auto n : sim.vote.counts) total += n;
        for (size_t i = 0; i < sim.vote.options.size(); ++i)
        {
            const int oy = y + 13 + 8 * (int) i;
            const auto name = (sim.vote.kind == 0 ? styleName (sim.vote.options[i]) : eventName (sim.vote.options[i])).toUpperCase();
            c.text ("!VOTE " + juce::String ((int) i + 1) + " " + name, x + 5, oy, 0xffffffff, 1, 0, false);
            const int bw = total > 0 ? (int) (50.0f * sim.vote.counts[i] / total) : 0;
            c.fillRect (x + w - 62, oy, bw, 5, 0xff2ce8f5, 0.9f);
            c.fillRect (x + w - 62 + bw, oy, 50 - bw, 5, 0xff2a2f4e, 0.8f);
            c.text (juce::String (sim.vote.counts[i]), x + w - 9, oy, 0xffffffff, 1, 0, false);
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
    };
    tipTimer += 1.0f / 30.0f;
    if (tipTimer > 9.0f) { tipTimer = 0; tip = (tip + 1) % (int) (sizeof (tips) / sizeof (tips[0])); }
    c.fillRect (0, H - 11, W, 11, 0xff0b1020, 0.75f);
    juce::String line = tips[tip];
    Colour col = 0xffffffff;
    if (! sim.news.empty()) { line = sim.news.back().text.toUpperCase(); col = sim.news.back().colour; }
    line = line.substring (0, 118);
    c.text (line, W / 2 - Canvas::textWidth (line) / 2, H - 8, col, 1, 0, false);
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
        c.text ("Z", cx + 6, base - 10 - (int) std::fmod (t * 2.0f, 4.0f), 0xffc0a7f0, 1, 0, false);
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
}
} // namespace ocean

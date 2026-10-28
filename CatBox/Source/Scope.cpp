#include "Scope.h"
#include "Project.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace catbox
{
namespace
{
using Stroke = std::vector<ScopeXYZ>;
using Strokes = std::vector<Stroke>;
constexpr float twoPi = 6.2831853f;

enum Pic { PCat = 0, PStar, PHeart, PSmile, PNote, PLightning, PHouse, PSpiral, PFlower, PCube, PPyramid, PGlobe, PText, PDrawing, NumPics };

Stroke poly (std::initializer_list<std::pair<float, float>> pts, bool close = false)
{
    Stroke s;
    for (auto& p : pts) s.push_back ({ p.first, p.second, 0.0f });
    if (close && ! s.empty()) s.push_back (s.front());
    return s;
}

Stroke ellipse (float cx, float cy, float rx, float ry, int n = 40, float from = 0.0f, float to = twoPi)
{
    Stroke s;
    for (int i = 0; i <= n; ++i)
    {
        const float a = from + (to - from) * (float) i / (float) n;
        s.push_back ({ cx + rx * std::cos (a), cy + ry * std::sin (a), 0.0f });
    }
    return s;
}

float deg (float d) { return d * twoPi / 360.0f; }

Strokes makeBuiltIn (int pic)
{
    Strokes s;
    switch (pic)
    {
        case PCat:
        {
            auto f = [] (float d) { return std::make_pair (0.85f * std::cos (deg (d)), 0.72f * std::sin (deg (d))); };
            Stroke head;
            auto add = [&head] (std::pair<float, float> p) { head.push_back ({ p.first, p.second, 0.0f }); };
            for (float d = 100.0f; d >= 80.0f; d -= 5.0f) add (f (d));          // between the ears
            add ({ 0.78f, 1.08f });                                              // the right ear
            for (float d = 55.0f; d >= -235.0f; d -= 6.0f) add (f (d));          // round the cheeks and the chin
            add (f (-235.0f));
            add ({ -0.78f, 1.08f });                                             // the left ear
            add (f (100.0f));
            s.push_back (head);
            s.push_back (ellipse (-0.33f, 0.12f, 0.12f, 0.15f, 28));             // eyes
            s.push_back (poly ({ { -0.33f, 0.24f }, { -0.33f, 0.0f } }));
            s.push_back (ellipse (0.33f, 0.12f, 0.12f, 0.15f, 28));
            s.push_back (poly ({ { 0.33f, 0.24f }, { 0.33f, 0.0f } }));
            s.push_back (poly ({ { -0.08f, -0.08f }, { 0.08f, -0.08f }, { 0.0f, -0.18f } }, true));   // nose
            s.push_back (poly ({ { -0.26f, -0.3f }, { -0.13f, -0.37f }, { 0.0f, -0.18f }, { 0.13f, -0.37f }, { 0.26f, -0.3f } }));   // mouth
            s.push_back (poly ({ { 1.0f, -0.42f }, { 0.42f, -0.28f } }));      // whiskers
            s.push_back (poly ({ { 0.42f, -0.2f }, { 1.08f, -0.22f } }));
            s.push_back (poly ({ { 1.05f, -0.02f }, { 0.42f, -0.12f } }));
            s.push_back (poly ({ { -0.42f, -0.12f }, { -1.05f, -0.02f } }));
            s.push_back (poly ({ { -1.08f, -0.22f }, { -0.42f, -0.2f } }));
            s.push_back (poly ({ { -0.42f, -0.28f }, { -1.0f, -0.42f } }));
            break;
        }
        case PStar:
        {
            Stroke st;
            for (int i = 0; i <= 10; ++i)
            {
                const float a = deg (90.0f + 36.0f * (float) i), r = i % 2 == 0 ? 1.0f : 0.4f;
                st.push_back ({ r * std::cos (a), r * std::sin (a), 0.0f });
            }
            s.push_back (st);
            break;
        }
        case PHeart:
        {
            Stroke h;
            for (int i = 0; i <= 160; ++i)
            {
                const float t = twoPi * (float) i / 160.0f;
                const float x = 16.0f * std::pow (std::sin (t), 3.0f);
                const float y = 13.0f * std::cos (t) - 5.0f * std::cos (2.0f * t) - 2.0f * std::cos (3.0f * t) - std::cos (4.0f * t);
                h.push_back ({ x / 16.0f, y / 16.0f, 0.0f });
            }
            s.push_back (h);
            break;
        }
        case PSmile:
            s.push_back (ellipse (0.0f, 0.0f, 1.0f, 1.0f, 72));
            s.push_back (ellipse (-0.35f, 0.3f, 0.1f, 0.14f, 20));
            s.push_back (ellipse (0.35f, 0.3f, 0.1f, 0.14f, 20));
            s.push_back (ellipse (0.0f, 0.0f, 0.6f, 0.6f, 30, deg (200.0f), deg (340.0f)));
            break;
        case PNote:
            s.push_back (ellipse (-0.52f, -0.62f, 0.26f, 0.18f, 28));
            s.push_back (poly ({ { -0.27f, -0.58f }, { -0.27f, 0.72f }, { 0.68f, 0.9f }, { 0.68f, -0.42f } }));
            s.push_back (ellipse (0.43f, -0.46f, 0.26f, 0.18f, 28));
            s.push_back (poly ({ { 0.68f, 0.7f }, { -0.27f, 0.52f } }));
            break;
        case PLightning:
            s.push_back (poly ({ { 0.25f, 1.0f }, { -0.5f, -0.08f }, { 0.0f, -0.08f }, { -0.25f, -1.0f }, { 0.55f, 0.22f }, { 0.06f, 0.22f } }, true));
            break;
        case PHouse:
            s.push_back (poly ({ { -0.7f, 0.2f }, { -0.7f, -0.9f }, { -0.2f, -0.9f }, { -0.2f, -0.35f }, { 0.2f, -0.35f }, { 0.2f, -0.9f }, { 0.7f, -0.9f }, { 0.7f, 0.2f } }));
            s.push_back (poly ({ { 0.85f, 0.2f }, { 0.0f, 0.95f }, { -0.85f, 0.2f }, { 0.85f, 0.2f } }));
            s.push_back (poly ({ { 0.32f, -0.12f }, { 0.55f, -0.12f }, { 0.55f, 0.08f }, { 0.32f, 0.08f } }, true));
            break;
        case PSpiral:
        {
            Stroke sp;
            for (int i = 0; i <= 320; ++i)
            {
                const float t = (float) i / 320.0f, a = t * twoPi * 4.0f;
                sp.push_back ({ t * std::cos (a), t * std::sin (a), 0.0f });
            }
            s.push_back (sp);
            break;
        }
        case PFlower:
        {
            Stroke fl;
            for (int i = 0; i <= 360; ++i)
            {
                const float a = twoPi * (float) i / 360.0f, r = 0.25f + 0.75f * std::abs (std::cos (2.5f * a));
                fl.push_back ({ r * std::cos (a), r * std::sin (a), 0.0f });
            }
            s.push_back (fl);
            s.push_back (ellipse (0.0f, 0.0f, 0.18f, 0.18f, 24));
            break;
        }
        case PCube:
        {
            const float c = 0.6f;
            Stroke bottom, top;
            const float sq[5][2] = { { -c, -c }, { c, -c }, { c, c }, { -c, c }, { -c, -c } };
            for (auto& p : sq) { bottom.push_back ({ p[0], -c, p[1] }); top.push_back ({ p[0], c, p[1] }); }
            s.push_back (bottom);
            s.push_back (top);
            for (int i = 0; i < 4; ++i) s.push_back ({ { sq[i][0], c, sq[i][1] }, { sq[i][0], -c, sq[i][1] } });
            break;
        }
        case PPyramid:
        {
            const float c = 0.65f;
            const ScopeXYZ a { -c, -0.6f, -c }, b { c, -0.6f, -c }, cc { c, -0.6f, c }, d { -c, -0.6f, c }, apex { 0.0f, 0.75f, 0.0f };
            s.push_back ({ a, b, cc, d, a, apex, cc });
            s.push_back ({ b, apex, d });
            break;
        }
        case PGlobe:
        {
            Stroke eq, m1, m2, l1, l2;
            for (int i = 0; i <= 48; ++i)
            {
                const float a = twoPi * (float) i / 48.0f, ca = std::cos (a), sa = std::sin (a);
                eq.push_back ({ ca, 0.0f, sa });
                m1.push_back ({ sa, ca, 0.0f });
                m2.push_back ({ 0.0f, ca, sa });
                l1.push_back ({ 0.866f * ca, 0.5f, 0.866f * sa });
                l2.push_back ({ 0.866f * ca, -0.5f, 0.866f * sa });
            }
            s = { eq, l1, m1, m2, l2 };
            break;
        }
        default: break;
    }
    return s;
}

Strokes textStrokes (const juce::String& text)
{
    juce::Path all;
    auto lines = juce::StringArray::fromLines (text.trim().isEmpty() ? juce::String ("CAT BOX") : text.trim());
    lines.removeEmptyStrings();
    while (lines.size() > 3) lines.remove (3);
    const juce::Font f (juce::FontOptions (100.0f, juce::Font::bold));
    for (int i = 0; i < lines.size(); ++i)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, lines[i].substring (0, 24), 0.0f, 0.0f);
        juce::Path lp;
        ga.createPath (lp);
        const auto b = lp.getBounds();
        all.addPath (lp, juce::AffineTransform::translation (-b.getCentreX(), 115.0f * (float) i));
    }
    Strokes s;
    juce::PathFlatteningIterator it (all, juce::AffineTransform(), 1.2f);
    int sub = -1;
    while (it.next())
    {
        if (it.subPathIndex != sub || s.empty()) { s.push_back ({}); s.back().push_back ({ it.x1, -it.y1, 0.0f }); sub = it.subPathIndex; }
        s.back().push_back ({ it.x2, -it.y2, 0.0f });
    }
    return s;
}

Strokes drawingStrokes (const std::vector<float>& d)
{
    Strokes s;
    bool newStroke = true;
    for (size_t i = 0; i + 1 < d.size(); i += 2)
    {
        if (d[i] >= scopePenUp) { newStroke = true; continue; }
        if (newStroke) { s.push_back ({}); newStroke = false; }
        s.back().push_back ({ d[i], d[i + 1], 0.0f });
    }
    return s;
}

/** Fits the strokes into -0.92..0.92 (keeping the shape) and walks them evenly; jumps between strokes go fast. */
void buildTable (Strokes strokes, ScopeXYZ* out, std::vector<std::vector<juce::Point<float>>>* outline, bool keepSize = false)
{
    strokes.erase (std::remove_if (strokes.begin(), strokes.end(), [] (const Stroke& s) { return s.empty(); }), strokes.end());
    if (outline != nullptr) outline->clear();
    if (strokes.empty()) { for (int i = 0; i < scopePoints; ++i) out[i] = {}; return; }
    // the order: always the nearest stroke next (either way round), so the jumps are short
    {
        auto d2 = [] (const ScopeXYZ& a, const ScopeXYZ& b) { return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z); };
        Strokes ordered;
        ordered.reserve (strokes.size());
        std::vector<bool> used (strokes.size(), false);
        ordered.push_back (strokes[0]);
        used[0] = true;
        for (size_t n = 1; n < strokes.size(); ++n)
        {
            const auto& end = ordered.back().back();
            size_t best = 0;
            bool flip = false;
            float bd = 1e30f;
            for (size_t i = 0; i < strokes.size(); ++i)
            {
                if (used[i]) continue;
                const float a = d2 (end, strokes[i].front()), b = d2 (end, strokes[i].back());
                if (a < bd) { bd = a; best = i; flip = false; }
                if (b < bd) { bd = b; best = i; flip = true; }
            }
            used[best] = true;
            ordered.push_back (strokes[best]);
            if (flip) std::reverse (ordered.back().begin(), ordered.back().end());
        }
        strokes = std::move (ordered);
    }
    float lo[3] { 1e9f, 1e9f, 1e9f }, hi[3] { -1e9f, -1e9f, -1e9f };
    for (auto& s : strokes)
        for (auto& p : s)
        {
            const float v[3] { p.x, p.y, p.z };
            for (int k = 0; k < 3; ++k) { lo[k] = std::min (lo[k], v[k]); hi[k] = std::max (hi[k], v[k]); }
        }
    const float cx = 0.5f * (lo[0] + hi[0]), cy = 0.5f * (lo[1] + hi[1]), cz = 0.5f * (lo[2] + hi[2]);
    float half = std::max ({ 0.5f * (hi[0] - lo[0]), 0.5f * (hi[1] - lo[1]), 1e-4f });
    if (hi[2] - lo[2] > 1e-4f)   // 3D: it turns, so the farthest corner must fit whichever way it faces
    {
        half = 1e-4f;
        for (auto& s : strokes)
            for (auto& p : s) half = std::max (half, std::sqrt ((p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy) + (p.z - cz) * (p.z - cz)));
    }
    const float k = keepSize ? 1.0f : 0.92f / half;
    for (auto& s : strokes)
    {
        for (auto& p : s) { p.x = (p.x - cx) * k; p.y = (p.y - cy) * k; p.z = (p.z - cz) * k; }
        if (outline != nullptr)
        {
            outline->push_back ({});
            for (auto& p : s) outline->back().push_back ({ p.x + p.z * 0.35f, p.y + p.z * 0.2f });   // 3D: seen a little from the side
        }
    }

    struct Seg { ScopeXYZ a, b; float len; };
    std::vector<Seg> segs;
    auto dist = [] (const ScopeXYZ& a, const ScopeXYZ& b) { return std::sqrt ((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); };
    for (size_t i = 0; i < strokes.size(); ++i)
    {
        const auto& s = strokes[i];
        if (s.size() == 1) segs.push_back ({ s[0], s[0], 0.03f });   // a dot: the beam rests a moment
        for (size_t j = 0; j + 1 < s.size(); ++j) segs.push_back ({ s[j], s[j + 1], dist (s[j], s[j + 1]) });
        const auto& next = strokes[(i + 1) % strokes.size()];
        segs.push_back ({ s.back(), next.front(), dist (s.back(), next.front()) * 0.12f });   // the jump: quick, so it hardly shows
    }
    float total = 0.0f;
    for (auto& sg : segs) total += sg.len;
    if (total <= 1e-6f) { for (int i = 0; i < scopePoints; ++i) out[i] = strokes[0][0]; return; }
    size_t si = 0;
    float at = 0.0f;   // where segment si starts
    for (int i = 0; i < scopePoints; ++i)
    {
        const float want = total * (float) i / (float) scopePoints;
        while (si + 1 < segs.size() && at + segs[si].len < want) { at += segs[si].len; ++si; }
        const auto& sg = segs[si];
        const float f = sg.len > 0.0f ? juce::jlimit (0.0f, 1.0f, (want - at) / sg.len) : 0.0f;
        out[i] = { sg.a.x + (sg.b.x - sg.a.x) * f, sg.a.y + (sg.b.y - sg.a.y) * f, sg.a.z + (sg.b.z - sg.a.z) * f };
    }
}

struct BuiltIn { std::array<ScopeXYZ, scopePoints> table; std::vector<std::vector<juce::Point<float>>> outline; };
std::array<BuiltIn, PText>& builtIns()
{
    static std::array<BuiltIn, PText> b;
    return b;
}
std::atomic<bool> ready { false };

struct Custom
{
    std::array<std::array<ScopeXYZ, scopePoints>, 2> table {};
    std::atomic<int> active { 0 };
    std::vector<std::vector<juce::Point<float>>> outline;   // the message thread's
    bool built = false;
};
std::array<std::array<Custom, 2>, numTracks>& customs()
{
    static std::array<std::array<Custom, 2>, numTracks> c;
    return c;
}

void setCustom (int track, int which, const Strokes& s)
{
    auto& c = customs()[(size_t) juce::jlimit (0, numTracks - 1, track)][(size_t) which];
    const int free = 1 - c.active.load();
    buildTable (s, c.table[(size_t) free].data(), &c.outline);
    c.active.store (free);
    c.built = true;
}
} // namespace

juce::StringArray scopePictureNames()
{
    return { "Cat", "Star", "Heart", "Smile", "Note", "Lightning", "House", "Spiral", "Flower", "Cube", "Pyramid", "Globe", "Text", "Drawing" };
}
int scopePictureText() { return PText; }
int scopePictureDrawing() { return PDrawing; }
bool scopePicture3D (int p) { return p == PCube || p == PPyramid || p == PGlobe; }

void scopeInit()
{
    if (ready.exchange (true)) return;
    for (int p = 0; p < PText; ++p) buildTable (makeBuiltIn (p), builtIns()[(size_t) p].table.data(), &builtIns()[(size_t) p].outline);
    for (int t = 0; t < numTracks; ++t)
    {
        if (! customs()[(size_t) t][0].built) setCustom (t, 0, textStrokes ({}));
        if (! customs()[(size_t) t][1].built) setCustom (t, 1, textStrokes ("DRAW"));
    }
}

const ScopeXYZ* scopePicture (int pic, int track)
{
    pic = juce::jlimit (0, NumPics - 1, pic);
    if (pic >= PText)
    {
        const auto& c = customs()[(size_t) juce::jlimit (0, numTracks - 1, track)][(size_t) (pic - PText)];
        return c.table[(size_t) c.active.load()].data();
    }
    return builtIns()[(size_t) pic].table.data();
}

void setScopeText (int track, const juce::String& text) { scopeInit(); setCustom (track, 0, textStrokes (text)); }

void setScopeDrawing (int track, const std::vector<float>& d)
{
    scopeInit();
    auto s = drawingStrokes (d);
    if (s.empty()) s = textStrokes ("DRAW");
    setCustom (track, 1, s);
}

juce::String scopeDrawingToString (const std::vector<float>& d)
{
    juce::String s;
    s.preallocateBytes (d.size() * 5);
    for (auto x : d) s << juce::roundToInt (juce::jlimit (-1.5f, scopePenUp, x) * 1000.0f) << ' ';
    return s.trimEnd();
}

std::vector<float> scopeDrawingFromString (const juce::String& s)
{
    std::vector<float> d;
    for (auto& tok : juce::StringArray::fromTokens (s, " ", ""))
        if (tok.isNotEmpty()) d.push_back ((float) tok.getIntValue() / 1000.0f);
    if (d.size() % 2 != 0) d.pop_back();
    return d;
}

std::vector<std::vector<juce::Point<float>>> scopeOutline (int pic, int track)
{
    scopeInit();
    pic = juce::jlimit (0, NumPics - 1, pic);
    if (pic >= PText) return customs()[(size_t) juce::jlimit (0, numTracks - 1, track)][(size_t) (pic - PText)].outline;
    return builtIns()[(size_t) pic].outline;
}
} // namespace catbox

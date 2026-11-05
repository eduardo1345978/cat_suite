#include "LavaSim.h"

namespace
{
constexpr float h = 12.0f;            // interaction radius (field pixels)
constexpr float kernelR = 15.0f;      // drawing radius
constexpr int numParticles = 280;

struct RGB { float r, g, b; };
RGB lavaColour (float t) noexcept
{
    // crust -> deep red -> orange -> yellow -> white hot
    static const float stops[][4] = { { 0.00f, 0.10f, 0.02f, 0.01f }, { 0.22f, 0.40f, 0.06f, 0.02f }, { 0.42f, 0.76f, 0.15f, 0.04f },
                                      { 0.62f, 1.00f, 0.40f, 0.09f }, { 0.80f, 1.00f, 0.68f, 0.26f }, { 1.00f, 1.00f, 0.95f, 0.75f } };
    t = juce::jlimit (0.0f, 1.0f, t);
    for (int i = 1; i < 6; ++i)
        if (t <= stops[i][0])
        {
            const float f = (t - stops[i - 1][0]) / (stops[i][0] - stops[i - 1][0]);
            return { stops[i - 1][1] + (stops[i][1] - stops[i - 1][1]) * f, stops[i - 1][2] + (stops[i][2] - stops[i - 1][2]) * f,
                     stops[i - 1][3] + (stops[i][3] - stops[i - 1][3]) * f };
        }
    return { 1.0f, 0.95f, 0.75f };
}

inline float smoothstep (float a, float b, float x) noexcept
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
}

LavaSim::LavaSim()
{
    noiseTable.resize (128 * 128);
    for (auto& v : noiseTable) v = rng.nextFloat();
    setSize (300, 210);
}

void LavaSim::setSize (int w, int hh)
{
    W = juce::jmax (60, w);
    H = juce::jmax (60, hh);
    field.assign ((size_t) (W * H), 0.0f);
    temp.assign ((size_t) (W * H), 0.0f);
    lava = juce::Image (juce::Image::ARGB, W, H, true);
    glowImage = juce::Image (juce::Image::ARGB, juce::jmax (1, W / 4), juce::jmax (1, H / 4), true);
    cx = W * 0.5f;
    cy = H * 0.47f;
    restR = juce::jmin (W, H) * 0.34f;
    reset();
}

void LavaSim::reset()
{
    ps.clear();
    comX = cx; comY = cy;
    // fill a disc evenly (sunflower spiral)
    for (int i = 0; i < numParticles; ++i)
    {
        const float r = restR * 0.92f * std::sqrt ((i + 0.5f) / numParticles);
        const float a = i * 2.39996f;
        const float qx = r * std::cos (a), qy = r * std::sin (a) * 0.9f;
        ps.push_back ({ cx + qx, cy + qy, 0.0f, 0.0f, 0.7f, false, qx, qy });
    }
}

int LavaSim::numFree() const noexcept
{
    int n = 0;
    for (auto& p : ps) n += p.free ? 1 : 0;
    return n;
}

// cellular noise: distance to the nearest and second-nearest point of a jittered grid, and the id of the nearest cell
// (its plates and cracks look like a cooling crust)
LavaSim::Cell LavaSim::cells (float x, float y) const noexcept
{
    const int ix = (int) std::floor (x), iy = (int) std::floor (y);
    float d1 = 1.0e9f, d2 = 1.0e9f;
    int id = 0;
    for (int yy = iy - 1; yy <= iy + 1; ++yy)
        for (int xx = ix - 1; xx <= ix + 1; ++xx)
        {
            const size_t k = (size_t) ((yy & 127) * 128 + (xx & 127));
            const float px = (float) xx + 0.15f + 0.7f * noiseTable[k], py = (float) yy + 0.15f + 0.7f * noiseTable[(k * 7 + 31) & (128 * 128 - 1)];
            const float d = (px - x) * (px - x) + (py - y) * (py - y);
            if (d < d1) { d2 = d1; d1 = d; id = (int) k; }
            else if (d < d2) d2 = d;
        }
    return { std::sqrt (d1), std::sqrt (d2), noiseTable[((size_t) id * 13 + 5) & (128 * 128 - 1)] };
}

float LavaSim::noise (float x, float y) const noexcept
{
    const int x0 = (int) std::floor (x), y0 = (int) std::floor (y);
    const float fx = x - (float) x0, fy = y - (float) y0;
    const float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    auto at = [this] (int xx, int yy) { return noiseTable[(size_t) ((yy & 127) * 128 + (xx & 127))]; };
    const float a = at (x0, y0), b = at (x0 + 1, y0), c = at (x0, y0 + 1), d = at (x0 + 1, y0 + 1);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

void LavaSim::poke (juce::Point<float> at, float strength)
{
    for (auto& p : ps)
    {
        const float dx = p.x - at.x, dy = p.y - at.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < 40.0f * 40.0f && d2 > 0.01f)
        {
            const float d = std::sqrt (d2), f = strength * (1.0f - d / 40.0f);
            p.vx += dx / d * f;
            p.vy += dy / d * f;
        }
    }
}

void LavaSim::dropFrom (int count)
{
    // the lowest point of the blob (a little to the left, where it hangs)
    int best = -1;
    float bestScore = -1.0e9f;
    for (size_t i = 0; i < ps.size(); ++i)
        if (! ps[i].free)
        {
            const float score = ps[i].y + 0.25f * (cx - ps[i].x) + rng.nextFloat() * 6.0f;
            if (score > bestScore) { bestScore = score; best = (int) i; }
        }
    if (best < 0) return;
    const auto origin = ps[(size_t) best];
    std::vector<std::pair<float, int>> near;
    for (size_t i = 0; i < ps.size(); ++i)
        if (! ps[i].free)
            near.push_back ({ (ps[i].x - origin.x) * (ps[i].x - origin.x) + (ps[i].y - origin.y) * (ps[i].y - origin.y), (int) i });
    std::partial_sort (near.begin(), near.begin() + juce::jmin ((int) near.size(), count), near.end());
    for (int k = 0; k < juce::jmin ((int) near.size(), count); ++k)
    {
        auto& p = ps[(size_t) near[(size_t) k].second];
        p.free = true;
        p.vy += 0.35f;
    }
}

void LavaSim::step (const Input& in)
{
    time += in.dt;
    heat += (in.heat - heat) * 0.08f;
    const float hh = heat;

    // drops: one for every falling echo, plus some on their own when it's hot
    if (lastDripCount < 0) lastDripCount = in.dripCount;
    const int newDrips = juce::jmin (3, in.dripCount - lastDripCount);
    lastDripCount = in.dripCount;
    if (numFree() < 60)
    {
        for (int i = 0; i < newDrips; ++i) dropFrom (4 + (int) (in.drip * 6.0f));
        if (rng.nextFloat() < in.drip * hh * in.dt * 0.6f) dropFrom (3 + (int) (hh * 4.0f));
    }

    // the sound: it breathes out with every rise in level, and bubbles when loud and hot
    const float rise = juce::jmax (0.0f, in.level - lastLevel);
    lastLevel += (in.level - lastLevel) * 0.5f;
    const float sway = in.wobble * 3.0f;
    const float ccx = cx + std::sin (time * 0.7f) * sway, ccy = cy + std::sin (time * 1.1f) * sway * 0.4f;
    for (auto& p : ps)
        if (! p.free)
        {
            const float dx = p.x - ccx, dy = p.y - ccy, d = std::sqrt (dx * dx + dy * dy) + 0.001f;
            p.vx += dx / d * rise * 2.2f;
            p.vy += dy / d * rise * 2.2f;
        }
    if (rng.nextFloat() < in.level * (0.3f + hh) * 0.8f)
    {
        auto& p = ps[(size_t) rng.nextInt ((int) ps.size())];
        if (! p.free && p.y < cy) p.vy -= 1.2f * (0.5f + in.level);
    }

    // simulation (two sub-steps a frame, in "step" units)
    const float g = 0.05f;
    const float damp = 0.985f - 0.015f * (1.0f - hh);
    const float k = 0.02f, kNear = 0.06f, rho0 = 3.2f;
    const float sigma = 0.03f + 0.10f * (1.0f - hh), beta = 0.02f;
    const int cs = (int) h;
    const int gw = W / cs + 2, gh = (H + 80) / cs + 2;
    juce::ignoreUnused (cs);
    for (int sub = 0; sub < 2; ++sub)
    {
        // the blob hangs from above: what's on top is held more than what's below, so the bottom sags
        // (more when hot) and the whole blob is pulled back to its place
        float mx = 0.0f, my = 0.0f; int na = 0;
        for (auto& p : ps) if (! p.free) { mx += p.x; my += p.y; ++na; }
        if (na > 0) { mx /= (float) na; my /= (float) na; }
        comX = mx; comY = my;
        const float sagK = 0.35f + 0.9f * hh;
        float wsum = 0.0f;
        for (auto& p : ps) if (! p.free) wsum += juce::jmax (0.05f, 1.0f + sagK * (my - p.y) / restR);
        const float wmean = na > 0 ? wsum / (float) na : 1.0f;
        const float pullX = (ccx - mx) * 0.004f, pullY = (ccy - my) * 0.004f;
        for (auto& p : ps)
        {
            p.vy += g;
            if (! p.free)
            {
                const float w = juce::jmax (0.05f, 1.0f + sagK * (my - p.y) / restR) / wmean;
                p.vy -= g * w;                 // held up (on average exactly against gravity)
                p.vx += pullX; p.vy += pullY;
            }
            p.vx *= damp; p.vy *= damp;
        }
        // neighbours (a grid)
        cellStart.assign ((size_t) (gw * gh), -1);
        cellNext.assign (ps.size(), -1);
        auto cellOf = [&] (const P& p)
        {
            const int x = juce::jlimit (0, gw - 1, (int) (p.x / h)), y = juce::jlimit (0, gh - 1, (int) (p.y / h));
            return y * gw + x;
        };
        for (size_t i = 0; i < ps.size(); ++i)
        {
            const int c = cellOf (ps[i]);
            cellNext[i] = cellStart[(size_t) c];
            cellStart[(size_t) c] = (int) i;
        }
        auto forNeighbours = [&] (size_t i, auto&& fn)
        {
            const int x = juce::jlimit (0, gw - 1, (int) (ps[i].x / h)), y = juce::jlimit (0, gh - 1, (int) (ps[i].y / h));
            for (int yy = juce::jmax (0, y - 1); yy <= juce::jmin (gh - 1, y + 1); ++yy)
                for (int xx = juce::jmax (0, x - 1); xx <= juce::jmin (gw - 1, x + 1); ++xx)
                    for (int j = cellStart[(size_t) (yy * gw + xx)]; j >= 0; j = cellNext[(size_t) j])
                        if ((size_t) j > i) fn ((size_t) j);
        };
        // viscosity (cold lava is thick)
        for (size_t i = 0; i < ps.size(); ++i)
            forNeighbours (i, [&] (size_t j)
            {
                auto& a = ps[i]; auto& b = ps[j];
                const float dx = b.x - a.x, dy = b.y - a.y, r = std::sqrt (dx * dx + dy * dy);
                if (r <= 0.0001f || r >= h) return;
                const float q = 1.0f - r / h, nx = dx / r, ny = dy / r;
                const float u = (a.vx - b.vx) * nx + (a.vy - b.vy) * ny;
                if (u > 0.0f)
                {
                    const float I = 0.5f * q * (sigma * u + beta * u * u);
                    a.vx -= I * nx; a.vy -= I * ny; b.vx += I * nx; b.vy += I * ny;
                }
            });
        // move, then push apart / pull together (double density relaxation: volume + surface tension)
        std::vector<float> px (ps.size()), py (ps.size());
        for (size_t i = 0; i < ps.size(); ++i) { px[i] = ps[i].x; py[i] = ps[i].y; ps[i].x += ps[i].vx; ps[i].y += ps[i].vy; }
        for (size_t i = 0; i < ps.size(); ++i)
        {
            float rho = 0.0f, rhoN = 0.0f;
            const int x = juce::jlimit (0, gw - 1, (int) (ps[i].x / h)), y = juce::jlimit (0, gh - 1, (int) (ps[i].y / h));
            for (int yy = juce::jmax (0, y - 1); yy <= juce::jmin (gh - 1, y + 1); ++yy)
                for (int xx = juce::jmax (0, x - 1); xx <= juce::jmin (gw - 1, x + 1); ++xx)
                    for (int j = cellStart[(size_t) (yy * gw + xx)]; j >= 0; j = cellNext[(size_t) j])
                    {
                        if ((size_t) j == i) continue;
                        const float dx = ps[(size_t) j].x - ps[i].x, dy = ps[(size_t) j].y - ps[i].y, r = std::sqrt (dx * dx + dy * dy);
                        if (r >= h) continue;
                        const float q = 1.0f - r / h;
                        rho += q * q; rhoN += q * q * q;
                    }
            const float P = k * (rho - rho0), PN = kNear * rhoN;
            float ddx = 0.0f, ddy = 0.0f;
            for (int yy = juce::jmax (0, y - 1); yy <= juce::jmin (gh - 1, y + 1); ++yy)
                for (int xx = juce::jmax (0, x - 1); xx <= juce::jmin (gw - 1, x + 1); ++xx)
                    for (int j = cellStart[(size_t) (yy * gw + xx)]; j >= 0; j = cellNext[(size_t) j])
                    {
                        if ((size_t) j == i) continue;
                        auto& b = ps[(size_t) j];
                        const float dx = b.x - ps[i].x, dy = b.y - ps[i].y, r = std::sqrt (dx * dx + dy * dy);
                        if (r >= h || r < 0.0001f) continue;
                        const float q = 1.0f - r / h;
                        const float D = (P * q + PN * q * q) * 0.5f;
                        b.x += D * dx / r; b.y += D * dy / r;
                        ddx -= D * dx / r; ddy -= D * dy / r;
                    }
            ps[i].x += ddx; ps[i].y += ddy;
        }
        // shape matching: the blob as a whole keeps its shape (stiff when cold, soft and saggy when hot),
        // so it can't tear apart - only the drops that let go leave it
        {
            float mx2 = 0.0f, my2 = 0.0f; int n2 = 0;
            for (auto& p : ps) if (! p.free) { mx2 += p.x; my2 += p.y; ++n2; }
            if (n2 > 8)
            {
                mx2 /= (float) n2; my2 /= (float) n2;
                float qmx = 0.0f, qmy = 0.0f;
                for (auto& p : ps) if (! p.free) { qmx += p.qx; qmy += p.qy; }
                qmx /= (float) n2; qmy /= (float) n2;
                float a00 = 0, a01 = 0, a10 = 0, a11 = 0, q00 = 0, q01 = 0, q11 = 0;
                for (auto& p : ps)
                    if (! p.free)
                    {
                        const float dx = p.x - mx2, dy = p.y - my2, qx = p.qx - qmx, qy = p.qy - qmy;
                        a00 += dx * qx; a01 += dx * qy; a10 += dy * qx; a11 += dy * qy;
                        q00 += qx * qx; q01 += qx * qy; q11 += qy * qy;
                    }
                // best rotation (kept small: lava doesn't spin) mixed with the best linear fit (lets it squash and stretch)
                const float ang = juce::jlimit (-0.25f, 0.25f, std::atan2 (a10 - a01, a00 + a11));
                const float ca = std::cos (ang), sa = std::sin (ang);
                const float det = q00 * q11 - q01 * q01;
                float l00 = ca, l01 = -sa, l10 = sa, l11 = ca;
                if (std::abs (det) > 1.0e-3f)
                {
                    const float i00 = q11 / det, i01 = -q01 / det, i11 = q00 / det;
                    float b00 = a00 * i00 + a01 * i01, b01 = a00 * i01 + a01 * i11, b10 = a10 * i00 + a11 * i01, b11 = a10 * i01 + a11 * i11;
                    // keep the area (lava doesn't grow or shrink)
                    const float bd = b00 * b11 - b01 * b10;
                    if (bd > 0.05f)
                    {
                        const float sc = 1.0f / std::sqrt (bd);
                        b00 *= sc; b01 *= sc; b10 *= sc; b11 *= sc;
                        const float mixB = 0.35f + 0.45f * hh;
                        l00 += (b00 - l00) * mixB; l01 += (b01 - l01) * mixB; l10 += (b10 - l10) * mixB; l11 += (b11 - l11) * mixB;
                    }
                }
                const float stiff = 0.10f + 0.22f * (1.0f - hh);
                for (auto& p : ps)
                    if (! p.free)
                    {
                        const float qx = p.qx - qmx, qy = p.qy - qmy;
                        const float gx = mx2 + l00 * qx + l01 * qy, gy = my2 + l10 * qx + l11 * qy;
                        p.x += (gx - p.x) * stiff; p.y += (gy - p.y) * stiff;
                    }
            }
        }
        for (size_t i = 0; i < ps.size(); ++i)
        {
            auto& p = ps[i];
            p.vx = juce::jlimit (-6.0f, 6.0f, p.x - px[i]);
            p.vy = juce::jlimit (-6.0f, 8.0f, p.y - py[i]);
            p.x = juce::jlimit (4.0f, (float) W - 4.0f, p.x);
        }
    }

    // temperature: hot inside, cooler at the skin; drops cool as they fall
    for (auto& p : ps)
    {
        if (p.free) p.t = juce::jmax (0.05f, p.t - 0.006f);
        else
        {
            const float d = std::sqrt ((p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy)) / restR;
            const float target = 0.30f + 0.42f * hh + 0.20f * (1.0f - juce::jmin (1.0f, d));
            p.t += (target - p.t) * 0.04f;
        }
        // fallen off the bottom: back into the blob (from inside, near its top)
        if (p.free && p.y > (float) H + 14.0f)
        {
            p.free = false;
            p.x = comX + p.qx * 0.85f;      // grows back in from the inside
            p.y = comY + p.qy * 0.85f;
            p.vx = p.vy = 0.0f;
            p.t = 0.6f + 0.3f * hh;
        }
        // a drop that didn't fall away joins the blob again
        if (p.free && std::abs (p.x - comX) < restR * 0.7f && p.y < comY + restR * 0.5f) p.free = false;
    }
    flowX += in.dt * (0.08f + 0.5f * hh);
    flowY += in.dt * (0.03f + 0.2f * hh);
}

void LavaSim::render()
{
    std::fill (field.begin(), field.end(), 0.0f);
    std::fill (temp.begin(), temp.end(), 0.0f);
    const float R2 = kernelR * kernelR;
    for (auto& p : ps)
    {
        const int x0 = juce::jmax (0, (int) (p.x - kernelR)), x1 = juce::jmin (W - 1, (int) (p.x + kernelR) + 1);
        const int y0 = juce::jmax (0, (int) (p.y - kernelR)), y1 = juce::jmin (H - 1, (int) (p.y + kernelR) + 1);
        for (int y = y0; y <= y1; ++y)
        {
            const float dy = (float) y - p.y;
            float* f = &field[(size_t) (y * W)];
            float* t = &temp[(size_t) (y * W)];
            for (int x = x0; x <= x1; ++x)
            {
                const float dx = (float) x - p.x, d2 = dx * dx + dy * dy;
                if (d2 >= R2) continue;
                const float q = 1.0f - d2 / R2, w = q * q;
                f[x] += w;
                t[x] += w * p.t;
            }
        }
    }

    const float thr = 1.35f;
    const float hh = heat;
    const float crustAmount = juce::jlimit (0.0f, 1.0f, 0.95f - 1.05f * hh);
    juce::Image::BitmapData out (lava, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < H; ++y)
    {
        for (int x = 0; x < W; ++x)
        {
            const size_t i = (size_t) (y * W + x);
            const float f = field[i];
            auto* px = (juce::PixelARGB*) out.getPixelPointer (x, y);
            if (f < thr * 0.6f) { px->setARGB (0, 0, 0, 0); continue; }
            const float a = smoothstep (thr - 0.18f, thr + 0.18f, f);
            const float tt = temp[i] / f;

            // the surface: lit like a glossy blob
            const float fl = field[(size_t) (y * W + juce::jmax (0, x - 1))], fr = field[(size_t) (y * W + juce::jmin (W - 1, x + 1))];
            const float fu = field[(size_t) (juce::jmax (0, y - 1) * W + x)], fd = field[(size_t) (juce::jmin (H - 1, y + 1) * W + x)];
            float nx = -(fr - fl) * 0.55f, ny = -(fd - fu) * 0.55f, nz = 1.0f;
            const float nl = 1.0f / std::sqrt (nx * nx + ny * ny + nz * nz);
            nx *= nl; ny *= nl; nz *= nl;
            const float lx = -0.45f, ly = -0.65f, lz = 0.62f;
            const float diff = juce::jmax (0.0f, nx * lx + ny * ly + nz * lz);
            const float rz = 2.0f * (nx * lx + ny * ly + nz * lz) * nz - lz;      // reflection towards the viewer
            const float spec = std::pow (juce::jmax (0.0f, rz), 28.0f);

            // flowing heat inside; on top a crust of dark plates (cellular) with glowing cracks between them.
            // Hotter = fewer, thinner plates and wider cracks; at full heat it's all molten
            const float flow = noise (x * 0.045f + flowX * 3.0f, y * 0.045f - flowY * 2.0f) * 0.6f
                             + noise (x * 0.11f - flowX * 5.0f, y * 0.11f + flowY * 4.0f) * 0.4f;
            const float wx = x * 0.05f + flowX * 0.8f + (noise (x * 0.05f, y * 0.05f + flowY) - 0.5f) * 0.9f;
            const float wy = y * 0.05f + flowY * 0.5f + (noise (x * 0.05f + 7.3f, y * 0.05f) - 0.5f) * 0.9f;
            const auto cl = cells (wx, wy);
            const float edge = cl.d2 - cl.d1;                                       // 0 on a crack
            const float plateOn = smoothstep (cl.id - 0.08f, cl.id + 0.08f, crustAmount * 1.15f);   // which plates are still solid
            const float crackW = 0.05f + 0.10f * hh;
            const float plate = plateOn * smoothstep (crackW, crackW + 0.11f, edge);
            const float crack = plateOn * (1.0f - smoothstep (0.0f, crackW + 0.05f, edge));
            const float skin = 1.0f - smoothstep (thr, thr * 2.2f, f);              // thin edge = cooler
            float heatHere = juce::jlimit (0.0f, 1.0f, tt * (0.85f + 0.3f * flow) - skin * 0.15f);
            auto c = lavaColour (heatHere);
            // the plates: nearly black rock, a bit warmer near their edges, with some grain
            const float grain = noise (x * 0.6f, y * 0.6f);
            const float rim = 1.0f - smoothstep (crackW, crackW + 0.25f, edge);
            // thin crust (hot) is only a dark film over the glow; thick crust (cold) is solid rock
            const float crustHere = juce::jlimit (0.0f, 1.0f, plate * (0.45f + 0.55f * juce::jmin (1.0f, crustAmount * 1.6f)) * (0.8f + 0.2f * skin));
            const RGB rock { 0.075f + 0.05f * grain + 0.28f * rim * heatHere, 0.035f + 0.025f * grain + 0.06f * rim * heatHere, 0.025f + 0.015f * grain };
            c.r += (rock.r - c.r) * crustHere; c.g += (rock.g - c.g) * crustHere; c.b += (rock.b - c.b) * crustHere;
            // the cracks glow hotter than the lava round them
            const auto hotC = lavaColour (juce::jmin (1.0f, heatHere + 0.22f));
            c.r += (hotC.r - c.r) * crack * 0.8f; c.g += (hotC.g - c.g) * crack * 0.8f; c.b += (hotC.b - c.b) * crack * 0.8f;

            const float shade = 0.55f + 0.7f * diff;
            const float emissive = 1.0f - crustHere;    // the molten part glows by itself, so light matters less there
            const float sp = spec * (0.25f + 0.45f * (1.0f - crustHere));
            const float sh = shade + (1.0f - shade) * emissive * 0.6f;
            float r = c.r * sh + sp * 1.0f, gg = c.g * sh + sp * 0.9f, b = c.b * sh + sp * 0.75f;
            r = juce::jmin (1.0f, r); gg = juce::jmin (1.0f, gg); b = juce::jmin (1.0f, b);
            const auto A = (juce::uint8) (a * 255.0f);
            px->setARGB (A, (juce::uint8) (r * a * 255.0f), (juce::uint8) (gg * a * 255.0f), (juce::uint8) (b * a * 255.0f));
        }
    }

    // the glow: a blurred, small copy
    const int gw = glowImage.getWidth(), gh = glowImage.getHeight();
    std::vector<float> gr ((size_t) (gw * gh)), gg ((size_t) (gw * gh)), gb ((size_t) (gw * gh));
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x)
        {
            float sum = 0.0f, ts = 0.0f;
            for (int yy = 0; yy < 4; ++yy)
                for (int xx = 0; xx < 4; ++xx)
                {
                    const size_t i = (size_t) (juce::jmin (H - 1, y * 4 + yy) * W + juce::jmin (W - 1, x * 4 + xx));
                    const float a = smoothstep (thr * 0.5f, thr * 1.2f, field[i]);
                    sum += a; ts += a * (field[i] > 0.0f ? temp[i] / field[i] : 0.0f);
                }
            sum /= 16.0f;
            const auto c = lavaColour (sum > 0.0f ? 0.25f + ts / 16.0f / juce::jmax (0.001f, sum) * 0.6f : 0.0f);
            gr[(size_t) (y * gw + x)] = c.r * sum; gg[(size_t) (y * gw + x)] = c.g * sum; gb[(size_t) (y * gw + x)] = c.b * sum;
        }
    auto blur = [gw, gh] (std::vector<float>& v)
    {
        std::vector<float> tmp (v.size());
        for (int pass = 0; pass < 3; ++pass)
        {
            for (int y = 0; y < gh; ++y)
                for (int x = 0; x < gw; ++x)
                {
                    float s = 0.0f; int c = 0;
                    for (int k = -2; k <= 2; ++k) { const int xx = x + k; if (xx >= 0 && xx < gw) { s += v[(size_t) (y * gw + xx)]; ++c; } }
                    tmp[(size_t) (y * gw + x)] = s / (float) c;
                }
            for (int y = 0; y < gh; ++y)
                for (int x = 0; x < gw; ++x)
                {
                    float s = 0.0f; int c = 0;
                    for (int k = -2; k <= 2; ++k) { const int yy = y + k; if (yy >= 0 && yy < gh) { s += tmp[(size_t) (yy * gw + x)]; ++c; } }
                    v[(size_t) (y * gw + x)] = s / (float) c;
                }
        }
    };
    blur (gr); blur (gg); blur (gb);
    juce::Image::BitmapData go (glowImage, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x)
        {
            const size_t i = (size_t) (y * gw + x);
            const float r = juce::jmin (1.0f, gr[i]), g = juce::jmin (1.0f, gg[i]), b = juce::jmin (1.0f, gb[i]);
            const float a = juce::jmin (1.0f, juce::jmax (r, juce::jmax (g, b)));
            ((juce::PixelARGB*) go.getPixelPointer (x, y))->setARGB ((juce::uint8) (a * 255.0f), (juce::uint8) (r * 255.0f), (juce::uint8) (g * 255.0f), (juce::uint8) (b * 255.0f));
        }
}

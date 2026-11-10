#include "PluginEditor.h"
#include "CatTheme.h"

using namespace juce;

namespace
{
constexpr int designW = 1000, designH = 620;
const Colour ink (0xfffff1e0), dim (0xffb39fb8), pink (0xffff6fae), moon (0xffffe6a3), night (0xff140c24), plum (0xff2a1638);
const Colour line (0xff1b0f22);      // the cartoon outline

float clamp01 (float x) { return jlimit (0.0f, 1.0f, x); }

void musicNote (Graphics& g, Point<float> p, float h, float spin)
{
    Path n;
    n.addEllipse (-h * 0.32f, -h * 0.2f, h * 0.42f, h * 0.3f);
    n.addRectangle (h * 0.04f, -h, h * 0.07f, h * 0.9f);
    n.startNewSubPath (h * 0.08f, -h);
    n.quadraticTo (h * 0.45f, -h * 0.75f, h * 0.35f, -h * 0.4f);
    n.quadraticTo (h * 0.3f, -h * 0.65f, h * 0.08f, -h * 0.75f);
    n.closeSubPath();
    g.fillPath (n, AffineTransform::rotation (spin).translated (p));
}

void fillOutlined (Graphics& g, const Path& p, Colour fill, float w)
{
    g.setColour (line);
    g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (fill);
    g.fillPath (p);
}

/** Who each cat in the choir is: the singer has googly eyes, the others are each silly in their own way. */
struct Persona { float eyeL, eyeR, cross, wink, tilt; bool tooth, blep, dizzy; };
const Persona personas[meow::maxCats] = {
    { 1.2f,  0.9f,  0.0f, 0.0f, 0.0f,   false, true,  false },   // the singer: googly, one eye bigger, a blep now and then
    { 1.0f,  1.0f,  0.9f, 0.0f, -0.12f, false, true,  false },   // black: cross-eyed, tongue out
    { 0.8f,  1.25f, 0.0f, 0.7f, 0.1f,   false, false, false },   // white: one eye shut, one huge
    { 1.3f,  1.3f,  0.0f, 0.0f, 0.0f,   true,  false, false },   // grey: wide crazy eyes, a buck tooth
    { 1.0f,  1.0f,  0.0f, 0.0f, 0.15f,  false, false, true  },   // calico: dizzy when it sings
    { 0.9f,  1.1f, -0.6f, 0.0f, -0.2f,  true,  true,  false },   // brown: eyes apart, tilted, tooth and tongue
};
}

//==============================================================================
MeowStage::MeowStage (MeowCatProcessor& p) : processor (p)
{
    setTooltip ("Play notes and the cat sings them. Hit a key hard: it hisses (HISS). Mod wheel: it purrs.");
    seenMeows = p.engine.meows.load();
}

void MeowStage::tick (float dt)
{
    time += dt;
    auto follow = [dt] (float& v, float target, float speed) { v += (target - v) * jmin (1.0f, dt * speed); };
    follow (mouth, processor.engine.mouth.load(), 18.0f);
    follow (hiss, processor.engine.hissing.load(), 14.0f);
    follow (purr, processor.engine.purring.load(), 6.0f);

    // blinking (not while it sings)
    nextBlink -= dt;
    if (nextBlink <= 0.0f && mouth < 0.1f) { blink = 1.0f; nextBlink = 1.5f + random.nextFloat() * 4.0f; }
    blink = jmax (0.0f, blink - dt * 7.0f);
    lookX += ((float) std::sin (time * 0.37f) * 0.7f - lookX) * jmin (1.0f, dt * 2.0f);

    // a note flies out of the mouth for every meow - and every cat's eyes wobble, its head stretches
    const int m = processor.engine.meows.load();
    const bool newMeow = m != seenMeows;
    if (newMeow)
    {
        const int count = jlimit (0, 4, m - seenMeows);
        for (int i = 0; i < count; ++i)
        {
            // out of the side of the mouth, away from the face
            const float side = (m + i) % 2 == 0 ? 1.0f : -1.0f;
            flying.push_back ({ mouthAt + Point<float> (side * mouthWidth, 0.0f),
                                { side * (70.0f + random.nextFloat() * 60.0f), -50.0f - random.nextFloat() * 50.0f }, 1.0f, side * 0.25f });
        }
        seenMeows = m;
    }
    for (auto& n : flying) { n.p += n.v * dt; n.v.x += std::sin (time * 3.0f + n.spin * 10.0f) * 30.0f * dt; n.life -= dt * 0.6f; }
    flying.erase (std::remove_if (flying.begin(), flying.end(), [] (const Note& n) { return n.life <= 0.0f; }), flying.end());
    if (flying.size() > 16) flying.erase (flying.begin(), flying.begin() + (long) (flying.size() - 16));

    // googly-eye physics: springs that overshoot
    const float step = jmin (dt, 0.05f);
    for (int k = 0; k < meow::maxCats; ++k)
    {
        auto& gf = goofy[(size_t) k];
        const auto& pr = personas[k];
        if (newMeow)
        {
            for (auto& v : gf.v) v += { (random.nextFloat() - 0.5f) * 14.0f, (random.nextFloat() - 0.6f) * 12.0f };
            gf.squashV += 2.6f + random.nextFloat() * 1.2f;
        }
        for (int e = 0; e < 2; ++e)
        {
            const float inward = (e == 0 ? 1.0f : -1.0f) * pr.cross;
            Point<float> target (jlimit (-1.0f, 1.0f, lookX * 0.6f + inward), 0.1f + std::sin (time * 0.5f + (float) k) * 0.15f);
            if (hiss > 0.2f) target += { (random.nextFloat() - 0.5f) * 1.2f * hiss, (random.nextFloat() - 0.5f) * 1.2f * hiss };   // trembling
            auto& p = gf.p[e];
            auto& v = gf.v[e];
            v += ((target - p) * 160.0f - v * (6.0f + (float) e)) * step;
            p += v * step;
            p.x = jlimit (-1.3f, 1.3f, p.x);
            p.y = jlimit (-1.3f, 1.3f, p.y);
        }
        gf.squashV += (-gf.squash * 220.0f - gf.squashV * 9.0f) * step;
        gf.squash += gf.squashV * step;
        gf.spin += step * (4.0f + 10.0f * mouth);
        // a blep: the tongue pokes out for a while when nothing happens
        gf.nextBlep -= dt;
        if (pr.blep && gf.nextBlep <= 0.0f && mouth < 0.1f) { gf.blepFor = 1.2f + random.nextFloat() * 2.0f; gf.nextBlep = 4.0f + random.nextFloat() * 6.0f; }
        gf.blepFor -= dt;
        follow (gf.tongue, gf.blepFor > 0.0f || (k == 1 && mouth < 0.1f) ? 1.0f : 0.0f, 10.0f);
    }
    repaint();
}

void MeowStage::drawCat (Graphics& g, Point<float> c, float s, const Look& look, bool body)
{
    const float anger = clamp01 (look.anger), lion = clamp01 ((look.size - 0.7f) / 0.3f), kitten = clamp01 ((0.3f - look.size) / 0.3f);
    const float mouthOpen = clamp01 (look.mouth), hissing = clamp01 (look.hiss);
    const auto fur = look.fur, dark = look.stripes;
    const float ol = jmax (1.2f, 0.035f * s);

    Graphics::ScopedSaveState keep (g);
    // purring shakes a little; singing bounces; the whole cat stretches and tilts around its feet
    const Point<float> feet (c.x, c.y + (body ? 1.5f : 1.0f) * s);
    const float bounce = std::abs (std::sin (look.time * 9.0f)) * mouthOpen * 0.07f * s;
    const float st = jlimit (-0.25f, 0.3f, look.squash);
    g.addTransform (AffineTransform::scale (1.0f - st * 0.45f, 1.0f + st, feet.x, feet.y)
                        .rotated (look.tilt, feet.x, feet.y)
                        .translated (std::sin (look.time * 150.0f) * look.purr * 1.2f, -bounce));

    if (body)
    {
        // a soft shadow on the roof
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillEllipse (c.x - 0.95f * s, c.y + 1.42f * s, 1.9f * s, 0.2f * s);

        // the tail, swishing (faster when angry), with stripes
        const float swish = std::sin (look.time * (1.3f + 4.0f * anger)) * (0.25f + 0.35f * anger);
        Path tail;
        tail.startNewSubPath (c.x + 0.55f * s, c.y + 1.3f * s);
        tail.cubicTo (c.x + 1.35f * s, c.y + 1.45f * s, c.x + (1.15f + swish) * s, c.y + 0.55f * s,
                      c.x + (0.95f + swish * 1.4f) * s, c.y + (0.35f - anger * 0.25f) * s);
        g.setColour (line);
        g.strokePath (tail, PathStrokeType (0.17f * s + ol * 2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (fur);
        g.strokePath (tail, PathStrokeType (0.17f * s, PathStrokeType::curved, PathStrokeType::rounded));
        Path dashed;
        const float dashes[] = { 0.06f * s, 0.12f * s };
        PathStrokeType (0.17f * s, PathStrokeType::curved, PathStrokeType::butt).createDashedStroke (dashed, tail, dashes, 2);
        g.setColour (dark.withAlpha (0.55f));
        g.fillPath (dashed);

        // the body, sitting
        const float bw = 1.6f * s * (1.0f + 0.1f * lion), bh = 1.1f * s;
        Path torso;
        torso.addEllipse (c.x - bw * 0.5f, c.y + 1.5f * s - bh, bw, bh);
        fillOutlined (g, torso, fur.darker (0.12f), ol * 2.0f);
        g.setColour (fur.brighter (0.3f).withAlpha (0.85f));
        g.fillEllipse (c.x - bw * 0.22f, c.y + 1.5f * s - bh * 0.82f, bw * 0.44f, bh * 0.7f);      // the chest
        for (float side : { -1.0f, 1.0f })                                                        // the paws, with toes
        {
            Path paw;
            const Rectangle<float> pr (c.x + side * 0.28f * s - 0.18f * s, c.y + 1.36f * s, 0.36f * s, 0.2f * s);
            paw.addEllipse (pr);
            fillOutlined (g, paw, fur.brighter (0.15f), ol * 1.6f);
            g.setColour (line.withAlpha (0.7f));
            for (float t : { -0.3f, 0.3f })
                g.drawLine (pr.getCentreX() + t * pr.getWidth() * 0.5f, pr.getY() + pr.getHeight() * 0.45f,
                            pr.getCentreX() + t * pr.getWidth() * 0.5f, pr.getBottom() - 1.0f, jmax (1.0f, ol * 0.6f));
        }
    }

    // a lion's mane
    if (lion > 0.0f)
    {
        Path mane;
        constexpr int spikes = 24;
        for (int i = 0; i <= spikes * 2; ++i)
        {
            const float a = (float) i / (float) (spikes * 2) * MathConstants<float>::twoPi;
            const float r = (i % 2 == 0 ? 1.42f + 0.06f * std::sin (look.time * 2.0f + (float) i) : 1.0f) * s;
            const Point<float> p (c.x + r * std::sin (a), c.y + 0.05f * s - r * 0.92f * std::cos (a));
            if (i == 0) mane.startNewSubPath (p); else mane.lineTo (p);
        }
        mane.closeSubPath();
        mane = mane.createPathWithRoundedCorners (0.06f * s);
        g.setColour (line.withAlpha (lion));
        g.strokePath (mane, PathStrokeType (ol * 2.0f));
        ColourGradient mg (Colour (0xffd2702a).withAlpha (lion), c.x, c.y - s, Colour (0xff7a3412).withAlpha (lion), c.x, c.y + s, false);
        g.setGradientFill (mg);
        g.fillPath (mane);
    }

    // hissing: the fur stands up
    if (hissing > 0.05f)
    {
        Path spikes;
        constexpr int k = 30;
        for (int i = 0; i <= k * 2; ++i)
        {
            const float a = (float) i / (float) (k * 2) * MathConstants<float>::twoPi;
            const float r = (i % 2 == 0 ? 1.0f + 0.24f * hissing : 0.92f) * s;
            const Point<float> p (c.x + r * std::sin (a), c.y - r * 0.88f * std::cos (a));
            if (i == 0) spikes.startNewSubPath (p); else spikes.lineTo (p);
        }
        spikes.closeSubPath();
        g.setColour (dark.withAlpha (0.95f));
        g.fillPath (spikes);
    }

    // ears: back and down when angry, big on a kitten, flopping a bit when it bounces
    const float earScale = 1.0f + 0.2f * kitten - 0.25f * lion;
    for (float side : { -1.0f, 1.0f })
    {
        const float flop = std::sin (look.time * 9.0f + side) * mouthOpen * 0.06f;
        const Point<float> tip (c.x + side * (0.62f + 0.38f * anger + flop) * s * earScale, c.y + (-0.98f + 0.5f * anger) * s * earScale);
        const Point<float> b1 (c.x + side * 0.88f * s, c.y - 0.3f * s), b2 (c.x + side * 0.18f * s, c.y - 0.74f * s);
        Path ear;
        ear.addTriangle (tip, b1, b2);
        ear = ear.createPathWithRoundedCorners (0.08f * s);
        fillOutlined (g, ear, fur.darker (0.08f), ol * 2.0f);
        Path inner;
        inner.addTriangle (tip + (b1 + b2 - tip * 2.0f) * 0.18f, b1 + (tip - b1) * 0.25f + (b2 - b1) * 0.15f, b2 + (tip - b2) * 0.25f + (b1 - b2) * 0.15f);
        inner = inner.createPathWithRoundedCorners (0.05f * s);
        g.setColour (Colour (0xffffa8c0).withAlpha (0.9f));
        g.fillPath (inner);
    }

    // the head: round, with fluffy cheeks
    const float hw = s * (1.0f + 0.08f * lion), hh = s * (0.86f + 0.08f * kitten);
    Path head;
    constexpr int around = 96;
    for (int i = 0; i < around; ++i)
    {
        const float a = (float) i / around * MathConstants<float>::twoPi;
        float r = 1.0f;
        for (float centre : { 2.05f, MathConstants<float>::twoPi - 2.05f })      // the cheeks, lower left and right
        {
            const float d = std::abs (a - centre);
            if (d < 0.45f)
            {
                const float tooth = 1.0f - std::abs (std::fmod (d * 4.0f / 0.45f, 2.0f) - 1.0f);   // a zigzag
                r += 0.09f * tooth * (1.0f - d / 0.45f);
            }
        }
        const Point<float> p (c.x + std::sin (a) * hw * r, c.y - std::cos (a) * hh * r);
        if (i == 0) head.startNewSubPath (p); else head.lineTo (p);
    }
    head.closeSubPath();
    g.setColour (line);
    g.strokePath (head, PathStrokeType (ol * 2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    ColourGradient hg (fur.brighter (0.25f), c.x - hw * 0.3f, c.y - hh, fur.darker (0.15f), c.x + hw * 0.2f, c.y + hh, false);
    g.setGradientFill (hg);
    g.fillPath (head);
    {
        // moonlight on the upper right edge
        Path rim;
        rim.addCentredArc (c.x, c.y, hw * 0.9f, hh * 0.9f, 0.0f, 0.15f, 1.35f, true);
        g.setColour (moon.withAlpha (0.4f));
        g.strokePath (rim, PathStrokeType (0.05f * s, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (dark.withAlpha (0.8f));                                                      // tabby stripes
    for (int i = -1; i <= 1; ++i)
    {
        Path stp;
        stp.startNewSubPath (c.x + (float) i * 0.17f * s, c.y - 0.84f * hh);
        stp.quadraticTo (c.x + (float) i * 0.22f * s, c.y - 0.62f * hh, c.x + (float) i * 0.12f * s, c.y - 0.45f * hh);
        g.strokePath (stp, PathStrokeType (0.06f * s, PathStrokeType::curved, PathStrokeType::rounded));
    }
    for (float side : { -1.0f, 1.0f })                                                        // cheek stripes
        for (int i = 0; i < 2; ++i)
            g.drawLine (c.x + side * 0.82f * s, c.y + (0.0f + 0.12f * (float) i) * s, c.x + side * 0.62f * s, c.y + (0.03f + 0.12f * (float) i) * s, 0.045f * s);
    g.setColour (fur.brighter (0.5f).withAlpha (0.95f));                                       // the muzzle
    g.fillEllipse (c.x - 0.42f * s, c.y + 0.02f * s, 0.84f * s, 0.5f * s);
    g.setColour (Colour (0xffff8fb0).withAlpha (0.35f + 0.25f * mouthOpen));                    // blush
    for (float side : { -1.0f, 1.0f })
        g.fillEllipse (c.x + side * 0.58f * s - 0.14f * s, c.y + 0.08f * s, 0.28f * s, 0.15f * s);

    // eyes: googly pupils, one eye bigger, winks, slits when angry, spirals when dizzy
    const float open = clamp01 ((1.0f - look.blink) * (1.0f - 0.35f * anger) * (1.0f - 0.6f * look.purr) * (1.0f - 0.3f * hissing) + 0.04f);
    for (int e = 0; e < 2; ++e)
    {
        const float side = e == 0 ? -1.0f : 1.0f;
        const float er = s * (0.19f + 0.06f * kitten) * look.eyeSize[e];
        const Point<float> ec (c.x + side * 0.36f * s, c.y - 0.12f * s - (er - s * 0.19f) * 0.3f);
        const float shut = e == 0 ? look.wink : 0.0f;
        const float ery = er * 1.1f * open * (1.0f - shut);
        if (ery < er * 0.15f)
        {
            // closed: a happy little arc
            Path arc;
            arc.addCentredArc (ec.x, ec.y + er * 0.2f, er * 0.85f, er * 0.55f, 0.0f, -1.3f, 1.3f, true);
            g.setColour (line);
            g.strokePath (arc, PathStrokeType (jmax (1.2f, 0.05f * s), PathStrokeType::curved, PathStrokeType::rounded));
            continue;
        }
        Path eye;
        eye.addEllipse (ec.x - er, ec.y - ery, er * 2.0f, ery * 2.0f);
        g.setColour (line);
        g.strokePath (eye, PathStrokeType (ol * 1.6f));
        ColourGradient iris (look.eyes.brighter (0.5f), ec.x, ec.y + ery * 0.4f, look.eyes.darker (0.35f), ec.x, ec.y - ery, true);
        g.setGradientFill (iris);
        g.fillPath (eye);

        Graphics::ScopedSaveState inEye (g);
        g.reduceClipRegion (eye);
        const auto pp = look.pupil[e];
        const Point<float> pc (ec.x + pp.x * er * 0.42f, ec.y + pp.y * ery * 0.35f);
        if (look.dizzy > 0.3f)
        {
            Path spiral;
            const float spin = look.time * 6.0f * (side);
            for (int i = 0; i <= 40; ++i)
            {
                const float t = (float) i / 40.0f, a = spin + t * 14.0f, r = t * er * 0.85f;
                const Point<float> q (ec.x + r * std::cos (a), ec.y + r * std::sin (a) * (ery / er));
                if (i == 0) spiral.startNewSubPath (q); else spiral.lineTo (q);
            }
            g.setColour (line);
            g.strokePath (spiral, PathStrokeType (jmax (1.2f, 0.04f * s), PathStrokeType::curved, PathStrokeType::rounded));
            continue;
        }
        const float pw = er * jmax (0.16f, 0.95f - 0.8f * anger - 0.35f * hissing + 0.1f * kitten);
        g.setColour (Colour (0xff120c18));
        g.fillEllipse (pc.x - pw * 0.5f, pc.y - ery * 0.85f, pw, ery * 1.7f);
        g.setColour (Colours::white.withAlpha (0.95f));
        g.fillEllipse (pc.x - pw * 0.5f - er * 0.05f, pc.y - ery * 0.55f, er * 0.36f, er * 0.36f);   // big shine
        g.setColour (Colours::white.withAlpha (0.7f));
        g.fillEllipse (pc.x + pw * 0.15f, pc.y + ery * 0.25f, er * 0.14f, er * 0.14f);              // small shine
    }
    if (anger > 0.3f)        // angry brows
        for (float side : { -1.0f, 1.0f })
        {
            const float er = s * 0.19f;
            const Point<float> ec (c.x + side * 0.36f * s, c.y - 0.12f * s);
            const float lift = (anger - 0.3f) / 0.7f;
            g.setColour (line);
            g.drawLine (ec.x - side * er * 1.1f, ec.y - er * 1.55f, ec.x + side * er * 0.9f, ec.y - er * (1.55f - 0.7f * lift), 0.07f * s);
        }

    // nose (with a shine)
    Path nose;
    nose.addTriangle (c.x - 0.08f * s, c.y + 0.08f * s, c.x + 0.08f * s, c.y + 0.08f * s, c.x, c.y + 0.18f * s);
    nose = nose.createPathWithRoundedCorners (0.03f * s);
    fillOutlined (g, nose, Colour (0xffff7fa6), ol);
    g.setColour (Colours::white.withAlpha (0.6f));
    g.fillEllipse (c.x - 0.04f * s, c.y + 0.09f * s, 0.035f * s, 0.025f * s);

    // mouth
    const float m = jmax (mouthOpen, hissing);
    if (m < 0.06f)
    {
        if (look.tongue > 0.05f)      // blep
        {
            Path t;
            const float tl = 0.16f * s * look.tongue;
            t.addRoundedRectangle (c.x + 0.01f * s, c.y + 0.22f * s, 0.11f * s, tl, 0.05f * s);
            fillOutlined (g, t, Colour (0xffff7f96), ol * 1.2f);
            g.setColour (Colour (0xffd9506c));
            g.drawLine (c.x + 0.065f * s, c.y + 0.24f * s, c.x + 0.065f * s, c.y + 0.22f * s + tl * 0.7f, jmax (1.0f, 0.015f * s));
        }
        Path w;
        w.startNewSubPath (c.x - 0.16f * s, c.y + 0.24f * s);
        w.quadraticTo (c.x - 0.08f * s, c.y + 0.31f * s, c.x, c.y + 0.18f * s);
        w.quadraticTo (c.x + 0.08f * s, c.y + 0.31f * s, c.x + 0.16f * s, c.y + 0.24f * s);
        g.setColour (line);
        g.strokePath (w, PathStrokeType (0.04f * s, PathStrokeType::curved, PathStrokeType::rounded));
        if (look.tooth)
        {
            Path tooth;
            tooth.addRoundedRectangle (c.x - 0.035f * s, c.y + 0.21f * s, 0.07f * s, 0.08f * s, 0.015f * s);
            fillOutlined (g, tooth, Colours::white, ol);
        }
    }
    else
    {
        const float mw = s * (0.2f + 0.14f * m + 0.12f * hissing), mh = s * (0.04f + 0.34f * m);
        const Rectangle<float> mr (c.x - mw * 0.5f, c.y + 0.2f * s, mw, mh);
        Path mp;
        mp.addEllipse (mr);
        fillOutlined (g, mp, Colour (0xff4a0f1c), ol * 1.6f);
        g.setColour (Colour (0xffff7f96));
        g.fillEllipse (mr.withTrimmedTop (mh * 0.55f).reduced (mw * 0.15f, 0.0f));
        g.setColour (Colours::white);
        if (hissing > 0.2f || lion > 0.5f)      // fangs
            for (float side : { -1.0f, 1.0f })
            {
                Path fang;
                fang.addTriangle (c.x + side * mw * 0.32f, mr.getY() + mh * 0.05f, c.x + side * mw * 0.14f, mr.getY() + mh * 0.05f,
                                  c.x + side * mw * 0.24f, mr.getY() + mh * 0.45f);
                g.fillPath (fang);
            }
        else if (look.tooth)
            g.fillRoundedRectangle (c.x - 0.035f * s, mr.getY() + mh * 0.02f, 0.07f * s, jmin (0.08f * s, mh * 0.45f), 0.015f * s);
    }

    // whiskers, a little curved (they shiver when it purrs)
    g.setColour (Colours::white.withAlpha (0.8f));
    for (float side : { -1.0f, 1.0f })
        for (int k = 0; k < 3; ++k)
        {
            const float shake = std::sin (look.time * 40.0f + (float) k) * look.purr * 0.025f * s;
            Path wh;
            const Point<float> a (c.x + side * 0.3f * s, c.y + (0.17f + (float) k * 0.055f) * s);
            const Point<float> b (c.x + side * 1.25f * s, c.y + (0.0f + (float) k * 0.14f) * s + shake - anger * 0.08f * s);
            wh.startNewSubPath (a);
            wh.quadraticTo ((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f - 0.05f * s, b.x, b.y);
            g.strokePath (wh, PathStrokeType (jmax (1.0f, 0.018f * s), PathStrokeType::curved, PathStrokeType::rounded));
        }
}

void MeowStage::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    Path clip;
    clip.addRoundedRectangle (b, 18.0f);
    g.reduceClipRegion (clip);

    // the night sky over the rooftops
    ColourGradient sky (Colour (0xff0b0a26), 0.0f, 0.0f, Colour (0xff3a1a48), 0.0f, b.getHeight(), false);
    g.setGradientFill (sky);
    g.fillRect (b);
    Random stars (3);
    for (int i = 0; i < 70; ++i)
    {
        const float x = stars.nextFloat() * b.getWidth(), y = stars.nextFloat() * b.getHeight() * 0.7f;
        const float tw = 0.4f + 0.6f * (0.5f + 0.5f * std::sin (time * (0.7f + stars.nextFloat() * 2.0f) + (float) i));
        g.setColour (Colours::white.withAlpha (0.25f + 0.5f * tw));
        const float r = 0.6f + stars.nextFloat() * 1.2f;
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
    }
    // the moon with a glow, and clouds drifting past it
    const Point<float> mc (b.getWidth() - 82.0f, 70.0f);
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (moon.withAlpha (0.05f * (float) (5 - i)));
        const float r = 36.0f + 14.0f * (float) i;
        g.fillEllipse (mc.x - r, mc.y - r, r * 2.0f, r * 2.0f);
    }
    g.setColour (moon);
    g.fillEllipse (mc.x - 36.0f, mc.y - 36.0f, 72.0f, 72.0f);
    g.setColour (moon.darker (0.15f));
    g.fillEllipse (mc.x - 14.0f, mc.y - 16.0f, 12.0f, 12.0f);
    g.fillEllipse (mc.x + 6.0f, mc.y + 6.0f, 9.0f, 9.0f);
    g.fillEllipse (mc.x - 20.0f, mc.y + 10.0f, 6.0f, 6.0f);
    for (int k = 0; k < 2; ++k)
    {
        const float span = b.getWidth() + 260.0f;
        const float x = std::fmod (time * (9.0f + 5.0f * (float) k) + (float) k * 300.0f, span) - 130.0f;
        const float y = 52.0f + 60.0f * (float) k;
        g.setColour (Colour (0xff6a5a8a).withAlpha (0.28f));
        for (auto [dx, dy, r] : { std::tuple { 0.0f, 0.0f, 26.0f }, { 30.0f, -8.0f, 30.0f }, { 62.0f, 2.0f, 22.0f }, { 30.0f, 10.0f, 24.0f } })
            g.fillEllipse (x + dx - r, y + dy - r * 0.6f, r * 2.0f, r * 1.2f);
    }

    // far rooftops, lit windows, a neon sign
    g.setColour (Colour (0xff1a1030));
    Path far;
    far.startNewSubPath (0.0f, b.getHeight());
    const float skyline[] = { 0.62f, 0.58f, 0.66f, 0.55f, 0.6f, 0.52f, 0.63f, 0.57f, 0.65f };
    for (int i = 0; i < 9; ++i)
    {
        const float x0 = b.getWidth() * (float) i / 9.0f, x1 = b.getWidth() * (float) (i + 1) / 9.0f;
        far.lineTo (x0, b.getHeight() * skyline[i]);
        far.lineTo (x1, b.getHeight() * skyline[i]);
    }
    far.lineTo (b.getWidth(), b.getHeight());
    far.closeSubPath();
    g.fillPath (far);
    Random win (9);
    for (int i = 0; i < 16; ++i)
    {
        const int col = win.nextInt (9);
        const float x = b.getWidth() * ((float) col + 0.2f + win.nextFloat() * 0.5f) / 9.0f;
        const float y = b.getHeight() * (skyline[col] + 0.03f + win.nextFloat() * 0.12f);
        const bool tv = i % 5 == 0;          // someone is watching TV
        g.setColour (tv ? Colour (0xff7fb8ff).withAlpha (0.45f + 0.2f * std::sin (time * 7.0f + (float) i)) : Colour (0xffffd27a).withAlpha (0.55f));
        g.fillRect (x, y, 7.0f, 9.0f);
    }
    {
        const bool flicker = std::fmod (time, 5.3f) > 5.05f && std::fmod (time * 13.0f, 1.0f) > 0.5f;
        const float glow = flicker ? 0.25f : 1.0f;
        const Rectangle<float> sign (24.0f, b.getHeight() * 0.58f - 20.0f, 92.0f, 26.0f);
        g.setFont (cat::displayFont (18.0f));
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (pink.withAlpha (0.12f * glow));
            g.drawText ("MEOW", sign.expanded ((float) i * 1.5f).translated ((float) -i * 0.5f, 0.0f), Justification::centred);
            g.drawText ("MEOW", sign.expanded ((float) i * 1.5f).translated ((float) i * 0.5f, 0.0f), Justification::centred);
        }
        g.setColour (pink.brighter (0.4f).withAlpha (glow));
        g.drawText ("MEOW", sign, Justification::centred);
        g.setColour (pink.withAlpha (0.5f * glow));
        g.drawRoundedRectangle (sign.reduced (0.0f, 2.0f), 6.0f, 1.5f);
    }

    // our roof: tiles, a chimney, an antenna
    const float roof = b.getHeight() - 70.0f;
    g.setColour (Colour (0xff160d24));
    g.fillRect (0.0f, roof, b.getWidth(), b.getHeight() - roof);
    g.setColour (Colour (0xff24163a));
    for (int row = 0; row < 4; ++row)
        for (float x = (row % 2) * 14.0f - 14.0f; x < b.getWidth(); x += 28.0f)
        {
            Path tile;
            tile.addCentredArc (x + 14.0f, roof + 10.0f + (float) row * 16.0f, 13.0f, 9.0f, 0.0f, MathConstants<float>::halfPi, MathConstants<float>::halfPi * 3.0f, true);
            g.strokePath (tile, PathStrokeType (1.5f));
        }
    g.setColour (Colour (0xff3a2654));
    g.fillRect (0.0f, roof, b.getWidth(), 4.0f);
    g.setColour (Colour (0xff2c1a3c));
    g.fillRect (38.0f, roof - 46.0f, 34.0f, 46.0f);
    g.fillRect (32.0f, roof - 52.0f, 46.0f, 8.0f);
    g.setColour (Colour (0xff3a2654));
    g.drawLine (b.getWidth() - 50.0f, roof, b.getWidth() - 50.0f, roof - 70.0f, 2.0f);
    g.drawLine (b.getWidth() - 66.0f, roof - 60.0f, b.getWidth() - 34.0f, roof - 60.0f, 2.0f);
    g.drawLine (b.getWidth() - 60.0f, roof - 48.0f, b.getWidth() - 40.0f, roof - 48.0f, 2.0f);
    g.setColour (Colour (0xffff4060).withAlpha (0.5f + 0.5f * std::sin (time * 2.0f)));
    g.fillEllipse (b.getWidth() - 53.0f, roof - 75.0f, 6.0f, 6.0f);

    auto& apvts = processor.apvts;
    auto val = [&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float size = val ("size"), mood = val ("mood"), chaos = val ("chaos");
    const int choir = jlimit (1, meow::maxCats, roundToInt (val ("choir")));

    Look look;
    look.mouth = mouth; look.hiss = hiss; look.purr = purr; look.size = size; look.blink = blink; look.lookX = lookX; look.time = time;
    look.anger = jlimit (0.0f, 1.0f, (mood - 0.45f) / 0.55f + hiss * 0.6f);
    auto silly = [&] (Look& o, int k)
    {
        const auto& pr = personas[k];
        const auto& gf = goofy[(size_t) k];
        o.pupil[0] = gf.p[0]; o.pupil[1] = gf.p[1];
        o.eyeSize[0] = pr.eyeL; o.eyeSize[1] = pr.eyeR;
        o.wink = pr.wink * (1.0f - o.mouth);
        o.tooth = pr.tooth;
        o.tongue = gf.tongue;
        o.squash = gf.squash * 0.06f;
        o.tilt = pr.tilt + std::sin (time * 2.3f + (float) k) * (0.04f + 0.12f * o.mouth) + std::sin (time * 1.1f) * 0.08f * chaos;
        o.dizzy = (pr.dizzy && o.mouth > 0.2f) || (chaos > 0.75f && k % 2 == 1 && o.mouth > 0.2f) ? 1.0f : 0.0f;
    };

    // the choir: the others sit beside it
    const Colour furs[] = { Colour (0xff2a2a31), Colour (0xffeeeae2), Colour (0xff8d8f99), Colour (0xffd9c3a5), Colour (0xff6b4a35) };
    const Colour stripeOf[] = { Colour (0xff17171c), Colour (0xffc9c3b6), Colour (0xff5d5f69), Colour (0xff3a2a20), Colour (0xff3e2a1e) };
    for (int k = choir - 2; k >= 0; --k)
    {
        const float side = (k % 2 == 0) ? -1.0f : 1.0f;
        const float gs = 40.0f - 3.0f * (float) (k / 2);
        const Point<float> gc (b.getCentreX() + side * (138.0f + 36.0f * (float) (k / 2)), roof - 1.5f * gs);
        Look o = look;
        o.fur = furs[k]; o.stripes = stripeOf[k];
        o.eyes = k == 0 ? Colour (0xffffd84a) : k == 1 ? Colour (0xff7fd3ff) : look.eyes;
        o.size = jlimit (0.0f, 1.0f, size + ((float) (k % 3) - 1.0f) * 0.15f);
        o.mouth = mouth * (0.75f + 0.25f * std::sin (time * 9.0f + (float) k * 1.7f));
        o.blink = 0.0f;
        o.time = time + (float) k * 0.9f;
        silly (o, k + 1);
        drawCat (g, gc, gs, o);
    }

    // the singer
    const float s = 92.0f * (0.85f + 0.25f * size);
    const Point<float> c (b.getCentreX(), roof - 1.5f * s);
    mouthAt = { c.x, c.y + 0.3f * s };
    mouthWidth = 0.75f * s;
    silly (look, 0);
    drawCat (g, c, s, look);

    // the notes it sings
    for (auto& n : flying)
    {
        const auto col = pink.interpolatedWith (moon, 1.0f - n.life);
        g.setColour (col.withAlpha (clamp01 (n.life * 1.6f) * 0.25f));
        g.fillEllipse (n.p.x - 18.0f, n.p.y - 26.0f, 36.0f, 36.0f);
        g.setColour (col.withAlpha (clamp01 (n.life * 1.6f)));
        musicNote (g, n.p, 22.0f, n.spin);
    }

    g.setFont (cat::displayFont (16.0f));
    if (purr > 0.05f)
    {
        g.setColour (ink.withAlpha (clamp01 (purr * 1.5f)));
        const float wob = std::sin (time * 6.0f) * 4.0f;
        g.drawText ("prrr", Rectangle<float> (c.x + s * 1.05f, c.y - s * 0.55f + wob, 90.0f, 24.0f), Justification::centredLeft);
        g.drawText ("prrr", Rectangle<float> (c.x - s * 1.05f - 90.0f, c.y - s * 0.3f - wob, 90.0f, 24.0f), Justification::centredRight);
    }
    if (hiss > 0.1f)
    {
        g.setColour (pink.withAlpha (clamp01 (hiss * 1.5f)));
        g.setFont (cat::displayFont (26.0f));
        g.drawText ("HSSS!", Rectangle<float> (c.x + s * 0.8f, c.y - s * 1.15f, 140.0f, 34.0f), Justification::centredLeft);
    }

    // who is singing
    g.setFont (cat::catFont (12.0f));
    g.setColour (ink.withAlpha (0.85f));
    const String who = MeowCatProcessor::sizeName (size) + "  .  " + MeowCatProcessor::moodName (mood) + "  .  "
                     + (choir == 1 ? String ("solo") : "choir of " + String (choir));
    g.drawText (who, b.withTrimmedTop (b.getHeight() - 34.0f).reduced (16.0f, 6.0f), Justification::centred);

    g.setColour (Colour (0xff3a2850));
    g.drawRoundedRectangle (b.reduced (0.5f), 18.0f, 1.0f);
}

//==============================================================================
MeowCatEditor::MeowCatEditor (MeowCatProcessor& p) : processor (p)
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (stage);
    for (auto* s : { &size, &mood, &length, &octave, &choir, &chaos, &purr, &hiss, &alley })
    {
        s->setColours (Colour (0xff8a5cff), pink);
        addAndMakeVisible (*s);
    }
    size.getSlider().setTooltip ("SIZE: a tiny kitten (left), a house cat, a big cat, a lion (right) - with a mane and a growl");
    mood.getSlider().setTooltip ("MOOD: a sweet 'mew?' (left), a hungry 'meeoow', a furious 'mrrraaooww' (right)");
    length.getSlider().setTooltip ("LENGTH: a short mew (left) or a very long meeeeooow (right)");
    octave.getSlider().setTooltip ("OCTAVE: the whole cat higher or lower");
    choir.getSlider().setTooltip ("CHOIR: how many cats sing every note (each one a bit different)");
    chaos.getSlider().setTooltip ("CHAOS: every meow a bit different, and now and then the voice cracks");
    purr.getSlider().setTooltip ("PURR: a purr under every held note (the mod wheel purrs too)");
    hiss.getSlider().setTooltip ("HISS: hit a key this hard and the cat hisses instead (left = never)");
    alley.getSlider().setTooltip ("ALLEY: the echo of a back alley at night");
    addAndMakeVisible (mode);
    mode.getCombo().setTooltip ("MEOW: one meow per note. SING: holds the 'aaa' while you hold the key. CHATTER: 'ek-ek-ek' like a cat watching birds. PURR: only purring");

    keys.setAvailableRange (36, 96);
    keys.setLowestVisibleKey (48);
    keys.setKeyWidth (22.0f);
    keys.setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xfff6ece4));
    keys.setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff1c1226));
    keys.setColour (MidiKeyboardComponent::keyDownOverlayColourId, pink.withAlpha (0.75f));
    keys.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, pink.withAlpha (0.25f));
    keys.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff3a2850));
    keys.setColour (MidiKeyboardComponent::shadowColourId, Colours::transparentBlack);
    keys.setVelocity (0.75f, true);
    keys.setOctaveForMiddleC (4);
    keys.setWantsKeyboardFocus (false);
    addAndMakeVisible (keys);

    setSize (designW, designH);
    startTimerHz (40);
}

MeowCatEditor::~MeowCatEditor() { stopTimer(); }

void MeowCatEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    ColourGradient bg (plum, b.getWidth() * 0.5f, b.getHeight() * 0.3f, night, b.getWidth() * 0.5f + b.getWidth() * 0.6f, b.getHeight() * 0.3f, true);
    g.setGradientFill (bg);
    g.fillAll();

    g.setColour (ink);
    g.setFont (cat::displayFont (30.0f));
    g.drawText ("MEOW-CAT", 40, 22, 320, 42, Justification::centredLeft);
    g.setColour (dim);
    g.setFont (cat::catFont (11.5f));
    g.drawText ("every note is a cat", 42, 62, 300, 16, Justification::centredLeft);

    // a small cat face next to the title
    MeowStage::Look tiny;
    tiny.mouth = processor.engine.mouth.load();
    tiny.time = (float) Time::getMillisecondCounter() * 0.001f;
    MeowStage::drawCat (g, { 248.0f, 44.0f }, 15.0f, tiny, false);

    g.setColour (dim.withAlpha (0.8f));
    g.setFont (cat::catFont (11.0f));
    g.drawText ("click the keys or play your MIDI keyboard  .  hit hard: it hisses  .  mod wheel: it purrs",
                40, designH - 30, designW - 80, 18, Justification::centred);
}

void MeowCatEditor::resized()
{
    presetBar.setBounds (getWidth() - 360, 28, 320, 34);
    stage.setBounds (250, 84, 500, 400);
    int y = 104;
    for (auto* s : { &size, &mood, &length, &octave }) { s->setBounds (40, y, 186, 40); y += 62; }
    mode.setBounds (40, y + 8, 186, 26);
    y = 104;
    for (auto* s : { &choir, &chaos, &purr, &hiss, &alley }) { s->setBounds (774, y, 186, 40); y += 62; }
    keys.setBounds (40, 500, 920, 84);
    keys.setKeyWidth (920.0f / 22.0f);   // three octaves fit: C3 .. C6
    keys.setLowestVisibleKey (48);
}

void MeowCatEditor::timerCallback()
{
    const auto now = Time::getMillisecondCounter();
    const float dt = lastTick == 0 ? 1.0f / 40.0f : jlimit (0.005f, 0.1f, (float) (now - lastTick) / 1000.0f);
    lastTick = now;
    stage.tick (dt);
    repaint (230, 26, 40, 40);
}

#include "CatVisualizer.h"

namespace cat
{
namespace
{
    // The cat is designed in a 100 x 120 box and scaled to the component.
    constexpr float designW = 100.0f, designH = 120.0f;

    float decay (float value, float rate, float dt) { return value * std::exp (-rate * dt); }

    juce::Path ear (juce::Point<float> base1, juce::Point<float> tip, juce::Point<float> base2, float angle, juce::Point<float> pivot)
    {
        juce::Path p;
        p.addTriangle (base1, tip, base2);
        p.applyTransform (juce::AffineTransform::rotation (angle, pivot.x, pivot.y));
        return p;
    }
}

CatVisualizer::CatVisualizer (NoteMonitor& m, std::function<double()> getPpq, std::function<bool()> isRunning, CatStyle s)
    : monitor (m), style (s), ppq (std::move (getPpq)), running (std::move (isRunning))
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
    rings.reserve (16);
    lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (60);
}

void CatVisualizer::noteOn (int note, int velocity)
{
    const float v = (float) velocity / 127.0f;
    energy = juce::jmin (1.0f, energy + v * 0.45f);
    earKick = juce::jmax (earKick, 0.4f + 0.6f * v);
    tailKick = juce::jmax (tailKick, 0.3f + 0.7f * v);
    bounce = juce::jmax (bounce, v);
    juce::ignoreUnused (note);

    // One ring per chord / burst, not one per note
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (now - lastRingTime > 0.06 && rings.size() < 12)
    {
        rings.push_back ({ 0.0f, 0.35f + 0.65f * v });
        lastRingTime = now;
    }
}

void CatVisualizer::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;
    time += dt;

    // Notes: detect new note-ons, average pitch of what is sounding
    int active = 0, pitchSum = 0;
    for (int n = 0; n < 128; ++n)
    {
        const uint8_t v = monitor.velocity[(size_t) n].load (std::memory_order_relaxed);
        if (v > 0 && previous[(size_t) n] == 0) noteOn (n, v);
        if (v > 0) { ++active; pitchSum += n; }
        previous[(size_t) n] = v;
    }

    energy = decay (energy, active > 0 ? 1.2f : 2.5f, dt);
    earKick = decay (earKick, 7.0f, dt);
    tailKick = decay (tailKick, 3.5f, dt);
    bounce = decay (bounce, 9.0f, dt);

    // Head tilts towards the register being played (higher notes -> head up)
    const float targetTilt = active > 0 ? juce::jlimit (-0.18f, 0.18f, ((float) pitchSum / (float) active - 60.0f) / 24.0f * -0.14f) : 0.0f;
    tilt += (targetTilt - tilt) * juce::jmin (1.0f, dt * 6.0f);

    // Beat: a quick nod on every quarter note
    if (running && running() && ppq)
    {
        const double phase = ppq() - std::floor (ppq());
        beatBob = (float) std::pow (1.0 - phase, 6.0);
    }
    else
    {
        beatBob = decay (beatBob, 6.0f, dt);
    }

    tailPhase += dt * (1.3f + energy * 5.0f);

    // Blink every few seconds (eyes stay open while it is busy listening)
    nextBlink -= dt;
    if (nextBlink <= 0.0f)
    {
        blink = 1.0f;
        nextBlink = 2.5f + rng.nextFloat() * 4.0f;
    }
    blink = juce::jmax (0.0f, blink - dt * 7.0f);

    for (auto& r : rings) r.age += dt;
    rings.erase (std::remove_if (rings.begin(), rings.end(), [] (const Ring& r) { return r.age > 1.1f; }), rings.end());

    if (isShowing())
        repaint();
}

void CatVisualizer::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float scale = juce::jmin (bounds.getWidth() / designW, bounds.getHeight() / designH);
    const auto toScreen = juce::AffineTransform::scale (scale)
                              .translated (bounds.getCentreX() - designW * scale * 0.5f, bounds.getBottom() - designH * scale);
    const auto accent = Colours::accent;

    // Rings behind the cat
    if (showRings)
    {
        for (const auto& r : rings)
        {
            const float radius = 28.0f + r.age * 60.0f;
            const float alpha = r.strength * juce::jmax (0.0f, 1.0f - r.age / 1.1f) * 0.55f;
            juce::Path ring;
            ring.addEllipse (50.0f - radius, 52.0f - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (accent.withAlpha (alpha));
            g.strokePath (ring, juce::PathStrokeType (1.4f / juce::jmax (0.2f, scale) * juce::jmin (1.0f, scale)), toScreen);
        }
    }

    const float breathe = 1.0f + 0.018f * std::sin (time * 1.7f) + 0.03f * energy;
    const float headDrop = beatBob * 4.0f + bounce * 3.0f;        // nod down on the beat / on notes
    const juce::Point<float> headCentre (50.0f, 40.0f + headDrop);

    juce::Path body;      // collects every part; each is filled on its own (no winding cancellation)
    std::vector<juce::Path> parts;
    // Body and chest (breathing scales them around the seat)
    {
        juce::Path torso;
        torso.addEllipse (50.0f - 28.0f, 60.0f, 56.0f, 58.0f);       // haunches
        torso.addEllipse (50.0f - 19.0f, 47.0f, 38.0f, 42.0f);       // chest
        torso.addEllipse (33.0f, 110.0f, 15.0f, 8.0f);              // front paws
        torso.addEllipse (52.0f, 110.0f, 15.0f, 8.0f);
        torso.applyTransform (juce::AffineTransform::scale (breathe, breathe, 50.0f, 118.0f));
        parts.push_back (torso);
    }

    // Tail: sways slowly, flicks on notes
    {
        const float sway = std::sin (tailPhase) * (0.18f + 0.35f * energy) + tailKick * 0.55f;
        juce::Path tail;
        tail.startNewSubPath (68.0f, 112.0f);
        if (style == CatStyle::Bend)
        {
            // Bend-Cat: the tail curls up into a spiral that winds tighter as it plays
            tail.cubicTo (88.0f, 116.0f, 100.0f, 100.0f, 94.0f, 88.0f);
            const float wind = 2.6f + sway * 0.6f;
            for (int k = 1; k <= 14; ++k)
            {
                const float a = (float) k / 14.0f * wind * juce::MathConstants<float>::pi;
                const float r = 9.0f * (1.0f - (float) k / 18.0f);
                tail.lineTo (86.0f + r * std::cos (a), 88.0f - r * std::sin (a));
            }
        }
        else
        {
            tail.cubicTo (88.0f, 116.0f, 98.0f, 100.0f + sway * 10.0f, 92.0f + sway * 8.0f, 78.0f - sway * 6.0f);
        }
        juce::Path stroked;
        juce::PathStrokeType (7.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (stroked, tail);
        parts.push_back (stroked);
    }

    // Head (with cheeks) and ears, tilted with pitch, bobbing with the beat
    {
        const auto headTilt = juce::AffineTransform::rotation (tilt, headCentre.x, headCentre.y + 12.0f);
        juce::Path head, cheeks;
        head.addEllipse (headCentre.x - 22.0f, headCentre.y - 19.0f, 44.0f, 36.0f);
        cheeks.addEllipse (headCentre.x - 24.0f, headCentre.y - 2.0f, 48.0f, 20.0f);

        const float twitch = earKick * 0.32f;
        auto leftEar = ear ({ headCentre.x - 21.0f, headCentre.y - 6.0f }, { headCentre.x - 19.0f, headCentre.y - 33.0f },
                            { headCentre.x - 3.0f, headCentre.y - 17.0f }, -twitch, { headCentre.x - 12.0f, headCentre.y - 12.0f });
        auto rightEar = ear ({ headCentre.x + 21.0f, headCentre.y - 6.0f }, { headCentre.x + 19.0f, headCentre.y - 33.0f },
                             { headCentre.x + 3.0f, headCentre.y - 17.0f }, twitch, { headCentre.x + 12.0f, headCentre.y - 12.0f });
        for (auto* part : { &head, &cheeks, &leftEar, &rightEar })
        {
            part->applyTransform (headTilt);
            parts.push_back (*part);
        }
    }

    g.setColour (bodyColour.withAlpha (1.0f));
    for (const auto& part : parts)
        g.fillPath (part, toScreen);
    juce::ignoreUnused (body);

    drawAccessory (g, toScreen, headCentre, std::sin (tailPhase));

    // Eyes: glow with the playing energy, blink when idle
    {
        const float open = juce::jlimit (0.12f, 1.0f, 1.0f - blink * (energy > 0.35f ? 0.0f : 1.0f));
        const float glow = 0.35f + 0.65f * energy;
        const auto eyeTransform = juce::AffineTransform::rotation (tilt, headCentre.x, headCentre.y + 12.0f).followedBy (toScreen);

        for (float side : { -1.0f, 1.0f })
        {
            const juce::Point<float> c (headCentre.x + side * 9.0f, headCentre.y - 1.0f);
            juce::Path halo, eye;
            halo.addEllipse (c.x - 6.5f, c.y - 6.5f * open, 13.0f, 13.0f * open);
            eye.addEllipse (c.x - 3.6f, c.y - 4.2f * open, 7.2f, 8.4f * open);
            g.setColour (accent.withAlpha (0.18f * glow));
            g.fillPath (halo, eyeTransform);
            g.setColour (accent.interpolatedWith (juce::Colours::white, 0.35f * energy).withAlpha (0.55f + 0.45f * glow));
            g.fillPath (eye, eyeTransform);
        }
    }
}

void CatVisualizer::drawAccessory (juce::Graphics& g, const juce::AffineTransform& toScreen, juce::Point<float> hc, float tailSway) const
{
    const auto headTilt = juce::AffineTransform::rotation (tilt, hc.x, hc.y + 12.0f).followedBy (toScreen);
    const auto accent = Colours::accent;
    const auto ink = bodyColour.darker (0.55f);
    auto stroke = [&g] (const juce::Path& p, float w, const juce::AffineTransform& t)
    {
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
    };

    switch (style)
    {
        case CatStyle::Chord:
        {
            // Headphones: band over the head, cups on the cheeks (cups pulse with the music)
            juce::Path band;
            band.addCentredArc (hc.x, hc.y - 2.0f, 25.0f, 24.0f, 0.0f, -1.45f, 1.45f, true);
            g.setColour (accent);
            stroke (band, 3.6f, headTilt);
            const float pulse = 1.0f + 0.12f * energy;
            for (float side : { -1.0f, 1.0f })
            {
                juce::Path cup;
                cup.addRoundedRectangle (hc.x + side * 25.0f - 5.5f * pulse, hc.y - 8.0f * pulse, 11.0f * pulse, 18.0f * pulse, 4.0f);
                g.fillPath (cup, headTilt);
            }
            break;
        }

        case CatStyle::Patt:
        {
            // Tabby stripes on the back and tail: a pattern cat
            g.setColour (ink.withAlpha (0.75f));
            for (int k = 0; k < 4; ++k)
            {
                juce::Path s;
                const float y = 70.0f + (float) k * 10.0f;
                s.startNewSubPath (28.0f + (float) k * 1.5f, y);
                s.quadraticTo (40.0f, y + 6.0f, 46.0f, y + 2.0f);
                stroke (s, 3.2f, toScreen);
            }
            for (int k = 0; k < 3; ++k)
            {
                juce::Path s;
                const float x = 80.0f + (float) k * 5.0f;
                s.startNewSubPath (x, 108.0f - (float) k * 9.0f);
                s.lineTo (x + 6.0f, 112.0f - (float) k * 11.0f);
                stroke (s, 3.0f, toScreen);
            }
            juce::Path forehead;                        // the classic "M" on a tabby's forehead
            forehead.startNewSubPath (hc.x - 7.0f, hc.y - 10.0f);
            forehead.lineTo (hc.x - 3.5f, hc.y - 15.0f);
            forehead.lineTo (hc.x, hc.y - 10.0f);
            forehead.lineTo (hc.x + 3.5f, hc.y - 15.0f);
            forehead.lineTo (hc.x + 7.0f, hc.y - 10.0f);
            stroke (forehead, 2.2f, headTilt);
            break;
        }

        case CatStyle::Bend:
        {
            // A pitch wave floating above the head, bending with the music
            juce::Path wave;
            for (int k = 0; k <= 24; ++k)
            {
                // drawn across the chest, so it never leaves the component
                const float x = 30.0f + (float) k * (40.0f / 24.0f);
                const float y = 82.0f + std::sin ((float) k * 0.55f + time * 4.0f) * (2.5f + 5.0f * energy) * std::sin ((float) k / 24.0f * 3.14159f);
                if (k == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
            }
            g.setColour (accent.withAlpha (0.85f));
            stroke (wave, 3.0f, toScreen);
            juce::ignoreUnused (tailSway, hc);
            break;
        }

        case CatStyle::Drum:
        {
            // Drum in front, two sticks that come down on the beat and on notes
            juce::Path shell;                               // snare: shell, rims and skin
            shell.addRectangle (22.0f, 96.0f, 56.0f, 12.0f);
            shell.addEllipse (22.0f, 102.0f, 56.0f, 12.0f);
            g.setColour (accent.darker (0.9f));
            g.fillPath (shell, toScreen);
            g.setColour (accent);
            juce::Path rim;
            rim.addEllipse (22.0f, 90.0f, 56.0f, 12.0f);
            g.fillPath (rim, toScreen);
            juce::Path skin;
            skin.addEllipse (24.5f, 91.5f, 51.0f, 9.0f);
            g.setColour (juce::Colour (0xffe8e9ef));
            g.fillPath (skin, toScreen);

            const float hit = juce::jmax (beatBob, bounce);   // 1 at the hit, then lifts again
            for (float side : { -1.0f, 1.0f })
            {
                const float lift = (1.0f - hit) * (side < 0 ? 1.0f : 0.6f);
                const juce::Point<float> hand (50.0f + side * 20.0f, 74.0f);
                const juce::Point<float> tip (50.0f + side * (8.0f + 6.0f * lift), 95.0f - 30.0f * lift);
                juce::Path stick;
                stick.startNewSubPath (hand);
                stick.lineTo (tip);
                g.setColour (juce::Colour (0xffb08850));
                stroke (stick, 3.4f, toScreen);
                juce::Path ball;
                ball.addEllipse (tip.x - 3.2f, tip.y - 3.2f, 6.4f, 6.4f);
                g.fillPath (ball, toScreen);
            }
            break;
        }

        case CatStyle::Orbit:
        {
            // Space helmet, and a little ringed planet orbiting the head
            juce::Path helmet;
            helmet.addEllipse (hc.x - 31.0f, hc.y - 36.0f, 62.0f, 60.0f);
            g.setColour (accent.withAlpha (0.08f));
            g.fillPath (helmet, headTilt);
            g.setColour (accent.withAlpha (0.6f));
            stroke (helmet, 1.8f, headTilt);

            const float a = time * (0.9f + 2.5f * energy);
            const juce::Point<float> planet (hc.x + 40.0f * std::cos (a), hc.y - 10.0f + 12.0f * std::sin (a));
            juce::Path ring;
            ring.addEllipse (planet.x - 8.0f, planet.y - 2.5f, 16.0f, 5.0f);
            juce::Path ball;
            ball.addEllipse (planet.x - 4.5f, planet.y - 4.5f, 9.0f, 9.0f);
            g.setColour (juce::Colour (0xffe2c48f));
            g.fillPath (ball, toScreen);
            g.setColour (juce::Colour (0xffe2c48f).withAlpha (0.8f));
            stroke (ring, 1.4f, toScreen);
            break;
        }

        case CatStyle::Sing:
        {
            // Singing: an open mouth that follows the voice, and a microphone
            juce::Path mouth;
            const float open = 2.0f + 7.0f * energy;
            mouth.addEllipse (hc.x - 4.5f, hc.y + 6.0f, 9.0f, open);
            g.setColour (ink);
            g.fillPath (mouth, headTilt);

            juce::Path handle;
            handle.startNewSubPath (64.0f, 92.0f);
            handle.lineTo (70.0f, 64.0f);
            g.setColour (ink);
            stroke (handle, 3.4f, toScreen);
            juce::Path mic;
            mic.addEllipse (65.0f, 52.0f, 11.0f, 13.0f);
            g.setColour (accent);
            g.fillPath (mic, toScreen);
            break;
        }

        case CatStyle::Bass:
        {
            // A bass guitar across the body; the strings shake with the notes
            juce::Path neck;
            neck.startNewSubPath (40.0f, 98.0f);
            neck.lineTo (82.0f, 50.0f);
            g.setColour (juce::Colour (0xff8a6a48));
            stroke (neck, 4.2f, toScreen);
            juce::Path head;
            head.addRoundedRectangle (79.0f, 43.0f, 8.0f, 11.0f, 2.0f);
            g.fillPath (head, toScreen);
            juce::Path body;
            body.addEllipse (22.0f, 88.0f, 30.0f, 20.0f);
            body.addEllipse (30.0f, 82.0f, 20.0f, 14.0f);
            g.setColour (accent);
            g.fillPath (body, toScreen);
            g.setColour (ink);
            for (int k = 0; k < 2; ++k)
            {
                juce::Path string;
                const float o = (float) k * 2.4f - 1.2f;
                for (int i = 0; i <= 12; ++i)
                {
                    const float u = (float) i / 12.0f;
                    const float wob = std::sin (u * 3.14159f) * std::sin (time * 40.0f + (float) k) * 1.6f * energy;
                    const juce::Point<float> pt (36.0f + u * 44.0f + o + wob, 97.0f - u * 46.0f + o * 0.6f + wob);
                    if (i == 0) string.startNewSubPath (pt); else string.lineTo (pt);
                }
                stroke (string, 0.9f, toScreen);
            }
            break;
        }

        case CatStyle::Glide:
        {
            // Gliding voice lines streaming past the cat, bending with the music
            const juce::Colour cols[] = { accent, Colours::teal, Colours::purple, juce::Colour (0xffe08aa0) };
            for (int k = 0; k < 4; ++k)
            {
                juce::Path line;
                for (int i = 0; i <= 24; ++i)
                {
                    const float u = (float) i / 24.0f;
                    const float x = 8.0f + u * 88.0f;
                    const float bend = std::tanh ((u - 0.5f) * 8.0f) * (6.0f + 10.0f * energy) * (k % 2 == 0 ? 1.0f : -1.0f);
                    const float y = 34.0f + (float) k * 9.0f + bend * 0.5f + std::sin (time * 2.0f + u * 6.0f + (float) k) * 1.5f;
                    if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
                }
                g.setColour (cols[k].withAlpha (0.75f));
                stroke (line, 2.0f, toScreen);
            }
            break;
        }

        case CatStyle::Talk:
        {
            // Pop idol: headset microphone, singing mouth and music notes floating up
            juce::Path mouth;
            const float open = 2.0f + 6.0f * energy;
            mouth.addEllipse (hc.x - 4.0f, hc.y + 6.5f, 8.0f, open);
            g.setColour (ink);
            g.fillPath (mouth, headTilt);

            juce::Path band;
            band.addCentredArc (hc.x, hc.y - 2.0f, 17.0f, 17.0f, 0.0f, -1.9f, 1.9f, true);
            g.setColour (ink);
            stroke (band, 2.4f, headTilt);
            juce::Path boom;
            boom.startNewSubPath (hc.x - 16.5f, hc.y + 2.0f);
            boom.quadraticTo (hc.x - 15.0f, hc.y + 13.0f, hc.x - 6.0f, hc.y + 11.0f);
            stroke (boom, 1.8f, headTilt);
            juce::Path capsule;
            capsule.addEllipse (hc.x - 7.5f, hc.y + 9.0f, 4.5f, 4.5f);
            g.setColour (accent);
            g.fillPath (capsule, headTilt);

            // notes rising from the mouth
            for (int k = 0; k < 3; ++k)
            {
                const float ph = std::fmod (time * 0.45f + (float) k / 3.0f, 1.0f);
                const float x = 62.0f + (float) k * 9.0f + std::sin (ph * 6.28f + (float) k) * 4.0f;
                const float y = 54.0f - ph * 40.0f;
                const float alpha = (0.25f + 0.75f * energy) * std::sin (ph * 3.14159f);
                juce::Path note;
                note.addEllipse (x, y, 6.0f, 4.5f);
                note.addRectangle (x + 5.0f, y - 10.0f, 1.4f, 12.0f);
                if (k == 1) note.addRectangle (x + 5.0f, y - 10.0f, 6.0f, 1.6f);
                g.setColour ((k == 1 ? Colours::teal : accent).withAlpha (juce::jlimit (0.0f, 1.0f, alpha)));
                g.fillPath (note, toScreen);
            }
            break;
        }

        case CatStyle::Plain:
        default:
            break;
    }
}

//==============================================================================
BootScreen::BootScreen (NoteMonitor& m, std::function<double()> getPpq, std::function<bool()> isRunning,
                        juce::String logoFirst, juce::String logoSecond, juce::String subtitle, CatStyle s)
    : cat (m, std::move (getPpq), std::move (isRunning), s),
      first (std::move (logoFirst)), second (std::move (logoSecond)), sub (std::move (subtitle))
{
    cat.setBodyColour (Colours::text.darker (0.08f));
    addAndMakeVisible (cat);
    startTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (60);
}

void BootScreen::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    const auto r = getLocalBounds().toFloat();
    const float logoH = juce::jmax (28.0f, r.getHeight() * 0.075f);
    const auto logo = juce::Rectangle<float> (r.getWidth(), logoH).withCentre ({ r.getCentreX(), cat.getBottom() + logoH * 1.1f });

    // Centre the two-part logo
    juce::GlyphArrangement ga;
    ga.addLineOfText (catFont (logoH * 0.72f, true), first, 0.0f, 0.0f);
    juce::GlyphArrangement gb;
    gb.addLineOfText (catFont (logoH * 0.72f, false), second, 0.0f, 0.0f);
    const float w = ga.getBoundingBox (0, -1, true).getWidth() + gb.getBoundingBox (0, -1, true).getWidth() + 4.0f;
    CatLookAndFeel::drawLogo (g, logo.withSizeKeepingCentre (w + 8.0f, logoH), first, second);

    g.setColour (Colours::textDim);
    g.setFont (catFont (juce::jmax (11.0f, logoH * 0.34f)));
    g.drawText (sub, logo.translated (0.0f, logoH * 0.9f), juce::Justification::centred);
}

void BootScreen::resized()
{
    const auto r = getLocalBounds();
    const int h = juce::roundToInt (r.getHeight() * 0.42f);
    cat.setBounds (juce::Rectangle<int> (juce::roundToInt (h * 0.9f), h).withCentre ({ r.getCentreX(), r.getCentreY() - h / 4 }));
}

void BootScreen::mouseDown (const juce::MouseEvent&)
{
    startTime = juce::jmin (startTime, juce::Time::getMillisecondCounterHiRes() * 0.001 - 1.6);
}

void BootScreen::timerCallback()
{
    // Visible for 1.6 s, then fades out over 0.45 s and hands over to the main screen
    const double elapsed = juce::Time::getMillisecondCounterHiRes() * 0.001 - startTime;
    const float fade = (float) juce::jlimit (0.0, 1.0, (elapsed - 1.6) / 0.45);
    setAlpha (1.0f - fade);
    if (fade >= 1.0f)
    {
        stopTimer();
        setVisible (false);
    }
}

} // namespace cat

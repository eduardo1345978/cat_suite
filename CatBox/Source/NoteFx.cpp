#include "NoteFx.h"
#include "Music.h"
#include <cmath>

namespace catbox
{
namespace
{
enum ArpP { aRate = 0, aMode, aOct, aGate, aSwing, aHold, aSteps, aHits, aRotate, aRatchet, aChance, aAccent, aTranspose, aVelMode, aHuman, aRepeat };
enum ChordP { cType = 0, cVoicing, cInversion, cStrum, cStrumDir, cBass, cOctave, cInKey, cVelSpread, cLength, cTop, cVelScale };
enum OrbitP { oGravity = 0, oDrag, oTurb, oLines, oAngle, oMass, oRadius, oEcc, oSpeed, oGate, oGrid, oHold, oDamp, oBoost, oOctave, oSpread };

const double arpBeats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
const double chordLenBeats[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0 };

ParamDef ranged (ParamDef d, float lo, float hi) { d.lo = lo; d.hi = hi; return d; }

EngineDef makeOff()
{
    EngineDef e;
    e.name = "Off";
    return e;
}

EngineDef makeArp()
{
    using namespace defs;
    EngineDef e;
    e.name = "Arp";
    e.params = {
        choice ("rate", "Rate", { "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32" }, 3),
        choice ("mode", "Order", { "Up", "Down", "Up-Down", "Bounce", "Random", "As played", "Chord" }, 0),
        num ("oct", "Octaves", 1, 4, 1),
        ranged (pct ("gate", "Gate", 0.6f), 0.05f, 1.5f),
        ranged (pct ("swing", "Swing", 0.0f), 0.0f, 0.75f),
        toggle ("hold", "Hold", false),
        num ("steps", "Steps", 1, 16, 16), num ("hits", "Hits", 1, 16, 16), num ("rotate", "Rotate", 0, 15, 0),
        num ("ratchet", "Ratchet", 1, 4, 1), pct ("chance", "Chance", 1.0f), num ("accent", "Accent every", 0, 8, 0),
        semi ("transpose", "Transpose", -24, 24, 0), choice ("velmode", "Velocity", { "As played", "Fixed", "Rise", "Fall" }, 0),
        pct ("human", "Humanize", 0.0f), num ("repeat", "Repeat", 1, 4, 1),
    };
    e.pages = { page ("ARP", { aRate, aMode, aOct, aGate, aSwing, aHold, aSteps, aHits, aRotate, aRatchet, aChance, aAccent,
                               aTranspose, aVelMode, aHuman, aRepeat }) };
    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, const char* cat, V v) { e.presets.push_back ({ name, cat, std::move (v) }); };
    add ("Up 16ths", "Arp", {});
    add ("Up-Down 2 oct", "Arp", { { "mode", 2 }, { "oct", 2 } });
    add ("Trance Gate", "Arp", { { "mode", 6 }, { "gate", 0.35f } });
    add ("Random Sparkle", "Arp", { { "mode", 4 }, { "oct", 3 }, { "rate", 5 }, { "chance", 0.7f }, { "gate", 0.3f } });
    add ("Euclid 5 of 8", "Arp", { { "steps", 8 }, { "hits", 5 }, { "accent", 3 } });
    add ("Euclid 3 of 8", "Arp", { { "steps", 8 }, { "hits", 3 }, { "rate", 3 }, { "gate", 0.9f } });
    add ("Swing 8ths", "Arp", { { "rate", 1 }, { "swing", 0.35f }, { "mode", 2 } });
    add ("Ratchet Roll", "Arp", { { "ratchet", 3 }, { "chance", 0.8f }, { "gate", 0.4f } });
    add ("Triplets", "Arp", { { "rate", 4 }, { "mode", 3 }, { "oct", 2 } });
    add ("Played Order", "Arp", { { "mode", 5 }, { "hold", 1 } });
    add ("Bass Pulse", "Arp", { { "rate", 1 }, { "mode", 0 }, { "gate", 0.5f }, { "transpose", -12 }, { "accent", 4 } });
    add ("Init", "Init", {});
    return e;
}

EngineDef makeChords()
{
    using namespace defs;
    EngineDef e;
    e.name = "Chords";
    e.params = {
        choice ("type", "Chord", { "Triad", "7th", "9th", "11th", "6th", "Sus 2", "Sus 4", "Power", "Octaves" }, 0),
        choice ("voicing", "Voicing", { "Close", "Open", "Drop 2", "Spread", "Wide" }, 0),
        num ("inv", "Inversion", 0, 3, 0),
        sec ("strum", "Strum", 0.0f, 0.3f, 0.0f),
        choice ("strumdir", "Strum dir", { "Up", "Down", "Alternate", "Random" }, 0),
        choice ("bass", "Bass note", { "Off", "-1 oct", "-2 oct" }, 0),
        num ("octave", "Octave", -2, 2, 0),
        toggle ("inkey", "In key", true),
        pct ("velspread", "Vel spread", 0.1f),
        choice ("length", "Length", { "Held", "1/16", "1/8", "1/4", "1/2", "1 bar" }, 0),
        toggle ("top", "Top octave", false),
        pct ("velscale", "Velocity", 1.0f),
    };
    e.pages = { page ("CHORDS", { cType, cVoicing, cInversion, cStrum, cStrumDir, cBass, cOctave, cInKey, cVelSpread, cLength, cTop, cVelScale }) };
    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, const char* cat, V v) { e.presets.push_back ({ name, cat, std::move (v) }); };
    add ("Triads", "Chords", {});
    add ("Sevenths", "Chords", { { "type", 1 } });
    add ("Neo Soul 9ths", "Chords", { { "type", 2 }, { "voicing", 2 }, { "strum", 0.03f } });
    add ("Open Pads", "Chords", { { "type", 1 }, { "voicing", 1 }, { "bass", 1 } });
    add ("Guitar Strum", "Chords", { { "voicing", 3 }, { "strum", 0.06f }, { "strumdir", 2 } });
    add ("House Stabs", "Chords", { { "type", 1 }, { "voicing", 2 }, { "length", 2 } });
    add ("Sus Dreams", "Chords", { { "type", 5 }, { "voicing", 4 }, { "top", 1 } });
    add ("Power", "Chords", { { "type", 7 }, { "bass", 1 } });
    add ("Init", "Init", {});
    return e;
}

EngineDef makeOrbit()
{
    using namespace defs;
    EngineDef e;
    e.name = "Orbit";
    ParamDef speed = num ("speed", "Speed", 0, 1, 1);
    speed.unit = ParamDef::Plain; speed.suffix = " x"; speed.lo = 0.25f; speed.hi = 4.0f; speed.def = 1.0f; speed.skew = 0.5f;
    e.params = {
        pct ("gravity", "Gravity", 0.5f), pct ("drag", "Drag", 0.0f), pct ("turb", "Turbulence", 0.0f),
        num ("lines", "Lines", 1, 6, 1), num ("angle", "Line angle", 0, 359, 0), num ("mass", "Mass", 1, 100, 50),
        num ("radius", "Orbit size", 1, 32, 8), ranged (pct ("ecc", "Ellipse", 0.0f), 0.0f, 0.9f), speed,
        sec ("gate", "Note length", 0.03f, 2.0f, 0.2f), choice ("grid", "Grid", orbitcat::getQuantizeNames(), 3), toggle ("hold", "Hold", false),
        pct ("damp", "Damping", 0.5f), pct ("boost", "Speed > loud", 0.0f), num ("octave", "Octave", -2, 2, 0), pct ("spread", "Spread", 0.3f),
    };
    e.pages = { page ("ORBIT", { oGravity, oDrag, oTurb, oLines, oAngle, oMass, oRadius, oEcc, oSpeed, oGate, oGrid, oHold,
                                 oDamp, oBoost, oOctave, oSpread }) };
    for (const auto& op : orbitcat::getOrbitPresets())
    {
        const auto& b0 = op.bodies[0];
        const auto& b1 = op.bodies[1];
        const float spread = juce::jlimit (0.0f, 1.0f, (b1.radius - b0.radius) / juce::jmax (0.5f, b0.radius * 0.35f));
        e.presets.push_back ({ op.name, "Orbit",
            { { "gravity", op.global.gravity }, { "drag", op.global.drag }, { "turb", op.global.turbulence },
              { "lines", (float) op.global.spokes }, { "angle", op.alignDegrees }, { "mass", b0.mass }, { "radius", b0.radius },
              { "ecc", b0.eccentricity }, { "speed", b0.speed }, { "gate", b0.gate }, { "grid", (float) op.quantize },
              { "damp", b0.damp }, { "boost", b0.boost }, { "octave", (float) b0.octave }, { "spread", spread } } });
    }
    e.presets.push_back ({ "Init", "Init", {} });
    return e;
}
} // namespace

const EngineDef& noteFxDef (int type)
{
    static const std::array<EngineDef, NumNoteFx> all { makeOff(), makeArp(), makeChords(), makeOrbit() };
    return all[(size_t) juce::jlimit (0, NumNoteFx - 1, type)];
}

juce::StringArray noteFxNames() { return { "Off", "Arp", "Chords", "Orbit" }; }

//==============================================================================
void NoteFx::prepare (double sampleRate)
{
    sr = sampleRate;
    numPending = 0;
    numKeys = 0;
    sounding.fill (0);
    orbit.reset();
    orbitDown.fill (false);
    chordSize.fill (0);
    nextArp = -1.0;
}

void NoteFx::set (int t, const float* v, int k, int s)
{
    for (int i = 0; i < numFxParams; ++i) values[(size_t) i] = v[i];
    key = k;
    scale = s;
    type = t;
    if (type == FxArp && p (aHold) < 0.5f && physicalDown == 0 && numKeys > 0) { numKeys = 0; nextArp = -1.0; }   // Hold off: the held notes go
    if (type == FxOrbit && p (oHold) < 0.5f)
        for (auto& b : orbit.bodies)
            if (b.active && ! orbitDown[(size_t) juce::jlimit (0, 127, b.note)]) orbit.removeNote (b.note, b.channel);
    if (type == FxOrbit)
    {
        og.gravity = p (oGravity); og.drag = p (oDrag); og.turbulence = p (oTurb);
        og.spokes = juce::roundToInt (p (oLines));
        og.alignRadians = p (oAngle) * juce::MathConstants<double>::pi / 180.0;
        for (int i = 0; i < orbitcat::numSlots; ++i)
        {
            auto& b = op[(size_t) i];
            b.mass = p (oMass);
            b.radius = juce::jlimit (1.0f, 32.0f, p (oRadius) * (1.0f + p (oSpread) * 0.35f * (float) i));
            b.eccentricity = p (oEcc);
            b.speed = p (oSpeed);
            b.octave = juce::roundToInt (p (oOctave));
            b.damp = p (oDamp);
            b.gate = p (oGate);
            b.boost = p (oBoost);
        }
    }
}

void NoteFx::schedule (double ppq, int note, float vel, bool on, bool accent)
{
    if (numPending >= (int) pending.size()) return;
    pending[(size_t) numPending++] = { ppq, juce::jlimit (0, 127, note), vel, on, accent };
}

void NoteFx::runPending (double upTo, NoteSink& sink)
{
    // in time order (offs before ons at the same time, so a repeated note restarts cleanly)
    for (;;)
    {
        int best = -1;
        for (int i = 0; i < numPending; ++i)
        {
            const auto& e = pending[(size_t) i];
            if (e.ppq > upTo) continue;
            if (best < 0 || e.ppq < pending[(size_t) best].ppq || (e.ppq == pending[(size_t) best].ppq && ! e.on)) best = i;
        }
        if (best < 0) return;
        const auto e = pending[(size_t) best];
        pending[(size_t) best] = pending[(size_t) --numPending];
        auto& s = sounding[(size_t) e.note];
        if (e.on)
        {
            if (s > 0) sink.fxNoteOff (e.note);   // the same note again: restart it
            sink.fxNoteOn (e.note, e.vel, false, e.accent);
            s = 1;
        }
        else if (s > 0)
        {
            s = 0;
            sink.fxNoteOff (e.note);
        }
    }
}

void NoteFx::releaseAll (NoteSink& sink)
{
    for (int n = 0; n < 128; ++n)
        if (sounding[(size_t) n] > 0) { sounding[(size_t) n] = 0; sink.fxNoteOff (n); }
    numPending = 0;
}

void NoteFx::reset (NoteSink& sink)
{
    releaseAll (sink);
    numKeys = 0;
    physicalDown = 0;
    orbit.reset();
    orbitDown.fill (false);
    chordSize.fill (0);
    nextArp = -1.0;
}

//==============================================================================
void NoteFx::buildChord (int root, std::array<int, 8>& out, int& n) const
{
    n = 0;
    const int t = juce::roundToInt (p (cType));
    const bool inKey = p (cInKey) > 0.5f && t <= 4;
    auto push = [&] (int note) { if (n < 8) out[(size_t) n++] = note; };
    if (inKey)
    {
        int o = 0, d = 0;
        music::degreeOf (root, key, scale, o, d);
        const int base = music::noteOfDegree (key, scale, o, d);
        const int shift = root - base;   // a key outside the scale keeps its own root
        static const int degs[5][6] = { { 0, 2, 4, -1 }, { 0, 2, 4, 6, -1 }, { 0, 2, 4, 6, 8, -1 }, { 0, 2, 4, 6, 8, 10 }, { 0, 2, 4, 5, -1 } };
        for (int i = 0; i < 6 && degs[t][i] >= 0; ++i)
            push (music::noteOfDegree (key, scale, o, d + degs[t][i]) + shift);
    }
    else
    {
        static const int iv[9][6] = { { 0, 4, 7, -1 }, { 0, 4, 7, 10, -1 }, { 0, 4, 7, 10, 14, -1 }, { 0, 4, 7, 10, 14, 17 },
                                      { 0, 4, 7, 9, -1 }, { 0, 2, 7, -1 }, { 0, 5, 7, -1 }, { 0, 7, 12, -1 }, { 0, 12, -1 } };
        for (int i = 0; i < 6 && iv[t][i] >= 0; ++i)
        {
            if (i > 0 && iv[t][i] == 0) break;
            push (root + iv[t][i]);
        }
    }
    std::sort (out.begin(), out.begin() + n);
    // inversion: the lowest notes go up an octave
    for (int i = 0; i < juce::roundToInt (p (cInversion)) && n > 1; ++i)
    {
        const int low = out[0] + 12;
        for (int k = 0; k < n - 1; ++k) out[(size_t) k] = out[(size_t) k + 1];
        out[(size_t) n - 1] = low;
    }
    // voicing
    switch (juce::roundToInt (p (cVoicing)))
    {
        case 1: if (n >= 3) out[1] += 12; break;                                      // open: the middle note up
        case 2: if (n >= 3) out[(size_t) n - 2] -= 12; break;                         // drop 2
        case 3: for (int i = 1; i < n; i += 2) out[(size_t) i] += 12; break;          // spread
        case 4: if (n >= 3) { out[0] -= 12; for (int i = 2; i < n; i += 2) out[(size_t) i] += 12; } break;          // wide: root down, every other up
        default: break;
    }
    std::sort (out.begin(), out.begin() + n);
    if (p (cTop) > 0.5f && n > 0) push (out[(size_t) n - 1] + 12);
    const int bass = juce::roundToInt (p (cBass));
    if (bass > 0 && n < 8) { for (int i = n; i > 0; --i) out[(size_t) i] = out[(size_t) i - 1]; out[0] = root - 12 * bass; ++n; }
    const int oct = 12 * juce::roundToInt (p (cOctave));
    for (int i = 0; i < n; ++i) out[(size_t) i] = juce::jlimit (0, 127, out[(size_t) i] + oct);
}

//==============================================================================
void NoteFx::noteOn (int note, float vel, bool slide, bool accent, double now, NoteSink& sink)
{
    note = juce::jlimit (0, 127, note);
    switch (type)
    {
        case FxArp:
        {
            const bool hold = p (aHold) > 0.5f;
            if (hold && physicalDown == 0) numKeys = 0;   // Hold: a new chord replaces the held one
            ++physicalDown;
            for (int i = 0; i < numKeys; ++i) if (keys[(size_t) i] == note) return;
            if (numKeys < 16) { keys[(size_t) numKeys] = note; keyVel[(size_t) numKeys] = vel; ++numKeys; }
            if (nextArp < 0.0 || numKeys == 1)
            {
                // the first key: start right away (on the next grid point if it is very close)
                nextArp = now;
                arpStep = 0; arpIndex = 0; arpDir = 1; hitCount = 0;
            }
            return;
        }
        case FxChords:
        {
            std::array<int, 8> notes {};
            int n = 0;
            buildChord (note, notes, n);
            const int dir = juce::roundToInt (p (cStrumDir));
            const bool down = dir == 1 || (dir == 2 && (strumAlt++ & 1));
            const double strumPpq = p (cStrum) * (double) bpmCache / 60.0;
            for (int i = 0; i < n; ++i)
            {
                const int k = down ? n - 1 - i : i;
                double when = now + (dir == 3 ? rng.nextDouble() : (double) i / juce::jmax (1, n - 1)) * strumPpq * (n > 1 ? 1.0 : 0.0);
                if (dir == 3 && i == 0) when = now;
                const float v = juce::jlimit (0.05f, 1.0f, vel * p (cVelScale) * (1.0f - p (cVelSpread) * rng.nextFloat()));
                schedule (when, notes[(size_t) k], v, true, accent);
                const double len = chordLenBeats[juce::jlimit (0, 5, juce::roundToInt (p (cLength)))];
                if (len > 0.0) schedule (when + len, notes[(size_t) k], 0.0f, false, false);
            }
            chordOf[(size_t) note] = notes;
            chordSize[(size_t) note] = n;
            runPending (now, sink);
            return;
        }
        case FxOrbit:
        {
            orbitDown[(size_t) note] = true;
            orbit.addNote (note, 1, og, op, rng);
            return;
        }
        default:
            sink.fxNoteOn (note, vel, slide, accent);
            sounding[(size_t) note] = 1;
            return;
    }
}

void NoteFx::noteOff (int note, double now, NoteSink& sink)
{
    note = juce::jlimit (0, 127, note);
    switch (type)
    {
        case FxArp:
        {
            physicalDown = juce::jmax (0, physicalDown - 1);
            if (p (aHold) > 0.5f) return;
            int n = 0;
            for (int i = 0; i < numKeys; ++i)
                if (keys[(size_t) i] != note) { keys[(size_t) n] = keys[(size_t) i]; keyVel[(size_t) n] = keyVel[(size_t) i]; ++n; }
            numKeys = n;
            if (numKeys == 0) nextArp = -1.0;
            return;
        }
        case FxChords:
        {
            if (juce::roundToInt (p (cLength)) > 0) { chordSize[(size_t) note] = 0; return; }   // fixed length: the notes end by themselves
            for (int i = 0; i < chordSize[(size_t) note]; ++i)
            {
                // a strummed note not started yet is dropped
                const int cn = chordOf[(size_t) note][(size_t) i];
                for (int k = 0; k < numPending;)
                    if (pending[(size_t) k].note == cn && pending[(size_t) k].on) pending[(size_t) k] = pending[(size_t) --numPending];
                    else ++k;
                schedule (now, cn, 0.0f, false, false);
            }
            chordSize[(size_t) note] = 0;
            runPending (now, sink);
            return;
        }
        case FxOrbit:
        {
            orbitDown[(size_t) note] = false;
            if (p (oHold) < 0.5f) orbit.removeNote (note, 1);
            return;
        }
        default:
            if (sounding[(size_t) note] > 0) { sounding[(size_t) note] = 0; sink.fxNoteOff (note); }
            return;
    }
}

//==============================================================================
void NoteFx::arpTick (double t, double stepLen, NoteSink& sink)
{
    // the euclidean rhythm: does this step play?
    const int steps = juce::jlimit (1, 16, juce::roundToInt (p (aSteps)));
    const int hits = juce::jlimit (1, steps, juce::roundToInt (p (aHits)));
    const int pos = ((arpStep + juce::roundToInt (p (aRotate))) % steps + steps) % steps;
    const bool onBeat = ((pos * hits) % steps) < hits;
    ++arpStep;
    if (! onBeat || numKeys == 0) return;
    if (rng.nextFloat() > p (aChance)) return;

    // the note list: sorted (or as played), over the octaves
    std::array<int, 64> list {};
    std::array<float, 64> vels {};
    int n = 0;
    const int mode = juce::roundToInt (p (aMode));
    std::array<int, 16> order {};
    for (int i = 0; i < numKeys; ++i) order[(size_t) i] = i;
    if (mode != 5) std::sort (order.begin(), order.begin() + numKeys, [this] (int a, int b) { return keys[(size_t) a] < keys[(size_t) b]; });
    const int octs = juce::jlimit (1, 4, juce::roundToInt (p (aOct)));
    for (int o = 0; o < octs; ++o)
        for (int i = 0; i < numKeys && n < 64; ++i)
        {
            list[(size_t) n] = keys[(size_t) order[(size_t) i]] + 12 * o;
            vels[(size_t) n] = keyVel[(size_t) order[(size_t) i]];
            ++n;
        }
    if (n == 0) return;

    const int transpose = juce::roundToInt (p (aTranspose));
    const int repeat = juce::jlimit (1, 4, juce::roundToInt (p (aRepeat)));
    const int accentEvery = juce::roundToInt (p (aAccent));
    const bool accent = accentEvery > 0 && (hitCount % accentEvery) == 0;
    const int velMode = juce::roundToInt (p (aVelMode));
    auto velocity = [&] (float played, int idx)
    {
        float v = played;
        if (velMode == 1) v = 0.8f;
        else if (velMode == 2) v = 0.45f + 0.55f * (float) (idx % 8) / 7.0f;
        else if (velMode == 3) v = 1.0f - 0.55f * (float) (idx % 8) / 7.0f;
        v *= 1.0f - p (aHuman) * 0.3f * rng.nextFloat();
        if (accent) v = juce::jmin (1.0f, v * 1.25f + 0.1f);
        return juce::jlimit (0.05f, 1.0f, v);
    };

    const int ratchet = juce::jlimit (1, 4, juce::roundToInt (p (aRatchet)));
    const double gate = juce::jmax (0.02, (double) p (aGate) * stepLen / ratchet);
    const double jitter = p (aHuman) * 0.08 * stepLen;
    auto play = [&] (int note, float v)
    {
        for (int r = 0; r < ratchet; ++r)
        {
            const double at = t + r * stepLen / ratchet + (jitter > 0.0 ? rng.nextDouble() * jitter : 0.0);
            const int nn = juce::jlimit (0, 127, note + transpose);
            schedule (at, nn, v * (r == 0 ? 1.0f : 0.8f), true, accent);
            schedule (at + gate, nn, 0.0f, false, false);
        }
    };

    if (mode == 6)   // chord: everything at once
    {
        for (int i = 0; i < n; ++i) play (list[(size_t) i], velocity (vels[(size_t) i], hitCount));
        ++hitCount;
        return;
    }

    // which note now
    const int rep = (hitCount / repeat);
    int idx = 0;
    switch (mode)
    {
        case 1: idx = n - 1 - (rep % n); break;                                            // down
        case 2: { const int cyc = juce::jmax (1, 2 * n - 2); const int k = rep % cyc; idx = k < n ? k : cyc - k; break; }   // up-down
        case 3: { const int cyc = 2 * n; const int k = rep % cyc; idx = k < n ? k : cyc - 1 - k; break; }                  // bounce (ends repeated)
        case 4: idx = (hitCount % repeat == 0) ? (arpIndex = rng.nextInt (n)) : juce::jmin (arpIndex, n - 1); break;      // random
        default: idx = rep % n; break;                                                      // up, as played
    }
    play (list[(size_t) idx], velocity (vels[(size_t) idx], hitCount));
    ++hitCount;
}

void NoteFx::advance (double from, double to, double origin, double bpm, NoteSink& sink)
{
    bpmCache = (float) bpm;
    if (type == FxArp && numKeys > 0)
    {
        const double stepLen = arpBeats[juce::jlimit (0, 5, juce::roundToInt (p (aRate)))];
        if (nextArp >= 0.0 && nextArp < from - stepLen) nextArp = from;
        // the first note snaps to the grid when the grid point is close (played a hair late)
        while (nextArp >= 0.0 && nextArp < to)
        {
            double t = nextArp;
            const double rel = (t - origin) / stepLen;
            const double snapped = origin + std::round (rel) * stepLen;
            if (std::abs (snapped - t) < stepLen * 0.25) t = snapped;
            const bool odd = ((long long) std::llround ((t - origin) / stepLen)) % 2 != 0;
            const double swung = odd ? t + p (aSwing) * stepLen * 0.5 : t;
            arpTick (juce::jmax (from, swung), stepLen, sink);
            nextArp = snapped + stepLen;
            if (nextArp <= t) nextArp = t + stepLen;
        }
    }
    else if (type == FxOrbit && orbit.numActive() > 0)
    {
        const double grid = orbitcat::quantizeToPpq (juce::roundToInt (p (oGrid)));
        const double secondsToPpq = bpm / 60.0;
        orbit.step (to - from, og, op, [&] (int i, double frac, double speedRatio)
        {
            const auto& b = orbit.bodies[(size_t) i];
            const auto& bp = op[(size_t) b.slot];
            double t = from + frac * (to - from);
            if (grid > 0.0) t = origin + std::ceil ((t - origin) / grid - 1.0e-9) * grid;
            const float fast = juce::jlimit (0.0f, 1.0f, (float) (speedRatio - 1.0) * 0.5f);
            const float level = juce::jlimit (0.1f, 1.0f, 0.45f + 0.35f * bp.mass / 100.0f + 0.3f * bp.boost * fast);
            const int note = juce::jlimit (0, 127, b.note + 12 * bp.octave);
            schedule (t, note, level, true, false);
            schedule (t + juce::jmax (0.03, (double) bp.gate * secondsToPpq), note, 0.0f, false, false);
        });
    }
    runPending (to, sink);
}
} // namespace catbox

// Cat Box Pocket: the sound engine (voices, sequencer, recording, effects).
#include "pocket.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace pocket {

static inline float clampf (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float softclip (float x) { x = clampf (x, -1.5f, 1.5f); return x - x * x * x * (4.0f / 27.0f); }
static inline int16_t toS16 (float x) { return (int16_t) clampf (x * 32767.0f + (x >= 0 ? 0.5f : -0.5f), -32768.0f, 32767.0f); }   // rounded
static constexpr float PI = 3.14159265358979f;

uint32_t Engine::rng () { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }

void Engine::init (int16_t* pool, uint32_t count, uint32_t s) {
    mem = pool; memSize = count; memUsed = 0; seed = s ? s : 1;
    for (auto& b : blocks) b = Block ();
    for (auto& sl : slots) sl = Slot ();
    for (auto& p : pats) p = Pattern ();
    for (auto& v : voices) v = Voice ();
    scenes[0].set = scenes[1].set = false; xfade = 0;
    chain[0] = 0; chainLen = 1; chainPos = 0; pat = 0; step = -1; loops = 0;
    songLen = 0; songMode = false; songRow = songRep = 0;
    undoValid = false; arpN = 0; loopState = LOOP_OFF; loopSlot = -1; recSlot = -1;
    running = false;
    static const int CL[NC] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617}, AL[NAP] = {556, 441, 341, 225};
    for (int c = 0; c < 2; ++c) {
        for (int i = 0; i < NC; ++i) { combLen[c][i] = std::min (CMAX, (CL[i] + c * 23) * SR / 44100); combPos[c][i] = 0; combLp[c][i] = 0; }
        for (int i = 0; i < NAP; ++i) { apLen[c][i] = std::min (AMAX, (AL[i] + c * 23) * SR / 44100); apPos[c][i] = 0; }
    }
    std::memset (comb, 0, sizeof comb); std::memset (ap, 0, sizeof ap);
}

// ------------------------------------------------------------------ names
const char* Engine::condName (int c) {
    static const char* N[NCOND] = {"ALWAYS", "1:2", "2:2", "1:3", "2:3", "3:3", "1:4", "2:4", "3:4", "4:4", "FILL", "!FILL", "1ST", "!1ST", "25%", "50%", "75%"};
    return c >= 0 && c < NCOND ? N[c] : "?";
}
static const int8_t SCALES[11][12] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10},
    {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 6, 7, 9, 11}, {0, 2, 4, 5, 7, 9, 10}, {0, 2, 4, 7, 9}, {0, 3, 5, 7, 10},
    {0, 3, 5, 6, 7, 10}, {0, 2, 3, 5, 7, 8, 11}};
static const int SCALELEN[11] = {12, 7, 7, 7, 7, 7, 7, 5, 5, 6, 7};
const char* Engine::scaleName (int s) {
    static const char* N[11] = {"CHROMA", "MAJOR", "MINOR", "DOR", "PHRYG", "LYD", "MIXO", "PENTA MAJ", "PENTA MIN", "BLUES", "HARM MIN"};
    return s >= 0 && s < NSCALE ? N[s] : "?";
}
int Engine::scaleSize (int s) { return s >= 0 && s < NSCALE ? SCALELEN[s] : 12; }
int Engine::padSemis (int pad) const {
    const int s = std::max (0, std::min (NSCALE - 1, scale)), n = SCALELEN[s];
    return root + (pad / n) * 12 + SCALES[s][pad % n];
}

// ------------------------------------------------------------------ the sample pool
int Engine::newBlock (uint32_t len) {
    if (len == 0 || memUsed + len > memSize) return -1;
    for (int i = 0; i < NBLOCK; ++i)
        if (blocks[i].refs == 0 && blocks[i].len == 0) {
            blocks[i].off = memUsed; blocks[i].len = len; blocks[i].refs = 1; memUsed += len;
            return i;
        }
    return -1;
}

void Engine::release (int b) {
    if (b < 0 || b >= NBLOCK || blocks[b].refs == 0) return;
    if (--blocks[b].refs > 0) return;
    // free it: move everything behind it down (the audio plays silence meanwhile)
    busy = true;
    const uint32_t off = blocks[b].off, len = blocks[b].len;
    for (auto& v : voices) if (v.on && slots[v.slot].block == b) v.on = false;
    if (off + len < memUsed) std::memmove (mem + off, mem + off + len, (memUsed - off - len) * sizeof (int16_t));
    memUsed -= len;
    for (auto& o : blocks) if (o.len && o.off > off) o.off -= len;
    blocks[b].len = 0; blocks[b].off = 0;
    for (auto& s : slots) if (s.block == b) s = Slot ();
    busy = false;
}

float Engine::memoryFree () const { return (memSize - memUsed) / float (SR); }

void Engine::clearSlot (int s) {
    if (s < 0 || s >= NSLOT || slots[s].empty ()) return;
    for (auto& v : voices) if (v.on && v.slot == s) v.on = false;
    if (loopSlot == s) { loopState = LOOP_OFF; loopSlot = -1; }
    const int b = slots[s].block;
    slots[s] = Slot ();
    release (b);
}

bool Engine::assign (int s, const int16_t* data, uint32_t len, uint32_t rate) {
    if (s < 0 || s >= NSLOT || len == 0) return false;
    clearSlot (s);
    const int b = newBlock (len);
    if (b < 0) return false;
    std::memcpy (mem + blocks[b].off, data, len * sizeof (int16_t));
    Slot& sl = slots[s]; sl = Slot (); sl.block = (int16_t) b; sl.start = 0; sl.len = len; sl.rate = rate;
    return true;
}

uint32_t Engine::slotData (int s, const int16_t** data) const {
    if (s < 0 || s >= NSLOT || slots[s].empty ()) { *data = nullptr; return 0; }
    *data = mem + blocks[slots[s].block].off + slots[s].start;
    return slots[s].len;
}

void Engine::trimNormalize (int s) {
    if (s < 0 || s >= NSLOT || slots[s].empty ()) return;
    Slot& sl = slots[s];
    int16_t* d = mem + blocks[sl.block].off + sl.start;
    const int thr = 330;          // -40 dB
    uint32_t a = 0, b = sl.len;
    while (a < b && std::abs ((int) d[a]) < thr) ++a;
    while (b > a && std::abs ((int) d[b - 1]) < thr) --b;
    if (b - a < 64) return;
    const uint32_t pre = sl.rate / 500;                       // keep 2 ms before the attack
    a = a > pre ? a - pre : 0; b = std::min (sl.len, b + sl.rate / 100);
    int peak = 1; for (uint32_t i = a; i < b; ++i) peak = std::max (peak, std::abs ((int) d[i]));
    const float g = 0.89f * 32767.0f / peak;                 // -1 dB
    for (auto& v : voices) if (v.on && v.slot == s) v.on = false;
    sl.start += a; sl.len = b - a; sl.trimA = 0; sl.trimB = 1; sl.saved = false;
    if (blocks[sl.block].refs == 1) { int16_t* p = d + a; for (uint32_t i = 0; i < sl.len; ++i) p[i] = toS16 (p[i] * g / 32768.0f); }
    else sl.volume = std::min (4.0f, sl.volume * g);
}

// ------------------------------------------------------------------ scenes (crossfader A..B)
static float sceneGet (const Scene& sc, int kind, int s) { return kind == 0 ? sc.pitch[s] : kind == 1 ? sc.filter[s] : sc.volume[s]; }
float Engine::sceneVal (int kind, int s, float own) const {
    if (! scenes[0].set && ! scenes[1].set) return own;
    const float a = scenes[0].set ? sceneGet (scenes[0], kind, s) : own;
    const float b = scenes[1].set ? sceneGet (scenes[1], kind, s) : own;
    return a + (b - a) * xfade;
}
void Engine::sceneStore (int ab) {
    Scene& sc = scenes[ab & 1];
    for (int s = 0; s < NSLOT; ++s) { sc.pitch[s] = slots[s].pitch; sc.filter[s] = slots[s].filter; sc.volume[s] = slots[s].volume; }
    sc.delay = delayAmt; sc.reverb = reverbAmt; sc.set = true;
}

// ------------------------------------------------------------------ voices
static float filterCoef (float cut) {
    const float fc = cut < 0 ? 18000.0f * std::pow (2.0f, 9.0f * cut) : 20.0f * std::pow (2.0f, 9.0f * cut);
    return std::tan (PI * clampf (fc, 20.0f, 16000.0f) / SR);
}
static void panGains (Voice& v, float p) { p = clampf (p, -1, 1); v.panL = std::min (1.0f, 1 - p); v.panR = std::min (1.0f, 1 + p); }

void Engine::startVoice (Voice& v, int s, int pad, float semis, float vel, int8_t filterLock, int8_t panLock) {
    const Slot& sl = slots[s];
    v = Voice ();
    v.on = true; v.held = true; v.slot = (uint8_t) s; v.pad = (uint8_t) (pad & 0xff);
    v.gain = vel * sceneVal (2, s, sl.volume);
    v.env = 0; v.dec = 1;
    v.attInc = 1.0f / (SR * std::max (0.002f, sl.attack));
    v.decCoef = sl.decay > 0 ? std::exp (-6.9f / (sl.decay * SR)) : 1.0f;
    v.semis = semis; v.drone = sl.drone; v.filterLocked = filterLock != NOLOCK;
    v.pitchNow = sceneVal (0, s, sl.pitch) + semis;
    v.inc = v.incTarget = (sl.rate / float (SR)) * std::pow (2.0f, v.pitchNow / 12.0f);
    const float a = sl.trimA * sl.len, b = std::max (a + 1, sl.trimB * sl.len);
    v.pos = sl.reverse ? b - 1 : a;
    float f = sceneVal (1, s, sl.filter);
    if (filterLock != NOLOCK) f = filterLock / 100.0f;
    v.cut = v.baseCut = f;
    if (v.cut < -0.02f || v.cut > 0.02f) v.hp = filterCoef (v.cut);
    v.pan = v.basePan = panLock != NOLOCK ? panLock / 100.0f : sl.pan;
    panGains (v, v.pan);
    if (sl.stretch || sl.freeze || sl.drone) {
        v.grains = true;
        const bool cloud = sl.freeze || sl.drone;
        v.nG = cloud ? 4 : 2;
        v.gLen = cloud ? std::max (32.0f, std::min (b - a - 2, sl.grainSize * sl.rate)) : SR * 0.045f * (sl.rate / float (SR));
        v.head = cloud ? a + sl.grainPos * (b - a - 2) : v.pos; v.lastPos = sl.grainPos;
        if (cloud) v.headInc = 0;     // the cloud reads its speed live
        else {
            const float secs = (b - a) / float (sl.rate);
            int beats = sl.beats;
            if (beats == 0) {   // the power of two nearest to the length at this tempo
                const float est = secs * bpm / 60.0f;
                beats = 1; while (beats * 2 <= 64 && std::fabs (beats * 2 - est) < std::fabs (beats - est)) beats *= 2;
            }
            const float target = beats * 60.0f / bpm;
            v.headInc = (sl.reverse ? -1.0f : 1.0f) * (secs / target) * (sl.rate / float (SR));
        }
        for (int g = 0; g < 4; ++g) { v.gPos[g] = v.head; v.gPh[g] = g / (float) v.nG; }
    }
}

Voice* Engine::noteOn (int s, int pad, float semis, float vel, int8_t filterLock, int8_t panLock) {
    if (s < 0 || s >= NSLOT || slots[s].empty () || busy) return nullptr;
    // a drone that is sounding glides to the new note instead of starting again
    if (slots[s].drone)
        for (auto& v : voices) if (v.on && v.drone && v.slot == s && v.relRate == 0) { v.semis = semis; v.gain = vel * sceneVal (2, s, slots[s].volume); return &v; }
    // the same pad retriggers (like a Pocket Operator): its old voice fades out in 3 ms instead of being cut,
    // and the new note takes a free voice, else one that is fading, else the oldest
    for (auto& v : voices) if (v.on && v.pad == (uint8_t) (pad & 0xff) && v.slot == s) { v.relRate = std::max (v.relRate, 1.0f / (SR * 0.003f)); v.pad = 0xFF; }
    Voice* pick = nullptr;
    for (auto& v : voices) if (! v.on) { pick = &v; break; }
    if (! pick) for (auto& v : voices) if (v.relRate > 0 && (! pick || v.env < pick->env)) pick = &v;
    if (! pick) { pick = &voices[0]; for (auto& v : voices) if (v.age > pick->age) pick = &v; }
    if (pick->on) stealFade (*pick);
    // choke group: this sound cuts the others of its group (open / closed hi-hat)
    if (const int g = slots[s].choke)
        for (auto& v : voices) if (&v != pick && v.on && slots[v.slot].choke == g) v.relRate = 1.0f / (SR * 0.004f);
    if (slots[s].duckSource) duckEnv = 1;
    startVoice (*pick, s, pad, semis, vel, filterLock, panLock);
    return pick;
}

void Engine::droneStop (int s) {
    for (auto& v : voices) if (v.on && v.drone && (s < 0 || v.slot == s)) { v.drone = false; v.held = false; }
}

// a voice that must be reused at once: keep its last output and fade that out instead of a click
void Engine::stealFade (Voice& v) {
    const float last = v.lastOut;
    declick[0] += last * v.panL; declick[1] += last * v.panR;
}

void Engine::noteOff (int pad) {
    for (auto& v : voices) if (v.on && ! v.drone && v.pad == (uint8_t) (pad & 0xff)) v.held = false;
}

void Engine::allOff () { for (auto& v : voices) v.on = false; }

// time-stretch: start the next grain where it best continues the grain that is playing (WSOLA),
// so a steady tone keeps its pitch and does not flutter
static float align (const int16_t* d, int a, int b, float head, float other, float dir) {
    const int W = 96, R = 240;
    const int o = (int) other;
    if (o - W < a || o + W >= b) return head;
    int best = (int) head; long long bestS = -(1LL << 62);
    for (int c = (int) head - R; c <= (int) head + R; c += 2) {
        if (c - W < a || c + W >= b) continue;
        long long s = 0;
        for (int k = 0; k < W; k += 3) { const int kk = dir > 0 ? k : -k; s += (int) d[c + kk] * (int) d[o + kk]; }
        if (s > bestS) { bestS = s; best = c; }
    }
    return (float) best + (other - o);
}

// control rate (every 32 samples): the LFO
void Engine::voiceControl (Voice& v) {
    const Slot& sl = slots[v.slot];
    // the knobs act on notes that are sounding: tune (a drone glides), filter
    const float p = sceneVal (0, v.slot, sl.pitch) + v.semis;
    if (p != v.pitchNow) { v.pitchNow = p; v.incTarget = (sl.rate / float (SR)) * std::pow (2.0f, p / 12.0f); }
    if (v.inc != v.incTarget) {
        if (v.drone && sl.glide > 0.001f) { v.inc += (v.incTarget - v.inc) * std::min (1.0f, 32.0f / (sl.glide * SR) * 2.3f); if (std::fabs (v.inc - v.incTarget) < 1e-6f) v.inc = v.incTarget; }
        else v.inc = v.incTarget;
    }
    if (! v.filterLocked && ! (sl.lfoDepth > 0 && sl.lfoTarget == 1)) {
        const float f = sceneVal (1, v.slot, sl.filter);
        if (f != v.baseCut) { v.cut = v.baseCut = f; v.hp = (f < -0.02f || f > 0.02f) ? filterCoef (f) : 0; }
    }
    if (sl.lfoDepth <= 0) return;
    v.lfoPh += sl.lfoRate * 32.0f / SR; if (v.lfoPh >= 1) v.lfoPh -= 1;
    const float w = std::sin (2 * PI * v.lfoPh), d = sl.lfoDepth;
    switch (sl.lfoTarget) {
        case 0: v.lfoMul = std::pow (2.0f, w * d * 2.0f / 12.0f); break;           // vibrato up to 2 semitones
        case 1: v.cut = clampf (v.baseCut + w * d, -1, 1); v.hp = (v.cut < -0.02f || v.cut > 0.02f) ? filterCoef (v.cut) : 0; break;
        case 2: v.lfoGain = 1 - d * (0.5f + 0.5f * w); break;                    // tremolo
        default: panGains (v, v.basePan + w * d); break;                          // auto-pan
    }
}

float Engine::voiceSample (Voice& v) {
    const Slot& sl = slots[v.slot];
    const int16_t* d = mem + blocks[sl.block].off + sl.start;
    const float a = sl.trimA * sl.len, b = std::max (a + 2, sl.trimB * sl.len);
    auto at = [&] (float p) -> float {
        if (p < a) p = a; if (p > b - 2) p = b - 2;
        const int i = (int) p; const float fr = p - i;
        return (d[i] + (d[i + 1] - d[i]) * fr) * (1.0f / 32768.0f);
    };
    float x;
    const float dir = sl.reverse ? -1.0f : 1.0f;
    const float inc = v.inc * transpose * v.lfoMul;
    if (v.grains) {
        x = 0;
        const bool cloud = sl.freeze || sl.drone;
        const float span = b - a - 2;
        if (cloud) {
            // the GRAIN page works live: POS jumps the read head, SPEED moves it (0 = frozen), SIZE and SPREAD shape new grains
            if (sl.grainPos != v.lastPos) { v.head = a + sl.grainPos * span; v.lastPos = sl.grainPos; }
            v.head += sl.grainSpeed * dir * (sl.rate / float (SR));
            if (v.head >= b - 2) v.head -= span; if (v.head < a) v.head += span;
        }
        const float norm = v.nG == 4 ? 0.5f : 1.0f;     // 4 overlapping windows add up to about 2
        for (int g = 0; g < v.nG; ++g) {
            // smoothstep window: two grains half a period apart always sum to 1 (and no sin per sample)
            const float u = v.gPh[g] < 0.5f ? 2 * v.gPh[g] : 2 - 2 * v.gPh[g];
            x += u * u * (3 - 2 * u) * at (v.gPos[g]) * norm;
            v.gPos[g] += dir * inc;
            if (v.gPos[g] >= b - 2) v.gPos[g] -= span; if (v.gPos[g] < a) v.gPos[g] += span;
            v.gPh[g] += 1.0f / v.gLen;
            if (v.gPh[g] >= 1) {
                v.gPh[g] -= 1;
                if (cloud) {
                    if (g == 0) v.gLen = std::max (32.0f, std::min (span, sl.grainSize * sl.rate));
                    const float spray = ((rng () & 1023) / 1023.0f - 0.5f) * (sl.grainSpread * span + v.gLen * 0.25f);
                    float p = v.head + spray; while (p >= b - 2) p -= span; while (p < a) p += span;
                    // start where it continues the grain that is loudest now, so a note keeps a clean pitch
                    v.gPos[g] = align (d, (int) a, (int) b, p, v.gPos[(g + v.nG / 2) % v.nG], dir);
                } else v.gPos[g] = align (d, (int) a, (int) b, v.head, v.gPos[1 - g], dir);
            }
        }
        if (! cloud) {
            v.head += v.headInc;
            const bool out = sl.reverse ? v.head < a : v.head >= b - 2;
            if (out) {
                if (sl.loopHold && v.held) v.head = sl.reverse ? b - 2 : a;
                else if (v.relRate == 0) v.relRate = 1.0f / (SR * 0.05f);
            }
        }
    } else {
        x = at (v.pos);
        if (! (sl.loopHold && v.held)) {       // the last 2 ms of a one-shot fade out (no click when a sample ends loud)
            const float left = (sl.reverse ? v.pos - a : (b - 2) - v.pos) / std::max (1e-4f, inc);
            if (left < 96) x *= std::max (0.0f, left) / 96.0f;
        }
        v.pos += dir * inc;
        const bool out = sl.reverse ? v.pos < a : v.pos >= b - 2;
        if (out) {
            if (sl.loopHold && v.held) v.pos = sl.reverse ? b - 2 : a;
            else { v.on = false; return 0; }
        }
    }
    // vintage samplers: sample-and-hold at their rate and bit depth (no anti-aliasing, on purpose)
    if (sl.vintage) {
        static const float RATE[4] = {0, 26040, 39375, 22050}, STEPS[4] = {0, 2048, 2048, 128};
        const int m = std::min<int> (3, sl.vintage);
        v.holdPh += RATE[m] / SR;
        if (v.holdPh >= 1) { v.holdPh -= 1; v.hold = std::round (x * STEPS[m]) / STEPS[m]; }
        x = v.hold;
    }
    // envelope: attack, then decay (if set); release when a held sound is let go (loop / freeze),
    // a grain voice ran out or a choke cut it
    if (v.relRate == 0 && ! v.held && (sl.loopHold || sl.freeze || sl.drone)) v.relRate = 1.0f / (SR * 0.05f);
    if (v.relRate > 0) {
        v.env -= v.relRate;
        if (v.env <= 0) { v.on = false; return 0; }
    } else if (v.env < 1) v.env = std::min (1.0f, v.env + v.attInc);
    else if (v.decCoef < 1) { v.dec *= v.decCoef; if (v.dec < 0.001f) { v.on = false; return 0; } }
    return x * v.env * v.dec;
}

// ------------------------------------------------------------------ recording
void Engine::recStart (int s, bool resample) {
    if (s < 0 || s >= NSLOT || recSlot >= 0 || busy) return;
    clearSlot (s);
    const uint32_t room = memSize - memUsed;
    if (room < SR / 10) return;
    recBlock = newBlock (room);
    if (recBlock < 0) return;
    recResample = resample; recLen = 0; recMax = room; recAcc = 0; recPhase = 0;
    recArmed = threshold && ! resample;      // wait for the sound to start
    recSlot = s;
}

void Engine::recStop () {
    if (recSlot < 0) return;
    Block& b = blocks[recBlock];
    int16_t* d = mem + b.off;
    // cut the silence before the sound (this block is the last one in the pool)
    if (autoTrim && ! recResample && recLen > 0) {
        uint32_t first = 0; while (first < recLen && std::abs ((int) d[first]) < 650) ++first;
        const uint32_t pre = recRate () / 200;
        const uint32_t cut = first > pre ? first - pre : 0;
        if (cut > 0 && cut < recLen) { std::memmove (d, d + cut, (recLen - cut) * sizeof (int16_t)); recLen -= cut; }
        else if (first >= recLen) recLen = 0;
    }
    memUsed = b.off + recLen; b.len = recLen;
    if (recLen < 64) { b.refs = 0; b.len = 0; memUsed = b.off; recSlot = -1; recBlock = -1; return; }
    Slot& sl = slots[recSlot]; sl = Slot ();
    sl.block = (int16_t) recBlock; sl.start = 0; sl.len = recLen; sl.rate = recRate ();
    recSlot = -1; recBlock = -1;
}

// ------------------------------------------------------------------ sequencer
void Engine::play (bool on) {
    if (on == running) return;
    if (on) {
        step = -1; nextStep = 0; stepClock = 1; chainPos = 0; loopPos = 0; loops = 0; songRow = 0; songRep = 0;
        pat = songMode && songLen > 0 ? song[0].pat : chain[0];
        for (auto& p : polyPos) p = 0;
    } else {
        step = -1; seqFx = 0;
    }
    running = on;
}

Trig* Engine::trig (int p, int st, int s) {
    if (p < 0 || p >= NPAT || st < 0 || st >= NSTEP) return nullptr;
    Step& S = pats[p].s[st];
    for (int i = 0; i < S.n; ++i) if (S.t[i].slot == s) return &S.t[i];
    return nullptr;
}
bool Engine::hasTrig (int p, int st, int s) const { return const_cast<Engine*> (this)->trig (p, st, s) != nullptr; }

void Engine::toggleStep (int st, int s, int8_t pitch, uint8_t vel) {
    if (st < 0 || st >= NSTEP) return;
    Step& S = pats[pat].s[st];
    for (int i = 0; i < S.n; ++i)
        if (S.t[i].slot == s) { for (int j = i; j < S.n - 1; ++j) S.t[j] = S.t[j + 1]; --S.n; S.t[S.n] = Trig (); return; }
    if (S.n >= NTRIG) return;
    Trig t; t.slot = (uint8_t) s; t.pitch = pitch; t.vel = vel;
    S.t[S.n++] = t;
}

int Engine::polyIndex (int p, int s) const {
    const Pattern& P = pats[p];
    for (int i = 0; i < P.nPoly; ++i) if (P.polySlot[i] == s && P.polyLen[i] > 0) return i;
    return -1;
}
int Engine::slotLen (int p, int s) const {
    const int i = polyIndex (p, s);
    return i >= 0 ? pats[p].polyLen[i] : std::max (1, (int) pats[p].len);
}
void Engine::setSlotLen (int p, int s, int len) {
    Pattern& P = pats[p];
    const int i = polyIndex (p, s);
    if (len <= 0 || len == P.len || len > NSTEP) {          // back to the pattern's length
        if (i >= 0) { for (int j = i; j < P.nPoly - 1; ++j) { P.polySlot[j] = P.polySlot[j + 1]; P.polyLen[j] = P.polyLen[j + 1]; } --P.nPoly; }
        return;
    }
    if (i >= 0) { P.polyLen[i] = (uint8_t) len; return; }
    if (P.nPoly >= NPOLY) return;
    P.polySlot[P.nPoly] = (uint8_t) s; P.polyLen[P.nPoly] = (uint8_t) len; ++P.nPoly;
}

void Engine::liveRecord (int s, int8_t pitch, uint8_t vel) {
    if (! running) return;
    const double frac = stepLenNow > 0 ? 1.0 - stepClock / stepLenNow : 0;
    int st;
    const int pi = polyIndex (pat, s);
    if (pi >= 0) {
        const int L = pats[pat].polyLen[pi], cur = (polyPos[pi] - 1 + L) % L;
        st = frac < 0.5 ? cur : (cur + 1) % L;
    } else {
        const int L = std::max (1, (int) pats[pat].len);
        st = frac < 0.5 ? std::max (step, 0) : (step + 1) % L;
    }
    Trig* t = trig (pat, st, s);
    if (t) { t->pitch = pitch; t->vel = vel; return; }
    Step& S = pats[pat].s[st];
    if (S.n < NTRIG) { Trig n; n.slot = (uint8_t) s; n.pitch = pitch; n.vel = vel; S.t[S.n++] = n; }
}

void Engine::euclid (int s, int hits, int rotate) {
    const int L = slotLen (pat, s);
    hits = std::max (0, std::min (L, hits));
    for (int st = 0; st < L; ++st) {
        const int i = ((st - rotate) % L + L) % L;
        const bool on = hits > 0 && (i * hits) % L < hits;
        const bool has = hasTrig (pat, st, s);
        if (on != has) toggleStep (st, s);
    }
}

void Engine::chaos (float amt) {
    Pattern& P = pats[pat];
    const int L = std::max (1, (int) P.len);
    uint8_t used[NSLOT]; int nu = 0;
    for (int st = 0; st < L; ++st) { const Step& S = P.s[st]; for (int i = 0; i < S.n; ++i) {
        bool seen = false; for (int k = 0; k < nu; ++k) seen |= used[k] == S.t[i].slot;
        if (! seen && nu < NSLOT) used[nu++] = S.t[i].slot;
    } }
    if (nu == 0) return;
    auto chance = [&] (float p) { return (rng () % 10000) < p * 10000; };
    for (int st = 0; st < L; ++st) {
        Step& S = P.s[st];
        for (int i = 0; i < S.n; ++i) {
            if (chance (amt * 0.25f)) {               // move to a neighbour step
                Trig t = S.t[i];
                const int to = (st + ((rng () & 1) ? 1 : L - 1)) % L;
                for (int j = i; j < S.n - 1; ++j) S.t[j] = S.t[j + 1];
                --S.n; S.t[S.n] = Trig (); --i;
                Step& D = P.s[to];
                bool dup = false; for (int k = 0; k < D.n; ++k) dup |= D.t[k].slot == t.slot;
                if (! dup && D.n < NTRIG) D.t[D.n++] = t;
                continue;
            }
            if (chance (amt * 0.15f)) S.t[i].pitch = (int8_t) ((int) (rng () % 25) - 12);
            if (chance (amt * 0.15f)) S.t[i].vel = (uint8_t) (50 + rng () % 78);
        }
        if (chance (amt * 0.2f) && S.n < NTRIG) {     // a new hit of a sound the pattern uses
            const uint8_t s = used[rng () % nu];
            bool dup = false; for (int k = 0; k < S.n; ++k) dup |= S.t[k].slot == s;
            if (! dup) { Trig t; t.slot = s; S.t[S.n++] = t; }
        }
        if (chance (amt * 0.1f) && S.n > 0) { S.t[rng () % S.n] = S.t[S.n - 1]; --S.n; S.t[S.n] = Trig (); }
    }
}

void Engine::clearPattern (int p) { if (p >= 0 && p < NPAT) pats[p] = Pattern (); }

void Engine::setChain (const uint8_t* p, int n) {
    if (n <= 0) return;
    n = std::min (n, NCHAIN);
    for (int i = 0; i < n; ++i) chain[i] = (uint8_t) (p[i] % NPAT);
    chainLen = n; chainPos = 0;
    if (! running) pat = chain[0];
}

void Engine::nudge (int p, int s, int dir) {
    Pattern& P = pats[p];
    if (s < 0) {                       // every sound (and the step effects) one step earlier / later
        const int L = std::max (1, (int) P.len);
        Step tmp[NSTEP];
        for (int i = 0; i < L; ++i) tmp[((i + dir) % L + L) % L] = P.s[i];
        for (int i = 0; i < L; ++i) P.s[i] = tmp[i];
        return;
    }
    const int L = slotLen (p, s);
    Trig moved[NSTEP]; bool has[NSTEP] = {false};
    for (int i = 0; i < L; ++i) { Trig* t = trig (p, i, s); if (t) { moved[i] = *t; has[i] = true; toggleStep (i, s); } }
    const int keep = pat; pat = p;
    for (int i = 0; i < L; ++i) if (has[i]) {
        const int to = ((i + dir) % L + L) % L;
        Step& S = P.s[to];
        if (S.n < NTRIG) S.t[S.n++] = moved[i];
    }
    pat = keep;
}

// undo: the pattern and the sound parameters as they were (undo again = redo)
void Engine::snapshot () {
    undoPat = pats[pat]; undoWhich = pat;
    for (int i = 0; i < NSLOT; ++i) undoSlots[i] = slots[i];
    undoValid = true;
}
void Engine::undo () {
    if (! undoValid || undoWhich < 0) return;
    std::swap (pats[undoWhich], undoPat);
    for (int i = 0; i < NSLOT; ++i) {
        Slot& a = slots[i]; Slot& b = undoSlots[i];
        // only parameters: the sound itself must still be the same sample
        if (a.block == b.block && a.start == b.start && a.len == b.len) std::swap (a, b);
        else b = a;
    }
}

bool Engine::condOk (const Trig& t) {
    switch (t.cond) {
        case C_ALWAYS: return true;
        case C_1_2: case C_2_2: return (int) (loops % 2) == t.cond - C_1_2;
        case C_1_3: case C_2_3: case C_3_3: return (int) (loops % 3) == t.cond - C_1_3;
        case C_1_4: case C_2_4: case C_3_4: case C_4_4: return (int) (loops % 4) == t.cond - C_1_4;
        case C_FILL: return fill;
        case C_NOTFILL: return ! fill;
        case C_FIRST: return loops == 0;
        case C_NOTFIRST: return loops > 0;
        case C_P25: return rng () % 100 < 25;
        case C_P50: return rng () % 100 < 50;
        case C_P75: return rng () % 100 < 75;
        default: return true;
    }
}

void Engine::playStep (int st, int only) {
    const Step& S = pats[pat].s[st];
    for (int i = 0; i < S.n; ++i) {
        const Trig& t = S.t[i];
        if (only >= 0 ? t.slot != only : polyIndex (pat, t.slot) >= 0) continue;
        if (slots[t.slot].mute || ! condOk (t)) continue;
        if (Voice* v = noteOn (t.slot, t.slot, t.pitch == NOLOCK ? 0 : t.pitch, t.vel / 100.0f, t.filter, t.pan)) v->held = v->drone;
    }
}

void Engine::advancePattern () {
    const int old = pat;
    if (songMode && songLen > 0) {
        if (++songRep >= std::max<int> (1, song[songRow].repeats)) { songRep = 0; songRow = (songRow + 1) % songLen; }
        pat = song[songRow].pat % NPAT;
    } else {
        chainPos = (chainPos + 1) % std::max (1, chainLen);
        pat = chain[chainPos] % NPAT;
    }
    if (pat != old) { loops = 0; for (auto& p : polyPos) p = 0; }
    else ++loops;
}

void Engine::seqTick () {
    const int fxNow = fx ? fx : seqFx;
    const Pattern& P = pats[pat];
    const int len = std::max (1, std::min (NSTEP, (int) P.len));
    // effect 14 (restart) loops the playing beat, 16 = half tempo
    int st = nextStep;
    if (fxNow == 14 && st % 4 == 0 && step >= 0) st = (step / 4) * 4;
    if (st >= len) st = 0;
    step = st;
    playStep (step, -1);
    for (int i = 0; i < P.nPoly; ++i) {             // sounds with their own length (polymeter)
        const int L = std::max<int> (1, P.polyLen[i]);
        polyPos[i] %= L;
        playStep (polyPos[i], P.polySlot[i]);
        polyPos[i] = (polyPos[i] + 1) % L;
    }
    seqFx = P.s[step].fx;
    if (step % 4 == 0 && metronome) { clickEnv = 1; clickHz = step == 0 ? 1600.0f : 1000.0f; clickPh = 0; }
    nextStep = step + 1;
    if (nextStep >= len) { nextStep = 0; if (fxNow != 14) advancePattern (); }
    double l = stepSamples ();
    l *= (step % 2 == 0) ? (1.0 + swing) : (1.0 - swing);
    if (fxNow == 16) l *= 2;
    stepLenNow = l;
    stepClock += l;
}

// ------------------------------------------------------------------ chop
int Engine::chop (int s) {
    if (s < 0 || s >= NSLOT || slots[s].empty ()) return -1;
    int bank = -1;
    for (int b = 0; b < NBANK && bank < 0; ++b) {
        bool free = true; for (int i = 0; i < 16; ++i) free &= slots[b * 16 + i].empty ();
        if (free) bank = b;
    }
    if (bank < 0) return -1;
    const Slot src = slots[s];
    const int16_t* d = mem + blocks[src.block].off + src.start;
    const uint32_t len = src.len;
    // onsets: a 256-sample frame whose energy jumps above the recent average
    const int F = 256; uint32_t cuts[17]; int nc = 0; cuts[nc++] = 0;
    float avg = 0; uint32_t last = 0;
    for (uint32_t f = 0; f + F <= len && nc < 16; f += F) {
        float e = 0; for (int i = 0; i < F; ++i) { const float x = d[f + i] / 32768.0f; e += x * x; }
        e /= F;
        if (f > 0 && e > 0.0004f && e > avg * 4.0f && f - last > (uint32_t) (src.rate * 0.06f)) { cuts[nc++] = f; last = f; }
        avg = avg * 0.8f + e * 0.2f;
    }
    if (nc < 4) { nc = 0; for (int i = 0; i < 16; ++i) cuts[nc++] = (uint32_t) ((uint64_t) len * i / 16); }
    cuts[nc] = len;
    for (int i = 0; i < nc; ++i) {
        Slot& o = slots[bank * 16 + i]; o = Slot ();
        o.block = src.block; o.start = src.start + cuts[i]; o.len = cuts[i + 1] - cuts[i]; o.rate = src.rate;
        o.volume = src.volume;
        blocks[src.block].refs++;
    }
    return bank;
}

// ------------------------------------------------------------------ looper
void Engine::looperNext (int s) {
    if (loopState == LOOP_OFF) {
        if (recSlot >= 0) return;
        const uint32_t len = (uint32_t) (stepSamples () * 16);
        if (slots[s].empty () || slots[s].len != len || slots[s].rate != SR) {
            clearSlot (s);
            const int b = newBlock (len);
            if (b < 0) return;
            std::memset (mem + blocks[b].off, 0, len * sizeof (int16_t));
            Slot& sl = slots[s]; sl = Slot (); sl.block = (int16_t) b; sl.len = len; sl.rate = SR;
        }
        slots[s].saved = false;
        loopSlot = s; loopState = LOOP_REC; loopPos = 0;
        if (! running) play (true);
    } else if (loopState == LOOP_REC) loopState = LOOP_PLAY;
    else { loopState = LOOP_OFF; loopSlot = -1; }
}

// ------------------------------------------------------------------ arpeggiator (keys mode)
void Engine::arpPress (int pad) {
    for (int i = 0; i < arpN; ++i) if (arpHeld[i] == pad) return;
    if (arpN < 16) arpHeld[arpN++] = (uint8_t) pad;
}
void Engine::arpRelease (int pad) {
    for (int i = 0; i < arpN; ++i) if (arpHeld[i] == pad) { for (int j = i; j < arpN - 1; ++j) arpHeld[j] = arpHeld[j + 1]; --arpN; break; }
    if (arpN == 0) noteOff (ARP_PAD);
}
void Engine::arpStep () {
    const int n = arpN;
    if (n <= 0) return;
    uint8_t order[16]; for (int i = 0; i < n; ++i) order[i] = arpHeld[i];
    if (arpMode != 5) std::sort (order, order + n);
    switch (arpMode) {
        case 1: arpIdx = (arpIdx + 1) % n; break;
        case 2: arpIdx = (arpIdx - 1 + n) % n; break;
        case 3:
            if (n == 1) arpIdx = 0;
            else { arpIdx += arpDir; if (arpIdx >= n) { arpIdx = n - 2; arpDir = -1; } if (arpIdx < 0) { arpIdx = 1; arpDir = 1; } }
            break;
        case 4: arpIdx = (int) (rng () % n); break;
        default: arpIdx = (arpIdx + 1) % n; break;
    }
    arpIdx = std::max (0, std::min (n - 1, arpIdx));
    noteOn (arpSlot, ARP_PAD, (float) padSemis (order[arpIdx]), 1);
    lastArpPad = order[arpIdx];
}

// ------------------------------------------------------------------ export
uint32_t Engine::renderStart (bool wholeSong) {
    play (false); allOff (); fx = 0; seqFx = 0; fxActive = 0; fxEnv = 0;
    std::memset (dbuf, 0, sizeof dbuf); std::memset (hist, 0, sizeof hist); std::memset (comb, 0, sizeof comb); std::memset (ap, 0, sizeof ap);
    dlp = 0; for (auto& c : combLp) for (auto& v : c) v = 0; declick[0] = declick[1] = 0;
    double steps = 0;
    renderSongWas = songMode;
    if (wholeSong && songLen > 0) {
        songMode = true;
        for (int i = 0; i < songLen; ++i) steps += (double) std::max<int> (1, pats[song[i].pat % NPAT].len) * std::max<int> (1, song[i].repeats);
    } else {
        songMode = false;
        for (int i = 0; i < chainLen; ++i) steps += std::max<int> (1, pats[chain[i] % NPAT].len);
    }
    renderMusic = (uint32_t) (steps * stepSamples ());
    renderFrames = renderMusic + SR;              // one second of tail (delay, reverb)
    play (true);
    return renderFrames;
}
void Engine::renderEnd () { play (false); allOff (); songMode = renderSongWas; }

// ------------------------------------------------------------------ effects
float Engine::histAt (int ch, float p) const {
    while (p < 0) p += HLEN;
    while (p >= HLEN) p -= HLEN;
    const int i = (int) p; const float fr = p - i;
    const int j = (i + 1) % HLEN;
    return (hist[ch][i] + (hist[ch][j] - hist[ch][i]) * fr) * (1.0f / 32768.0f);
}

void Engine::fxProcess (float& l, float& r) {
    const int want = fx ? fx : seqFx;
    hist[0][hpos] = toS16 (l); hist[1][hpos] = toS16 (r);
    if (want != fxActive) {
        fxActive = want; fxStart = hpos; fxT = 0; fxRead = (float) hpos;
        if (want == 10 || want == 11) { fic1[0] = fic1[1] = fic2[0] = fic2[1] = 0; }
    }
    // octave effects act on the voices; tempo effects act in the sequencer
    transpose = fxActive == 12 ? 2.0f : (fxActive == 13 ? 0.5f : 1.0f);
    const float in[2] = {l, r}; float y[2] = {l, r};
    const float st = stepSamples ();
    auto loopLen = [&] (float steps) { return std::min ((float) HLEN - 1, std::max (64.0f, steps * st)); };
    if (fxActive == 6) { const float rr = std::max (0.0f, 1.0f - fxT / (st * 4)); fxRead += rr; }
    if (fxActive == 8) {
        fxRead += std::sin (2 * PI * fxT / (st * 2)) * 1.6f;
        if (fxRead > fxStart) fxRead = (float) fxStart;
        if (fxStart - fxRead > HLEN - 2) fxRead = (float) (fxStart - (HLEN - 2));
    }
    const bool crushNow = fxActive == 9 && crushCount++ % 6 == 0;
    if ((fxActive == 10 || fxActive == 11) && (fxT & 31) == 0) {
        const float t = std::min (1.0f, fxT / (st * 8));
        const float fc = fxActive == 10 ? 12000.0f * std::pow (2.0f, -7.0f * t) : 60.0f * std::pow (2.0f, 7.0f * t);
        sweepG = std::tan (PI * std::min (fc, 18000.0f) / SR);
    }
    for (int c = 0; c < 2; ++c) {
        const float x = in[c];
        switch (fxActive) {
            case 1: case 2: case 3: case 4: {
                const float L = loopLen (fxActive == 1 ? 1 : fxActive == 2 ? 2 : fxActive == 3 ? 4 : 8);
                const float ph = std::fmod ((float) fxT, L);
                y[c] = histAt (c, fxStart - L + ph);
                if (ph > L - 128 && 2 * L + 130 < HLEN) {          // the last 128 samples blend into what came before the loop start
                    const float w = (ph - (L - 128)) / 128.0f;
                    y[c] = y[c] * (1 - w) + histAt (c, fxStart - 2 * L + ph) * w;
                }
                break;
            }
            case 5: {     // stutter: 1/32 loop, gated
                const float L = loopLen (0.5f); const float ph = std::fmod ((float) fxT, L);
                const float on = L * 0.6f, g = std::min (1.0f, std::min (ph, on - ph) / 48.0f);   // 1 ms ramps at both ends of the gate
                y[c] = ph < on ? histAt (c, fxStart - L + ph) * std::max (0.0f, g) : 0; break;
            }
            case 6: {     // tape stop over one beat
                const float rr = std::max (0.0f, 1.0f - fxT / (st * 4));
                y[c] = histAt (c, fxRead) * std::min (1.0f, rr * 4); break;
            }
            case 7: y[c] = histAt (c, fxStart - (float) std::min<uint32_t> (fxT, HLEN - 2)); break;     // reverse
            case 8: y[c] = histAt (c, fxRead); break;                                                  // scratch
            case 9: if (crushNow) crushHold[c] = std::round (x * 16) / 16; y[c] = crushHold[c]; break; // bitcrush: 5 bits, 1/6 rate
            case 10: case 11: {   // filter sweeps over two beats
                const float g = sweepG, k = 0.7f;
                const float a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2;
                const float v3 = x - fic2[c], v1 = a1 * fic1[c] + a2 * v3, v2 = fic2[c] + a2 * fic1[c] + a3 * v3;
                fic1[c] = 2 * v1 - fic1[c]; fic2[c] = 2 * v2 - fic2[c];
                y[c] = fxActive == 10 ? v2 : x - k * v1 - v2; break;
            }
            case 15: {    // freeze: two grains over the last 90 ms
                const float L = SR * 0.09f; float s = 0;
                for (int g = 0; g < 2; ++g) {
                    const float ph = std::fmod (fxT / L + g * 0.5f, 1.0f);
                    const float u = ph < 0.5f ? 2 * ph : 2 - 2 * ph;
                    s += u * u * (3 - 2 * u) * histAt (c, fxStart - L * 1.5f + ph * L);
                }
                y[c] = s; break;
            }
            default: break;
        }
    }
    const bool any = fxActive != 0 && fxActive != 12 && fxActive != 13 && fxActive != 14 && fxActive != 16;
    fxEnv = std::max (0.0f, std::min (1.0f, fxEnv + (any ? 1.0f : -1.0f) / 240.0f));
    hpos = (hpos + 1) % HLEN; ++fxT;
    l = in[0] + (y[0] - in[0]) * fxEnv;
    r = in[1] + (y[1] - in[1]) * fxEnv;
}

// ------------------------------------------------------------------ audio
void Engine::process (const float* in, float* out, int n) {
    inProcess = true;
    if (busy || hold) std::memset (out, 0, sizeof (float) * 2 * n);
    else processRaw (in, out, n);
    inProcess = false;
}

void Engine::processRaw (const float* in, float* out, int n) {
    const float rollEvery = SR * 60.0f / bpm / std::max (1.0f, rollDiv);
    const float arpEvery = SR * 60.0f / bpm / std::max (0.25f, arpDiv);
    // the crossfader moves the master sends too
    const float eDelay = scenes[0].set || scenes[1].set ? (scenes[0].set ? scenes[0].delay : delayAmt) * (1 - xfade) + (scenes[1].set ? scenes[1].delay : delayAmt) * xfade : delayAmt;
    const float eReverb = scenes[0].set || scenes[1].set ? (scenes[0].set ? scenes[0].reverb : reverbAmt) * (1 - xfade) + (scenes[1].set ? scenes[1].reverb : reverbAmt) * xfade : reverbAmt;
    const float compTh = std::pow (10.0f, (-4.0f - 22.0f * compAmt) / 20.0f), makeup = 1 + compAmt * 0.9f;
    for (int i = 0; i < n; ++i) {
        const float input = (in ? in[i] : 0) * inGain;
        // sequencer
        if (running) {
            stepClock -= 1;
            while (stepClock <= 0) seqTick ();
        }
        if (rollSlot >= 0) {
            rollClock -= 1;
            if (rollClock <= 0) { noteOn (rollSlot, rollPad, 0, 0.9f); rollClock += rollEvery; }
        } else rollClock = 0;
        if (arpMode > 0 && arpN > 0) {
            arpClock -= 1;
            if (arpClock <= 0) { arpStep (); arpClock += arpEvery; }
        } else arpClock = 0;
        // voices
        float dryL = 0, dryR = 0, dsend = 0, rsend = 0;
        duckEnv *= 0.99983f;
        const float duck = 1 - duckAmt * duckEnv;
        for (auto& v : voices) {
            if (! v.on) continue;
            ++v.age;
            const Slot& sl = slots[v.slot];
            if ((v.ctl++ & 31) == 0) voiceControl (v);
            float x = voiceSample (v);
            if (! v.on) continue;
            if (v.hp > 0) {
                const float g = v.hp, k = 1.2f - 1.08f * sl.reso;     // RESO: up to a singing peak
                const float a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2;
                const float v3 = x - v.bp, v1 = a1 * v.lp + a2 * v3, v2 = v.bp + a2 * v.lp + a3 * v3;
                v.lp = 2 * v1 - v.lp; v.bp = 2 * v2 - v.bp;
                x = v.cut < 0 ? v2 : x - k * v1 - v2;
            }
            if (sl.drive > 0) x = softclip (x * (1 + 6 * sl.drive)) / (1 + sl.drive);
            x *= v.gain * v.lfoGain;
            if (! sl.duckSource) x *= duck;
            v.lastOut = x;
            dryL += x * v.panL; dryR += x * v.panR;
            dsend += x * clampf (eDelay + sl.delay, 0, 1);
            rsend += x * clampf (eReverb + sl.reverb, 0, 1);
        }
        // looper (one bar, overdub from the input)
        if (loopState != LOOP_OFF && loopSlot >= 0 && ! slots[loopSlot].empty ()) {
            Slot& ls = slots[loopSlot];
            int16_t* d = mem + blocks[ls.block].off + ls.start;
            const uint32_t p = loopPos % ls.len;
            const float old = d[p] / 32768.0f;
            dryL += old * ls.volume; dryR += old * ls.volume;
            if (loopState == LOOP_REC) d[p] = toS16 (old + input);
            ++loopPos;
        }
        if (monitor) { dryL += input; dryR += input; }
        // voices that had to be reused at once fade out from where they were
        dryL += declick[0]; dryR += declick[1]; declick[0] *= 0.993f; declick[1] *= 0.993f;
        // punch-in effects on the dry mix
        fxProcess (dryL, dryR);
        // delay: 3/16, filtered feedback
        const int dl = std::min (DLEN - 1, (int) (stepSamples () * 3));
        int rp = dpos - dl; if (rp < 0) rp += DLEN;
        const float dout = dbuf[rp] / 32768.0f;
        dlp += (dout - dlp) * 0.35f;
        dbuf[dpos] = toS16 (dsend * 0.6f + dlp * 0.45f);
        dpos = (dpos + 1) % DLEN;
        // reverb (Freeverb): 8 parallel damped combs, then 4 all-pass filters, a slightly different set for each side
        float rv[2];
        {
            const float in = rsend * 0.1f, fb = 0.84f, d1 = 0.25f, d2 = 0.75f;   // a hot level inside: the int16 buffers keep the quiet tail
            for (int c = 0; c < 2; ++c) {
                float sum = 0;
                for (int k = 0; k < NC; ++k) {
                    int16_t& z = comb[c][k][combPos[c][k]];
                    const float o = z * (1.0f / 32768.0f);
                    combLp[c][k] = o * d2 + combLp[c][k] * d1;
                    z = toS16 (in + combLp[c][k] * fb);
                    if (++combPos[c][k] >= combLen[c][k]) combPos[c][k] = 0;
                    sum += o;
                }
                for (int k = 0; k < NAP; ++k) {
                    int16_t& z = ap[c][k][apPos[c][k]];
                    const float bo = z * (1.0f / 32768.0f);
                    z = toS16 (sum + bo * 0.5f);
                    sum = bo - sum;
                    if (++apPos[c][k] >= apLen[c][k]) apPos[c][k] = 0;
                }
                rv[c] = sum;
            }
        }
        float L = dryL + dlp * 0.8f + rv[0] * 0.48f;
        float R = dryR + dlp * 0.8f + rv[1] * 0.48f;
        // master compressor (feed-forward, 4:1)
        if (compAmt > 0) {
            const float pk = std::max (std::fabs (L), std::fabs (R));
            compEnv += (pk - compEnv) * (pk > compEnv ? 0.004f : 0.00025f);
            float g = 1;
            if (compEnv > compTh) g = std::pow (compTh / compEnv, 0.75f);
            gr = 1 - g;
            L *= g * makeup; R *= g * makeup;
        } else gr = 0;
        L *= master; R *= master;
        if (lofi) { L = std::round (L * 2048) / 2048; R = std::round (R * 2048) / 2048; }
        L = softclip (L); R = softclip (R);
        level = std::max (level * 0.9995f, std::fabs (L));
        // recording (mic / line, or the output = resampling); lo-fi stores every second sample
        if (recSlot >= 0) {
            const float src = recResample ? 0.5f * (L + R) : input;
            if (recArmed && std::fabs (src) > 0.02f) recArmed = false;
            if (! recArmed) {
                int16_t* d = mem + blocks[recBlock].off;
                if (lofi) {
                    recAcc += src;
                    if (++recPhase == 2) { recPhase = 0; if (recLen < recMax) d[recLen++] = toS16 (std::round (recAcc * 0.5f * 2048) / 2048); recAcc = 0; }
                } else if (recLen < recMax) d[recLen++] = toS16 (src);
                if (recLen >= recMax) recStop ();
            }
        }
        // the metronome is heard but not recorded
        if (clickEnv > 0.001f) {
            clickPh += clickHz / SR; if (clickPh >= 1) clickPh -= 1;
            const float c = (clickPh < 0.5f ? 1.0f : -1.0f) * clickEnv * 0.2f;
            clickEnv *= 0.9985f;
            L += c; R += c;
        }
        out[2 * i] = L; out[2 * i + 1] = R;
        ++frames;
    }
}

} // namespace pocket

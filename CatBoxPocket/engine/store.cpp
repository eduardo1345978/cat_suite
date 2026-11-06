// Cat Box Pocket: the flash disk. A small FAT16 volume (so the computer sees it over USB) with
// PROJECT.CBP (settings, sound parameters, patterns) and one WAV per sound: A01.WAV .. D16.WAV.
#include "pocket.h"
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cmath>

namespace pocket {
namespace store {

static inline uint16_t rd16 (const uint8_t* p) { return (uint16_t) (p[0] | p[1] << 8); }
static inline uint32_t rd32 (const uint8_t* p) { return (uint32_t) (p[0] | p[1] << 8 | p[2] << 16 | (uint32_t) p[3] << 24); }
static inline void wr16 (uint8_t* p, uint32_t v) { p[0] = (uint8_t) v; p[1] = (uint8_t) (v >> 8); }
static inline void wr32 (uint8_t* p, uint32_t v) { p[0] = (uint8_t) v; p[1] = (uint8_t) (v >> 8); p[2] = (uint8_t) (v >> 16); p[3] = (uint8_t) (v >> 24); }

static uint8_t sec[512];          // one sector buffer (data)
static uint8_t fatSec[512];       // the cached FAT sector
static uint32_t fatNum = 0xffffffff; static bool fatDirty = false;

struct Vol { uint32_t total, fatStart, fatSecs, nFats, rootStart, rootSecs, dataStart, spc, clusters; };

static bool vol (BlockDev& d, Vol& v) {
    uint8_t b[512];
    if (! d.read (0, b) || b[510] != 0x55 || b[511] != 0xAA || rd16 (b + 11) != 512) return false;
    if (std::memcmp (b + 54, "FAT16", 5) != 0) return false;
    v.spc = b[13]; const uint32_t res = rd16 (b + 14); v.nFats = b[16];
    const uint32_t rootEnt = rd16 (b + 17);
    v.total = rd16 (b + 19) ? rd16 (b + 19) : rd32 (b + 32);
    v.fatSecs = rd16 (b + 22);
    if (v.spc == 0 || v.nFats == 0 || v.fatSecs == 0) return false;
    v.fatStart = res; v.rootStart = res + v.nFats * v.fatSecs; v.rootSecs = (rootEnt * 32 + 511) / 512;
    v.dataStart = v.rootStart + v.rootSecs;
    v.clusters = (v.total - v.dataStart) / v.spc;
    fatNum = 0xffffffff; fatDirty = false;
    return true;
}

static void fatFlush (BlockDev& d, const Vol& v) {
    if (! fatDirty) return;
    for (uint32_t f = 0; f < v.nFats; ++f) d.write (v.fatStart + f * v.fatSecs + fatNum, fatSec);
    fatDirty = false;
}
static uint16_t fatGet (BlockDev& d, const Vol& v, uint32_t c) {
    const uint32_t s = c * 2 / 512;
    if (s != fatNum) { fatFlush (d, v); d.read (v.fatStart + s, fatSec); fatNum = s; }
    return rd16 (fatSec + (c * 2) % 512);
}
static void fatSet (BlockDev& d, const Vol& v, uint32_t c, uint16_t val) {
    fatGet (d, v, c);
    wr16 (fatSec + (c * 2) % 512, val); fatDirty = true;
}
static uint32_t clusterSector (const Vol& v, uint32_t c) { return v.dataStart + (c - 2) * v.spc; }

static void name83 (const char* n, uint8_t out[11]) {
    std::memset (out, ' ', 11);
    int i = 0;
    for (; *n && *n != '.' && i < 8; ++n) out[i++] = (uint8_t) ((*n >= 'a' && *n <= 'z') ? *n - 32 : *n);
    while (*n && *n != '.') ++n;
    if (*n == '.') { ++n; for (int j = 0; *n && j < 3; ++n) out[8 + j++] = (uint8_t) ((*n >= 'a' && *n <= 'z') ? *n - 32 : *n); }
}
static void name83to (const uint8_t* e, char* out) {
    int k = 0;
    for (int i = 0; i < 8 && e[i] != ' '; ++i) out[k++] = (char) e[i];
    if (e[8] != ' ') { out[k++] = '.'; for (int i = 8; i < 11 && e[i] != ' '; ++i) out[k++] = (char) e[i]; }
    out[k] = 0;
}

bool format (BlockDev& d, const char* label) {
    const uint32_t total = d.sectors (), spc = 4, res = 1, nf = 2, rootEnt = 512, rootSecs = rootEnt * 32 / 512;
    uint32_t fs = 1;
    for (int it = 0; it < 8; ++it) { const uint32_t cl = (total - res - nf * fs - rootSecs) / spc; fs = ((cl + 2) * 2 + 511) / 512; }
    const uint32_t clusters = (total - res - nf * fs - rootSecs) / spc;
    if (clusters < 4085 || clusters > 65524) return false;
    uint8_t b[512]; std::memset (b, 0, 512);
    b[0] = 0xEB; b[1] = 0x3C; b[2] = 0x90; std::memcpy (b + 3, "CATPOCKT", 8);
    wr16 (b + 11, 512); b[13] = (uint8_t) spc; wr16 (b + 14, res); b[16] = (uint8_t) nf; wr16 (b + 17, rootEnt);
    if (total < 65536) wr16 (b + 19, total); else wr32 (b + 32, total);
    b[21] = 0xF8; wr16 (b + 22, fs); wr16 (b + 24, 32); wr16 (b + 26, 64);
    b[36] = 0x80; b[38] = 0x29; wr32 (b + 39, 0x20261106);
    uint8_t lab[11]; name83 (label, lab); std::memcpy (b + 43, lab, 11);
    std::memcpy (b + 54, "FAT16   ", 8); b[510] = 0x55; b[511] = 0xAA;
    if (! d.write (0, b)) return false;
    std::memset (b, 0, 512);
    for (uint32_t f = 0; f < nf; ++f) for (uint32_t s = 0; s < fs; ++s) {
        uint8_t z[512]; std::memset (z, 0, 512);
        if (s == 0) { wr16 (z, 0xFFF8); wr16 (z + 2, 0xFFFF); }
        d.write (res + f * fs + s, z);
    }
    for (uint32_t s = 0; s < rootSecs; ++s) {
        uint8_t z[512]; std::memset (z, 0, 512);
        if (s == 0) { std::memcpy (z, lab, 11); z[11] = 0x08; }
        d.write (res + nf * fs + s, z);
    }
    d.flush ();
    fatNum = 0xffffffff; fatDirty = false;
    return true;
}

bool mounted (BlockDev& d) { Vol v; return vol (d, v); }

// find a root entry: returns sector + index, or -1
static bool findEntry (BlockDev& d, const Vol& v, const uint8_t n[11], uint32_t& sOut, int& iOut, uint8_t* entry) {
    for (uint32_t s = 0; s < v.rootSecs; ++s) {
        d.read (v.rootStart + s, sec);
        for (int i = 0; i < 16; ++i) {
            const uint8_t* e = sec + i * 32;
            if (e[0] == 0) return false;
            if (e[0] == 0xE5 || (e[11] & 0x08) || e[11] == 0x0F) continue;
            if (std::memcmp (e, n, 11) == 0) { sOut = s; iOut = i; if (entry) std::memcpy (entry, e, 32); return true; }
        }
    }
    return false;
}

static void freeChain (BlockDev& d, const Vol& v, uint32_t c) {
    while (c >= 2 && c < 0xFFF8 && c < v.clusters + 2) { const uint32_t n = fatGet (d, v, c); fatSet (d, v, c, 0); c = n; }
}

bool remove (BlockDev& d, const char* name) {
    Vol v; if (! vol (d, v)) return false;
    uint8_t n[11]; name83 (name, n);
    uint32_t s; int i; uint8_t e[32];
    if (! findEntry (d, v, n, s, i, e)) return false;
    freeChain (d, v, rd16 (e + 26));
    fatFlush (d, v);
    d.read (v.rootStart + s, sec); sec[i * 32] = 0xE5; d.write (v.rootStart + s, sec);
    d.flush ();
    return true;
}

int list (BlockDev& d, char names[][13], uint32_t* sizes, int max) {
    Vol v; if (! vol (d, v)) return 0;
    int n = 0;
    for (uint32_t s = 0; s < v.rootSecs && n < max; ++s) {
        d.read (v.rootStart + s, sec);
        for (int i = 0; i < 16 && n < max; ++i) {
            const uint8_t* e = sec + i * 32;
            if (e[0] == 0) return n;
            if (e[0] == 0xE5 || (e[11] & 0x18) || e[11] == 0x0F) continue;
            name83to (e, names[n]); if (sizes) sizes[n] = rd32 (e + 28); ++n;
        }
    }
    return n;
}

typedef void (*FillFn) (void* ctx, uint32_t off, uint8_t* buf, uint32_t n);
typedef bool (*TakeFn) (void* ctx, uint32_t off, const uint8_t* data, uint32_t n);

static bool writeStream (BlockDev& d, const char* name, uint32_t len, FillFn fill, void* ctx) {
    remove (d, name);
    Vol v; if (! vol (d, v)) return false;
    const uint32_t csize = v.spc * 512, need = (len + csize - 1) / csize;
    // allocate a chain
    uint32_t first = 0, prev = 0, got = 0;
    for (uint32_t c = 2; c < v.clusters + 2 && got < need; ++c)
        if (fatGet (d, v, c) == 0) {
            if (prev) fatSet (d, v, prev, (uint16_t) c); else first = c;
            fatSet (d, v, c, 0xFFFF); prev = c; ++got;
        }
    if (got < need) { if (first) freeChain (d, v, first); fatFlush (d, v); return false; }
    // write the data along the chain
    uint32_t c = first, off = 0;
    while (off < len && c >= 2 && c < 0xFFF8) {
        for (uint32_t k = 0; k < v.spc && off < len; ++k) {
            const uint32_t n = std::min<uint32_t> (512, len - off);
            std::memset (sec, 0, 512); fill (ctx, off, sec, n);
            d.write (clusterSector (v, c) + k, sec); off += n;
        }
        c = fatGet (d, v, c);
    }
    fatFlush (d, v);
    // the directory entry
    uint8_t n[11]; name83 (name, n);
    for (uint32_t s = 0; s < v.rootSecs; ++s) {
        d.read (v.rootStart + s, sec);
        for (int i = 0; i < 16; ++i) {
            uint8_t* e = sec + i * 32;
            if (e[0] != 0 && e[0] != 0xE5) continue;
            const bool wasEnd = e[0] == 0;
            std::memset (e, 0, 32); std::memcpy (e, n, 11); e[11] = 0x20;
            wr16 (e + 14, 0x6000); wr16 (e + 16, (uint16_t) (((2026 - 1980) << 9) | (11 << 5) | 6));
            wr16 (e + 22, 0x6000); wr16 (e + 24, (uint16_t) (((2026 - 1980) << 9) | (11 << 5) | 6));
            wr16 (e + 26, first); wr32 (e + 28, len);
            (void) wasEnd;
            d.write (v.rootStart + s, sec); d.flush ();
            return true;
        }
    }
    freeChain (d, v, first); fatFlush (d, v);
    return false;
}

static bool readStream (BlockDev& d, const char* name, TakeFn take, void* ctx, uint32_t* sizeOut = nullptr) {
    Vol v; if (! vol (d, v)) return false;
    uint8_t n[11]; name83 (name, n);
    uint32_t s; int i; uint8_t e[32];
    if (! findEntry (d, v, n, s, i, e)) return false;
    const uint32_t len = rd32 (e + 28);
    if (sizeOut) *sizeOut = len;
    uint32_t c = rd16 (e + 26), off = 0;
    while (off < len && c >= 2 && c < 0xFFF8 && c < v.clusters + 2) {
        for (uint32_t k = 0; k < v.spc && off < len; ++k) {
            const uint32_t m = std::min<uint32_t> (512, len - off);
            d.read (clusterSector (v, c) + k, sec);
            if (! take (ctx, off, sec, m)) return true;
            off += m;
        }
        c = fatGet (d, v, c);
    }
    return true;
}

struct BufCtx { const uint8_t* src; uint8_t* dst; uint32_t max, got; };
bool writeFile (BlockDev& d, const char* name, const uint8_t* data, uint32_t len) {
    BufCtx c { data, nullptr, 0, 0 };
    return writeStream (d, name, len, [] (void* x, uint32_t off, uint8_t* b, uint32_t n) { std::memcpy (b, ((BufCtx*) x)->src + off, n); }, &c);
}
bool readFile (BlockDev& d, const char* name, uint8_t* buf, uint32_t max, uint32_t* got) {
    BufCtx c { nullptr, buf, max, 0 };
    const bool ok = readStream (d, name, [] (void* x, uint32_t off, const uint8_t* data, uint32_t n) {
        BufCtx& b = *(BufCtx*) x;
        if (off >= b.max) return false;
        const uint32_t m = std::min (n, b.max - off); std::memcpy (b.dst + off, data, m); b.got = off + m; return true; }, &c);
    if (got) *got = c.got;
    return ok;
}

// ------------------------------------------------------------------ WAV (streaming parser: any PCM / float -> mono int16)
struct WavIn {
    enum { HDR, CHUNK, FMT, SKIP, DATA, DONE } st = HDR;
    uint8_t hb[48]; uint32_t hn = 0, need = 12, chunkLeft = 0;
    uint16_t fmtTag = 1, ch = 1, bits = 16; uint32_t rate = SR;
    uint8_t carry[16]; uint32_t cn = 0;
    int16_t* out = nullptr; uint32_t max = 0, frames = 0; bool ok = false;

    void sample (const uint8_t* f) {
        float acc = 0;
        for (int c = 0; c < ch; ++c) {
            const uint8_t* p = f + c * (bits / 8); float x = 0;
            if (fmtTag == 3 && bits == 32) { float y; std::memcpy (&y, p, 4); x = y; }
            else if (bits == 8) x = (p[0] - 128) / 128.0f;
            else if (bits == 16) x = (int16_t) rd16 (p) / 32768.0f;
            else if (bits == 24) x = ((int32_t) ((uint32_t) p[0] << 8 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 24) >> 8) / 8388608.0f;
            else if (bits == 32) x = (int32_t) rd32 (p) / 2147483648.0f;
            acc += x;
        }
        acc = acc / ch * 32768.0f;
        if (acc > 32767) acc = 32767; if (acc < -32768) acc = -32768;
        if (frames < max) out[frames++] = (int16_t) std::lround (acc);
    }
    bool feed (const uint8_t* d, uint32_t n) {
        while (n > 0 && st != DONE) {
            if (st == HDR || st == CHUNK || st == FMT) {
                const uint32_t take = std::min (n, need - hn);
                std::memcpy (hb + hn, d, take); hn += take; d += take; n -= take;
                if (hn < need) continue;
                if (st == HDR) {
                    if (std::memcmp (hb, "RIFF", 4) || std::memcmp (hb + 8, "WAVE", 4)) { st = DONE; return false; }
                    st = CHUNK; hn = 0; need = 8;
                } else if (st == CHUNK) {
                    const uint32_t sz = rd32 (hb + 4);
                    if (! std::memcmp (hb, "fmt ", 4)) { st = FMT; hn = 0; need = std::min<uint32_t> (sz, 40); chunkLeft = sz - need + (sz & 1); }
                    else if (! std::memcmp (hb, "data", 4)) {
                        const uint32_t fb = ch * bits / 8;
                        if (fb == 0 || fb > 16 || (bits != 8 && bits != 16 && bits != 24 && bits != 32) || (fmtTag != 1 && fmtTag != 3 && fmtTag != 0xFFFE)) { st = DONE; return false; }
                        st = DATA; chunkLeft = sz; ok = true;
                    } else { st = SKIP; chunkLeft = sz + (sz & 1); }
                } else {   // FMT
                    fmtTag = rd16 (hb); ch = rd16 (hb + 2); rate = rd32 (hb + 4); bits = rd16 (hb + 14);
                    if (fmtTag == 0xFFFE && need >= 26) fmtTag = rd16 (hb + 24);
                    st = SKIP;
                }
            } else if (st == SKIP) {
                const uint32_t take = std::min (n, chunkLeft); d += take; n -= take; chunkLeft -= take;
                if (chunkLeft == 0) { st = CHUNK; hn = 0; need = 8; }
            } else if (st == DATA) {
                const uint32_t fb = ch * bits / 8;
                uint32_t take = std::min (n, chunkLeft);
                chunkLeft -= take; n -= take;
                while (take > 0) {
                    if (cn > 0 || take < fb) {
                        const uint32_t m = std::min (take, fb - cn); std::memcpy (carry + cn, d, m); cn += m; d += m; take -= m;
                        if (cn == fb) { sample (carry); cn = 0; }
                    } else { sample (d); d += fb; take -= fb; }
                }
                if (chunkLeft == 0 || frames >= max) st = DONE;
            }
        }
        return true;
    }
};

bool parseWav (const uint8_t* buf, uint32_t len, int16_t* out, uint32_t max, uint32_t* frames, uint32_t* rate) {
    WavIn w; w.out = out; w.max = max;
    w.feed (buf, len);
    if (frames) *frames = w.frames; if (rate) *rate = w.rate;
    return w.ok && w.frames > 0;
}

static void wavHeader (uint8_t* h, uint32_t frames, uint32_t rate) {
    std::memcpy (h, "RIFF", 4); wr32 (h + 4, 36 + frames * 2); std::memcpy (h + 8, "WAVEfmt ", 8);
    wr32 (h + 16, 16); wr16 (h + 20, 1); wr16 (h + 22, 1); wr32 (h + 24, rate); wr32 (h + 28, rate * 2); wr16 (h + 32, 2); wr16 (h + 34, 16);
    std::memcpy (h + 36, "data", 4); wr32 (h + 40, frames * 2);
}

uint32_t makeWav (const int16_t* data, uint32_t frames, uint32_t rate, uint8_t* out, uint32_t max) {
    if (max < 44 + frames * 2) return 0;
    wavHeader (out, frames, rate);
    for (uint32_t i = 0; i < frames; ++i) wr16 (out + 44 + i * 2, (uint16_t) data[i]);
    return 44 + frames * 2;
}

// ------------------------------------------------------------------ the project
static uint8_t proj[12 * 1024];
static constexpr uint32_t MAGIC = 0x31504243;   // "CBP1"

static void slotFile (int s, char* n) { std::snprintf (n, 13, "%c%02d.WAV", 'A' + s / 16, s % 16 + 1); }

struct Writer { uint8_t* p; uint32_t n = 0;
    void u8 (uint32_t v) { p[n++] = (uint8_t) v; } void u32 (uint32_t v) { wr32 (p + n, v); n += 4; }
    void f (float v) { std::memcpy (p + n, &v, 4); n += 4; } void raw (const void* d, uint32_t len) { std::memcpy (p + n, d, len); n += len; } };
struct Reader { const uint8_t* p; uint32_t n = 0, len;
    bool okay (uint32_t k) const { return n + k <= len; }
    uint32_t u8 () { return okay (1) ? p[n++] : 0; } uint32_t u32 () { uint32_t v = okay (4) ? rd32 (p + n) : 0; n += 4; return v; }
    float f () { float v = 0; if (okay (4)) std::memcpy (&v, p + n, 4); n += 4; return v; }
    void raw (void* d, uint32_t k) { if (okay (k)) std::memcpy (d, p + n, k); n += k; } };

bool save (BlockDev& d, const Engine& e, const Ui& ui) {
    if (! mounted (d) && ! format (d)) return false;
    Writer w { proj };
    w.u32 (MAGIC); w.u32 (1);
    w.f (e.bpm); w.f (e.swing); w.f (e.delayAmt); w.f (e.reverbAmt); w.f (e.master); w.f (e.inGain);
    w.u8 (e.lofi); w.u8 (ui.bank); w.u8 (ui.sel); w.u8 (e.chainLen); w.raw (e.chain, NCHAIN);
    for (const Slot& s : e.slots) {
        w.f (s.pitch); w.f (s.filter); w.f (s.trimA); w.f (s.trimB); w.f (s.volume); w.f (s.delay); w.f (s.reverb);
        w.u8 ((s.reverse ? 1 : 0) | (s.loopHold ? 2 : 0) | (s.stretch ? 4 : 0) | (s.freeze ? 8 : 0)); w.u8 (s.beats);
    }
    for (const Pattern& p : e.pats) for (const Step& st : p.s) {
        w.u8 (st.n); w.u8 (st.fx);
        for (const Trig& t : st.t) { w.u8 (t.slot); w.u8 ((uint8_t) t.pitch); w.u8 ((uint8_t) t.filter); }
    }
    if (! writeFile (d, "PROJECT.CBP", proj, w.n)) return false;
    char n[13];
    for (int s = 0; s < NSLOT; ++s) {
        slotFile (s, n);
        if (e.slots[s].empty ()) { remove (d, n); continue; }
        uint8_t nn[11]; name83 (n, nn); uint32_t fs; int fi;
        Vol v; if (e.slots[s].saved && vol (d, v) && findEntry (d, v, nn, fs, fi, nullptr)) continue;   // unchanged: keep the file
        const int16_t* data; const uint32_t frames = e.slotData (s, &data);
        struct Ctx { const int16_t* data; uint32_t frames, rate; } c { data, frames, e.slots[s].rate };
        const bool ok = writeStream (d, n, 44 + frames * 2, [] (void* x, uint32_t off, uint8_t* b, uint32_t m) {
            Ctx& c = *(Ctx*) x;
            for (uint32_t i = 0; i < m; ++i) {
                const uint32_t o = off + i;
                if (o < 44) { uint8_t h[44]; wavHeader (h, c.frames, c.rate); b[i] = h[o]; continue; }
                const uint32_t k = (o - 44) / 2; const uint16_t v = (uint16_t) c.data[k];
                b[i] = (uint8_t) (((o - 44) & 1) ? v >> 8 : v);
            }
        }, &c);
        if (! ok) return false;
        const_cast<Engine&> (e).slots[s].saved = true;
    }
    d.flush ();
    return true;
}

// load one WAV straight into the sample pool
static bool loadWav (BlockDev& d, const char* name, Engine& e, int slot) {
    const uint32_t room = e.memSize - e.memUsed;
    if (room < 64) return false;
    const int b = e.newBlock (room);
    if (b < 0) return false;
    WavIn w; w.out = e.mem + e.blocks[b].off; w.max = room;
    readStream (d, name, [] (void* x, uint32_t, const uint8_t* data, uint32_t n) { return ((WavIn*) x)->feed (data, n) && ((WavIn*) x)->st != WavIn::DONE; }, &w);
    Block& bl = e.blocks[b];
    if (! w.ok || w.frames < 16) { bl.refs = 0; bl.len = 0; e.memUsed = bl.off; return false; }
    bl.len = w.frames; e.memUsed = bl.off + w.frames;
    Slot& s = e.slots[slot]; s = Slot (); s.block = (int16_t) b; s.start = 0; s.len = w.frames; s.rate = w.rate ? w.rate : SR;
    s.saved = true;
    return true;
}

bool load (BlockDev& d, Engine& e, Ui& ui) {
    if (! mounted (d)) return false;
    e.allOff ();
    e.busy = true;
    e.init (e.mem, e.memSize);
    // settings and patterns
    uint32_t got = 0;
    Slot params[NSLOT];
    const bool haveProj = readFile (d, "PROJECT.CBP", proj, sizeof proj, &got) && got > 8 && rd32 (proj) == MAGIC;
    if (haveProj) {
        Reader r { proj, 8, got };
        e.bpm = r.f (); e.swing = r.f (); e.delayAmt = r.f (); e.reverbAmt = r.f (); e.master = r.f (); e.inGain = r.f ();
        e.lofi = r.u8 (); ui.bank = (int) r.u8 () % NBANK; ui.sel = (int) r.u8 () % NSLOT; e.chainLen = std::max<int> (1, std::min<int> (NCHAIN, (int) r.u8 ()));
        r.raw (e.chain, NCHAIN);
        for (Slot& s : params) {
            s.pitch = r.f (); s.filter = r.f (); s.trimA = r.f (); s.trimB = r.f (); s.volume = r.f (); s.delay = r.f (); s.reverb = r.f ();
            const uint32_t fl = r.u8 (); s.reverse = fl & 1; s.loopHold = fl & 2; s.stretch = fl & 4; s.freeze = fl & 8; s.beats = (uint8_t) r.u8 ();
        }
        for (Pattern& p : e.pats) for (Step& st : p.s) {
            st.n = (uint8_t) std::min<uint32_t> (NTRIG, r.u8 ()); st.fx = (uint8_t) r.u8 ();
            for (Trig& t : st.t) { t.slot = (uint8_t) (r.u8 () % NSLOT); t.pitch = (int8_t) r.u8 (); t.filter = (int8_t) r.u8 (); }
        }
        e.pat = e.chain[0] % NPAT;
    }
    // the sounds: A01.WAV .. D16.WAV first, then any other WAV into the empty slots
    static char names[160][13]; static uint32_t sizes[160];
    const int n = list (d, names, sizes, 160);
    bool used[160] = {false};
    for (int s = 0; s < NSLOT; ++s) {
        char fn[13]; slotFile (s, fn);
        for (int i = 0; i < n; ++i) if (! used[i] && ! std::strcmp (names[i], fn)) {
            used[i] = true;
            if (loadWav (d, fn, e, s) && haveProj) {
                Slot& sl = e.slots[s]; const Slot& p = params[s];
                sl.pitch = p.pitch; sl.filter = p.filter; sl.trimA = p.trimA; sl.trimB = p.trimB; sl.volume = p.volume;
                sl.delay = p.delay; sl.reverb = p.reverb; sl.reverse = p.reverse; sl.loopHold = p.loopHold; sl.stretch = p.stretch;
                sl.freeze = p.freeze; sl.beats = p.beats;
            }
        }
    }
    // other WAVs, alphabetically
    for (;;) {
        int best = -1;
        for (int i = 0; i < n; ++i) {
            if (used[i]) continue;
            const char* dot = std::strrchr (names[i], '.');
            if (! dot || std::strcmp (dot, ".WAV")) { used[i] = true; continue; }
            if (best < 0 || std::strcmp (names[i], names[best]) < 0) best = i;
        }
        if (best < 0) break;
        used[best] = true;
        int slot = -1; for (int s = 0; s < NSLOT && slot < 0; ++s) if (e.slots[s].empty ()) slot = s;
        if (slot < 0) break;
        loadWav (d, names[best], e, slot);
    }
    e.busy = false;
    return true;
}

} // namespace store
} // namespace pocket

// Cat Box Pocket: the disks. A FAT16 volume in the flash or a FAT16 / FAT32 microSD card (with or
// without a partition table), so a computer sees it over USB. In the root directory:
//   PROJnn.CBP  a project (settings, sound parameters, patterns, song, scenes), nn = 01..16
//   PnnA01.WAV  .. PnnD16.WAV  its sounds
//   KITnn.CBK + KnnP01.WAV .. KnnP16.WAV  a kit (one bank of 16 sounds)
//   EXPnn.WAV   an exported pattern or song (stereo)
// Any other WAV on the disk can be loaded with the browser; project 1 also takes them into empty pads.
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

static uint8_t sec[512];          // one sector buffer (data / directory)
static uint8_t fatSec[512];       // the cached FAT sector
static uint32_t fatNum = 0xffffffff; static bool fatDirty = false;

struct Vol {
    uint32_t base = 0, total = 0, fatStart = 0, fatSecs = 0, nFats = 0, rootStart = 0, rootSecs = 0, rootClus = 0, dataStart = 0, spc = 0, clusters = 0;
    bool fat32 = false;
    uint32_t eoc () const { return fat32 ? 0x0FFFFFF8u : 0xFFF8u; }
};

static bool bpbOk (const uint8_t* b) {
    return b[510] == 0x55 && b[511] == 0xAA && rd16 (b + 11) == 512 && b[13] != 0 && (b[13] & (b[13] - 1)) == 0 && b[16] != 0 && rd16 (b + 14) != 0;
}

static bool vol (BlockDev& d, Vol& v) {
    uint8_t b[512];
    if (! d.read (0, b)) return false;
    v = Vol ();
    if (! bpbOk (b)) {
        // a partition table: take the first FAT partition
        if (b[510] != 0x55 || b[511] != 0xAA) return false;
        bool found = false;
        for (int i = 0; i < 4 && ! found; ++i) {
            const uint8_t* e = b + 446 + i * 16; const uint8_t t = e[4];
            if (t == 0x01 || t == 0x04 || t == 0x06 || t == 0x0B || t == 0x0C || t == 0x0E) { v.base = rd32 (e + 8); found = true; }
        }
        if (! found || ! d.read (v.base, b) || ! bpbOk (b)) return false;
    }
    v.spc = b[13]; const uint32_t res = rd16 (b + 14); v.nFats = b[16];
    const uint32_t rootEnt = rd16 (b + 17);
    v.total = rd16 (b + 19) ? rd16 (b + 19) : rd32 (b + 32);
    v.fatSecs = rd16 (b + 22);
    if (v.fatSecs == 0) { v.fatSecs = rd32 (b + 36); v.fat32 = true; v.rootClus = rd32 (b + 44); }
    if (v.fatSecs == 0) return false;
    v.fatStart = v.base + res;
    v.rootStart = v.fatStart + v.nFats * v.fatSecs;
    v.rootSecs = v.fat32 ? 0 : (rootEnt * 32 + 511) / 512;
    v.dataStart = v.rootStart + v.rootSecs;
    v.clusters = (v.total - (v.dataStart - v.base)) / v.spc;
    fatNum = 0xffffffff; fatDirty = false;
    return true;
}

static void fatFlush (BlockDev& d, const Vol& v) {
    if (! fatDirty) return;
    for (uint32_t f = 0; f < v.nFats; ++f) d.write (v.fatStart + f * v.fatSecs + fatNum, fatSec);
    fatDirty = false;
}
static uint32_t fatGet (BlockDev& d, const Vol& v, uint32_t c) {
    const uint32_t bytes = v.fat32 ? 4 : 2;
    const uint32_t s = c * bytes / 512;
    if (s != fatNum) { fatFlush (d, v); d.read (v.fatStart + s, fatSec); fatNum = s; }
    return v.fat32 ? rd32 (fatSec + (c * 4) % 512) & 0x0FFFFFFF : rd16 (fatSec + (c * 2) % 512);
}
static void fatSet (BlockDev& d, const Vol& v, uint32_t c, uint32_t val) {
    fatGet (d, v, c);
    if (v.fat32) { const uint32_t o = (c * 4) % 512; wr32 (fatSec + o, (rd32 (fatSec + o) & 0xF0000000u) | (val & 0x0FFFFFFF)); }
    else wr16 (fatSec + (c * 2) % 512, val);
    fatDirty = true;
}
static uint32_t clusterSector (const Vol& v, uint32_t c) { return v.dataStart + (c - 2) * v.spc; }
static bool chainOk (const Vol& v, uint32_t c) { return c >= 2 && c < v.eoc () && c < v.clusters + 2; }

static uint32_t allocCluster (BlockDev& d, const Vol& v, uint32_t from) {
    for (uint32_t c = std::max<uint32_t> (2, from); c < v.clusters + 2; ++c) if (fatGet (d, v, c) == 0) return c;
    return 0;
}

// the n-th sector of the root directory (FAT32: along its cluster chain, grown when asked)
static bool dirSector (BlockDev& d, const Vol& v, uint32_t n, uint32_t& out, bool grow) {
    if (! v.fat32) { if (n >= v.rootSecs) return false; out = v.rootStart + n; return true; }
    uint32_t c = v.rootClus;
    for (uint32_t k = n / v.spc; k > 0; --k) {
        uint32_t nx = fatGet (d, v, c);
        if (! chainOk (v, nx)) {
            if (! grow) return false;
            nx = allocCluster (d, v, c);
            if (! nx) return false;
            fatSet (d, v, c, nx); fatSet (d, v, nx, 0x0FFFFFFF); fatFlush (d, v);
            uint8_t z[512]; std::memset (z, 0, 512);
            for (uint32_t s = 0; s < v.spc; ++s) d.write (clusterSector (v, nx) + s, z);
        }
        c = nx;
    }
    out = clusterSector (v, c) + n % v.spc;
    return true;
}

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
static uint32_t firstCluster (const Vol& v, const uint8_t* e) { return rd16 (e + 26) | (v.fat32 ? (uint32_t) rd16 (e + 20) << 16 : 0); }

// ------------------------------------------------------------------ format (FAT16 for the flash, FAT32 for big cards)
bool format (BlockDev& d, const char* label, bool fat32) {
    const uint32_t total = d.sectors ();
    uint8_t lab[11]; name83 (label, lab);
    uint8_t b[512]; std::memset (b, 0, 512);
    b[0] = 0xEB; b[1] = 0x58; b[2] = 0x90; std::memcpy (b + 3, "CATPOCKT", 8);
    wr16 (b + 11, 512); b[21] = 0xF8; wr16 (b + 24, 32); wr16 (b + 26, 64);
    uint32_t spc, res, fs, rootSecs, clusters;
    const uint32_t nf = 2;
    if (! fat32) {
        spc = 4; res = 1; rootSecs = 32; fs = 1;
        for (int it = 0; it < 8; ++it) { const uint32_t cl = (total - res - nf * fs - rootSecs) / spc; fs = ((cl + 2) * 2 + 511) / 512; }
        clusters = (total - res - nf * fs - rootSecs) / spc;
        if (clusters < 4085 || clusters > 65524) return false;
        b[13] = (uint8_t) spc; wr16 (b + 14, res); b[16] = (uint8_t) nf; wr16 (b + 17, 512);
        if (total < 65536) wr16 (b + 19, total); else wr32 (b + 32, total);
        wr16 (b + 22, fs);
        b[36] = 0x80; b[38] = 0x29; wr32 (b + 39, 0x20261107); std::memcpy (b + 43, lab, 11); std::memcpy (b + 54, "FAT16   ", 8);
    } else {
        spc = total > 32u * 1024 * 1024 ? 64 : total > 16u * 1024 * 1024 ? 32 : total > 512u * 1024 ? 8 : 1;
        res = 32; rootSecs = 0; fs = 1;
        for (int it = 0; it < 8; ++it) { const uint32_t cl = (total - res - nf * fs) / spc; fs = ((cl + 2) * 4 + 511) / 512; }
        clusters = (total - res - nf * fs) / spc;
        if (clusters < 65525) return false;
        b[13] = (uint8_t) spc; wr16 (b + 14, res); b[16] = (uint8_t) nf; wr32 (b + 32, total);
        wr32 (b + 36, fs); wr32 (b + 44, 2); wr16 (b + 48, 1); wr16 (b + 50, 6);
        b[64] = 0x80; b[66] = 0x29; wr32 (b + 67, 0x20261107); std::memcpy (b + 71, lab, 11); std::memcpy (b + 82, "FAT32   ", 8);
    }
    b[510] = 0x55; b[511] = 0xAA;
    if (! d.write (0, b)) return false;
    if (fat32) {
        d.write (6, b);                                   // backup boot sector
        uint8_t fi[512]; std::memset (fi, 0, 512);
        wr32 (fi, 0x41615252); wr32 (fi + 484, 0x61417272); wr32 (fi + 488, 0xFFFFFFFF); wr32 (fi + 492, 3); fi[510] = 0x55; fi[511] = 0xAA;
        d.write (1, fi); d.write (7, fi);
    }
    uint8_t z[512];
    for (uint32_t f = 0; f < nf; ++f) for (uint32_t s = 0; s < fs; ++s) {
        std::memset (z, 0, 512);
        if (s == 0) {
            if (fat32) { wr32 (z, 0x0FFFFFF8); wr32 (z + 4, 0x0FFFFFFF); wr32 (z + 8, 0x0FFFFFFF); }
            else { wr16 (z, 0xFFF8); wr16 (z + 2, 0xFFFF); }
        }
        d.write (res + f * fs + s, z);
    }
    const uint32_t rootAt = res + nf * fs, rootN = fat32 ? spc : rootSecs;
    for (uint32_t s = 0; s < rootN; ++s) {
        std::memset (z, 0, 512);
        if (s == 0) { std::memcpy (z, lab, 11); z[11] = 0x08; }
        d.write (rootAt + s, z);
    }
    d.flush ();
    fatNum = 0xffffffff; fatDirty = false;
    return true;
}

bool mounted (BlockDev& d) { Vol v; return vol (d, v); }

// ------------------------------------------------------------------ the root directory
struct DirPos { uint32_t sector; int index; };

// walks the root: fn (entry, pos, longName) returns true to stop; returns false at the end
template <typename Fn> static bool walk (BlockDev& d, const Vol& v, Fn fn) {
    char lfn[64]; int lfnLen = 0; bool haveLfn = false;
    for (uint32_t n = 0;; ++n) {
        uint32_t s; if (! dirSector (d, v, n, s, false)) return false;
        d.read (s, sec);
        for (int i = 0; i < 16; ++i) {
            uint8_t* e = sec + i * 32;
            if (e[0] == 0) return false;
            if (e[0] == 0xE5) { haveLfn = false; continue; }
            if (e[11] == 0x0F) {             // a long name piece: 13 characters
                const int seq = e[0] & 0x1F;
                if (e[0] & 0x40) { std::memset (lfn, 0, sizeof lfn); lfnLen = 0; haveLfn = true; }
                static const int OFF[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
                for (int k = 0; k < 13; ++k) {
                    const int at = (seq - 1) * 13 + k; if (at < 0 || at >= 63) continue;
                    const uint16_t c = rd16 (e + OFF[k]);
                    if (c == 0 || c == 0xFFFF) continue;
                    lfn[at] = c < 128 ? (char) c : '_'; lfnLen = std::max (lfnLen, at + 1);
                }
                continue;
            }
            if (e[11] & 0x08) { haveLfn = false; continue; }
            if (fn (e, DirPos { s, i }, haveLfn ? lfn : nullptr)) return true;
            haveLfn = false;
        }
    }
}

static bool findEntry (BlockDev& d, const Vol& v, const uint8_t n[11], DirPos& pos, uint8_t* entry) {
    return walk (d, v, [&] (uint8_t* e, DirPos p, const char*) {
        if ((e[11] & 0x10) || std::memcmp (e, n, 11) != 0) return false;
        pos = p; if (entry) std::memcpy (entry, e, 32); return true;
    });
}

static void freeChain (BlockDev& d, const Vol& v, uint32_t c) {
    while (chainOk (v, c)) { const uint32_t n = fatGet (d, v, c); fatSet (d, v, c, 0); c = n; }
}

bool exists (BlockDev& d, const char* name) {
    Vol v; if (! vol (d, v)) return false;
    uint8_t n[11]; name83 (name, n); DirPos p;
    return findEntry (d, v, n, p, nullptr);
}

bool remove (BlockDev& d, const char* name) {
    Vol v; if (! vol (d, v)) return false;
    uint8_t n[11]; name83 (name, n);
    DirPos p; uint8_t e[32];
    if (! findEntry (d, v, n, p, e)) return false;
    freeChain (d, v, firstCluster (v, e));
    fatFlush (d, v);
    d.read (p.sector, sec);
    sec[p.index * 32] = 0xE5;
    for (int i = p.index - 1; i >= 0 && sec[i * 32 + 11] == 0x0F; --i) sec[i * 32] = 0xE5;   // its long name too
    d.write (p.sector, sec);
    d.flush ();
    return true;
}

int list (BlockDev& d, char names[][13], uint32_t* sizes, int max, char longNames[][24]) {
    Vol v; if (! vol (d, v)) return 0;
    int n = 0;
    walk (d, v, [&] (uint8_t* e, DirPos, const char* lfn) {
        if (e[11] & 0x18) return false;
        name83to (e, names[n]); if (sizes) sizes[n] = rd32 (e + 28);
        if (longNames) { std::snprintf (longNames[n], 24, "%.23s", lfn ? lfn : names[n]); }
        return ++n >= max;
    });
    return n;
}

// ------------------------------------------------------------------ streams
typedef void (*FillFn) (void* ctx, uint32_t off, uint8_t* buf, uint32_t n);

static bool writeStream (BlockDev& d, const char* name, uint32_t len, FillFn fill, void* ctx) {
    remove (d, name);
    Vol v; if (! vol (d, v)) return false;
    const uint32_t csize = v.spc * 512, need = std::max<uint32_t> (1, (len + csize - 1) / csize);
    // allocate a chain
    uint32_t first = 0, prev = 0, got = 0;
    for (uint32_t c = 2; c < v.clusters + 2 && got < need; ++c)
        if (fatGet (d, v, c) == 0) {
            if (prev) fatSet (d, v, prev, c); else first = c;
            fatSet (d, v, c, v.fat32 ? 0x0FFFFFFF : 0xFFFF); prev = c; ++got;
        }
    if (got < need) { if (first) freeChain (d, v, first); fatFlush (d, v); return false; }
    fatFlush (d, v);
    // write the data along the chain
    uint32_t c = first, off = 0;
    while (off < len && chainOk (v, c)) {
        for (uint32_t k = 0; k < v.spc && off < len; ++k) {
            const uint32_t n = std::min<uint32_t> (512, len - off);
            std::memset (sec, 0, 512); fill (ctx, off, sec, n);
            d.write (clusterSector (v, c) + k, sec); off += n;
        }
        c = fatGet (d, v, c);
    }
    // the directory entry: a free one (FAT32 grows the root when it is full)
    uint8_t n[11]; name83 (name, n);
    for (uint32_t k = 0;; ++k) {
        uint32_t s;
        if (! dirSector (d, v, k, s, true)) break;
        d.read (s, sec);
        for (int i = 0; i < 16; ++i) {
            uint8_t* e = sec + i * 32;
            if (e[0] != 0 && e[0] != 0xE5) continue;
            std::memset (e, 0, 32); std::memcpy (e, n, 11); e[11] = 0x20;
            const uint16_t date = (uint16_t) (((2026 - 1980) << 9) | (11 << 5) | 7);
            wr16 (e + 14, 0x6000); wr16 (e + 16, date); wr16 (e + 18, date); wr16 (e + 22, 0x6000); wr16 (e + 24, date);
            wr16 (e + 20, first >> 16); wr16 (e + 26, first & 0xFFFF); wr32 (e + 28, len);
            d.write (s, sec); d.flush ();
            return true;
        }
    }
    freeChain (d, v, first); fatFlush (d, v);
    return false;
}

// a file opened for reading, byte by byte
struct FileIn {
    BlockDev* d = nullptr; Vol v; uint32_t len = 0, pos = 0, clus = 0; uint8_t buf[512]; uint32_t bufSec = 0xffffffff;
    bool open (BlockDev& dev, const char* name) {
        d = &dev; if (! vol (dev, v)) return false;
        uint8_t n[11]; name83 (name, n); DirPos p; uint8_t e[32];
        if (! findEntry (dev, v, n, p, e)) return false;
        len = rd32 (e + 28); clus = firstCluster (v, e); pos = 0;
        return true;
    }
    // reads up to n bytes, returns how many
    uint32_t read (uint8_t* out, uint32_t n) {
        uint32_t got = 0;
        const uint32_t csize = v.spc * 512;
        while (got < n && pos < len) {
            if (! chainOk (v, clus)) break;
            const uint32_t inClus = pos % csize, s = clusterSector (v, clus) + inClus / 512;
            if (s != bufSec) { d->read (s, buf); bufSec = s; }
            const uint32_t o = pos % 512, m = std::min ({n - got, 512 - o, len - pos});
            std::memcpy (out + got, buf + o, m); got += m; pos += m;
            if (pos % csize == 0) clus = fatGet (*d, v, clus);
        }
        return got;
    }
};

struct BufCtx { const uint8_t* src; };
bool writeFile (BlockDev& d, const char* name, const uint8_t* data, uint32_t len) {
    BufCtx c { data };
    return writeStream (d, name, len, [] (void* x, uint32_t off, uint8_t* b, uint32_t n) { std::memcpy (b, ((BufCtx*) x)->src + off, n); }, &c);
}
bool readFile (BlockDev& d, const char* name, uint8_t* buf, uint32_t max, uint32_t* got) {
    FileIn f; if (! f.open (d, name)) return false;
    const uint32_t n = f.read (buf, max);
    if (got) *got = n;
    return true;
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

static void wavHeader (uint8_t* h, uint32_t frames, uint32_t rate, int ch = 1) {
    const uint32_t bytes = frames * 2 * ch;
    std::memcpy (h, "RIFF", 4); wr32 (h + 4, 36 + bytes); std::memcpy (h + 8, "WAVEfmt ", 8);
    wr32 (h + 16, 16); wr16 (h + 20, 1); wr16 (h + 22, ch); wr32 (h + 24, rate); wr32 (h + 28, rate * 2 * ch); wr16 (h + 32, 2 * ch); wr16 (h + 34, 16);
    std::memcpy (h + 36, "data", 4); wr32 (h + 40, bytes);
}

uint32_t makeWav (const int16_t* data, uint32_t frames, uint32_t rate, uint8_t* out, uint32_t max) {
    if (max < 44 + frames * 2) return 0;
    wavHeader (out, frames, rate);
    for (uint32_t i = 0; i < frames; ++i) wr16 (out + 44 + i * 2, (uint16_t) data[i]);
    return 44 + frames * 2;
}

// one sound as a mono WAV
static bool writeSlotWav (BlockDev& d, const char* name, const Engine& e, int s) {
    const int16_t* data; const uint32_t frames = e.slotData (s, &data);
    struct Ctx { const int16_t* data; uint32_t frames, rate; } c { data, frames, e.slots[s].rate };
    return writeStream (d, name, 44 + frames * 2, [] (void* x, uint32_t off, uint8_t* b, uint32_t m) {
        Ctx& c = *(Ctx*) x;
        for (uint32_t i = 0; i < m; ++i) {
            const uint32_t o = off + i;
            if (o < 44) { uint8_t h[44]; wavHeader (h, c.frames, c.rate); b[i] = h[o]; continue; }
            const uint32_t k = (o - 44) / 2; const uint16_t v = (uint16_t) c.data[k];
            b[i] = (uint8_t) (((o - 44) & 1) ? v >> 8 : v);
        }
    }, &c);
}

// load one WAV straight into the sample pool
static bool loadWav (BlockDev& d, const char* name, Engine& e, int slot) {
    FileIn f; if (! f.open (d, name)) return false;
    const uint32_t room = e.memSize - e.memUsed;
    if (room < 64) return false;
    const int b = e.newBlock (room);
    if (b < 0) return false;
    WavIn w; w.out = e.mem + e.blocks[b].off; w.max = room;
    uint8_t buf[512];
    for (;;) { const uint32_t n = f.read (buf, sizeof buf); if (n == 0 || ! w.feed (buf, n) || w.st == WavIn::DONE) break; }
    Block& bl = e.blocks[b];
    if (! w.ok || w.frames < 16) { bl.refs = 0; bl.len = 0; e.memUsed = bl.off; return false; }
    bl.len = w.frames; e.memUsed = bl.off + w.frames;
    Slot& s = e.slots[slot]; s = Slot (); s.block = (int16_t) b; s.start = 0; s.len = w.frames; s.rate = w.rate ? w.rate : SR;
    s.saved = true;
    return true;
}

bool loadWavInto (BlockDev& d, const char* name, Engine& e, int slot) {
    if (slot < 0 || slot >= NSLOT) return false;
    e.clearSlot (slot);
    const bool ok = loadWav (d, name, e, slot);
    if (ok) e.slots[slot].saved = false;     // not one of the project's files yet
    return ok;
}

// ------------------------------------------------------------------ serializing (projects and kits)
// The writer only keeps the bytes in [from, to): the project is serialized again for every sector,
// so no big buffer is needed (the patterns alone can be 50 KB).
struct Out {
    uint8_t* buf = nullptr; uint32_t from = 0, to = 0, n = 0;
    void u8 (uint32_t v) { if (buf && n >= from && n < to) buf[n - from] = (uint8_t) v; ++n; }
    void u16 (uint32_t v) { u8 (v); u8 (v >> 8); }
    void u32 (uint32_t v) { u16 (v); u16 (v >> 16); }
    void f (float v) { uint32_t u; std::memcpy (&u, &v, 4); u32 (u); }
};
struct In {
    FileIn* f; bool bad = false;
    uint32_t u8 () { uint8_t b; if (f->read (&b, 1) != 1) { bad = true; return 0; } return b; }
    uint32_t u16 () { const uint32_t a = u8 (); return a | u8 () << 8; }
    uint32_t u32 () { const uint32_t a = u16 (); return a | u16 () << 16; }
    float f32 () { const uint32_t u = u32 (); float v; std::memcpy (&v, &u, 4); return v; }
};

static void putSlot (Out& w, const Slot& s) {
    w.f (s.pitch); w.f (s.filter); w.f (s.volume); w.f (s.pan); w.f (s.trimA); w.f (s.trimB);
    w.u8 ((s.reverse ? 1 : 0) | (s.loopHold ? 2 : 0) | (s.stretch ? 4 : 0) | (s.freeze ? 8 : 0) | (s.duckSource ? 16 : 0) | (s.mute ? 32 : 0));
    w.u8 (s.beats); w.f (s.attack); w.f (s.decay); w.u8 (s.choke); w.f (s.drive);
    w.f (s.lfoRate); w.f (s.lfoDepth); w.u8 (s.lfoTarget); w.u8 (s.vintage); w.f (s.delay); w.f (s.reverb);
}
static void getSlot (In& r, Slot& s) {
    s.pitch = r.f32 (); s.filter = r.f32 (); s.volume = r.f32 (); s.pan = r.f32 (); s.trimA = r.f32 (); s.trimB = r.f32 ();
    const uint32_t fl = r.u8 (); s.reverse = fl & 1; s.loopHold = fl & 2; s.stretch = fl & 4; s.freeze = fl & 8; s.duckSource = fl & 16; s.mute = fl & 32;
    s.beats = (uint8_t) r.u8 (); s.attack = r.f32 (); s.decay = r.f32 (); s.choke = (uint8_t) (r.u8 () % 5); s.drive = r.f32 ();
    s.lfoRate = r.f32 (); s.lfoDepth = r.f32 (); s.lfoTarget = (uint8_t) (r.u8 () % 4); s.vintage = (uint8_t) (r.u8 () % 4); s.delay = r.f32 (); s.reverb = r.f32 ();
}
static void copyParams (Slot& to, const Slot& p) {
    const int16_t b = to.block; const uint32_t st = to.start, len = to.len, rate = to.rate; const bool sv = to.saved;
    to = p; to.block = b; to.start = st; to.len = len; to.rate = rate; to.saved = sv;
}

static constexpr uint32_t MAGIC = 0x32504243;   // "CBP2"
static constexpr uint32_t KITMAGIC = 0x324B4243; // "CBK2"

static void serializeProject (Out& w, const Engine& e, const Ui& ui) {
    w.u32 (MAGIC); w.u32 (2);
    w.f (e.bpm); w.f (e.swing); w.f (e.delayAmt); w.f (e.reverbAmt); w.f (e.master); w.f (e.inGain); w.f (e.compAmt); w.f (e.duckAmt);
    w.u8 ((e.lofi ? 1 : 0) | (e.metronome ? 2 : 0) | (e.autoTrim ? 4 : 0) | (e.threshold ? 8 : 0) | (e.songMode ? 16 : 0));
    w.u8 (e.scale); w.u8 ((uint8_t) e.root); w.u8 (e.arpMode); w.f (e.arpDiv); w.u8 (e.midiChannel); w.u8 (e.syncMode);
    w.u8 (ui.bank); w.u8 (ui.sel);
    w.u8 (e.chainLen); for (int i = 0; i < NCHAIN; ++i) w.u8 (e.chain[i]);
    w.u8 (e.songLen); for (int i = 0; i < e.songLen; ++i) { w.u8 (e.song[i].pat); w.u8 (e.song[i].repeats); }
    for (const Scene& sc : e.scenes) {
        w.u8 (sc.set);
        if (sc.set) { for (int s = 0; s < NSLOT; ++s) { w.f (sc.pitch[s]); w.f (sc.filter[s]); w.f (sc.volume[s]); } w.f (sc.delay); w.f (sc.reverb); }
    }
    for (const Slot& s : e.slots) putSlot (w, s);
    for (const Pattern& p : e.pats) {
        w.u8 (p.len); w.u8 (p.nPoly);
        for (int i = 0; i < p.nPoly; ++i) { w.u8 (p.polySlot[i]); w.u8 (p.polyLen[i]); }
        int used = 0; for (const Step& st : p.s) used += st.n > 0 || st.fx;
        w.u8 (used);
        for (int k = 0; k < NSTEP; ++k) {
            const Step& st = p.s[k];
            if (st.n == 0 && ! st.fx) continue;
            w.u8 (k); w.u8 (st.n); w.u8 (st.fx);
            for (int i = 0; i < st.n; ++i) { const Trig& t = st.t[i]; w.u8 (t.slot); w.u8 ((uint8_t) t.pitch); w.u8 ((uint8_t) t.filter); w.u8 (t.vel); w.u8 ((uint8_t) t.pan); w.u8 (t.cond); }
        }
    }
}

static bool writeSerialized (BlockDev& d, const char* name, void (*ser) (Out&, const void*), const void* ctx) {
    Out count; ser (count, ctx);
    struct C { void (*ser) (Out&, const void*); const void* ctx; } c { ser, ctx };
    return writeStream (d, name, count.n, [] (void* x, uint32_t off, uint8_t* b, uint32_t n) {
        C& c = *(C*) x; Out w; w.buf = b; w.from = off; w.to = off + n; c.ser (w, c.ctx);
    }, &c);
}

static int savedProj = -1;      // the project whose WAVs match the sounds marked `saved`
static void projFile (int p, char* n) { std::snprintf (n, 13, "PROJ%02d.CBP", p + 1); }
static void slotFile (int p, int s, char* n) { std::snprintf (n, 13, "P%02d%c%02d.WAV", p + 1, 'A' + s / 16, s % 16 + 1); }
static void kitFile (int k, char* n) { std::snprintf (n, 13, "KIT%02d.CBK", k + 1); }
static void kitWav (int k, int i, char* n) { std::snprintf (n, 13, "K%02dP%02d.WAV", k + 1, i + 1); }

bool save (BlockDev& d, const Engine& e, const Ui& ui, int project) {
    if (project < 0 || project >= NPROJ) return false;
    if (! mounted (d) && ! format (d)) return false;
    char n[13]; projFile (project, n);
    struct P { const Engine* e; const Ui* ui; } pc { &e, &ui };
    if (! writeSerialized (d, n, [] (Out& w, const void* x) { const P& p = *(const P*) x; serializeProject (w, *p.e, *p.ui); }, &pc)) return false;
    const bool same = savedProj == project;
    for (int s = 0; s < NSLOT; ++s) {
        slotFile (project, s, n);
        if (e.slots[s].empty ()) { remove (d, n); continue; }
        if (same && e.slots[s].saved && exists (d, n)) continue;     // unchanged: keep the file
        if (! writeSlotWav (d, n, e, s)) return false;
        const_cast<Engine&> (e).slots[s].saved = true;
    }
    savedProj = project;
    d.flush ();
    return true;
}

static bool reserved (const char* n) {      // the files the Pocket owns (not loose sounds)
    auto num = [] (char c) { return c >= '0' && c <= '9'; };
    if (n[0] == 'P' && num (n[1]) && num (n[2]) && n[3] >= 'A' && n[3] <= 'D') return true;
    if (n[0] == 'K' && num (n[1]) && num (n[2]) && n[3] == 'P') return true;
    if (! std::strncmp (n, "EXP", 3) && num (n[3])) return true;
    return false;
}

bool load (BlockDev& d, Engine& e, Ui& ui, int project) {
    if (! mounted (d) || project < 0 || project >= NPROJ) return false;
    e.allOff (); e.play (false);
    e.busy = true;
    e.init (e.mem, e.memSize);
    char n[13]; projFile (project, n);
    static Slot params[NSLOT];
    FileIn f; bool haveProj = false;
    if (f.open (d, n)) {
        In r { &f };
        if (r.u32 () == MAGIC && r.u32 () == 2) {
            e.bpm = r.f32 (); e.swing = r.f32 (); e.delayAmt = r.f32 (); e.reverbAmt = r.f32 (); e.master = r.f32 (); e.inGain = r.f32 (); e.compAmt = r.f32 (); e.duckAmt = r.f32 ();
            const uint32_t fl = r.u8 (); e.lofi = fl & 1; e.metronome = fl & 2; e.autoTrim = fl & 4; e.threshold = fl & 8; e.songMode = fl & 16;
            e.scale = (int) r.u8 () % NSCALE; e.root = (int8_t) r.u8 (); e.arpMode = (int) r.u8 () % 6; e.arpDiv = r.f32 (); e.midiChannel = (int) r.u8 () & 15; e.syncMode = (int) r.u8 () % 3;
            ui.bank = (int) r.u8 () % NBANK; ui.sel = (int) r.u8 () % NSLOT;
            e.chainLen = std::max<int> (1, std::min<int> (NCHAIN, (int) r.u8 ()));
            for (int i = 0; i < NCHAIN; ++i) e.chain[i] = (uint8_t) (r.u8 () % NPAT);
            e.songLen = std::min<int> (NSONG, (int) r.u8 ());
            for (int i = 0; i < e.songLen; ++i) { e.song[i].pat = (uint8_t) (r.u8 () % NPAT); e.song[i].repeats = (uint8_t) std::max<uint32_t> (1, r.u8 ()); }
            for (Scene& sc : e.scenes) {
                sc.set = r.u8 ();
                if (sc.set) { for (int s = 0; s < NSLOT; ++s) { sc.pitch[s] = r.f32 (); sc.filter[s] = r.f32 (); sc.volume[s] = r.f32 (); } sc.delay = r.f32 (); sc.reverb = r.f32 (); }
            }
            for (Slot& s : params) { s = Slot (); getSlot (r, s); }
            for (Pattern& p : e.pats) {
                p = Pattern ();
                p.len = (uint8_t) std::max<uint32_t> (1, std::min<uint32_t> (NSTEP, r.u8 ()));
                p.nPoly = (uint8_t) std::min<uint32_t> (NPOLY, r.u8 ());
                for (int i = 0; i < p.nPoly; ++i) { p.polySlot[i] = (uint8_t) (r.u8 () % NSLOT); p.polyLen[i] = (uint8_t) r.u8 (); }
                const int used = (int) r.u8 ();
                for (int k = 0; k < used && ! r.bad; ++k) {
                    Step& st = p.s[r.u8 () % NSTEP];
                    st.n = (uint8_t) std::min<uint32_t> (NTRIG, r.u8 ()); st.fx = (uint8_t) (r.u8 () % (NFX + 1));
                    for (int i = 0; i < st.n; ++i) {
                        Trig& t = st.t[i]; t.slot = (uint8_t) (r.u8 () % NSLOT); t.pitch = (int8_t) r.u8 (); t.filter = (int8_t) r.u8 ();
                        t.vel = (uint8_t) r.u8 (); t.pan = (int8_t) r.u8 (); t.cond = (uint8_t) (r.u8 () % NCOND);
                    }
                }
            }
            haveProj = ! r.bad;
            e.pat = (e.songMode && e.songLen > 0 ? e.song[0].pat : e.chain[0]) % NPAT;
        }
    }
    // the sounds: PnnA01.WAV .. PnnD16.WAV, then (project 1 only) loose WAVs into the empty pads
    static char names[256][13];
    const int nf = list (d, names, nullptr, 256);
    for (int s = 0; s < NSLOT; ++s) {
        slotFile (project, s, n);
        bool there = false; for (int i = 0; i < nf && ! there; ++i) there = ! std::strcmp (names[i], n);
        if (there && loadWav (d, n, e, s) && haveProj) copyParams (e.slots[s], params[s]);
    }
    if (project == 0) {
        static bool used[256];
        std::memset (used, 0, sizeof used);
        for (;;) {
            int best = -1;
            for (int i = 0; i < nf; ++i) {
                if (used[i]) continue;
                const char* dot = std::strrchr (names[i], '.');
                if (! dot || std::strcmp (dot, ".WAV") || reserved (names[i])) { used[i] = true; continue; }
                if (best < 0 || std::strcmp (names[i], names[best]) < 0) best = i;
            }
            if (best < 0) break;
            used[best] = true;
            int slot = -1; for (int s = 0; s < NSLOT && slot < 0; ++s) if (e.slots[s].empty ()) slot = s;
            if (slot < 0) break;
            if (loadWav (d, names[best], e, slot)) e.slots[slot].saved = false;
        }
    }
    savedProj = project;
    ui.project = project;
    e.busy = false;
    return true;
}

// ------------------------------------------------------------------ kits: one bank of 16 sounds
bool saveKit (BlockDev& d, const Engine& e, int bank, int kit) {
    if (bank < 0 || bank >= NBANK || kit < 0 || kit >= 99) return false;
    if (! mounted (d)) return false;
    struct K { const Engine* e; int bank; } kc { &e, bank };
    char n[13]; kitFile (kit, n);
    if (! writeSerialized (d, n, [] (Out& w, const void* x) {
        const K& k = *(const K*) x; w.u32 (KITMAGIC);
        for (int i = 0; i < 16; ++i) { const Slot& s = k.e->slots[k.bank * 16 + i]; w.u8 (! s.empty ()); putSlot (w, s); }
    }, &kc)) return false;
    for (int i = 0; i < 16; ++i) {
        kitWav (kit, i, n);
        const int s = bank * 16 + i;
        if (e.slots[s].empty ()) { remove (d, n); continue; }
        if (! writeSlotWav (d, n, e, s)) return false;
    }
    d.flush ();
    return true;
}

bool loadKit (BlockDev& d, Engine& e, int bank, int kit) {
    if (bank < 0 || bank >= NBANK || kit < 0 || kit >= 99) return false;
    char n[13]; kitFile (kit, n);
    FileIn f; if (! f.open (d, n)) return false;
    In r { &f };
    if (r.u32 () != KITMAGIC) return false;
    Slot p[16]; bool has[16];
    for (int i = 0; i < 16; ++i) { has[i] = r.u8 (); getSlot (r, p[i]); }
    if (r.bad) return false;
    for (int i = 0; i < 16; ++i) e.clearSlot (bank * 16 + i);
    for (int i = 0; i < 16; ++i) {
        if (! has[i]) continue;
        kitWav (kit, i, n);
        const int s = bank * 16 + i;
        if (loadWav (d, n, e, s)) { copyParams (e.slots[s], p[i]); e.slots[s].saved = false; e.slots[s].mute = false; }
    }
    return true;
}

// ------------------------------------------------------------------ export: render the pattern chain or the song to a stereo WAV
bool exportWav (BlockDev& d, Engine& e, bool song, char* nameOut) {
    if (! mounted (d)) return false;
    char n[13] = "";
    for (int i = 1; i < 100; ++i) { std::snprintf (n, sizeof n, "EXP%02d.WAV", i); if (! exists (d, n)) break; }
    e.hold = true;
    while (e.inProcess) {}                     // the audio core finishes its block
    const uint32_t frames = e.renderStart (song);
    struct R { Engine* e; uint32_t frames, music, done; float buf[2 * 128]; int16_t pcm[2 * 128]; uint32_t have, at; } *rc = new R ();
    rc->e = &e; rc->frames = frames; rc->music = e.renderMusic; rc->done = 0; rc->have = 0; rc->at = 0;
    const bool ok = writeStream (d, n, 44 + frames * 4, [] (void* x, uint32_t off, uint8_t* b, uint32_t m) {
        R& r = *(R*) x;
        for (uint32_t i = 0; i < m; ++i) {
            const uint32_t o = off + i;
            if (o < 44) { uint8_t h[44]; wavHeader (h, r.frames, SR, 2); b[i] = h[o]; continue; }
            if (r.at >= r.have * 4) {           // render the next 128 frames
                if (r.done >= r.music && r.e->playing ()) r.e->play (false);   // the tail: let it ring out
                const uint32_t k = std::min<uint32_t> (r.done < r.music ? r.music - r.done : 128, std::min<uint32_t> (128, r.frames - r.done));
                r.e->processRaw (nullptr, r.buf, (int) k);
                for (uint32_t j = 0; j < 2 * k; ++j) r.pcm[j] = (int16_t) std::max (-32767.0f, std::min (32767.0f, r.buf[j] * 32767.0f));
                r.done += k; r.have = k; r.at = 0;
            }
            const uint16_t v = (uint16_t) r.pcm[r.at / 2];
            b[i] = (uint8_t) ((r.at & 1) ? v >> 8 : v); ++r.at;
        }
    }, rc);
    delete rc;
    e.renderEnd ();
    e.hold = false;
    if (nameOut) std::snprintf (nameOut, 13, "%s", n);
    return ok;
}

} // namespace store
} // namespace pocket

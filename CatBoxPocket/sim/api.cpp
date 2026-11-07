// Cat Box Pocket simulator: the real engine and UI compiled to WebAssembly for the browser.
// The page calls these functions; the audio, the keys and the screen go through them.
#include "../engine/pocket.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <new>
#include <initializer_list>

using namespace pocket;
#define API extern "C" __attribute__((visibility ("default")))

static int16_t pool[4u * 1024 * 1024];            // 8 MB like the PSRAM
static constexpr uint32_t DISK_N = 28672;
static uint8_t diskData[DISK_N * 512];           // 14 MB like the flash disk (kept out of the object: zero memory)
struct RamDisk : BlockDev {
    static constexpr uint32_t N = DISK_N;
    uint8_t* d = diskData;
    uint32_t sectors () const override { return N; }
    bool read (uint32_t s, uint8_t* b) override { if (s >= N) return false; std::memcpy (b, d + s * 512, 512); return true; }
    bool write (uint32_t s, const uint8_t* b) override { if (s >= N) return false; std::memcpy (d + s * 512, b, 512); return true; }
};
static RamDisk disk;
alignas (16) static uint8_t engineMem[sizeof (Engine)];
alignas (16) static uint8_t uiMem[sizeof (Ui)];
static Engine* E = nullptr; static Ui* U = nullptr;
static float inBuf[2048], outBuf[4096];
static uint8_t fb[1024], rgb[48];
static uint8_t xfer[1 << 20];                    // files in and out (1 MB)

static uint32_t seed = 1;
static float noise () { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (seed & 65535) / 32768.0f - 1; }
static int16_t s16 (double x) { return (int16_t) std::fmax (-32767, std::fmin (32767, x * 32767)); }

API void sim_init (int seedIn, int demo) {
    seed = (uint32_t) seedIn | 1;
    E = new (engineMem) Engine (); U = new (uiMem) Ui (*E);
    E->init (pool, sizeof (pool) / 2, (uint32_t) seedIn);
    U->disk = &disk;
    if (! store::mounted (disk)) store::format (disk, "CATPOCKET");
    if (demo) { factory::sounds (*E); factory::project (*E); }
}
API int sim_load_project (int p) { return store::load (disk, *E, *U, p); }
API float* sim_in () { return inBuf; }
API float* sim_out () { return outBuf; }
API void sim_process (int n) { E->process (inBuf, outBuf, n); }
API void sim_key (int k, int down) { U->key (k, down != 0); }
API void sim_encoder (int i, int d) { U->encoder (i, d); }
API void sim_fader (float x) { U->fader (x); }
API void sim_tick (uint32_t ms) { U->tick (ms); }
API int sim_service () {     // 1 = the disk changed (the page keeps a copy)
    const bool disky = U->wantSave || U->wantKit >= 0 || U->wantExport;
    U->service ();
    return disky ? 1 : 0;
}
API int sim_busy () { return U->wantSave || U->wantLoad || U->wantKit >= 0 || U->wantExport || U->wantBrowse || U->browseLoad >= 0; }
API uint8_t* sim_fb () { U->render (fb); return fb; }
API uint8_t* sim_leds () { U->leds (rgb); return rgb; }
API int sim_step () { return E->playing () ? E->step : -1; }
API int sim_playing () { return E->playing (); }
API float sim_level () { return E->level; }
API int sim_wants_usb () { const int w = U->wantUsb; U->wantUsb = false; return w; }
// files: the page puts bytes into xfer, then names them
API uint8_t* sim_xfer () { return xfer; }
API int sim_xfer_size () { return (int) sizeof (xfer); }
API int sim_write_file (const char* name, int len) { return store::writeFile (disk, name, xfer, (uint32_t) len); }
API int sim_read_file (const char* name) { uint32_t got = 0; return store::readFile (disk, name, xfer, sizeof (xfer), &got) ? (int) got : -1; }
API int sim_load_wav (const char* name, int slot) { return store::loadWavInto (disk, name, *E, slot); }
// raw mono samples straight into a pad (the page decodes any audio file)
static int16_t pcm[48000 * 20];
API int16_t* sim_pcm () { return pcm; }
API int sim_pcm_max () { return (int) (sizeof (pcm) / 2); }
API int sim_assign (int slot, int len, int rate) { const bool ok = E->assign (slot, pcm, (uint32_t) len, (uint32_t) rate); if (ok) E->noteOn (slot, slot); return ok; }
API int sim_sel () { return U->sel; }
API int sim_bank () { return U->bank; }
API uint8_t* sim_disk () { return disk.d; }
API int sim_disk_size () { return (int) sizeof (diskData); }
API int sim_list (char* out, int max) {        // names, one per line
    static char names[128][13]; const int n = store::list (disk, names, nullptr, 128);
    int k = 0; for (int i = 0; i < n; ++i) { const int l = (int) std::strlen (names[i]); if (k + l + 1 >= max) break; std::memcpy (out + k, names[i], l); k += l; out[k++] = '\n'; }
    out[k] = 0; return k;
}

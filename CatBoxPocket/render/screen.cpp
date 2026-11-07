#include "pocket.h"
#include <cmath>
#include <cstdio>
#include <vector>
using namespace pocket;
static std::vector<int16_t> pool (1 << 22);
int main () {
    Engine* e = new Engine (); e->init (pool.data (), (uint32_t) pool.size ());
    Ui* u = new Ui (*e);
    std::vector<int16_t> k (30000);
    for (int i = 0; i < 30000; ++i) { double t = i / 48000.0; k[i] = (int16_t) (26000 * std::sin (2 * M_PI * (45 * t + 90 * (1 - std::exp (-t * 30)) / 30)) * std::exp (-t * 7) + 3000 * std::sin (i * 0.9) * std::exp (-t * 40)); }
    e->assign (0, k.data (), 30000, SR);
    for (int s : {0, 4, 8, 12, 14}) e->toggleStep (s, 0);
    e->bpm = 124; e->slots[0].filter = -0.2f; e->slots[0].pitch = -2; u->sel = 0; u->page = 0;
    e->play (true); float o[256]; for (int i = 0; i < 100; ++i) e->process (nullptr, o, 64);
    uint8_t fb[1024]; u->tick (100000); u->render (fb);
    FILE* f = std::fopen ("screen.pgm", "wb"); std::fprintf (f, "P5 128 64 255\n");
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 128; ++x) std::fputc ((fb[(y >> 3) * 128 + x] >> (y & 7) & 1) ? 255 : 0, f);
    std::fclose (f);
}

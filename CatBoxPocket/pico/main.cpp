// Cat Box Pocket on the Pimoroni Pico Plus 2 (RP2350B, 8 MB PSRAM) with a Waveshare WM8960 module.
// Core 1 makes the sound (I2S over PIO + DMA), core 0 reads the keys, draws the OLED, drives the LEDs,
// saves to the flash disk and serves it over USB. All the music is in ../engine.
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/flash.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/xip_ctrl.h"
#include "tusb.h"
#include "i2s.pio.h"
#include "ws2812.pio.h"
#include "../engine/pocket.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <initializer_list>

using namespace pocket;

// ------------------------------------------------------------------ pins (see the wiring table in the design doc)
namespace pin {
constexpr uint LINE_DETECT = 0, LEDS = 2, ENC_B_PUSH = 3, SDA = 4, SCL = 5;
constexpr uint ROW0 = 6, COL0 = 11;                 // rows 6..10, columns 11..15
constexpr uint BCLK = 16, /* LRCLK = 17 */ DOUT = 18, DIN = 19, MCLK = 21;
constexpr uint ENC_A1 = 20, ENC_A2 = 22, ENC_B1 = 26, ENC_B2 = 27, BATTERY = 28;
constexpr uint PSRAM_CS = 47;
}
constexpr uint32_t SYS_KHZ = 144000;                // 144 MHz / 12 = 12 MHz MCLK for the WM8960 PLL
constexpr uint32_t DISK_OFFSET = 2u * 1024 * 1024;  // the flash disk starts at 2 MB
constexpr uint32_t DISK_SECTORS = (16u - 2u) * 1024 * 1024 / 512;

static Engine engine;
static Ui ui (engine);
static int16_t sramPool[64 * 1024];                  // fallback when the PSRAM does not answer (1.3 s)

// ------------------------------------------------------------------ PSRAM (QMI chip select 1)
static size_t __no_inline_not_in_flash_func (psramSetup) () {
    gpio_set_function (pin::PSRAM_CS, GPIO_FUNC_XIP_CS1);
    const uint32_t irq = save_and_disable_interrupts ();
    qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) {}
    // read the ID
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    uint8_t kgd = 0, eid = 0;
    for (int i = 0; i < 7; ++i) {
        qmi_hw->direct_tx = i == 0 ? 0x9F : 0xFF;
        while (! (qmi_hw->direct_csr & QMI_DIRECT_CSR_TXEMPTY_BITS)) {}
        while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) {}
        const uint8_t r = (uint8_t) qmi_hw->direct_rx;
        if (i == 5) kgd = r; else if (i == 6) eid = r;
    }
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);
    if (kgd != 0x5D) { restore_interrupts (irq); return 0; }
    size_t size = 1024 * 1024;
    const uint8_t sizeId = eid >> 5;
    if (eid == 0x26 || sizeId == 2) size *= 8; else if (sizeId == 0) size *= 2; else if (sizeId == 1) size *= 4;
    // reset enable, reset, quad mode
    qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) {}
    for (int i = 0; i < 3; ++i) {
        qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        qmi_hw->direct_tx = i == 0 ? 0x66 : i == 1 ? 0x99 : 0x35;
        while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) {}
        qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        for (int j = 0; j < 20; ++j) __asm volatile ("nop");
        (void) qmi_hw->direct_rx;
    }
    qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_EN_BITS;
    // timing for 133 MHz max
    const int sysHz = (int) clock_get_hz (clk_sys), maxHz = 133000000;
    int div = (sysHz + maxHz - 1) / maxHz; if (div == 1 && sysHz > 100000000) div = 2;
    int rxdelay = div; if (sysHz / div > 100000000) rxdelay += 1;
    const int periodFs = (int) (1000000000000000ll / sysHz);
    const int maxSelect = (125 * 1000000) / periodFs;
    const int minDeselect = (18 * 1000000 + (periodFs - 1)) / periodFs - (div + 1) / 2;
    qmi_hw->m[1].timing = 1u << QMI_M1_TIMING_COOLDOWN_LSB | QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB |
                          (uint32_t) maxSelect << QMI_M1_TIMING_MAX_SELECT_LSB | (uint32_t) minDeselect << QMI_M1_TIMING_MIN_DESELECT_LSB |
                          (uint32_t) rxdelay << QMI_M1_TIMING_RXDELAY_LSB | (uint32_t) div << QMI_M1_TIMING_CLKDIV_LSB;
    qmi_hw->m[1].rfmt = QMI_M0_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_PREFIX_WIDTH_LSB | QMI_M0_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M0_RFMT_ADDR_WIDTH_LSB |
                        QMI_M0_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_SUFFIX_WIDTH_LSB | QMI_M0_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M0_RFMT_DUMMY_WIDTH_LSB |
                        QMI_M0_RFMT_DATA_WIDTH_VALUE_Q << QMI_M0_RFMT_DATA_WIDTH_LSB | QMI_M0_RFMT_PREFIX_LEN_VALUE_8 << QMI_M0_RFMT_PREFIX_LEN_LSB |
                        6u << QMI_M0_RFMT_DUMMY_LEN_LSB;
    qmi_hw->m[1].rcmd = 0xEB;
    qmi_hw->m[1].wfmt = QMI_M0_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_PREFIX_WIDTH_LSB | QMI_M0_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M0_WFMT_ADDR_WIDTH_LSB |
                        QMI_M0_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_SUFFIX_WIDTH_LSB | QMI_M0_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M0_WFMT_DUMMY_WIDTH_LSB |
                        QMI_M0_WFMT_DATA_WIDTH_VALUE_Q << QMI_M0_WFMT_DATA_WIDTH_LSB | QMI_M0_WFMT_PREFIX_LEN_VALUE_8 << QMI_M0_WFMT_PREFIX_LEN_LSB;
    qmi_hw->m[1].wcmd = 0x38;
    restore_interrupts (irq);
    hw_set_bits (&xip_ctrl_hw->ctrl, XIP_CTRL_WRITABLE_M1_BITS);
    return size;
}

static bool psramTest (volatile uint32_t* p, size_t words) {
    for (size_t i = 0; i < words; i += 4097) p[i] = (uint32_t) (i * 2654435761u);
    for (size_t i = 0; i < words; i += 4097) if (p[i] != (uint32_t) (i * 2654435761u)) return false;
    return true;
}

// ------------------------------------------------------------------ the flash disk (4 KB erase blocks, cached)
struct FlashDisk : BlockDev {
    uint8_t cache[FLASH_SECTOR_SIZE]; int32_t cached = -1; bool dirty = false;
    uint32_t sectors () const override { return DISK_SECTORS; }
    static const uint8_t* xip (uint32_t off) { return (const uint8_t*) (XIP_BASE + DISK_OFFSET + off); }
    bool read (uint32_t s, uint8_t* b) override {
        if (s >= DISK_SECTORS) return false;
        const uint32_t off = s * 512, blk = off / FLASH_SECTOR_SIZE;
        if ((int32_t) blk == cached) std::memcpy (b, cache + off % FLASH_SECTOR_SIZE, 512);
        else std::memcpy (b, xip (off), 512);
        return true;
    }
    bool write (uint32_t s, const uint8_t* b) override {
        if (s >= DISK_SECTORS) return false;
        const uint32_t off = s * 512, blk = off / FLASH_SECTOR_SIZE;
        if ((int32_t) blk != cached) { flush (); std::memcpy (cache, xip (blk * FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE); cached = (int32_t) blk; }
        if (std::memcmp (cache + off % FLASH_SECTOR_SIZE, b, 512) == 0) return true;
        std::memcpy (cache + off % FLASH_SECTOR_SIZE, b, 512); dirty = true;
        return true;
    }
    static void program (void* p) {
        FlashDisk& d = *(FlashDisk*) p;
        // the flash routines may touch the QMI; keep the PSRAM window as it was
        const uint32_t t = qmi_hw->m[1].timing, rf = qmi_hw->m[1].rfmt, rc = qmi_hw->m[1].rcmd, wf = qmi_hw->m[1].wfmt, wc = qmi_hw->m[1].wcmd;
        const uint32_t off = DISK_OFFSET + (uint32_t) d.cached * FLASH_SECTOR_SIZE;
        flash_range_erase (off, FLASH_SECTOR_SIZE);
        flash_range_program (off, d.cache, FLASH_SECTOR_SIZE);
        qmi_hw->m[1].timing = t; qmi_hw->m[1].rfmt = rf; qmi_hw->m[1].rcmd = rc; qmi_hw->m[1].wfmt = wf; qmi_hw->m[1].wcmd = wc;
    }
    void flush () override {
        if (! dirty || cached < 0) return;
        flash_safe_execute (program, this, 1000);
        dirty = false;
    }
};
static FlashDisk disk;

// ------------------------------------------------------------------ WM8960 (I2C 0x1A, 9-bit registers)
static void wm (uint8_t reg, uint16_t val) {
    const uint8_t b[2] = { (uint8_t) (reg << 1 | (val >> 8 & 1)), (uint8_t) (val & 0xff) };
    i2c_write_blocking (i2c0, 0x1A, b, 2, false);
}
static void codecInit () {
    wm (0x0F, 0x000);                 // reset
    sleep_ms (10);
    wm (0x19, 0x0FE);                 // power 1: VMID 50k, VREF, AINL/R, ADCL/R, MICBIAS
    wm (0x1A, 0x1F9);                 // power 2: DACL/R, LOUT1/ROUT1, SPKL/R, PLL
    wm (0x2F, 0x03C);                 // power 3: LMIC, RMIC, LOMIX, ROMIX
    // clock: MCLK 12 MHz -> PLL -> SYSCLK 12.288 MHz (48 kHz), slave I2S 16 bit
    wm (0x34, 0x028); wm (0x35, 0x031); wm (0x36, 0x026); wm (0x37, 0x0E9);
    wm (0x04, 0x005);                 // SYSCLK from the PLL, /2
    wm (0x07, 0x002);                 // I2S, 16 bit, slave
    wm (0x05, 0x000);                 // DAC unmute
    wm (0x0A, 0x1FF); wm (0x0B, 0x1FF);   // DAC volume 0 dB
    wm (0x22, 0x100); wm (0x25, 0x100);   // DAC -> output mixers
    wm (0x02, 0x16F); wm (0x03, 0x16F);   // headphones
    wm (0x28, 0x170); wm (0x29, 0x170);   // speakers
    wm (0x31, 0x0F7);                 // class D on, both speakers
    wm (0x33, 0x09B);                 // speaker boost
    // inputs: microphone (LINPUT1 / RINPUT1) through the boost mixers, +20 dB
    wm (0x20, 0x128); wm (0x21, 0x128);
    wm (0x00, 0x11F); wm (0x01, 0x11F);   // input PGA +6 dB, unmuted
    wm (0x15, 0x1C3); wm (0x16, 0x1C3);   // ADC volume 0 dB
}

// ------------------------------------------------------------------ OLED (SSD1306, I2C 0x3C)
static void oledCmd (std::initializer_list<uint8_t> c) {
    uint8_t b[16]; b[0] = 0x00; int n = 1; for (uint8_t x : c) b[n++] = x;
    i2c_write_blocking (i2c0, 0x3C, b, n, false);
}
static void oledInit () {
    oledCmd ({0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8});
    oledCmd ({0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF});
}
static void oledShow (const uint8_t* fb) {
    oledCmd ({0x21, 0, 127, 0x22, 0, 7});
    static uint8_t b[129];
    for (int i = 0; i < 1024; i += 128) { b[0] = 0x40; std::memcpy (b + 1, fb + i, 128); i2c_write_blocking (i2c0, 0x3C, b, 129, false); }
}

// ------------------------------------------------------------------ audio: I2S duplex with DMA ping-pong, on core 1
constexpr int N = 64;
static uint32_t txb[2][N], rxb[2][N];
static int dTx[2], dRx[2];
static PIO audioPio = pio0; static uint audioSm = 0;
static float inF[N], outF[2 * N];

static void __not_in_flash_func (audioIrq) () {
    for (int i = 0; i < 2; ++i) {
        if (! (dma_hw->ints1 & (1u << dTx[i]))) continue;
        dma_hw->ints1 = 1u << dTx[i];
        dma_channel_set_read_addr (dTx[i], txb[i], false);
        dma_channel_set_write_addr (dRx[i], rxb[i], false);
        for (int k = 0; k < N; ++k) {
            const int16_t l = (int16_t) (rxb[i][k] & 0xffff), r = (int16_t) (rxb[i][k] >> 16);
            inF[k] = (l + r) * (1.0f / 65536.0f);
        }
        engine.process (inF, outF, N);
        for (int k = 0; k < N; ++k) {
            const int l = (int) (outF[2 * k] * 32767.0f), r = (int) (outF[2 * k + 1] * 32767.0f);
            txb[i][k] = (uint32_t) (uint16_t) r << 16 | (uint16_t) l;
        }
    }
}

static void audioStart () {
    const uint off = pio_add_program (audioPio, &i2s_duplex_program);
    const float div = (float) clock_get_hz (clk_sys) / (SR * 32.0f * 4.0f);
    i2s_duplex_init (audioPio, audioSm, off, pin::DOUT, pin::DIN, pin::BCLK, div);
    for (int i = 0; i < 2; ++i) { dTx[i] = dma_claim_unused_channel (true); dRx[i] = dma_claim_unused_channel (true); }
    for (int i = 0; i < 2; ++i) {
        dma_channel_config c = dma_channel_get_default_config (dTx[i]);
        channel_config_set_transfer_data_size (&c, DMA_SIZE_32);
        channel_config_set_read_increment (&c, true); channel_config_set_write_increment (&c, false);
        channel_config_set_dreq (&c, pio_get_dreq (audioPio, audioSm, true));
        channel_config_set_chain_to (&c, dTx[1 - i]);
        dma_channel_configure (dTx[i], &c, &audioPio->txf[audioSm], txb[i], N, false);
        dma_channel_config r = dma_channel_get_default_config (dRx[i]);
        channel_config_set_transfer_data_size (&r, DMA_SIZE_32);
        channel_config_set_read_increment (&r, false); channel_config_set_write_increment (&r, true);
        channel_config_set_dreq (&r, pio_get_dreq (audioPio, audioSm, false));
        channel_config_set_chain_to (&r, dRx[1 - i]);
        dma_channel_configure (dRx[i], &r, rxb[i], &audioPio->rxf[audioSm], N, false);
        dma_channel_set_irq1_enabled (dTx[i], true);
    }
    irq_set_exclusive_handler (DMA_IRQ_1, audioIrq);
    irq_set_enabled (DMA_IRQ_1, true);
    dma_start_channel_mask (1u << dTx[0] | 1u << dRx[0]);
    pio_sm_set_enabled (audioPio, audioSm, true);
}

static void core1Main () {
    multicore_lockout_victim_init ();     // the flash writes pause this core safely
    audioStart ();
    for (;;) __wfi ();
}

// ------------------------------------------------------------------ keys (5x5 matrix + one push), encoders, LEDs
static const uint8_t MATRIX[5][5] = {
    {K_PAD0 + 0,  K_PAD0 + 1,  K_PAD0 + 2,  K_PAD0 + 3,  K_SOUND},
    {K_PAD0 + 4,  K_PAD0 + 5,  K_PAD0 + 6,  K_PAD0 + 7,  K_PATTERN},
    {K_PAD0 + 8,  K_PAD0 + 9,  K_PAD0 + 10, K_PAD0 + 11, K_BPM},
    {K_PAD0 + 12, K_PAD0 + 13, K_PAD0 + 14, K_PAD0 + 15, K_FX},
    {K_SHIFT,     K_REC,       K_PLAY,      K_WRITE,     K_ENCA}};
static bool keyState[NKEY]; static uint8_t keyCount[NKEY];

static void keysInit () {
    for (uint r = 0; r < 5; ++r) { gpio_init (pin::ROW0 + r); gpio_set_dir (pin::ROW0 + r, GPIO_IN); gpio_disable_pulls (pin::ROW0 + r); }
    for (uint c = 0; c < 5; ++c) { gpio_init (pin::COL0 + c); gpio_set_dir (pin::COL0 + c, GPIO_IN); gpio_pull_up (pin::COL0 + c); }
    for (uint p : {pin::ENC_B_PUSH, pin::ENC_A1, pin::ENC_A2, pin::ENC_B1, pin::ENC_B2, pin::LINE_DETECT}) { gpio_init (p); gpio_set_dir (p, GPIO_IN); gpio_pull_up (p); }
}

static void keysScan () {
    bool now[NKEY] = {false};
    for (uint r = 0; r < 5; ++r) {
        // drive one row low (the others float), read the columns
        gpio_set_dir (pin::ROW0 + r, GPIO_OUT); gpio_put (pin::ROW0 + r, 0);
        busy_wait_us_32 (5);
        for (uint c = 0; c < 5; ++c) if (! gpio_get (pin::COL0 + c)) now[MATRIX[r][c]] = true;
        gpio_set_dir (pin::ROW0 + r, GPIO_IN);
    }
    now[K_ENCB] = ! gpio_get (pin::ENC_B_PUSH);
    for (int k = 0; k < NKEY; ++k) {
        // debounce: 4 equal scans (1 ms apart)
        if (now[k] == keyState[k]) { keyCount[k] = 0; continue; }
        if (++keyCount[k] >= 4) { keyState[k] = now[k]; keyCount[k] = 0; ui.key (k, now[k]); }
    }
}

struct Encoder { uint a, b; uint8_t last = 0; int acc = 0; };
static Encoder encA { pin::ENC_A1, pin::ENC_A2 }, encB { pin::ENC_B1, pin::ENC_B2 };
static int encRead (Encoder& e) {
    static const int8_t T[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    const uint8_t s = (uint8_t) ((gpio_get (e.a) ? 2 : 0) | (gpio_get (e.b) ? 1 : 0));
    e.acc += T[e.last << 2 | s]; e.last = s;
    if (e.acc >= 4) { e.acc -= 4; return 1; }
    if (e.acc <= -4) { e.acc += 4; return -1; }
    return 0;
}

static PIO ledPio = pio0; static uint ledSm = 1;
static void ledsShow (const uint8_t* rgb) {
    for (int i = 0; i < 16; ++i) {
        const uint32_t g = rgb[i * 3 + 1] / 2, r = rgb[i * 3] / 2, b = rgb[i * 3 + 2] / 2;   // half brightness
        pio_sm_put_blocking (ledPio, ledSm, (g << 16 | r << 8 | b) << 8);
    }
}

static int batteryPercent () {
    adc_select_input (2);
    uint32_t s = 0; for (int i = 0; i < 8; ++i) s += adc_read ();
    const float v = s / 8.0f * 3.3f / 4095.0f * 2.0f;       // 100k / 100k divider
    const int p = (int) std::lround ((v - 3.3f) / (4.15f - 3.3f) * 100.0f);
    return p < 0 ? 0 : (p > 100 ? 100 : p);
}

// ------------------------------------------------------------------ USB disk (mass storage)
static bool usbReload = false;
extern "C" {
void tud_msc_inquiry_cb (uint8_t, uint8_t vendor[8], uint8_t product[16], uint8_t rev[4]) {
    std::memcpy (vendor, "CatBox  ", 8); std::memcpy (product, "Pocket Disk     ", 16); std::memcpy (rev, "1.0 ", 4);
}
bool tud_msc_test_unit_ready_cb (uint8_t) {
    if (! ui.usbDisk) { tud_msc_set_sense (0, SCSI_SENSE_NOT_READY, 0x3A, 0x00); return false; }
    return true;
}
void tud_msc_capacity_cb (uint8_t, uint32_t* count, uint16_t* size) { *count = DISK_SECTORS; *size = 512; }
bool tud_msc_start_stop_cb (uint8_t, uint8_t, bool start, bool load_eject) {
    if (load_eject && ! start) { disk.flush (); usbReload = true; }
    return true;
}
int32_t tud_msc_read10_cb (uint8_t, uint32_t lba, uint32_t offset, void* buf, uint32_t bufsize) {
    uint8_t* p = (uint8_t*) buf; uint8_t tmp[512];
    for (uint32_t done = 0; done < bufsize; ) {
        const uint32_t s = lba + (offset + done) / 512, o = (offset + done) % 512, n = std::min<uint32_t> (512 - o, bufsize - done);
        if (! disk.read (s, tmp)) return -1;
        std::memcpy (p + done, tmp + o, n); done += n;
    }
    return (int32_t) bufsize;
}
int32_t tud_msc_write10_cb (uint8_t, uint32_t lba, uint32_t offset, uint8_t* buf, uint32_t bufsize) {
    uint8_t tmp[512];
    for (uint32_t done = 0; done < bufsize; ) {
        const uint32_t s = lba + (offset + done) / 512, o = (offset + done) % 512, n = std::min<uint32_t> (512 - o, bufsize - done);
        disk.read (s, tmp); std::memcpy (tmp + o, buf + done, n);
        if (! disk.write (s, tmp)) return -1;
        done += n;
    }
    return (int32_t) bufsize;
}
void tud_msc_write10_complete_cb (uint8_t) {}
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void*, uint16_t) {
    if (scsi_cmd[0] == SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL) return 0;
    tud_msc_set_sense (lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}
}

// ------------------------------------------------------------------ main (core 0)
int main () {
    set_sys_clock_khz (SYS_KHZ, true);
    // the sample memory
    const size_t psram = psramSetup ();
    int16_t* pool = sramPool; uint32_t poolLen = sizeof (sramPool) / 2;
    if (psram >= 1024 * 1024 && psramTest ((volatile uint32_t*) 0x11000000, psram / 4)) { pool = (int16_t*) 0x11000000; poolLen = (uint32_t) (psram / 2); }
    engine.init (pool, poolLen, time_us_32 ());

    // buses and parts
    i2c_init (i2c0, 400 * 1000);
    gpio_set_function (pin::SDA, GPIO_FUNC_I2C); gpio_set_function (pin::SCL, GPIO_FUNC_I2C);
    gpio_pull_up (pin::SDA); gpio_pull_up (pin::SCL);
    clock_gpio_init (pin::MCLK, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, 12);
    sleep_ms (5);
    codecInit ();
    oledInit ();
    keysInit ();
    adc_init (); adc_gpio_init (pin::BATTERY);
    ws2812_init (ledPio, ledSm, pio_add_program (ledPio, &ws2812_program), pin::LEDS, (float) clock_get_hz (clk_sys));

    // SHIFT held while switching on = USB disk
    for (int i = 0; i < 30; ++i) { keysScan (); sleep_ms (1); }
    ui.usbDisk = keyState[K_SHIFT];

    // the flash disk: format it the first time, then load the project
    uint8_t fb[1024];
    if (! store::mounted (disk)) { ui.say ("PRVE ZAPNUTIE...", 3000); ui.render (fb); oledShow (fb); store::format (disk); }
    if (! ui.usbDisk) store::load (disk, engine, ui);
    if (pool == sramPool) ui.say ("PSRAM CHYBA!", 4000);

    tusb_init ();
    multicore_launch_core1 (core1Main);

    uint32_t lastScreen = 0, lastLeds = 0, lastBat = 0, lastScan = 0;
    for (;;) {
        tud_task ();
        const uint32_t ms = to_ms_since_boot (get_absolute_time ());
        if (ms != lastScan) {
            lastScan = ms;
            keysScan ();
            if (const int d = encRead (encA)) ui.encoder (0, d);
            if (const int d = encRead (encB)) ui.encoder (1, d);
            ui.lineIn = ! gpio_get (pin::LINE_DETECT);
            ui.tick (ms);
        }
        if (ui.usbDisk && (usbReload || (keyState[K_PLAY]))) {
            ui.usbDisk = false; usbReload = false;
            store::load (disk, engine, ui); ui.say ("NACITANE");
        }
        if (ui.wantSave) {
            ui.wantSave = false;
            const bool ok = store::save (disk, engine, ui); disk.flush ();
            ui.say (ok ? "ULOZENE" : "PLNY DISK!");
        }
        if (ms - lastBat > 5000) { lastBat = ms; ui.battery = batteryPercent (); }
        if (ms - lastScreen >= 50) { lastScreen = ms; ui.render (fb); oledShow (fb); }
        if (ms - lastLeds >= 20) { lastLeds = ms; uint8_t rgb[48]; ui.leds (rgb); ledsShow (rgb); }
    }
}

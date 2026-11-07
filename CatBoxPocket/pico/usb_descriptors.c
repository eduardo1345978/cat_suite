// USB descriptors: a mass storage interface ("Pocket Disk") and a MIDI interface ("Cat Box Pocket").
#include "tusb.h"
#include <string.h>

static const tusb_desc_device_t dev = {
    .bLength = sizeof (tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE, .bcdUSB = 0x0200,
    .bDeviceClass = 0x00, .bDeviceSubClass = 0x00, .bDeviceProtocol = 0x00, .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x2E8A, .idProduct = 0x10C7, .bcdDevice = 0x0200,
    .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1 };

uint8_t const* tud_descriptor_device_cb (void) { return (uint8_t const*) &dev; }

#define EPNUM_MSC_OUT  0x01
#define EPNUM_MSC_IN   0x81
#define EPNUM_MIDI_OUT 0x02
#define EPNUM_MIDI_IN  0x82
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN + TUD_MIDI_DESC_LEN)
static const uint8_t cfg[] = {
    TUD_CONFIG_DESCRIPTOR (1, 3, 0, CONFIG_TOTAL_LEN, 0x00, 250),
    TUD_MSC_DESCRIPTOR (0, 4, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
    TUD_MIDI_DESCRIPTOR (1, 2, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, 64) };
uint8_t const* tud_descriptor_configuration_cb (uint8_t index) { (void) index; return cfg; }

static const char* strs[] = { "", "Cat Box", "Cat Box Pocket", "0001", "Pocket Disk" };
static uint16_t desc[32];
uint16_t const* tud_descriptor_string_cb (uint8_t index, uint16_t langid) {
    (void) langid;
    uint8_t n;
    if (index == 0) { desc[1] = 0x0409; n = 1; }
    else {
        if (index >= sizeof (strs) / sizeof (strs[0])) return NULL;
        const char* s = strs[index]; n = (uint8_t) strlen (s); if (n > 31) n = 31;
        for (uint8_t i = 0; i < n; ++i) desc[1 + i] = (uint16_t) s[i];
    }
    desc[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * n + 2));
    return desc;
}

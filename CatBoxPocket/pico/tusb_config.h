// TinyUSB: Cat Box Pocket is a USB disk (mass storage) and a USB MIDI device at the same time.
#pragma once
#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_DEVICE
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS             OPT_OS_PICO
#endif
#define CFG_TUD_ENDPOINT0_SIZE  64
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             1
#define CFG_TUD_HID             0
#define CFG_TUD_MIDI            1
#define CFG_TUD_VENDOR          0
#define CFG_TUD_MSC_EP_BUFSIZE  512
#define CFG_TUD_MIDI_RX_BUFSIZE 64
#define CFG_TUD_MIDI_TX_BUFSIZE 64

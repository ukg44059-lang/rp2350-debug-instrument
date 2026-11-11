#pragma once
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_NONE
#endif
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define CFG_TUD_ENABLED 1
#define CFG_TUD_MAX_SPEED OPT_MODE_FULL_SPEED
#define CFG_TUD_ENDPOINT0_SIZE 64
#define CFG_TUD_CDC 1
#define CFG_TUD_CDC_RX_BUFSIZE 64
#define CFG_TUD_CDC_TX_BUFSIZE 128
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 2
#define CFG_TUD_VENDOR_RX_BUFSIZE 64
// Direct packet writes: suppress stream-style automatic ZLPs after 64-byte DAP replies.
#define CFG_TUD_VENDOR_TX_BUFSIZE 0
// Transfer buffer, NOT the descriptor's wMaxPacketSize (still 64 bytes).
// Large IN transfers let the RP2350 driver double-buffer USB packets without
// waiting for the application task after every 64-byte packet. Keep FIFO off
// to preserve existing CMSIS-DAP framing and suppress automatic ZLPs.
#define CFG_TUD_VENDOR_EPSIZE 4160
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))

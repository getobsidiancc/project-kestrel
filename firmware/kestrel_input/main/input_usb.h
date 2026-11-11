// Project Kestrel - USB host input (mouse + keyboard through a hub)
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int32_t  dx, dy;        // mouse counts since the last snapshot
    int32_t  wheel;         // wheel clicks since the last snapshot (boot mice may not report it)
    uint8_t  mouse_buttons; // bit0 left, bit1 right, bit2 middle, bit3 back, bit4 forward
    uint8_t  keys[32];      // bitmap of pressed HID usage IDs (0x00-0xFF), modifiers at 0xE0-0xE7
    uint8_t  mice;          // connected mouse interfaces
    uint8_t  keyboards;     // connected keyboard interfaces
} input_snapshot_t;

// Start the USB host stack and HID driver.
void input_usb_start(void);

// Called right after every keyboard/mouse report is stored, so the main loop can process it at
// once instead of waiting for its next 1 ms tick.
void input_usb_set_report_cb(void (*cb)(void));

// USB host channel sharing with the Xbox auth controller (the S3 only has 8 channels):
// close all keyboard interfaces (blocks until done) ...
void input_usb_suspend_keyboards(void);
// ... and reopen anything that was closed or couldn't get a channel.
void input_usb_resume_parked(void);

// Copy current input and reset the motion accumulators. Safe from any task.
void input_usb_snapshot(input_snapshot_t *out);

static inline bool input_key_down(const input_snapshot_t *s, uint8_t usage)
{
    return (s->keys[usage >> 3] >> (usage & 7)) & 1;
}

// Project Kestrel - chip-to-chip link protocol
//
// The MAKCU has two ESP32-S3s joined by a UART:
//   RIGHT chip (input side)  : USB host  -> reads mouse/keyboard, runs aim translation
//   LEFT  chip (output side) : USB device -> shows up as a controller to the PC/console
//
// Frame layout on the wire:
//   0xA5 0x5A | type (1) | len (1) | payload (len bytes) | crc16 (2, little endian)
// CRC is CRC-16/CCITT-FALSE over type, len and payload.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- Wiring -------------------------------------------------------------
// Matches the open-source MAKCM firmware: UART1, GPIO1 <-> GPIO1 and GPIO2 <-> GPIO2.
// LEFT chip receives on GPIO1 and transmits on GPIO2; RIGHT chip is the reverse.
// If your board is wired differently, change these and rebuild both sides.
#define KLINK_UART_NUM        UART_NUM_1
#define KLINK_BAUD            2000000
#define KLINK_LEFT_RX_PIN     1
#define KLINK_LEFT_TX_PIN     2
#define KLINK_RIGHT_RX_PIN    2
#define KLINK_RIGHT_TX_PIN    1

#define KLINK_SOF0            0xA5
#define KLINK_SOF1            0x5A
#define KLINK_MAX_PAYLOAD     64

// ---- Message types --------------------------------------------------------
typedef enum {
    KLINK_MSG_PAD_STATE   = 0x01,   // RIGHT -> LEFT : controller state to send to the console
    KLINK_MSG_STATUS      = 0x02,   // LEFT  -> RIGHT: USB mounted, etc.
    KLINK_MSG_PING        = 0x03,   // either way    : link check
    KLINK_MSG_INPUT_STATUS = 0x04,  // RIGHT -> LEFT : connected mice/keyboards (for the log)
    KLINK_MSG_LOG         = 0x05,   // RIGHT -> LEFT : RIGHT chip's log text, printed by LEFT
    // Xbox One auth relay (raw GIP packets, up to 64 bytes)
    KLINK_MSG_GIP_TO_DEVICE  = 0x10, // LEFT  -> RIGHT: console's auth packet, for the real controller
    KLINK_MSG_GIP_TO_CONSOLE = 0x11, // RIGHT -> LEFT : real controller's auth packet, for the console
    KLINK_MSG_AUTH_DEV_STATUS = 0x12,// RIGHT -> LEFT : auth controller state

    // Config app (PC <-> LEFT's COM port <-> link <-> RIGHT, which owns the settings)
    KLINK_MSG_CFG_GET      = 0x20,  // LEFT  -> RIGHT: send me the settings
    KLINK_MSG_CFG_DATA     = 0x21,  // RIGHT -> LEFT : [offset u16 LE][bytes...]
    KLINK_MSG_CFG_END      = 0x22,  // RIGHT -> LEFT : [total length u16 LE]
    KLINK_MSG_CFG_SET      = 0x23,  // LEFT  -> RIGHT: [offset u16 LE][bytes...]
    KLINK_MSG_CFG_COMMIT   = 0x24,  // LEFT  -> RIGHT: [total length u16 LE] apply + save
    KLINK_MSG_CFG_DEFAULTS = 0x25,  // LEFT  -> RIGHT: reset to defaults + save
    KLINK_MSG_CFG_RESULT   = 0x26,  // RIGHT -> LEFT : [code] 0 ok, 1 invalid, 2 save failed
    KLINK_MSG_CAL          = 0x27,  // LEFT  -> RIGHT: [int16 stick x 1/1000, <0 = off]
    KLINK_MSG_LIVE         = 0x28,  // LEFT  -> RIGHT: [on] stream pressed keys
    KLINK_MSG_KEYS         = 0x29,  // RIGHT -> LEFT : [mouse buttons][count][usages...]
} klink_msg_t;

typedef struct __attribute__((packed)) {
    uint8_t  mounted;   // an Xbox One controller is plugged into the hub
    uint8_t  ready;     // it finished its start-up handshake and can answer auth
    uint16_t vid, pid;
} auth_dev_status_t;

// ---- Controller state (XInput button layout) -----------------------------
#define PAD_DPAD_UP     0x0001
#define PAD_DPAD_DOWN   0x0002
#define PAD_DPAD_LEFT   0x0004
#define PAD_DPAD_RIGHT  0x0008
#define PAD_START       0x0010   // Menu
#define PAD_BACK        0x0020   // View
#define PAD_LS          0x0040   // left stick click
#define PAD_RS          0x0080   // right stick click
#define PAD_LB          0x0100
#define PAD_RB          0x0200
#define PAD_GUIDE       0x0400   // Xbox button
#define PAD_A           0x1000
#define PAD_B           0x2000
#define PAD_X           0x4000
#define PAD_Y           0x8000

typedef struct __attribute__((packed)) {
    uint16_t buttons;   // PAD_* bits
    uint8_t  lt;        // 0-255
    uint8_t  rt;        // 0-255
    int16_t  lx, ly;    // -32767..32767, +y = up
    int16_t  rx, ry;    // -32767..32767, +y = up
    uint8_t  seq;       // increments every frame
} pad_state_t;

typedef struct __attribute__((packed)) {
    uint8_t usb_mounted;    // 1 when the PC/console has configured the controller
    uint8_t fw_version;
    uint8_t auth_done;      // Xbox One mode: console accepted us (or host needs no auth)
    uint8_t want_auth;      // Xbox One mode: console is waiting to authenticate
} link_status_t;

typedef struct __attribute__((packed)) {
    uint8_t mice;
    uint8_t keyboards;
    uint8_t fw_version;
} input_status_t;

// ---- API -------------------------------------------------------------------
typedef void (*klink_rx_cb_t)(uint8_t type, const uint8_t *payload, uint8_t len, void *arg);

// Start the UART and a receive task. is_left selects the pin set.
esp_err_t klink_init(bool is_left, klink_rx_cb_t cb, void *cb_arg);

// Send one frame. Safe to call from any task (not from ISR).
esp_err_t klink_send(uint8_t type, const void *payload, uint8_t len);

// Counters for diagnostics.
uint32_t klink_rx_frames(void);
uint32_t klink_rx_errors(void);

#ifdef __cplusplus
}
#endif

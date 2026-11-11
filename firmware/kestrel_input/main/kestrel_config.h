// Project Kestrel - FACTORY DEFAULTS
//
// Your live settings are stored on the adapter and edited with the config app
// ("Kestrel Config.bat"). These values are only used the first time, or after
// "Reset to defaults" in the app.

#pragma once

#include "kestrel_link.h"
#include "kestrel_cfg.h"

// HID keyboard usage IDs (USB HID Usage Tables, keyboard page 0x07)
#define K_A 0x04
#define K_C 0x06
#define K_D 0x07
#define K_E 0x08
#define K_F 0x09
#define K_G 0x0A
#define K_Q 0x14
#define K_R 0x15
#define K_S 0x16
#define K_V 0x19
#define K_W 0x1A
#define K_Z 0x1D
#define K_1 0x1E
#define K_2 0x1F
#define K_3 0x20
#define K_4 0x21
#define K_ESC   0x29
#define K_TAB   0x2B
#define K_SPACE 0x2C
#define K_HOME  0x4A
#define K_RIGHT 0x4F
#define K_LEFT  0x50
#define K_DOWN  0x51
#define K_UP    0x52
#define K_LCTRL  0xE0
#define K_LSHIFT 0xE1
#define K_LALT   0xE2

#define PADBIT_DPAD_UP    0
#define PADBIT_DPAD_DOWN  1
#define PADBIT_DPAD_LEFT  2
#define PADBIT_DPAD_RIGHT 3
#define PADBIT_START      4
#define PADBIT_BACK       5
#define PADBIT_LS         6
#define PADBIT_RS         7
#define PADBIT_LB         8
#define PADBIT_RB         9
#define PADBIT_GUIDE      10
#define PADBIT_A          12
#define PADBIT_B          13
#define PADBIT_X          14
#define PADBIT_Y          15


// Default aim: a neutral linear game (360 deg/s at full stick, 10% deadzone).
// Calibrate per game in the app to replace this.
#define KESTREL_DEFAULT_AIM {                                        \
    .cm_per_360 = 30.0f,                                             \
    .y_ratio = 1.0f,                                                 \
    .npoints = 6,                                                    \
    .defl = { 0.10f, 0.28f, 0.46f, 0.64f, 0.82f, 1.00f },            \
    .dps  = { 0.0f, 72.0f, 144.0f, 216.0f, 288.0f, 360.0f },         \
}

static const kcfg_t KESTREL_FACTORY_DEFAULTS = {
    .magic = KCFG_MAGIC,
    .version = KCFG_VERSION,
    .size = sizeof(kcfg_t),

    .mouse_dpi = 800.0f,
    .smoothing = 0.0f,
    .invert_y = 0,
    .dz_shape = 0,
    .window_ms = 8,
    .walk_scale_pct = 50,
    .wheel_hold_ms = 40,
    .ads_enabled = 0,

    .hip = KESTREL_DEFAULT_AIM,
    .ads = KESTREL_DEFAULT_AIM,

    .binds = {
        { K_W,           KTGT_MOVE_UP, 0 },
        { K_S,           KTGT_MOVE_DOWN, 0 },
        { K_A,           KTGT_MOVE_LEFT, 0 },
        { K_D,           KTGT_MOVE_RIGHT, 0 },
        { K_LALT,        KTGT_WALK, 0 },

        { KSRC_MOUSE(0), KTGT_RT, 0 },                     // fire
        { KSRC_MOUSE(1), KTGT_LT, 0 },                     // aim
        { KSRC_MOUSE(2), KTGT_BUTTON(PADBIT_RS), 0 },      // melee
        { KSRC_MOUSE(3), KTGT_BUTTON(PADBIT_LB), 0 },
        { KSRC_MOUSE(4), KTGT_BUTTON(PADBIT_RB), 0 },
        { KSRC_WHEEL_UP,   KTGT_BUTTON(PADBIT_Y), 0 },
        { KSRC_WHEEL_DOWN, KTGT_BUTTON(PADBIT_Y), 0 },

        { K_SPACE,  KTGT_BUTTON(PADBIT_A), 0 },
        { K_LCTRL,  KTGT_BUTTON(PADBIT_B), 0 },
        { K_C,      KTGT_BUTTON(PADBIT_B), 0 },
        { K_R,      KTGT_BUTTON(PADBIT_X), 0 },
        { K_F,      KTGT_BUTTON(PADBIT_X), 0 },
        { K_1,      KTGT_BUTTON(PADBIT_Y), 0 },
        { K_2,      KTGT_BUTTON(PADBIT_Y), 0 },
        { K_Q,      KTGT_BUTTON(PADBIT_LB), 0 },
        { K_E,      KTGT_BUTTON(PADBIT_RB), 0 },
        { K_LSHIFT, KTGT_BUTTON(PADBIT_LS), 0 },
        { K_V,      KTGT_BUTTON(PADBIT_RS), 0 },
        { K_TAB,    KTGT_BUTTON(PADBIT_BACK), 0 },
        { K_ESC,    KTGT_BUTTON(PADBIT_START), 0 },
        { K_HOME,   KTGT_BUTTON(PADBIT_GUIDE), 0 },
        { K_UP,     KTGT_BUTTON(PADBIT_DPAD_UP), 0 },
        { K_DOWN,   KTGT_BUTTON(PADBIT_DPAD_DOWN), 0 },
        { K_LEFT,   KTGT_BUTTON(PADBIT_DPAD_LEFT), 0 },
        { K_RIGHT,  KTGT_BUTTON(PADBIT_DPAD_RIGHT), 0 },
        { K_3,      KTGT_BUTTON(PADBIT_DPAD_LEFT), 0 },
        { K_4,      KTGT_BUTTON(PADBIT_DPAD_RIGHT), 0 },
        { K_G,      KTGT_BUTTON(PADBIT_DPAD_DOWN), 0 },
        { K_Z,      KTGT_BUTTON(PADBIT_DPAD_UP), 0 },
    },
};

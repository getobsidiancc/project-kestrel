// Project Kestrel - adapter settings (stored on the RIGHT chip, edited by the config app)
//
// The config app (tools/app/index.html) reads and writes this exact byte layout
// (little-endian, packed). If you change it, bump KCFG_VERSION and update the app.
//
// Version history
//   1: 138 bytes - one aim setup (max turn speed + curve exponent + deadzone), 48 binds
//   2: 464 bytes - hipfire and ADS aim setups as measured lookup tables, 96 binds with a
//                  toggle flag, look-with-keys targets

#pragma once

#include <stdint.h>

#define KCFG_MAGIC         0x4354534Bu   // "KSTC"
#define KCFG_VERSION       2
#define KCFG_MAX_BINDS     96
#define KCFG_CURVE_POINTS  8

// Bind sources
//   0x01-0xE7 : keyboard key (USB HID usage ID)
#define KSRC_MOUSE(n)    (0xF0 + (n))  // mouse button n: 0 left, 1 right, 2 middle, 3 back, 4 forward
#define KSRC_WHEEL_UP    0xF8
#define KSRC_WHEEL_DOWN  0xF9

// Bind targets
#define KTGT_NONE        0
#define KTGT_BUTTON(bit) (1 + (bit))   // 1..16 = PAD_* bit 0..15 (see kestrel_link.h)
#define KTGT_LT          17            // also switches aiming to the ADS setup while active
#define KTGT_RT          18
#define KTGT_MOVE_UP     19
#define KTGT_MOVE_DOWN   20
#define KTGT_MOVE_LEFT   21
#define KTGT_MOVE_RIGHT  22
#define KTGT_WALK        23            // hold: left stick at walk_scale_pct
#define KTGT_LOOK_UP     24            // right stick with keys (overrides the mouse while held)
#define KTGT_LOOK_DOWN   25
#define KTGT_LOOK_LEFT   26
#define KTGT_LOOK_RIGHT  27
#define KTGT_RAPID_MODE  28            // press: turn rapid-click mode on/off (always starts off)
#define KTGT_RAPID_ARM   29            // press while rapid-click mode is on: arm it (until mode goes off)
#define KTGT_RAPID_DISARM 30           // press: disarm rapid-click (arm key needed again)
#define KTGT_FIRE_OR_PLACE 31          // RT normally; X while gadget mode is on (Siege console places
                                       // gadgets with X), so the fire key also places gadgets
#define KTGT_MAX         31

#define KBIND_TOGGLE     0x01          // tap to turn on, tap again to turn off
#define KBIND_AIM_ONLY   0x02          // only works while an LT (aim) bind is active - e.g. Siege lean
                                       // on the stick clicks without triggering sprint/melee
#define KBIND_TAP        0x04          // pressing sends one quick click (KBIND_TAP_MS), however long
                                       // the key is held - for actions the game fires on release
#define KBIND_TAP_MS     80          // short enough that holding never turns a tap into a hold
                                       // action (Siege: crouch never becomes prone by accident)
#define KBIND_LONG       0x08          // a quick press is held for at least KBIND_LONG_MS - one tap
                                       // does a "hold" action (Siege: tap Ctrl = hold crouch = prone)
#define KBIND_LONG_MS    500
// Rapid-click (singleplayer use, e.g. Minecraft fast break). It only runs when ALL of these hold:
// mode on (KTGT_RAPID_MODE key), armed (KTGT_RAPID_ARM key pressed since), and a KBIND_RAPID_HOLD
// key held. Then holding a KBIND_RAPID key clicks its button over and over. Otherwise every
// bind works normally.
#define KBIND_RAPID      0x10          // the key that rapid-clicks (works normally otherwise)
#define KBIND_NOT_RAPID  0x20          // the bind does nothing while rapid-click mode is on (so the same
                                       // key can do something else in rapid mode)
#define KBIND_RAPID_HOLD 0x40          // must be held for rapid-click to run; sends nothing while armed
#define KBIND_GADGET     0x80          // gadget key: a press turns gadget mode on (press again = off).
                                       // In gadget mode the next KTGT_FIRE_OR_PLACE press sends X.
                                       // Gadget mode ends after that press, after KBIND_GADGET_MS,
                                       // when aiming, or when Y (weapon swap) is pressed.
#define KBIND_GADGET_MS  4000
#define KBIND_RAPID_ON_MS  25
#define KBIND_RAPID_OFF_MS 25

typedef struct __attribute__((packed)) {
    uint8_t src;
    uint8_t tgt;
    uint8_t flags;
} kcfg_bind_t;

// One aim setup (hipfire or ADS).
// The response table says how fast the GAME turns at each stick position, as measured by
// the app's calibration: (defl[i], dps[i]) with defl ascending in 0..1. The first point is
// normally (deadzone, 0). The adapter inverts it to find the stick position for a wanted
// turn speed.
typedef struct __attribute__((packed)) {
    float   cm_per_360;          // mouse distance for a full turn (your sensitivity)
    float   y_ratio;             // vertical speed relative to horizontal
    uint8_t npoints;             // 2..KCFG_CURVE_POINTS
    uint8_t reserved[3];
    float   defl[KCFG_CURVE_POINTS];
    float   dps[KCFG_CURVE_POINTS];
} kcfg_aim_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    uint8_t  reserved0;
    uint16_t size;               // sizeof(kcfg_t)

    float    mouse_dpi;
    float    smoothing;
    uint8_t  invert_y;
    uint8_t  dz_shape;           // 0 round, 1 square
    uint8_t  window_ms;
    uint8_t  walk_scale_pct;     // left stick % while the walk key is held
    uint8_t  wheel_hold_ms;      // how long a wheel click holds its button
    uint8_t  ads_enabled;        // use the ADS setup while the LT bind is active
    uint8_t  aim_settle_10ms;    // "only while aiming" binds wait until aim has been held this long
                                 // (x10 ms; 0 = default 200 ms) - lets the game finish entering ADS
    uint8_t  hip_fast_pct;       // hipfire fast-turn boost: extra speed (%) for mouse moves faster
                                 // than a slow aim, so fast turns reach the game's top speed sooner
                                 // while small aim stays 1:1 (0 = off)

    kcfg_aim_t hip;
    kcfg_aim_t ads;

    kcfg_bind_t binds[KCFG_MAX_BINDS];
} kcfg_t;

// The config app hard-codes this layout (tools/app/index.html: KCFG_SIZE and offsets)
_Static_assert(sizeof(kcfg_aim_t) == 76, "kcfg_aim_t layout changed - update the config app");
_Static_assert(sizeof(kcfg_t) == 464, "kcfg_t size changed - update the config app");
_Static_assert(__builtin_offsetof(kcfg_t, hip) == 24, "kcfg_t layout changed - update the config app");
_Static_assert(__builtin_offsetof(kcfg_t, ads) == 100, "kcfg_t layout changed - update the config app");
_Static_assert(__builtin_offsetof(kcfg_t, binds) == 176, "kcfg_t layout changed - update the config app");

// ---- Version 1 layout, kept so saved settings can be upgraded ----------------------------
#define KCFG_V1_MAX_BINDS 48
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    uint8_t  size;
    float    mouse_dpi, cm_per_360, y_ratio, game_max_turn_dps, game_curve_exp, deadzone, smoothing;
    uint8_t  invert_y, dz_shape, window_ms, walk_scale_pct, wheel_hold_ms, reserved[3];
    struct __attribute__((packed)) { uint8_t src, tgt; } binds[KCFG_V1_MAX_BINDS];
} kcfg_v1_t;
_Static_assert(sizeof(kcfg_v1_t) == 138, "v1 layout must stay 138 bytes");

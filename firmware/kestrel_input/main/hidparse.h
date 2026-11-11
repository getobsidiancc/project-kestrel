// Project Kestrel - minimal HID report descriptor decoder
//
// Finds the parts of a keyboard / mouse report we care about so devices can be used in
// their full ("report protocol") mode instead of the 6-key boot mode:
//   keyboard keys (bitmap = n-key rollover, or array), mouse buttons, X, Y, wheel.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define HP_MAX_FIELDS 24

typedef enum {
    HPF_KEY_BITMAP,   // one bit per key, usages usage_min..usage_max
    HPF_KEY_ARRAY,    // `count` slots holding pressed key usages
    HPF_BUTTONS,      // mouse buttons, bit i = button usage_min+i
    HPF_X,
    HPF_Y,
    HPF_WHEEL,
} hpf_kind_t;

typedef struct {
    uint8_t  kind;
    uint8_t  report_id;     // 0 when the device uses no report IDs
    uint8_t  size;          // bits per value
    uint8_t  count;         // number of values
    uint8_t  is_signed;
    uint16_t bitoff;        // bit offset inside the report (after the report ID byte)
    uint16_t usage_min;
    uint16_t usage_max;
} hp_field_t;

typedef struct {
    uint8_t    nfields;
    bool       uses_report_ids;
    uint16_t   key_count;    // keys covered by bitmap/array fields (0 = not a keyboard)
    bool       has_xy;       // has relative X and Y (a mouse)
    bool       has_wheel;
    uint8_t    xy_bits;
    hp_field_t f[HP_MAX_FIELDS];
} hp_layout_t;

// Returns true if the descriptor was understood (it may still have no useful fields).
bool hidparse(const uint8_t *desc, size_t len, hp_layout_t *out);

// Read `size` bits at `bitoff` (little-endian), sign-extended if is_signed.
int32_t hp_extract(const uint8_t *data, size_t len, uint16_t bitoff, uint8_t size, bool is_signed);

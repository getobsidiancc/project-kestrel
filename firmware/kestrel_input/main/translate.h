// Project Kestrel - mouse -> right stick translation
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "kestrel_cfg.h"

// Load aim settings (DPI, smoothing, hipfire + ADS tables) from the adapter settings.
void translate_init(const kcfg_t *cfg);

// Call once per millisecond with the mouse counts that arrived during that millisecond.
// ads = the aim button is active (uses the ADS setup if it's enabled).
// Writes the right stick position (-32767..32767, +y = up).
void translate_step(int32_t dx, int32_t dy, bool ads, int16_t *out_rx, int16_t *out_ry);

// Calibration: force the right stick to a fixed horizontal deflection (0..1), or <0 to stop.
void translate_set_override(float x_deflection);

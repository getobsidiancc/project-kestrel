// Project Kestrel - saved settings (RIGHT chip, NVS)
#pragma once

#include <stdbool.h>
#include "kestrel_cfg.h"

// Load from flash (or factory defaults). Call once at boot.
void settings_init(void);

// Copy of the current settings (thread-safe).
void settings_get(kcfg_t *out);

// Validate, apply and save. Returns 0 ok, 1 invalid, 2 save failed.
int settings_set(const kcfg_t *cfg);

// Reset to factory defaults and save. Returns 0 ok, 2 save failed.
int settings_reset(void);

// Increments every time settings change (so the input loop can pick them up).
uint32_t settings_generation(void);

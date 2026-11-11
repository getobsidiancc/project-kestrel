// Project Kestrel - controller output selection (LEFT chip)
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "kestrel_link.h"

#define OUTPUT_XINPUT   1   // Xbox 360 wired controller - Windows PCs
#define OUTPUT_XBOX_ONE 2   // Xbox One controller (GIP) - Xbox One / Series consoles, also Windows

#ifndef KESTREL_OUTPUT_MODE
#define KESTREL_OUTPUT_MODE OUTPUT_XBOX_ONE
#endif

// Install TinyUSB with this mode's descriptors.
void output_start(void);

// True once the PC/console has configured the controller.
bool output_mounted(void);

// Called about every millisecond with the latest controller state.
void output_poll(const pad_state_t *pad);

// Link messages for the auth relay (Xbox One mode only).
void output_link_rx(uint8_t type, const uint8_t *payload, uint8_t len);

// Xbox One auth state, sent to the RIGHT chip so it can share USB channels.
bool output_auth_done(void);
bool output_wants_auth(void);

// Short status text for the log.
const char *output_status(void);

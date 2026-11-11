// Project Kestrel - config app bridge (LEFT chip)
//
// The config app talks to the LEFT chip's UART0, which is the MAKCU's middle "COM" port
// (CH343). Commands are text lines; replies start with "@KST " so the app can pick them
// out of the normal log output.
//
//   KST PING            -> @KST PONG <left fw> <right fw> <mode>
//   KST GET             -> @KST CFG <hex of kcfg_t>
//   KST SET <hex>       -> @KST RESULT <0 ok | 1 invalid | 2 save failed | 3 timeout>
//   KST DEFAULTS        -> @KST RESULT <code>
//   KST CAL <milli>     hold the right stick at <milli>/1000 to the right (-1 = off)
//   KST LIVE <0|1>      stream "@KST PAD ..." (30 Hz) and "@KST KEYS ..." lines
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "kestrel_link.h"

void cfg_bridge_start(const input_status_t *input_status, int left_fw);

// Queue text for the PC without ever waiting (dropped if the PC link is backed up).
void pc_write(const char *s, size_t len);
void cfg_bridge_link_rx(uint8_t type, const uint8_t *payload, uint8_t len);
void cfg_bridge_pad(const pad_state_t *pad);   // call every loop with the current state

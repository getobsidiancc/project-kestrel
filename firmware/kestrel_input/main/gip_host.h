// Project Kestrel - Xbox One controller on the hub, used to answer the console's auth (RIGHT chip)
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "kestrel_link.h"

// Start the GIP host client. Call after input_usb_start() (needs the USB host installed).
void gip_host_start(void);

// Raw auth packet from the console (via the LEFT chip) to forward to the controller.
void gip_host_from_console(const uint8_t *data, uint8_t len);

// What the console is doing (from the LEFT chip's status message).
void gip_host_set_console_state(bool auth_done, bool want_auth);

// Current state, for the status message to the LEFT chip.
void gip_host_status(auth_dev_status_t *out);

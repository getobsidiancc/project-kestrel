# Local copy of espressif/usb_host_hid 1.0.4

Copied from the ESP Component Registry (Apache-2.0, see LICENSE) so Project Kestrel can
add one function:

- `hid_host_device_pause()` in `hid_host.c` / `include/usb/hid_host.h`: releases an
  interface's USB host channel but keeps the interface known to the driver, so
  `hid_host_device_open()` can claim it again later. Used to lend the keyboard's channel
  to the Xbox auth controller while the console authenticates.

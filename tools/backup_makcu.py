# Project Kestrel - back up one MAKCU chip through the ESP32-S3 ROM loader.
#
# Reading over the MAKCU's native USB is slow (~0.5 KB/s) and esptool's helper stub
# stalls on it, so instead of dumping all 4 MB this saves:
#   0x0000-0xFFFF : bootloader, partition table, NVS settings, PHY calibration
#   0x10000-...   : the firmware image, exactly as long as it is (checked with its SHA-256)
# Restoring = writing the file back at 0x0.
#
# Usage: python tools/backup_makcu.py COM4 backups/makcu_left_original.bin

import hashlib
import struct
import sys
import time

from esptool.cmds import detect_chip

port, out_file = sys.argv[1], sys.argv[2]
APP = 0x10000

esp = detect_chip(port, 115200, "no_reset", False)
esp.flash_spi_attach(0)

t0 = time.time()


def rd(off, n, label=None):
    last = [0]

    def prog(done, total):
        if label and (done - last[0] >= 16384 or done == total):
            last[0] = done
            print("  %s: %d / %d (%.0f%%)  %.0fs" % (label, done, total, 100.0 * done / total, time.time() - t0),
                  flush=True)

    return esp.read_flash_slow(off, n, prog)


hdr = rd(APP, 24)
if hdr[0] != 0xE9:
    sys.exit("No firmware image at 0x10000 (magic %02x) - nothing to back up" % hdr[0])
pos = 24
for _ in range(hdr[1]):
    _, ln = struct.unpack("<II", rd(APP + pos, 8))
    pos += 8 + ln
size = (pos + 1 + 15) & ~15
hash_appended = hdr[23] == 1
if hash_appended:
    size += 32
print("firmware image: %d bytes - about %d minutes total" % (size, (APP + size) / 500 / 60 + 1), flush=True)

head = rd(0, APP, "settings area")
app = rd(APP, size, "firmware")

if hash_appended and hashlib.sha256(app[:-32]).digest() != app[-32:]:
    sys.exit("SHA-256 check FAILED - backup not saved, try again")
print("SHA-256 check: OK" if hash_appended else "no SHA-256 in image, saved unchecked")

with open(out_file, "wb") as f:
    f.write(head + app)
print("saved %s (%d bytes)" % (out_file, len(head) + len(app)))

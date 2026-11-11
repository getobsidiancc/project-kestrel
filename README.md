# Project Kestrel

A mouse-and-keyboard to controller adapter built on the MAKCU's two ESP32-S3 chips.

```
Mouse + keyboard ─► USB hub ─► RIGHT chip ──(internal UART link)──► LEFT chip ─► PC / console
                               USB host                              USB device
                               reads mouse/keyboard,                 shows up as a
                               translates aim                        wired controller
```

## Status

| Stage | What | State |
|---|---|---|
| 1 | Mouse/keyboard → Xbox 360-style controller on a **Windows PC** | **Working** (tested 9 Nov 2026: link 0 errors, sticks/triggers/buttons reach Windows) |
| 2 | Xbox One controller (GIP) + auth relay through your own Xbox controller | **Working on Windows** (9 Nov 2026: PowerA answered Windows' security check through the relay, input live). **Not yet tested on an Xbox console.** |
| 3 | Config app: keybinds, sensitivity, live view, calibration, profiles | **Working** (tested on the adapter 9 Nov 2026) |
| 4 | Settings v2: keyboard-picture binds (96, toggle, look keys), hipfire + ADS lookup tables, multi-point calibration, Siege PC → console import | Built; app tested against simulated data; needs flashing + hardware test |

## Config app

Double-click **`Kestrel Config.bat`**. It opens the app in your browser (use **Chrome or Edge**).

1. Plug the MAKCU's **middle port** into the PC (port 1 can be on the Xbox or the PC).
2. Close `tools\monitor.ps1` if it's running (only one program can use the COM port).
3. Click **Connect** and pick **USB-Enhanced-SERIAL CH343**.
4. Change things, then click **Save to adapter**. Settings are stored on the MAKCU itself.

| Tab | What it does |
|---|---|
| Sensitivity | DPI, sensitivity (cm per 360°), vertical speed, invert, smoothing, walk speed |
| Keybinds | Click **+**, then press a key or mouse button on the adapter's keyboard/mouse (or this PC's keyboard) |
| Game tuning | The game's max turn speed, response curve and deadzone |
| Calibrate | Measures those three in-game (the adapter drives the right stick while you time it) |
| Live view | Sticks, triggers and buttons exactly as the console sees them |
| Profiles | Save/load per-game setups in the browser, export/import as files, reset to defaults |
| Log | The adapter's log |

The settings format is defined in `components/kestrel_link/include/kestrel_cfg.h`; the firmware
refuses to build if it stops matching the app. Settings saved by older firmware are upgraded
automatically (keybinds and sensitivity kept).

### Siege PC → console (1:1)
1. **Siege PC → console** tab: choose your PC `GameSettings.ini`, enter your mouse DPI.
   The app shows your PC hipfire / per-scope ADS in cm per 360° and the exact console Siege
   controller settings to use (acceleration off, per-zoom ADS values that keep every scope
   matched to your PC).
2. Set those on console Siege, click **Apply PC sensitivity**.
3. **Calibrate** tab: measure **Hipfire**, then **Aiming** (holding aim, 1x sight). Save.
4. Back on the Siege tab: **Scope check** with a 2.5x to confirm scopes match.

Formulas: hipfire degrees/count = slider × (multiplier / 0.02) × 0.00572957795; ADS follows
Ubisoft's Y5S3 model (ADS value × ADS unit × tan(zoom·vFOV/2)/tan(vFOV/2), console vFOV 60°).
Mouse movement faster than the game's max turn speed is capped (shown on the Game tuning tab).

The LEFT chip is built in **Xbox One mode** by default (works on Xbox One / Series and on Windows).
For the older Xbox 360 mode, set `KESTREL_OUTPUT_MODE` to `OUTPUT_XINPUT` in
`firmware/kestrel_output/main/output.h` and rebuild.

## What's in here

| Path | What it is |
|---|---|
| `firmware/kestrel_input/` | RIGHT chip: USB host, keyboard/mouse reading, aim translation, Xbox auth controller |
| `firmware/kestrel_output/` | LEFT chip: Xbox One (GIP) or Xbox 360 (XInput) controller |
| `components/kestrel_link/` | The protocol the two chips use to talk to each other |
| `components/kestrel_gip/` | Xbox One protocol helpers (ported from GP2040-CE, MIT) |
| `components/usb_host_hid/` | Espressif's HID host driver (Apache-2.0) + one added function, see `KESTREL_CHANGES.md` |
| `firmware/kestrel_input/main/kestrel_config.h` | **Your settings**: DPI, sensitivity, key bindings |
| `tools/build.ps1` | Builds both firmwares into `release/` |
| `tools/flash.ps1` | Backs up the original MAKCU firmware, then flashes one side |
| `tools/monitor.ps1` | Shows the adapter's live log (both chips) |
| `tools/xinput_test.py` | Reads the controller the way Windows sees it and reports what moved |

## Stage 1 test (PC only, nothing to buy)

### 1. Build
```
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```
This puts `kestrel_LEFT_output.bin` and `kestrel_RIGHT_input.bin` in `release\`.

### 2. Flash the LEFT chip
1. Unplug every cable from the MAKCU.
2. Hold the **LEFT** button and plug a cable from your PC into the **LEFT** USB port.
3. Find the new COM port in Device Manager → Ports (COM & LPT).
4. Run (with your COM number):
   ```
   powershell -ExecutionPolicy Bypass -File tools\flash.ps1 -Side Left -Port COM5
   ```
   The first run backs up the original MAKCU firmware to `backups\` before writing anything.

### 3. Flash the RIGHT chip
Same thing, but hold the **RIGHT** button, plug into the **RIGHT** port, and use `-Side Right`.

If flashing fails, put that side back into flash mode and add `-NoStub` to the command.

### 4. Plug it in
- **Farthest LEFT** port → your PC (this is the "controller" cable)
- **Farthest RIGHT** port → USB hub, with your mouse and keyboard plugged into the hub
- **Middle** port (optional) → your PC, for the log

### 5. Check it works
1. Windows should make the "device connected" sound. The adapter appears as an Xbox controller under *Settings → Bluetooth & devices*, or in `joy.cpl` (press Win+R, type `joy.cpl`).
2. Open a gamepad tester in your browser (for example https://hardwaretester.com/gamepad).
3. Check:
   - **WASD** moves the left stick
   - **moving the mouse** moves the right stick, and it springs back to centre when you stop
   - **left click** = right trigger, **right click** = left trigger
   - Space = A, Ctrl/C = B, R/F = X, 1/2 = Y, Q = LB, E = RB, Shift = left stick click, V = right stick click, Tab = View, Esc = Menu, arrows = D-pad

### LED (GPIO 9)
- **Solid**: everything is working
- **Fast blink** (LEFT chip): the two chips aren't talking (see troubleshooting)
- **Slow blink**: waiting for the PC to accept the controller, or for a mouse

### Log
Plug in the middle port, find its COM port (shows as **CH343**), then run:
```
powershell -ExecutionPolicy Bypass -File tools\monitor.ps1 -Port COM7
```
Every 5 seconds you'll see something like:
```
kestrel_out: usb=mounted link=up mice=1 keyboards=1 rx_ok=51234 rx_err=0
```

## Stage 2: Xbox One / Series

```
Port 1 (left)   → Xbox USB port
Port 2 (middle) → a PC (optional, for the log)
Port 3 (right)  → USB hub ─┬─ mouse
                           ├─ keyboard
                           └─ a wired Xbox controller (PowerA, or a Microsoft pad with a DATA cable)
```

What happens (about 5 seconds):
1. The Xbox sees a wired controller and asks it to prove it's licensed.
2. The adapter passes every security packet to your real controller on the hub, untouched,
   and passes its answers back. Log: `console started the security handshake`.
3. Log: `*** Xbox accepted the controller - input is live ***`
4. The adapter releases your controller and the keyboard comes back. The keyboard does
   nothing during those few seconds (the S3 only has 8 USB channels, see `gip_host.c`).

If it gets stuck on `authenticating`: unplug the controller from the hub and plug it back in.
The adapter reconnects to the console and retries by itself.

Notes:
- Many micro-USB cables are **charge-only**; the controller then never shows up in the log.
- The hub has no power supply of its own, so the controller can drop off at startup if the
  keyboard's lighting draws a lot. Replugging the controller fixes it; a powered hub avoids it.

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| `link=DOWN`, rx_ok stays 0 | The two chips aren't on GPIO1/GPIO2 on your board revision. Send me the log and I'll adjust `KLINK_*_PIN` in `components/kestrel_link/include/kestrel_link.h`. |
| `mice=0` with the mouse plugged in | Try the mouse straight into the RIGHT port without the hub. If that works, try a different hub (unpowered hubs from monitors sometimes don't work). |
| `rx_err` climbing fast | Link too fast for the board: lower `KLINK_BAUD` to `1000000` and rebuild both sides. |
| Windows says "USB device not recognized" | Send me the log; that's a descriptor issue on the LEFT side. |
| Aim feels too slow/fast | Change `cm_per_360` and `mouse_dpi` in `kestrel_config.h`, rebuild, reflash the RIGHT side only. |

## Going back to the previous firmware
The backups in `backups\` hold whatever was on each chip before Kestrel. On this MAKCU that was
custom firmware built on 9 Nov 2026 (`ximfw_gippad` on LEFT, `gip_relay_host` on RIGHT), **not** the
stock MAKCU firmware. For stock firmware, use MAKCU's official AIO tool instead.
```
powershell -ExecutionPolicy Bypass -File tools\flash.ps1 -Side Left  -Port COM5 -Restore
powershell -ExecutionPolicy Bypass -File tools\flash.ps1 -Side Right -Port COM6 -Restore
```
(Each side needs flash mode, same as above.)

## Notes
- Many console shooters (Call of Duty, Apex, Valorant, Siege and others) ban mouse-and-keyboard adapters. Use this in games that allow it.
- Requires ESP-IDF v5.5.6 at `C:\esp\esp-idf-v5.5.6` (already installed on this PC).

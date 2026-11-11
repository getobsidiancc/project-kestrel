# Project Kestrel: handoff notes

Read this first if you're an AI (or a person) picking up this project. It describes the state as of **10 Nov 2026**, when the owner said the adapter "feels great". Some parts of README.md are older than this file. Where the two disagree, this file is right.

**Most important rule:** the current setup is the result of many failed rounds of guessing. **Don't re-tune by feel first.** If something feels off, read the adapter's settings back and compare them with the backup (section 6). Restore the backup before changing anything.

---

## 1. What this is

The MAKCU is a small box with **two ESP32-S3 chips**, made into a XIM-style adapter. A mouse and keyboard plugged into it show up on an **Xbox One** as a wired Xbox controller. The owner mainly plays **Rainbow Six Siege**, plus **singleplayer Minecraft**.

```
Mouse + keyboard + PowerA controller ─► USB hub ─► RIGHT chip (USB host) ─UART 2 Mbaud─► LEFT chip (USB device) ─► Xbox
```

- **RIGHT chip** (`firmware/kestrel_input`, **v22**):
  - reads the mouse and keyboard (USB host, full HID report mode, NKRO);
  - translates mouse movement to the right stick, and applies the keybinds;
  - stores the settings in NVS.
- **LEFT chip** (`firmware/kestrel_output`, **v5**):
  - acts as the Xbox One controller (GIP protocol, ported from GP2040-CE, MIT licence);
  - relays the console's authentication to the real **PowerA controller (20d6:4001)** on the hub.
- **Middle USB port** = CH343 USB-serial, **COM3** at **921600 baud**. The config app and all the scripts talk to the adapter through it.

### Cabling for normal use
- **Port 1 (left):** to the Xbox.
- **Port 2 (middle):** to the PC (config and scripts).
- **Port 3 (right):** to the USB hub, with the mouse, keyboard and **PowerA controller** on it. The PowerA must stay plugged in: the Xbox authenticates through it.

### Keyboards
- Use the **SteelSeries keyboard**.
- The Razer keyboard went into a reset loop on the unpowered hub (its RGB draws too much power).

---

## 2. Building and flashing

- **Toolchain:** ESP-IDF **5.5.6** at `C:\esp\esp-idf-v5.5.6`. Git is not installed on this PC.
- **Build both chips:**
  ```
  powershell -ExecutionPolicy Bypass -File tools\build.ps1
  ```
  The output goes to `release\left\` and `release\right\` (bootloader, partition table and app).
- **Flash the RIGHT chip**, which is where almost all changes happen:
  1. The owner unplugs everything, **holds the RIGHT button**, and plugs the PC into the **RIGHT port**. The chip shows up as **COM5** (USB-JTAG, 303A:1001).
  2. Run:
     ```
     powershell -ExecutionPolicy Bypass -File tools\flash.ps1 -Side Right -Port COM5
     ```
  3. The owner replugs normally.
- **Flash the LEFT chip:** hold the **LEFT** button and plug into the **LEFT** port. It shows up as a ROM USB-OTG port (303A:0009), and the script uses `--no-stub`.
- `flash.ps1` writes the three parts separately, so **the settings in NVS survive the flash**.
  - **Never flash the merged image:** it erases the settings.
- If the RIGHT chip is silent after a flash, a full power-cycle fixes it.
- After every RIGHT flash:
  - bump `FW_VERSION` in `firmware/kestrel_input/main/main.c`;
  - check that `KST PING` reports it: `PONG <left> <right> xbox_one`, for example `PONG 5 21 xbox_one`.

---

## 3. The settings (stored on the RIGHT chip)

The layout is defined in `components/kestrel_link/include/kestrel_cfg.h`: `kcfg_t`, version 2, **464 bytes**, little-endian.

**Offsets:**

| Offset | Field |
|---|---|
| 8 | DPI |
| 12 | smoothing |
| 18 | window_ms |
| 19 | walk % |
| 20 | wheel ms |
| 21 | ADS on |
| 22 | aim_settle (×10 ms; 0 = 200 ms) |
| 23 | `hip_fast_pct` |
| 24 | hipfire aim block (`kcfg_aim_t`, 76 bytes) |
| 100 | ADS aim block |
| 176 | 96 binds × 3 bytes (src, tgt, flags) |

The aim block holds: `cm_per_360` (float); `y_ratio` (float); `npoints` (u8) plus 3 reserved bytes; `defl[8]` (floats at +12); `dps[8]` (floats at +44).

### Protocol on COM3
Text lines at 921600 baud.

**Commands:**
- `KST PING`
- `KST GET` → `@KST CFG <928 hex chars>`
- `KST SET <hex>` → `@KST RESULT 0` (0 means OK)
- `KST DEFAULTS`
- `KST CAL <milli>`: holds the right stick at that deflection (0–1000); `-1` releases it.
- `KST LIVE 1/0`: streams `@KST PAD ...` and `@KST KEYS <mousebtns> <hid,hid,...>`.

**Gotchas:**
- A SET occasionally doesn't stick. Always read it back with GET and retry. The scripts do this.
- If COM3 gives "Access denied", the config app (a browser tab) is holding the port. Ask the owner to click **Disconnect** or close the tab.

### The good settings (confirmed by the owner)
| Setting | Value |
|---|---|
| DPI | 800 |
| Hipfire | **7.5 cm/360**, `y_ratio` 1.0 |
| ADS | **20 cm/360**, ADS on |
| Hipfire fast-turn boost (`hip_fast_pct`) | **100 %** |
| Speed averaging (window) | **2 ms** |
| Smoothing | 0 |
| Walk | 40 % |
| Deadzone shape | round |

### Measured response curves
These were measured on the owner's Xbox with the F12 calibration scripts. They are the reason it finally feels right.

**Hipfire** (Look Acceleration at max, delay and ramp-up 0):

| Stick | 0.092 (deadzone) | 0.104 | 0.145 | 0.235 | 0.325 | 0.55 | 0.775 | 1.0 |
|---|---|---|---|---|---|---|---|---|
| °/s | 0 | 8.8 | 26.4 | 139.3 | 337 | 603 | 917 | 1200 (extrapolated) |

**ADS** (Aim Acceleration **0**):

| Stick | 0.086 (deadzone) | 0.098 | 0.145 | 0.19 | 0.28 | 0.55 | 0.775 | 1.0 |
|---|---|---|---|---|---|---|---|---|
| °/s | 0 | 7.2 | 43.9 | 125.8 | 365.8 | 929* | 1412* | 1848* |

\* Hipfire shape × 1.54, not measured directly.

**Lessons learned:**
- **The real deadzone is about 0.09.** The early guess of 0.115–0.125 created a minimum-speed floor: super-slow moves came out fast and inaccurate. That was the main cause of "it feels terrible".
- Fast rows (under 1 s per turn) are inaccurate to time. Measure with slow rows: 1–6 s per turn.
- With look acceleration on, turning speed also depends on how long the stick is held. It's a known imperfection; the owner accepted it, because turning acceleration off caps hipfire at about 440°/s.

---

## 4. Keybinds (50, all saved on the adapter)

Bind flags:
- `0x01` toggle
- `0x02` **only while aiming** (lean; waits until aim has been held 200 ms, and sends clean presses with a minimum length)
- `0x04` tap (one clean 80 ms press per keypress, spam-safe)
- `0x08` long press (500 ms)
- `0x10` rapid
- `0x20` off in rapid mode
- `0x40` hold-for-rapid

Targets:
- 1–16 = controller button bit + 1
- 17 = LT, 18 = RT
- 19–22 = move up / down / left / right, 23 = walk
- 24–27 = look keys
- 28 = rapid mode on/off, 29 = rapid arm, 30 = rapid disarm

| Key | Controller | Notes |
|---|---|---|
| W A S D | left stick | Left Alt = walk (40 %) |
| Left click | RT | also the rapid key (Minecraft) |
| Right click | LT (aim) | plus a second bind with the hold-for-rapid flag |
| Q / Mouse 4 | L3 (only while aiming) | lean left |
| E / Mouse 5 | R3 (only while aiming) | lean right |
| Left Shift | L3 | sprint (ignored while aiming) |
| V | R3 | melee (ignored while aiming) |
| Space, Enter | A | |
| C | B (tap) | quick crouch, never prone |
| Left Ctrl | B (hold) | tap = crouch, hold = prone |
| Z, Up arrow | D-pad up | ping |
| R, F | X | reload / interact / drone scan; hold 7 = switch operator |
| 7 | X | hold = switch operator |
| 1, 2, wheel up/down, X | Y | weapon swap; hold X = drone scan |
| G, Left arrow | LB | secondary gadget / previous drone |
| Middle click, Right arrow | RB | operator ability / next drone |
| 3 / 4 / 5 | D-pad left / right / down | 5 = cameras / exit drone |
| Down arrow | D-pad down | |
| Tab / Esc | View / Menu | |
| Home, Windows key | Xbox button | |
| F11 | rapid mode on/off | |
| 2 | rapid arm | as well as its normal Y |
| 1, 3, 4, 5, 6, 7, 8, 9 | rapid disarm | as well as their normal binds |

**Firmware behaviour to know about:**
- **Diagonal sprint:** while sprinting, W+A / W+D aims the stick 25° off straight ahead instead of 45°, so Siege doesn't cancel the sprint.
- **Lean:** only works while holding aim. Console Siege has no hip lean. The owner set Siege's lean mode to toggle.
- **Minecraft fast break** (singleplayer only):
  1. F11 turns rapid mode on.
  2. Press 2 to arm it.
  3. Hold right-click (it still works normally).
  4. Hold left-click: it rapid-clicks about 20 times per second.

  Pressing 1 or 3–9 disarms it, so the owner has to press 2 again. F11 turns it off. It always starts off at power-up.
- **Gadget placing** (v22): console Siege places gadgets with **X**.
  - Left click is bound to target 31, "Fire / place gadget".
  - Middle click (RB) and G (LB) have the gadget-key flag `0x80`.
  - After a gadget key, the next left click sends X instead of RT. Gadget mode ends after that click, after 4 s, on aim, on weapon swap (Y), or on another gadget-key press.
- **Latency:**
  - Each mouse or keyboard report is processed the instant it arrives (v21), and the 1 ms timer restarts from that moment.
  - The adapter adds about 1–2 ms in total; the rest of the delay is the Xbox and Siege.
- **Slow-move anti-shake:** while the 2 ms window holds fewer than 3 counts, the speed is averaged over up to 4× the window. It stops at once when the mouse stops. Longer look-backs (32 ms) felt floaty and were rejected.

---

## 5. Console Siege settings the owner uses

**Look (hipfire):**
- Horizontal and vertical sensitivity: 100
- **Look Acceleration: max**, delay 0, ramp-up 0

**Aim (ADS):**
- **Aim Acceleration (horizontal and vertical): 0.** The ADS curve was measured with this setting, and turning it back on brings the float back.

**Other:**
- FOV 90
- Controller layout: Default. Lean is on the stick clicks while aiming; the drone, defuser and ability mode are on the D-pad.
- Recommended per-scope ADS: 1x 100, 2.5x 168, all others 135, with "use specific ADS per zoom" on.
  - Not confirmed whether the owner has actually set these.

---

## 6. Scripts and tools

Run these with the ESP-IDF Python, which has pyserial:
```
$py = Get-ChildItem "$env:USERPROFILE\.espressif\python_env\*\Scripts\python.exe" | Select-Object -First 1 -ExpandProperty FullName
```

**Restore the good setup** (the first thing to try if anything feels off):
```
& $py tools\scripts\restore_cfg.py COM3 profiles\siege-good.hex
```

All scripts are in `tools/scripts/`:

| Script | What it does |
|---|---|
| `proto_test.py COM3` | Health check: firmware versions, DPI, hip table, bind count, live data rate |
| `save_cfg.py COM3 file.hex` / `restore_cfg.py COM3 file.hex` | Back up / restore the full settings |
| `dumpbinds.py COM3` | Lists binds as `src>tgt/flags` (hex HID usage > target / flags) |
| `set_feel.py COM3 <window_ms> <smoothing> <fast%>` | Feel settings |
| `apply_full_measured.py COM3 <hip_cm> <ads_cm>` | Loads both measured curves and sets both sensitivities; use it for "make it faster/slower" |
| `easy_cal.py COM3 0.05,0.15,0.25` | F12 one-turn calibration at stick = 0.10 + 0.9 × fraction. The owner presses F12 to start and F12 after one full circle. Hold right-click for ADS. |
| `lowend_cal.py COM3` | Finds the real deadzone (ramp, F12 when the view moves) plus one slow turn, for ADS then hip |
| `add_bind.py`, `set_flag.py`, `rapid_binds.py`, `disarm_binds.py`, `rmb_normal.py` | Small bind edits |
| `logrec.py COM3 out.txt 60` | Records the adapter log (survives replugs) |
| `apply_full_layout.py` | The **older** 36-bind layout. Prefer `restore_cfg.py` with the hex backup. |

Other tools:
- **Config app:** `Kestrel Config.bat` serves `tools/app/index.html` on http://localhost:8642. Use it in Chrome or Edge, with Web Serial.
  - **It overwrote good settings once:** an old profile cut the binds to 19 and set ADS to 61.
  - Tell the owner to click **Connect** first and never load old profiles.
- **Research report:** `reports/ESP32 S3 XIM adapter feasibility.md`.

---

## 7. How to work with the owner

- They type fast and casually, and get frustrated when something feels bad. Keep replies short and concrete, with one change at a time, and confirm every change by reading it back from the adapter.
- When they say "faster" or "slower", change only the cm/360 numbers (about 12–15 % per step), using `apply_full_measured.py`. The curves are right; don't touch them.
- For **anything that feels "off" or "jittery"**:
  1. Compare the adapter with `profiles/siege-good.hex`.
  2. If they match, measure instead of guessing.
- Flash procedure wording they know: "hold the RIGHT button, plug the PC into the RIGHT port, tell me 'RIGHT, replugged'". After the flash: "plug back in normally, tell me 'Plugged in'".

### Boundaries kept throughout (keep them)
- No anti-cheat or MouseTrap evasion.
- No aimbot, aim assist or anti-recoil.
- No extracting keys from the controller.
- No multi-step macros in Siege.
  - Single-key behaviours (toggle, tap, long press, aim-only) are fine.
  - The rapid-click feature exists **for singleplayer Minecraft only**, as the owner asked.

---

## 8. Ideas not done yet (optional)

- **Mouse DPI 1600:** smoother slow aim. Set the mouse's DPI and the adapter's DPI (offset 8) to the same value, or the sens doubles.
- **Re-measure the fast end** of the hipfire curve (rows from 0.55 to 1.0) with more careful timing.
- **Measure ADS rows above 0.28**, which are currently scaled from the hipfire shape.
- **Wired internet, and Game Mode on the TV or monitor.** These are outside the adapter, but they cut much more delay than anything left inside it.

# Open-source projects relevant to a DIY ESP32-S3 XIM-style (mouse/keyboard to console controller) adapter

Research date: 2026-11-09. Scope: catalog of existing OSS projects, capabilities, hardware, consoles, mouse-to-stick support, license, activity, reuse for ESP32-S3. Anti-cheat evasion was deliberately NOT researched. About 17 tool calls; several projects could only be covered at a surface level (see Gaps).

## Q1. GP2040-CE (RP2040/RP2350): USB host keyboard/mouse, console modes, auth passthrough, mouse-to-analog

### Takeaway
GP2040-CE is the most mature open-source multi-console gamepad firmware (MIT). As of v0.7.12 (Nov 2025) it can map a mouse on the "EXT" USB-host port to the left or right analog stick. It also handles console auth: PS4/PS5 via a USB dongle or passthrough, Xbox One via passthrough, and Xbox 360 now without a dongle. It is RP2040/RP2350-only, so for an ESP32-S3 build it is a reference design, not drop-in code.

### Cited Findings
- Described as multi-platform gamepad firmware for the Raspberry Pi Pico and other RP2040 boards. Features include SOCD cleaning, turbo, RGB LEDs and a built-in web configurator. MIT license. — [GitHub: OpenStickCommunity/GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE)
- Input modes: X-Input, Nintendo Switch, PlayStation 4/5, Xbox One, D-Input, Keyboard and others (13–14 modes). Listed as compatible with PC, PS3, PS4, PS5, Switch, Xbox One, Steam Deck, MiSTer and Android. — [GitHub: GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE)
- Credits: PassingLink for PS4 implementation details. Santroller and GIMX for Xbox One auth via pass-through. Santroller for Xbox 360 working without a dongle. — [GitHub: GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE)
- **v0.7.12 (10 Nov 2025):** a mouse connected to the EXT port can act as the left or right analog stick. Xbox 360 mode "no longer requires a dongle to be connected and will still authenticate". Very early RP2350 support. A USB host fix. — [GP2040-CE Releases](https://github.com/OpenStickCommunity/GP2040-CE/releases)
- **v0.7.11:** controller USB-host passthrough option (works only with DS4 and Stadia controllers, or another GP2040-CE device in PS4 mode). Keyboard host clean-up and unmount bug fix. Warning that PS4/PS5 mode can lose auth when a USB dongle is used and an action triggers a flash save (real-time LED saves were disabled for this). Xbox One multiplayer and auth bug fix. — [GP2040-CE Releases](https://github.com/OpenStickCommunity/GP2040-CE/releases)
- **v0.7.8:** fixed keyboard host up/down being flipped. Open issues exist on keyboard host boot-mode selection and on 8BitDo keyboard support. — [GP2040-CE issues / search summary](https://www.Github.com/OpenStickCommunity/GP2040-CE/issues)
- A community discussion on composite HID with trackball/mouse support exists (#437). — [GitHub Discussion #437](https://www.github.com/OpenStickCommunity/GP2040-CE/discussions/437)
- Top three releases: v0.7.12 (10 Nov 2025), v0.7.11 (11 Apr), v0.7.10 (04 Oct). No release after v0.7.12 was visible on the releases page as of 2026-11-09. — [GP2040-CE Releases](https://github.com/OpenStickCommunity/GP2040-CE/releases)

### Inferences
- GP2040-CE's mouse-to-stick is a recent, likely basic feature. The release notes give no detail about curves, deadzone compensation or XIM-style "smoothness/translation" tuning. For competitive "feel", an ESP32-S3 project would need its own translation algorithm (GIMX-style), even if it borrows GP2040-CE's console protocol and auth code.
- Its auth architecture is the most reusable conceptual model: a USB-host port that accepts a genuine controller or dongle and relays auth challenges. The auth-loss-on-flash-write warning shows that auth timing is fragile.
- Activity: release cadence slowed to about two releases per year (Apr 2025, Nov 2025). Still a large, active community, but no 2026 release was seen.

### Gaps
- Could not fetch a GP2040-CE docs page for the Keyboard/Mouse Host add-on (the guessed URL returned 404). Exact settings (sensitivity, deadzone, curve) for mouse-to-stick are unconfirmed.
- Whether any post-v0.7.12 nightly or 2026 release exists was not confirmed (the release page shows v0.7.12 as latest).
- How the PS5 auth handles the "8-minute timeout" was not documented in the fetched notes.

## Q2. HID Remapper (jfedor2, RP2040), and whether it does mouse-to-gamepad

### Takeaway
HID Remapper is a very active (pre-releases through Nov 2026), MIT-licensed RP2040/nRF52840 USB/BT remapping dongle with a WebHID configurator. It can emulate a Switch gamepad and a "PS4 arcade stick", so its mapping engine can route mouse axes to gamepad outputs. Console PS4 auth is not handled natively, so it needs an officially licensed arcade stick or a dongle.

### Cited Findings
- Configurable USB dongle remapping mice, keyboards and other devices "completely in hardware". Features: button remap, mouse/key cross-mapping, sensitivity, axis rotation, polling-rate overclock to 1000 Hz, wireless receivers, hubs. — [GitHub: jfedor2/hid-remapper](https://github.com/jfedor2/hid-remapper)
- Hardware: Raspberry Pi Pico (single and dual-Pico variants), Adafruit Feather RP2040 USB Host, custom boards v1–v8 (RP2040). There is a Bluetooth version on Feather nRF52840 / Seeed Xiao nRF52840, and a serial-mouse version. No ESP32 support mentioned. — [GitHub: jfedor2/hid-remapper](https://github.com/jfedor2/hid-remapper)
- License: MIT (software), CC BY 4.0 (hardware). — [GitHub: jfedor2/hid-remapper](https://github.com/jfedor2/hid-remapper)
- Emulated device types in the configurator include Mouse and keyboard, Absolute mouse and keyboard, Switch gamepad, and PS4 arcade stick. — [HID Remapper Configuration v9](https://jfedor.org/hid-remapper-config/v9); [newreleases r2023-09-27](https://newreleases.io/project/github/jfedor2/hid-remapper/release/r2023-09-27)
- The PS4 option is an arcade-stick emulation. It needs an officially licensed PS4 arcade stick or a dongle such as Mayflash MagicBoots PS4 v1.1 or PXN N5 for auth. — [newreleases summary / HID Remapper release notes](https://newreleases.io/project/github/jfedor2/hid-remapper/release/r2024-12-05)
- Recent coverage: Steam Controller support on Switch / Switch 2 with a working trackpad. — [Windows Report](https://windowsreport.com/steam-controller-gets-full-hid-remapper-support-for-switch-2-and-android/); [SteamDeckHQ](https://steamdeckhq.com/news/hid-remapper-steam-controller/)
- Activity: pre-releases r2026-11-06-test1 (07 Nov 2026), r2026-10-29-test1, r2026-10-22-test1, r2026-10-21-test1, r2026-08-29-test. Recent notes mention Pico 2 W boards. — [HID Remapper Releases](https://github.com/jfedor2/hid-remapper/releases)

### Inferences
- HID Remapper is the closest OSS analogue to "configurable mouse-to-gamepad" with a mature UI. Its config model (usage-to-usage mappings with scaling, stored as a binary config, edited over WebHID) is a strong design template for an ESP32-S3 port, since the remapping core is largely MCU-agnostic C++. Porting would require replacing the RP2040 PIO-USB/TinyUSB glue with ESP32-S3 USB OTG host plus TinyUSB device. The S3 has only one USB OTG peripheral, so host and device at the same time needs a second MCU or a different approach.
- For PS4/PS5 it would rely on an external auth dongle, the same as the "arcade-stick plus MagicBoots" approach.

### Gaps
- No primary source fetched explicitly documented mouse X/Y to stick-axis mapping or its curve/deadzone handling. This is likely possible via the generic mapping engine, but unverified here.
- No 2026 non-test (stable) release date was confirmed.

## Q3. GIMX (PC-based adapter) and its mouse translation

### Takeaway
GIMX (GPL-3.0) is the historical open-source XIM equivalent. A PC does the mouse-to-stick translation and drives a USB adapter or Bluetooth link to PS3/PS4/Xbox. PS5 support is effectively broken by auth hardening, and project activity appears low.

### Cited Findings
- GIMX is free software that turns a Windows/Linux PC into a hub for gaming devices. It connects to PlayStation and Xbox consoles via a USB adapter (sold by the project) or a Bluetooth dongle (Bluetooth only for PS3/PS4). GPL-3.0. 1,971 commits, 653 stars, 122 forks, 77 open issues. No GitHub releases are listed. — [GitHub: matlo/GIMX](https://github.com/matlo/GIMX)
- A Tindie listing sells a "GIMX x86 USB mouse & keyboard adapter for PS4 or Xbox". — [Tindie](https://www.tindie.com/products/jcrorxp/gimx-x86-usb-mouse-keyboard-adapter-for-ps4-xbox/)
- PS4 use needs an official PS4 controller connected for auth. — [rewasd blog (third party)](https://rewasd.com/blog/post/how-to-use-keyboard-and-mouse-on-ps4); [GIMX FAQ](https://gimx.fr/wiki/index.php?title=FAQ)
- PS5: forum posts say Sony hardened controller auth, so the adapter "currently works no more than 8 minutes on PS5". Also that Sony blocked third-party PS4 controllers for newer PS5 games. One user claimed a GIMX firmware build works on PS4/PS5/Xbox One/Series (single unverified report). — [GIMX forum: Will GIMX work on PS5?](https://forum.gimx.fr/viewtopic.php?p=20158); [GIMX forum: PS5 yes or no](https://forum.gimx.fr/viewtopic.php?p=19973)
- GP2040-CE credits GIMX (with Santroller) for technical examples of Xbox One auth via pass-through. — [GitHub: GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE)

### Inferences
- GIMX's mouse-calibration model (deadzone, multiplier, exponent, acceleration-ish curves tuned per game, "mcal") is the best open-source reference for XIM-like translation "feel". Its config format and math could be ported to run on-device on an ESP32-S3.
- The "8-minute" limit on PS5 when using a PS4-controller auth pattern is a key feasibility constraint for any DIY adapter targeting PS5 games that need a DualSense.

### Gaps
- The GitHub README did not describe the mouse translation parameters. Details of GIMX's translation algorithm were not verified from a primary doc in this session (the GIMX wiki "Mouse calibration" page was not fetched).
- Last commit/release date for GIMX was not determined. Activity is judged low only from undated forum posts and the absence of releases.

## Q4. Joypad OS (formerly USBRetro), OGX-Mini, BlueRetro, Bluepad32, and other ESP32-S3-relevant projects

### Takeaway
Joypad OS (Apache-2.0) is the most ESP32-S3-relevant multi-target framework. It supports RP2040, ESP32-S3 (currently a BLE-only bt2usb app) and nRF52840, and offers USB-device outputs (XInput, PS3/PS4, Switch) plus a PS4 app with "local RSA auth, passthrough". OGX-Mini and its ESP32-S3 sibling OGX-Wireless-Lite, BlueRetro and Bluepad32 are controller-oriented building blocks; none documents mouse-to-stick.

### Cited Findings
- **Joypad OS:** Apache-2.0 firmware for controller adapters. MCUs: RP2040, ESP32-S3, nRF52840. The ESP32-S3 `bt2usb` app is BLE-only, built with ESP-IDF. Inputs: USB host controllers plus "USB keyboards, mice, hubs, Bluetooth dongles"; Classic BT/BLE on Pico W; BLE on ESP32-S3/nRF52840; native retro controllers. Outputs: USB device (XInput, PS3/PS4, Switch, etc.), retro console protocols via PIO, keyboard plus mouse. The PS4 app (`usb2usb`) lists "Local RSA auth, passthrough, rumble". Mouse-to-analog mapping is not mentioned. — [GitHub: joypad-ai/joypad-os](https://github.com/joypad-ai/joypad-os)
- Joypad OS is the renamed USBRetro. Recent releases v2.0.0 through v2.3.0 (v2.3.0 was very recent at search time). Composite USB device with gamepad, keyboard and mouse interfaces is documented. — [newreleases v2.3.0](https://newreleases.io/project/github/joypad-ai/joypad-os/release/v2.3.0); [usb-device doc mirror](https://github-users.yhajoa.workers.dev/joypad-ai/joypad-os/blob/main/docs/output/usb-device.md)
- **OGX-Mini (wiredopposite and forks):** RP2040 firmware emulating gamepads for Original Xbox, PS3, PS4 and docked Switch. Wired inputs include Xbox, DS3/DS4, DualSense, Switch Pro, Steam and Stadia. Bluetooth via Pico W / ESP32 is early-stage. A Pico plus ESP32 build exists. — [GitHub: LukeZGD/OGX-Mini fork](https://github.com/LukeZGD/OGX-Mini); [newreleases v1.0.0-alpha2](https://newreleases.io/project/github/wiredopposite/OGX-Mini/release/v1.0.0-alpha2)
- **OGX-Wireless-Lite:** separate ESP32-S3 firmware using BLE gamepads on several consoles. — [GitHub: wiredopposite/OGX-Wireless-Lite](https://github.com/wiredopposite/OGX-Wireless-Lite)
- **OGX-Mini-2026 (MegaCadeDev fork):** active 2026 fork. Its STEAM output mode includes a touchpad mouse, but its DualSense emulation has no mouse and is gamepad-only. — [GitHub: MegaCadeDev/OGX-Mini-2026](https://github.com/MegaCadeDev/OGX-Mini-2026)
- **BlueRetro:** ESP32 firmware (darthcloud) for Bluetooth controllers on retro consoles. Lists keyboard and mouse among its inputs. — [BlueRetro on itch.io](https://darthcloud.itch.io/blueretro)
- **Bluepad32:** Bluetooth gamepad, mouse and keyboard host support for ESP32 (incl. S3) and Pico W. It is a host library and does no mouse-to-stick translation. — [GitHub: ricardoquesada/bluepad32](https://github.com/ricardoquesada/bluepad32)
- **jocover/esp32s3_xbox_adapter:** ESP32-S3 project bridging Xbox Series controllers to PS4/PC. Controller input only, not mouse/keyboard. — [awesome.ecosyste.ms entry](https://awesome.ecosyste.ms/projects/github.com%2Fjocover%2Fesp32s3_xbox_adapter)
- **stadia-dongle-esp:** ESP32-S3 firmware bridging Stadia BLE controllers to PC as a wired Xbox 360 controller plus HID mouse plus keyboard. — [stadia-dongle-esp](https://danzig666.github.io/stadia-dongle-esp/)
- **mortara/ps2usb:** ESP32-S3 USB keyboard/mouse to PS/2. It demonstrates S3 USB-host HID parsing. — [awesome.ecosyste.ms entry](https://awesome.ecosyste.ms/projects/github.com%2Fmortara%2Fps2usb)
- **touchgadget/usbnswa and blemouse2xac:** small adapters (USB to Switch, BLE mouse to Xbox Adaptive Controller). These are relevant prior art for mouse-to-joystick for the XAC. — [github.com/touchgadget/usbnswa](https://github.com/touchgadget/usbnswa); [blemouse2xac mirror](https://github.imc.re/touchgadget/blemouse2xac)

### Inferences
- For an ESP32-S3 build, Joypad OS is the strongest code base to fork or borrow from: it already targets ESP32-S3 with ESP-IDF, has a PS4 auth/passthrough implementation, and uses a permissive license. Its ESP32-S3 path is BLE-input only today, so wired USB-host mouse input on S3 would be new work. Because the S3 has one USB OTG port, a wired-mouse-in plus USB-device-out design needs either BLE mouse input, a second MCU or USB host chip (for example MAX3421E or a second S3/RP2040), or a different split.
- OGX-Wireless-Lite and Bluepad32 show that BLE HID host plus USB device output on a single S3 is practical. A BLE gaming mouse, however, adds latency and polling limits relative to wired 1 kHz mice.

### Gaps
- ESP32-S3 maturity in Joypad OS, its supported USB output modes on S3, and whether PS4 auth runs on S3 were not verified (an ESP32 platform doc exists but was not fetched).
- Santroller (sanjay900) was not fetched directly. Known here only via GP2040-CE credits for Xbox 360/One auth techniques. Its license and activity are unverified.
- BlueRetro license, latest release and whether its mouse input maps to analog sticks were not verified.
- No "pico-xim"/"usb2ps" project was surfaced by searches.

## Q5. MAKCU / KMBox-like ESP32-S3 hardware with open firmware

### Takeaway
MAKCM (terrafirma2021) is an open-source dual-ESP32-S3 "MAKCU" mouse passthrough/injection device. Controller support and "eliminating the need for XIM" are only roadmap items. Its dual-S3 USB-host plus USB-device split is directly relevant hardware prior art for solving the S3 single-OTG limitation.

### Cited Findings
- MAKCM: "new open-source implementation for remote injection of mouse control with passthrough". Uses two ESP32-S3 MCUs (left and right; each 4 MB flash, 2 MB PSRAM), 6 Mbps serial between them, RTOS tasks, automatic HID report-descriptor parsing and JSON-streamed USB descriptors. Folders: `MAKCM_ESP32s3_Device_Mouse_Left` and `MAKCM_ESP32s3_HID_Mouse_Right`. Contains `PS5_Controller.txt` and `X_controller.txt`. Roadmap: "comprehensive controller support", eliminating the need for XIM. License is inconsistent (GPL-3.0 in the sidebar, MIT in the README). 146 stars, 29 forks, 48 commits. — [GitHub: terrafirma2021/MAKCM](https://github.com/terrafirma2021/MAKCM)
- MAKCU's own FAQ claims it is "completely free and open source" and contrasts itself with KMBox. Its DIY guide points to an "ESP32s3_KMBOX" guide. These are vendor claims. — [MAKCU FAQ](https://makcu.gitbook.io/makcu/faq); [MAKCU DIY guide](https://makcu.gitbook.io/makcu/diy-guide)
- Commercial pre-flashed ESP32-S3 MAKCU units are sold on Tindie. — [Tindie: MAKCU ESP32-S3](https://www.tindie.com/products/topmacro/makcu-esp32-s3-usb-controller-in-topmacro-case/)

### Inferences
- MAKCU/KMBox devices are primarily marketed around PC mouse-input injection. Those use cases overlap with anti-cheat evasion, which was out of scope and not researched. The relevant reusable part is the dual-S3 architecture: one S3 as USB host for the mouse, a fast UART link, and one S3 as USB device. This is a proven pattern for an S3-based XIM-like adapter.

### Gaps
- Last commit date and whether controller output was ever implemented in MAKCM are unknown.

## Q6. Console auth handling (high level)

### Takeaway
No open-source project "breaks" PS4/PS5/Xbox auth. They either relay auth challenges to a genuine controller or licensed dongle (passthrough), use a licensed arcade stick or dongle (MagicBoots, PXN), or emulate a console whose auth is weaker (Switch, PS3, Xbox 360 per GP2040-CE). PS5 with PS5-native games remains the hard case.

### Cited Findings
- GP2040-CE: PS4/PS5 mode via USB dongle (auth can be lost on flash save). Controller USB-host passthrough with DS4/Stadia. Xbox One auth via passthrough (credited to Santroller/GIMX). Xbox 360 authenticates without a dongle as of v0.7.12. — [GP2040-CE Releases](https://github.com/OpenStickCommunity/GP2040-CE/releases); [GitHub: GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE)
- Joypad OS PS4 app: "Local RSA auth, passthrough". — [GitHub: joypad-ai/joypad-os](https://github.com/joypad-ai/joypad-os)
- HID Remapper: PS4 arcade-stick mode needs a licensed arcade stick or a MagicBoots/PXN dongle. — [newreleases HID Remapper](https://newreleases.io/project/github/jfedor2/hid-remapper/release/r2024-12-05)
- GIMX: a genuine PS4 controller is needed for PS4 auth. On PS5, a PS4-auth-based adapter reportedly works for about 8 minutes, and newer PS5 games block PS4-class controllers. — [GIMX forum](https://forum.gimx.fr/viewtopic.php?p=20158); [GIMX forum](https://forum.gimx.fr/viewtopic.php?p=19973)

### Inferences
- "Local RSA auth" for PS4 in Joypad OS likely means signing with a user-supplied key extracted from a licensed device, rather than a bypass. Not verified.

### Gaps
- No open-source DualSense (PS5-native) auth solution was found.

## Q7. Community reports on DIY XIM builds (latency, feel vs XIM)

### Takeaway
Searches surfaced no substantive Reddit or GitHub threads reporting a working DIY XIM clone with latency or "feel" comparisons. The community evidence is thin and the best leads are the GIMX forum and the MAKCM roadmap.

### Cited Findings
- A targeted search for DIY XIM / Pico mouse-to-controller Reddit threads returned only adjacent projects (pico-rectangle, DreamPico, a PS2-to-KBM CircuitPython project). — [pico-rectangle](https://github.com/manniefesto/pico-rectangle); [PiPyPS2](https://github.com/nandanhere/PiPyPS2)
- GP2040-CE publishes latency results (v0.7.12 measured), but these concern button input, not mouse translation. — [GitHub: GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE)
- A third-party blog notes that a GIMX adapter showed lower latency than a Bluetooth connection (vendor/third-party, low reliability). — [rewasd blog](https://rewasd.com/blog/post/how-to-use-keyboard-and-mouse-on-ps4)

### Inferences
- The general lesson is that raw mouse-to-stick mapping without tuning (deadzone compensation, curves) feels "sluggish or twitchy". The tuning layer is where XIM and GIMX add value, and it is the main thing a DIY ESP32-S3 project must build itself.

### Gaps
- No Reddit r/XIM, r/esp32 or r/raspberry_pi_pico threads with measured DIY results were found within the tool budget. User reports on GP2040-CE v0.7.12 mouse-to-stick feel were not found.

## Summary catalog (for report writer)

| Project | MCU | Consoles / outputs | Mouse to stick? | Auth approach | License | Activity (as of 2026-11) | ESP32-S3 reuse |
|---|---|---|---|---|---|---|---|
| GP2040-CE | RP2040, early RP2350 | XInput, Switch, PS3, PS4/5, Xbox One, more | Yes (v0.7.12, EXT port mouse to L/R stick) | Dongle/passthrough; X360 no dongle | MIT | Last release v0.7.12, Nov 2025 | Reference for protocols and auth; no S3 port |
| HID Remapper | RP2040, nRF52840 | Mouse/KB, Switch gamepad, PS4 arcade stick | Likely via mapping engine (unverified) | External licensed stick or dongle for PS4 | MIT / CC BY 4.0 | Very active (test builds Nov 2026) | Remapping core and config UX portable |
| Joypad OS (USBRetro) | RP2040, ESP32-S3 (BLE), nRF52840 | XInput, PS3/PS4, Switch, retro consoles | Not documented | PS4 local RSA auth plus passthrough | Apache-2.0 | Active (v2.x releases) | Best S3 base: already builds with ESP-IDF |
| GIMX | PC plus USB adapter or BT | PS3, PS4, Xbox 360/One (PS5 limited) | Yes (PC-side, core feature) | Genuine controller passthrough | GPL-3.0 | Low/unclear | Port the translation math |
| OGX-Mini / OGX-Wireless-Lite | RP2040 (+ESP32); ESP32-S3 | OG Xbox, PS3, PS4, Switch | Not documented | Not detailed | Not verified | Active forks in 2026 | S3 BLE-to-console output reference |
| Bluepad32 | ESP32 family, Pico W | Host library only | No | n/a | Not verified here | Active | BT mouse/KB input on S3 |
| BlueRetro | ESP32 | Retro consoles | Not verified | n/a | Not verified | Not verified | BT input reference |
| MAKCM (MAKCU) | 2x ESP32-S3 | PC mouse passthrough; controller on roadmap | No (roadmap) | n/a | GPL-3.0/MIT (conflicting) | Unclear | Dual-S3 host/device split architecture |

# XIM and Comparable Mouse/Keyboard-to-Console Adapters: Architecture and Mouse-to-Stick Translation

Scope note: the notes describe how these devices are built and how they translate input, so a reader knows what a DIY (e.g., ESP32-S3) design would have to replicate. They do not cover avoiding detection by game or console anti-cheat. Research date: 2026-11-09. Many official XIM pages are thin on hardware detail. Retailer listings (desertcart and similar) are low-authority and are marked as such.

## 1. XIM hardware: MCUs, ports, auth controller, hub, app, firmware

### Takeaway
No public teardown of the XIM APEX, MATRIX or NEXUS was found that names the MCU. The only confirmed chip ID is from the old XIM3, which used an NXP LPC1768 Cortex-M3. The current XIM MATRIX has two input USB ports, one dedicated auth/"special" USB port, a USB-C upstream port to the console, and an Ethernet port for PS5 (Remote Play). It is configured over Bluetooth LE from a phone/PC app and its firmware can be upgraded. The XIM APEX used a separate 3-port hub, and the console controller had to sit in port 3.

### Cited Findings
**MCU / teardown**
- XIM3 (circa 2011) teardown: the central microcontroller is an NXP LPC1768FBD100 (ARM Cortex-M3). This is the only XIM chip ID turned up by search. It is not evidence for the APEX, MATRIX or NEXUS. — [element14 XIM3 teardown (search snippet; page fetch timed out)](https://community.element14.com/technologies/embedded/b/blog/posts/xbox-xim-3-teardown-and-analysis)
- No teardown naming the MCU of the XIM4, APEX, MATRIX or NEXUS turned up in searches. — (gap; see below)

**XIM MATRIX ports and I/O (official guide)**
- USB 1 and USB 2 (left and right sides) are input ports for mice, keyboards and wired controllers. USB 3 (back) is "a special port for console authentication on Xbox and PlayStation 4". A USB-C port (back) connects to the console/PC, with a USB-A-to-C cable included. An Ethernet port is "used to game on a PlayStation 5". A pairing button is used for wireless pairing and flash upgrades. — [XIM MATRIX Guide: Hardware Compatibility](https://guide.xim.tech/Hardware-Compatibility/)
- Wired controllers accepted on USB 1/2: Xbox Elite/Series/One (licensed third-party wired Xbox pads included), DualSense, DS4, DS3, Move Navigation, Switch Pro, XInput/DirectInput, XIM NEXUS. Third-party PlayStation and Nintendo controllers are not supported. Gyro aim works on DualSense, DS4, Switch Pro and Joy-Con. Bluetooth gamepads: Xbox BT controllers, DualSense, DS4, DS3, Switch Pro, Joy-Cons, NEXUS. — [XIM MATRIX Guide: Hardware Compatibility](https://guide.xim.tech/Hardware-Compatibility/)
- Supported outputs: Xbox Series X|S, PS5, Xbox One, PS4, PC. Inputs are "mice, keyboards, gyro controllers, cross console controllers, and any combination thereof". The device is firmware upgradable. The XIM MATRIX Manager app runs on Android, iOS and PC. — [xim.tech XIM MATRIX product page](https://xim.tech/xim-matrix/)
- Configuration is done over Bluetooth LE ("cyan flashing LED" means ready to pair). The Manager needs iOS 13 / Android 6 / Windows 10 x64 with BTLE. The XIM MATRIX and the Manager must run the same version. Users report app disconnects when the phone screen turns off (phone battery-saving settings). — [desertcart retailer listing (low authority)](https://www.desertcart.co.uk/products/657789655-mcbazel-xim-matrix-adapter-for-xbox-series-x-s-ps5.md)
- Guide navigation lists "Updating Firmware", "Manager Wireless Health", "Trace Log" and "Debug Dump" pages, which point to a BLE management link plus a host-side flash update model. — [XIM MATRIX Guide: Velocity Calibration (nav)](https://guide.xim.tech/Velocity-Calibration/)

**XIM MATRIX on PS5 (no USB auth path)**
- On PS5 the MATRIX connects over wired Ethernet using the console's **Remote Play** system. XIM says the link is "equivalent to USB speeds and latency" and stays on the LAN. Video still goes from the PS5 to the TV. — [XIM Guide: Gaming on PlayStation 5](https://guide.xim.tech/Gaming-On-PlayStation-5/)
- Setup: sign in with a Sony account in the Manager (XIM says it does not store the credentials). Register up to 3 PSN accounts. Get a Link Key from the PS5 (Remote Play enabled, PIN disabled, network status Wired, "Pair Device") and enter the 8-digit code. The Link Key step needs a DualSense connected directly to the PS5. A DualSense in port 3 is needed only for a headset. — [XIM Guide: Gaming on PlayStation 5](https://guide.xim.tech/Gaming-On-PlayStation-5/)
- The USB alternative on PS5 needs a third-party "P5General" adapter plugged into port 3. — [XIM Guide: Gaming on PlayStation 5](https://guide.xim.tech/Gaming-On-PlayStation-5/)

**XIM APEX (previous generation)**
- The APEX used an external XIM hub with three ports. Preferred layout: Port 1 = mouse, Port 2 = keyboard/joystick, Port 3 = controller. "XIM APEX requires your console's controller plugged into USB port #3 on your XIM HUB at all times to function." — [Quadra Stores APEX listing quoting XIM support text](https://quadrastores.com/index.php/home/product_view/2846/XIM-APEX)
- APEX Manager: iOS 8+, Android 4.4+, Windows 8/8.1/10. The Windows Manager needed a micro-USB cable ("Bluetooth PC connection not supported"). Firmware updates needed Windows 7+ or macOS 10.11+. — [Quadra Stores APEX listing](https://quadrastores.com/index.php/home/product_view/2846/XIM-APEX)
- APEX Manager exists as an iOS App Store app. — [App Store: XIM APEX Manager](https://apps.apple.com/us/app/xim-apex-manager/id1179213690)
- The APEX Manager exposes a "Response Rate" setting separate from the mouse poll rate. A user reply suggested keeping the mouse at 1000 Hz and setting Response Rate to 125 Hz in the APEX Manager. — [r/XIM thread (via redlib mirror)](https://redlib.belloworld.it/r/XIM/comments/1gjyv2t/polling_rate_question/lvhezjs)

**XIM NEXUS**
- A gyro ("Motion Aim") controller that presents itself as a standard Bluetooth controller. It ships with a wireless adapter for consoles into which "a native console controller must be plugged". About 10 h battery, 24 mappable buttons plus 4 back buttons, onboard "embedded system" to minimise latency. Supports Xbox Series/One, PS4, PC. Uses the same "Smart Translator" aim tech. — [xim.tech XIM NEXUS](https://xim.tech/xim-nexus/)

**XIM4**
- The XIM4 manual says the user should set in-game look sensitivity to maximum. "Smart Translators" aim for a "1:1 connection between the mouse and aiming reticle on-screen up to that speed limit". A fast-blinking red light warns "exceeded maximum turn speed". — [ManualsLib XIM4 manual p.2](https://www.manualslib.com/manual/945796/Ximo-Xim4.html?page=2)

### Inferences
- The MATRIX design has these blocks: (a) a USB host with at least two downstream ports for HID mice, keyboards and gamepads, (b) a third host port for the genuine controller used for auth, (c) a USB device port that presents itself as a console controller, (d) a BLE radio for the app plus Classic BT for wireless gamepads, and (e) an Ethernet MAC/PHY with enough CPU to run a Remote Play client on PS5. Remote Play needs real networking/crypto, so the MCU is probably fairly capable (e.g., a Cortex-M7/i.MX RT-class part or similar). This is speculation; there is no teardown.
- Given the XIM3's LPC1768, NXP Cortex-M lineage is plausible for later XIMs, but this is unconfirmed.
- The XIM4 to APEX generations needed a hub because the adapter exposed a single host port, or few ports. The MATRIX moved to integrated ports.

### Gaps
- No teardown or FCC-photo source naming the MCU, BT chip or USB hub IC of the XIM4, APEX, MATRIX or NEXUS was found. The next step is FCC ID internal photos.
- The official XIM MSRP for the APEX could not be confirmed from a primary source. xim.tech/shop lists only retailers, with no prices.
- The XIM4 port/hub specifics were not retrieved (manual page 1 not fetched).

## 2. Data flow and console-auth passthrough

### Takeaway
All of these devices use the same pipeline. A USB host reads HID reports from the mouse, keyboard and gamepad. A mapping/translation engine produces a virtual gamepad state. A USB device stack emits reports that look like a genuine controller. Console security challenges (Xbox One/Series, PS4) cannot be answered by the adapter itself, so they are relayed to a genuine controller plugged into a dedicated "auth" port, and the responses come back the same way. PS5 USB auth is not answered this way by XIM. It uses Remote Play over Ethernet, or a third-party PS5 dongle.

### Cited Findings
- XIM MATRIX USB 3 is "a special port for console authentication on Xbox and PlayStation 4". — [XIM Guide: Hardware Compatibility](https://guide.xim.tech/Hardware-Compatibility/)
- Titan Two: Input A is for the user's preferred controller or device. Input B is for the console's original controller, "needed for authentication and console-specific features" (PS4, Xbox One, Xbox 360). Output goes to the console via micro-USB. — [ConsoleTuner Titan Two product page](https://www.consoletuner.com/products/titan-two/)
- GIMX (open source) chain: DS4 → PC → USB-to-UART adapter → AVR USB board → PS4. Auth requests from the PS4 pass through the UART to the PC and on to the DS4, and replies take the reverse path. The author notes that PS4 auth traffic is periodic, unlike the Xbox 360's bidirectional flow. — [GIMX blog (search snippet)](https://blog.gimx.fr/category/uncategorized/)
- GIMX's author recommends an ATmega32U4-based board, with the AVR TX level converted to 3.3 V for the USB-UART (CP2102-type) adapter. Users report the DS4 worked only when it was on a separate USB bus from the adapter. — [GIMX forum (search snippets)](https://forum.gimx.fr/viewtopic.php?p=7146); [GIMX forum](https://gimx.fr/forum/viewtopic.php?p=10253)
- ReaSnow S1 manual: a Micro USB port goes to the console or PC. "USB1: Connect to the official wired controller shipped with game console". For PS4/PS3 and Xbox One the console's controller must always be in USB port 1. "For PlayStation 5, a third-party controller is required." — [ReaSnow S1 User Manual (search snippet)](https://www.reasnow.com/download/ReaSnowApp/ReaSnow%20S1-UserManual.pdf)
- To use the ReaSnow S1 on PS5 you need a Besavior P5Mate adapter or Remote Play. — [Ozon listing (low authority)](https://www.ozon.ru/product/adapter-reasnow-s1-1595628580/)
- Brook Wingman P5 is a controller-to-PS4/PS5 converter. It accepts 100+ wired and wireless pads (Xbox 360/One, PS3/PS4, Switch, Switch 2), supports remap, macro, turbo and stick calibration through the Windows "Brook Converter Center", has firmware updates via PC/Mac, and emulates touchpad and motion. The retailer page does not mention keyboard/mouse support or an auth controller. — [Focus Attack Wingman P5 listing](https://focusattack.com/brook-wingman-p5-wired-wireless-controller-to-native-ps4-ps5-converter/)

### Inferences
- In a replication, the auth passthrough is a protocol relay. The adapter enumerates as a controller, forwards the console's auth/security feature or vendor requests to the genuine pad on the auth port, and returns the pad's replies. The cryptographic material stays in the genuine controller. Brook-style converters that need no auth pad appear to carry their own licensed or embedded auth solution. That is inferred, not confirmed by the sources fetched.
- PS5 is the hard case. Both XIM and ReaSnow avoid native PS5 USB auth: XIM through Remote Play over Ethernet, ReaSnow through a third-party PS5 dongle. A DIY ESP32-S3 design would face the same constraint.
- The USB host side needs either a hub-capable host stack (mouse, keyboard and auth pad on one root port through a hub) or several host controllers. The ESP32-S3 has one USB OTG FS port, so it cannot be host and device at the same time on that port. This is general knowledge and should be verified by the hardware-focused researcher.

### Gaps
- No primary-source detail on how XIM relays Xbox Series auth, or on the timing constraints.
- Cronus Zen auth mechanics were not retrieved from official docs. A retailer says the Zen has 5 USB ports and Bluetooth (see section 5).

## 3. Mouse-to-stick translation (XIM Smart Translation, deadzones, curves, rates)

### Takeaway
XIM works in real-world units: mouse speed in cm/360 and reticle speed in °/s. Its "Smart Translators" are per-game models of the game's stick-to-turn-rate response. The adapter inverts that model so mouse velocity maps 1:1 to reticle velocity, up to the game's maximum turn speed. To make that inversion valid, XIM requires specific in-game settings (max sensitivity, specific deadzone and response curve). Smoothing, quantization and "stick upscaling" (sub-LSB dithering on PlayStation's 8-bit sticks) sit on top of the inversion.

### Cited Findings
**Units and the per-game model**
- Mouse sensitivity is set in cm/360 (a lower value turns faster). Accurate real-world velocity needs the correct mouse DPI in the Config. — [XIM Guide: Aim Settings](https://guide.xim.tech/Aim-Settings/)
- Velocity Calibration "teach[es] your XIM MATRIX how the reticle behaves in the game". It measures reticle velocity in °/s by turning a full circle (X) and sweeping down to up (Y), repeated for ADS. XIM "operates in real world units", so Configs can be shared across hardware. — [XIM Guide: Velocity Calibration](https://guide.xim.tech/Velocity-Calibration/)
- Controller-output Configs use Smart Translators that already know reticle velocity, so calibration is optional. It is needed only if the user changes in-game sensitivity, ADS sensitivity or FOV. "Deadzone and aiming technique must stay at their required values." XIM recommends 100/100% thumbstick aim sensitivity. Mouse-output (PC) Configs have no Smart Translators and need calibration. — [XIM Guide: Velocity Calibration](https://guide.xim.tech/Velocity-Calibration/)
- XIM4: set in-game look sensitivity to max. The goal is a "1:1 connection between the mouse and aiming reticle on-screen up to that speed limit". The red light warns when the user exceeds the game's maximum turn speed. — [ManualsLib XIM4 manual](https://www.manualslib.com/manual/945796/Ximo-Xim4.html?page=2)
- Smart Translation is "proprietary aim translation technology". "True Aim Curves" "respond as designed". — [xim.tech XIM MATRIX](https://xim.tech/xim-matrix/)

**Deadzones and curves**
- Inner deadzone = zero reticle velocity from centre, defaulting to the game's native deadzone. Outer deadzone = maps to max reticle velocity. Axial deadzone = zero velocity along the X/Y axes. Offsets correct stick drift, and the coordinate system can be rotated. "The game's deadzone settings must match the required settings for that game." — [XIM Guide: Aim Settings](https://guide.xim.tech/Aim-Settings/)
- Aim curves map 0 to 100% of reticle velocity, linear by default, with editable handles and presets. Authored curves are not layered on top of the game's own curve: "the curves you author will be the aim response you experience in game". For thumbstick input, a custom curve needs "Custom Stick Behavior", which replaces the game's native mechanics. — [XIM Guide: Aim Settings](https://guide.xim.tech/Aim-Settings/)
- Velocity mapping (controller output) has four modes: Standard, Uniform, Limit (cap) and Reduce (subtract a set velocity from the game's per-angle maximum). Maximum turn speed still applies. — [XIM Guide: Aim Settings](https://guide.xim.tech/Aim-Settings/)

**Smoothing / "steady aim"**
- Standard (dynamic) smoothing has four parameters:
  - Precision: smoothing at low speed (heavier reticle).
  - Response: lighter feel at high speed.
  - Easing: start from rest.
  - Stability: motion aim only, reduces hand jitter.
- Classic smoothing is "based on the XIM APEX". It has Smooth plus Decay (9.0 gives APEX-equivalent behaviour) and Synch (8/16/32/0 match the APEX presets). — [XIM Guide: Aim Settings](https://guide.xim.tech/Aim-Settings/)
- Quantization snaps reticle velocity to set magnitudes and angles to "improve compatibility with aim assist". It can reduce real-world sensitivity accuracy. — [XIM Guide: Aim Settings](https://guide.xim.tech/Aim-Settings/)

**Stick resolution / upscaling**
- Xbox sticks are 16-bit and PlayStation sticks are 8-bit ("256 times less precise"). "Stick Upscaling" simulates higher-resolution aim on PlayStation. By default it is active only when firing while ADS, and it should be used only on PlayStation. — [XIM Guide: Aim Settings](https://guide.xim.tech/Aim-Settings/)

**Poll rates / console input rates**
- APEX-era users ran mice at 1000 Hz and set a separate "Response Rate" (e.g., 125 Hz) in the Manager. — [r/XIM thread mirror](https://redlib.belloworld.it/r/XIM/comments/1gjyv2t/polling_rate_question/lvhezjs)
- The standard DualSense uses bInterval 4 over USB (250 Hz). The DualSense Edge reports at 1 kHz over USB (per an SDL patch). Standard DualSense can be "overclocked" to 1000 Hz on PC by changing bInterval. — [SDL discourse: 1 kHz for DualSense Edge](https://discourse.libsdl.org/t/sdl-allow-1-khz-sample-rate-for-dualsense-edge-over-usb/60373); [gpad guide (low authority)](https://gpad.pages.dev/guides/polling-rate-overclocking); [reWASD forum](https://forum.rewasd.com/forum/rewasd/technical-questions-aa/243294-why-virtual-ds4-performance-mode-is-not-500hz)
- Titan Two claims an internal response time of 50 to 250 µs on average. — [ConsoleTuner Titan Two](https://www.consoletuner.com/products/titan-two/)
- XIM says the PS5 Remote Play link is "equivalent to USB speeds and latency". This is a vendor claim with no measurement. — [XIM Guide: PS5](https://guide.xim.tech/Gaming-On-PlayStation-5/)

### Inferences
- Core math a replica needs (inferred from the XIM docs above, not from XIM code):
  1. Mouse counts per report → physical speed: v_mouse [cm/s] = counts/DPI × 2.54 / Δt.
  2. Desired reticle angular speed: ω_target [°/s] = v_mouse × 360 / (cm/360 setting).
  3. Invert the game's stick response, the function f(stick deflection, angle) → ω. f includes the in-game deadzone, response curve, sensitivity, ADS multiplier and any acceleration ramp. Solve deflection = f⁻¹(ω_target), applying the inner-deadzone offset so tiny motions are not swallowed.
  4. Clamp at the outer deadzone, which is the game's max turn speed (the XIM4 red-light warning).
  5. Smooth across reports and quantize to the output resolution: 8-bit on PS, with dithering ("upscaling") across frames to simulate sub-LSB values.
- Games' own curves and deadzones matter because the stick commands a rate, not a position. Any nonlinearity or hidden deadzone breaks a 1:1 mapping unless it is inverted exactly. That is why XIM fixes required in-game settings per title and offers calibration otherwise. In-game "acceleration" (turn-rate ramp-up over time at full deflection) is time-dependent and is the hardest part to invert.
- Mouse report rate (1000 Hz) higher than console report rate (125 to 1000 Hz) means the adapter must accumulate and average the mouse counts in each console frame, and carry over the remainders. A higher mouse rate gives finer velocity estimates and lower quantisation jitter. The 1000 Hz recommendation is common community lore. A primary XIM statement recommending 1000 Hz was not retrieved.

### Gaps
- No public reverse-engineering of XIM's Smart Translator tables or file format was found.
- A primary XIM source stating "use a 1000 Hz mouse" was not retrieved. Only the anecdotal APEX thread was found.
- Authoritative Xbox controller USB report rates (often cited as about 125 to 250 Hz) were not confirmed in this session.

## 4. Open documentation of translation math (GIMX)

### Takeaway
GIMX's source (`core/config.c`) is the best open reference. It applies, in order: a weighted moving-average filter over a buffer of mouse deltas, frequency scaling, a power-law curve (multiplier × |v|^exponent), a dead-zone offset (elliptical or circular in 2D via a polar decomposition), clamping, and sub-position residue carry-over. It does not invert per-game curves, which is the key difference from XIM.

### Cited Findings
- Smoothing in `cfg_process_motion`: m = Σ d_{i−j}·f^j / Σ f^j, over `buffer_size` frames with `filter` weight f. The previous residue is added to the buffer if `subpositions` is on. — [GIMX core/config.c](https://github.com/matlo/GIMX/blob/master/core/config.c)
- Scaling: `val *= gimx_params.frequency_scale` (1D) or `norm = hypotenuse * frequency_scale` (2D). The multiplier and deadzone are scaled by `controller_get_axis_scale()` to the controller's axis range. — [GIMX core/config.c](https://github.com/matlo/GIMX/blob/master/core/config.c)
- Curve (1D): `z = multiplier * copysign(pow(fabs(val), exp), val)`. In aiming mode the deadzone is added to |z| so output starts just outside it. In driving mode the stick accumulates motion and is clamped. — [GIMX core/config.c](https://github.com/matlo/GIMX/blob/master/core/config.c)
- Residue: `copysign(fabs(val) - pow(fabs(ztrunk/multiplier), 1/exp), multiplier*val)`. Values below 0.0039 (about 1/256) are dropped ("256 subpositions"). — [GIMX core/config.c](https://github.com/matlo/GIMX/blob/master/core/config.c)
- 2D: the delta is converted to polar form (hypot, |cos|, |sin|). z = norm^exponent is split by direction. For the circle shape the deadzones are scaled by cos and sin, so the deadzone is round. The offset is applied when |z| ≥ 1. — [GIMX core/config.c](https://github.com/matlo/GIMX/blob/master/core/config.c)
- Forum notes:
  - The sensitivity multiplier is set in sixemuconf, e.g. 0.0035. The smallest step is 1/32767.
  - With a circle shape and different X/Y deadzone values, the deadzone becomes an ellipse.
  - A GIMX fork focused on "1:1 translation" ignores the exponent.
  - [GIMX forum](https://forum.gimx.fr/viewtopic.php?p=1620); [GIMX forum](https://forum.gimx.fr/viewtopic.php?p=1564); [GIMX forum](https://gimx.fr/forum/viewtopic.php?p=15732)

### Inferences
- A practical replica can start from the GIMX pipeline: filter, scale, power curve, deadzone offset, residue. On top of it, add an XIM-style per-game inverse lookup table (measured deflection→°/s curves) to get real-world-unit 1:1 aim.
- GIMX's residue and sub-position handling is the open-source analogue of XIM's "Stick Upscaling" on 8-bit PS axes.

### Gaps
- The GIMX wiki pages (mouse calibration, DIY adapter) returned 404s or empty content during this session. The details above come from source code and forum snippets.
- No peer-reviewed or formal write-up of "response-curve inversion" for specific games was found.

## 5. Cronus Zen / Titan Two / ReaSnow / Brook: architecture and prices

### Takeaway
The Titan Two uses a 66 MHz 32-bit RISC MCU with a high-speed USB 2.0 controller, Input A (user device), Input B (auth controller) and an Output port, an optional BT 4.1 module, and GPC scripting. The Cronus Zen has 5 USB ports and built-in Bluetooth, and runs scripts from 8 × 32 KB slots. Its MCU is unconfirmed. The ReaSnow S1 is a dedicated M&K adapter with a single auth port and needs a third-party dongle for PS5. Brook's Wingman line is a controller-to-controller converter. Prices fall roughly between US$70 and US$125 at MSRP, and resale is higher for XIM.

### Cited Findings
- Titan Two:
  - "Single-Cycle 32bit RISC micro-controller, running at 66MHz" with a High-Speed USB 2.0 controller.
  - Limited internal flash plus a micro-SD slot.
  - Optional Bluetooth module: "Dual-Mode Bluetooth 4.1 Class 1.5", up to 4 connections.
  - Keyboard and mouse can connect directly or be redirected from a PC via Gtuner IV.
  - Scripts: GPC scripts and Gamepacks with "millisecond precision".
  - Response time 50 to 250 µs.
  - Price is not listed on the official page.
  - [ConsoleTuner Titan Two](https://www.consoletuner.com/products/titan-two/)
- The Titan Two manual's "Technical Specifications" lists USB IDs 2508:0032 (device) and 2508:8032 (bootloader). — [Titan Two manual PDF (search snippet)](https://m.media-amazon.com/images/I/91A8FN3IOeS.pdf)
- Cronus Zen:
  - 5 USB ports: 3 for controllers and accessories, 1 for PC programming, 1 to the console.
  - Built-in Bluetooth.
  - 8 × 32k script slots.
  - Retailer price €99.00, release listed as 1 June 2021.
  - [Konsolinet (retailer)](https://en.konsolinet.fi/product/24933/)
- Cronus Zen MCU: not found in any teardown search. — (gap)
- ReaSnow S1:
  - Launch price ¥14,800 incl. tax (Japan, Nov 2019). [Game Watch Impress](https://game.watch.impress.co.jp/docs/news/1218799.html)
  - Ozon listing at ₽16,226. [Ozon](https://www.ozon.ru/product/adapter-reasnow-s1-1595628580/)
- Brook Wingman P5: US$69.95 at Focus Attack. — [Focus Attack](https://focusattack.com/brook-wingman-p5-wired-wireless-controller-to-native-ps4-ps5-converter/)
- XIM MATRIX:
  - Retails for $125 from XIM's webstore. Average reseller price is about $200 (undated). [Resell Calendar](https://resellcalendar.com/?p=26903)
  - UK marketplace listings run from about £180 to £277. [desertcart UK (low authority)](https://www.desertcart.co.uk/products/657789655-mcbazel-xim-matrix-adapter-for-xbox-series-x-s-ps5.md)

### Inferences
- The commercial pattern is consistent: a single mid-range MCU with USB host and device capability, plus a hub or multiple host ports, a BT module, and app or PC-tool configuration. The Titan Two's 66 MHz part shows that the translation and relay work does not need much compute. XIM's Remote Play support on PS5 is the outlier that needs more compute and networking.
- The Cronus/Titan products focus on scripting (GPC bytecode on device). XIM focuses on calibrated aim translation in real-world units.

### Gaps
- No primary MSRP was found for the Cronus Zen (often cited as US$99.99), the Titan Two, the XIM APEX or the XIM NEXUS.
- No teardown was found for the Cronus Zen, ReaSnow S1 or Brook converters.
- ReaSnow S1 translation settings were not retrieved (the manual PDF could not be text-extracted).

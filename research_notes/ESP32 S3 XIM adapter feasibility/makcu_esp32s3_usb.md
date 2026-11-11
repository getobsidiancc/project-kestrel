# MAKCU Hardware Architecture and ESP32-S3 USB Capabilities/Limits (for a mouse/keyboard-to-controller adapter)

Research date: 2026-11-09. Sources are mostly vendor docs (Espressif, PJRC, Adafruit), the MAKCU GitBook, the MAKCU developer's GitHub, and community write-ups. The "confirmed" label is used only for primary or vendor documentation. MAKCU-specific facts come from the project's own docs and repos, not from independent teardowns.

## Q1. What is the MAKCU in hardware terms? (chips, ports, inter-chip link, firmware, flashing, price, intended use)

### Takeaway
The MAKCU is a "KMBox alternative": a small board with **two ESP32-S3 MCUs** plus a **WCH CH343 USB-to-UART bridge**. One S3 runs USB **host** for the physical mouse. The other runs USB **device** toward the main PC. They talk over UART, and the CH343 gives a second PC a high-baud serial command port. It grew out of an open-source DIY project (two stacked ESP32-S3 DevKits) by GitHub user terrafirma2021. Firmware is split into "LEFT" and "RIGHT" images, flashed with an "AIO Tool". The open-source MAKCM repo (license unclear) predates the V3 beta binaries, which are distributed as .bin files. I found no USB hub or external PHY in any source.

### Cited Findings
**Origin and chip count**
- The DIY predecessor "ESP32s3_KMBOX" uses **two ESP32-S3 dev kits (YD-ESP32-S3 N16R8) stacked**. The bottom board ("ESP-B") runs **USB Host mode for the mouse** and needs the USB_OTG pads soldered. The top board ("ESP-A") runs **USB Device mode** and also accepts "KMBOX B Pro" commands through its other USB-C port. The boards communicate over **UART** (pins and baud not given), share ground, and the top board powers the bottom. The README says the project "has been merged into the MAKCU project". — [terrafirma2021/ESP32s3_KMBOX](https://github.com/terrafirma2021/ESP32s3_KMBOX)
- The same README claims average command processing **< 700 µs** for v2. This is a developer claim, not independently measured. — [ESP32s3_KMBOX](https://github.com/terrafirma2021/ESP32s3_KMBOX)
- The MAKCM repo is described as "Open source ESP32-S3 MAKCU". Its README describes "dual ESP32-S3 microcontrollers based on RTOS". Source folders: `MAKCM_ESP32s3_Device_Mouse_Left` (device side), `MAKCM_ESP32s3_HID_Mouse_Right` (host side, using the ESP-IDF `usb_host_*` APIs), and `AIO_Tool`. The architecture is described as "Hybrid Arduino/RTOS". Each MCU has 4 MB quad-SPI flash and 2 MB PSRAM. The internal PHY is switched to OTG mode by an **eFuse burn done automatically on first boot**. Claimed features include automatic HID report-descriptor parsing. Gamepad/controller support (PS5 and Xbox text files are in the repo) is on the roadmap only. — [terrafirma2021/MAKCM](https://github.com/terrafirma2021/MAKCM)
- MAKCM baud claims: the host-facing COM port supports "6 Mbps", and a command changes COM speed from 115k to 5 Mbps. The README does not say which link each figure applies to. — [MAKCM](https://github.com/terrafirma2021/MAKCM)
- MAKCM license conflicts: the README says MIT, but the GitHub sidebar says GPL-3.0. — [MAKCM](https://github.com/terrafirma2021/MAKCM)
- Marketplace listings call the product "ESP32-S3x2" / "Development Boards ESP32-S3x2 Chip". Some listing text is inconsistent, for example also mentioning ESP32-C3. — [eBay listing title](https://www.ebay.de/itm/167610279087), [Fruugo listing](https://www.fruugo.ca/for-makcu-keymouse-box-development-boards-esp32-s3-with-shell-and-cable/p-467460522) (titles from search results only; both pages returned 403 when fetched)
- A Tindie reseller (TopMacro, Ukraine) sells "1 original MAKCU ESP32-S3 board" in a 3D-printed case with **three USB ports**, a BOOT button and a status LED, for **$80**. Intended use is described as "macro, accessibility, automation and DIY input projects". — [Tindie](https://www.tindie.com/products/topmacro/makcu-esp32-s3-usb-controller-in-topmacro-case/)

**USB-UART bridge**
- The official FAQ says the board uses an ESP32-S3 and a **CH343 UART controller "up to 4 Mbps"**. It also says firmware is "free and open source" (no repo named), every unit is factory-assembled and tested, sales go through **AliExpress or Taobao**, and "Cherry USB" is credited as a USB developer. It claims "zero latency", which is unverified marketing. — [MAKCU FAQ](https://makcu.gitbook.io/makcu/faq/frequently-asked-questions.md)
- The device enumerates on the PC as "USB-Enhanced-SERIAL CH343", **VID 0x1A86 / PID 0x55D3**. It uses a text command protocol (e.g., `km.version()\r` returns `km.MAKCU`) and "Supports all b pro commands", meaning it is KMBox B Pro compatible. — [MAKCU "Adding Makcu support"](https://makcu.gitbook.io/makcu/chair-devs/adding-makcu-support.md)
- WCH rates the CH343 UART at 50 bps to **6 Mbps**. Auto-baud works only at ≤115200. — [CNX Software on CH343](https://www.cnx-software.com/2022/04/19/wch-ch343-usb-to-serial-chips-support-1-8v-to-5v-io-voltage-6-mbps-baud-rate-custom-vid-pid/); [Waveshare CH343 wiki](https://waveshare.com/wiki/CH343_USB_UART_Board)

**Ports and roles**
- Official board guide, normal use: the "farthest-left" cable goes to the **main PC** (USB device side), the "middle" cable to the **secondary PC** (CH343 serial command port), and the "farthest-right" cable to the **mouse** (USB host side). For flashing, use the LEFT port while holding the LEFT button, then the RIGHT port while holding the RIGHT button. Flashing is done from the second PC with the AIO tool (Flash Left / Flash Right / TEST). The CH343 driver is required. — [MAKCU Board Guide](https://makcu.gitbook.io/makcu/makcu-board/makcu-board-guide.md)
- The DIY "Soldered" guide uses a four-port layout: top-left = main PC, bottom-left = mouse, top-right = second PC, bottom-right unassigned in normal use. The top-right port is used to flash LEFT firmware and the bottom-right port for RIGHT firmware. On the DevKit variant, flashing is automatic. — [MAKCU DIY Soldered guide](https://makcu.gitbook.io/makcu/diy-guide/soldered.md)
- A third-party flashing guide describes the same LEFT/RIGHT button-plus-port procedure. The AIO Tool is downloaded from GitHub `terrafirma2021/MAKCM_v2_files` and auto-updates. — [ducks-services GitBook](https://ducks-services.gitbook.io/dma/makcu/makcu-flashing)

**Firmware**
- "Makcu V3" firmware is distributed as **BETA binaries** (`LEFT_v3_BETA.bin`, `RIGHT_V3_BETA.bin`), flashed left first, then right, using "aio offline". — [MAKCU V3 page](https://makcu.gitbook.io/makcu/makcu-board/makcu-v3.md)
- The MAKCU GitBook includes an "XIM Matrix Guide". It only covers plugging the MAKCU into the XIM Matrix's USB port and setting up XIM Matrix Manager. **No console/gamepad emulation is claimed for the MAKCU itself.** — [XIM Matrix Guide](https://makcu.gitbook.io/makcu/makcu-board/xim-matrix-guide.md)

**Price and where sold**
- Third-party listings range from about €22 to $74. Examples: Fruugo CA $35.95 + shipping; Newegg (VersaCom) $73.99 for a "MAKCU V2… pre-flashed with full passthrough v3 firmware"; Tindie $80 in a custom case. Official channels are AliExpress and Taobao. — [Tindie](https://www.tindie.com/products/topmacro/makcu-esp32-s3-usb-controller-in-topmacro-case/); [MAKCU FAQ](https://makcu.gitbook.io/makcu/faq/frequently-asked-questions.md); other prices are from search-result snippets of Fruugo/Newegg/OnBuy and were not fetched (403).

**Intended use**
- The MAKCU docs have a "chair devs" section ("Adding Makcu support") and a V3 section titled "Connect Makcu to the cheat". The product is positioned as a KMBox B Pro-compatible mouse passthrough with external serial command injection from a second PC. — [MAKCU GitBook index (llms.txt)](https://makcu.gitbook.io/makcu/llms.txt); [MAKCU V3](https://makcu.gitbook.io/makcu/makcu-board/makcu-v3.md)

### Inferences
- MAKCU's architecture is "one S3 per USB role, bridged by UART". This matches the ESP32-S3 limit of one USB role at a time (see Q2). It is evidence that a dual-S3 design is a proven, shippable pattern for host-in/device-out passthrough.
- MAKCU firmware is clearly replaceable. Both MCUs are ordinary ESP32-S3s flashable over their USB ports (BOOT/button + USB, or via the AIO tool, which uses Espressif's flashing toolchain). The original DIY version runs on stock dev kits. One caveat: MAKCM burns an eFuse to switch the PHY to OTG, which is irreversible. After that, USB-Serial-JTAG on that PHY is gone and the chip must be flashed via ROM download mode / OTG DFU or UART. This is inferred from the MAKCM README and the Espressif PHY-sharing docs.
- As shipped, the MAKCU is a mouse passthrough to a PC, not a console controller emulator. Building a mouse/keyboard-to-gamepad adapter on it would need custom firmware on the "device" S3 that presents a gamepad descriptor instead of a mouse. Console authentication is outside this note's scope.

### Gaps
- I found no published schematic, PCB files or teardown photos of the commercial MAKCU board. The repo includes STL files but no MAKCU PCB design files. Whether the CH343 connects to the device-side S3, the host-side S3, or both is not documented.
- The exact inter-chip UART baud rate and pins were not found. The MAKCM README mentions "6 Mbps" and a 115k–5 Mbps command range without saying which link. The FAQ says CH343 "up to 4 Mbps".
- Whether V3 firmware source is public was not determined. Only V3 BETA .bin files are referenced, while the older MAKCM source is public.
- No official MAKCU MSRP was found. Official sales are via AliExpress/Taobao links in the GitBook (not fetched).

## Q2. ESP32-S3 USB: can OTG be host and device at once? Endpoints, speeds, stacks, PHY

### Takeaway
**No.** The ESP32-S3 has one USB 1.1 Full-Speed (12 Mbps) OTG controller. It can be **host or device, not both at once**; roles can be switched in software. It shares a single internal PHY with the device-only USB-Serial-JTAG block. An external FS PHY lets OTG and USB-Serial-JTAG run at the same time, but USB-Serial-JTAG is device-only, so this gives no second host port. There is **no high-speed** support. Device mode allows EP0 + 6 configurable endpoints with **max 5 IN endpoints active, including EP0**. Host mode has **8 channels**. To be host and device at once you need two chips, or an S3 plus an external host controller (MAX3421E or similar).

### Cited Findings
**Single role at a time**
- Espressif FAQ: the S3 OTG interface "can not be used as USB Host and USB Device at the same time"; switching by software is possible. Standard OTG role negotiation is supported in hardware only, not in the software protocol. — [ESP-FAQ USB](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html)
- USB-OTG and USB-Serial-JTAG share one PHY, so only one can run at a time. An **external PHY** lets both run independently. Tested external PHYs: **SP5301, TUSB1106** (direct), **STUSB03E** (needs an analog switch). Configured via `usb_phy_ext_io_conf_t`, `skip_setup = true`. — [esp-usb Device docs (S3)](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html); [esp-usb Host docs (S3)](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_host.html)
- USB-Serial-JTAG "is device only; you cannot connect a device to it" (Espressif forum staff, via search snippet). — [esp32.com thread](https://esp32.com/viewtopic.php?p=124219)
- ESP-IoT-Solution USB overview: enabling both controllers simultaneously is possible by adding an external PHY. — [ESP USB Peripheral Introduction](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_overview/usb_overview.html)
- The ESP32-P4 has two USB controllers (HS OTG 2.0 + FS OTG). — [Daniel Mangum, USB OTG on ESP32-S3](https://danielmangum.com/posts/usb-otg-esp32s3/); [ESP-FAQ USB](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html)

**Speed and pins**
- One USB Full-Speed (USB 1.1) peripheral with internal PHY. D+/D− on **GPIO20/GPIO19**. — [esp-usb Device docs](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html)
- Host supports **FS and LS only**. Only ESP32-P4 (and the newer ESP32-S31) support USB 2.0 High-Speed; ESP32-S2/S3 do not. — [esp-usb Host docs](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_host.html); [ESP-FAQ USB](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html)
- S2/S3 FS throughput is limited by CPU polling: about 6.4 Mbps expected with TinyUSB, about 9.6 Mbps theoretical for bulk. — [ESP-FAQ USB](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html)

**Device-mode endpoint limits**
- "Endpoint 0 plus six additional endpoints" configurable as IN or OUT. **"Up to five IN endpoints, including Endpoint 0, can be active at the same time."** — [esp-usb Device docs (S3)](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html)
- Older ESP-IDF wording: "limited to 6 USB endpoints (5 IN/OUT endpoints and 1 IN endpoint)". — [ESP-IDF v5.1 USB Device Driver](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/api-reference/peripherals/usb_device.html)
- esp-hal (Rust) source cites the TRM: six additional endpoints (1–6) configurable IN or OUT; max = 7 including EP0. — [esp-hal otg_fs.rs](https://docs.espressif.com/projects/rust/esp-hal/1.0.0/esp32s3/src/esp_hal/otg_fs.rs.html)

**Device stack**
- The device stack is **TinyUSB**, packaged as the `esp_tinyusb` managed component. Classes: CDC, HID, MIDI, MSC, DFU, MTP, ECM/NCM/RNDIS, vendor. Composite devices are supported (the HID example is a keyboard+mouse). Remote wakeup is supported. — [esp-usb Device docs](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html)
- A self-powered device must monitor VBUS through a divider or comparator, since S3 pins are 3.3 V. It must detect unplug within 3 ms. — [esp-usb Device docs](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html)

**Host stack**
- ESP-IDF USB Host Library supports Control, Bulk, Interrupt and Isochronous transfers ("only supports asynchronous transfers"). Multiple class-driver clients are supported. Only one configuration per device; no transfer timeouts. — [esp-usb Host docs](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_host.html)
- **8 host channels** on S2/S3; ESP32-P4 HS host has 16. Enumeration needs one free channel, and each claimed endpoint uses one channel, so the device count is limited by channels. — [esp-usb Host docs](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_host.html); [ESP-FAQ USB](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html)
- **Hubs:** single-hub support by default (`CONFIG_USB_HOST_HUBS_SUPPORTED`). Multi-level hubs need `CONFIG_USB_HOST_HUB_MULTI_LEVEL`. The external hub driver lacks remote wakeup and error handling. The FAQ says the S3 can host two devices via a hub. An older forum post said "no hub support at this point in time", now superseded. — [esp-usb Host docs](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_host.html); [ESP-FAQ USB](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html); [esp32.com](https://esp32.com/viewtopic.php?p=124219)
- **HID host driver:** the `usb_host_hid` managed component (ESP Component Registry). Its example supports keyboards and mice on the S3. — [esp-usb Host docs](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_host.html)
- Host max packet size limits: the FAQ cites an error "EP MPS (512) exceeds supported limit (408)" for a 4G module. A Hackaday project (DJI motion controller, Dec 2025) hit **"EP MPS (512) exceeds supported limit (128)"** on a periodic (interrupt) endpoint, which the author attributes to the S3 host FIFO split. The `CONFIG_USB_HOST_HW_BUFFER_BIAS_*` options reportedly did not help (ESP-IDF issue #14941, "Won't Do"). Standard mice/keyboards (8–64-byte reports) are unaffected. — [ESP-FAQ USB](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html); [Hackaday.io project 204697 logs](https://hackaday.io/project/204697/logs)
- Max transfer size on S3 = min(65535, 127 × bMaxPacketSize). Light-sleep adds about 90 µs to enter and 40 µs to exit on the S3. — [esp-usb Host docs](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_host.html)
- Arduino-ESP32 note: do not combine "USB Mode: USB-OTG (TinyUSB)" with "USB CDC On Boot: Enabled" on the S3, because of the shared PHY. — [Arduino-ESP32 USB Host API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb_host.html) (via search summary)

**Polling and gaming mice**
- With the S3 at USB 1.1 FS, the arfevrier passthrough caps HID reporting at **1000 Hz (1 ms)** and spaces reports with esp_timer. — [arfevrier blog (30 Oct 2025)](https://blog.arfevrier.fr/usb-passthrough-for-keyboard-and-mouse-using-two-esp32/)

### Inferences
- Full-Speed USB has 1 ms frames, so the minimum interrupt bInterval is 1 ms and the maximum rate is 1000 Hz per endpoint. 2/4/8 kHz polling needs High-Speed 125 µs microframes. **8 kHz (and 2–4 kHz) gaming mice will enumerate at FS and poll at most 1000 Hz** when attached to an ESP32-S3 host. This is standard USB 2.0 spec behavior from my own knowledge, not quoted from a fetched source. Most HS-capable mice fall back to FS, but this was not verified per model.
- For a gamepad device descriptor, the 5-IN-endpoint limit is ample: one HID interrupt IN endpoint plus EP0. It becomes tight only for composite devices, such as gamepad + audio + CDC debug, or vendor protocols needing several IN endpoints.
- 8 host channels are enough for one mouse and one keyboard, even through a hub. Each HID interface typically uses one interrupt IN channel; a hub uses one channel.

### Gaps
- I could not fetch the ESP32-S3 TRM USB chapter directly for FIFO sizes (device/host total FIFO depth). The figures above are from Espressif's esp-usb docs and FAQ.
- I found no Espressif statement on minimum host polling interval or jitter for interrupt endpoints on the S3.

## Q3. Measured latency for ESP32-S3 HID passthrough

### Takeaway
Measured data is scarce. The best published figure is a two-ESP32-S3 (SPI-linked) keyboard passthrough. Its Caps Lock round trip measured **~0.93–1.39 ms (<1.5 ms)**, and the author estimates **≤ ~0.5 ms added** by the bridge. The MAKCU developer claims <700 µs command processing. I found no independent end-to-end mouse latency benchmark of MAKCU or any S3 passthrough.

### Cited Findings
- arfevrier "macroPassthrough": two ESP32-S3s, "usb-input" (USB host, reads keyboard/mouse, hub OK) → **SPI** → "usb-output" (USB device, composite keyboard+mouse HID). A second SPI link carries LED reports back. Caps Lock ping round trip (key press → LED report from PC): about 927–1388 µs, "less than 1.5 ms". The author concludes the ESP32 adds ≤ about 500 µs. UART debug logging adds several ms per print. Date: 30 Oct 2025. Repo: github.com/arfevrier/macroPassthrough. — [arfevrier blog](https://blog.arfevrier.fr/usb-passthrough-for-keyboard-and-mouse-using-two-esp32/)
- ESP32s3_KMBOX (MAKCU predecessor): "average command processing under 700 µs" for v2. This is a developer claim for serial-command handling, not end-to-end mouse latency. — [ESP32s3_KMBOX](https://github.com/terrafirma2021/ESP32s3_KMBOX)
- finger563/esp-usb-latency-test: an ESP32-S3 USB-host tool that actuates a gamepad button via GPIO and times the HID report arrival. It works with 8BitDo Pro 2, Backbone One, DualSense, and Switch Pro. Xbox controllers fail because they use **GIP, not HID** ("No HID device at USB port 1"). No numeric results are published in the README. — [esp-usb-latency-test](https://github.com/finger563/esp-usb-latency-test)
- An Arduino forum thread (Jan 2026) asks exactly the S3 host+device question. The OP reports a Leonardo passthrough polling at about 16 Hz. Replies suggest the RP2040 "HID Remapper" (remapper.org) and note that the Teensy 4.1 has separate device and host ports. No S3 measurements. — [Arduino Forum](https://forum.arduino.cc/t/esp32-s3-usb-host-device-at-same-time-mouse-passthrough-project-performance-alternatives/1426469)

### Inferences
- With FS on both sides, the floor is set by USB frame timing. Up to 1 ms waits for the next host poll on each side, plus inter-chip transfer (tens of µs at multi-Mbps UART/SPI). The best case is roughly 0.5–2 ms added, consistent with the arfevrier measurement. A console's own polling rate for the emulated controller (often less than 1000 Hz) will likely dominate.
- A USB-host-side Xbox controller (GIP) is not HID. This matters if the adapter ever needs to read or authenticate with a real Xbox pad.

### Gaps
- No independent oscilloscope or photodiode end-to-end latency figures for MAKCU or other S3 passthroughs were found.
- No measurements of S3 host-side polling jitter with 1000 Hz mice were found.

## Q4. Alternatives: two-S3, S3 + MAX3421E, RP2040/RP2350 + PIO-USB, Teensy 4.1

### Takeaway
Every proven route to simultaneous host + device uses **two USB controllers**. Options: two S3s (MAKCU, arfevrier); S3 native device + **MAX3421E** SPI host (TinyUSB dual-role example tested on S3); RP2040/RP2350 native USB + **Pico-PIO-USB** software host (HID Remapper ecosystem); or **Teensy 4.1** (480 Mbit device port + separate 480/12/1.5 Mbit host port). Teensy 4.1 is the only listed option with a High-Speed host, so it alone could read an 8 kHz mouse at native rate.

### Cited Findings
- **Two ESP32-S3:** MAKCU uses UART between host-side and device-side S3s ([ESP32s3_KMBOX](https://github.com/terrafirma2021/ESP32s3_KMBOX)). arfevrier uses SPI ([blog](https://blog.arfevrier.fr/usb-passthrough-for-keyboard-and-mouse-using-two-esp32/)). The author notes "only one USB OTG port per ESP32-S3, so two boards are required" and considered STM32 parts with multiple OTG ports. — [arfevrier blog](https://blog.arfevrier.fr/usb-passthrough-for-keyboard-and-mouse-using-two-esp32/)
- **ESP32-S3 + MAX3421E:** Adafruit TinyUSB Arduino library example `device_info_max3421e` runs the "Device on native usb controller (roothub port0)" and the "Host on MAX3421E controller (roothub port1)". It was tested on SAMD21, SAMD51, nRF52840, ESP32-S2, **ESP32-S3**, and ESP32. Caveats: ESP32 Arduino core 3.2.0 broke compilation (use 3.1.3), and the MAX3421E is slower because it uses SPI. — [Adafruit USB Host FeatherWing guide](https://learn.adafruit.com/adafruit-usb-host-featherwing-with-max3421e?view=all)
- The MAX3421E is Full-Speed with a 64-byte FIFO. It failed with the DJI controller's 512-byte endpoints, which is not relevant for mice/keyboards. — [Hackaday.io 204697](https://hackaday.io/project/204697/logs)
- CircuitPython `max3421e` module default SPI clock is **26 MHz**. — [CircuitPython docs](https://docs.circuitpython.org/en/latest/shared-bindings/max3421e)
- **RP2040/RP2350 + Pico-PIO-USB:** "USB host/device implementation using PIO of raspberry pi pico (RP2040/RP2350)". Supports FS host, LS host and FS device, with hub and multi-port support. It uses 1 PIO block, 3 state machines, 32 instructions, 2 GPIOs, about 15 KB, and a 1 ms repeating timer in host mode. TinyUSB example `host_hid_to_device_cdc` runs native USB device plus PIO USB host at once. D+ needs an external 1.5 kΩ pull-up for device mode, and 22 Ω series resistors are recommended. — [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB)
- HID Remapper (RP2040, Pico SDK) is suggested by the community as an off-the-shelf host-to-device remapper. — [Arduino Forum](https://forum.arduino.cc/t/esp32-s3-usb-host-device-at-same-time-mouse-passthrough-project-performance-alternatives/1426469)
- **Teensy 4.1:** Cortex-M7 at 600 MHz. The main USB port is a device at **480 Mbit/s**. The separate USB host port "runs at 480, 12 or 1.5 Mbit/sec" depending on the device. — [PJRC Teensy 4.1](https://www.pjrc.com/store/teensy41.html)
- **ESP32-P4:** has a HS OTG 2.0 controller plus an FS OTG controller, 16 HS host channels, and 8 IN endpoints on HS device (7 usable). It has no radio, so a companion chip is needed for wireless. — [ESP-FAQ USB](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html)

### Inferences
- For a console-facing gamepad emulator that reads mouse/keyboard, ranked by simplicity: (1) a single RP2040/RP2350 with PIO-USB host (one chip, mature TinyUSB support, FS host = 1000 Hz cap); (2) two ESP32-S3s (MAKCU-like, proven, adds a UART/SPI hop); (3) ESP32-S3 + MAX3421E (one MCU, extra IC, SPI overhead); (4) Teensy 4.1 (only option preserving >1000 Hz mouse polling, highest cost). Note: FS device output to the console caps at 1000 Hz anyway, so a HS host mainly helps mice that misbehave at FS.
- An external FS PHY on the S3 does not help this use case. It only frees the PHY for USB-Serial-JTAG, which is device-only. It does not add a second OTG controller.

### Gaps
- No PIO-USB host latency or jitter figures were found. The README has no performance section, and the 120 MHz-multiple clock requirement was not confirmed on the fetched page.
- Teensy 4.1 current price was not shown on PJRC's page (it points to SparkFun).

## Q5. ESP32-S3 Bluetooth: BLE only — implications for PS4/PS5/Switch controller emulation

### Takeaway
**The ESP32-S3 supports Bluetooth LE 5.0 only; no Classic BR/EDR.** DualShock 4, DualSense and Switch Pro controllers use **Classic BR/EDR**, so an S3 cannot natively emulate (or connect to) them over Bluetooth. The **original ESP32** (and Pico W / Pico 2 W) have Classic BT. Xbox Wireless controllers (newer firmware) use BLE.

### Cited Findings
- ESP-IDF: "ESP-Bluedroid for ESP32-S3 supports Bluetooth LE only. Classic Bluetooth is not supported". NimBLE is also LE-only. The S3 supports Bluetooth 5.0 (LE) and is certified for LE 5.4. — [ESP-IDF BLE overview (S3)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/ble/overview.html)
- Espressif staff (2021) confirmed the S3 "only supports BLE". — [esp32.com thread](https://esp32.com/viewtopic.php?p=83447)
- Bluepad32 FAQ: "The only ESP32 chip that supports Bluetooth Classic (AKA BR/EDR) is the original ESP32". S3/C3/C6/H2 don't. Switch, Wii, DualSense and DualShock "only talk BR/EDR"; only a few gamepads (e.g., Xbox Wireless, some Android pads) use BLE. — [Bluepad32 FAQ](https://bluepad32.readthedocs.io/en/latest/FAQ/); [Bluepad32 supported gamepads](https://bluepad32.readthedocs.io/en/latest/supported_gamepads/)
- A USB-to-BLE bridge on the S3 notes that a 1000 Hz USB mouse outpaces BLE HID's ~133 Hz effective rate, so movement must be aggregated. — [NullYing ESP32S3-BLE-Keyboard-Mouse (search snippet)](https://github-users.yhajoa.workers.dev/NullYing/ESP32S3-BLE-Keyboard-Mouse)

### Inferences
- For a Bluetooth-connected controller emulator targeting PS4/PS5/Switch, the S3 is the wrong chip. Use an original ESP32 (Classic BT, but **no USB OTG**, so mouse/keyboard input would need a MAX3421E, a second MCU, or BT input) or a Pico W / Pico 2 W (Classic BT + native USB + PIO-USB). Wired USB toward the console avoids the BT issue, but the console's acceptance of third-party wired controllers is a separate question.
- An S3 can still act as a BLE HID gamepad to PCs, phones and devices that accept BLE gamepads. The BLE connection interval (≥7.5 ms) limits update rate versus wired 1 ms.

### Gaps
- I did not verify which consoles accept generic BLE HID gamepads. PS4/PS5/Switch do not, according to the Bluepad32 protocol notes above, which describe the controllers rather than console acceptance.
- Console authentication requirements for wired third-party controllers (PS4/PS5 auth, Xbox security chip) were not researched; they are out of scope here.

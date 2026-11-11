# Console Controller Authentication Requirements for a DIY Controller-Emulation Adapter (PS4, PS5, Xbox One/Series, Switch/Switch 2)

Scope note: conceptual/architectural only. No key-extraction or anti-cheat-evasion procedures are included. Research date: 2026-11-09. Several official doc URLs (gp2040-ce.info add-on pages, santroller.com auth page) returned 404 at fetch time; findings rely on the GP2040-CE FAQ page that did load, the GitHub README/releases, XIM's official guide, Cronus's guide, and Espressif component registry pages.

## PS4: DS4 challenge/response, the 8-minute timeout, and how open-source projects satisfy it

### Takeaway
The PS4 requires a controller to answer a cryptographic (RSA-signature) challenge; devices that do not answer it are disconnected/stop responding after about 8 minutes. Hobby firmware handles this in one of two ways: (a) the user uploads a private key + serial + Sony signature taken from a genuine device (GP2040-CE "Uploaded Key File" PS4 mode; the project refuses to supply or explain how to obtain them), or (b) USB-host passthrough, where a licensed PS4 accessory/dongle plugged into the adapter's second USB port answers the challenge while the adapter supplies the inputs.

### Cited Findings
- GP2040-CE FAQ: "PlayStation 4 and PlayStation 5 use an authentication mechanism to ensure only authorized controllers" can be used; "If a device doesn't implement this authentication mechanism, the controller is subject to an 8-minute timeout." — [GP2040-CE Console Compatibility FAQ](https://gp2040-ce.info/faq/faq-console-compatibility/)
- The timeout is avoided by using PS4 Input Mode (works on PS4 or PS5) or PS5 Input Mode (PS5 only); PS4 mode on PC has no timeout. Removing the passthrough auth device after setup triggers the timeout and the controller stops responding until reset/replug (i.e., PlayStation auth is ongoing, not one-shot). — [GP2040-CE Console Compatibility FAQ](https://gp2040-ce.info/faq/faq-console-compatibility/)
- GP2040-CE PS4 Input Mode supports two auth sources: uploaded key files or a Host-USB passthrough device. Uploaded key files are "Private Key (PEM)", "Serial Number (16 Bytes in Hex Ascii)" and "Signature (256 Bytes in Binary)". — [GP2040-CE Console Compatibility FAQ](https://gp2040-ce.info/faq/faq-console-compatibility/)
- GP2040-CE: "The GP2040-CE project will not provide any files or information related to acquiring these keys and files," and warns users not to ask for them in any channel. — [GP2040-CE Console Compatibility FAQ](https://gp2040-ce.info/faq/faq-console-compatibility/)
- Passthrough principle: "The passthrough authentication device is used solely for authentication without processing inputs"; inputs come directly from the GP2040-CE firmware (no added input latency). The auth device plugs directly into the controller board's USB host port (not through a converter). — [GP2040-CE Console Compatibility FAQ](https://gp2040-ce.info/faq/faq-console-compatibility/)
- Known-working PS4/PS5 auth passthrough devices for GP2040-CE: "MagicBoots FPS Adapter Joystick Converter for PS4 v1.1", "N5 for PXN Game Steering Wheel", and "Licensed PS4 Arcade Sticks"; any PS4-categorized arcade stick, racing wheel or flight stick should work. — [GP2040-CE Console Compatibility FAQ](https://gp2040-ce.info/faq/faq-console-compatibility/)
- NOT supported as GP2040-CE auth sources: DualShock 4, DualSense, DualSense Edge, other licensed PS5 controllers. — [GP2040-CE Console Compatibility FAQ](https://gp2040-ce.info/faq/faq-console-compatibility/)
- Contrast: XIM's DualShock 4 output "Requires a DualShock 4 v2 controller for authentication" (i.e., XIM's implementation does use a real DS4 as the auth source). — [XIM Inputs and Outputs](https://guide.xim.tech/Inputs-Outputs/)
- PS Passthrough add-on arrived in GP2040-CE v0.7.4 (PS4 + non-native PS5 compatibility). — [GP2040-CE General FAQ](https://gp2040-ce.info/faq/faq-general/) (via search snippet); GP2040-CE README credits PassingLink for technical details/code of the PS4 implementation. — [GP2040-CE GitHub](https://github.com/OpenStickCommunity/GP2040-CE)
- OpenStickCommunity's "USB Passthrough Board" add-on PCB exists specifically to let "a compatible dongle or controller authenticate" so an RP2040 device works natively on PS4, or as an arcade stick on PS5 for titles supporting legacy PS4 arcade sticks. — [OpenStickCommunity Hardware: USB Passthrough Board](https://github.com/OpenStickCommunity/Hardware/blob/main/Boards/Supported%20Boards/USB%20Passthrough%20Board/README.md)
- Protocol shape (community RE): the console sends a nonce over several HID feature reports (0xF0), the controller replies with status/ack reports (0xF2) while computing, then returns the signed response over 0xF1 reports; the ack/wait exists because the RSA operation is slow on the controller's MCU. — [GIMX forum: PS4 Controller Authentication](https://forum.gimx.fr/viewtopic.php?p=13326); reference implementation in Python — [dogtopus DS4 auth gist](https://gist.github.com/dogtopus/28189b28ba70a74829dac1976ac8c263). One forum poster notes the PS4 dev-wiki packet descriptions are wrong/incomplete. — [GIMX forum](https://forum.gimx.fr/viewtopic.php?p=13299)
- GIMX developer: the authentication sequence is periodic, so the authenticating controller must stay connected. — [GIMX blog: PS4 support status 3](https://blog.gimx.fr/?p=765) (via search snippet)
- fail0verflow: the PS4 kernel verifies an RSA signature produced by the device, and dumping the console kernel doesn't help because "there is no shared secret" — i.e., auth is asymmetric (device-held private key), not a console-side secret. — [fail0verflow: PS4 Aux Hax 3: DualShock4](https://fail0verflow.com/blog/2018/ps4-ds4)

### Inferences
- Because auth is asymmetric (per-device private key + Sony signature over the device's serial/public key, as implied by GP2040-CE's three-file format), a homemade device cannot generate valid credentials itself; it must either borrow a genuine device's identity (key file) or proxy challenges to a genuine/licensed device in real time (passthrough).
- Legal/ethical: key dumping copies Sony-issued credentials out of hardware the user owns; GP2040-CE deliberately distances itself from it. Distributing such keys would plausibly implicate copyright/anti-circumvention law (e.g., DMCA §1201-type provisions) and Sony's ToS; a reused/shared key could also be revoked/blacklisted. These are inferences; no court ruling on DS4 key reuse was found. Passthrough of a purchased licensed dongle is the "clean" architecture favored by open-source projects.
- For an ESP32-S3 adapter: PS4 mode requires either (1) storing user-supplied key material in flash and doing RSA-2048 signing on-device (feasible on S3; it has an RSA accelerator, not verified here), or (2) a USB host port to talk to an auth dongle. The S3 has only one USB-OTG peripheral, so being a USB device to the console AND a USB host to a dongle simultaneously requires extra hardware (e.g., a MAX3421E SPI USB host controller, or a second MCU such as an RP2040 running PIO-USB host). This is an architectural inference, not sourced.
- Timing implication: auth exchanges recur (periodic), so the passthrough path must remain live during play.

### Gaps
- No primary source found stating the exact cryptographic contents (what fields are signed, key length beyond the 256-byte signature implying RSA-2048) — inferred from file format only.
- Official Sony documentation is not public; the psdevwiki DS4 auth page was not fetched.
- The exact definition of "8 minutes" (measured from connection vs. last successful auth) is not documented beyond the GP2040-CE FAQ statement.

## PS5: DS4-mode limits, DualSense auth, XIM/Cronus approaches, GP2040-CE PS5 mode status (late 2026)

### Takeaway
No hobby or commercial project publicly emulates a DualSense well enough to play native PS5 games over USB. Native PS5 games only accept a DualSense-class (or PS5-licensed) controller; PS4-class devices work on PS5 only for PS4 games (back-compat) and for PS5 games that explicitly support legacy "PS4 specialty" peripherals (mainly fighting games/racing wheels). Commercial devices sidestep USB auth via Remote Play over the network (XIM MATRIX LAN mode; Cronus Zen with Zen LINK Wi-Fi module) or via a third-party PS5 auth dongle (XIM + "P5General"). GP2040-CE's "PS5 mode" is a PS4-legacy-auth arcade-stick mode that needs a passthrough dongle; it is not native DualSense emulation.

### Cited Findings
- GP2040-CE v0.7.8 split the earlier PS4 mode into dedicated PS4 and PS5 modes ("expansion of PS4 mode into dedicated PS4 and PS5modes"). — [GP2040-CE v0.7.8 release](https://www.github.com/OpenStickCommunity/GP2040-CE/releases/tag/v0.7.8)
- GP2040-CE PS5 Input Mode is "Host USB only", using a compatible controller or dongle to answer authentication; DualSense/DualSense Edge/licensed PS5 controllers are not supported as auth sources. The FAQ does not describe native PS5 support. — [GP2040-CE Console Compatibility FAQ](https://gp2040-ce.info/faq/faq-console-compatibility/)
- Passthrough board: device can be used "as an arcade stick on the PS5 for titles that support legacy PS4 arcade sticks." — [OpenStickCommunity USB Passthrough Board](https://github.com/OpenStickCommunity/Hardware/blob/main/Boards/Supported%20Boards/USB%20Passthrough%20Board/README.md)
- Retailer listings for GP2040-CE boards: no native PS5 support; with a dongle (MayFlash MagicBoots v1.1, PXN N5, Brook Wingman FGC) it works for PS5 games compatible with PS4 legacy controllers such as Street Fighter 6 / Mortal Kombat 11. — [Arcade Express RP2040 board listing](https://www.arcadexpress.com/en/other-generic/949-rp2040-advanced-breakout-board-54e-fighting-board-pc-nintendo-switch-ps3-ps4-ps5.html); [Focus Attack GP2040-CE board](https://focusattack.com/gp2040-ce-v5-6e-usb-b-usb-c-open-source-multi-console-fight-board) (seller claims, not official)
- XIM: PS5 USB output "Requires a P5General for authentication"; PS5 LAN output "Uses Remote Play via your local network". — [XIM Inputs and Outputs](https://guide.xim.tech/Inputs-Outputs/)
- XIM PS5 guide: XIM MATRIX connects to native PS5 games via the Remote Play protocol over wired Ethernet, described as "equivalent to USB speeds and latency"; requires Sony account registration with a Link Key from the PS5 and PIN disabled. Alternative: plug a P5General (third-party device) into port 3 and choose Controller output, skipping registration. A DualSense on port 3 is only needed for headset audio. — [XIM: Gaming on PlayStation 5](https://guide.xim.tech/Gaming-On-PlayStation-5/)
- Cronus Zen: claims "full native PlayStation 5 console support," but full functionality requires the Zen LINK Wi-Fi Expansion Module; without it the controller disconnects from the PS5. The earlier "PS5 Bypass" is marked "No Longer Supported." — [Cronus Guide: PlayStation 5](https://guide.cronus.support/playstation5)
- Cronus FAQ: latest PS5 support needs firmware 2.2.12+, Zen Studio 1.6.1 Build 77+ and the Zen LINK adapter; "as of April 9th, 2025, almost every PS5 game works"; some PS5 games lack rumble. — [Collective Minds Cronus PS5 FAQ](https://collectiveminds.gitbook.io/cronus/supported-devices/ps5/ps5-faq)
- Historical Cronus approach: PS5 games only via "PS4 Specialty" mode with a PS4 specialty auth controller (e.g., HORI PS4 Mini-Pad / Nacon specialty pad); the Zen PS5 dongle "will not work if this mode is not supported." — [Attack of the Fanboy guide](https://attackofthefanboy.com/guides/how-to-use-cronus-zen-with-or-without-a-dongle-on-ps5/)
- A PS5 firmware update (date unclear, "January 24th") broke Cronus Zen and Besavior U5 direct-connect methods for PS5 games. — [QuadStick PS5 manual page](https://quadstick.s3.amazonaws.com/documents/user_manual/um/ps5_print.htm)
- Community claim (unverified, forum-sourced): DualSense protocol has per-packet authentication/encryption with timestamps, making spoofing impractical. — [EA Forums thread](https://forums.ea.com/discussions/battlefield-2042-general-discussion-en/keyboard-and-mouse-on-consoles/6798285/replies/6798452)

### Inferences
- The commercial trend (XIM 2023+, Cronus 2025) is to abandon USB-level DualSense spoofing and instead act as a Remote Play client over LAN/Wi-Fi, which is authenticated via the user's PSN account rather than controller crypto. For a DIY ESP32-S3 adapter, a Remote Play client (ESP32 implementing Sony's Remote Play stack, as open-source chiaki does on PC) is the only known route to native PS5 games without a third-party PS5 auth dongle; this is a substantial software undertaking and is an inference, not a documented ESP32 project.
- "P5General" is a third-party (likely gray-market) PS5 auth device; its internal mechanism is undocumented. A DIY design could in principle reserve a USB host port for such a device, mirroring XIM's port-3 architecture.
- A PS4-mode DIY adapter will work on PS5 for PS4 titles and for PS5 titles that accept legacy PS4 peripherals; it will not work for the bulk of native PS5 games.

### Gaps
- No public GP2040-CE native DualSense support found as of late 2026; could not fetch the current gp2040-ce.info PS5 add-on page (404).
- Mayflash "Magic-S"/"S5" as GP2040-CE auth sources: not listed in the GP2040-CE FAQ; one search summary noted mixed reviews and a buyer saying it didn't work on PS5 — unverified.
- No authoritative technical description of DualSense/PS5 auth was found (only forum claims).

## Xbox One / Series X|S: GIP authentication and passthrough

### Takeaway
Xbox One/Series consoles run an authentication ("security") handshake over Microsoft's Gaming Input Protocol (GIP); it has not been publicly broken, so all hobby/commercial adapters proxy the handshake to a genuine/licensed Xbox accessory on a USB host port. Unlike PlayStation, Xbox authenticates once at connection, so the auth device can be removed after the handshake (though GP2040-CE recommends keeping it attached).

### Cited Findings
- GP2040-CE added Xbox One X/S and Series X/S support in v0.7.6 "via USB passthrough authentication" (Dec 2023). — [TheTrain (GP2040-CE team) on X](https://twitter.com/TheTrain24/status/1737567176909881420); "Like our PS4/5 support you will need to use a dongle or compatible device for passthrough authentication." — [TheTrain on X](https://x.com/TheTrain24/status/1735004914164658367)
- GP2040-CE Xbox One Input Mode = Host USB with a compatible auth dongle. Known working: "MagicBoots FPS Adapter Joystick Converter for Xbox One" and "Magic-X Wireless Bluetooth USB Adapter." Not supported: "Xbox Wireless Controllers, Xbox One Elite Controllers, and other licensed Xbox One controllers will not work with this add-on." Xbox consoles "only perform the authentication process once," so the device can be removed afterward (still recommended to leave connected). — [GP2040-CE Console Compatibility FAQ](https://gp2040-ce.info/faq/faq-console-compatibility/)
- GP2040-CE README credits Santroller and GIMX for "technical examples of Xbox One authentication using pass-through"; Santroller also credited for Xbox 360 operation without a dongle. — [GP2040-CE GitHub](https://github.com/OpenStickCommunity/GP2040-CE)
- GP2040-CE PR #1002 "Update TinyUSB to latest / Xbox One Auth fix"; v0.7.8 includes "Fix for Xbox One / USB Host Soft-Reboot." — [GP2040-CE PR #1002](https://www.github.com/OpenStickCommunity/GP2040-CE/pull/1002); [v0.7.8 release](https://www.github.com/OpenStickCommunity/GP2040-CE/releases/tag/v0.7.8)
- XIM: Xbox output over USB "Requires an Xbox controller for authentication." — [XIM Inputs and Outputs](https://guide.xim.tech/Inputs-Outputs/) (note conflict with GP2040-CE, which does not support first-party Xbox controllers as auth sources — implementation-specific, not a console rule)
- Santroller: when USB Inputs are enabled, "authentication checks will automatically be passed to a controller"; passthrough only works on Pi Pico (RP2040) builds. — [Santroller authentication controllers page (mirror)](https://round-lake.dustinice.workers.dev/https/santroller.com/console_guides/authentication.html) (search snippet; primary URL 404'd). Adafruit's USB Host Feather RP2040 + Santroller guide lists Xbox One among supported consoles. — [Adafruit Feather Guitar Hero adapter](https://learn.adafruit.com/feather-guitar-hero-adapter/overview)
- Titan One (Cronus predecessor) also required plugging that console's own controller into its USB port to authenticate. — [Windows Central Titan One review](https://windowscentral.com/titan-one-review)
- Community forum comment: the Xbox One authentication mechanism remains uncracked publicly; passthrough devices require an authentic controller. — [lemmy.federate.cc comment](https://lemmy.federate.cc/comment/1051697) (low-authority source)
- Protocol references: Microsoft published GIP as an open standard (Sept 2024); community GIP write-up lists command 0x06 as "Authentication", with chunked messages used during auth. — [TheNathannator GIP gist](https://gist.github.com/TheNathannator/bcebc77e653f71e77634144940871596). Linux `xbox_gip` driver patch series includes support for the "security handshake" (required for wireless-dongle devices); SDL's GIP driver initially lacked the security handshake. — [linux-input: xbox_gip patch series](https://spinics.net/lists/linux-input/msg102348.html); [SDL GIP driver discussion](https://discourse.libsdl.org/t/sdl-joystick-add-new-gip-driver-to-replace-old-xbox-one-wired-driver/59475). xone is the out-of-tree Linux GIP driver. — [medusalix/xone](https://github.com/medusalix/xone)

### Inferences
- Note the direction: Linux drivers implement the host side of the handshake (PC authenticating a dongle/controller), whereas a console adapter needs the device side, which requires a Microsoft-provisioned secret on a licensed accessory; hence passthrough.
- Because auth is one-shot at connect, an ESP32-S3 Xbox adapter could, architecturally, use a host port only at boot; but it still needs USB-host hardware alongside its USB-device role (same S3 single-OTG limitation as PS4). Xbox (GIP) is a vendor-class USB protocol, not HID.
- Which auth device works is implementation-dependent (GP2040-CE: Mayflash dongles only; XIM: Xbox controllers; Santroller: "a controller").

### Gaps
- Microsoft's GIP spec pages describing the security handshake were not fetched; whether the device side is documented in the open spec is unknown.
- Santroller's primary docs page returned 404; its exact supported Xbox auth devices could not be confirmed.

## Nintendo Switch / Switch 2

### Takeaway
Switch 1 has no controller cryptographic authentication: wired USB HID pads (HORI Pokken-style descriptors) and Switch Pro Controller USB/BT protocol emulation are accepted, which is why GP2040-CE and many hobby projects support Switch with no extra hardware. Switch 2 accepts wired third-party USB pads through its dock and original Pro Controllers; the new Switch 2 Pro Controller uses a proprietary BLE GATT protocol with a pairing handshake whose "authentication" relies on a published fixed key, and an ESP32-S3 emulator for it (espp switch2_pro) is reported working.

### Cited Findings
- GP2040-CE lists Nintendo Switch among input modes and supported consoles with no auth add-on required (auth add-ons are only discussed for PS4/PS5/Xbox). — [GP2040-CE GitHub](https://github.com/OpenStickCommunity/GP2040-CE); [GP2040-CE Console Compatibility FAQ](https://gp2040-ce.info/faq/faq-console-compatibility/)
- Switch 2: "Wired third-party controllers that connected via USB to the original Switch dock will also work when plugged into the Switch 2 dock," treated as standard USB gamepads without pairing. — [itechguides / TechyOrker HORI Switch 2 guides](https://techyorker.com/connect-your-hori-controller-to-nintendo-switch-2-setup-and-controls/) (secondary source)
- Nintendo has not listed third-party brands on its Switch 2 compatibility page; some third-party wireless controllers have pairing issues and cannot wake the console. — [SlashGear](https://www.slashgear.com/1888642/can-you-use-third-party-controller-for-nintendo-switch); [Gizmodo](https://gizmodo.com/cant-connect-controllers-to-your-switch-2-heres-why-2000613985)
- Original Pro Controller works on Switch 2 wirelessly or wired via dock; Switch 2 Pro Controller is Switch-2-only. — [itechguides HORI Switch 2 accessories](https://www.itechguides.com/which-hori-switch-2-accessories-work-with-the-original-nintendo-switch/)
- espp `switch2_pro` component (v1.4.0) emulates a Switch 2 Pro Controller over BLE using NimBLE with a custom GATT layout and reverse-engineered pairing; ESP32-S3 recommended and "fully working" with ESP-IDF v6.1+; tested on a real Switch 2 (pairs, inputs register, reconnect and wake-from-sleep work). Requires accepting a 5 ms connection interval (below BT spec minimum) enabled via `CONFIG_BT_CTRL_BLE_MIN_CONN_INTERVAL_ENABLE`. The "authentication" step uses a published fixed key, i.e., a possession check, not per-device attestation. Based on ndeadly's switch2_controller_research. — [Espressif Component Registry: espp/switch2_pro](https://components.espressif.com/components/espp/switch2_pro/versions/1.4.0/readme)
- espp also has a `switch_pro` component targeting ESP32-S3/S2/P4 USB-OTG (wired Switch Pro Controller emulation via TinyUSB). — [Espressif Component Registry: espp/switch_pro](https://components.espressif.com/components/espp/switch_pro/versions/1.3.4/readme) (via search summary)

### Inferences
- For Switch 1 and (wired) Switch 2, an ESP32-S3 USB-device-only design needs no extra auth hardware.
- Switch 2 wireless emulation on ESP32-S3 is feasible via BLE (not Classic BT) per espp; this is significant because the S3 lacks Classic BT.

### Gaps
- No direct test report found of GP2040-CE Switch mode on Switch 2 hardware (only general third-party wired-pad evidence).
- Whether Switch 2-exclusive features (e.g., GameChat button, HD Rumble 2) require the new protocol is not covered.

## ESP32 Bluetooth emulation of Switch Pro / DS4 (Classic vs BLE; S3 limitation)

### Takeaway
Only the original ESP32 has Bluetooth Classic (BR/EDR); ESP32-S3/C3/C6/H2 do not. Switch 1 Pro Controller and DS4 are Classic-BT HID devices, so emulating them wirelessly requires an original ESP32 (with BTstack preferred over Bluedroid). Bluepad32 is a host library (connects controllers to the ESP32), not a device emulator. The ESP32-S3 can only do wired USB emulation or BLE (e.g., Switch 2 Pro via espp).

### Cited Findings
- Bluepad32 FAQ: "The only ESP32 chip that supports 'Bluetooth Classic' (AKA BR/EDR) is the original ESP32. ESP32-S3, ESP32-C3, ESP32-C6 and ESP32-H2 don't support it." It lists Switch Pro controllers as BR/EDR gamepads; Bluepad32 uses BTstack. — [Bluepad32 FAQ](https://bluepad32.readthedocs.io/en/latest/FAQ/)
- A Hackaday project emulating a Switch Pro Controller over Classic BT on an ESP32-WROOM-32E found ESP-IDF's Bluedroid stack needed five source patches (Class of Device bits stripped, HID device path, sniff handling, Secure Connections); moving the Classic side to BTstack (real HID device profile) removed all patches and ~80 KB flash. — [Hackaday.io: Bluetooth NES Advantage](https://hackaday.io/project/206290-bluetooth-nes-advantage/details)
- ESP32-S3 Switch Pro emulation over USB via TinyUSB exists (esp-usb-ble-hid bridge; espp switch_pro). — [finger563/esp-usb-ble-hid](https://github.com/finger563/esp-usb-ble-hid); [espp/switch_pro](https://components.espressif.com/components/espp/switch_pro/versions/1.3.4/readme)

### Inferences
- Wireless DS4 emulation to a PS4 would still face the same RSA auth problem as USB (auth is protocol-level), so BT does not bypass PlayStation auth.
- Named projects "retro.pico.switch", "ESP32-Switch-Controller", "UniJoysticle" were not individually verified in this pass.

### Gaps
- No source fetched confirming a maintained ESP32 Classic-BT DS4 device emulator; not researched due to tool-call budget.

## Implication: extra hardware per console for an ESP32-S3 adapter

### Takeaway
Switch 1/Switch 2 (wired) need nothing extra. PS4 needs either user-supplied key material (legally fraught) or a USB host port for a licensed PS4 dongle/accessory that stays connected. Xbox One/Series needs a USB host port for a compatible auth device at connect time. Native PS5 has no USB DIY path; options are PS4-legacy mode with a dongle (limited titles), a third-party PS5 auth device (XIM's P5General model), or a Remote Play network client.

### Cited Findings
- GP2040-CE: PS4 (key files OR host USB), PS5 (host USB only), Xbox One (host USB dongle); auth device plugs directly into the board's USB host port. — [GP2040-CE Console Compatibility FAQ](https://gp2040-ce.info/faq/faq-console-compatibility/)
- XIM uses a dedicated auth port (port 3) for DS4 v2 / Xbox controller / P5General; PS5 alternatively via LAN Remote Play. — [XIM Inputs and Outputs](https://guide.xim.tech/Inputs-Outputs/); [XIM PS5 guide](https://guide.xim.tech/Gaming-On-PlayStation-5/)
- Cronus PS5 native requires a Zen LINK Wi-Fi expansion module. — [Cronus Guide: PlayStation 5](https://guide.cronus.support/playstation5)

### Inferences
- Summary table (architecture inference):
  - Switch 1 / Switch 2 wired: S3 USB device only. Switch 2 wireless: S3 BLE (espp switch2_pro). Switch 1 wireless: needs original ESP32 (Classic BT).
  - PS4: S3 USB device + (a) on-device RSA with user key files, or (b) second USB host (MAX3421E over SPI, or RP2040/RP2350 co-processor with PIO-USB) for a licensed PS4 dongle; passthrough must stay attached.
  - PS5: same as PS4 for PS4 titles/legacy-peripheral PS5 titles; native PS5 requires third-party PS5 auth device on a host port or network Remote Play implementation (Ethernet/Wi-Fi), not feasible as a simple USB HID emulator.
  - Xbox One/Series: S3 USB device (GIP vendor class) + USB host port for auth device during handshake; optionally a USB hub on the host side if also reading a real controller as input.
- If the adapter also reads a USB mouse/keyboard (XIM-style), the host port must handle both inputs and the auth device, implying a hub downstream of the host controller.
- Using games' anti-cheat-detectable input devices may violate game EULAs (Cronus FAQ itself warns of this) — [Collective Minds Cronus PS5 FAQ](https://collectiveminds.gitbook.io/cronus/supported-devices/ps5/ps5-faq).

### Gaps
- ESP32-S3 USB host + device simultaneous capability was not verified from Espressif docs in this pass (S3 has one USB-OTG controller; assumed one role at a time).

# Project Kestrel - read the adapter as Windows sees it (XInput) and report what moved.
# Usage: python tools/xinput_test.py [seconds]

import ctypes
import sys
import time


class GAMEPAD(ctypes.Structure):
    _fields_ = [("wButtons", ctypes.c_ushort), ("bLeftTrigger", ctypes.c_ubyte),
                ("bRightTrigger", ctypes.c_ubyte), ("sThumbLX", ctypes.c_short),
                ("sThumbLY", ctypes.c_short), ("sThumbRX", ctypes.c_short), ("sThumbRY", ctypes.c_short)]


class STATE(ctypes.Structure):
    _fields_ = [("dwPacketNumber", ctypes.c_uint), ("Gamepad", GAMEPAD)]


BUTTONS = {0x0001: "DPAD_UP", 0x0002: "DPAD_DOWN", 0x0004: "DPAD_LEFT", 0x0008: "DPAD_RIGHT",
           0x0010: "MENU", 0x0020: "VIEW", 0x0040: "LS", 0x0080: "RS", 0x0100: "LB", 0x0200: "RB",
           0x1000: "A", 0x2000: "B", 0x4000: "X", 0x8000: "Y"}

xinput = ctypes.windll.xinput1_4
seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 20.0

pads = [i for i in range(4) if xinput.XInputGetState(i, ctypes.byref(STATE())) == 0]
if not pads:
    sys.exit("No XInput controller found")
idx = pads[0]
print("Reading controller %d for %.0f s..." % (idx, seconds), flush=True)

mins = {"LX": 0, "LY": 0, "RX": 0, "RY": 0}
maxs = {"LX": 0, "LY": 0, "RX": 0, "RY": 0}
lt = rt = 0
pressed = set()
packets = 0
last_pkt = None
end = time.time() + seconds
st = STATE()
while time.time() < end:
    if xinput.XInputGetState(idx, ctypes.byref(st)) != 0:
        print("controller disconnected!")
        break
    g = st.Gamepad
    if st.dwPacketNumber != last_pkt:
        packets += 1
        last_pkt = st.dwPacketNumber
    for k, v in (("LX", g.sThumbLX), ("LY", g.sThumbLY), ("RX", g.sThumbRX), ("RY", g.sThumbRY)):
        mins[k] = min(mins[k], v)
        maxs[k] = max(maxs[k], v)
    lt, rt = max(lt, g.bLeftTrigger), max(rt, g.bRightTrigger)
    for bit, name in BUTTONS.items():
        if g.wButtons & bit:
            pressed.add(name)
    time.sleep(0.002)

print("state changes seen: %d" % packets)
for k in ("LX", "LY", "RX", "RY"):
    print("  %s range: %6d .. %6d" % (k, mins[k], maxs[k]))
print("  LT max: %d   RT max: %d" % (lt, rt))
print("  buttons pressed: %s" % (", ".join(sorted(pressed)) or "none"))
print("  final right stick: RX=%d RY=%d (should be 0 when the mouse is still)" % (st.Gamepad.sThumbRX, st.Gamepad.sThumbRY))

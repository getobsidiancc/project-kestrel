# Full measured curves incl. the precise low end (2026-11-10):
#  ADS: deadzone 0.086, 0.098 -> 7.2, 0.145 -> 43.9, 0.19 -> 125.8, 0.28 -> 365.8 (+ hip shape x1.54 above)
#  hip: deadzone 0.092, 0.104 -> 8.8, 0.145 -> 26.4, 0.235 -> 139.3, 0.325 -> 337, 0.55 -> 603, 0.775 -> 917
import serial, struct, sys, time
port = sys.argv[1]
HIP_CM = float(sys.argv[2]) if len(sys.argv) > 2 else 18.14
ADS_CM = float(sys.argv[3]) if len(sys.argv) > 3 else 28.70
HIP = ([0.092, 0.104, 0.145, 0.235, 0.325, 0.550, 0.775, 1.000], [0, 8.8, 26.4, 139.3, 337.0, 603.2, 917.4, 1200.0])
ADS = ([0.086, 0.098, 0.145, 0.190, 0.280, 0.550, 0.775, 1.000], [0, 7.2, 43.9, 125.8, 365.8, 929.0, 1412.0, 1848.0])
s = serial.Serial(port, 921600, timeout=0.1); buf = b""
def ask(cmd, want, secs=4):
    global buf
    s.write((cmd + "\n").encode()); end = time.time() + secs
    while time.time() < end:
        buf += s.read(8192)
        while b"\n" in buf:
            l, buf = buf.split(b"\n", 1); l = l.decode("utf-8", "replace").strip(); i = l.find("@KST ")
            if i >= 0 and l[i + 5:].startswith(want): return l[i + 5:]
def put(b, off, cm, tbl):
    d, v = tbl
    struct.pack_into("<f", b, off, cm); b[off + 8] = len(d)
    for i in range(8):
        struct.pack_into("<f", b, off + 12 + 4 * i, float(d[i]) if i < len(d) else 0.0)
        struct.pack_into("<f", b, off + 44 + 4 * i, float(v[i]) if i < len(v) else 0.0)
time.sleep(0.3); s.reset_input_buffer()
b = bytearray(bytes.fromhex(ask("KST GET", "CFG").split()[1]))
assert len(b) == 464 and b[4] == 2
put(b, 24, HIP_CM, HIP); put(b, 100, ADS_CM, ADS)
h = bytes(b).hex(); ok = False; r = None
for attempt in range(3):
    r = ask("KST SET " + h, "RESULT", 6); time.sleep(0.4)
    g = ask("KST GET", "CFG")
    if g and g.split()[-1] == h: ok = True; break
print("SET", r, "| verify", ok, f"| hip {HIP_CM} cm, ads {ADS_CM} cm, fast {b[23]}%, both curves start at the measured deadzone")

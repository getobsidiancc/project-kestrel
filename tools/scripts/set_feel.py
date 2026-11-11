# Set speed averaging window (ms), smoothing (0..0.95) and hip fast-turn boost (%); print before/after
import serial, struct, sys, time
port, window, smooth, fast = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), int(sys.argv[4])
s = serial.Serial(port, 921600, timeout=0.1); buf = b""
def ask(cmd, want, secs=4):
    global buf
    s.write((cmd + "\n").encode()); end = time.time() + secs
    while time.time() < end:
        buf += s.read(8192)
        while b"\n" in buf:
            l, buf = buf.split(b"\n", 1); l = l.decode("utf-8", "replace").strip(); i = l.find("@KST ")
            if i >= 0 and l[i + 5:].startswith(want): return l[i + 5:]
def show(b, tag):
    h, = struct.unpack_from("<f", b, 24); a, = struct.unpack_from("<f", b, 100); sm, = struct.unpack_from("<f", b, 12)
    print(f"{tag}: hip {h:.1f} cm  ads {a:.1f} cm  window {b[18]} ms  smoothing {sm:.2f}  fast {b[23]}%")
time.sleep(0.3); s.reset_input_buffer()
b = bytearray(bytes.fromhex(ask("KST GET", "CFG").split()[1]))
assert len(b) == 464 and b[4] == 2
show(b, "before")
b[18] = window; struct.pack_into("<f", b, 12, smooth); b[23] = fast
h = bytes(b).hex(); ok = False; r = None
for attempt in range(3):
    r = ask("KST SET " + h, "RESULT", 6); time.sleep(0.4)
    g = ask("KST GET", "CFG")
    if g and g.split()[-1] == h: ok = True; break
print("SET", r, "| verify", ok)
show(b, "after ")

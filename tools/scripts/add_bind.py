# Add one bind (src tgt [flags]) if not already there; keeps everything else
import serial, sys, time
port, src, tgt = sys.argv[1], int(sys.argv[2], 0), int(sys.argv[3], 0)
flags = int(sys.argv[4], 0) if len(sys.argv) > 4 else 0
s = serial.Serial(port, 921600, timeout=0.1); buf = b""
def ask(cmd, want, secs=4):
    global buf
    s.write((cmd + "\n").encode()); end = time.time() + secs
    while time.time() < end:
        buf += s.read(8192)
        while b"\n" in buf:
            l, buf = buf.split(b"\n", 1); l = l.decode("utf-8", "replace").strip(); i = l.find("@KST ")
            if i >= 0 and l[i + 5:].startswith(want): return l[i + 5:]
time.sleep(0.3); s.reset_input_buffer()
b = bytearray(bytes.fromhex(ask("KST GET", "CFG").split()[1]))
assert len(b) == 464 and b[4] == 2
slots = [(b[176 + 3 * i], b[177 + 3 * i]) for i in range(96)]
if (src, tgt) in slots: print("already bound"); sys.exit(0)
free = next(i for i, (a, t) in enumerate(slots) if not (a and t))
b[176 + 3 * free:179 + 3 * free] = bytes([src, tgt, flags])
h = bytes(b).hex(); r = None
for attempt in range(3):
    r = ask("KST SET " + h, "RESULT", 6)
    if r and r.endswith("0"): break
time.sleep(0.3)
print("SET", r, "| verify", (ask("KST GET", "CFG") or "").split()[-1] == h, f"| slot {free}, binds {sum(1 for a, t in slots if a and t) + 1}")

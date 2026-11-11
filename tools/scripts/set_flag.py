# Set the flags of an existing bind: set_flag.py PORT SRC TGT FLAGS
import serial, sys, time
port, src, tgt, flags = sys.argv[1], int(sys.argv[2], 0), int(sys.argv[3], 0), int(sys.argv[4], 0)
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
n = 0
for i in range(96):
    o = 176 + 3 * i
    if b[o] == src and b[o + 1] == tgt: b[o + 2] = flags; n += 1
h = bytes(b).hex(); ok = False; r = None
for attempt in range(3):
    r = ask("KST SET " + h, "RESULT", 6); time.sleep(0.4)
    g = ask("KST GET", "CFG")
    if g and g.split()[-1] == h: ok = True; break
print("SET", r, "| verify", ok, f"| {n} bind changed")

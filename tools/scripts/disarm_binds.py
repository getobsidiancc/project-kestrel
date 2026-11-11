# v18: number keys 1 and 3-9 disarm rapid-click (their normal binds stay as they are)
import serial, sys, time
port = sys.argv[1]
DISARM_T = 30
KEYS = [0x1E, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26]   # 1, 3, 4, 5, 6, 7, 8, 9
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
print("PING", ask("KST PING", "PONG"))
b = bytearray(bytes.fromhex(ask("KST GET", "CFG").split()[1]))
assert len(b) == 464 and b[4] == 2
for k in KEYS:
    slots = [(b[176 + 3 * i], b[177 + 3 * i]) for i in range(96)]
    if (k, DISARM_T) in slots: continue
    free = next(i for i, (a, t) in enumerate(slots) if not (a and t))
    b[176 + 3 * free:179 + 3 * free] = bytes([k, DISARM_T, 0])
h = bytes(b).hex(); ok = False; r = None
for attempt in range(3):
    r = ask("KST SET " + h, "RESULT", 6); time.sleep(0.4)
    g = ask("KST GET", "CFG")
    if g and g.split()[-1] == h: ok = True; break
nb = sum(1 for i in range(96) if b[176 + 3 * i] and b[177 + 3 * i])
print("SET", r, "| verify", ok, f"| binds {nb}")

# v17 rapid-click setup: F11 = mode on/off, 2 = arm (2's normal Y is off in rapid mode),
# right-click = hold-to-enable (sends nothing while armed), left-click = the key that rapid-clicks.
import serial, sys, time
port = sys.argv[1]
F11, N2, ML, MR = 0x44, 0x1F, 0xF0, 0xF1
LT_T, RT_T, Y_T, MODE_T, ARM_T = 17, 18, 16, 28, 29
RAPID, NOT_RAPID, HOLD = 0x10, 0x20, 0x40
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
for i in range(96):
    o = 176 + 3 * i
    src, tgt = b[o], b[o + 1]
    if not (src and tgt): continue
    b[o + 2] &= ~(RAPID | NOT_RAPID | HOLD) & 0xFF      # clear old rapid flags everywhere
    if src == MR and tgt == LT_T: b[o + 2] |= HOLD
    if src == ML and tgt == RT_T: b[o + 2] |= RAPID
    if src == N2 and tgt == Y_T: b[o + 2] |= NOT_RAPID
def ensure(src, tgt):
    slots = [(b[176 + 3 * i], b[177 + 3 * i]) for i in range(96)]
    if (src, tgt) in slots: return
    free = next(i for i, (a, t) in enumerate(slots) if not (a and t))
    b[176 + 3 * free:179 + 3 * free] = bytes([src, tgt, 0])
ensure(F11, MODE_T); ensure(N2, ARM_T)
h = bytes(b).hex(); ok = False; r = None
for attempt in range(3):
    r = ask("KST SET " + h, "RESULT", 6); time.sleep(0.4)
    g = ask("KST GET", "CFG")
    if g and g.split()[-1] == h: ok = True; break
print("SET", r, "| verify", ok)
print(" ".join(f"{b[176+3*i]:02x}>{b[177+3*i]}/{b[178+3*i]}" for i in range(96) if b[176+3*i] and b[177+3*i]))

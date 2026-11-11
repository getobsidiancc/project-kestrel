# v22 gadget placing: left click = "Fire / place gadget" (target 31, keeps its rapid flag);
# middle click (RB) and G (LB) are gadget keys (flag 0x80). Needs RIGHT firmware v22+.
import serial, sys, time
port = sys.argv[1]
ML, MM, G = 0xF0, 0xF2, 0x0A
RT_T, FIREPLACE_T, RB_T, LB_T, GADGET = 18, 31, 1 + 9, 1 + 8, 0x80
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
pong = ask("KST PING", "PONG"); print("PING", pong)
assert pong and int(pong.split()[2]) >= 22, "flash firmware v22 first"
b = bytearray(bytes.fromhex(ask("KST GET", "CFG").split()[1]))
assert len(b) == 464 and b[4] == 2
for i in range(96):
    o = 176 + 3 * i
    if b[o] == ML and b[o + 1] == RT_T: b[o + 1] = FIREPLACE_T
    if (b[o], b[o + 1]) in ((MM, RB_T), (G, LB_T)): b[o + 2] |= GADGET
h = bytes(b).hex(); ok = False; r = None
for attempt in range(3):
    r = ask("KST SET " + h, "RESULT", 6); time.sleep(0.4)
    g = ask("KST GET", "CFG")
    if g and g.split()[-1] == h: ok = True; break
print("SET", r, "| verify", ok)
print(" ".join(f"{b[176+3*i]:02x}>{b[177+3*i]}/{b[178+3*i]}" for i in range(96) if b[176+3*i] in (ML, MM, G) and b[177+3*i]))

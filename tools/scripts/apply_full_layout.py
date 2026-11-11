# Full Siege console layout (current version). Replaces ALL keybinds; keeps sensitivity/aim settings.
import serial, sys, time

port = sys.argv[1]
BTN = lambda bit: 1 + bit
DUP, DDOWN, DLEFT, DRIGHT, START, BACK, LS, RS, LB, RB, GUIDE, A, B, X, Y = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15
LT, RT, MUP, MDOWN, MLEFT, MRIGHT, WALK = 17, 18, 19, 20, 21, 22, 23
K = dict(W=0x1A, A=0x04, S=0x16, D=0x07, Q=0x14, E=0x08, R=0x15, F=0x09, G=0x0A, X=0x1B, Z=0x1D, C=0x06, V=0x19,
         N1=0x1E, N2=0x1F, N3=0x20, N4=0x21, N5=0x22, SPACE=0x2C, TAB=0x2B, ESC=0x29, HOME=0x4A, LSHIFT=0xE1, LCTRL=0xE0, LALT=0xE2,
         UP=0x52, DOWN=0x51, LEFT=0x50, RIGHT=0x4F)
ML, MR, MM, M4, M5, WUP, WDN = 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF8, 0xF9
AIM, TAP, LONG = 0x02, 0x04, 0x08

BINDS = [
    (K["W"], MUP), (K["S"], MDOWN), (K["A"], MLEFT), (K["D"], MRIGHT), (K["LALT"], WALK),
    (ML, RT), (MR, LT),
    (K["Q"], BTN(LS), AIM), (M4, BTN(LS), AIM), (K["E"], BTN(RS), AIM), (M5, BTN(RS), AIM),
    (K["LSHIFT"], BTN(LS)), (K["V"], BTN(RS)),
    (K["SPACE"], BTN(A)),
    (K["C"], BTN(B), TAP), (K["LCTRL"], BTN(B), LONG), (K["Z"], BTN(B)),
    (K["R"], BTN(X)), (K["F"], BTN(X)),
    (K["N1"], BTN(Y)), (K["N2"], BTN(Y)), (WUP, BTN(Y)), (WDN, BTN(Y)),
    (K["G"], BTN(LB)), (K["X"], BTN(RB)), (MM, BTN(RB)),
    (K["N3"], BTN(DLEFT)), (K["N4"], BTN(DRIGHT)), (K["N5"], BTN(DDOWN)),
    (K["TAB"], BTN(BACK)), (K["ESC"], BTN(START)), (K["HOME"], BTN(GUIDE)),
    (K["UP"], BTN(DUP)), (K["DOWN"], BTN(DDOWN)), (K["LEFT"], BTN(DLEFT)), (K["RIGHT"], BTN(DRIGHT)),
]

s = serial.Serial(port, 921600, timeout=0.1)
buf = b""
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
for i in range(96):
    b[176 + 3 * i:179 + 3 * i] = bytes([BINDS[i][0], BINDS[i][1], BINDS[i][2] if len(BINDS[i]) > 2 else 0]) if i < len(BINDS) else b"\0\0\0"
h = bytes(b).hex(); r = None
for attempt in range(3):
    r = ask("KST SET " + h, "RESULT", 6)
    if r and r.endswith("0"): break
time.sleep(0.3)
print("SET", r, "| verify", (ask("KST GET", "CFG") or "").split()[-1] == h, f"| {len(BINDS)} binds")

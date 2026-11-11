# Restore adapter settings from a saved hex file (made by save_cfg.py):
#   python restore_cfg.py COM3 ..\..\profiles\siege-good.hex
# Writes the exact 464-byte settings block (sens, curves, all keybinds) and reads it back to verify.
import serial, sys, time
port, path = sys.argv[1], sys.argv[2]
h = open(path).read().strip()
assert len(h) == 928, "not a Kestrel v2 settings file (expected 464 bytes)"
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
ok = False; r = None
for attempt in range(3):
    r = ask("KST SET " + h, "RESULT", 6); time.sleep(0.4)
    g = ask("KST GET", "CFG")
    if g and g.split()[-1] == h: ok = True; break
print("SET", r, "| verify", ok)

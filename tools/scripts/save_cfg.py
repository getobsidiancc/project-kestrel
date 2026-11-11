# Save the adapter's current settings (hex) to a file: save_cfg.py PORT OUTFILE
import serial, sys, time
port, out = sys.argv[1], sys.argv[2]
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
h = ask("KST GET", "CFG").split()[1]
assert len(h) == 928
open(out, "w").write(h + "\n")
print("saved", out)

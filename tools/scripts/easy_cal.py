# Easy hipfire calibration: the player presses F12 to start a row (the adapter turns right at a fixed
# stick position) and F12 again when they're back facing the same spot after ONE full turn.
import serial, sys, time
port = sys.argv[1]
F12 = 0x45
DZ = 0.10
ROWS = [float(x) for x in sys.argv[2].split(",")] if len(sys.argv) > 2 else [0.25, 0.50, 0.75]   # fraction past the deadzone
s = serial.Serial(port, 921600, timeout=0.02); buf = b""
f12 = False
def pump():
    global buf, f12
    buf += s.read(4096)
    while b"\n" in buf:
        l, buf = buf.split(b"\n", 1); l = l.decode("utf-8", "replace").strip(); i = l.find("@KST KEYS ")
        if i >= 0:
            parts = l[i + 10:].split()
            keys = [int(x, 16) for x in parts[1].split(",")] if len(parts) > 1 and parts[1] else []
            f12 = F12 in keys
last_live = 0
def live():
    global last_live
    if time.time() - last_live > 1.5:
        s.write(b"KST LIVE 1\n"); last_live = time.time()
def wait_f12(state, limit=900):
    end = time.time() + limit
    while time.time() < end:
        live(); pump()
        if f12 == state: return time.perf_counter()
    raise SystemExit("timed out waiting for F12")
time.sleep(0.3); s.reset_input_buffer()
s.write(b"KST CAL -1\n")
out = []
try:
    for frac in ROWS:
        defl = DZ + (1 - DZ) * frac
        print(f"row {int(frac*100)}% (stick {defl:.3f}): waiting for F12 to start", flush=True)
        wait_f12(False); wait_f12(True)
        s.write(f"KST CAL {round(defl * 1000)}\n".encode()); t0 = time.perf_counter()
        print("  turning... waiting for F12 to stop", flush=True)
        wait_f12(False); t1 = wait_f12(True)
        s.write(b"KST CAL -1\n")
        secs = t1 - t0; dps = 360 / secs
        out.append((defl, dps))
        print(f"  {secs:.2f} s for one turn -> {dps:.0f} deg/s", flush=True)
finally:
    s.write(b"KST CAL -1\n"); s.write(b"KST LIVE 0\n")
print("RESULT", " ".join(f"{d:.3f}:{v:.1f}" for d, v in out), flush=True)

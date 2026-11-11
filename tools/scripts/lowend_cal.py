# Precise low end of the stick (where super-slow mouse moves land). For each pass (ADS, then hipfire):
#  A) F12 starts a slow stick ramp from 0.080 up (+0.002 every 300 ms); F12 the moment the view starts moving.
#  B) F12 starts a turn just past that point; F12 again after ONE full circle.
import serial, sys, time
port = sys.argv[1]
F12 = 0x45
s = serial.Serial(port, 921600, timeout=0.02); buf = b""; f12 = False; last_live = 0
def pump():
    global buf, f12
    buf += s.read(4096)
    while b"\n" in buf:
        l, buf = buf.split(b"\n", 1); l = l.decode("utf-8", "replace").strip(); i = l.find("@KST KEYS ")
        if i >= 0:
            parts = l[i + 10:].split()
            keys = [int(x, 16) for x in parts[1].split(",")] if len(parts) > 1 and parts[1] else []
            f12 = F12 in keys
def live():
    global last_live
    if time.time() - last_live > 1.5: s.write(b"KST LIVE 1\n"); last_live = time.time()
def wait_f12(state, limit=900, tick=None):
    end = time.time() + limit
    while time.time() < end:
        live(); pump()
        if tick: tick()
        if f12 == state: return time.perf_counter()
    raise SystemExit("timed out waiting for F12")
def press(): wait_f12(False); return wait_f12(True)
def stick(v): s.write(f"KST CAL {round(v * 1000) if v >= 0 else -1}\n".encode())
time.sleep(0.3); s.reset_input_buffer(); stick(-1)
res = {}
try:
    for name in ("ads", "hip"):
        print(f"[{name}] ramp: waiting for F12 to start", flush=True)
        press()
        v = [0.080]; t = [time.time()]; stick(v[0])
        def tick():
            if time.time() - t[0] >= 0.3:
                v[0] = min(0.30, v[0] + 0.002); t[0] = time.time(); stick(v[0])
        wait_f12(False, tick=tick); wait_f12(True, tick=tick)
        dz = max(0.08, v[0] - 0.002)          # take off ~one step for reaction time
        stick(-1)
        print(f"  starts moving at ~{dz:.3f}", flush=True)
        test = dz + 0.012
        print(f"[{name}] slow turn at {test:.3f}: waiting for F12 to start", flush=True)
        t0 = press(); stick(test)
        t1 = press(); stick(-1)
        dps = 360 / (t1 - t0)
        print(f"  {t1 - t0:.2f} s for one turn -> {dps:.1f} deg/s", flush=True)
        res[name] = (dz, test, dps)
finally:
    stick(-1); s.write(b"KST LIVE 0\n")
print("RESULT", " ".join(f"{k}:{d:.3f}:{t:.3f}:{v:.1f}" for k, (d, t, v) in res.items()), flush=True)

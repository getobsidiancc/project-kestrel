# Watch what the adapter sends: prints every change in keys held and in the controller buttons/triggers.
#   python watch_buttons.py COM3 40
import serial, sys, time
port, secs = sys.argv[1], float(sys.argv[2]) if len(sys.argv) > 2 else 40
NAMES = {0: 'D-UP', 1: 'D-DOWN', 2: 'D-LEFT', 3: 'D-RIGHT', 4: 'MENU', 5: 'VIEW', 6: 'L3', 7: 'R3', 8: 'LB', 9: 'RB', 10: 'XBOX', 12: 'A', 13: 'B', 14: 'X', 15: 'Y'}
s = serial.Serial(port, 921600, timeout=0.02); buf = b""; t0 = time.time(); last_pad = None; last_keys = None; live_at = 0
end = t0 + secs
while time.time() < end:
    if time.time() - live_at > 1.5: s.write(b"KST LIVE 1\n"); live_at = time.time()
    buf += s.read(4096)
    while b"\n" in buf:
        l, buf = buf.split(b"\n", 1); l = l.decode("utf-8", "replace").strip(); i = l.find("@KST ")
        if i < 0: continue
        p = l[i + 5:].split()
        if p[0] == "PAD" and len(p) >= 4:
            b, lt, rt = int(p[1]), int(p[2]), int(p[3])
            state = ([n for bit, n in NAMES.items() if b >> bit & 1], lt > 0, rt > 0)
            if state != last_pad:
                last_pad = state
                print(f"{time.time() - t0:6.2f}s  PAD  {' '.join(state[0]) or '-'}{'  LT' if state[1] else ''}{'  RT' if state[2] else ''}", flush=True)
        elif p[0] == "KEYS":
            k = " ".join(p[1:])
            if k != last_keys:
                last_keys = k
                print(f"{time.time() - t0:6.2f}s  KEYS {k}", flush=True)
s.write(b"KST LIVE 0\n")

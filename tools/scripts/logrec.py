# Record the adapter log for N seconds, surviving the COM port disappearing/reappearing.
import serial, sys, time
port, out, secs = sys.argv[1], sys.argv[2], float(sys.argv[3])
end = time.time() + secs
with open(out, "ab") as f:
    while time.time() < end:
        try:
            s = serial.Serial(port, 921600, timeout=0.2)
            f.write(b"\n--- port opened ---\n"); f.flush()
            while time.time() < end:
                d = s.read(4096)
                if d:
                    f.write(d); f.flush()
        except Exception:
            time.sleep(0.2)

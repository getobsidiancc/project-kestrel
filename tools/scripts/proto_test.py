import serial, struct, sys, time

port = sys.argv[1]
s = serial.Serial(port, 921600, timeout=0.1)
buf = b""

def lines(seconds):
    global buf
    end = time.time() + seconds
    while time.time() < end:
        buf += s.read(4096)
        while b"\n" in buf:
            l, buf = buf.split(b"\n", 1)
            yield l.decode("utf-8", "replace").rstrip("\r")

def ask(cmd, want, seconds=3):
    s.write((cmd + "\n").encode())
    for l in lines(seconds):
        i = l.find("@KST ")
        if i >= 0 and l[i + 5:].startswith(want):
            return l[i + 5:]
    return None

def aim(b, o):
    cm, y, n = struct.unpack_from("<ffB", b, o)
    d = struct.unpack_from("<8f", b, o + 12)[:n]
    v = struct.unpack_from("<8f", b, o + 44)[:n]
    return cm, y, list(zip([round(x, 3) for x in d], [round(x) for x in v]))

time.sleep(0.3)
s.reset_input_buffer()
print("PING ->", ask("KST PING", "PONG"))
cfg = ask("KST GET", "CFG")
if not cfg:
    print("GET -> no answer"); sys.exit(1)
hexs = cfg.split()[1]
b = bytes.fromhex(hexs)
magic, ver, _, size = struct.unpack_from("<IBBH", b, 0)
dpi, = struct.unpack_from("<f", b, 8)
binds = [(b[176 + 3 * i], b[177 + 3 * i], b[178 + 3 * i]) for i in range(96) if b[176 + 3 * i] and b[177 + 3 * i]]
print(f"GET -> {len(b)} bytes, magic ok={magic == 0x4354534B}, version {ver}, size {size}, dpi {dpi:.0f}, ADS {'on' if b[21] else 'off'}")
print("       hipfire", aim(b, 24))
print(f"       {len(binds)} binds")
print("SET (unchanged) ->", ask("KST SET " + hexs, "RESULT"))
print("GET again matches ->", (ask("KST GET", "CFG") or "").split()[-1] == hexs)
s.write(b"KST LIVE 1\n")
t0 = time.time()
pads = [l for l in lines(2.0) if "@KST PAD" in l]
print(f"LIVE -> {len(pads)} PAD lines in 2.0 s (~{len(pads) / 2:.0f}/s), last: {pads[-1] if pads else None}")
s.write(b"KST LIVE 0\n")

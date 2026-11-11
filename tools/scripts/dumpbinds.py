import serial, sys, time
s = serial.Serial(sys.argv[1], 921600, timeout=0.1); time.sleep(0.3); s.reset_input_buffer()
s.write(b"KST GET\n"); buf = b""; end = time.time() + 5; hexs = None
while time.time() < end and hexs is None:
    buf += s.read(8192)
    while b"\n" in buf:
        l, buf = buf.split(b"\n", 1); l = l.decode("utf-8", "replace").strip(); i = l.find("@KST CFG")
        if i >= 0: hexs = l[i:].split()[2]; break
b = bytes.fromhex(hexs)
print(" ".join(f"{b[176+3*i]:02x}>{b[177+3*i]}/{b[178+3*i]}" for i in range(96) if b[176+3*i] and b[177+3*i]))

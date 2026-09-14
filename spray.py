#!/usr/bin/env python3
# usage: spray.py 10s          raw seconds (calibration runs)
#        spray.py 5 | 5g       grams -> seconds via fit, then spray
#        optional 2nd arg: serial port (default /dev/ttyUSB0)
import sys, time, serial  # sudo apt install python3-serial

SLOPE, INTERCEPT = 1.7611, -1.5001

arg = sys.argv[1].lower()
if arg.endswith("s"):
    secs = float(arg[:-1])
    print(f"raw {secs} s")
else:
    grams = float(arg.rstrip("g"))
    if grams <= 0: sys.exit("grams must be > 0")
    secs = round((grams - INTERCEPT) / SLOPE, 2)
    print(f"{grams} g -> spray {secs} s")
if secs <= 0 or secs > 60:
    sys.exit("duration out of range (0-60 s)")

ser = serial.Serial(sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyUSB0", 9600, timeout=1)
while b"ready" not in ser.readline():  # board resets on port open
    pass
ser.write(f"spray {secs}\n".encode())

# wall time = blow time + gaps (~0.5 s per 0.7 s pulse) + ~9 s cycle overhead
end = time.time() + secs * 1.8 + 15
while time.time() < end:
    line = ser.readline().decode(errors="replace").strip()
    if line:
        print(line)
        if line.startswith(("OK", "ERR")):
            break
ser.close()

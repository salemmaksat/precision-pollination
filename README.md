# Automated Date Palm Pollen Dispenser

Automated pollen/powder dispensing end-effector for a **UR10e robotic arm**, intended to run from a **Jetson**.

The current prototype uses a commercial air blower Wolfbox MF200 with a ~200 mL powder jar and two physical push buttons. Two SG90 servos mechanically actuate those buttons, while an Arduino Nano handles the low-level timing sequence. A Python script on the host converts a requested powder mass into a spray duration using an experimentally fitted linear regression.

The intended user-facing command is:

```bash
python3 spray.py 5
```

which requests approximately **5 g** of powder.

> **Important:** This system is currently open-loop. It estimates dispensed mass from spray duration; it does not measure powder mass during spraying. Output depends on powder properties, fill level, packing, nozzle condition, air flow, and environmental conditions.

---

## Project Status

Current functionality:

- Arduino Nano controls two SG90 servos.
- Servo A toggles blower power.
- Servo B holds the blower trigger during dispensing.
- Host computer communicates with the Arduino over USB serial.
- `spray.py` accepts either:
  - a target mass in grams, or
  - a raw spray duration in seconds for calibration.
- Target mass is converted to spray time using the current regression:

```text
grams = 1.7611 × seconds - 1.5001
```

Therefore:

```text
seconds = (grams + 1.5001) / 1.7611
```

The current calibration has shown roughly **R² ≈ 0.80**. Performance has been most reliable near the 6 g target, while intermediate targets such as 4 g have shown systematic under-delivery. The most likely contributor observed so far is changing powder flow as the jar empties.

The next development stage is ROS 2 integration on the Jetson and improvement of dispensing repeatability.

---

## System Architecture

```text
User / ROS 2
     |
     | target mass, e.g. 5 g
     v
  spray.py
     |
     | USB serial: "spray <seconds>"
     v
Arduino Nano
     |
     +---- PWM D9  ---> SG90 Servo A ---> Blower power button
     |
     +---- PWM D10 ---> SG90 Servo B ---> Blower trigger button
                                      |
                                      v
                            Blower + pollen jar + nozzle
```

The Arduino owns the complete actuation cycle:

```text
Power ON -> settle -> blow for requested duration -> Power OFF
```

This means the host only needs to send one command.

---

## Repository Files

```text
.
├── README.md
├── requirements.txt
├── spray.py
└── spray_controller.ino
```

### `spray_controller.ino`

Arduino firmware responsible for:

- serial communication at 9600 baud,
- servo positioning,
- blower power-on sequence,
- settling delay,
- timed spray,
- blower power-off sequence,
- command validation,
- completion response.

Accepted serial command:

```text
spray <seconds>
```

Example:

```text
spray 3.5
```

Typical Arduino response:

```text
ready
OK spray 3.500
```

Invalid commands return an `ERR ...` message.

### `spray.py`

Host-side command-line utility.

Examples:

```bash
python3 spray.py 5
python3 spray.py 5g
python3 spray.py 3.5s
```

- `5` or `5g` means **target 5 grams**.
- `3.5s` means **spray for exactly 3.5 seconds**, bypassing the regression.

An optional second argument selects a serial port:

```bash
python3 spray.py 5 /dev/ttyUSB1
```

Default serial port:

```text
/dev/ttyUSB0
```

---

## Hardware

### Main components

- UR10e 
- NVIDIA Jetson 
- Arduino Nano
- 2 × SG90 micro servo motors
- Commercial air blower
- ~200 mL powder/pollen jar
- Blower nozzle
- External regulated 5 V supply for servos
- USB cable between Jetson/host and Arduino
- Mechanical servo mounts / button actuators

### Servo assignments

| Function | Arduino Pin | Rest Angle | Press Angle |
|---|---:|---:|---:|
| Servo A — power button | D9 | 90° | 60° |
| Servo B — blow trigger | D10 | 90° | 150° |

These values are the ones currently present in `spray_controller.ino`.

> Servo angles are mechanical calibration values. Re-check them whenever the servo mount, horn position, blower, or button geometry changes.

---

## Wiring

### Signal wiring

```text
Arduino Nano D9  -> Servo A signal
Arduino Nano D10 -> Servo B signal
```

### Servo power

Do **not** power both SG90 servos directly from the Nano's USB/5 V rail.

Recommended arrangement:

```text
External regulated 5 V + ----> Servo A V+
                           └--> Servo B V+

External supply GND ------> Servo A GND
                           ├--> Servo B GND
                           └--> Arduino GND

Arduino D9  -------------> Servo A signal
Arduino D10 -------------> Servo B signal
```

The Arduino and servo power supply **must share ground** so that the PWM signal has a common reference.

Separating servo power from USB power helps prevent Arduino resets and brownouts caused by servo current spikes.

---

## Host / Jetson Setup

Python 3 is required.

Create a virtual environment:

```bash
python3 -m venv .venv
source .venv/bin/activate
```

Install Python dependencies:

```bash
pip install -r requirements.txt
```

Make the script executable if desired:

```bash
chmod +x spray.py
```

Then either run:

```bash
python3 spray.py 5
```

or:

```bash
./spray.py 5
```

---

## Serial Port Permissions on Ubuntu / Jetson

First identify the Arduino:

```bash
ls /dev/ttyUSB*
ls /dev/ttyACM*
```

The current script defaults to:

```text
/dev/ttyUSB0
```

If permission is denied, add the current user to the `dialout` group:

```bash
sudo usermod -aG dialout $USER
```

Then log out and log back in, or reboot.

Check membership with:

```bash
groups
```

Do not run the system permanently using `sudo python3 ...` just to bypass serial permissions.

---

## Usage

### Spray by requested mass

```bash
python3 spray.py 5
```

or:

```bash
python3 spray.py 5g
```

The script computes the required duration using:

```python
SLOPE = 1.7611
INTERCEPT = -1.5001
seconds = (grams - INTERCEPT) / SLOPE
```

For example, a 5 g request corresponds to approximately:

```text
(5 - (-1.5001)) / 1.7611 ≈ 3.69 s
```

The host then sends:

```text
spray 3.69
```

to the Arduino.

### Raw duration mode

For calibration and characterization, append `s`:

```bash
python3 spray.py 2s
python3 spray.py 4s
python3 spray.py 6s
```

This bypasses the grams-to-seconds model.

### Different serial port

```bash
python3 spray.py 5 /dev/ttyUSB1
```

---

## Firmware Sequence

For a valid command, the firmware performs approximately:

1. Press Servo A to toggle blower ON.
2. Hold the power button.
3. Release Servo A.
4. Wait for blower stabilization.
5. Press Servo B.
6. Hold Servo B for the requested spray duration.
7. Release Servo B.
8. Press Servo A again to toggle blower OFF.
9. Return `OK`.

Current relevant firmware constants:

```cpp
const int A_REST  = 90;
const int A_PRESS = 60;

const int B_REST  = 90;
const int B_PRESS = 150;

const unsigned long A_HOLD = 2500;
const unsigned long SETTLE = 1500;
const unsigned long MOVE   = 400;
const unsigned long MAX_MS = 30000;
```

Servos are detached when idle to reduce holding current, noise, heating, and unintended loading on the blower buttons.

---

## Calibration

The current system uses a linear model:

```text
mass = slope × spray_time + intercept
```

Current parameters:

```text
slope     = 1.7611 g/s
intercept = -1.5001 g
```

These values are defined near the top of `spray.py`.

### Current characterization history

The prototype was characterized using sessions starting with approximately **50 g of powder**, with nominal 2 g, 4 g, and 6 g targets and three trials per target.

Observed behavior:

- regression converged to approximately `grams = 1.76 × seconds - 1.50`,
- overall fit was approximately `R² ≈ 0.80`,
- 6 g delivery reached approximately 9% coefficient of variation,
- 4 g repeatedly under-delivered by roughly 30%,
- powder flow appears to depend on remaining fill level.

This strongly suggests that spray duration alone does not capture all system state.

---

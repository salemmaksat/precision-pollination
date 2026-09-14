# Automated Date Palm Pollen Dispenser

Automated pollen/powder dispensing end-effector for a **UR10e robotic arm**, intended to run from a **Jetson computer with ROS 2**.

The current prototype uses a commercial air blower with a ~200 mL powder jar and two physical push buttons. Two SG90 servos mechanically actuate those buttons, while an Arduino Nano handles the low-level timing sequence. A Python script on the host converts a requested powder mass into a spray duration using an experimentally fitted linear regression.

The intended user-facing command is:

```bash
python3 spray.py 5
```

which requests approximately **5 g** of powder.

> **Important:** This system is currently open-loop. It estimates dispensed mass from spray duration; it does not measure powder mass during spraying. Output depends on powder properties, fill level, packing, nozzle condition, air flow, and environmental conditions.

---

## 1. Project Status

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

## 2. System Architecture

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

## 3. Repository Files

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

## 4. Hardware

### Main components

- UR10e robotic manipulator
- NVIDIA Jetson host computer
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

## 5. Wiring

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

## 6. Arduino Setup

### Required software

Install the Arduino IDE or Arduino CLI.

The firmware uses:

```cpp
#include <Servo.h>
```

`Servo` is part of the standard Arduino library ecosystem and is not installed through `pip`.

### Upload firmware

1. Connect the Arduino Nano over USB.
2. Open `spray_controller.ino`.
3. Select the correct Arduino Nano board/processor.
4. Select the correct serial port.
5. Upload the sketch.
6. Open the serial monitor at **9600 baud** if manual testing is required.

After boot/reset, the board should print:

```text
ready
```

### Starting condition

**Always begin a test with the blower physically OFF.**

The firmware assumes Servo A's first long press turns the blower ON and the second long press turns it OFF.

Because the blower button is a toggle, losing track of its state can invert the sequence.

---

## 7. Host / Jetson Setup

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

## 8. Serial Port Permissions on Ubuntu / Jetson

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

## 9. Usage

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

## 10. Firmware Sequence

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

## 11. Calibration

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

### Recommended calibration procedure

For each calibration session:

1. Start with a known powder mass and record it.
2. Keep powder type and preparation consistent.
3. Keep the nozzle geometry fixed.
4. Keep the end-effector orientation fixed.
5. Run several known spray durations using raw-second mode.
6. Weigh the powder output after each shot.
7. Record:
   - starting jar mass,
   - ending jar mass,
   - actual dispensed mass,
   - commanded duration,
   - shot order,
   - remaining powder,
   - environmental notes if relevant.
8. Fit:

```text
grams = slope × seconds + intercept
```

9. Replace `SLOPE` and `INTERCEPT` in `spray.py`.
10. Validate with independent trials that were not used to fit the model.

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

## 12. Known Limitations

### Open-loop mass estimation

The system does not currently use a load cell or other real-time mass measurement. Therefore the requested mass is an estimate.

### Fill-level dependence

Powder delivery rate changes as the jar empties. A single global linear regression may therefore be insufficient.

Potential improvements include:

- fill-level-dependent calibration,
- piecewise regression,
- polynomial/nonlinear model,
- maintaining a narrower jar fill range,
- closed-loop gravimetric dispensing,
- controlled hopper/auger feed upstream of the blower,
- vibration/agitation to reduce powder bridging.

### Powder behavior

Fine cohesive powders can:

- bridge,
- compact,
- cling electrostatically,
- form channels,
- settle differently between shots,
- respond to humidity.

Real date-palm pollen may behave differently from calibration powder, so final calibration should be performed with the intended pollen formulation when feasible.

### Blower state is not sensed

The Arduino assumes the blower starts OFF. There is currently no feedback confirming whether the blower is actually ON or OFF.

### Nozzle / orientation sensitivity

Flow may change with:

- nozzle angle,
- robot acceleration,
- tool orientation,
- powder redistribution during UR10e motion.

Calibration should ultimately be repeated under realistic robot poses and operating conditions.

---

## 13. Important Code / Documentation Mismatches to Verify

Before handoff or field use, verify these items physically and make the code/documentation consistent.

### Servo B press angle

The earlier mechanical calibration description used:

```text
90° -> 120°
```

but the current firmware contains:

```cpp
B_REST = 90;
B_PRESS = 150;
```

The firmware value of **150°** is documented in this README because it is what the uploaded code currently executes.

### Blower power-button hold

The blower is described as requiring a **3 second hold**, but the current firmware uses:

```cpp
A_HOLD = 2500;
```

which is **2.5 seconds**.

Confirm whether 2.5 s reliably toggles the commercial blower. Increase it if necessary.

### Maximum duration mismatch

Current Arduino firmware:

```cpp
MAX_MS = 30000;
```

so it rejects spray commands longer than **30 s**.

Current Python script accepts durations up to:

```text
60 s
```

For consistency, make the Python and Arduino limits identical.

---

## 14. Troubleshooting

### `Permission denied: /dev/ttyUSB0`

Add the user to `dialout`:

```bash
sudo usermod -aG dialout $USER
```

Then log out/in or reboot.

### `/dev/ttyUSB0` does not exist

Check:

```bash
ls /dev/ttyUSB*
ls /dev/ttyACM*
```

Then specify the actual port:

```bash
python3 spray.py 5 /dev/ttyACM0
```

### Script hangs waiting for `ready`

`spray.py` waits for the Arduino to print `ready` after the serial connection opens.

Check:

- correct serial device,
- firmware is uploaded,
- baud rate is 9600,
- USB cable supports data,
- board is not being used by Arduino Serial Monitor or another process.

### Arduino resets when servo moves

Likely servo power/current issue.

Check:

- servos use a dedicated regulated 5 V supply,
- Arduino and servo supply grounds are connected,
- supply can handle servo current spikes,
- USB is not being used as the servo power source.

### Servo buzzes or pushes button continuously

Check:

- rest angle,
- horn position,
- mechanical alignment,
- linkage preload.

The firmware detaches servos after movement, so persistent mechanical load usually indicates geometry/calibration problems.

### Blower turns OFF when it should turn ON

The blower probably started in the wrong state.

Manually return it to **OFF** before running another cycle.

### Dispensed mass is inaccurate

Check:

- remaining powder mass/fill level,
- powder packing,
- nozzle blockage,
- humidity,
- robot/tool orientation,
- powder type,
- regression coefficients.

Use raw-duration mode to diagnose flow separately from the mass conversion:

```bash
python3 spray.py 3s
```

---

## 15. ROS 2 Integration

The current Python script is intentionally independent of ROS 2.

A future ROS 2 node can wrap the same basic behavior:

```text
ROS 2 request
    -> target mass
    -> convert mass to duration
    -> send "spray <sec>" over serial
    -> wait for OK / ERR
    -> report action result
```

Recommended ROS 2 interface options:

- service for simple blocking requests,
- action server if cancellation/feedback is required.

A useful future interface might conceptually expose:

```text
/spray_pollen
target_mass_g: 5.0
```

and return:

```text
success
requested_mass_g
commanded_duration_s
controller_response
```

Keep the serial protocol simple and deterministic; the Arduino should remain responsible for the complete physical actuation cycle.


## 17. Dependency Summary

### Host Python

Installed from `requirements.txt`:

- `pyserial`

### Arduino

- Arduino `Servo` library

### Future ROS 2 system

ROS 2 dependencies are intentionally **not** included in `requirements.txt` yet because the uploaded project does not currently contain a ROS 2 package or ROS 2 Python node.

When ROS 2 integration is added, manage ROS dependencies using the ROS package metadata (`package.xml`) and the system package manager rather than treating ROS 2 as a normal pip dependency.


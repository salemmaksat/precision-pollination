// spray_controller: "spray <sec>" -> power on (A), settle, blow (B), power off (A).
// Start each session with the blower OFF. Servos detached when idle.
// A: D9 (power button, 3s hold toggles). B: D10 (blow while held). 9600 baud.

#include <Servo.h>

const int A_REST = 90, A_PRESS = 60;   // calibrated angles
const int B_REST = 90, B_PRESS = 150;
const unsigned long A_HOLD = 2500, SETTLE = 1500, MOVE = 400, MAX_MS = 30000;

Servo sa, sb;

void press(Servo& s, byte pin, int pressAngle, int restAngle, unsigned long holdMs) {
  s.attach(pin);
  s.write(pressAngle);
  delay(holdMs);
  s.write(restAngle);
  delay(MOVE);
  s.detach();
}

void setup() {
  Serial.begin(9600);
  sa.attach(9);  sa.write(A_REST);
  sb.attach(10); sb.write(B_REST);
  delay(500);
  sa.detach(); sb.detach();
  Serial.println(F("ready"));
}

void loop() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (!line.length()) return;

  if (!line.startsWith("spray")) { Serial.println(F("ERR use: spray <sec>")); return; }
  float secs = line.substring(5).toFloat();
  unsigned long ms = (unsigned long)(secs * 1000 + 0.5);
  if (secs <= 0 || ms > MAX_MS) { Serial.println(F("ERR bad duration")); return; }

  press(sa, 9, A_PRESS, A_REST, A_HOLD);    // on
  delay(SETTLE);
  press(sb, 10, B_PRESS, B_REST, ms);       // blow
  press(sa, 9, A_PRESS, A_REST, A_HOLD);    // off
  while (Serial.available()) Serial.read();

  Serial.print(F("OK spray ")); Serial.println(secs, 3);
}

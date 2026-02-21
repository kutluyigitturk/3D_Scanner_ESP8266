/*
 * ============================================================
 *  3D SCANNER — Arduino Uno Controller
 *
 *  [1] Angle settings moved into a clean CONFIGURATION block.
 *  [2] ADAPTIVE MEDIAN FILTER:
 *      - Empty area (no object) → 1 fast reading, discard if invalid.
 *      - Object detected        → take 3 readings, return median.
 *      Empty areas are swept quickly while object accuracy is preserved.
 *
 *  WIRING:
 *    Servo Yaw   → D9    Servo Pitch → D10
 *    VL53L0X SDA → A4    VL53L0X SCL → A5
 *    ESP8266 RX  ← D3    ESP8266 TX  → D2
 * ============================================================
 */

#include <Wire.h>
#include <VL53L0X.h>
#include <Servo.h>
#include <SoftwareSerial.h>

SoftwareSerial espSerial(2, 3);
VL53L0X sensor;
Servo yawServo;
Servo pitchServo;

// ╔══════════════════════════════════════════════════════════╗
// ║              >>> CONFIGURE HERE <<<                      ║
// ╠══════════════════════════════════════════════════════════╣
// ║  Pan (horizontal) and Tilt (vertical) angle limits       ║
// ║  Servos physically operate between 0–180°.               ║
// ║  Narrow the range with MIN/MAX to skip unwanted areas.   ║
// ╚══════════════════════════════════════════════════════════╝

const int YAW_MIN   = 0;    // Pan start angle  (°)  ← CHANGE
const int YAW_MAX   = 90;   // Pan end angle    (°)  ← CHANGE
const int PITCH_MIN = 40;   // Tilt start angle (°)  ← CHANGE
const int PITCH_MAX = 110;  // Tilt end angle   (°)  ← CHANGE

// Step size: smaller = more points but slower scan
const int STEP_YAW   = 1;  // (°)  ← CHANGE
const int STEP_PITCH = 1;  // (°)  ← CHANGE

// Valid distance window (meters) — readings outside this range are discarded
const float VALID_MIN = 0.04f;  // (m)  ← CHANGE
const float VALID_MAX = 0.30f;  // (m)  ← CHANGE

// ── Coordinate offsets (do not touch) ────────────────────────
// These define the mathematical origin for 3D coordinate calculation.
// They affect output coordinates, not physical servo angles.
const float YAW_OFFSET    = -40.0f;
const float PITCH_OFFSET  = -45.0f;
const float SENSOR_OFFSET = 0.012f;  // Mechanical mounting offset (m)

// ── Timing ───────────────────────────────────────────────────
const int PIN_YAW     = 9;
const int PIN_PITCH   = 10;
const int SETTLE_MS   = 60;
const int SERIAL_WAIT = 25;

int currentYaw   = 40;
int currentPitch = 45;

// ── Detach PWM, send data, re-attach ─────────────────────────
void safeSend(const String& data) {
  yawServo.detach();
  pitchServo.detach();

  espSerial.println(data);
  delay(SERIAL_WAIT);

  yawServo.attach(PIN_YAW);
  pitchServo.attach(PIN_PITCH);
  yawServo.write(currentYaw);
  pitchServo.write(currentPitch);
  delay(10);
}

void moveYaw(int pos)   { currentYaw = pos;   yawServo.write(pos); }
void movePitch(int pos) { currentPitch = pos; pitchServo.write(pos); }

// ── Adaptive median-filtered sensor reading ───────────────────
//
//  STEP 1 — Fast pre-scan (1 reading):
//    If invalid (empty area), return -1 immediately → fast iteration.
//
//  STEP 2 — Object detected, take 2 more readings:
//    Return the median of all 3 → noise suppressed.
//
//  Empty area : ~70ms  (1 × 66ms budget)
//  Object hit : ~210ms (3 × 66ms budget + 2 × 5ms delay)
//
float readFiltered() {
  // ── Step 1: Fast pre-reading ──────────────────────────────
  float first = sensor.readRangeSingleMillimeters() * 0.001f;

  if (sensor.timeoutOccurred() || first < VALID_MIN || first > VALID_MAX) {
    return -1.0f;  // Empty area → exit fast
  }

  // ── Step 2: Object found, take 2 more readings (3 total) ─
  float buf[3];
  buf[0] = first;
  int cnt = 1;

  for (int i = 0; i < 2; i++) {
    delay(5);
    float d = sensor.readRangeSingleMillimeters() * 0.001f;
    if (!sensor.timeoutOccurred() && d >= VALID_MIN && d <= VALID_MAX)
      buf[cnt++] = d;
  }

  if (cnt == 1) return buf[0];  // Other readings invalid, use the first one

  // Simple sort → median
  for (int i = 0; i < cnt - 1; i++)
    for (int j = i + 1; j < cnt; j++)
      if (buf[j] < buf[i]) { float t = buf[i]; buf[i] = buf[j]; buf[j] = t; }

  return buf[cnt / 2];
}

// ── Cartesian conversion ──────────────────────────────────────
String buildPoint(int yaw, int pitch, float dist) {
  float yr = (yaw   + YAW_OFFSET)   * (M_PI / 180.0f);
  float pr = (pitch + PITCH_OFFSET)  * (M_PI / 180.0f);
  float od = SENSOR_OFFSET + dist;
  float x  = -sin(yr) * od * cos(pr);
  float y  =  cos(yr) * od * cos(pr);
  float z  =  od * sin(pr);
  return String(x, 4) + "," + String(y, 4) + "," + String(z, 4);
}

// ── Main Scan ─────────────────────────────────────────────────
void scan() {
  Serial.println(F("[UNO] Scan starting"));
  safeSend("START");

  bool hasPending = false;
  String pending  = "";

  for (int yaw = YAW_MIN; yaw <= YAW_MAX; yaw += STEP_YAW) {
    moveYaw(yaw);

    if (hasPending) {
      delay(10);
      safeSend(pending);
      hasPending = false;
      delay(SETTLE_MS - 10);
    } else {
      delay(SETTLE_MS);
    }

    for (int pitch = PITCH_MIN; pitch <= PITCH_MAX; pitch += STEP_PITCH) {
      movePitch(pitch);

      if (hasPending) {
        delay(10);
        safeSend(pending);
        hasPending = false;
        delay(SETTLE_MS - 10);
      } else {
        delay(SETTLE_MS);
      }

      float d = readFiltered();
      if (d > 0) {
        pending    = buildPoint(yaw, pitch, d);
        hasPending = true;
        Serial.print(F("Y:")); Serial.print(yaw);
        Serial.print(F(" P:")); Serial.print(pitch);
        Serial.print(F(" D:")); Serial.println(d, 3);
      }
    }
  }

  if (hasPending) safeSend(pending);

  delay(50);
  safeSend("END");
  moveYaw((YAW_MIN + YAW_MAX) / 2);
  movePitch((PITCH_MIN + PITCH_MAX) / 2);
  Serial.println(F("[UNO] Scan complete"));
}

void setup() {
  Serial.begin(115200);
  espSerial.begin(9600);
  Wire.begin();

  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println(F("[UNO] VL53L0X not found!"));
    while (1) delay(500);
  }
  sensor.setMeasurementTimingBudget(66000);

  yawServo.attach(PIN_YAW);
  pitchServo.attach(PIN_PITCH);
  moveYaw((YAW_MIN + YAW_MAX) / 2);
  movePitch((PITCH_MIN + PITCH_MAX) / 2);
  delay(1500);

  Serial.println(F("[UNO] Ready. Waiting for SCAN command from ESP..."));
}

void loop() {
  if (espSerial.available()) {
    String cmd = espSerial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "SCAN") {
      Serial.println(F("[UNO] SCAN received"));
      scan();
    }
  }
}

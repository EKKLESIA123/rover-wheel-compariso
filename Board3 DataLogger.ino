/**
 * @file    rover_logger.ino
 * @brief   Mars Rover Data Logger — Board 3 (Sensor & SD)
 *
 * Logs IMU, power, and motor current data at 50 Hz to SD card (CSV).
 * Displays real-time telemetry on a 20×4 I²C LCD.
 *
 * Hardware:
 *   - Arduino MEGA 2560
 *   - MPU6050  (IMU, I²C 0x68)
 *   - ACS712-5A (total current, A0)
 *   - Voltage divider R1=20kΩ / R2=10kΩ (A3)
 *   - BTS7960 RIS/LIS current sense (A1, A2, A4, A5)
 *   - MicroSD module (SPI, CS=53)
 *   - LCD 20×4 I²C 0x27
 */

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal_I2C.h>

// ─────────────────────────────────────────────
//  MPU6050
// ─────────────────────────────────────────────
#define MPU_ADDR     0x68
const float ACCEL_LSB = 16384.0;   // ±2 g  → LSB/g
const float GYRO_LSB  = 131.0;     // ±250 dps → LSB/(°/s)

// ─────────────────────────────────────────────
//  LCD 20×4
// ─────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ─────────────────────────────────────────────
//  Pin Map (Arduino MEGA)
// ─────────────────────────────────────────────
const int PIN_SD_CS   = 53;
const int PIN_CURRENT = A0;   // ACS712-5A  — total current
const int PIN_VOLTAGE = A3;   // Voltage divider
const int PIN_RIS_L   = A1;   // BTS7960 LEFT  — forward sense
const int PIN_LIS_L   = A2;   // BTS7960 LEFT  — reverse sense
const int PIN_RIS_R   = A4;   // BTS7960 RIGHT — forward sense
const int PIN_LIS_R   = A5;   // BTS7960 RIGHT — reverse sense

// ─────────────────────────────────────────────
//  ACS712-5A Calibration
// ─────────────────────────────────────────────
const float ACS_SENS   = 0.185;     // Sensitivity  : 185 mV/A (5 A module)
const float ACS_OFFSET = 3.9587;    // Zero-current voltage (measured, not 2.5 V)
const float VREF       = 5.0;
const float ADC_RES    = 1023.0;

// ─────────────────────────────────────────────
//  Voltage Divider
// ─────────────────────────────────────────────
const float R1 = 20000.0;   // Ω
const float R2 = 10000.0;   // Ω

// ─────────────────────────────────────────────
//  BTS7960 Current Sense
//  I_motor = (V_sense / R_SENSE) × BTS_RATIO
//  Adjust R_SENSE to match the resistor on RIS/LIS
// ─────────────────────────────────────────────
const float R_SENSE   = 1000.0;   // Ω  — resistor on sense pin
const float BTS_RATIO = 8500.0;   // 1:8500 per datasheet

// ─────────────────────────────────────────────
//  Runtime State
// ─────────────────────────────────────────────
float totalEnergy_Wh = 0;
float rollF  = 0, pitchF  = 0;   // complementary-filter angles
float currentF = 0;               // low-pass filtered current

File logFile;
unsigned long lastLog     = 0;
unsigned long lastFlush   = 0;
unsigned long lastDisplay = 0;
unsigned long prevTime    = 0;
bool firstLoop            = true;

// ─────────────────────────────────────────────
//  MPU6050 — Initialize
// ─────────────────────────────────────────────
void mpuInit() {
  // Wake up (clear sleep bit)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);
  Wire.endTransmission();

  // Accel full-scale: ±2 g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C); Wire.write(0x00);
  Wire.endTransmission();

  // Gyro full-scale: ±250 dps
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B); Wire.write(0x00);
  Wire.endTransmission();

  // DLPF: 44 Hz (reduces high-frequency noise)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1A); Wire.write(0x03);
  Wire.endTransmission();
}

// ─────────────────────────────────────────────
//  MPU6050 — Raw Read (14 bytes, 0x3B)
// ─────────────────────────────────────────────
struct ImuRaw { int16_t ax, ay, az, gx, gy, gz; };

ImuRaw readMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  ImuRaw d;
  d.ax = Wire.read() << 8 | Wire.read();
  d.ay = Wire.read() << 8 | Wire.read();
  d.az = Wire.read() << 8 | Wire.read();
  Wire.read(); Wire.read();   // temperature — unused
  d.gx = Wire.read() << 8 | Wire.read();
  d.gy = Wire.read() << 8 | Wire.read();
  d.gz = Wire.read() << 8 | Wire.read();

  // Axis remap to match physical mounting orientation
  d.ax = -d.ax; d.az = -d.az;
  d.gx = -d.gx; d.gz = -d.gz;
  return d;
}

// ─────────────────────────────────────────────
//  ADC — Oversampled average (reduces noise)
// ─────────────────────────────────────────────
float readSmooth(int pin, int n = 10) {
  long sum = 0;
  for (int i = 0; i < n; i++) sum += analogRead(pin);
  return sum / (float)n;
}

// ─────────────────────────────────────────────
//  BTS7960 RIS/LIS → Motor current (A)
// ─────────────────────────────────────────────
float senseToAmps(int pin) {
  float v = (readSmooth(pin) * VREF) / ADC_RES;
  return (v / R_SENSE) * BTS_RATIO;
}

// ─────────────────────────────────────────────
//  LCD — Safe print (pad / truncate to maxLen)
// ─────────────────────────────────────────────
void lcdPrint(int col, int row, String s, int maxLen) {
  lcd.setCursor(col, row);
  while ((int)s.length() < maxLen) s += ' ';
  lcd.print(s.substring(0, maxLen));
}

// ─────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(500);

  lcd.init();
  lcd.backlight();
  lcdPrint(0, 0, "Initializing...", 20);

  mpuInit();
  Serial.println("[OK] MPU6050");

  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("[ERR] SD init failed");
    lcdPrint(0, 1, "SD Card FAILED!", 20);
    while (true);
  }

  // Auto-increment filename: RUN001.CSV, RUN002.CSV, …
  char filename[16];
  for (int i = 1; i <= 999; i++) {
    snprintf(filename, sizeof(filename), "RUN%03d.CSV", i);
    if (!SD.exists(filename)) break;
  }

  logFile = SD.open(filename, FILE_WRITE);
  if (!logFile) {
    Serial.println("[ERR] Cannot open log file");
    while (true);
  }

  // CSV header — 18 columns
  logFile.println(
    "time_ms,dt_s,"
    "ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw,"
    "roll_deg,pitch_deg,"
    "current_A,voltage_V,power_W,energy_Wh,"
    "I_L_fwd_A,I_L_rev_A,I_R_fwd_A,I_R_rev_A"
  );
  logFile.flush();

  Serial.print("[OK] Logging → ");
  Serial.println(filename);

  lcd.clear();
  lcdPrint(0, 0, "Ready!", 20);
  lcdPrint(0, 1, filename, 20);
  delay(1500);
  lcd.clear();
}

// ─────────────────────────────────────────────
//  Main Loop — 50 Hz (20 ms interval)
// ─────────────────────────────────────────────
void loop() {
  unsigned long now = millis();
  if (now - lastLog < 20) return;
  lastLog = now;

  // Time delta — fixed 20 ms on first iteration to prevent NaN
  float dt = firstLoop ? 0.02f : (now - prevTime) / 1000.0f;
  prevTime  = now;
  firstLoop = false;

  // ── 1. IMU ────────────────────────────────
  ImuRaw imu = readMPU();

  float ax_g = imu.ax / ACCEL_LSB;
  float ay_g = imu.ay / ACCEL_LSB;
  float az_g = imu.az / ACCEL_LSB;

  // Tilt angles from accelerometer
  float roll  = atan2(ay_g, az_g) * 180.0 / PI;
  float pitch = atan2(-ax_g, sqrt(ay_g * ay_g + az_g * az_g)) * 180.0 / PI;

  float gx_dps = imu.gx / GYRO_LSB;
  float gy_dps = imu.gy / GYRO_LSB;

  // Complementary filter: 98 % gyro + 2 % accel
  rollF  = 0.98f * (rollF  + gx_dps * dt) + 0.02f * roll;
  pitchF = 0.98f * (pitchF + gy_dps * dt) + 0.02f * pitch;

  // ── 2. ACS712 — Total current ─────────────
  float rawV   = (readSmooth(PIN_CURRENT) * VREF) / ADC_RES;
  float rawI   = (rawV - ACS_OFFSET) / ACS_SENS;
  currentF     = 0.90f * currentF + 0.10f * rawI;   // IIR low-pass (α=0.1)
  float current = currentF;   // negative = reverse; kept intentionally

  // ── 3. Battery voltage ────────────────────
  float vADC   = (readSmooth(PIN_VOLTAGE) * VREF) / ADC_RES;
  float voltage = vADC * ((R1 + R2) / R2);

  // ── 4. Power & cumulative energy ──────────
  float power = voltage * abs(current);   // |I| — energy is always positive
  totalEnergy_Wh += power * (dt / 3600.0f);

  // ── 5. BTS7960 per-side current sense ─────
  float I_L_fwd = senseToAmps(PIN_RIS_L);
  float I_L_rev = senseToAmps(PIN_LIS_L);
  float I_R_fwd = senseToAmps(PIN_RIS_R);
  float I_R_rev = senseToAmps(PIN_LIS_R);

  // ── 6. Write CSV row ───────────────────────
  logFile.print(now);               logFile.print(',');
  logFile.print(dt, 4);             logFile.print(',');
  logFile.print(imu.ax);            logFile.print(',');
  logFile.print(imu.ay);            logFile.print(',');
  logFile.print(imu.az);            logFile.print(',');
  logFile.print(imu.gx);            logFile.print(',');
  logFile.print(imu.gy);            logFile.print(',');
  logFile.print(imu.gz);            logFile.print(',');
  logFile.print(rollF, 2);          logFile.print(',');
  logFile.print(pitchF, 2);         logFile.print(',');
  logFile.print(current, 3);        logFile.print(',');
  logFile.print(voltage, 2);        logFile.print(',');
  logFile.print(power, 2);          logFile.print(',');
  logFile.print(totalEnergy_Wh, 5); logFile.print(',');
  logFile.print(I_L_fwd, 3);        logFile.print(',');
  logFile.print(I_L_rev, 3);        logFile.print(',');
  logFile.print(I_R_fwd, 3);        logFile.print(',');
  logFile.println(I_R_rev, 3);

  // Flush every 2 s to balance SD write cycles vs. data safety
  if (now - lastFlush >= 2000) {
    logFile.flush();
    lastFlush = now;
  }

  // ── 7. Display update (500 ms) ────────────
  if (now - lastDisplay >= 500) {
    lastDisplay = now;

    // Row 0: Attitude
    lcdPrint(0, 0, "R:" + String(rollF, 1) + " P:" + String(pitchF, 1), 20);

    // Row 1: Voltage & Current
    lcdPrint(0, 1, "V:" + String(voltage, 1) + "V I:" + String(current, 2) + "A", 20);

    // Row 2: Power & Energy
    lcdPrint(0, 2, "P:" + String(power, 1) + "W E:" + String(totalEnergy_Wh, 3), 20);

    // Row 3: Per-side motor current (fwd/rev)
    lcdPrint(0, 3,
      "L:" + String(I_L_fwd, 1) + "/" + String(I_L_rev, 1) +
      " R:" + String(I_R_fwd, 1) + "/" + String(I_R_rev, 1), 20);

    // Serial monitor
    Serial.print("t=");  Serial.print(now);
    Serial.print(" R="); Serial.print(rollF, 1);
    Serial.print(" P="); Serial.print(pitchF, 1);
    Serial.print(" I="); Serial.print(current, 2);  Serial.print("A");
    Serial.print(" V="); Serial.print(voltage, 1);  Serial.print("V");
    Serial.print(" P="); Serial.print(power, 1);    Serial.print("W");
    Serial.print(" E="); Serial.print(totalEnergy_Wh, 4); Serial.print("Wh");
    Serial.print(" IL="); Serial.print(I_L_fwd, 1);
    Serial.print("/");   Serial.print(I_L_rev, 1);
    Serial.print(" IR="); Serial.print(I_R_fwd, 1);
    Serial.print("/");   Serial.println(I_R_rev, 1);
  }
}

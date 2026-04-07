// ============================================================
//  BOARD 3 — DATA LOGGER & SENSOR HUB
//  Mars Rover RC Project
// ============================================================
//
//  
//
//  MAIN TASKS:
//    - IMU (MPU6050): Acc + Gyro → Roll/Pitch
//    - Current: ACS712 (กระแสรวม)
//    - Voltage: Voltage Divider
//    - Motor Current: BTS7960 (ซ้าย/ขวา)
//    - Display: LCD 20x4
//    - Storage: SD Card (CSV)
//
//  DATA RATE:
//    Logging ~50Hz (20ms)
//
// ============================================================

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal_I2C.h>

// ============================================================
//  MPU6050 CONFIG
// ============================================================
#define MPU_ADDR 0x68
const float ACCEL_LSB = 16384.0;
const float GYRO_LSB  = 131.0;

// ============================================================
//  LCD CONFIG
// ============================================================
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ============================================================
//  PIN MAP (Arduino MEGA)
// ============================================================
//  A0  → ACS712
//  A1  → BTS7960 L_RIS
//  A2  → BTS7960 L_LIS
//  A3  → Voltage
//  A4  → BTS7960 R_RIS
//  A5  → BTS7960 R_LIS
//  53  → SD CS
// ============================================================
const int PIN_SD_CS   = 53;
const int PIN_CURRENT = A0;
const int PIN_VOLTAGE = A3;
const int PIN_RIS_L   = A1;
const int PIN_LIS_L   = A2;
const int PIN_RIS_R   = A4;
const int PIN_LIS_R   = A5;

// ============================================================
//  SENSOR CONSTANTS
// ============================================================

// ACS712
const float ACS_SENS   = 0.185;
const float ACS_OFFSET = 3.9587;
const float VREF       = 5.0;
const float ADC_RES    = 1023.0;

// Voltage Divider
const float R1 = 20000.0;
const float R2 = 10000.0;

// BTS7960
const float R_SENSE   = 1000.0;
const float BTS_RATIO = 8500.0;

// ============================================================
//  SYSTEM VARIABLES
// ============================================================
float totalEnergy_Wh = 0;
float rollF = 0, pitchF = 0;
float currentF = 0;

// ============================================================
//  TIMING CONTROL
// ============================================================
File logFile;

unsigned long lastLog     = 0;
unsigned long lastFlush   = 0;
unsigned long lastDisplay = 0;
unsigned long prevTime    = 0;

bool firstLoop = true;

// ============================================================
//  MPU INIT
// ============================================================
void mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C); Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B); Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1A); Wire.write(0x03);
  Wire.endTransmission();
}

// ============================================================
//  IMU STRUCT
// ============================================================
struct ImuRaw {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
};

// ============================================================
//  READ IMU
// ============================================================
ImuRaw readMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  ImuRaw d;
  d.ax = Wire.read() << 8 | Wire.read();
  d.ay = Wire.read() << 8 | Wire.read();
  d.az = Wire.read() << 8 | Wire.read();

  Wire.read(); Wire.read();

  d.gx = Wire.read() << 8 | Wire.read();
  d.gy = Wire.read() << 8 | Wire.read();
  d.gz = Wire.read() << 8 | Wire.read();

  return d;
}

// ============================================================
//  SMOOTH ADC
// ============================================================
float readSmooth(int pin, int n = 10) {
  long sum = 0;
  for (int i = 0; i < n; i++) sum += analogRead(pin);
  return sum / (float)n;
}

// ============================================================
//  CURRENT (ACS712)
// ============================================================
float readCurrent() {
  float v = (readSmooth(PIN_CURRENT) * VREF) / ADC_RES;
  return (v - ACS_OFFSET) / ACS_SENS;
}

// ============================================================
//  VOLTAGE
// ============================================================
float readVoltage() {
  float v = (readSmooth(PIN_VOLTAGE) * VREF) / ADC_RES;
  return v * (R1 + R2) / R2;
}

// ============================================================
//  BTS CURRENT
// ============================================================
float senseToAmps(int pin) {
  float v = (readSmooth(pin) * VREF) / ADC_RES;
  return (v / R_SENSE) * BTS_RATIO;
}

// ============================================================
//  SETUP
// ============================================================
void setup() {

  // Serial Monitor (Debug)
  Serial.begin(115200);

  // I2C Start
  Wire.begin();

  // LCD Init
  lcd.init();
  lcd.backlight();
  lcd.print("Initializing...");

  // IMU Init
  mpuInit();

  // SD Card Init
  if (!SD.begin(PIN_SD_CS)) {
    lcd.setCursor(0, 1);
    lcd.print("SD FAIL");
    while (1);
  }

  // Create File (RUN001.CSV etc.)
  char filename[15];
  for (int i = 0; i < 1000; i++) {
    sprintf(filename, "RUN%03d.CSV", i);
    if (!SD.exists(filename)) {
      logFile = SD.open(filename, FILE_WRITE);
      break;
    }
  }

  // CSV Header
  logFile.println("time,voltage,current,roll,pitch");

  prevTime = millis();
}

// ============================================================
//  MAIN LOOP
// ============================================================
void loop() {

  unsigned long now = millis();

  // =========================
  //  TIMING (50Hz)
  // =========================
  if (now - lastLog >= 20) {
    lastLog = now;

    // -------------------------
    //  READ SENSORS
    // -------------------------
    ImuRaw imu = readMPU();

    float ax = imu.ax / ACCEL_LSB;
    float ay = imu.ay / ACCEL_LSB;
    float az = imu.az / ACCEL_LSB;

    float gx = imu.gx / GYRO_LSB;
    float gy = imu.gy / GYRO_LSB;

    float voltage = readVoltage();
    float current = readCurrent();

    // -------------------------
    //  ANGLE CALCULATION
    // -------------------------
    float dt = (now - prevTime) / 1000.0;
    prevTime = now;

    float rollAcc  = atan2(ay, az) * 180 / PI;
    float pitchAcc = atan2(-ax, sqrt(ay * ay + az * az)) * 180 / PI;

    rollF  = 0.98 * (rollF  + gx * dt) + 0.02 * rollAcc;
    pitchF = 0.98 * (pitchF + gy * dt) + 0.02 * pitchAcc;

    // -------------------------
    //  ENERGY CALCULATION
    // -------------------------
    float power = voltage * current;
    totalEnergy_Wh += power * dt / 3600.0;

    // -------------------------
    //  LOG DATA (CSV)
    // -------------------------
    logFile.print(now);
    logFile.print(",");
    logFile.print(voltage);
    logFile.print(",");
    logFile.print(current);
    logFile.print(",");
    logFile.print(rollF);
    logFile.print(",");
    logFile.println(pitchF);

    // -------------------------
    //  DISPLAY (LCD)
    // -------------------------
    if (now - lastDisplay > 200) {
      lastDisplay = now;

      lcd.setCursor(0, 0);
      lcd.print("V:");
      lcd.print(voltage);

      lcd.setCursor(0, 1);
      lcd.print("I:");
      lcd.print(current);

      lcd.setCursor(0, 2);
      lcd.print("R:");
      lcd.print(rollF);

      lcd.setCursor(0, 3);
      lcd.print("E:");
      lcd.print(totalEnergy_Wh);
    }

    // -------------------------
    //  FLUSH SD 
    // -------------------------
    if (now - lastFlush > 1000) {
      lastFlush = now;
      logFile.flush();
    }
  }
}

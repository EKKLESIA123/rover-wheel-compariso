#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal_I2C.h>

// ===== MPU6050 =====
#define MPU_ADDR 0x68
const float ACCEL_LSB    = 16384.0;
const float GYRO_LSB     = 131.0;

// ===== LCD 20x4 =====
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ===== Pins (Arduino MEGA) =====
const int PIN_SD_CS   = 53;
const int PIN_CURRENT = A0;   // ACS712-5A
const int PIN_VOLTAGE = A3;   // Voltage Divider
const int PIN_RIS_L   = A1;   // BTS7960 LEFT  RIS
const int PIN_LIS_L   = A2;   // BTS7960 LEFT  LIS
const int PIN_RIS_R   = A4;   // BTS7960 RIGHT RIS
const int PIN_LIS_R   = A5;   // BTS7960 RIGHT LIS

// ===== ACS712-5A Calibration =====
const float ACS_SENS   = 0.185;    // 5A version
const float ACS_OFFSET = 3.9587;   // วัดจริงตอน calibrate
const float VREF       = 5.0;
const float ADC_RES    = 1023.0;

// ===== Voltage Divider =====
const float R1 = 20000.0;
const float R2 = 10000.0;

// ===== BTS7960 Current Sense =====
// RIS/LIS output: Iout / 8500 (datasheet)
// V = I_sense * R_sense (ถ้าต่อ resistor 1kΩ ที่ขา RIS/LIS)
// แก้ R_SENSE ให้ตรงกับ resistor ที่ใช้จริง
const float R_SENSE     = 1000.0;  // Ohm
const float BTS_RATIO   = 8500.0;  // 1:8500

// ===== State =====
float totalEnergy_Wh = 0;
float rollF = 0, pitchF = 0;
float currentF = 0;

File logFile;
unsigned long lastLog         = 0;
unsigned long lastFlush       = 0;
unsigned long lastDisplay     = 0;
unsigned long prevTime        = 0;
bool firstLoop                = true;

// ===== MPU6050 =====
void mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);  // wake
  Wire.endTransmission();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C); Wire.write(0x00);  // accel +-2g
  Wire.endTransmission();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B); Wire.write(0x00);  // gyro +-250dps
  Wire.endTransmission();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1A); Wire.write(0x03);  // DLPF 44Hz
  Wire.endTransmission();
}

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
  Wire.read(); Wire.read();  // skip temp
  d.gx = Wire.read() << 8 | Wire.read();
  d.gy = Wire.read() << 8 | Wire.read();
  d.gz = Wire.read() << 8 | Wire.read();
  // remap: กลับแกนให้ตรงกับทิศทางติดตั้งจริง
  d.ax = -d.ax; d.az = -d.az;
  d.gx = -d.gx; d.gz = -d.gz;
  return d;
}

// ===== ADC smooth =====
float readSmooth(int pin, int n = 10) {
  long sum = 0;
  for (int i = 0; i < n; i++) sum += analogRead(pin);
  return sum / (float)n;
}

// ===== BTS7960 RIS/LIS → Ampere =====
// I_motor = (V_sense / R_SENSE) * BTS_RATIO
float senseToAmps(int pin) {
  float v = (readSmooth(pin) * VREF) / ADC_RES;
  return (v / R_SENSE) * BTS_RATIO;
}

// ===== LCD helper: พิมพ์แบบไม่ล้น =====
void lcdPrint(int col, int row, String s, int maxLen) {
  lcd.setCursor(col, row);
  while ((int)s.length() < maxLen) s += ' ';
  lcd.print(s.substring(0, maxLen));
}

// ===== Setup =====
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
    Serial.println("[ERR] SD FAIL");
    lcdPrint(0, 1, "SD Card FAILED!", 20);
    while (true);
  }

  // หาชื่อไฟล์ใหม่
  char filename[16];
  for (int i = 1; i <= 999; i++) {
    snprintf(filename, sizeof(filename), "RUN%03d.CSV", i);
    if (!SD.exists(filename)) break;
  }

  logFile = SD.open(filename, FILE_WRITE);
  if (!logFile) {
    Serial.println("[ERR] File open FAIL");
    while (true);
  }

  // CSV Header
  logFile.println(
    "time_ms,dt_s,"
    "ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw,"
    "roll_deg,pitch_deg,"
    "current_A,voltage_V,power_W,energy_Wh,"
    "I_L_fwd_A,I_L_rev_A,I_R_fwd_A,I_R_rev_A"
  );
  logFile.flush();

  Serial.print("[OK] Logging: ");
  Serial.println(filename);

  lcd.clear();
  lcdPrint(0, 0, "Ready!", 20);
  lcdPrint(0, 1, filename, 20);
  delay(1500);
  lcd.clear();
}

// ===== Loop =====
void loop() {
  unsigned long now = millis();
  if (now - lastLog < 20) return;  // 50 Hz
  lastLog = now;

  // dt — ป้องกัน NaN รอบแรก
  float dt = firstLoop ? 0.02 : (now - prevTime) / 1000.0;
  prevTime = now;
  firstLoop = false;

  // 1. IMU
  ImuRaw imu = readMPU();

  float ax_g = imu.ax / ACCEL_LSB;
  float ay_g = imu.ay / ACCEL_LSB;
  float az_g = imu.az / ACCEL_LSB;

  float roll  = atan2(ay_g, az_g) * 180.0 / PI;
  float pitch = atan2(-ax_g, sqrt(ay_g * ay_g + az_g * az_g)) * 180.0 / PI;

  float gx_dps = imu.gx / GYRO_LSB;
  float gy_dps = imu.gy / GYRO_LSB;

  rollF  = 0.98 * (rollF  + gx_dps * dt) + 0.02 * roll;
  pitchF = 0.98 * (pitchF + gy_dps * dt) + 0.02 * pitch;

  // 2. ACS712 กระแสรวม
  float rawV   = (readSmooth(PIN_CURRENT) * VREF) / ADC_RES;
  float rawI   = (rawV - ACS_OFFSET) / ACS_SENS;
  currentF     = 0.90 * currentF + 0.10 * rawI;
  float current = currentF;  // ไม่ตัดค่าลบ — ถอยหลังมีกระแสจริง

  // 3. Voltage
  float vADC  = (readSmooth(PIN_VOLTAGE) * VREF) / ADC_RES;
  float voltage = vADC * ((R1 + R2) / R2);

  // 4. Power & Energy
  float power = voltage * abs(current);  // power ใช้ abs เพราะพลังงานไม่มีลบ
  totalEnergy_Wh += power * (dt / 3600.0);

  // 5. BTS7960 Current Sense (RIS/LIS)
  float I_L_fwd = senseToAmps(PIN_RIS_L);
  float I_L_rev = senseToAmps(PIN_LIS_L);
  float I_R_fwd = senseToAmps(PIN_RIS_R);
  float I_R_rev = senseToAmps(PIN_LIS_R);

  // 6. บันทึก CSV
  logFile.print(now);              logFile.print(',');
  logFile.print(dt, 4);            logFile.print(',');
  logFile.print(imu.ax);           logFile.print(',');
  logFile.print(imu.ay);           logFile.print(',');
  logFile.print(imu.az);           logFile.print(',');
  logFile.print(imu.gx);           logFile.print(',');
  logFile.print(imu.gy);           logFile.print(',');
  logFile.print(imu.gz);           logFile.print(',');
  logFile.print(rollF, 2);         logFile.print(',');
  logFile.print(pitchF, 2);        logFile.print(',');
  logFile.print(current, 3);       logFile.print(',');
  logFile.print(voltage, 2);       logFile.print(',');
  logFile.print(power, 2);         logFile.print(',');
  logFile.print(totalEnergy_Wh, 5); logFile.print(',');
  logFile.print(I_L_fwd, 3);       logFile.print(',');
  logFile.print(I_L_rev, 3);       logFile.print(',');
  logFile.print(I_R_fwd, 3);       logFile.print(',');
  logFile.println(I_R_rev, 3);

  // flush ทุก 2 วินาที
  if (now - lastFlush >= 2000) {
    logFile.flush();
    lastFlush = now;
  }

  // 7. LCD + Serial ทุก 500ms
  if (now - lastDisplay >= 500) {
    lastDisplay = now;

    // บรรทัด 0: Roll Pitch
    String l0 = "R:" + String(rollF, 1) + " P:" + String(pitchF, 1);
    lcdPrint(0, 0, l0, 20);

    // บรรทัด 1: Voltage & Current
    String l1 = "V:" + String(voltage, 1) + "V I:" + String(current, 2) + "A";
    lcdPrint(0, 1, l1, 20);

    // บรรทัด 2: Power & Energy
    String l2 = "P:" + String(power, 1) + "W E:" + String(totalEnergy_Wh, 3);
    lcdPrint(0, 2, l2, 20);

    // บรรทัด 3: BTS current sense
    String l3 = "L:" + String(I_L_fwd, 1) + "/" + String(I_L_rev, 1)
              + " R:" + String(I_R_fwd, 1) + "/" + String(I_R_rev, 1);
    lcdPrint(0, 3, l3, 20);

    // Serial
    Serial.print("t="); Serial.print(now);
    Serial.print(" R="); Serial.print(rollF, 1);
    Serial.print(" P="); Serial.print(pitchF, 1);
    Serial.print(" I="); Serial.print(current, 2); Serial.print("A");
    Serial.print(" V="); Serial.print(voltage, 1); Serial.print("V");
    Serial.print(" P="); Serial.print(power, 1); Serial.print("W");
    Serial.print(" E="); Serial.print(totalEnergy_Wh, 4); Serial.print("Wh");
    Serial.print(" IL="); Serial.print(I_L_fwd, 1); Serial.print("/"); Serial.print(I_L_rev, 1);
    Serial.print(" IR="); Serial.print(I_R_fwd, 1); Serial.print("/"); Serial.println(I_R_rev, 1);
  }
}

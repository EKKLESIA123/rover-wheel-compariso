// ============================================================
//  BOARD 2 — MOTOR CONTROLLER
//  Mars Rover RC | BTS7960 Differential Drive
// ============================================================
//  Pin Map:
//    Pin 2  -> RC CH5 Mode switch     (interrupt)
//    Pin 3  -> RC CH2 Throttle        (interrupt)
//    Pin 4  -> Pivot signal from Board 1 (INPUT_PULLUP)
//    Pin 5  -> BTS7960 LEFT  RPWM (forward)
//    Pin 6  -> BTS7960 LEFT  LPWM (reverse)
//    Pin 9  -> BTS7960 RIGHT RPWM (forward)
//    Pin 10 -> BTS7960 RIGHT LPWM (reverse)
//    R_EN, L_EN of both BTS7960 -> 5V (always enabled)
//
//  Mode behavior:
//    Mode 1 (Cruise): fixed speed forward, arm required first
//    Mode 2 (Normal): throttle-controlled, both sides equal
//    Mode 3 (Pivot):  wait for Board 1 signal, then counter-rotate
//
//  Startup safety:
//    Motors are locked for STARTUP_LOCK_MS on power-on.
//    ISRs accumulate mode readings during this window so the
//    first confirmed mode is already stable when lock releases.
// ============================================================

// ---------- Pin definitions ----------
const int PIN_RC_MODE     = 2;   // Mode switch CH5 (interrupt)
const int PIN_RC_THROTTLE = 3;   // Throttle CH2 (interrupt)
const int PIN_DIAMOND_IN  = 4;   // Pivot-ready signal from Board 1
const int PIN_L_RPWM      = 5;   // Left motor forward PWM
const int PIN_L_LPWM      = 6;   // Left motor reverse PWM
const int PIN_R_RPWM      = 9;   // Right motor forward PWM
const int PIN_R_LPWM      = 10;  // Right motor reverse PWM

// ---------- Drive parameters ----------
const int  DEADZONE     = 50;    // PWM deadband around 1500us (prevents drift)
const int  CRUISE_SPEED = 160;   // Fixed PWM for cruise mode (0-255)
const int  MAX_PWM      = 255;
const int  RAMP_FWD     = 12;    // PWM increment per loop for forward (~0.4s to full)
const int  RAMP_REV     = 20;    // Faster ramp for reverse
const int  RAMP_BRAKE   = 36;    // Fast ramp to zero on brake

// ---------- Startup lock ----------
const unsigned long STARTUP_LOCK_MS = 1000;  // Hold motors off for 1s after boot
unsigned long startupTime   = 0;
bool          startupLocked = true;

// ---------- ISR: Throttle (interrupt-driven PWM) ----------
// Using interrupt instead of pulseIn avoids blocking the main loop.
// Filter rejects pulses outside valid RC range (800-2200us).
volatile unsigned long throttleRise = 0;
volatile unsigned long throttleRaw  = 1500;

void throttleISR() {
  if (digitalRead(PIN_RC_THROTTLE) == HIGH)
    throttleRise = micros();
  else {
    unsigned long w = micros() - throttleRise;
    if (w > 800 && w < 2200) throttleRaw = w;
  }
}

// ---------- ISR: Mode switch (interrupt-driven PWM) ----------
volatile unsigned long modeRise = 0;
volatile unsigned long modeRaw  = 1500;

void modeISR() {
  if (digitalRead(PIN_RC_MODE) == HIGH)
    modeRise = micros();
  else {
    unsigned long w = micros() - modeRise;
    if (w > 800 && w < 2200) modeRaw = w;
  }
}

// Atomic read for volatile unsigned long (32-bit on AVR is not atomic by default)
unsigned long readVolatileUL(volatile unsigned long &v) {
  unsigned long val;
  noInterrupts();
  val = v;
  interrupts();
  return val;
}

// ---------- Motor state ----------
int targetL  = 0, targetR  = 0;
int currentL = 0, currentR = 0;
bool cruiseArmed = false;  // Cruise requires explicit throttle input before activating

// ---------- Mode debounce ----------
// Requires CONFIRM_NEEDED consecutive identical readings before
// accepting a mode change — prevents noise-induced mode jumps.
int stableMode    = -1;   // -1 = unknown, motors locked until confirmed
int candidateMode = -1;
int confirmCount  = 0;
const int CONFIRM_NEEDED = 8;

// ---------- Diamond signal debounce ----------
// Debounce the digital signal from Board 1 to prevent false
// pivot triggers from electrical noise on the signal line.
bool stableDiamond    = false;
bool candidateDiamond = false;
int  diaConfirmCount  = 0;
const int DIA_CONFIRM = 3;

int rawPWMtoMode(int pw) {
  if (pw < 800 || pw > 2200) return -1;  // out-of-range — treat as unknown
  if (pw < 1200) return 1;
  if (pw < 1700) return 2;
  return 3;
}

int readMode() {
  int raw = rawPWMtoMode((int)readVolatileUL(modeRaw));
  if (raw == candidateMode) {
    confirmCount++;
    if (confirmCount >= CONFIRM_NEEDED) {
      stableMode   = candidateMode;
      confirmCount = CONFIRM_NEEDED;
    }
  } else {
    candidateMode = raw;
    confirmCount  = 0;
  }
  return stableMode;
}

bool readDiamond() {
  // INPUT_PULLUP: pin floats HIGH when disconnected.
  // Board 1 drives HIGH when pivot is ready — logic unchanged.
  // Hardware fix: add 10k resistor from Pin 4 to GND for reliability.
  bool raw = (digitalRead(PIN_DIAMOND_IN) == HIGH);
  if (raw == candidateDiamond) {
    diaConfirmCount++;
    if (diaConfirmCount >= DIA_CONFIRM) {
      stableDiamond   = candidateDiamond;
      diaConfirmCount = DIA_CONFIRM;
    }
  } else {
    candidateDiamond = raw;
    diaConfirmCount  = 0;
  }
  return stableDiamond;
}

// ---------- PWM to speed conversion ----------
// RC neutral = 1500us. Deadzone prevents motor movement from
// stick drift. Output is inverted because this RC transmitter
// sends high PWM for reverse (calibrated from hardware test).
int pwmToSpeed(int pwm) {
  pwm = constrain(pwm, 1000, 2000);
  int centered = pwm - 1500;
  if (abs(centered) < DEADZONE) return 0;
  int sign = (centered > 0) ? 1 : -1;
  int mag  = abs(centered) - DEADZONE;
  return -sign * map(mag, 0, 500 - DEADZONE, 0, MAX_PWM);
}

// ---------- Motor ramp ----------
// Ramp rate depends on direction to protect drivetrain:
// forward is gradual, reverse is faster, braking is fastest.
int rampToward(int current, int target) {
  int step = (target == 0) ? RAMP_BRAKE : (target < 0 ? RAMP_REV : RAMP_FWD);
  if (current < target) return min(current + step, target);
  if (current > target) return max(current - step, target);
  return current;
}

// ---------- BTS7960 driver ----------
// speed > 0: RPWM active (forward)
// speed < 0: LPWM active (reverse)
// speed = 0: both low (coast/brake)
void driveMotor(int rpwm, int lpwm, int speed) {
  speed = constrain(speed, -MAX_PWM, MAX_PWM);
  if (speed > 0)      { analogWrite(rpwm, speed); analogWrite(lpwm, 0); }
  else if (speed < 0) { analogWrite(rpwm, 0);     analogWrite(lpwm, -speed); }
  else                 { analogWrite(rpwm, 0);     analogWrite(lpwm, 0); }
}

// ============================================================
void setup() {
  Serial.begin(9600);

  pinMode(PIN_RC_MODE,     INPUT);
  pinMode(PIN_RC_THROTTLE, INPUT);
  pinMode(PIN_DIAMOND_IN,  INPUT_PULLUP);
  pinMode(PIN_L_RPWM, OUTPUT); pinMode(PIN_L_LPWM, OUTPUT);
  pinMode(PIN_R_RPWM, OUTPUT); pinMode(PIN_R_LPWM, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(PIN_RC_MODE),     modeISR,     CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_RC_THROTTLE), throttleISR, CHANGE);

  driveMotor(PIN_L_RPWM, PIN_L_LPWM, 0);
  driveMotor(PIN_R_RPWM, PIN_R_LPWM, 0);

  startupTime = millis();
  Serial.println("Board2 Motor Ready");
}

// ============================================================
void loop() {

  // Startup lock: keep motors off for STARTUP_LOCK_MS.
  // readMode/readDiamond still run to accumulate confirm counts
  // so the first usable mode is already stable when lock ends.
  if (startupLocked) {
    readMode();
    readDiamond();
    driveMotor(PIN_L_RPWM, PIN_L_LPWM, 0);
    driveMotor(PIN_R_RPWM, PIN_R_LPWM, 0);
    if (millis() - startupTime < STARTUP_LOCK_MS) {
      Serial.println("STARTUP LOCK...");
      delay(20);
      return;
    }
    startupLocked = false;
    Serial.println("READY");
  }

  int  mode      = readMode();
  bool isDiamond = readDiamond();
  int  speed     = pwmToSpeed((int)readVolatileUL(throttleRaw));

  // Mode unknown — keep motors locked until ISR confirms a valid mode
  if (mode == -1) {
    driveMotor(PIN_L_RPWM, PIN_L_LPWM, 0);
    driveMotor(PIN_R_RPWM, PIN_R_LPWM, 0);
    currentL = currentR = 0;
    Serial.println("WAITING: mode not confirmed...");
    delay(20);
    return;
  }

  if (mode == 1) {
    // Cruise: fixed speed, but requires a throttle push first.
    // Prevents rover from driving off immediately on power-on
    // if the switch is already in position 1.
    if (!cruiseArmed && abs(speed) > 0) cruiseArmed = true;
    targetL = cruiseArmed ? CRUISE_SPEED : 0;
    targetR = cruiseArmed ? CRUISE_SPEED : 0;

  } else {
    cruiseArmed = false;  // reset arm state when leaving cruise mode

    if (mode == 2) {
      // Normal drive: both motors follow throttle equally
      targetL = speed;
      targetR = speed;

    } else {
      // Pivot turn (Mode 3): counter-rotate left and right sides.
      // Wait for Board 1 to confirm servo settle before driving.
      if (isDiamond) {
        targetL =  speed;   // left side forward
        targetR = -speed;   // right side reverse -> CW rotation
                            // negative throttle -> CCW rotation
      } else {
        // Servos still moving to X position — hold motors
        targetL = 0;
        targetR = 0;
        currentL = rampToward(currentL, 0);
        currentR = rampToward(currentR, 0);
        driveMotor(PIN_L_RPWM, PIN_L_LPWM, currentL);
        driveMotor(PIN_R_RPWM, PIN_R_LPWM, currentR);
        Serial.println("PIVOT: waiting for servo settle...");
        delay(20);
        return;
      }
    }
  }

  currentL = rampToward(currentL, targetL);
  currentR = rampToward(currentR, targetR);
  driveMotor(PIN_L_RPWM, PIN_L_LPWM, currentL);
  driveMotor(PIN_R_RPWM, PIN_R_LPWM, currentR);

  Serial.print("M:"); Serial.print(mode);
  Serial.print(" D:"); Serial.print(isDiamond);
  Serial.print(" PW:"); Serial.print((int)readVolatileUL(throttleRaw));
  Serial.print(" spd:"); Serial.print(speed);
  Serial.print(" L:"); Serial.print(currentL);
  Serial.print(" R:"); Serial.println(currentR);

  delay(20);  // 50 Hz loop
}

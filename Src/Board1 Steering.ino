// ============================================================
//  BOARD 1 — STEERING CONTROLLER
//  Mars Rover RC | 4-Wheel Independent Steering
// ============================================================
//  Pin Map:
//    Pin 2  -> RC CH1 Steering (interrupt)
//    Pin 3  -> Signal OUT to Board 2 (HIGH = pivot ready)
//    Pin 5  -> RC CH5 Mode switch (pulseIn)
//    Pin 9  -> Servo FL (Front-Left)
//    Pin 10 -> Servo FR (Front-Right)
//    Pin 11 -> Servo RL (Rear-Left)
//    Pin 12 -> Servo RR (Rear-Right)
//
//  Mode switch positions (CH5 PWM):
//    < 1200 us -> Mode 1: Cruise (fixed speed, Ackermann steering)
//    < 1700 us -> Mode 2: Normal Drive (throttle + Ackermann)
//    >= 1700 us -> Mode 3: Pivot Turn (Diamond/X wheel config)
//
//  Inter-board signal (Pin 3 -> Board2 Pin 4):
//    LOW  = Ackermann mode (Board 2 drives normally)
//    HIGH = Pivot ready    (Board 2 enables counter-rotation)
// ============================================================

// Declare struct before #include to avoid Arduino IDE 1.x
// auto-prototype bug where return types are not yet known
struct WheelAngles {
  float fl, fr, rl, rr;
};

#include <Servo.h>

// ---------- Servo objects ----------
Servo servoFL, servoFR, servoRL, servoRR;

// ---------- Pin definitions ----------
const int PIN_RC_STEER = 2;   // Steering channel (interrupt-capable)
const int PIN_MODE_OUT = 3;   // Signal output to Board 2
const int PIN_RC_MODE  = 5;   // Mode switch CH5
const int PIN_FL = 9,  PIN_FR = 10;
const int PIN_RL = 11, PIN_RR = 12;

// ---------- Servo trim (degrees) — adjust if wheels are not straight ----------
const int TRIM_FL = 0, TRIM_FR = 0, TRIM_RL = 0, TRIM_RR = 0;

// ---------- Ackermann geometry (mm) ----------
const float TRACK_WIDTH     = 160.0;  // Left-right wheel spacing
const float WHEELBASE       = 200.0;  // Front-rear axle distance
const float MAX_OUTER_ANGLE = 30.0;   // Max outer wheel turn angle (deg)

// ---------- Pivot settle time ----------
// Servos take ~500ms to reach 45deg from center.
// Board 2 only receives HIGH after this delay to prevent
// motors spinning before wheels are fully positioned.
const unsigned long SERVO_SETTLE_MS = 500;
unsigned long pivotStartTime = 0;
bool          pivotReady     = false;
int           lastMode       = 0;

// ---------- Steering ISR (interrupt-driven PWM read) ----------
// Using interrupt avoids pulseIn blocking on the steering channel.
// unsigned long is 32-bit on AVR — atomic read, no corruption risk.
volatile unsigned long steerRiseTime = 0;
volatile unsigned long steerRaw      = 1500;

void steerISR() {
  if (digitalRead(PIN_RC_STEER) == HIGH)
    steerRiseTime = micros();
  else {
    unsigned long w = micros() - steerRiseTime;
    if (w > 800 && w < 2200) steerRaw = w;  // reject out-of-range noise
  }
}

// Atomic read helper for volatile unsigned long
unsigned long readVolatileUL(volatile unsigned long &v) {
  unsigned long val;
  noInterrupts();
  val = v;
  interrupts();
  return val;
}

// ---------- Mode switch (debounced pulseIn) ----------
// CH5 uses pulseIn (not interrupt) since mode changes are slow.
// Debounce requires CONFIRM_NEEDED consecutive identical reads
// before accepting a new mode — prevents noise-induced jumps.
int stableMode    = 2;
int candidateMode = 2;
int confirmCount  = 0;
const int CONFIRM_NEEDED = 5;

int readRawMode() {
  int pw = (int)pulseIn(PIN_RC_MODE, HIGH, 25000);
  if (pw == 0) pw = 1500;
  if (pw < 1200) return 1;
  if (pw < 1700) return 2;
  return 3;
}

int readMode() {
  int raw = readRawMode();
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

// ---------- PWM to steering angle ----------
float pwmToAngle(int pwm, float maxAngle) {
  pwm = constrain(pwm, 1000, 2000);
  return map(pwm, 1000, 2000, (int)(-maxAngle * 10), (int)(maxAngle * 10)) / 10.0;
}

// ---------- Ackermann steering calculation ----------
// Returns individual wheel angles so inner wheel turns sharper
// than outer wheel, tracing concentric arcs around the turn center.
// Rear wheels steer opposite to front (4-wheel steering).
WheelAngles calcAckermann(float outerAngleDeg) {
  WheelAngles wa = {0, 0, 0, 0};
  if (abs(outerAngleDeg) < 1.0) return wa;  // straight — no calculation needed

  float rad   = abs(outerAngleDeg) * PI / 180.0;
  float R     = WHEELBASE / tan(rad);        // turn radius from centerline
  float inner = atan(WHEELBASE / (R - TRACK_WIDTH / 2.0)) * 180.0 / PI;
  float outer = atan(WHEELBASE / (R + TRACK_WIDTH / 2.0)) * 180.0 / PI;

  if (outerAngleDeg > 0) {  // turning right
    wa.fr =  inner;  wa.fl =  outer;
    wa.rr = -inner;  wa.rl = -outer;
  } else {                  // turning left
    wa.fl = -inner;  wa.fr = -outer;
    wa.rl =  inner;  wa.rr =  outer;
  }
  return wa;
}

// ---------- Diamond / X-Mode wheel config ----------
// All four wheels turn 45deg inward, forming an X pattern.
// Combined with counter-rotating motors on Board 2,
// the rover spins on the spot (zero-radius turn).
WheelAngles calcDiamond() {
  WheelAngles wa = {+45.0, -45.0, -45.0, +45.0};
  return wa;
}

// ---------- Write angles to servos ----------
// FR and RR servos are physically mirrored — negate their angle.
// constrain() prevents servo damage from out-of-range commands.
void writeServos(WheelAngles wa) {
  servoFL.write(constrain(90 + (int)wa.fl + TRIM_FL, 0, 180));
  servoFR.write(constrain(90 - (int)wa.fr + TRIM_FR, 0, 180));
  servoRL.write(constrain(90 + (int)wa.rl + TRIM_RL, 0, 180));
  servoRR.write(constrain(90 - (int)wa.rr + TRIM_RR, 0, 180));
}

// ============================================================
void setup() {
  Serial.begin(9600);

  servoFL.attach(PIN_FL);
  servoFR.attach(PIN_FR);
  servoRL.attach(PIN_RL);
  servoRR.attach(PIN_RR);

  pinMode(PIN_RC_STEER, INPUT);
  pinMode(PIN_RC_MODE,  INPUT);
  pinMode(PIN_MODE_OUT, OUTPUT);

  // Interrupt on both edges for accurate PWM timing
  attachInterrupt(digitalPinToInterrupt(PIN_RC_STEER), steerISR, CHANGE);

  WheelAngles init_wa = {0, 0, 0, 0};
  writeServos(init_wa);
  digitalWrite(PIN_MODE_OUT, LOW);

  Serial.println("Board1 Steering Ready");
}

// ============================================================
void loop() {
  int mode        = readMode();
  int steerPWMcpy = (int)readVolatileUL(steerRaw);
  WheelAngles wa  = {0, 0, 0, 0};

  if (mode == 3) {
    wa = calcDiamond();

    // Start settle timer on first entry into mode 3
    if (lastMode != 3) {
      pivotStartTime = millis();
      pivotReady     = false;
    }

    // Hold Board 2 signal LOW until servos have settled
    if (!pivotReady && (millis() - pivotStartTime >= SERVO_SETTLE_MS)) {
      pivotReady = true;
    }
    digitalWrite(PIN_MODE_OUT, pivotReady ? HIGH : LOW);

  } else {
    float angle = pwmToAngle(steerPWMcpy, MAX_OUTER_ANGLE);
    wa = calcAckermann(angle);

    // Reset pivot state when leaving mode 3
    pivotReady = false;
    digitalWrite(PIN_MODE_OUT, LOW);
  }

  lastMode = mode;
  writeServos(wa);

  Serial.print("M:"); Serial.print(mode);
  Serial.print(" PW:"); Serial.print(steerPWMcpy);
  Serial.print(" FL:"); Serial.print(wa.fl, 1);
  Serial.print(" FR:"); Serial.print(wa.fr, 1);
  Serial.print(" RL:"); Serial.print(wa.rl, 1);
  Serial.print(" RR:"); Serial.println(wa.rr, 1);

  delay(20);  // 50 Hz loop
}

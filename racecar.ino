#include <BluetoothSerial.h>

// ===================== Config =====================
// Drive motors: 0 = OUT1/OUT2, 1 = OUT3/OUT4
const int EN[2]    = {25, 13};
const int INA[2]   = {26, 14};
const int INB[2]   = {27, 4};
const int ENC_A[2] = {32, 18};
const int ENC_B[2] = {33, 19};
int motorDir[2] = {1, 1};  // positive drive = car rolls forward
int encDir[2]   = {1, -1};  // forward = ticks count up

// Steering servo
const int SERVO_PIN    = 23;
const int SERVO_MIN    = 1000;
const int SERVO_CENTER = 1500;
const int SERVO_MAX    = 2000;
const int STEER_SIGN   = 1;  // flip to -1 if steer(1.0) turns right instead of left

// Safety
const unsigned long CMD_TIMEOUT_MS = 1000;  // stop driving if no command arrives for this long

// ===================== State =====================
volatile long ticks[2] = {0, 0};
int driveCmd = 0;
int servoUs = SERVO_CENTER;
bool testRunning = false;
bool aborted = false;
String pendingTest = "";
BluetoothSerial SerialBT;
String usbLine, btLine;    // one buffer per source so their bytes never mix
Stream* out = &Serial;     // replies go to whichever source sent the last command
unsigned long lastCmdMs = 0;

// ===================== Encoders =====================
void IRAM_ATTR onA0() { ticks[0] += digitalRead(ENC_B[0]) ? encDir[0] : -encDir[0]; }
void IRAM_ATTR onA1() { ticks[1] += digitalRead(ENC_B[1]) ? encDir[1] : -encDir[1]; }

long getTicks(int i) { return ticks[i]; }

void resetTicks() {
  noInterrupts();
  ticks[0] = 0;
  ticks[1] = 0;
  interrupts();
}

// ===================== Drive =====================
void setMotor(int i, int speed) {
  speed *= motorDir[i];
  digitalWrite(INA[i], speed > 0);
  digitalWrite(INB[i], speed < 0);
  ledcWrite(EN[i], min(abs(speed), 255));
}

void drive(int speed) {  // -255 to 255
  driveCmd = constrain(speed, -255, 255);
  setMotor(0, driveCmd);
  setMotor(1, driveCmd);
}

void stopDrive() { drive(0); }

// ===================== Steering =====================
void steerUs(int us) {
  servoUs = constrain(us, SERVO_MIN, SERVO_MAX);
  ledcWrite(SERVO_PIN, (uint32_t)servoUs * 16384 / 20000);  // 50 Hz, 14-bit
}

// s = -1.0 (full one way) to 1.0 (full the other way), 0 = straight
void steer(float s) {
  s = constrain(s * STEER_SIGN, -1.0f, 1.0f);
  int us;
  if (s >= 0) us = SERVO_CENTER + s * (SERVO_MAX - SERVO_CENTER);
  else        us = SERVO_CENTER + s * (SERVO_CENTER - SERVO_MIN);
  steerUs(us);
}

void center() { steer(0); }

// ===================== Telemetry =====================
void printTelemetry() {
  static unsigned long lastPrint = 0;
  static long last0 = 0, last1 = 0;
  if (millis() - lastPrint < 200) return;
  lastPrint = millis();
  long t0 = getTicks(0), t1 = getTicks(1);
  out->printf("drive %4d servo %4d | M0 ticks %7ld rate %4ld | M1 ticks %7ld rate %4ld\r\n",
              driveCmd, servoUs, t0, t0 - last0, t1, t1 - last1);
  last0 = t0;
  last1 = t1;
}

// ===================== Commands =====================
// d <speed>    drive, -255 to 255
// s <us>       raw servo pulse
// a <pct>      steer percent, -100 to 100
// t <name>     run a test
// r            reset ticks
// x            emergency stop, also aborts a running test
// Commands come from USB or Bluetooth. Driving stops if no line arrives for CMD_TIMEOUT_MS.
void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;
  char type = cmd[0];
  String arg = cmd.substring(1);
  arg.trim();

  switch (type) {
    case 'x':
      aborted = true;
      stopDrive();
      center();
      out->println(">> STOP");
      return;
    case 'd': if (!testRunning) drive(arg.toInt()); break;
    case 's': if (!testRunning) steerUs(arg.toInt()); break;
    case 'a': if (!testRunning) steer(arg.toInt() / 100.0f); break;
    case 'r': resetTicks(); break;
    case 't': if (!testRunning) pendingTest = arg; break;
    default:
      out->println(">> unknown command");
      return;
  }
  out->printf(">> drive %d  servo %d\r\n", driveCmd, servoUs);
}

void readLines(Stream& in, String& buf) {
  while (in.available()) {
    char c = in.read();
    if (c == '\n' || c == '\r') {
      lastCmdMs = millis();
      out = &in;
      handleCommand(buf);
      buf = "";
    } else {
      buf += c;
    }
  }
}

void handleSerial() {
  readLines(Serial, usbLine);
  readLines(SerialBT, btLine);
}

// Stops driving if commands stop arriving, e.g. Bluetooth dropped. Aborts a running test like 'x'.
void checkCmdTimeout() {
  if (driveCmd == 0 || millis() - lastCmdMs < CMD_TIMEOUT_MS) return;
  aborted = true;
  stopDrive();
  out->println(">> no command for 1 s, STOP");
}

// Holds the current drive and steer for ms milliseconds.
// Keeps telemetry and commands running. Returns false if aborted with 'x' or the command timeout.
bool runFor(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    handleSerial();
    checkCmdTimeout();
    printTelemetry();
    if (aborted) return false;
    delay(1);
  }
  return true;
}

// ===================== Tests =====================
// Pattern: set drive and steer, then "if (!runFor(ms)) return;"

const int CHECK_SPEED = 130;     // low, but high enough to overcome the L298N drop
const int CHECK_MIN_TICKS = 20;  // each wheel must move at least this much

// Drives at 'speed' while sweeping steering center, left, right, center (~1.1 s)
bool sweepWhileDriving(int speed) {
  const float pts[] = {0, 0.5, 1.0, 0.5, 0, -0.5, -1.0, -0.5, 0};
  drive(speed);
  for (float s : pts) {
    steer(s);
    if (!runFor(120)) return false;
  }
  return true;
}

// Short forward and backward drive with steering sweeps.
// Returns true only if both encoders counted the right way in both directions.
bool preflight() {
  out->println(">> preflight: forward + sweep");
  long f0[2] = {getTicks(0), getTicks(1)};
  if (!sweepWhileDriving(CHECK_SPEED)) return false;
  long f1[2] = {getTicks(0), getTicks(1)};

  stopDrive();
  center();
  if (!runFor(500)) return false;  // let it coast to a stop

  out->println(">> preflight: reverse + sweep");
  long r0[2] = {getTicks(0), getTicks(1)};
  if (!sweepWhileDriving(-CHECK_SPEED)) return false;
  long r1[2] = {getTicks(0), getTicks(1)};

  stopDrive();
  center();
  if (!runFor(500)) return false;

  bool allOk = true;
  for (int i = 0; i < 2; i++) {
    long fwd = f1[i] - f0[i];
    long rev = r1[i] - r0[i];
    bool ok = (fwd > CHECK_MIN_TICKS) && (rev < -CHECK_MIN_TICKS);
    out->printf(">> M%d  forward %+ld  reverse %+ld  %s\r\n",
                i, fwd, rev, ok ? "PASS" : "FAIL");
    if (!ok) allOk = false;
  }
  out->println(allOk ? ">> preflight PASS (check steering looked right)"
                     : ">> preflight FAIL, stopping");
  return allOk;
}

void testCheck() {
  preflight();
}

void testStraight() {
  if (!preflight()) return;
  center();
  drive(120);
  if (!runFor(3000)) return;
}

void testCircle() {
  if (!preflight()) return;
  steer(1.0);
  drive(120);
  if (!runFor(8000)) return;
}

void testFigureEight() {
  if (!preflight()) return;
  drive(120);
  steer(1.0);
  if (!runFor(6000)) return;
  steer(-1.0);
  if (!runFor(6000)) return;
}

void testSteerSweep() {  // steering only, no driving
  for (float s = -1.0; s <= 1.0; s += 0.25) {
    steer(s);
    if (!runFor(500)) return;
  }
}

void runTest(const String& name) {
  testRunning = true;
  aborted = false;
  out->printf(">> test %s start\r\n", name.c_str());

  if      (name == "check")    testCheck();
  else if (name == "straight") testStraight();
  else if (name == "circle")   testCircle();
  else if (name == "eight")    testFigureEight();
  else if (name == "sweep")    testSteerSweep();
  else out->println(">> unknown test");

  stopDrive();
  center();
  out->printf(">> test %s %s\r\n", name.c_str(), aborted ? "aborted" : "done");
  testRunning = false;
  aborted = false;
}

// ===================== Main =====================
void setup() {
  Serial.begin(115200);
  SerialBT.begin("rover");
  for (int i = 0; i < 2; i++) {
    pinMode(INA[i], OUTPUT);
    pinMode(INB[i], OUTPUT);
    ledcAttach(EN[i], 1000, 8);
    pinMode(ENC_A[i], INPUT_PULLUP);
    pinMode(ENC_B[i], INPUT_PULLUP);
  }
  attachInterrupt(digitalPinToInterrupt(ENC_A[0]), onA0, RISING);
  attachInterrupt(digitalPinToInterrupt(ENC_A[1]), onA1, RISING);

  ledcAttachChannel(SERVO_PIN, 50, 14, 4);
  center();
  stopDrive();

  Serial.println("Commands: d <speed>, s <us>, a <pct>, t <test>, r, x");
  Serial.println("Tests: check, straight, circle, eight, sweep");
}

void loop() {
  handleSerial();
  checkCmdTimeout();
  printTelemetry();
  if (pendingTest.length() > 0) {
    String t = pendingTest;
    pendingTest = "";
    runTest(t);
  }
}
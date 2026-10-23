// Motor 0 = OUT1/OUT2, Motor 1 = OUT3/OUT4
const int EN[2]    = {25, 13};
const int INA[2]   = {26, 14};
const int INB[2]   = {27, 4};
const int ENC_A[2] = {32, 18};
const int ENC_B[2] = {33, 19};

// Flip to -1 after testing (see notes)
int motorDir[2] = {1, 1};  // positive cmd = car rolls forward
int encDir[2]   = {1, 1};  // forward = ticks count up

volatile long ticks[2] = {0, 0};

void IRAM_ATTR onA0() { ticks[0] += digitalRead(ENC_B[0]) ? encDir[0] : -encDir[0]; }
void IRAM_ATTR onA1() { ticks[1] += digitalRead(ENC_B[1]) ? encDir[1] : -encDir[1]; }

void setMotor(int i, int speed) {  // -255 to 255
  speed *= motorDir[i];
  digitalWrite(INA[i], speed > 0);
  digitalWrite(INB[i], speed < 0);
  ledcWrite(EN[i], min(abs(speed), 255));
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 2; i++) {
    pinMode(INA[i], OUTPUT);
    pinMode(INB[i], OUTPUT);
    ledcAttach(EN[i], 1000, 8);
    pinMode(ENC_A[i], INPUT_PULLUP);
    pinMode(ENC_B[i], INPUT_PULLUP);
  }
  attachInterrupt(digitalPinToInterrupt(ENC_A[0]), onA0, RISING);
  attachInterrupt(digitalPinToInterrupt(ENC_A[1]), onA1, RISING);
}

void loop() {
  int cmds[] = {150, 0, -150, 0};
  int cmd = cmds[(millis() / 3000) % 4];  // forward, stop, reverse, stop
  setMotor(0, cmd);
  setMotor(1, cmd);

  static long last0 = 0, last1 = 0;
  long t0 = ticks[0], t1 = ticks[1];
  Serial.printf("cmd %4d | M0 ticks %7ld rate %4ld | M1 ticks %7ld rate %4ld\r\n",
                cmd, t0, t0 - last0, t1, t1 - last1);
  last0 = t0;
  last1 = t1;
  delay(100);
}
// Arduino Nano - TWO 4-pin PWM Fan Controller with RPM feedback
// PWM: 0–255 range
// Fan 1: PWM pin 9  (OC1A), Tach pin 2
// Fan 2: PWM pin 10 (OC1B), Tach pin 3
// Both @ ~25 kHz using Timer1

#define PWM_PIN_1     9
#define TACH_PIN_1    2
#define PWM_PIN_2    10
#define TACH_PIN_2    3

#define PWM_FREQ_HZ   25000

// ── Period measurement variables ───────────────────────────────
volatile unsigned long last_tach_time1 = 0;
volatile unsigned long last_tach_time2 = 0;

volatile unsigned long period1 = 0;   // microseconds between pulses
volatile unsigned long period2 = 0;

unsigned int rpm1 = 0;
unsigned int rpm2 = 0;

uint8_t pwm_value1 = 0;
uint8_t pwm_value2 = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  pinMode(PWM_PIN_1, OUTPUT);
  pinMode(PWM_PIN_2, OUTPUT);
  pinMode(TACH_PIN_1, INPUT_PULLUP);
  pinMode(TACH_PIN_2, INPUT_PULLUP);

  // ── Setup Timer1 Fast PWM ~25 kHz ───────────────────────────────
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM11);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);

  ICR1 = F_CPU / PWM_FREQ_HZ - 1;
  OCR1A = 0;
  OCR1B = 0;

  // Tach interrupts
  attachInterrupt(digitalPinToInterrupt(TACH_PIN_1), tach_isr_1, FALLING);
  attachInterrupt(digitalPinToInterrupt(TACH_PIN_2), tach_isr_2, FALLING);

  Serial.println("Dual Fan Controller ready (period-based RPM mode)");

  // Safe start
  setPwmRaw(1, 180);
  setPwmRaw(2, 180);
}

void loop() {
  // Compute RPM from period
  noInterrupts();
  unsigned long p1 = period1;
  unsigned long p2 = period2;
  interrupts();

  if (p1 > 0) rpm1 = filterRPM(p1);
  else rpm1 = 0;

  if (p2 > 0) rpm2 = filterRPM(p2);
  else rpm2 = 0;

  // Handle serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd.startsWith("p1")) {
      int val = cmd.substring(2).toInt();
      if (val >= 0 && val <= 255) {
        if (val > 240) val = 240;  // this is upper limit that is usually needed for fans that behave veird at constant voltages on pwm pin (feedback tach signal gets messed up)
        if (val < 70) val = 80;  // this is usually lower limit for laptop fans, so they still report good pwm signals, without external pullup resistor
        setPwmRaw(1, val);
      }
    }
    else if (cmd.startsWith("p2")) {
      int val = cmd.substring(2).toInt();
      if (val >= 0 && val <= 255) {
        if (val > 240) val = 240;
        if (val < 70) val = 80;
        setPwmRaw(2, val);
      }
    }
    else if (cmd == "rpm1") {
      Serial.print("Fan 1 RPM: "); Serial.println(rpm1);
    }
    else if (cmd == "rpm2") {
      Serial.print("Fan 2 RPM: "); Serial.println(rpm2);
    }
    else if (cmd == "status1") {
      printFanStatus(1);
    }
    else if (cmd == "status2") {
      printFanStatus(2);
    }
    else if (cmd == "status") {
      printFanStatus(1);
      printFanStatus(2);
    }
    else {
      Serial.println("Unknown command");
    }
  }
}

// ── Tachometer ISRs (period measurement) ─────────────────────────
void tach_isr_1() {
  unsigned long now = micros();
  unsigned long dt = now - last_tach_time1;

  if (dt > 500) {     // debounce, migh need to lower for laptop fans to like 500 for good desktop you can use like 5000 even 
    period1 = dt;
    last_tach_time1 = now;
  }
}

void tach_isr_2() {
  unsigned long now = micros();
  unsigned long dt = now - last_tach_time2;

  if (dt > 500) {
    period2 = dt;
    last_tach_time2 = now;
  }
}

// ── Set PWM for fan 1 or 2 ───────────────────────────────────────
void setPwmRaw(uint8_t fan, uint8_t value) {
  value = constrain(value, 0, 255);
  uint16_t duty = (uint32_t)value * (ICR1 + 1) / 255;

  if (fan == 1) {
    pwm_value1 = value;
    OCR1A = duty;
  } else {
    pwm_value2 = value;
    OCR1B = duty;
  }
}

// ── Print status ────────────────────────────────────────────────
void printFanStatus(uint8_t fan) {
  if (fan == 1) {
    Serial.print("Fan 1 - PWM: ");
    Serial.print(pwm_value1);
    Serial.print("/255   RPM: ");
    Serial.println(rpm1);
  } else {
    Serial.print("Fan 2 - PWM: ");
    Serial.print(pwm_value2);
    Serial.print("/255   RPM: ");
    Serial.println(rpm2);
  }
}

// ───────────────────────────────────────────────
// Nuvoton‑style RPM filtering module (no stall detect)
// ───────────────────────────────────────────────

// Minimum valid tach period (µs)
// Reject pulses faster than 6000 µs (~5000 RPM)
#define MIN_VALID_PERIOD 6000

// Median filter buffer
unsigned long period_buf[3] = {0, 0, 0};

// Moving average buffer (4 samples)
unsigned long avg_buf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint8_t avg_index = 0;

// Exponential smoothing factor (0.0–1.0)
float rpm_smooth = 0;
const float SMOOTH_ALPHA = 0.10;   // 10% new, 75% old  // 0.10 -desktop fan use, 0.07 laptop fan use

// Insert new period into median filter
unsigned long median3(unsigned long a, unsigned long b, unsigned long c) {
    if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
    if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
    return c;
}

// Main filter function — call this with your raw period
unsigned int filterRPM(unsigned long raw_period) {

    // ───────────────────────────────────────────────────────
    // 0) Glitch rejector (reject sudden jumps >20%) (!!)comment for desktop use ONLY FOR LAPTOP FANS
    // ───────────────────────────────────────────────────────
    static unsigned long last_period = 0;
    static uint8_t glitch_count = 0;
    
    unsigned long candidate = raw_period;
    
    if (last_period > 0) {
        long diff = (long)candidate - (long)last_period;
    
        // If jump >20%, treat as possible glitch
        if (abs(diff) > (last_period / 5)) {
            glitch_count++;
    
            // If too many in a row, accept new value
            if (glitch_count >= 5) {      // set to 3 for desktop fans and to 5 for laptop fans
                last_period = candidate;   // accept new speed
                glitch_count = 0;
            } else {
                candidate = last_period;   // reject this pulse
            }
        } else {
            // Normal pulse, accept immediately
            last_period = candidate;
            glitch_count = 0;
        }
    } else {
        // First sample
        last_period = candidate;
    }
    
    raw_period = candidate;
    // ───────────────────────────────────────────────────────
        
    // 1) Reject impossible pulses (too fast)
    if (raw_period < MIN_VALID_PERIOD) {
        raw_period = MIN_VALID_PERIOD;
    }

    // 2) Median filter (3 samples)
    period_buf[0] = period_buf[1];
    period_buf[1] = period_buf[2];
    period_buf[2] = raw_period;

    unsigned long med = median3(period_buf[0], period_buf[1], period_buf[2]);

    // 3) 4‑sample moving average
    avg_buf[avg_index] = med;
    avg_index = (avg_index + 1) % 8;

    unsigned long sum = avg_buf[0] + avg_buf[1] + avg_buf[2] + avg_buf[3] + avg_buf[4] + avg_buf[5] + avg_buf[6] + avg_buf[7];
    unsigned long avg_period = sum / 8;

    // 4) Convert to RPM
    unsigned int rpm = 60000000UL / (avg_period * 2);

    // 5) Exponential smoothing
    rpm_smooth = rpm_smooth * (1.0 - SMOOTH_ALPHA) + rpm * SMOOTH_ALPHA;

    static unsigned int last_rpm = 0;

    if (abs((int)rpm_smooth - (int)last_rpm) < 5) {
        rpm_smooth = last_rpm;
    }
    
    last_rpm = rpm_smooth;

    return (unsigned int)rpm_smooth;
}

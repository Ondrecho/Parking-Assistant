#include <Arduino.h>

#define TRIG_PIN  45
#define ECHO_PIN  48

volatile unsigned long t_rise = 0;
volatile unsigned long t_fall = 0;
volatile bool have_rise = false;
volatile bool have_pulse = false;

void IRAM_ATTR echoChange() {
  // В ISR читаем уровень и запоминаем время
  int level = digitalRead(ECHO_PIN);
  unsigned long now = micros();
  if (level == HIGH) {
    t_rise = now;
    have_rise = true;
  } else {
    // FALL
    // Только если был предыдущий RISE — считаем валидный импульс
    if (have_rise) {
      t_fall = now;
      have_pulse = true;
      have_rise = false; // сбрасываем маркер
    }
  }
}

void sendTrigger() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoChange, CHANGE);

  Serial.println("Using single CHANGE ISR — ready");
}

void loop() {
  // посылаем триггер
  sendTrigger();

  unsigned long start = millis();
  while (!have_pulse && (millis() - start) < 60) {
    delay(1);
  }

  if (have_pulse) {
    noInterrupts();
    unsigned long tr = t_rise;
    unsigned long tf = t_fall;
    have_pulse = false;
    interrupts();

    unsigned long dur = (tf > tr) ? (tf - tr) : 0;
    float dist = dur / 58.0;
    Serial.printf("Echo: %lu us  =>  %.2f cm\n", dur, dist);
  } else {
    Serial.println("timeout (no echo)");
  }

  delay(200);
}

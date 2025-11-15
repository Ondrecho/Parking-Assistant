#include "sensors_task.h"
#include <Arduino.h>
#include "config.h"
#include "state.h"

volatile unsigned long t_rise[NUM_SENSORS] = {0};
volatile unsigned long t_fall[NUM_SENSORS] = {0};
volatile bool have_rise[NUM_SENSORS] = {false};
volatile bool have_pulse[NUM_SENSORS] = {false};

const int SMOOTH_LEN = 6;
float smoothBuf[NUM_SENSORS][SMOOTH_LEN];
int smoothIdx[NUM_SENSORS] = {0};
bool initialFilled[NUM_SENSORS] = {false};

void IRAM_ATTR echoChange0() {
  if (digitalRead(SENSOR_PINS[0].echo) == HIGH) {
    t_rise[0] = micros();
    have_rise[0] = true;
  } else if (have_rise[0]) {
    t_fall[0] = micros();
    have_pulse[0] = true;
    have_rise[0] = false;
  }
}
void IRAM_ATTR echoChange1() {
  if (digitalRead(SENSOR_PINS[1].echo) == HIGH) {
    t_rise[1] = micros();
    have_rise[1] = true;
  } else if (have_rise[1]) {
    t_fall[1] = micros();
    have_pulse[1] = true;
    have_rise[1] = false;
  }
}
void IRAM_ATTR echoChange2() {
  if (digitalRead(SENSOR_PINS[2].echo) == HIGH) {
    t_rise[2] = micros();
    have_rise[2] = true;
  } else if (have_rise[2]) {
    t_fall[2] = micros();
    have_pulse[2] = true;
    have_rise[2] = false;
  }
}

void sendTriggerPin(uint8_t trigPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
}

float microsToCm(unsigned long us) {
  return (float)us / 58.0f;
}

float getSmoothedValue(int idx, float newValue) {
    if (!initialFilled[idx]) {
        for (int i = 0; i < SMOOTH_LEN; ++i) {
            smoothBuf[idx][i] = newValue;
        }
        initialFilled[idx] = true;
    }

    smoothBuf[idx][smoothIdx[idx]] = newValue;
    smoothIdx[idx] = (smoothIdx[idx] + 1) % SMOOTH_LEN;

    float sum = 0;
    for (int i = 0; i < SMOOTH_LEN; ++i) {
        sum += smoothBuf[idx][i];
    }
    return sum / (float)SMOOTH_LEN;
}


void sensors_task(void *pvParameters) {
  (void)pvParameters;

  for (int i = 0; i < NUM_SENSORS; ++i) {
    pinMode(SENSOR_PINS[i].trig, OUTPUT);
    digitalWrite(SENSOR_PINS[i].trig, LOW);
    pinMode(SENSOR_PINS[i].echo, INPUT);
  }
  attachInterrupt(digitalPinToInterrupt(SENSOR_PINS[0].echo), echoChange0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PINS[1].echo), echoChange1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PINS[2].echo), echoChange2, CHANGE);
  
  Serial.println("Sensors task started");

  for (;;) {
    xEventGroupWaitBits(xAppEventGroup, PARKTRONIC_ACTIVE_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    while (xEventGroupGetBits(xAppEventGroup) & PARKTRONIC_ACTIVE_BIT) {
      float measuredDistances[NUM_SENSORS];

      for (int i = 0; i < NUM_SENSORS; ++i) {
        have_pulse[i] = false;
        have_rise[i] = false;

        sendTriggerPin(SENSOR_PINS[i].trig);

        unsigned long startMs = millis();
        bool pulse_received = false;
        while (millis() - startMs < 50) { 
          if (have_pulse[i]) {
            pulse_received = true;
            break;
          }
          vTaskDelay(pdMS_TO_TICKS(1));
        }

        if (pulse_received) {
          unsigned long duration = (t_fall[i] > t_rise[i]) ? (t_fall[i] - t_rise[i]) : 0;
          float dist_cm = microsToCm(duration);
          
          if (dist_cm > 400.0) dist_cm = 400.0; 
          
          measuredDistances[i] = getSmoothedValue(i, dist_cm);

        } else {
          measuredDistances[i] = getSmoothedValue(i, 400.0); 
        }
        
        vTaskDelay(pdMS_TO_TICKS(30));
      }

      if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
        for(int i = 0; i < NUM_SENSORS; ++i) {
          g_app_state.sensor_distances[i] = measuredDistances[i];
        }
        xSemaphoreGive(xStateMutex);
      }

      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}
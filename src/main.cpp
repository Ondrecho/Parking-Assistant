/*
  Multi-ultrasonic (ESP32-S3)
  - три датчика: left (TRIG=42,ECHO=41), center (TRIG=47,ECHO=21), right (TRIG=20,ECHO=19)
  - измерение через ISR (CHANGE), сглаживание, FreeRTOS tasks
  - ESP in AP mode + simple web server returning JSON /data
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ----------------------------- Конфигурация -----------------------------
struct SensorCfg {
  uint8_t trig;
  uint8_t echo;
};

SensorCfg cfgs[3] = {
  {42, 41}, // left
  {45, 48}, // center
  {47, 21}  // right
};

const int NUM_SENS = 3;
const int SMOOTH_LEN = 6;         // длина буфера сглаживания
const float MAX_DELTA_CM = 15.0;  // максимальное изменение за одно обновление
const int SENSOR_TIMEOUT_MS = 60; // таймаут ожидания эхо в ms

// WiFi AP
const char *AP_SSID = "ESP32_Park_AP";
const char *AP_PASS = "park12345";

// WebServer
WebServer server(80);

// ----------------------------- Данные сенсоров -----------------------------
volatile unsigned long t_rise[NUM_SENS] = {0};
volatile unsigned long t_fall[NUM_SENS] = {0};
volatile bool have_rise[NUM_SENS] = {false};
volatile bool have_pulse[NUM_SENS] = {false};

// сглаживание и буферы
float smoothBuf[NUM_SENS][SMOOTH_LEN];
int smoothIdx[NUM_SENS] = {0};
float smoothedVal[NUM_SENS] = {0.0};
bool initialFilled[NUM_SENS] = {false};

// синхронизация (ESP32)
portMUX_TYPE sensorMux = portMUX_INITIALIZER_UNLOCKED;

// ----------------------------- ISR для каждого датчика -----------------------------
void IRAM_ATTR echoChange0() {
  int level = digitalRead(cfgs[0].echo);
  unsigned long now = micros();
  if (level == HIGH) {
    t_rise[0] = now;
    have_rise[0] = true;
  } else {
    if (have_rise[0]) {
      t_fall[0] = now;
      have_pulse[0] = true;
      have_rise[0] = false;
    }
  }
}
void IRAM_ATTR echoChange1() {
  int level = digitalRead(cfgs[1].echo);
  unsigned long now = micros();
  if (level == HIGH) {
    t_rise[1] = now;
    have_rise[1] = true;
  } else {
    if (have_rise[1]) {
      t_fall[1] = now;
      have_pulse[1] = true;
      have_rise[1] = false;
    }
  }
}
void IRAM_ATTR echoChange2() {
  int level = digitalRead(cfgs[2].echo);
  unsigned long now = micros();
  if (level == HIGH) {
    t_rise[2] = now;
    have_rise[2] = true;
  } else {
    if (have_rise[2]) {
      t_fall[2] = now;
      have_pulse[2] = true;
      have_rise[2] = false;
    }
  }
}

// ----------------------------- Утилиты -----------------------------
void sendTriggerPin(uint8_t trigPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10); // 10 µs trigger
  digitalWrite(trigPin, LOW);
}

float microsToCm(unsigned long us) {
  // speed of sound approx 343 m/s -> round-trip: us / 58 = cm (approx)
  return (float)us / 58.0f;
}

float smoothAndClamp(int idx, float newVal) {
  // ограничиваем резкие скачки относительно последнего smoothedVal
  portENTER_CRITICAL(&sensorMux);
  float prev = smoothedVal[idx];
  portEXIT_CRITICAL(&sensorMux);

  if (!initialFilled[idx]) {
    // заполняем буфер начальными значениями
    for (int i = 0; i < SMOOTH_LEN; ++i) smoothBuf[idx][i] = newVal;
    smoothIdx[idx] = 0;
    initialFilled[idx] = true;
    float avg = newVal;
    portENTER_CRITICAL(&sensorMux);
    smoothedVal[idx] = avg;
    portEXIT_CRITICAL(&sensorMux);
    return avg;
  }

  float delta = newVal - prev;
  if (fabs(delta) > MAX_DELTA_CM) {
    // ограничим
    if (delta > 0) newVal = prev + MAX_DELTA_CM;
    else newVal = prev - MAX_DELTA_CM;
  }

  smoothBuf[idx][smoothIdx[idx]] = newVal;
  smoothIdx[idx] = (smoothIdx[idx] + 1) % SMOOTH_LEN;

  float sum = 0;
  for (int i = 0; i < SMOOTH_LEN; ++i) sum += smoothBuf[idx][i];
  float avg = sum / (float)SMOOTH_LEN;

  portENTER_CRITICAL(&sensorMux);
  smoothedVal[idx] = avg;
  portEXIT_CRITICAL(&sensorMux);

  return avg;
}

// ----------------------------- Задачи FreeRTOS -----------------------------
void readSensorsTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    for (int i = 0; i < NUM_SENS; ++i) {
      // сброс флагов до триггера
      portENTER_CRITICAL(&sensorMux);
      have_pulse[i] = false;
      have_rise[i] = false;
      portEXIT_CRITICAL(&sensorMux);

      // триггерим
      sendTriggerPin(cfgs[i].trig);

      // ждём измерение (с таймаутом) — адаптивно опрашиваем
      unsigned long startMs = millis();
      bool ok = false;
      while ((millis() - startMs) < SENSOR_TIMEOUT_MS) {
        if (have_pulse[i]) { ok = true; break; }
        // даём шанс другим таскам
        vTaskDelay(pdMS_TO_TICKS(1));
      }

      if (ok) {
        // захватываем времена
        unsigned long tr, tf;
        portENTER_CRITICAL(&sensorMux);
        tr = t_rise[i];
        tf = t_fall[i];
        // clear flag (готово)
        have_pulse[i] = false;
        portEXIT_CRITICAL(&sensorMux);

        unsigned long dur = (tf > tr) ? (tf - tr) : 0;
        float dist = microsToCm(dur);
        float avg = smoothAndClamp(i, dist);

        // можно вести лог в Serial
        // Serial.printf("S%d dur=%lu us raw=%.2f cm smooth=%.2f cm\n", i, dur, dist, avg);
      } else {
        // timeout: можно отметить как очень большая дистанция или -1
        // не затираем smoothedVal, просто логим при необходимости
        // Serial.printf("S%d TIMEOUT\n", i);
      }

      // невеликая пауза между триггерами, чтобы датчики не мешали друг другу
      vTaskDelay(pdMS_TO_TICKS(30));
    }

    // Пауза между полными кругами измерений
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}

void webServerTask(void *pvParameters) {
  (void) pvParameters;

  // handlers
  server.on("/", HTTP_GET, []() {
    // простой HTML/JS, который запрашивает /data
    const char* page =
      "<!doctype html><html><head><meta charset='utf-8'><title>Parktronic</title>"
      "<style>body{font-family:Arial,Helvetica,sans-serif;padding:10px;} .s{margin:6px 0;padding:8px;border:1px solid #ddd;border-radius:6px;width:260px}</style>"
      "</head><body>"
      "<h2>ESP32 Parktronic</h2>"
      "<div id='left' class='s'>Left: -- cm</div>"
      "<div id='center' class='s'>Center: -- cm</div>"
      "<div id='right' class='s'>Right: -- cm</div>"
      "<script>"
      "async function fetchData(){try{let r=await fetch('/data');let j=await r.json();document.getElementById('left').textContent='Left: '+j.left.toFixed(1)+' cm';document.getElementById('center').textContent='Center: '+j.center.toFixed(1)+' cm';document.getElementById('right').textContent='Right: '+j.right.toFixed(1)+' cm';}catch(e){console.log(e);} }"
      "setInterval(fetchData,200);fetchData();"
      "</script></body></html>";
    server.send(200, "text/html", page);
  });

  server.on("/data", HTTP_GET, []() {
    float l, c, r;
    portENTER_CRITICAL(&sensorMux);
    l = smoothedVal[0];
    c = smoothedVal[1];
    r = smoothedVal[2];
    portEXIT_CRITICAL(&sensorMux);

    // формируем простую JSON строку
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"left\":%.2f,\"center\":%.2f,\"right\":%.2f}", l, c, r);
    server.send(200, "application/json", buf);
  });

  server.begin();
  Serial.println("HTTP server started");

  for (;;) {
    // WebServer в loop нужно вызывать в задаче
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void displayTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    // Периодически печатаем в Serial (индикация / lcd / leds можно выводить тут)
    portENTER_CRITICAL(&sensorMux);
    float l = smoothedVal[0];
    float c = smoothedVal[1];
    float r = smoothedVal[2];
    portEXIT_CRITICAL(&sensorMux);

    Serial.printf("LEFT: %.2f cm | CENTER: %.2f cm | RIGHT: %.2f cm\n", l, c, r);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// ----------------------------- setup / loop -----------------------------
void setupPinsAndISRs() {
  for (int i = 0; i < NUM_SENS; ++i) {
    pinMode(cfgs[i].trig, OUTPUT);
    digitalWrite(cfgs[i].trig, LOW);
    pinMode(cfgs[i].echo, INPUT);
  }

  // attach interrupts (по одному handler на каждый датчик)
  attachInterrupt(digitalPinToInterrupt(cfgs[0].echo), echoChange0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(cfgs[1].echo), echoChange1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(cfgs[2].echo), echoChange2, CHANGE);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // заполнение начальных значений нулями
  for (int i = 0; i < NUM_SENS; ++i) {
    for (int j = 0; j < SMOOTH_LEN; ++j) smoothBuf[i][j] = 0.0;
    smoothedVal[i] = 0.0;
    initialFilled[i] = false;
  }

  setupPinsAndISRs();

  // start WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("AP started: %s (IP %s)\n", AP_SSID, ip.toString().c_str());

  // создаём задачи
  xTaskCreatePinnedToCore(readSensorsTask, "ReadSens", 4096, NULL, 5, NULL, 1);
  xTaskCreatePinnedToCore(webServerTask, "WebSrv", 8192, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(displayTask, "Disp", 4096, NULL, 1, NULL, 1);

  Serial.println("Setup complete");
}

void loop() {
  // пусто — всё в тасках
  delay(1000);
}

#pragma once
#include <Arduino.h>

// --- WiFi ---
#define WIFI_AP_SSID "ESP32_Park_AP"
#define WIFI_AP_PASS "12345678"

// --- Пин-ауты датчиков ---
#define NUM_SENSORS 3
struct SensorConfig {
  uint8_t trig;
  uint8_t echo;
};
const SensorConfig SENSOR_PINS[NUM_SENSORS] = {
  {42, 41}, // Left
  {45, 48}, // Center
  {47, 21}  // Right
};

// --- Пин-ауты камеры (OV5640) ---
#define CAM_PIN_PWDN     -1
#define CAM_PIN_RESET    -1
#define CAM_PIN_XCLK     15
#define CAM_PIN_SIOD      4
#define CAM_PIN_SIOC      5
#define CAM_PIN_D7       16 // Y9
#define CAM_PIN_D6       17 // Y8
#define CAM_PIN_D5       18 // Y7
#define CAM_PIN_D4       12 // Y6
#define CAM_PIN_D3       10 // Y5
#define CAM_PIN_D2        8 // Y4
#define CAM_PIN_D1        9 // Y3
#define CAM_PIN_D0       11 // Y2
#define CAM_PIN_VSYNC     6
#define CAM_PIN_HREF      7
#define CAM_PIN_PCLK     13

// --- Структура настроек (соответствует клиенту) ---
struct AppSettings {
    // Parktronic
    int thresh_yellow;
    int thresh_orange;
    int thresh_red;
    int bpm_min;
    int bpm_max;
    bool auto_start;
    // Camera
    bool show_grid;
    int cam_angle;
    float grid_opacity;
    int grid_offset_x;
    int grid_offset_y;
    int grid_offset_z;
    char resolution[16];
    int jpeg_quality;
    bool flip_h;
    bool flip_v;
    // System
    bool is_muted;
    bool stream_active;
    // WiFi
    char wifi_ssid[32];
    char wifi_pass[64];
};
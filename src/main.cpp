#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include <LittleFS.h>

#include "config.h"
#include "state.h"
#include "tasks/camera_task.h"
#include "tasks/sensors_task.h"
#include "web/web_server.h"

// --- Глобальные определения ---
AppState g_app_state;
SemaphoreHandle_t xStateMutex = NULL;
EventGroupHandle_t xAppEventGroup = NULL;

void load_default_settings() {
    // Загрузка настроек по умолчанию при первом старте
    // (Позже здесь будет загрузка из файла)
    g_app_state.settings = {
        .thresh_yellow = 2.0, .thresh_orange = 1.0, .thresh_red = 0.5,
        .bpm_min = 0, .bpm_max = 300, .auto_start = true,
        .show_grid = true, .cam_angle = 45, .grid_opacity = 0.8,
        .grid_offset_x = 0, .grid_offset_y = 0, .grid_offset_z = 0,
        .resolution = (int)FRAMESIZE_VGA, .jpeg_quality = 12, .flip_h = true, .flip_v = false,
        .is_muted = false, .stream_active = true
    };
    strncpy(g_app_state.settings.wifi_ssid, WIFI_AP_SSID, sizeof(g_app_state.settings.wifi_ssid));
    strncpy(g_app_state.settings.wifi_pass, WIFI_AP_PASS, sizeof(g_app_state.settings.wifi_pass));

    for (int i = 0; i < NUM_SENSORS; ++i) {
        g_app_state.sensor_distances[i] = 999.0; // Начальное значение "бесконечность"
    }
    g_app_state.is_camera_initialized = false;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Booting up...");

    // 1. Инициализация синхронизации
    xStateMutex = xSemaphoreCreateMutex();
    xAppEventGroup = xEventGroupCreate();
    if (!xStateMutex || !xAppEventGroup) {
        Serial.println("Failed to create sync objects!");
        return;
    }

    // 2. Загрузка настроек
    load_default_settings();

    // 3. Инициализация файловой системы
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed!");
        return;
    }

    // 4. Настройка WiFi
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    // 5. Запуск веб-сервера
    init_web_server();

    // 6. Создание задач
    xTaskCreatePinnedToCore(sensors_task, "SensorsTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(camera_task, "CameraTask", 4096, NULL, 5, NULL, 0);
    
    Serial.println("Setup complete. Tasks are running.");
}

void loop() {
    // Пусто. Все работает в задачах.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
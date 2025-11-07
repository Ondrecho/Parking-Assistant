#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "config.h"
#include "state.h"
#include "tasks/camera_task.h"
#include "tasks/sensors_task.h"
#include "tasks/stream_task.h"
#include "web/web_server.h"
#include "web/websocket_manager.h"
#include "settings_manager.h"
#include "tasks/frame_grab_task.h" // <--- ДОБАВЛЕНО

// Глобальные определения
AppState g_app_state;
SemaphoreHandle_t xStateMutex = NULL;
EventGroupHandle_t xAppEventGroup = NULL;
SemaphoreHandle_t xCameraMutex = NULL;
QueueHandle_t xFrameQueue = NULL;      // <--- ДОБАВЛЕНО

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Booting up...");

    xStateMutex = xSemaphoreCreateMutex();
    xAppEventGroup = xEventGroupCreate();
    xCameraMutex = xSemaphoreCreateMutex();
    // Создаем очередь для 1 указателя на буфер кадра
    xFrameQueue = xQueueCreate(1, sizeof(camera_fb_t *)); // <--- ДОБАВЛЕНО

    if (!xStateMutex || !xAppEventGroup || !xCameraMutex || !xFrameQueue) { // <--- ДОБАВЛЕНО
        Serial.println("Failed to create sync objects!");
        return;
    }

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed!");
        return;
    }

    settings_init();

    for (int i = 0; i < NUM_SENSORS; ++i) {
        g_app_state.sensor_distances[i] = 999.0;
    }
    g_app_state.is_camera_initialized = false;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(g_app_state.settings.wifi_ssid, g_app_state.settings.wifi_pass);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    init_web_server();

    xTaskCreatePinnedToCore(sensors_task, "SensorsTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(camera_task, "CameraTask", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(stream_task, "StreamTask", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(broadcast_sensors_task, "WsBroadcastTask", 4096, NULL, 3, NULL, 1);
    // Запускаем новую задачу на ядре 0
    xTaskCreatePinnedToCore(frame_grab_task, "FrameGrabTask", 4096, NULL, 4, NULL, 0); // <--- ДОБАВЛЕНО
    
    Serial.println("Setup complete. Tasks are running.");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
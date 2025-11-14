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

AppState g_app_state;
SemaphoreHandle_t xStateMutex = NULL;
EventGroupHandle_t xAppEventGroup = NULL;

SemaphoreHandle_t xCameraMutex = NULL;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Booting up...");

    xStateMutex = xSemaphoreCreateMutex();
    xAppEventGroup = xEventGroupCreate();
     xCameraMutex = xSemaphoreCreateMutex();
    if (!xStateMutex || !xAppEventGroup  || !xCameraMutex) {
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

    BaseType_t task_creation_result;

    task_creation_result = xTaskCreatePinnedToCore(sensors_task, "SensorsTask", 4096, NULL, 5, NULL, 1);
    if (task_creation_result != pdPASS) {
        Serial.println("CRITICAL: Failed to create SensorsTask!");
        while(1) vTaskDelay(1000); 
    }

    task_creation_result = xTaskCreatePinnedToCore(camera_task, "CameraTask", 4096, NULL, 5, NULL, 0);
    if (task_creation_result != pdPASS) {
        Serial.println("CRITICAL: Failed to create CameraTask!");
        while(1) vTaskDelay(1000); 
    }

    task_creation_result = xTaskCreatePinnedToCore(stream_task, "StreamTask", 4096, NULL, 4, NULL, 1);
    if (task_creation_result != pdPASS) {
        Serial.println("CRITICAL: Failed to create StreamTask!");
        while(1) vTaskDelay(1000); 
    }
    
    task_creation_result = xTaskCreatePinnedToCore(broadcast_sensors_task, "WsSensorsTask", 4096, NULL, 3, NULL, 1);
    if (task_creation_result != pdPASS) {
        Serial.println("CRITICAL: Failed to create WsSensorsTask!");
        while(1) vTaskDelay(1000); 
    }

    Serial.println("Setup complete. All tasks are running.");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
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
#include "tasks/parktronic_manager_task.h"
#include "tasks/buzzer_task.h"

AppState g_app_state;
SemaphoreHandle_t xStateMutex = NULL;
EventGroupHandle_t xAppEventGroup = NULL;
SemaphoreHandle_t xCameraMutex = NULL;

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("Booting up...");

    xStateMutex = xSemaphoreCreateMutex();
    xAppEventGroup = xEventGroupCreate();
    xCameraMutex = xSemaphoreCreateMutex();
    if (!xStateMutex || !xAppEventGroup || !xCameraMutex)
    {
        Serial.println("CRITICAL: Failed to create sync objects!");
        while (1) vTaskDelay(1000);
    }

    if (!LittleFS.begin(true))
    {
        Serial.println("CRITICAL: LittleFS mount failed!");
        while (1) vTaskDelay(1000);
    }

    settings_init();

    for (int i = 0; i < NUM_SENSORS; ++i)
    {
        g_app_state.sensor_distances[i] = 999.0;
    }
    g_app_state.is_camera_initialized = false;
    g_app_state.is_parktronic_active = false;
    g_app_state.is_manually_activated = false;
    g_app_state.is_muted = false;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(g_app_state.settings.wifi_ssid, g_app_state.settings.wifi_pass);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    init_web_server();

    BaseType_t task_creation_result;

    // --- Ядро 0 ---
    task_creation_result = xTaskCreatePinnedToCore(
        camera_task, "CameraTask", 4096, NULL, 4, NULL, 0);
    if (task_creation_result != pdPASS)
    {
        Serial.println("CRITICAL: Failed to create CameraTask!");
        while (1) vTaskDelay(1000);
    }

    // --- Ядро 1 ---
    task_creation_result = xTaskCreatePinnedToCore(
        sensors_task, "SensorsTask", 2048, NULL, 5, NULL, 1);
    if (task_creation_result != pdPASS)
    {
        Serial.println("CRITICAL: Failed to create SensorsTask!");
        while (1) vTaskDelay(1000);
    }

    task_creation_result = xTaskCreatePinnedToCore(
        stream_task, "StreamTask", 4096, NULL, 4, NULL, 1);
    if (task_creation_result != pdPASS)
    {
        Serial.println("CRITICAL: Failed to create StreamTask!");
        while (1) vTaskDelay(1000);
    }
    
    task_creation_result = xTaskCreatePinnedToCore(
        buzzer_task, "BuzzerTask", 2048, NULL, 4, NULL, 1);
    if (task_creation_result != pdPASS)
    {
        Serial.println("CRITICAL: Failed to create BuzzerTask!");
        while (1) vTaskDelay(1000);
    }

    task_creation_result = xTaskCreatePinnedToCore(
        parktronic_manager_task, "ParktronicManager", 2048, NULL, 2, NULL, 1);
    if (task_creation_result != pdPASS)
    {
        Serial.println("CRITICAL: Failed to create ParktronicManager!");
        while (1) vTaskDelay(1000);
    }

    task_creation_result = xTaskCreatePinnedToCore(
        broadcast_sensors_task, "WsSensorsTask", 2048, NULL, 2, NULL, 1);
    if (task_creation_result != pdPASS)
    {
        Serial.println("CRITICAL: Failed to create WsSensorsTask!");
        while (1) vTaskDelay(1000);
    }

    Serial.println("Setup complete. All tasks are running.");
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
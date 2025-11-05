#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "config.h"
#include "state.h"
#include "tasks/camera_task.h"
#include "tasks/sensors_task.h"
#include "web/web_server.h"
#include "settings_manager.h" // <-- Добавили

// Глобальные определения
AppState g_app_state;
SemaphoreHandle_t xStateMutex = NULL;
EventGroupHandle_t xAppEventGroup = NULL;


// Новая задача для отправки данных по WebSocket
void websocket_broadcast_task(void *pvParameters) {
    (void)pvParameters;
    DynamicJsonDocument doc(128);
    char buffer[100];

    for (;;) {
        // Ждем 100 мс между отправками (10 Гц)
        vTaskDelay(pdMS_TO_TICKS(100));

        // Если нет подключенных клиентов, ничего не делаем
        if (get_ws_clients_count() == 0) {
            continue;
        }

        // Захватываем мьютекс для безопасного чтения данных
        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            JsonArray sensors = doc["sensors"].to<JsonArray>();
            sensors.clear();
            for (int i = 0; i < NUM_SENSORS; ++i) {
                // Округляем до 2 знаков после запятой
                sensors.add(round(g_app_state.sensor_distances[i] / 10.0) / 10.0);
            }

            // Освобождаем мьютекс
            xSemaphoreGive(xStateMutex);

            size_t len = serializeJson(doc, buffer);
            broadcast_ws_text(buffer, len);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Booting up...");

    xStateMutex = xSemaphoreCreateMutex();
    xAppEventGroup = xEventGroupCreate();
    if (!xStateMutex || !xAppEventGroup) {
        Serial.println("Failed to create sync objects!");
        return;
    }

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed!");
        return;
    }
    
    // Инициализируем настройки из файла
    settings_init(); // <-- Заменили

    // Заполняем начальные значения датчиков
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
    // Создаем новую задачу для WebSocket
    xTaskCreatePinnedToCore(websocket_broadcast_task, "WsBroadcastTask", 4096, NULL, 3, NULL, 1); // <-- Добавили
    
    Serial.println("Setup complete. Tasks are running.");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
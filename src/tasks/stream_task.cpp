#include "stream_task.h"
#include <Arduino.h>
#include <vector>
#include "ESPAsyncWebServer.h"
#include "freertos/semphr.h"
#include "state.h"
#include "tasks/camera_task.h"

// Глобальные объекты
std::vector<AsyncResponseStream*> g_stream_clients;
SemaphoreHandle_t xStreamMutex = xSemaphoreCreateMutex();

void stream_task(void *pvParameters) {
    (void)pvParameters;
    
    for (;;) {
        // ШАГ 1: Ждем, пока не появится запрос на стрим
        xEventGroupWaitBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        Serial.println("[StreamTask] Request received. Initializing camera...");

        // ШАГ 2: Инициализируем камеру
        bool flip_h, flip_v;
        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
            g_camera_config.frame_size = string_to_framesize(g_app_state.settings.resolution);
            g_camera_config.jpeg_quality = g_app_state.settings.jpeg_quality;
            flip_h = g_app_state.settings.flip_h;
            flip_v = g_app_state.settings.flip_v;
            xSemaphoreGive(xStateMutex);
        }

        if (esp_camera_init(&g_camera_config) != ESP_OK) {
            Serial.println("[StreamTask] Camera init failed!");
            xEventGroupClearBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT); // Сбрасываем запрос
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        sensor_t *s = esp_camera_sensor_get();
        if (s) {
            s->set_hmirror(s, flip_h ? 1 : 0);
            s->set_vflip(s, flip_v ? 1 : 0);
        }
        Serial.println("[StreamTask] Camera initialized successfully.");

        // ШАГ 3: Входим в цикл стриминга, пока есть клиенты
        while (!g_stream_clients.empty()) {
            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) {
                Serial.println("[StreamTask] Failed to get frame buffer");
                vTaskDelay(pdMS_TO_TICKS(100)); // Пауза при ошибке
                continue;
            }

            // Отправляем кадр всем подключенным клиентам
            if (xSemaphoreTake(xStreamMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                for (auto client : g_stream_clients) {
                    client->print("--123456789000000000000987654321\r\n");
                    client->print("Content-Type: image/jpeg\r_n");
                    client->printf("Content-Length: %u\r\n\r\n", fb->len);
                    client->write(fb->buf, fb->len);
                    client->print("\r\n");
                }
                xSemaphoreGive(xStreamMutex);
            }

            esp_camera_fb_return(fb);
            vTaskDelay(pdMS_TO_TICKS(45)); // ~22 FPS
        }

        // ШАГ 4: Клиентов не осталось, выключаем камеру
        Serial.println("[StreamTask] No clients left. De-initializing camera.");
        esp_camera_deinit();
        xEventGroupClearBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT); // Гарантированно сбрасываем флаг
    }
}
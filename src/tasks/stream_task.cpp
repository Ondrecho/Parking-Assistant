#include "stream_task.h"
#include <Arduino.h>
#include <vector>
#include "ESPAsyncWebServer.h"
#include "freertos/semphr.h"
#include "state.h"
#include "tasks/camera_task.h"

// --- Глобальные объекты для стриминга, доступные из web_server.cpp ---
std::vector<AsyncResponseStream *> g_stream_clients;
SemaphoreHandle_t xStreamMutex = xSemaphoreCreateMutex();

void stream_task(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        // Контролируем FPS здесь
        vTaskDelay(pdMS_TO_TICKS(66)); // ~15 FPS

        // Если нет клиентов, просто отдыхаем
        if (g_stream_clients.empty())
        {
            continue;
        }

        camera_fb_t *fb = camera_get_one_frame();
        if (!fb)
        {
            // Камера может быть не готова, это нормально
            continue;
        }

        // Блокируем мьютекс для безопасной работы со списком клиентов
        if (xSemaphoreTake(xStreamMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            for (auto it = g_stream_clients.begin(); it != g_stream_clients.end(); ++it)
            {
                AsyncResponseStream *client = *it;

                client->print("--123456789000000000000987654321\r\n");
                client->print("Content-Type: image/jpeg\r\n");
                client->printf("Content-Length: %u\r\n\r\n", fb->len);
                client->write(fb->buf, fb->len);
                client->print("\r\n");
            }
            xSemaphoreGive(xStreamMutex);
        }

        esp_camera_fb_return(fb);
    }
}

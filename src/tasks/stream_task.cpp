#include "stream_task.h"
#include <Arduino.h>
#include <vector>
#include "ESPAsyncWebServer.h"
#include "freertos/semphr.h"
#include "state.h"
#include "tasks/camera_task.h"

// Глобальные объекты
std::vector<AsyncResponseStream *> g_stream_clients;
SemaphoreHandle_t xStreamMutex = xSemaphoreCreateMutex();

void stream_task(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        // ШАГ 1: Ждем, пока камера не будет полностью готова
        xEventGroupWaitBits(xAppEventGroup, CAM_INITIALIZED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        // ШАГ 2: Внутренний цикл отправки кадров (работает, пока камера включена)
        while (xEventGroupGetBits(xAppEventGroup) & CAM_INITIALIZED_BIT)
        {

            if (g_stream_clients.empty())
            {
                // Если клиентов не стало, выходим и ждем нового сигнала
                break;
            }

            camera_fb_t *fb = camera_get_one_frame();
            if (!fb)
            {
                // Камера готова, но буфер еще не захвачен, это нормально.
                // Добавляем короткую задержку, чтобы избежать hot-loop.
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

            if (xSemaphoreTake(xStreamMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                for (auto client : g_stream_clients)
                {
                    client->print("--123456789000000000000987654321\r\n");
                    client->print("Content-Type: image/jpeg\r\n");
                    client->printf("Content-Length: %u\r\n\r\n", fb->len);
                    client->write(fb->buf, fb->len);
                    client->print("\r\n");
                }
                xSemaphoreGive(xStreamMutex);
            }

            esp_camera_fb_return(fb);

            vTaskDelay(pdMS_TO_TICKS(66)); // ~15 FPS
        }
    }
}
#include "stream_task.h"
#include <Arduino.h>
#include <vector>
#include "ESPAsyncWebServer.h"
#include "freertos/semphr.h"
#include "state.h"
#include "tasks/camera_task.h"

// Глобальные объекты для стриминга
std::vector<AsyncResponseStream*> g_stream_clients;
SemaphoreHandle_t xStreamMutex = xSemaphoreCreateMutex();

void stream_task(void *pvParameters) {
    (void)pvParameters;
    
    for (;;) {
        // Сначала ждем, пока камера не будет полностью готова
        // Этот вызов заблокирует задачу, пока camera_task не выставит флаг
        EventBits_t bits = xEventGroupWaitBits(xAppEventGroup, CAM_INITIALIZED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        // Как только камера готова, входим во внутренний цикл отправки кадров
        // Этот цикл будет работать до тех пор, пока флаг готовности установлен
        while (xEventGroupGetBits(xAppEventGroup) & CAM_INITIALIZED_BIT) {
            
            // Если клиентов вдруг не стало, выходим из внутреннего цикла
            if (g_stream_clients.empty()) {
                break;
            }

            camera_fb_t *fb = camera_get_one_frame();
            if (!fb) {
                vTaskDelay(pdMS_TO_TICKS(10)); // Камера занята, короткая пауза
                continue;
            }

            if (xSemaphoreTake(xStreamMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                for (auto client : g_stream_clients) {
                    client->print("--123456789000000000000987654321\r\n");
                    client->print("Content-Type: image/jpeg\r\n");
                    client->printf("Content-Length: %u\r\n\r\n", fb->len);
                    client->write(fb->buf, fb->len);
                    client->print("\r\n");
                }
                xSemaphoreGive(xStreamMutex);
            }
            
            esp_camera_fb_return(fb);
            
            // Контролируем FPS
            vTaskDelay(pdMS_TO_TICKS(66)); // ~15 FPS
        }
        // Если мы вышли из цикла, значит, камера была выключена.
        // Просто ждем в начале внешнего цикла for(;;), пока она снова не будет готова.
    }
}
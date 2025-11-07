#include "stream_task.h"
#include <Arduino.h>
#include "state.h"
#include "esp_camera.h"
#include "web/websocket_manager.h"

extern SemaphoreHandle_t xCameraMutex;

void stream_task(void *pvParameters) {
    (void)pvParameters;

    for (;;) {
        xEventGroupWaitBits(xAppEventGroup, CAM_INITIALIZED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        while (xEventGroupGetBits(xAppEventGroup) & CAM_INITIALIZED_BIT) {
            
            if (get_stream_clients_count() == 0) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            // Шаг 1: Сначала проверяем, можем ли мы ВООБЩЕ что-то отправить.
            if (!is_stream_writable()) {
                // Дадим сетевой задаче время на очистку буфера.
                vTaskDelay(pdMS_TO_TICKS(10)); 
                continue; // Начинаем следующую итерацию цикла.
            }

            // Если мы здесь, значит буфер свободен. Теперь можно работать с камерой.
            camera_fb_t *fb = NULL;

            if (xSemaphoreTake(xCameraMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                fb = esp_camera_fb_get();
                xSemaphoreGive(xCameraMutex);
            }

            if (!fb) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

            // Отправляем кадр (мы уже знаем, что буфер был свободен)
            broadcast_ws_stream(fb->buf, fb->len);

            // Возвращаем буфер кадра
            if (xSemaphoreTake(xCameraMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                esp_camera_fb_return(fb);
                xSemaphoreGive(xCameraMutex);
            }
            
            // Минимальная задержка, чтобы уступить процессорное время, когда все хорошо
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
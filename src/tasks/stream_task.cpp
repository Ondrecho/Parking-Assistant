#include "stream_task.h"
#include <Arduino.h>
#include "state.h"
#include "esp_camera.h"
#include "web/websocket_manager.h"

extern SemaphoreHandle_t xCameraMutex;

const size_t MAX_FRAME_SIZE_BYTES = 100 * 1024;

void stream_task(void *pvParameters) {
    (void)pvParameters;

    for (;;) {
        xEventGroupWaitBits(xAppEventGroup, CAM_INITIALIZED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        while (xEventGroupGetBits(xAppEventGroup) & CAM_INITIALIZED_BIT) {
            
            if (get_stream_clients_count() == 0) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            if (!is_stream_writable()) {
                vTaskDelay(pdMS_TO_TICKS(10)); 
                continue;
            }

            camera_fb_t *fb = NULL;

            if (xSemaphoreTake(xCameraMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                fb = esp_camera_fb_get();
                xSemaphoreGive(xCameraMutex);
            }

            if (fb) {
                if (fb->len > MAX_FRAME_SIZE_BYTES) {
                    Serial.printf("[StreamTask] Frame too large (%u bytes > %u), dropping.\n", fb->len, MAX_FRAME_SIZE_BYTES);
                } else {
                    broadcast_ws_stream(fb->buf, fb->len);
                }

                if (xSemaphoreTake(xCameraMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    esp_camera_fb_return(fb);
                    xSemaphoreGive(xCameraMutex);
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
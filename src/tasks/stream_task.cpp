#include "stream_task.h"
#include <Arduino.h>
#include "esp_camera.h"
#include "web/websocket_manager.h"

extern QueueHandle_t xFrameQueue;
extern SemaphoreHandle_t xCameraMutex;

void stream_task(void *pvParameters) {
    (void)pvParameters;
    camera_fb_t *fb = NULL;

    for (;;) {
        if (xQueueReceive(xFrameQueue, &fb, portMAX_DELAY) == pdTRUE) {
            if (fb) {
                if (get_stream_clients_count() > 0 && is_stream_writable()) {
                    broadcast_ws_stream(fb->buf, fb->len);
                }

                if (xSemaphoreTake(xCameraMutex, portMAX_DELAY) == pdTRUE) {
                    esp_camera_fb_return(fb);
                    xSemaphoreGive(xCameraMutex);
                }
            }
        }
    }
}
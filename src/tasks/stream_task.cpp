#include "stream_task.h"
#include <Arduino.h>
#include "state.h"
#include "esp_camera.h"
#include "web/websocket_manager.h"
#include "tasks/camera_utils.h" 

extern SemaphoreHandle_t xCameraMutex;

void stream_task(void *pvParameters) {
    (void)pvParameters;

    for (;;) {
        xEventGroupWaitBits(xAppEventGroup, CAM_INITIALIZED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        while (xEventGroupGetBits(xAppEventGroup) & CAM_STREAM_REQUEST_BIT) {
            
            if (get_stream_clients_count() == 0) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            if (!is_stream_writable()) {
                vTaskDelay(pdMS_TO_TICKS(10)); 
                continue;
            }

            camera_fb_t *fb = safe_camera_get_fb();

            if (fb) {
                broadcast_ws_stream(fb->buf, fb->len);
                safe_camera_return_fb(fb);
            } else {
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
    }
}
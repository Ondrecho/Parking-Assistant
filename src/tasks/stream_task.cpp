#include "stream_task.h"
#include <Arduino.h>
#include "state.h"
#include "tasks/camera_task.h"
#include "web/websocket_manager.h"

void stream_task(void *pvParameters) {
    (void)pvParameters;

    for (;;) {
        xEventGroupWaitBits(xAppEventGroup, CAM_INITIALIZED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        while (xEventGroupGetBits(xAppEventGroup) & CAM_INITIALIZED_BIT) {
            if (get_stream_clients_count() == 0) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            camera_fb_t *fb = camera_get_one_frame();
            if (!fb) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

            broadcast_ws_stream(fb->buf, fb->len);

            esp_camera_fb_return(fb);

            vTaskDelay(pdMS_TO_TICKS(66));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
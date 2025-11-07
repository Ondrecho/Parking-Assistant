#include "stream_task.h"
#include <Arduino.h>
#include "state.h"
#include "web/websocket_manager.h"
#include "tasks/camera_utils.h"

#define LOG_DEBUG(format, ...) Serial.printf("[%-12s] " format "\n", "StreamTask", ##__VA_ARGS__)

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

            LOG_DEBUG("Attempting to get frame...");
            camera_fb_t *fb = safe_camera_get_fb();

            if (fb) {
                LOG_DEBUG("Got frame, size %u. Broadcasting...", fb->len);
                broadcast_ws_stream(fb->buf, fb->len);
                LOG_DEBUG("Broadcast done. Returning frame...");
                safe_camera_return_fb(fb);
                LOG_DEBUG("Frame returned.");
            } else {
                LOG_DEBUG("Failed to get frame. Camera might be busy/restarting.");
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
        LOG_DEBUG("Exited inner stream loop. Waiting for camera to be ready again.");
    }
}
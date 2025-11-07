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

        while (xEventGroupGetBits(xAppEventGroup) & CAM_STREAM_REQUEST_BIT) {
            
            if (get_stream_clients_count() == 0) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            if (!is_stream_writable()) {
                vTaskDelay(pdMS_TO_TICKS(10)); 
                continue;
            }

            if (xSemaphoreTake(xCameraMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                
                if (!(xEventGroupGetBits(xAppEventGroup) & CAM_INITIALIZED_BIT)) {
                    xSemaphoreGive(xCameraMutex);
                    break; 
                }
                
                camera_fb_t *fb = esp_camera_fb_get();

                if (fb) {
                    broadcast_ws_stream(fb->buf, fb->len);
                    
                    esp_camera_fb_return(fb);
                }
                
                xSemaphoreGive(xCameraMutex);

            } else {

                vTaskDelay(pdMS_TO_TICKS(5));
            }
            
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
    }
}
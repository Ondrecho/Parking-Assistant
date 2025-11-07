#include "frame_grab_task.h"
#include "state.h"
#include "esp_camera.h"
#include "freertos/queue.h"

extern EventGroupHandle_t xAppEventGroup;
extern SemaphoreHandle_t xCameraMutex;
extern QueueHandle_t xFrameQueue;

void frame_grab_task(void *pvParameters) {
    (void)pvParameters;

    for (;;) {
        xEventGroupWaitBits(xAppEventGroup, CAM_INITIALIZED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        camera_fb_t *fb = NULL;

        if (xSemaphoreTake(xCameraMutex, portMAX_DELAY) == pdTRUE) {
            if (xEventGroupGetBits(xAppEventGroup) & CAM_INITIALIZED_BIT) {
                fb = esp_camera_fb_get();
            }
            xSemaphoreGive(xCameraMutex);
        }

        if (fb) {
  
            xQueueOverwrite(xFrameQueue, &fb);
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
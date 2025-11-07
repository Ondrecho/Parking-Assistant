#include "camera_utils.h"
#include "state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern SemaphoreHandle_t xCameraMutex;
extern EventGroupHandle_t xAppEventGroup;

camera_fb_t* safe_camera_get_fb() {
    camera_fb_t* fb = NULL;

    if (xSemaphoreTake(xCameraMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Проверяем состояние камеры ПОД мьютексом, чтобы гарантировать,
        // что она не будет деинициализирована между проверкой и использованием.
        if (xEventGroupGetBits(xAppEventGroup) & CAM_INITIALIZED_BIT) {
            fb = esp_camera_fb_get();
        }
        xSemaphoreGive(xCameraMutex);
    }
    return fb;
}

void safe_camera_return_fb(camera_fb_t* fb) {
    if (!fb) {
        return;
    }
    if (xSemaphoreTake(xCameraMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        esp_camera_fb_return(fb);
        xSemaphoreGive(xCameraMutex);
    }
}
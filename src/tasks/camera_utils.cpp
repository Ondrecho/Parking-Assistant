#include "camera_utils.h"
#include "state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern SemaphoreHandle_t xCameraMutex;
extern EventGroupHandle_t xAppEventGroup;

#define LOG_DEBUG(format, ...) Serial.printf("[%-12s] " format "\n", "CamUtils", ##__VA_ARGS__)

camera_fb_t* safe_camera_get_fb() {
    camera_fb_t* fb = NULL;

    LOG_DEBUG("GET_FB: Attempting to take mutex...");
    if (xSemaphoreTake(xCameraMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        LOG_DEBUG("GET_FB: Mutex TAKEN.");
        if (xEventGroupGetBits(xAppEventGroup) & CAM_INITIALIZED_BIT) {
            LOG_DEBUG("GET_FB: Camera is initialized, calling esp_camera_fb_get()...");
            fb = esp_camera_fb_get();
        } else {
            LOG_DEBUG("GET_FB: Camera NOT initialized, skipping get.");
        }
        LOG_DEBUG("GET_FB: Giving mutex...");
        xSemaphoreGive(xCameraMutex);
    } else {
        LOG_DEBUG("GET_FB: FAILED to take mutex!");
    }
    return fb;
}

void safe_camera_return_fb(camera_fb_t* fb) {
    if (!fb) {
        return;
    }
    LOG_DEBUG("RETURN_FB: Attempting to take mutex...");
    if (xSemaphoreTake(xCameraMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        LOG_DEBUG("RETURN_FB: Mutex TAKEN. Returning FB...");
        esp_camera_fb_return(fb);
        LOG_DEBUG("RETURN_FB: Giving mutex...");
        xSemaphoreGive(xCameraMutex);
    } else {
        LOG_DEBUG("RETURN_FB: FAILED to take mutex!");
    }
}
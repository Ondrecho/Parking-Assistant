#include "camera_task.h"
#include <Arduino.h>
#include "freertos/event_groups.h"
#include "config.h"
#include "state.h"

// Вспомогательная функция для преобразования строки в framesize_t
framesize_t string_to_framesize(const char* str) {
    if (strcmp(str, "QQVGA") == 0) return FRAMESIZE_QQVGA;
    if (strcmp(str, "QVGA") == 0) return FRAMESIZE_QVGA;
    if (strcmp(str, "VGA") == 0) return FRAMESIZE_VGA;
    if (strcmp(str, "SVGA") == 0) return FRAMESIZE_SVGA;
    if (strcmp(str, "XGA") == 0) return FRAMESIZE_XGA;
    return FRAMESIZE_VGA;
}

// --- Полная версия camera_task ---
void camera_task(void *pvParameters) {
    (void)pvParameters;
    
    // Конфигурация камеры
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAM_PIN_D0;
    config.pin_d1 = CAM_PIN_D1;
    config.pin_d2 = CAM_PIN_D2;
    config.pin_d3 = CAM_PIN_D3;
    config.pin_d4 = CAM_PIN_D4;
    config.pin_d5 = CAM_PIN_D5;
    config.pin_d6 = CAM_PIN_D6;
    config.pin_d7 = CAM_PIN_D7;
    config.pin_xclk = CAM_PIN_XCLK;
    config.pin_pclk = CAM_PIN_PCLK;
    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href = CAM_PIN_HREF;
    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;
    config.pin_pwdn = CAM_PIN_PWDN;
    config.pin_reset = CAM_PIN_RESET;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_count = 2;

    for (;;) {
        // 1. Ждем запроса от веб-сервера на включение
        xEventGroupWaitBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        // 2. Загружаем актуальные настройки
        bool flip_h, flip_v;
        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
            config.frame_size = string_to_framesize(g_app_state.settings.resolution);
            config.jpeg_quality = g_app_state.settings.jpeg_quality;
            flip_h = g_app_state.settings.flip_h;
            flip_v = g_app_state.settings.flip_v;
            g_app_state.is_camera_initialized = false;
            xSemaphoreGive(xStateMutex);
        }

        // 3. Инициализируем камеру
        esp_err_t err = esp_camera_init(&config);
        if (err != ESP_OK) {
            Serial.printf("[CameraTask] CRITICAL: Camera init failed with error 0x%x (%s)\n", err, esp_err_to_name(err));
            xEventGroupClearBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        
        // Применяем настройки к сенсору
        sensor_t *s = esp_camera_sensor_get();
        if (s) {
            s->set_hmirror(s, flip_h ? 1 : 0);
            s->set_vflip(s, flip_v ? 1 : 0);
        }
        
        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
            g_app_state.is_camera_initialized = true;
            xSemaphoreGive(xStateMutex);
        }

        // 4. Подаем сигнал "ГОТОВО" другим задачам
        xEventGroupSetBits(xAppEventGroup, CAM_INITIALIZED_BIT);
        Serial.println("[CameraTask] Camera initialized. Signal sent.");

        // 5. Ждем, пока веб-сервер не отменит запрос
        while (xEventGroupGetBits(xAppEventGroup) & CAM_STREAM_REQUEST_BIT) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // 6. Убираем сигнал "ГОТОВО" и деинициализируем камеру
        xEventGroupClearBits(xAppEventGroup, CAM_INITIALIZED_BIT);
        esp_camera_deinit();

        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
            g_app_state.is_camera_initialized = false;
            xSemaphoreGive(xStateMutex);
        }
        Serial.println("[CameraTask] Camera de-initialized.");
    }
}

camera_fb_t* camera_get_one_frame() {
    bool is_cam_init = false;
    if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        is_cam_init = g_app_state.is_camera_initialized;
        xSemaphoreGive(xStateMutex);
    }
    if (!is_cam_init) { return NULL; }
    return esp_camera_fb_get();
}
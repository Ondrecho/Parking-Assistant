#include "camera_task.h"
#include <Arduino.h>
#include "config.h"
#include "state.h"

// Глобальная, общая для всех конфигурация
camera_config_t g_camera_config;

// Вспомогательная функция (остается здесь)
framesize_t string_to_framesize(const char* str) {
    if (strcmp(str, "QQVGA") == 0) return FRAMESIZE_QQVGA;
    if (strcmp(str, "QVGA") == 0) return FRAMESIZE_QVGA;
    if (strcmp(str, "VGA") == 0) return FRAMESIZE_VGA;
    if (strcmp(str, "SVGA") == 0) return FRAMESIZE_SVGA;
    if (strcmp(str, "XGA") == 0) return FRAMESIZE_XGA;
    return FRAMESIZE_VGA;
}

// Инициализация базовой конфигурации при старте
void camera_init_config() {
    g_camera_config.ledc_channel = LEDC_CHANNEL_0;
    g_camera_config.ledc_timer = LEDC_TIMER_0;
    g_camera_config.pin_d0 = CAM_PIN_D0;
    g_camera_config.pin_d1 = CAM_PIN_D1;
    g_camera_config.pin_d2 = CAM_PIN_D2;
    g_camera_config.pin_d3 = CAM_PIN_D3;
    g_camera_config.pin_d4 = CAM_PIN_D4;
    g_camera_config.pin_d5 = CAM_PIN_D5;
    g_camera_config.pin_d6 = CAM_PIN_D6;
    g_camera_config.pin_d7 = CAM_PIN_D7;
    g_camera_config.pin_xclk = CAM_PIN_XCLK;
    g_camera_config.pin_pclk = CAM_PIN_PCLK;
    g_camera_config.pin_vsync = CAM_PIN_VSYNC;
    g_camera_config.pin_href = CAM_PIN_HREF;
    g_camera_config.pin_sccb_sda = CAM_PIN_SIOD;
    g_camera_config.pin_sccb_scl = CAM_PIN_SIOC;
    g_camera_config.pin_pwdn = CAM_PIN_PWDN;
    g_camera_config.pin_reset = CAM_PIN_RESET;
    g_camera_config.xclk_freq_hz = 20000000;
    g_camera_config.pixel_format = PIXFORMAT_JPEG;
    g_camera_config.fb_location = CAMERA_FB_IN_PSRAM;
    g_camera_config.grab_mode = CAMERA_GRAB_LATEST;
    g_camera_config.fb_count = 2;
}


// Эта задача теперь просто спит. Она нам больше не нужна для стрима.
// Оставляем ее как заглушку для будущей логики snapshot.
void camera_task(void *pvParameters) {
    (void)pvParameters;
    for(;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

// Эта функция больше не нужна, т.к. стрим-задача сама управляет камерой
camera_fb_t* camera_get_one_frame() {
    // В будущем здесь будет логика для snapshot
    return NULL; 
}
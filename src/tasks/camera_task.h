#pragma once
#include "esp_camera.h"

extern camera_config_t g_camera_config; // Делаем конфиг доступным

void camera_init_config(); // Объявляем функцию инициализации конфига
void camera_task(void *pvParameters);
camera_fb_t* camera_get_one_frame();
framesize_t string_to_framesize(const char* str); // Оставляем
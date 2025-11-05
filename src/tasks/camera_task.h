#pragma once
#include "esp_camera.h" // Нужно для camera_fb_t

// Прототип задачи управления камерой
void camera_task(void *pvParameters);

// Функция для получения одного кадра (для snapshot)
// Возвращает указатель на frame buffer, который нужно освободить с помощью esp_camera_fb_return()
camera_fb_t* camera_get_one_frame();
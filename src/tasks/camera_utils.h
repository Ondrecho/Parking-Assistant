#pragma once
#include "esp_camera.h"

// Безопасно получает один кадр.
// Возвращает NULL, если камера не инициализирована или произошла ошибка.
camera_fb_t* safe_camera_get_fb();

// Безопасно возвращает буфер кадра.
void safe_camera_return_fb(camera_fb_t* fb);
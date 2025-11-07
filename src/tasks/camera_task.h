#pragma once
#include "esp_camera.h"

void camera_task(void *pvParameters);
camera_fb_t* camera_get_one_frame();
#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "config.h"

struct AppState {
    AppSettings settings;
    float sensor_distances[NUM_SENSORS];
    bool is_camera_initialized;
    bool is_parktronic_active;     
    bool is_muted;
};

extern AppState g_app_state;
extern SemaphoreHandle_t xStateMutex;
extern EventGroupHandle_t xAppEventGroup;

const EventBits_t CAM_STREAM_REQUEST_BIT = (1 << 0);
const EventBits_t CAM_INITIALIZED_BIT    = (1 << 1);
const EventBits_t PARKTRONIC_ACTIVE_BIT  = (1 << 2);
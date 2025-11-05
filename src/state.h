#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "config.h"

// --- Глобальное состояние приложения ---
struct AppState {
    AppSettings settings;
    float sensor_distances[NUM_SENSORS];
    bool is_camera_initialized;
};

extern AppState g_app_state;
extern SemaphoreHandle_t xStateMutex; // Мьютекс для защиты g_app_state
extern EventGroupHandle_t xAppEventGroup; // Группа событий для управления задачами

// --- Биты для группы событий ---
const EventBits_t CAM_STREAM_REQUEST_BIT = (1 << 0); // Бит: клиент запрашивает стрим

// --- Утилиты для безопасного доступа к состоянию ---
// (В будущем здесь можно будет добавить inline-функции для блокировки/разблокировки)
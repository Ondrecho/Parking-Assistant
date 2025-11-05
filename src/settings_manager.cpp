#include "settings_manager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "state.h"
#include "config.h"
#include "esp_camera.h"

void load_default_settings() {
    g_app_state.settings.thresh_yellow = 2.0;
    g_app_state.settings.thresh_orange = 1.0;
    g_app_state.settings.thresh_red = 0.5;
    g_app_state.settings.bpm_min = 0;
    g_app_state.settings.bpm_max = 300;
    g_app_state.settings.auto_start = true;
    g_app_state.settings.show_grid = true;
    g_app_state.settings.cam_angle = 45;
    g_app_state.settings.grid_opacity = 0.8;
    g_app_state.settings.grid_offset_x = 0;
    g_app_state.settings.grid_offset_y = 0;
    g_app_state.settings.grid_offset_z = 0;
    strlcpy(g_app_state.settings.resolution, "XGA", sizeof(g_app_state.settings.resolution));
    g_app_state.settings.jpeg_quality = 12;
    g_app_state.settings.flip_h = true;
    g_app_state.settings.flip_v = false;
    g_app_state.settings.is_muted = false;
    g_app_state.settings.stream_active = true;
    strncpy(g_app_state.settings.wifi_ssid, WIFI_AP_SSID, sizeof(g_app_state.settings.wifi_ssid));
    strncpy(g_app_state.settings.wifi_pass, WIFI_AP_PASS, sizeof(g_app_state.settings.wifi_pass));
}

void settings_init() {
    if (LittleFS.exists("/settings.json")) {
        File file = LittleFS.open("/settings.json", "r");
        if (file) {
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, file);
            if (!error) {
                // ЗАМЕНИТЬ старый блок загрузки на этот полный блок
                g_app_state.settings.thresh_yellow = doc["thresh_yellow"] | 2.0;
                g_app_state.settings.thresh_orange = doc["thresh_orange"] | 1.0;
                g_app_state.settings.thresh_red = doc["thresh_red"] | 0.5;
                g_app_state.settings.bpm_min = doc["bpm_min"] | 0;
                g_app_state.settings.bpm_max = doc["bpm_max"] | 300;
                g_app_state.settings.auto_start = doc["auto_start"] | true;
                g_app_state.settings.show_grid = doc["show_grid"] | true;
                g_app_state.settings.cam_angle = doc["cam_angle"] | 45;
                g_app_state.settings.grid_opacity = doc["grid_opacity"] | 0.8;
                g_app_state.settings.grid_offset_x = doc["grid_offset_x"] | 0;
                g_app_state.settings.grid_offset_y = doc["grid_offset_y"] | 0;
                g_app_state.settings.grid_offset_z = doc["grid_offset_z"] | 0;
                strlcpy(g_app_state.settings.resolution, doc["resolution"] | "XGA", sizeof(g_app_state.settings.resolution));
                g_app_state.settings.jpeg_quality = doc["jpeg_quality"] | 12;
                g_app_state.settings.flip_h = doc["flip_h"] | true;
                g_app_state.settings.flip_v = doc["flip_v"] | false;
                g_app_state.settings.is_muted = doc["is_muted"] | false;
                g_app_state.settings.stream_active = doc["stream_active"] | true;
                strlcpy(g_app_state.settings.wifi_ssid, doc["wifi_ssid"] | WIFI_AP_SSID, sizeof(g_app_state.settings.wifi_ssid));
                strlcpy(g_app_state.settings.wifi_pass, doc["wifi_pass"] | WIFI_AP_PASS, sizeof(g_app_state.settings.wifi_pass));
                Serial.println("Settings loaded from file.");
            } else {
                Serial.println("Failed to parse settings.json, loading defaults.");
                load_default_settings();
            }
            file.close();
        }
    } else {
        Serial.println("settings.json not found, loading defaults and creating file.");
        load_default_settings();
        settings_save(); // Сохраняем дефолтные настройки, чтобы файл создался
    }
}

bool settings_save() {
    DynamicJsonDocument doc(2048);

    // Блокируем мьютекс на время чтения состояния
    if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("Failed to take mutex for saving settings");
        return false;
    }
    
    // Сериализуем все настройки в JSON
    doc["thresh_yellow"] = g_app_state.settings.thresh_yellow;
    doc["thresh_orange"] = g_app_state.settings.thresh_orange;
    doc["thresh_red"] = g_app_state.settings.thresh_red;
    doc["bpm_min"] = g_app_state.settings.bpm_min;
    doc["bpm_max"] = g_app_state.settings.bpm_max;
    doc["auto_start"] = g_app_state.settings.auto_start;
    doc["show_grid"] = g_app_state.settings.show_grid;
    doc["cam_angle"] = g_app_state.settings.cam_angle;
    doc["grid_opacity"] = g_app_state.settings.grid_opacity;
    doc["grid_offset_x"] = g_app_state.settings.grid_offset_x;
    doc["grid_offset_y"] = g_app_state.settings.grid_offset_y;
    doc["grid_offset_z"] = g_app_state.settings.grid_offset_z;
    doc["resolution"] = g_app_state.settings.resolution;
    doc["jpeg_quality"] = g_app_state.settings.jpeg_quality;
    doc["flip_h"] = g_app_state.settings.flip_h;
    doc["flip_v"] = g_app_state.settings.flip_v;
    doc["is_muted"] = g_app_state.settings.is_muted;
    doc["stream_active"] = g_app_state.settings.stream_active;
    doc["wifi_ssid"] = g_app_state.settings.wifi_ssid;
    doc["wifi_pass"] = g_app_state.settings.wifi_pass;

    xSemaphoreGive(xStateMutex);

    File file = LittleFS.open("/settings.json", "w");
    if (!file) {
        Serial.println("Failed to open settings.json for writing");
        return false;
    }
    
    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write to settings.json");
        file.close();
        return false;
    }

    file.close();
    Serial.println("Settings saved successfully.");
    return true;
}
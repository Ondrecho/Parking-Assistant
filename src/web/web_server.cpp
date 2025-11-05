#include "web_server.h"
#include "ESPAsyncWebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "state.h"
#include "config.h"
#include "settings_manager.h"

// --- Глобальные объекты сервера ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// --- Вспомогательные функции ---
int get_ws_clients_count() {
    return ws.count();
}

void broadcast_ws_text(const char* data, size_t len) {
    ws.textAll(data, len);
}

// --- Обработчики ---

// Обработчик событий WebSocket
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WS client #%u disconnected\n", client->id());
  }
}

// Обработчик для несуществующих страниц
void onNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

// Обработчики API
void handle_get_settings(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(2048);
    
    if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Ручная сериализация
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
    } else {
        request->send(503, "text/plain", "Service Unavailable");
        return;
    }

    serializeJson(doc, *response);
    request->send(response);
}

void handle_post_settings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index + len != total) return; // Ждем, пока придет все тело запроса
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "text/plain", "Invalid JSON");
        return;
    }

    if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Десериализация с проверкой наличия ключа
        if (doc.containsKey("thresh_yellow")) g_app_state.settings.thresh_yellow = doc["thresh_yellow"];
        if (doc.containsKey("thresh_orange")) g_app_state.settings.thresh_orange = doc["thresh_orange"];
        if (doc.containsKey("thresh_red")) g_app_state.settings.thresh_red = doc["thresh_red"];
        if (doc.containsKey("bpm_min")) g_app_state.settings.bpm_min = doc["bpm_min"];
        if (doc.containsKey("bpm_max")) g_app_state.settings.bpm_max = doc["bpm_max"];
        if (doc.containsKey("auto_start")) g_app_state.settings.auto_start = doc["auto_start"];
        if (doc.containsKey("show_grid")) g_app_state.settings.show_grid = doc["show_grid"];
        if (doc.containsKey("cam_angle")) g_app_state.settings.cam_angle = doc["cam_angle"];
        if (doc.containsKey("grid_opacity")) g_app_state.settings.grid_opacity = doc["grid_opacity"];
        if (doc.containsKey("grid_offset_x")) g_app_state.settings.grid_offset_x = doc["grid_offset_x"];
        if (doc.containsKey("grid_offset_y")) g_app_state.settings.grid_offset_y = doc["grid_offset_y"];
        if (doc.containsKey("grid_offset_z")) g_app_state.settings.grid_offset_z = doc["grid_offset_z"];
        if (doc.containsKey("resolution")) g_app_state.settings.resolution = doc["resolution"];
        if (doc.containsKey("jpeg_quality")) g_app_state.settings.jpeg_quality = doc["jpeg_quality"];
        if (doc.containsKey("flip_h")) g_app_state.settings.flip_h = doc["flip_h"];
        if (doc.containsKey("flip_v")) g_app_state.settings.flip_v = doc["flip_v"];
        if (doc.containsKey("is_muted")) g_app_state.settings.is_muted = doc["is_muted"];
        if (doc.containsKey("stream_active")) g_app_state.settings.stream_active = doc["stream_active"];
        if (doc.containsKey("wifi_ssid")) strlcpy(g_app_state.settings.wifi_ssid, doc["wifi_ssid"], sizeof(g_app_state.settings.wifi_ssid));
        if (doc.containsKey("wifi_pass")) strlcpy(g_app_state.settings.wifi_pass, doc["wifi_pass"], sizeof(g_app_state.settings.wifi_pass));
        xSemaphoreGive(xStateMutex);
    } else {
        request->send(503, "text/plain", "Service Unavailable");
        return;
    }

    if (settings_save()) {
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "Failed to save settings");
    }
}

// --- Инициализация ---
void init_web_server() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // --- Сначала регистрируем API и будущие обработчики ---
    server.on("/api/settings", HTTP_GET, handle_get_settings);
    server.on("/api/settings", HTTP_POST, 
        [](AsyncWebServerRequest *request){ request->send(200); },
        NULL,
        handle_post_settings
    );
    // Здесь в будущем будут /api/action, /stream, /api/snapshot

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("max-age=600"); // <-- ПЕРЕМЕСТИЛИ ЭТУ СТРОКУ ВНИЗ
    
    server.onNotFound(onNotFound);

    server.begin();
    Serial.println("Web server started");
}
#include "web_server.h"
#include "ESPAsyncWebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "state.h"
#include "config.h"
#include "settings_manager.h"
#include "tasks/camera_task.h" // <-- Добавили

// --- Глобальные объекты сервера ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
volatile int stream_clients_count = 0; // Счетчик активных клиентов стрима

// --- Вспомогательные функции ---
int get_ws_clients_count() { return ws.count(); }
void broadcast_ws_text(const char* data, size_t len) { ws.textAll(data, len); }

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

// --- Обработчики API ---
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
        if (doc.containsKey("resolution")) strlcpy(g_app_state.settings.resolution, doc["resolution"], sizeof(g_app_state.settings.resolution));
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

void handle_api_action(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index + len != total) return;
    
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, data, len)) {
        request->send(400, "text/plain", "Invalid JSON");
        return;
    }

    const char* action = doc["action"];
    if (strcmp(action, "toggleMute") == 0) {
        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_app_state.settings.is_muted = !g_app_state.settings.is_muted;
            xSemaphoreGive(xStateMutex);
        }
        settings_save();
        request->send(200, "text/plain", "OK");
    } else if (strcmp(action, "toggleStream") == 0) {
        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_app_state.settings.stream_active = !g_app_state.settings.stream_active;
            xSemaphoreGive(xStateMutex);
        }
        settings_save();
        request->send(200, "text/plain", "OK");
    } else {
        request->send(400, "text/plain", "Unknown action");
    }
}


void handle_snapshot(AsyncWebServerRequest *request) {
    camera_fb_t *fb = camera_get_one_frame();
    if (!fb) {
        request->send(503, "text/plain", "Camera not ready");
        return;
    }
    
    request->send_P(200, "image/jpeg", (const uint8_t *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

void handle_stream(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("multipart/x-mixed-replace;boundary=123456789000000000000987654321");
    
    // --- Клиент подключился ---
    stream_clients_count++;
    if (stream_clients_count == 1) {
        xEventGroupSetBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT); // Просим включить камеру
    }

    // --- Клиент отключился ---
    request->onDisconnect([response]() {
        stream_clients_count--;
        if (stream_clients_count == 0) {
            xEventGroupClearBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT); // Просим выключить камеру
        }
        delete response; // Очищаем память
    });

    // --- Отправляем кадры ---
    while(true) {
        if(stream_clients_count == 0) break; // Все клиенты отключились
        
        camera_fb_t * fb = camera_get_one_frame();
        if (!fb) {
            vTaskDelay(pdMS_TO_TICKS(100)); // Камера еще не готова, ждем
            continue;
        }

        response->print("--123456789000000000000987654321\r\n");
        response->print("Content-Type: image/jpeg\r\n");
        response->print("Content-Length: ");
        response->print(fb->len);
        response->print("\r\n\r\n");
        response->write(fb->buf, fb->len);
        response->print("\r\n");
        
        esp_camera_fb_return(fb);

        // Даем шанс другим задачам и контролируем FPS
        vTaskDelay(pdMS_TO_TICKS(66)); // ~15 FPS
    }
}

// --- Инициализация ---
void init_web_server() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // --- API и обработчики ---
    server.on("/api/settings", HTTP_GET, handle_get_settings);
    server.on(
        "/api/settings", HTTP_POST, 
        [](AsyncWebServerRequest *request){ request->send(200); }, NULL, handle_post_settings
    );
    server.on(
        "/api/action", HTTP_POST,
        [](AsyncWebServerRequest *request){}, NULL, handle_api_action
    );
    server.on("/api/snapshot", HTTP_GET, handle_snapshot);
    server.on("/stream", HTTP_GET, handle_stream);
    
    // --- Статика и 404 ---
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("max-age=600");
    server.onNotFound(onNotFound);

    server.begin();
    Serial.println("Web server started");
}
#include "web_server.h"
#include "ESPAsyncWebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "state.h"
#include "config.h"
#include "settings_manager.h"
#include "tasks/camera_task.h"
#include "websocket_manager.h"
#include "esp_camera.h"

AsyncWebServer server(80);
extern SemaphoreHandle_t xCameraMutex;

void onNotFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

void handle_get_settings(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(2048);

    if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
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
        doc["is_muted"] = g_app_state.is_muted;
        doc["volume"] = g_app_state.settings.volume;
        doc["beep_freq"] = g_app_state.settings.beep_freq;
        doc["rotation"] = g_app_state.settings.rotation;
        doc["xclk_freq"] = g_app_state.settings.xclk_freq;
        doc["wifi_ssid"] = g_app_state.settings.wifi_ssid;
        doc["wifi_pass"] = g_app_state.settings.wifi_pass;
        xSemaphoreGive(xStateMutex);
    }
    else
    {
        request->send(503, "text/plain", "Service Unavailable");
        return;
    }

    Serial.println("Sending settings JSON:");
    serializeJsonPretty(doc, Serial);
    Serial.println();

    serializeJson(doc, *response);
    request->send(response);
}

void handle_post_settings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    if (index + len != total)
        return;

    Serial.print("Received JSON: ");
    Serial.write(data, len);
    Serial.println();

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, data, len);

    if (error)
    {
        request->send(400, "text/plain", "Invalid JSON");
        return;
    }

    if (doc.containsKey("wifi_ssid"))
    {
        const char *ssid = doc["wifi_ssid"];
        if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 31)
        {
            request->send(400, "text/plain", "Invalid wifi_ssid: must be 1-31 chars long and not null.");
            return;
        }
    }

    if (doc.containsKey("wifi_pass"))
    {
        const char *pass = doc["wifi_pass"];
        if (pass && strlen(pass) > 0 && strlen(pass) < 8)
        {
            request->send(400, "text/plain", "Invalid wifi_pass: must be null or >= 8 chars long.");
            return;
        }
    }

    if (doc.containsKey("jpeg_quality"))
    {
        int quality = doc["jpeg_quality"];
        if (quality < 0 || quality > 63)
        {
            request->send(400, "text/plain", "Invalid jpeg_quality: must be between 0 and 63.");
            return;
        }
    }

    if (doc.containsKey("xclk_freq"))
    {
        int freq = doc["xclk_freq"];
        if (freq < 10 || freq > 24)
        {
            request->send(400, "text/plain", "Invalid xclk_freq: must be between 10 and 24.");
            return;
        }
    }

    if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        if (doc.containsKey("thresh_yellow"))
            g_app_state.settings.thresh_yellow = doc["thresh_yellow"];
        if (doc.containsKey("thresh_orange"))
            g_app_state.settings.thresh_orange = doc["thresh_orange"];
        if (doc.containsKey("thresh_red"))
            g_app_state.settings.thresh_red = doc["thresh_red"];
        if (doc.containsKey("bpm_min"))
            g_app_state.settings.bpm_min = doc["bpm_min"];
        if (doc.containsKey("bpm_max"))
            g_app_state.settings.bpm_max = doc["bpm_max"];
        if (doc.containsKey("auto_start"))
            g_app_state.settings.auto_start = doc["auto_start"];
        if (doc.containsKey("show_grid"))
            g_app_state.settings.show_grid = doc["show_grid"];
        if (doc.containsKey("cam_angle"))
            g_app_state.settings.cam_angle = doc["cam_angle"];
        if (doc.containsKey("grid_opacity"))
            g_app_state.settings.grid_opacity = doc["grid_opacity"];
        if (doc.containsKey("grid_offset_x"))
            g_app_state.settings.grid_offset_x = doc["grid_offset_x"];
        if (doc.containsKey("grid_offset_y"))
            g_app_state.settings.grid_offset_y = doc["grid_offset_y"];
        if (doc.containsKey("grid_offset_z"))
            g_app_state.settings.grid_offset_z = doc["grid_offset_z"];
        if (doc.containsKey("jpeg_quality"))
            g_app_state.settings.jpeg_quality = doc["jpeg_quality"];
        if (doc.containsKey("flip_h"))
            g_app_state.settings.flip_h = doc["flip_h"];
        if (doc.containsKey("flip_v"))
            g_app_state.settings.flip_v = doc["flip_v"];
        if (doc.containsKey("volume"))
            g_app_state.settings.volume = doc["volume"];
        if (doc.containsKey("beep_freq"))
            g_app_state.settings.beep_freq = doc["beep_freq"];
        if (doc.containsKey("rotation"))
            g_app_state.settings.rotation = doc["rotation"];
        if (doc.containsKey("xclk_freq"))
            g_app_state.settings.xclk_freq = doc["xclk_freq"];

        if (doc.containsKey("resolution"))
        {
            const char *res_str = doc["resolution"];
            if (res_str)
            {
                strlcpy(g_app_state.settings.resolution, res_str, sizeof(g_app_state.settings.resolution));
            }
        }
        if (doc.containsKey("wifi_ssid"))
        {
            const char *ssid_str = doc["wifi_ssid"];
            if (ssid_str)
            {
                strlcpy(g_app_state.settings.wifi_ssid, ssid_str, sizeof(g_app_state.settings.wifi_ssid));
            }
        }
        if (doc.containsKey("wifi_pass"))
        {
            const char *pass_str = doc["wifi_pass"];
            if (pass_str)
            {
                strlcpy(g_app_state.settings.wifi_pass, pass_str, sizeof(g_app_state.settings.wifi_pass));
            }
            else
            {
                g_app_state.settings.wifi_pass[0] = '\0';
            }
        }

        xSemaphoreGive(xStateMutex);
    }
    else
    {
        request->send(503, "text/plain", "Service Unavailable");
        return;
    }

    if (settings_save())
    {
        request->send(200, "text/plain", "OK");
    }
    else
    {
        request->send(500, "text/plain", "Failed to save settings");
    }
}

void handle_api_action(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    if (index + len != total)
        return;

    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, data, len))
    {
        request->send(400, "text/plain", "Invalid JSON");
        return;
    }

    const char *action = doc["action"];
    if (!action)
    {
        request->send(400, "text/plain", "Action not specified");
        return;
    }

    if (strcmp(action, "toggleMute") == 0)
    {
        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            g_app_state.is_muted = !g_app_state.is_muted;
            xSemaphoreGive(xStateMutex);
        }
        request->send(200, "text/plain", "OK");
    }
    else if (strcmp(action, "resetSettings") == 0)
    {
        settings_reset_to_default();
        request->send(200, "text/plain", "OK");
    }
    else if (strcmp(action, "toggleParktronic") == 0)
    {
        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            g_app_state.is_manually_activated = !g_app_state.is_manually_activated;
            xSemaphoreGive(xStateMutex);
        }
        request->send(200, "text/plain", "OK");
    }
    else
    {
        request->send(400, "text/plain", "Unknown action");
    }
}

void handle_snapshot(AsyncWebServerRequest *request)
{
    camera_fb_t *fb = NULL;
    bool stream_was_active = (xEventGroupGetBits(xAppEventGroup) & CAM_INITIALIZED_BIT) != 0;

    // Шаг 1: Включаем камеру, если она выключена
    if (!stream_was_active)
    {
        xEventGroupSetBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT);
        EventBits_t bits = xEventGroupWaitBits(xAppEventGroup, CAM_INITIALIZED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(2000));
        if ((bits & CAM_INITIALIZED_BIT) == 0)
        {
            xEventGroupClearBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT);
            request->send(503, "text/plain", "Camera snapshot timeout");
            return;
        }
    }

    // Шаг 2: Безопасно получаем кадр с помощью мьютекса
    if (xSemaphoreTake(xCameraMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        fb = esp_camera_fb_get();
        xSemaphoreGive(xCameraMutex);
    }
    else
    {
        // Не удалось получить доступ к камере, она может быть занята
        request->send(503, "text/plain", "Camera is busy, try again");
        // Выключаем камеру, если мы ее включили
        if (!stream_was_active && get_stream_clients_count() == 0)
        {
            xEventGroupClearBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT);
        }
        return;
    }

    // Шаг 3: Выключаем камеру, если мы ее включили и больше нет "зрителей"
    if (!stream_was_active)
    {
        if (get_stream_clients_count() == 0)
        {
            xEventGroupClearBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT);
        }
    }

    // Шаг 4: Отправляем кадр и безопасно возвращаем буфер
    if (!fb)
    {
        request->send(503, "text/plain", "Failed to get frame");
    }
    else
    {
        request->send_P(200, "image/jpeg", (const uint8_t *)fb->buf, fb->len);

        // Безопасно возвращаем буфер
        if (xSemaphoreTake(xCameraMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            esp_camera_fb_return(fb);
            xSemaphoreGive(xCameraMutex);
        }
    }
}

void init_web_server()
{
    init_websockets(server);

    server.on("/api/settings", HTTP_GET, handle_get_settings);
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request)
              { request->send(200); }, NULL, handle_post_settings);
    server.on("/api/action", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handle_api_action);
    server.on("/api/snapshot", HTTP_GET, handle_snapshot);

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("max-age=600");
    server.onNotFound(onNotFound);

    server.begin();
    Serial.println("Web server started");
}
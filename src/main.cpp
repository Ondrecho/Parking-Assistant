#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>

// --- Конфигурация ---
const char *SSID = "ESP32-Parking-Cam";
const char *PASSWORD = "12345678";
#define SETTINGS_FILE "/settings.json"
#define SENSORS_UPDATE_INTERVAL_MS 150
#define STREAM_FRAME_INTERVAL_MS 100 // ~10 FPS

// --- Глобальные объекты ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
StaticJsonDocument<1024> settings;

// --- Прототипы функций ---
void generateSensorData(void *parameter);
void notifyClients();
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
JsonObject getDefaultSettings();
void loadSettings();
void saveSettings();

// --- Переменные состояния ---
TaskHandle_t sensorTaskHandle = NULL;
float sensorDistances[3] = {2.5f, 2.5f, 2.5f};
bool streamActive = true;

// =================================================================
// SETUP - Инициализация
// =================================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n[SERVER] Initializing...");

    if (!LittleFS.begin(true)) {
        Serial.println("[ERROR] LittleFS Mount Failed. Halting.");
        while(1) delay(1000);
    }
    Serial.println("[INFO] LittleFS Mounted.");

    loadSettings();

    WiFi.softAP(SSID, PASSWORD);
    Serial.print("[WIFI] AP IP address: ");
    Serial.println(WiFi.softAPIP());

    ws.onEvent(onEvent);
    server.addHandler(&ws);

    // --- НАСТРОЙКА МАРШРУТОВ (ИСПРАВЛЕННАЯ СЕКЦИЯ) ---
    
    // 1. Обслуживание ВСЕЙ статики
    // Маршрут: / соответствует корню LittleFS.
    // .setDefaultFile("index.html") автоматически отдаст index.html при запросе /
    // Этот обработчик неблокирующий и сам разрулит все 404 для файлов.
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    
    // 2. Обработчики API 
    
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        String response;
        serializeJson(settings, response);
        request->send(200, "application/json", response);
    });

    AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/api/settings", [](AsyncWebServerRequest *request, JsonVariant &json) {
        settings.clear();
        settings = json.as<JsonObject>();
        saveSettings();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    server.addHandler(handler);

    server.on("/api/action", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        StaticJsonDocument<96> doc;
        // Проверяем, что есть данные для десериализации
        if (len > 0) {
            deserializeJson(doc, (const char*)data);
            String action = doc["action"];
            
            if (action == "toggleStream") {
                streamActive = !streamActive;
                settings["stream_active"] = streamActive;
                Serial.printf("[ACTION] Stream toggled: %s\n", streamActive ? "ON" : "OFF");
            } else if (action == "toggleMute") {
                settings["is_muted"] = !settings["is_muted"].as<bool>();
                Serial.printf("[ACTION] Mute toggled: %s\n", settings["is_muted"].as<bool>() ? "ON" : "OFF");
            }
            saveSettings(); 
        }
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    server.on("/api/snapshot", HTTP_GET, [](AsyncWebServerRequest *request){
        // request->send(LittleFS, "/assets/XGA.jpg", "image/jpeg");
        // Явно проверяем, чтобы не попасть в рекурсивный 404, если файл не найден
        if (LittleFS.exists("/assets/XGA.jpg")) {
            request->send(LittleFS, "/assets/XGA.jpg", "image/jpeg");
        } else {
             request->send(404, "text/plain", "Snapshot image not found");
        }
    });
    
    // 3. Видеопоток (без изменений, с исправлением WDT от прошлой итерации)
    server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponseStream("multipart/x-mixed-replace; boundary=--frame");
        response->addHeader("Access-Control-Allow-Origin", "*");
        
        File frameFile = LittleFS.open("/assets/XGA.jpg", "r");
        if (!frameFile) {
            Serial.println("[ERROR] /assets/XGA.jpg not found for stream.");
            request->send(404);
            return;
        }
        size_t fileSize = frameFile.size();
        uint8_t* frameBuffer = (uint8_t*)malloc(fileSize);
        if (!frameBuffer) {
            frameFile.close();
            Serial.println("[ERROR] Not enough memory for frame buffer.");
            request->send(500, "text/plain", "Not enough memory");
            return;
        }
        frameFile.read(frameBuffer, fileSize);
        frameFile.close();

        while (true) {
            if (!request->client()->connected()) {
                Serial.println("[STREAM] Client disconnected.");
                break;
            }

            if (streamActive) {
                String header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + String(fileSize) + "\r\n\r\n";
                // Используем static_cast для асинхронной записи
                static_cast<AsyncResponseStream*>(response)->write((const uint8_t*)header.c_str(), header.length());
                static_cast<AsyncResponseStream*>(response)->write(frameBuffer, fileSize);
                static_cast<AsyncResponseStream*>(response)->write((const uint8_t*)"\r\n", 2);
            }
            
            // Задержка потока
            vTaskDelay(STREAM_FRAME_INTERVAL_MS / portTICK_PERIOD_MS);
        }
        
        free(frameBuffer);
        request->send(response);
    });

    // 4. Оставшийся 404 обработчик (только для API, которые не были найдены)
    server.onNotFound([](AsyncWebServerRequest *request) {
        Serial.printf("[404] URL Not Found: %s\n", request->url().c_str());
        request->send(404, "text/plain", "Not Found (API or unhandled URL)");
    });

    server.begin();
    Serial.println("[SERVER] HTTP server started.");

    xTaskCreate(
        generateSensorData, "SensorDataGenerator", 4096, NULL, 1, &sensorTaskHandle
    );
}

// ... (остальные функции loadSettings, saveSettings и т.д. без изменений)

// =================================================================
// LOOP - Основной цикл
// =================================================================
void loop() {
    ws.cleanupClients();
}

// =================================================================
// Функции
// =================================================================
void generateSensorData(void *parameter) {
    for (;;) { 
        sensorDistances[0] = 2.0f + (sin(millis() / 1000.0f) * 1.8f);
        sensorDistances[1] = 1.5f + (cos(millis() / 700.0f) * 1.45f);
        sensorDistances[2] = 2.0f + (sin(millis() / 1200.0f) * 1.9f);

        for (int i=0; i<3; i++) {
            if (sensorDistances[i] < 0.1) sensorDistances[i] = 0.1;
        }
        
        if (ws.count() > 0) {
            notifyClients();
        }
        
        vTaskDelay(SENSORS_UPDATE_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

void notifyClients() {
    StaticJsonDocument<100> doc;
    JsonArray sensors = doc.createNestedArray("sensors");
    sensors.add(sensorDistances[0]);
    sensors.add(sensorDistances[1]);
    sensors.add(sensorDistances[2]);
    
    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] WebSocket client #%u disconnected\n", client->id());
            break;
        default:
            break;
    }
}

JsonObject getDefaultSettings() {
    StaticJsonDocument<1024> doc; 
    JsonObject root = doc.to<JsonObject>();
    root["thresh_yellow"] = 2.0; root["thresh_orange"] = 1.0; root["thresh_red"] = 0.5;
    root["bpm_min"] = 0; root["bpm_max"] = 300; root["auto_start"] = true;
    root["show_grid"] = true; root["cam_angle"] = 45; 
    root["grid_opacity"] = 0.8;
    root["grid_offset_x"] = 0; root["grid_offset_y"] = 0; root["grid_offset_z"] = 0;
    root["resolution"] = "XGA"; root["jpeg_quality"] = 12;
    root["flip_h"] = true; root["flip_v"] = false; 
    root["is_muted"] = false; root["stream_active"] = true;
    root["wifi_ssid"] = "ESP32_AP"; root["wifi_pass"] = "12345678";
    return root;
}

void loadSettings() {
    if (LittleFS.exists(SETTINGS_FILE)) {
        File file = LittleFS.open(SETTINGS_FILE, "r");
        if (file) {
            DeserializationError error = deserializeJson(settings, file);
            if (error) {
                Serial.println("[ERROR] Failed to read settings, using defaults.");
                settings = getDefaultSettings();
            } else {
                Serial.println("[SETTINGS] Loaded settings from file.");
            }
            file.close();
        }
    } else {
        Serial.println("[SETTINGS] No settings file found, creating defaults.");
        settings = getDefaultSettings(); 
        saveSettings();
    }
    streamActive = settings["stream_active"];
}

void saveSettings() {
    File file = LittleFS.open(SETTINGS_FILE, "w");
    if (!file) {
        Serial.println("[ERROR] Failed to open settings file for writing.");
        return;
    }
    if (serializeJson(settings, file) == 0) {
        Serial.println("[ERROR] Failed to write settings to file.");
    } else {
        Serial.println("[SETTINGS] Settings saved to file.");
    }
    file.close();
    streamActive = settings["stream_active"];
}
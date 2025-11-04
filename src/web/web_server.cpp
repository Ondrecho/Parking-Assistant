#include "web_server.h"
#include "ESPAsyncWebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h> // Подключаем заранее для будущего API
#include "state.h"
#include "config.h"

// --- Глобальные объекты сервера ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// --- Обработчики ---

// Обработчик событий WebSocket
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WS client #%u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    // Пока не обрабатываем входящие сообщения
  }
}

// Обработчик для несуществующих страниц
void onNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

// --- Инициализация ---
void init_web_server() {
  // 1. Подключаем обработчик WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // 2. Настраиваем раздачу статических файлов из LittleFS
  // Все запросы, которые не совпали с другими обработчиками,
  // будут искаться в файловой системе.
  server.serveStatic("/", LittleFS, "/")
    .setDefaultFile("index.html")
    .setCacheControl("max-age=600"); // Кэшируем статику на 10 минут

  // 3. API эндпоинты (будут добавлены позже)
  // server.on("/api/settings", HTTP_GET, ...);
  // server.on("/api/settings", HTTP_POST, ...);
  // server.on("/stream", HTTP_GET, ...);

  // 4. Обработчик 404
  server.onNotFound(onNotFound);

  // 5. Запуск сервера
  server.begin();
  Serial.println("Web server started");
}
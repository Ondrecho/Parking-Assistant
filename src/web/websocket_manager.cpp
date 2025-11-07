#include "websocket_manager.h"
#include <vector>
#include "state.h"

static AsyncWebSocket ws("/ws");
static AsyncWebSocket ws_stream("/ws_stream");

SemaphoreHandle_t xWsMutex = xSemaphoreCreateMutex();

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WS client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WS client #%u disconnected\n", client->id());
    }
}

void onWsStreamEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("Stream client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        if (xSemaphoreTake(xWsMutex, portMAX_DELAY) == pdTRUE) {
            if (ws_stream.count() == 1) {
                xEventGroupSetBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT);
            }
            xSemaphoreGive(xWsMutex);
        }
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("Stream client #%u disconnected\n", client->id());
        if (xSemaphoreTake(xWsMutex, portMAX_DELAY) == pdTRUE) {
            if (ws_stream.count() == 0) {
                xEventGroupClearBits(xAppEventGroup, CAM_STREAM_REQUEST_BIT);
            }
            xSemaphoreGive(xWsMutex);
        }
    }
}

void init_websockets(AsyncWebServer& server) {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    ws_stream.onEvent(onWsStreamEvent);
    server.addHandler(&ws_stream);
}

int get_ws_clients_count() {
    return ws.count();
}

int get_stream_clients_count() {
    return ws_stream.count();
}

void broadcast_ws_json(JsonDocument& doc) {
    String buffer;
    serializeJson(doc, buffer);
    ws.textAll(buffer);
}

bool is_stream_writable() {
    return ws_stream.availableForWriteAll();
}

void broadcast_ws_stream(const uint8_t* data, size_t len) {
    ws_stream.binaryAll(reinterpret_cast<const char*>(data), len);
}

void broadcast_sensors_task(void *pvParameters) {
    (void)pvParameters;
    DynamicJsonDocument doc(128);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));

        if (get_ws_clients_count() == 0) continue;

        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            JsonArray sensors = doc.createNestedArray("sensors");
            for (int i = 0; i < NUM_SENSORS; ++i) {
                sensors.add(g_app_state.sensor_distances[i]);
            }
            xSemaphoreGive(xStateMutex);

            broadcast_ws_json(doc);
            doc.clear();
        }
    }
}
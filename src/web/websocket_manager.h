#pragma once
#include "ESPAsyncWebServer.h"
#include <ArduinoJson.h>

void init_websockets(AsyncWebServer& server);

int get_ws_clients_count();
int get_stream_clients_count();
bool is_stream_writable();

void broadcast_ws_json(JsonDocument& doc);
void broadcast_ws_stream(const uint8_t* data, size_t len);

void broadcast_sensors_task(void *pvParameters);
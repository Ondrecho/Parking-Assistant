#pragma once
#include <ArduinoJson.h>
#include <cstddef>

void init_web_server();
int get_ws_clients_count();
void broadcast_ws_json(JsonDocument& doc);
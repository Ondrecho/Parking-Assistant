#pragma once
#include <cstddef>

// Инициализация веб-сервера
void init_web_server();

// Получить количество активных WS клиентов
int get_ws_clients_count();

// Отправить текстовое сообщение всем WS клиентам
void broadcast_ws_text(const char* data, size_t len);
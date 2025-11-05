#pragma once

// Инициализация настроек: загрузка из файла или установка значений по умолчанию.
void settings_init();

// Сохранение текущих настроек из g_app_state в файл.
bool settings_save();
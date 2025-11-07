#pragma once

void settings_init();
bool settings_save();
void settings_reset_to_default(); 
void settings_save_task(void *pvParameters); 
#include "camera_task.h"
#include <Arduino.h>

// Задача для управления камерой.
// Пока это просто заглушка, чтобы проект компилировался.
// Логика будет добавлена на следующих этапах.
void camera_task(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    // Ждем, ничего не делая
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
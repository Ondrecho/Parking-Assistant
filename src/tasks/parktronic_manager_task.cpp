#include "parktronic_manager_task.h"
#include <Arduino.h>
#include "state.h"
#include "config.h"

const unsigned long GRACE_PERIOD_MS = 15000; 

void parktronic_manager_task(void *pvParameters) {
    (void)pvParameters;

    pinMode(REVERSE_GEAR_PIN, INPUT_PULLUP);
    pinMode(SENSORS_POWER_PIN, OUTPUT);
    digitalWrite(SENSORS_POWER_PIN, LOW); 

    unsigned long last_reverse_active_time = 0;
    bool current_state_is_active = false;

    Serial.println("Parktronic Manager task started");

    for (;;) {
        bool is_reverse_gear_on = (digitalRead(REVERSE_GEAR_PIN) == LOW);
        bool auto_start_enabled = false;
        bool manually_activated = false;
        
        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
            auto_start_enabled = g_app_state.settings.auto_start;
            manually_activated = g_app_state.is_manually_activated;
            xSemaphoreGive(xStateMutex);
        }

        bool should_be_active_now = (is_reverse_gear_on && auto_start_enabled) || manually_activated;

        if (is_reverse_gear_on) {
            last_reverse_active_time = millis();
        }

        bool final_decision_is_active = should_be_active_now || (millis() - last_reverse_active_time < GRACE_PERIOD_MS);

        if (final_decision_is_active != current_state_is_active) {
            current_state_is_active = final_decision_is_active;

            if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
                g_app_state.is_parktronic_active = current_state_is_active;
                xSemaphoreGive(xStateMutex);
            }

            if (current_state_is_active) {
                Serial.println("[Parktronic] Activating...");
                digitalWrite(SENSORS_POWER_PIN, HIGH);
                xEventGroupSetBits(xAppEventGroup, PARKTRONIC_ACTIVE_BIT);
            } else {
                Serial.println("[Parktronic] Deactivating...");
                digitalWrite(SENSORS_POWER_PIN, LOW);
                xEventGroupClearBits(xAppEventGroup, PARKTRONIC_ACTIVE_BIT);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}
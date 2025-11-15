#include "buzzer_task.h"
#include <Arduino.h>
#include "state.h"
#include "config.h"

const int BUZZER_LEDC_CHANNEL = 1;

void buzzer_task(void *pvParameters) {
    (void)pvParameters;

    ledcSetup(BUZZER_LEDC_CHANNEL, 1000, 8); 
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);

    Serial.println("Buzzer task started");

    for (;;) {
        xEventGroupWaitBits(xAppEventGroup, PARKTRONIC_ACTIVE_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        while (xEventGroupGetBits(xAppEventGroup) & PARKTRONIC_ACTIVE_BIT) {
            
            float min_dist = 999.0;
            bool is_muted = false;
            int vol = 100, tone = 1760, r = 50, o = 100, y = 200, min_bpm = 0, max_bpm = 300;

            if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                for (int i = 0; i < NUM_SENSORS; i++) {
                    if (g_app_state.sensor_distances[i] < min_dist) {
                        min_dist = g_app_state.sensor_distances[i];
                    }
                }
                is_muted = g_app_state.is_muted;
                vol = g_app_state.settings.volume;
                tone = g_app_state.settings.buzzer_tone_hz;
                r = g_app_state.settings.thresh_red;
                o = g_app_state.settings.thresh_orange;
                y = g_app_state.settings.thresh_yellow;
                min_bpm = g_app_state.settings.bpm_min;
                max_bpm = g_app_state.settings.bpm_max;
                xSemaphoreGive(xStateMutex);
            }

            if (is_muted || min_dist > y) {
                ledcWrite(BUZZER_LEDC_CHANNEL, 0); 
                vTaskDelay(pdMS_TO_TICKS(100)); 
                continue;
            }

            long delay_ms = 500;
            long beep_duration_ms = 100;

            if (min_dist <= r) { 
                delay_ms = 0;
                beep_duration_ms = 1000;
            } else { 
                long current_bpm = map(min_dist, y, r, min_bpm, max_bpm);
                current_bpm = constrain(current_bpm, min_bpm, max_bpm);
                if (current_bpm > 0) {
                    delay_ms = 60000 / current_bpm - beep_duration_ms;
                    if (delay_ms < 0) delay_ms = 0;
                }
            }

            int duty_cycle = map(vol, 0, 100, 0, 128); 
            ledcChangeFrequency(BUZZER_LEDC_CHANNEL, tone, 8);
            ledcWrite(BUZZER_LEDC_CHANNEL, duty_cycle);
            vTaskDelay(pdMS_TO_TICKS(beep_duration_ms));

            ledcWrite(BUZZER_LEDC_CHANNEL, 0); 
            if (delay_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
            }
        }
        ledcWrite(BUZZER_LEDC_CHANNEL, 0); 
    }
}
#include "frame_grab_task.h"
#include "state.h"
#include "esp_camera.h"
#include "freertos/queue.h"

// Внешние переменные, к которым задача имеет доступ
extern EventGroupHandle_t xAppEventGroup;
extern SemaphoreHandle_t xCameraMutex;
extern QueueHandle_t xFrameQueue;

void frame_grab_task(void *pvParameters) {
    (void)pvParameters;

    for (;;) {
        // Ждем, пока камера не будет полностью инициализирована
        xEventGroupWaitBits(xAppEventGroup, CAM_INITIALIZED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        camera_fb_t *fb = NULL;

        // Безопасно получаем кадр, используя мьютекс
        if (xSemaphoreTake(xCameraMutex, portMAX_DELAY) == pdTRUE) {
            // Повторно проверяем состояние, чтобы избежать гонки
            if (xEventGroupGetBits(xAppEventGroup) & CAM_INITIALIZED_BIT) {
                fb = esp_camera_fb_get();
            }
            xSemaphoreGive(xCameraMutex);
        }

        if (fb) {
            // Отправляем указатель на кадр в очередь.
            // xQueueOverwrite гарантирует, что в очереди всегда будет самый свежий кадр,
            // предотвращая ее переполнение, если stream_task не успевает.
            xQueueOverwrite(xFrameQueue, &fb);
        } else {
            // Если кадр не получен, делаем небольшую паузу
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
    printf("Starting ESP32C6\n");

    while(1) {
        printf("Hello from C6 Finance Hub!\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

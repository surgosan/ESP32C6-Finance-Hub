//
// Created by sergy on 1/1/2025.
//
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

_Noreturn void app_main(void) {
    while (true) {
        printf("Hello from C6 Finance Hub\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
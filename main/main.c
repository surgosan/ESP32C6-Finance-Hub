#include <stdio.h>
#include <string.h>
#include <driver/spi_master.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "TFT_eSPI.h"
#include "src/drivers/display/ili9341/lv_ili9341.h"
#include "src/drivers/display/lcd/lv_lcd_generic_mipi.h"
#include "src/misc/lv_fs.h"

#define BL 15 //Manually turn off/on when needed
#define SCK 4
#define MISO 5
#define MOSI 6
#define CS 23
#define RST 22
#define DC 21

TFT_eSPI tft = TFT_eSPI();

int32_t my_lcd_send_cmd(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size, const uint8_t *param, size_t param_size) {
    tft.startWrite();
    tft.writeCommand(*cmd);  // Send command
    for (size_t i = 0; i < param_size; i++) {
        tft.write(param[i]);  // Send parameters
    }
    tft.endWrite();
    return 0;  // Return 0 if successful
}

int32_t my_lcd_send_color(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size, uint8_t *param, size_t param_size) {
    tft.startWrite();
    tft.writeCommand(*cmd);  // Send command (usually for writing to RAM)
    tft.pushColors((uint16_t *)param, param_size / 2, true);  // Send color data
    tft.endWrite();
    return 0;  // Return 0 if successful
}


lv_display_t * lv_ili9341_create(
        uint32_t hor_res,
        uint32_t ver_res,
        lv_lcd_flag_t flags,
        lv_ili9341_send_cmd_cb_t send_cmd_cb,
        lv_ili9341_send_color_cb_t send_color_cb
);

lv_display_t *display = lv_ili9341_create(240, 320, 0, my_lcd_send_cmd, my_lcd_send_color);


void my_flush_cb(lv_fs_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area));
    tft.pushColors((uint16_t*)&color_p->full, lv_area_get_width(area) * lv_area_get_height(area), true);
    tft.endWrite();
    lv_disp_flush_ready((lv_display_t *) disp_drv);  // Notify LVGL that flush is done
}

// Register the flush_cb
lv_fs_drv_t disp_drv;
lv_fs_drv_init(&disp_drv);
lv_disp_drv_register(&disp_drv);



void lv_tick_task(void *arg) {
    while (1) {
        lv_tick_inc(1);
        vTaskDelay(pdMS_TO_TICKS(1)); // Call this every 1 ms
    }
}


void app_main(void) {
    // Initialize LVGL
    lv_init();

    // Initialize TFT driver
    tft.begin();
    tft.setRotation(1);

    // Create a display buffer
    static lv_draw_buf_t draw_buf;
    static lv_color_t buf[240 * 10];  // Size of the buffer
    lv_draw_buf_init(&draw_buf, 320, 240, (lv_color_format_t) buf, 0, NULL, 320 * 10);


    // Create a "Hello World" label
    lv_obj_t *label = lv_label_create(lv_scr_act());  // Create a label on the active screen
    lv_label_set_text(label, "Hello World!");         // Set label text
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);       // Align the label to the center

    xTaskCreate(lv_tick_task, "lv_tick_task", 2048, NULL, 10, NULL);



    while (1) {
        lv_timer_handler();  // Handle the LVGL tasks
        vTaskDelay(pdMS_TO_TICKS(10)); // Call this every 10 ms
    }
}
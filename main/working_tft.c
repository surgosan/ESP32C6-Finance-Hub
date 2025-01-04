#include <stdio.h>
#include <esp_timer.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_ili9341.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_http_client.h"
#include "cJSON.h"
//#include "ui.h"

#define BL 15
#define SCK 6
#define MISO 4
#define MOSI 5
#define CS 23
#define RST 22
#define DC 21

// Declare buffer as a global variable
static uint16_t buffer[320 * 40];
static esp_lcd_panel_handle_t panel_handle;

static lv_obj_t *counter_label;
static int counter = 0;
// ------------------------------------------  Filling Screen  ------------------------------------------
/*
static void fillScreen(esp_lcd_panel_handle_t panel_handle, uint16_t color) {
    // Fill the buffer with red
    for (int i = 0; i < 320 * 40; i++) {
        buffer[i] = __builtin_bswap16(color);
    }

    // Draw screen in chunks
    for (int y = 0; y < 320; y += 40) { // Draw in chunks
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 320, y + 40, buffer));
    }
}
*/
// ------------------------------------------ LVGL Functions ------------------------------------------
uint32_t lv_tick_get_cb(void) {
    return esp_timer_get_time() / 1000;
}

void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, (uint16_t *)px_map));
    lv_display_flush_ready(display); // Notify LVGL the flushing is done
}

static void counter_update_cb() {
    char timer_buffer[16];
    snprintf(timer_buffer, sizeof(timer_buffer), "Count: %d", counter++);
    lv_label_set_text(counter_label, timer_buffer);
}

_Noreturn void lvgl_task() {
    while(true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void app_main(void) {
    printf("Starting Application\n");
// ------------------------------------------  SPI Bus  ------------------------------------------
    spi_bus_config_t busConfig = {
            .sclk_io_num = SCK,
            .mosi_io_num = MOSI,
            .miso_io_num = MISO,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 320 * 40 * sizeof(uint16_t)
    };
    spi_bus_free(SPI2_HOST);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO));
    printf("SPI Init\n");
// ------------------------------------------  Display Config  ------------------------------------------
    esp_lcd_panel_io_spi_config_t io_config = {
            .dc_gpio_num = DC,
            .cs_gpio_num = CS,
            .pclk_hz = 64 * 1000 * 1000,
            .spi_mode = 0,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .trans_queue_depth = 10,
            .on_color_trans_done = NULL,
            .user_ctx = NULL,
            .flags = {
                    .dc_low_on_data = 0,
                    .dc_low_on_param = 0
            }
    };
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));
    printf("Display Config\n");
// ------------------------------------------  Panel Config  ------------------------------------------
    esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = RST,
            .rgb_ele_order = ESP_LCD_COLOR_SPACE_RGB,
            .bits_per_pixel = 16
    };
//    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
    printf("Panel Config\n");
// ------------------------------------------  Backlight config  ------------------------------------------
    gpio_config_t bl_gpio_config = {
            .pin_bit_mask = (1ULL << BL),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&bl_gpio_config);
    gpio_set_level(BL, 1);
    printf("BL Set\n");

// ------------------------------------------  Resetting TFT  ------------------------------------------
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    printf("TFT Reset\n");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    printf("TFT Init\n");
// -----------------------------------  Set the display orientation ------------------------------------
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true)); // True = Landscape
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));
    printf("Orientation Set\n");
// ----------------------------------  Ensuring display is turned on  ----------------------------------
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    vTaskDelay(pdMS_TO_TICKS(100));


// --------------------------------------------  LVGL  --------------------------------------------
    printf("Starting LVGL\n");
    lv_init();
    lv_tick_set_cb(lv_tick_get_cb);
    printf("Init and tick set\n");

    // Creating LVGL display
    lv_display_t *display = lv_display_create(320, 240);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    printf("Dsiplay set\n");
    // Set LVGL Buffers (1 buffer)
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    printf("Buffers set\n");
    // Set LVGL flush callback
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    printf("Flush set\n");

    lv_obj_set_size(lv_screen_active(), 320, 240); // Explicit size
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_180);
    // Set background to red
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);


//    ui_init();

    // Create Hello World Label
    lv_obj_t *label = lv_label_create(lv_screen_active());
    printf("Label Created\n");
    lv_label_set_text(label, "Hello World\n");
    printf("Text Set\n");
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), LV_PART_MAIN);
    printf("Set Color to black\n");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 40);
    printf("Text Aligned\n");

    // Create counter label
    counter_label = lv_label_create(lv_screen_active());
    lv_label_set_text(counter_label, "Count: 0");
    lv_obj_set_style_text_color(counter_label, lv_color_hex(0xffffff), LV_PART_MAIN);


    lv_timer_create(counter_update_cb, 1000, NULL);

    printf("Time since startup: %lu\n", lv_tick_get_cb());
// ---------------------------------------------  RUNNING ---------------------------------------------
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 8192, NULL, 1, NULL, 0);
    printf("Created Task\n");
}
#include <stdio.h>
#include <esp_timer.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "cJSON.h"
#include "lvgl.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "esp_lcd_ili9341.h"
#include "esp_spiffs.h"
#include "gifdec.h"
#include <fcntl.h>
#include "unistd.h"

#define FRAME_DELAY_MS 100

#define BL 15
#define SCK 6
#define MISO 4
#define MOSI 5
#define CS 23
#define RST 22
#define DC 21

// Declare buffer as a global variable
static uint16_t buffer[240 * 20];
static uint16_t buffer2[240 * 20];
static esp_lcd_panel_handle_t panel_handle;

void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = true
    };

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
}

// ------------------------------------------ LVGL Objects ------------------------------------------
static lv_obj_t *home;

void display_gif(const char *file_path, lv_obj_t *img_obj) {
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE("GIF", "Failed to open GIF file: %s", file_path);
        return;
    }

    gd_GIF *gif = gd_open_gif((const char *) fd);
    if (!gif) {
        ESP_LOGE("GIF", "Failed to decode GIF");
        close(fd);
        return;
    }

    // Prepare LVGL image descriptor
    static lv_img_dsc_t img_dsc;
    img_dsc.header.w = gif->width;
    img_dsc.header.h = gif->height;
    img_dsc.data_size = gif->width * gif->height * 2; // RGB565
    img_dsc.data = malloc(img_dsc.data_size);

    uint16_t *framebuffer = (uint16_t *)img_dsc.data;

    while (1) {
        if (!gd_get_frame(gif)) break;

        // Convert RGB888 from gifdec to RGB565 for the framebuffer
        for (int i = 0; i < gif->width * gif->height; i++) {
            uint8_t *rgb888 = &gif->frame[i * 3];
            framebuffer[i] = ((rgb888[0] >> 3) << 11) |  // Red
                             ((rgb888[1] >> 2) << 5)  |  // Green
                             (rgb888[2] >> 3);          // Blue
        }

        // Update the LVGL image widget
        lv_img_set_src(img_obj, &img_dsc);
        lv_obj_invalidate(img_obj); // Mark the image object as dirty to refresh

        // Wait for the frame delay
        vTaskDelay(gif->gce.delay * 10 / portTICK_PERIOD_MS);

        // Handle loop count
//        if (gif->loop_count && gif->loop_count == gif->loop) break;
    }

    // Cleanup
    free(img_dsc.data);
    gd_close_gif(gif);
    close(fd);
}
// ------------------------------------------ LVGL Functions ------------------------------------------

// Gets the amount of time since system startup in ms
uint32_t lv_tick_get_cb(void) {
    return esp_timer_get_time() / 1000;
}

// Draws the screen
void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, (uint16_t *)px_map));
    lv_display_flush_ready(display); // Notify LVGL the flushing is done
}

// Main LVGL task that will run indefinitely (Like void loop() in arduino)
_Noreturn void lvgl_task() {
    while(true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

// Main application setup
void app_main(void) {
    printf("Starting Application\n");

    init_spiffs();
// ------------------------------------------  SPI Bus  ------------------------------------------
    // Config the SPI bus
    spi_bus_config_t busConfig = {
            .sclk_io_num = SCK,
            .mosi_io_num = MOSI,
            .miso_io_num = MISO,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 240 * 20 * sizeof(uint16_t)
    };
    // Ensure the SPI bus is free
    spi_bus_free(SPI2_HOST);
    // Init the SPI bus and respond to errors.
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO));
// ------------------------------------------  Display Config  ------------------------------------------

    // Config lcd panel
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
    // Create the handle and set to empty
    esp_lcd_panel_io_handle_t io_handle = NULL;

    // Init the lcd with the empty handle & config
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));
// ------------------------------------------  Panel Config  ------------------------------------------

    // Config panel data
    esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = RST,
            .rgb_ele_order = ESP_LCD_COLOR_SPACE_RGB,
            .bits_per_pixel = 16
    };
    // Leave this commented. I did this at the beginning of this file to have it global
//    esp_lcd_panel_handle_t panel_handle = NULL;

    // Init the panel with the empty handle & config
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
// ------------------------------------------  Backlight config  ------------------------------------------

    // Define gpio for BL
    gpio_config_t bl_gpio_config = {
            .pin_bit_mask = (1ULL << BL),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
    };
    // Accept config
    gpio_config(&bl_gpio_config);
    // Set gpio to digital 1 (3.3V)
    gpio_set_level(BL, 1);

// ------------------------------------------  Resetting TFT  ------------------------------------------
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

// -----------------------------------  Set the display orientation ------------------------------------
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true)); // True = Landscape
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));

// ----------------------------------  Ensuring display is turned on  ----------------------------------
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    vTaskDelay(pdMS_TO_TICKS(100));

// --------------------------------------------  LVGL  --------------------------------------------
    // Mandatory function. LVGL functions will not work without this
    lv_init();
    // Set tick callback
    lv_tick_set_cb(lv_tick_get_cb);
    // Creating LVGL display
    lv_display_t *display = lv_display_create(320, 240);
    // Define screen color format
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    // Set LVGL Buffers (2 buffers for smooth and consistent display)
    lv_display_set_buffers(display, buffer, buffer2, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    // Set LVGL draw (flush) callback
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    //Define screen size
    lv_obj_set_size(lv_screen_active(), 320, 240);
    // Define rotation
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_180);
    // Create home screen
    home = lv_obj_create(NULL);
    // Load Home screen
    lv_screen_load(home);
    // Set background to black
    lv_obj_set_style_bg_color(home, lv_color_hex(0x000000), LV_PART_MAIN);

    // Load GIF on screen
    lv_obj_t *img_obj = lv_img_create(home);
    lv_obj_align(img_obj, LV_ALIGN_CENTER, 0, 0);
    display_gif("/spiffs/ouiaiu.gif", img_obj);

    // O U I I A U Text
    lv_obj_t *left_eye = lv_label_create(home);
    lv_label_set_text(left_eye, LV_SYMBOL_EYE_OPEN "_" LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_color(left_eye, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(left_eye, LV_ALIGN_TOP_LEFT, 10, 10);

    // O U I I A U Text
    lv_obj_t *totalBalance = lv_label_create(home);
    lv_label_set_text(totalBalance, "O U I I A U");
    lv_obj_set_style_text_color(totalBalance, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(totalBalance, LV_ALIGN_TOP_MID, 0, 10);

    // O U I I A U Text
    lv_obj_t *right_eye = lv_label_create(home);
    lv_label_set_text(right_eye, LV_SYMBOL_EYE_OPEN "_" LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_color(right_eye, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(right_eye, LV_ALIGN_TOP_RIGHT, -10, 10);

    // Call lvgl_task to run indefinitely
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 8192, NULL, 1, NULL, 0);

    // From here, the _Noreturn void lvgl_task() will run until the system is powered off.
}
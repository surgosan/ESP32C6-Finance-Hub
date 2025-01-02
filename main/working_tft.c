#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_ili9341.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define BL 15
#define SCK 6
#define MISO 4
#define MOSI 5
#define CS 23
#define RST 22
#define DC 21

// Declare buffer as a global variable
static uint16_t color_buffer[320 * 40];

static void fillScreen(esp_lcd_panel_handle_t panel_handle, uint16_t color) {
// ------------------------------------------  Filling Screen  ------------------------------------------
    // Fill the buffer with red
    for (int i = 0; i < 320 * 40; i++) {
        color_buffer[i] = __builtin_bswap16(color);
    }

    // Draw screen in chunks
    for (int y = 0; y < 320; y += 40) { // Draw in chunks
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 320, y + 40, color_buffer));
    }
}



_Noreturn void app_main(void) {
// ------------------------------------------  SPI Bus  ------------------------------------------
    spi_bus_config_t busConfig = {
            .sclk_io_num = SCK,
            .mosi_io_num = MOSI,
            .miso_io_num = MISO,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 240 * 20 * sizeof(uint16_t)
    };
    spi_bus_free(SPI2_HOST);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO));
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

// ------------------------------------------  Panel Config  ------------------------------------------
    esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = RST,
            .rgb_ele_order = ESP_LCD_COLOR_SPACE_BGR,
            .bits_per_pixel = 16
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

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

// ------------------------------------------  Resetting TFT  ------------------------------------------
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

// -----------------------------------  Set the display orientation ------------------------------------
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));

    // Init colors array
    uint16_t color_array[6] = {0xf800, 0x07e0, 0x001f, 0xffe0, 0x07ff, 0xf81f};
    uint8_t currentColor = 0;

// ----------------------------------  Ensuring display is turned on  ----------------------------------
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    while (true) {
        switch (currentColor) {
            case 0:
                printf("Red!\n");
                break;
            case 1:
                printf("Green!\n");
                break;
            case 2:
                printf("Blue!\n");
                break;
            case 3:
                printf("Yellow!\n");
                break;
            case 4:
                printf("Cyan!\n");
                break;
            case 5:
                printf("Magenta!\n");
                break;
            default:
                currentColor = 0;
                printf("Red!\n");
                break;
        }

        fillScreen(panel_handle, color_array[currentColor]);
        currentColor++;

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
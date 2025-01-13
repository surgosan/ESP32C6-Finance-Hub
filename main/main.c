//
// Created by sergy on 1/9/2025.
//
#include <stdio.h>
#include <esp_timer.h>
#include <string.h>
#include <esp_log.h>
#include <nvs_flash.h>
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
#include "esp_wifi_connect.h"
#include "esp_http_client_handler.h"
#include "env.h"

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

// ------------------------------------------- Plaid Vars -------------------------------------------
static const char *TAG = "Plaid API";
// ------------------------------------------ LVGL Objects ------------------------------------------
static lv_obj_t *home;
static lv_obj_t *second;

static lv_obj_t *counter_label;
static int counter = 0;

static lv_obj_t *time_label;

static lv_style_t bar_style_bg;
static lv_style_t bar_style_indic;
static lv_obj_t *api_progress_label;
// -----------------------------------------  API Functions  ------------------------------------------

// Updates time label
void update_time() {
    char displayString[20];
    sprintf(displayString, "Year: 2025\n Week: %s", fetch_time());
    lv_label_set_text(time_label, displayString);
}
// ------------------------------------------ LVGL Functions ------------------------------------------
// Gets the amount of time since system startup in ms
uint32_t lv_tick_get_cb(void) { return esp_timer_get_time() / 1000; }

// Draws the screen
void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, (uint16_t *)px_map));
    lv_display_flush_ready(display); // Notify LVGL the flushing is done
}

// Controls custom count label with each second
static void counter_update_cb() {
    char timer_buffer[16];
    snprintf(timer_buffer, sizeof(timer_buffer), "Count: %d", counter++);
    lv_label_set_text(counter_label, timer_buffer);
}

static void delete_progress_bar() {
    if(api_progress_label) {
        lv_obj_delete(api_progress_label);
        api_progress_label = NULL;
    }
}

// Main LVGL task that will run indefinitely (Like void loop() in arduino)
_Noreturn void lvgl_task() {
    while(true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}



// Main application setup
void app_main(void) {
    printf("Starting Application\n");
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
// -------------------------------------------  Wi-Fi  -------------------------------------------
    // Call function to init Wi-Fi
    wifi_init();
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
    // Create second screen
    second = lv_obj_create(NULL);
    // Load Home screen
    lv_screen_load(home);
    // Set background to black
    lv_obj_set_style_bg_color(home, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(second, lv_color_hex(0x000000), LV_PART_MAIN);

    // Credit Cards Balance
    lv_obj_t *total_credit_balance_label = lv_label_create(home);
    lv_label_set_text(total_credit_balance_label, LV_SYMBOL_WIFI " Fetching Credit Accounts...");
    lv_obj_set_style_text_color(total_credit_balance_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(total_credit_balance_label, LV_ALIGN_CENTER, 0, 0);

    // Checking Accounts Balance
    lv_obj_t *total_checking_balance_label = lv_label_create(home);
    lv_label_set_text(total_checking_balance_label, LV_SYMBOL_WIFI " Fetching Checking Accounts...");
    lv_obj_set_style_text_color(total_checking_balance_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(total_checking_balance_label, LV_ALIGN_CENTER, 0, 40);

    // Fetching Progress Label Styles
    lv_style_init(&bar_style_bg);
    lv_style_set_border_color(&bar_style_bg, lv_color_hex(0xffffff));
    lv_style_set_border_width(&bar_style_bg, 2);
    lv_style_set_pad_all(&bar_style_bg, 3);
    lv_style_set_radius(&bar_style_bg, 10);
    lv_style_set_anim_duration(&bar_style_bg, 1000);

    lv_style_init(&bar_style_indic);
    lv_style_set_bg_opa(&bar_style_indic, LV_OPA_COVER);
    lv_style_set_bg_color(&bar_style_indic, lv_color_hex(0x0000ff));
    lv_style_set_radius(&bar_style_indic, 10);
    // Fetching Progress Label
    api_progress_label = lv_bar_create(home);
    lv_obj_remove_style_all(api_progress_label);
    lv_obj_add_style(api_progress_label, &bar_style_bg, 0);
    lv_obj_add_style(api_progress_label, &bar_style_indic, LV_PART_INDICATOR);

    lv_obj_set_size(api_progress_label, 150, 20);
    lv_bar_set_value(api_progress_label, 0, LV_ANIM_ON);
    lv_obj_align(api_progress_label, LV_ALIGN_TOP_MID, 0, 10);

    // Counter label
    counter_label = lv_label_create(home);
    lv_label_set_text(counter_label, "Count: 0");
    lv_obj_set_style_text_color(counter_label, lv_color_hex(0xffffff), LV_PART_MAIN);

    // Time label
    time_label = lv_label_create(home);
    lv_label_set_text(time_label, "Fetching...");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(time_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    // Create timer updater
    lv_timer_create(counter_update_cb, 1000, NULL);

    // Get current time via API
    update_time();

    // Call lvgl_task to run indefinitely
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 8192, NULL, 1, NULL, 0);


// --------------------------------------------  Plaid  --------------------------------------------
    const char* access_tokens[] = {
            AMEX_TOKEN,
            BOFA_TOKEN,
            CAPONE_TOKEN,
            "American Express",
            "Bank of America",
            "Capital One"
    };
    int token_count = sizeof(access_tokens) / sizeof(access_tokens[0]);
    cJSON* all_accounts = cJSON_CreateArray();
    double total_credit_balance = 0.0;
    double total_checking_balance = 0.0;

    // Go over each account and place it in all_accounts
    for(int i = 0; i < token_count-3; i++) {
        char* balance_response = plaid_fetch_balance(access_tokens[i], access_tokens[i + 3]);
        // Visual update on API progress
        switch(i) {
            case 0:
                lv_bar_set_value(api_progress_label, 33, LV_ANIM_ON);
                break;
            case 1:
                lv_bar_set_value(api_progress_label, 66, LV_ANIM_ON);
                break;
            case 2:
                lv_bar_set_value(api_progress_label, 100, LV_ANIM_ON);
                break;
            default:
                lv_bar_set_value(api_progress_label, 0, LV_ANIM_ON);
                break;
        }
        ESP_LOGI(TAG, "Finished %s", access_tokens[i+3]);
        if(balance_response) {
//            ESP_LOGI(TAG, "MAIN CODE: %s", balance_response);
            cJSON* institution_accounts = cJSON_Parse(balance_response);
            if(institution_accounts) {
                cJSON* account;
                cJSON_ArrayForEach(account, institution_accounts) {
                    cJSON_AddItemToArray(all_accounts, cJSON_Duplicate(account, 1)); // Copy to the new local array

                    // Get the balance & account name.
                    cJSON* current_balance = cJSON_GetObjectItem(account, "Balance");
                    cJSON* current_account = cJSON_GetObjectItem(account, "Account");

                    if(cJSON_IsString(current_account) && cJSON_IsNumber(current_balance)) {
                        // Define account name and balance as appropriate variable types
                        const char* account_name = cJSON_GetStringValue(current_account);
                        double balance_double = current_balance->valuedouble;

                        // If it is a checking account, add to checking total, else add to credit total
                        if(strcmp(account_name, "Advantage Savings") == 0 ||
                                strcmp(account_name, "Rewards Checking") == 0 ||
                                strcmp(account_name, "Adv Plus Banking") == 0) {
                            total_checking_balance += balance_double;
                        } else {
                            total_credit_balance += balance_double;
                        }
                    } else {
                        ESP_LOGW(TAG, "Invalid Response");
                    }
                }
                cJSON_Delete(institution_accounts); // Delete temp accounts to free memory
            }
            free(balance_response); // Delete response to free memory
        } else {
            ESP_LOGE(TAG, "Failed to fetch data from Plaid");
        }
    }

    char total_credit_balance_string[32];
    sprintf(total_credit_balance_string, "Credit Balance: $%.2f", total_credit_balance);
    lv_label_set_text(total_credit_balance_label, total_credit_balance_string);

    char total_checking_balance_string[32];
    sprintf(total_checking_balance_string, "Checking Balance: $%.f", total_checking_balance);
    lv_label_set_text(total_checking_balance_label, total_checking_balance_string);

    // Create a one-shot timer with a 5-second delay
    TimerHandle_t bar_deletion_timer = xTimerCreate(
            "Delete_Bar_Timer",
            pdMS_TO_TICKS(5000),
            pdFALSE,                     // One-shot timer
            NULL,
            delete_progress_bar        // Callback function
    );

    if(bar_deletion_timer != NULL) { xTimerStart(bar_deletion_timer, 0); }

    // From here, the _Noreturn void lvgl_task() will run until the system is powered off.
}
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

#define BOOT_BUTTON_PIN GPIO_NUM_9

#define BL 15
#define SCK 6
#define MISO 4
#define MOSI 5
#define CS 23
#define RST 22
#define DC 21

// ------------------------------------------- Plaid Vars -------------------------------------------
static const char *TAG = "Plaid API";

// ------------------------------------------ LVGL Objects ------------------------------------------
// Declare buffer as a global variable
static uint16_t buffer[240 * 20];
static uint16_t buffer2[240 * 20];
static esp_lcd_panel_handle_t panel_handle;

static bool data_loaded = false;

static lv_obj_t* loading_screen;
static lv_obj_t *home_page;
static lv_obj_t *accounts_page;
static lv_obj_t *transactions_page;
static uint8_t page_number = 0;

static lv_obj_t* home_button_label;
static lv_obj_t* accounts_button_label;
static lv_obj_t* transactions_button_label;

static lv_obj_t *counter_label;
static int counter = 0;

static lv_obj_t *time_label;

static lv_style_t bar_style_bg;
static lv_style_t bar_style_indic;
static lv_style_t nav_style;
static lv_style_t menu_style;
static lv_style_t menu_button_style;

static lv_obj_t *nav_bar;
static lv_obj_t *api_progress_label;
static lv_color_t deselected;
// -----------------------------------------  API Functions  ------------------------------------------

// Updates time label
void update_time() {
    char displayString[20];
    char (*time_api)[32] = fetch_time();
    vTaskDelay(pdMS_TO_TICKS(100));
    sprintf(displayString, "%s\n%s", time_api[0], time_api[1]);
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

static void clear_loading_screen() {
    lv_screen_load(home_page);
    data_loaded = true;
    lv_obj_delete(loading_screen);
}

// Main LVGL task that will run indefinitely (Like void loop() in arduino)
_Noreturn void lvgl_task() {
    while(true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}

// Page changing gpio
_Noreturn void next_page() {
    while (true) {
        int gpio_state = gpio_get_level(BOOT_BUTTON_PIN);

        if(gpio_state == 0 && data_loaded) {
            (page_number > 1) ? page_number = 0 : page_number++;

            lv_obj_set_style_text_color(home_button_label, deselected, LV_PART_MAIN);
            lv_obj_set_style_text_color(accounts_button_label, deselected, LV_PART_MAIN);
            lv_obj_set_style_text_color(transactions_button_label, deselected, LV_PART_MAIN);

            switch(page_number) {
                case 0:
                    lv_obj_set_style_text_color(home_button_label, lv_color_hex(0x000000), LV_PART_MAIN);
                    lv_screen_load_anim(home_page, LV_SCR_LOAD_ANIM_NONE, 100, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(150));
                    lv_obj_set_parent(nav_bar, home_page);
                    break;
                case 1:
                    lv_obj_set_style_text_color(accounts_button_label, lv_color_hex(0x000000), LV_PART_MAIN);
                    lv_screen_load_anim(accounts_page, LV_SCR_LOAD_ANIM_NONE, 100, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(150));
                    lv_obj_set_parent(nav_bar, accounts_page);
                    break;
                default:
                    lv_obj_set_style_text_color(transactions_button_label, lv_color_hex(0x000000), LV_PART_MAIN);
                    lv_screen_load_anim(transactions_page, LV_SCR_LOAD_ANIM_NONE, 100, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(150));
                    lv_obj_set_parent(nav_bar, transactions_page);
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Main application setup
void app_main(void) {
    printf("Starting Application\n");
// -------------------------------------------  GPIO  --------------------------------------------
    // Configure GPIO as input
    gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << BOOT_BUTTON_PIN),
            .mode = GPIO_MODE_INPUT,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
// -------------------------------------------  NVS  ---------------------------------------------
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
    // set colors
    deselected = lv_color_make(14,14,28);
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
    // Define rotation
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_180);

    // Create loading screen
    loading_screen = lv_obj_create(NULL);
    lv_screen_load(loading_screen);
    lv_obj_set_style_bg_color(loading_screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_size(loading_screen, 320, 200);

    // Create home screen
    home_page = lv_obj_create(NULL);
    // Set background to black
    lv_obj_set_style_bg_color(home_page, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_size(home_page, 320, 200);
//-------------------------------------------------  LOADING SCREEN  --------------------------------------------------
    // Center div
    lv_style_t loading_div_style;
    lv_style_init(&loading_div_style);
    lv_style_set_border_width(&loading_div_style, 0);
    lv_style_set_pad_all(&loading_div_style, 0);
    lv_style_set_radius(&loading_div_style, 0);
    lv_style_set_size(&loading_div_style, 220, 60);
    lv_style_set_bg_color(&loading_div_style, lv_color_hex(0x000000));
    lv_obj_t* loading_div = lv_obj_create(loading_screen);
    lv_obj_add_style(loading_div, &loading_div_style, 0);
    lv_obj_align(loading_div, LV_ALIGN_CENTER, 0, 0);


    // Div header
    lv_obj_t* loading_header = lv_label_create(loading_div);
    lv_label_set_text(loading_header, LV_SYMBOL_WIFI " Loading...");
    lv_obj_set_style_text_color(loading_header, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(loading_header, LV_ALIGN_BOTTOM_MID, 0, 0);

// Fetching Progress Label Styles
    // Bar Background Style
    lv_style_init(&bar_style_bg);
    lv_style_set_border_color(&bar_style_bg, lv_color_hex(0xffffff));
    lv_style_set_border_width(&bar_style_bg, 2);
    lv_style_set_pad_all(&bar_style_bg, 3);
    lv_style_set_radius(&bar_style_bg, 10);
    lv_style_set_anim_duration(&bar_style_bg, 1500);
    // Bar Indicator Style
    lv_style_init(&bar_style_indic);
    lv_style_set_bg_opa(&bar_style_indic, LV_OPA_COVER);
    lv_style_set_bg_color(&bar_style_indic, lv_color_hex(0x0000ff));
    lv_style_set_radius(&bar_style_indic, 10);
    // Bar Label
    api_progress_label = lv_bar_create(loading_div);
    lv_obj_remove_style_all(api_progress_label);
    lv_obj_add_style(api_progress_label, &bar_style_bg, 0);
    lv_obj_add_style(api_progress_label, &bar_style_indic, LV_PART_INDICATOR);
    lv_obj_set_size(api_progress_label, 220, 30);
    lv_bar_set_value(api_progress_label, 0, LV_ANIM_ON);
    lv_obj_align(api_progress_label, LV_ALIGN_TOP_MID, 0, 0);

//-----------------------------------------------------  NAV BAR  -----------------------------------------------------
    // Nav bar style
    lv_style_init(&nav_style);
    lv_style_set_pad_hor(&nav_style, 0);
    lv_style_set_pad_ver(&nav_style, 8);
    lv_style_set_radius(&nav_style, 0);
    lv_style_set_size(&nav_style, 320, 40);
    lv_style_set_bg_opa(&nav_style, LV_OPA_COVER);
    lv_style_set_bg_color(&nav_style, lv_color_make(28,1,38));
    lv_style_set_text_color(&nav_style, lv_color_hex(0x000000));
    lv_style_set_border_width(&nav_style, 0);
    // Nav bar
    nav_bar = lv_obj_create(home_page);
    lv_obj_align(nav_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_style(nav_bar, &nav_style, 0);
    lv_obj_set_scrollbar_mode(nav_bar, LV_SCROLLBAR_MODE_OFF);


    //---------------------------  Menu  ---------------------------
    // Menu style
    lv_style_init(&menu_style);
    lv_style_set_pad_hor(&menu_style, 4);
    lv_style_set_pad_ver(&menu_style, 8);
    lv_style_set_radius(&menu_style, 0);
    lv_style_set_bg_opa(&menu_style, LV_OPA_COVER);
    lv_style_set_bg_color(&menu_style, lv_color_make(28,1,38));
    lv_style_set_border_width(&menu_style, 0);
// Menu
    lv_obj_t* menu = lv_obj_create(nav_bar);
    lv_obj_align(menu, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_style(menu, &menu_style, 0);
    lv_obj_set_scrollbar_mode(menu, LV_SCROLLBAR_MODE_OFF);

    lv_style_init(&menu_button_style);
    lv_style_set_pad_hor(&menu_button_style, 4);
    lv_style_set_pad_ver(&menu_button_style, 8);
    lv_style_set_radius(&menu_button_style, 0);
    lv_style_set_size(&menu_button_style, 150, 35);
    lv_style_set_bg_opa(&menu_button_style, LV_OPA_0);
    lv_style_set_border_width(&menu_button_style, 0);
    lv_style_set_outline_width(&menu_button_style, 0);
    lv_style_set_shadow_width(&menu_button_style, 0);

// Home Button
    lv_obj_t* home_button = lv_button_create(menu);
    lv_obj_align(home_button, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_style(home_button, &menu_button_style, 0);

    home_button_label = lv_label_create(home_button);
    lv_label_set_text(home_button_label, LV_SYMBOL_HOME);
    lv_obj_set_style_text_color(home_button, lv_color_hex(0x000000), LV_PART_MAIN);

// Accounts Button
    lv_obj_t* accounts_button = lv_button_create(menu);
    lv_obj_align(accounts_button, LV_ALIGN_LEFT_MID, 25, 0);
    lv_obj_add_style(accounts_button, &menu_button_style, 0);

    accounts_button_label = lv_label_create(accounts_button);
    lv_label_set_text(accounts_button_label, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(accounts_button, deselected, LV_PART_MAIN);

// Transactions Button
    lv_obj_t* transactions_button = lv_button_create(menu);
    lv_obj_align(transactions_button, LV_ALIGN_LEFT_MID, 50, 0);
    lv_obj_add_style(transactions_button, &menu_button_style, 0);

    transactions_button_label = lv_label_create(transactions_button);
    lv_label_set_text(transactions_button_label, LV_SYMBOL_BELL);
    lv_obj_set_style_text_color(transactions_button, deselected, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(nav_bar);
    lv_label_set_text(title, "Finance Hub");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Time label
    time_label = lv_label_create(nav_bar);
    lv_label_set_text(time_label, "Fetching...");
    lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

//--------------------------------------------------------------------  HOME SCREEN  --------------------------------------------------------------------
    // Counter label
    counter_label = lv_label_create(home_page);
    lv_label_set_text(counter_label, "Count: 0");
    lv_obj_set_style_text_color(counter_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(counter_label, LV_ALIGN_TOP_MID, 0, 45);

    // Credit Cards Balance
    lv_obj_t *total_credit_balance_label = lv_label_create(home_page);
    lv_label_set_text(total_credit_balance_label, LV_SYMBOL_WIFI " Credit");
    lv_obj_set_style_text_color(total_credit_balance_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(total_credit_balance_label, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_set_style_text_align(total_credit_balance_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);


    // Checking Accounts Balance
    lv_obj_t *total_checking_balance_label = lv_label_create(home_page);
    lv_label_set_text(total_checking_balance_label, LV_SYMBOL_WIFI " Checking");
    lv_obj_set_style_text_color(total_checking_balance_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(total_checking_balance_label, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_text_align(total_checking_balance_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

//--------------------------------------------------------------------  ACCOUNTS SCREEN  --------------------------------------------------------------------
    // Create accounts screen
    accounts_page = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(accounts_page, lv_color_hex(0x000000), LV_PART_MAIN);

// Counter label
    lv_obj_t *accounts_label = lv_label_create(accounts_page);
    lv_label_set_text(accounts_label, "Accounts Page");
    lv_obj_set_style_text_color(accounts_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(accounts_label, LV_ALIGN_TOP_MID, 0, 45);

//------------------------------------------------------------------  TRANSACTIONS SCREEN  ------------------------------------------------------------------
    transactions_page = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(transactions_page, lv_color_hex(0x000000), LV_PART_MAIN);

    // Counter label
    lv_obj_t *transactions_label = lv_label_create(transactions_page);
    lv_label_set_text(transactions_label, "Transactions Page");
    lv_obj_set_style_text_color(transactions_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(transactions_label, LV_ALIGN_TOP_MID, 0, 45);



    // Create timer updater
    lv_timer_create(counter_update_cb, 1000, NULL);

    // Get current time via API
    update_time();

    // Call lvgl_task to run indefinitely
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 8192, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(next_page, "Next Page", 2048, NULL, 1, NULL, tskNO_AFFINITY);


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
    sprintf(total_credit_balance_string, "Credit Balance\n$%.2f", total_credit_balance);
    lv_label_set_text(total_credit_balance_label, total_credit_balance_string);

    char total_checking_balance_string[32];
    sprintf(total_checking_balance_string, "Checking Balance\n$%.f", total_checking_balance);
    lv_label_set_text(total_checking_balance_label, total_checking_balance_string);

    // Create a one-shot timer with a 5-second delay
    TimerHandle_t bar_deletion_timer = xTimerCreate(
            "Delete_Bar_Timer",
            pdMS_TO_TICKS(5000),
            pdFALSE,                     // One-shot timer
            NULL,
            clear_loading_screen        // Callback function
    );

    if(bar_deletion_timer != NULL) { xTimerStart(bar_deletion_timer, 0); }

    // From here, the _Noreturn void lvgl_task() will run until the system is powered off.
}
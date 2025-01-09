#include <stdio.h>
#include <esp_timer.h>
#include <esp_log.h>
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "lvgl.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "esp_lcd_ili9341.h"
//#include "ui.h"

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

// Tags are for logging. Basically a label for logs
static const char *WIFI_TAG = "WiFi";
static const char *TAG = "HTTP_CLIENT";

// Wifi Config
#define WIFI_SSID "NETGEAR17"
#define WIFI_PASS "Rosselin06"
#define WIFI_MAX_RETRY 5 // The max amount of wifi connect retries before it throws an error

static EventGroupHandle_t s_wifi_event_group; // Handles the event and its responses
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
static int s_retry_num = 0; // Keeps count of how many times a connection was attempted

// ------------------------------------------ LVGL Objects ------------------------------------------
static lv_obj_t *home;
static lv_obj_t *second;

//static lv_style_t text_style;

static lv_obj_t *counter_label;
static int counter = 0;

static lv_obj_t *time_label;
// -----------------------------------------  API Functions  ------------------------------------------

// -----------------------  WiFi  -----------------------

// Function to connect to WiFi, get it IP address and log progress. Also, will automatically retry to connect if it fails
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Try to connect again up to X amount of times set in WIFI_MAX_RETRY
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect(); // Try to connect
            s_retry_num++; // Increase retry num
            ESP_LOGI(WIFI_TAG, "Retry to connect to AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT); // Wifi Failed
        }
        ESP_LOGI(WIFI_TAG, "Failed to connect to AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initializes the WiFi library and handlers. WiFi functions will not work without this
void wifi_init(void) {
    ESP_LOGI(WIFI_TAG, "Initializing WIFI...");
    // Create the event group to handle WiFi events
    s_wifi_event_group = xEventGroupCreate();

    // Boring mandatory Espressif stuff.
    ESP_ERROR_CHECK(esp_netif_init());

    // Set network setting to station (connect to another source)
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Set Device name
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif, "ESP32 C6 Finance Hub"));


    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id
            )
    );
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip
            )
    );

    // List the WiFi configuration like name, password, type of connection, etc.
    wifi_config_t wifi_config = {
            .sta = {
                    .ssid = WIFI_SSID,
                    .password = WIFI_PASS,
                    .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "Wi-Fi initialization finished");

    EventBits_t bits = xEventGroupWaitBits(
            s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY
        );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "Connected to SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(WIFI_TAG, "Unexpected Wi-Fi event");
    }
}



// -----------------------  Time  -----------------------

// Updates time label with given parameter
void update_time(const char *time_str) {
    char displayString[20];
    sprintf(displayString, "Year: 2025\n Week: %s",time_str);
    lv_label_set_text(time_label, displayString);
}


esp_err_t http_event_handler(esp_http_client_event_t *event) {
    static char response_buffer[1024];
    static int data_len = 0;

    switch (event->event_id) {
        case HTTP_EVENT_ON_DATA: // Run this while still receiving data
            if (!esp_http_client_is_chunked_response(event->client)) {
                if (data_len + event->data_len < sizeof(response_buffer)) {
                    memcpy(response_buffer + data_len, event->data, event->data_len);
                    data_len += event->data_len;
                    response_buffer[data_len] = 0;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH: // Run this once all data is received
            cJSON *json = cJSON_Parse(response_buffer); // Parses response_buffer into JSON
            const cJSON *jsonResult = cJSON_GetObjectItem(json, "week_number"); // Gets the object we want

            if (cJSON_IsString(jsonResult)) { // If a string, call update_time to insert it into the label
                update_time(jsonResult->valuestring);
            } else if(cJSON_IsNumber(jsonResult)){ // If an integer, call update_time to insert it into the label
                char intBuffer[5]; // Buffer to hold integer as a string
                snprintf(intBuffer, sizeof(intBuffer), "%d", jsonResult->valueint); // Convert int to string
                update_time(intBuffer);
            } else {
                update_time("Invalid Time"); // Else display this
            }

            cJSON_Delete(json); // Free up memory
            data_len = 0; // Reset counter
            break;
        case HTTP_EVENT_ERROR: // Run this if we run into an error
            update_time("Error");
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        default:
            break;
    }
    return ESP_OK;
}

void fetch_time() {

    // Define the url and handler that we want to connect => Link to processing function
    esp_http_client_config_t config = {
            .url = "http://worldtimeapi.org/api/timezone/Etc/UTC",
            .event_handler = http_event_handler,
    };

    // Init the client with the config
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Make sure the client runs correctly
    esp_err_t err = esp_http_client_perform(client);
    if(err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Success");
    } else {
        ESP_LOGE(TAG, "HTTP GET Failed");
        update_time("HTTP ERROR");
    }
    esp_http_client_cleanup(client); // Free up memory
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

// Controls custom count label with each second
static void counter_update_cb() {
    char timer_buffer[16];
    snprintf(timer_buffer, sizeof(timer_buffer), "Count: %d", counter++);
    lv_label_set_text(counter_label, timer_buffer);

//    if(counter % 20 == 0) {
//        lv_screen_load_anim(home, LV_SCR_LOAD_ANIM_OVER_TOP, 1000, 0, false);
//        lv_obj_set_parent(counter_label, home);
//        return;
//    }
//
//    if(counter % 10 == 0) {
//        lv_screen_load_anim(second, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 1000, 0, false);
//        lv_obj_set_parent(counter_label, second);
//        return;
//    }
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
// -------------------------------------------  Wi-Fi  -------------------------------------------

    // Boring mandatory espressif wifi stuff
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Call function to init wifi
    wifi_init();

    // More boring mandatory espressif wifi stuff
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_get_ip_info(netif, &ip_info);
    printf("IP Address: " IPSTR "\n", IP2STR(&ip_info.ip));
    printf("Gateway: " IPSTR "\n", IP2STR(&ip_info.gw));
    printf("Netmask: " IPSTR "\n", IP2STR(&ip_info.netmask));
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
    // Init style
//    lv_style_init(&text_style);
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
    lv_obj_set_style_bg_color(home, lv_color_hex(0xff0000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(second, lv_color_hex(0x000000), LV_PART_MAIN);

    // This is for SquareLine Studio if I am using it. (Currently not).
//    ui_init();

    // Hello World Label
    lv_obj_t *totalBalance = lv_label_create(home);
    lv_label_set_text(totalBalance, LV_SYMBOL_WIFI "Fetching Balance...\n");
    lv_obj_set_style_text_color(totalBalance, lv_color_hex(0xffffff), LV_PART_MAIN);
//    lv_obj_set_style_text_font(totalBalance, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(totalBalance, LV_ALIGN_CENTER, 0, 0);

    // Counter label
    counter_label = lv_label_create(home);
    lv_label_set_text(counter_label, "Count: 0");
    lv_obj_set_style_text_color(counter_label, lv_color_hex(0xffffff), LV_PART_MAIN);

    // Time label
/*
    time_label = lv_label_create(second);
    lv_label_set_text(time_label, "Fetching...");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 0);
*/
    // Create timer updater
    lv_timer_create(counter_update_cb, 1000, NULL);

    // Get current time via API
//    fetch_time();

    // Call lvgl_task to run indefinitely
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 8192, NULL, 1, NULL, 0);

    // From here, the _Noreturn void lvgl_task() will run until the system is powered off.
}
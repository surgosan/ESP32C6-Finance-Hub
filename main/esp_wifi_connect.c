//
// Created by sergy on 1/10/2025.
//

#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_wifi_connect.h"
#include "freertos/event_groups.h"
#include "env.h"

// Wifi Config
#define WIFI_MAX_RETRY 5 // The max amount of wifi connect retries before it throws an error

static EventGroupHandle_t s_wifi_event_group; // Handles the event and its responses
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
static int s_retry_num = 0; // Keeps count of how many times a connection was attempted
// Tags are for logging. Basically a label for logs
static const char *WIFI_TAG = "WiFi";

// -----------------------  WiFi  -----------------------

/*
 * Function to connect to Wi-Fi, get its IP address and log progress.
 * Also, will automatically retry to connect if it fails
*/
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
/*  // Uncomment this if not already in main code
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
*/
    ESP_LOGI(WIFI_TAG, "Initializing WIFI...");
    ESP_ERROR_CHECK(esp_netif_init());
    // Set network setting to station (connect to another source)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (!netif) {
        ESP_LOGE(WIFI_TAG, "Failed to create default WiFi station netif");
        return;
    }

    ESP_ERROR_CHECK(esp_netif_set_hostname(netif, "ESP32 C6 Finance Hub"));

    // Create the event group to handle WiFi events
    s_wifi_event_group = xEventGroupCreate();

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

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    printf("IP Address: " IPSTR "\n", IP2STR(&ip_info.ip));
    printf("Gateway: " IPSTR "\n", IP2STR(&ip_info.gw));
    printf("Netmask: " IPSTR "\n", IP2STR(&ip_info.netmask));

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
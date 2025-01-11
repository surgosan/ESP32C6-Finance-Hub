//
// Created by sergy on 1/11/2025.
//

#include "esp_http_client_handler.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include "nvs_flash.h"

// Define a TAG specific to the HTTP module
static const char *TAG = "HTTP_CLIENT";

// Buffer to hold the HTTP response data
static char response_buffer[1024];
static int data_len = 0;

// Buffer to hold the parsed time (static for simplicity)
static char time_buffer[32];

// Event handler for HTTP client
esp_err_t http_event_handler(esp_http_client_event_t *event) {
    switch (event->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(event->client)) {
                if (data_len + event->data_len < sizeof(response_buffer)) {
                    memcpy(response_buffer + data_len, event->data, event->data_len);
                    data_len += event->data_len;
                    response_buffer[data_len] = '\0'; // Null-terminate the buffer
                }
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            cJSON *json = cJSON_Parse(response_buffer);
            if (json) {
                const cJSON *jsonResult = cJSON_GetObjectItem(json, "week_number");
                if (cJSON_IsString(jsonResult)) {
                    strncpy(time_buffer, jsonResult->valuestring, sizeof(time_buffer) - 1);
                    time_buffer[sizeof(time_buffer) - 1] = '\0'; // Null-terminate
                } else if (cJSON_IsNumber(jsonResult)) {
                    snprintf(time_buffer, sizeof(time_buffer), "%d", jsonResult->valueint);
                } else {
                    strncpy(time_buffer, "Invalid Time", sizeof(time_buffer) - 1);
                    time_buffer[sizeof(time_buffer) - 1] = '\0';
                }
                cJSON_Delete(json);
            } else {
                strncpy(time_buffer, "JSON Parse Error", sizeof(time_buffer) - 1);
                time_buffer[sizeof(time_buffer) - 1] = '\0';
            }
            data_len = 0; // Reset data length for the next request
            break;

        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            strncpy(time_buffer, "HTTP Error", sizeof(time_buffer) - 1);
            time_buffer[sizeof(time_buffer) - 1] = '\0';
            break;

        default:
            break;
    }
    return ESP_OK;
}

// Function to fetch time and return it as a string
const char* fetch_time() {
    esp_http_client_config_t config = {
            .url = "http://worldtimeapi.org/api/timezone/America/New_York",
            .event_handler = http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Success");
    } else {
        ESP_LOGE(TAG, "HTTP GET Failed");
        strncpy(time_buffer, "HTTP Request Failed", sizeof(time_buffer) - 1);
        time_buffer[sizeof(time_buffer) - 1] = '\0';
    }

    esp_http_client_cleanup(client); // Free up memory
    return time_buffer; // Return the time or error message
}

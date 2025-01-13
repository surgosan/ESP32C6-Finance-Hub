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
static const char *PLAID_TAG = "Plaid Tag";


// Buffer to hold the HTTP response data
static char response_buffer[1024];
static int data_len = 0;

// -----------------------------------------------------  Time API  -----------------------------------------------------
// Buffer to hold the parsed time (static for simplicity)
static char time_buffer[32];
// Event handler for HTTP client
esp_err_t time_handler(esp_http_client_event_t *event) {
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
            .event_handler = time_handler,
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


// --------------------------------------------------  Plaid Sandbox  --------------------------------------------------
static char plaid_response[1024];

static const char *PLAID_ROOT_CERT =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
        "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
        "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
        "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
        "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
        "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
        "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
        "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
        "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
        "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
        "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
        "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
        "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
        "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
        "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
        "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
        "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
        "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
        "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
        "MrY=\n"
        "-----END CERTIFICATE-----\n";

esp_err_t plaid_http_handler(esp_http_client_event_t* evt) {
    static char* plaid_handler_buffer = NULL; // Buffer to accumulate the response
    static int response_buffer_len = 0;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0) {
                // Reallocate buffer to accommodate new data
                char* temp = realloc(plaid_handler_buffer, response_buffer_len + evt->data_len + 1);
                if (!temp) {
                    ESP_LOGE("Plaid Handler", "Failed to allocate memory for response buffer");
                    free(plaid_handler_buffer);
                    plaid_handler_buffer = NULL;
                    response_buffer_len = 0;
                    return ESP_FAIL;
                }
                plaid_handler_buffer = temp;

                // Copy new data to the buffer
                memcpy(plaid_handler_buffer + response_buffer_len, evt->data, evt->data_len);
                response_buffer_len += evt->data_len;
                plaid_handler_buffer[response_buffer_len] = '\0'; // Null-terminate the buffer
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI("Plaid Handler", "HTTP response finished. Total response size: %d bytes", response_buffer_len);
//            ESP_LOGI("Plaid Handler", "Response: %s", response_buffer);

            // Parse and process the JSON response
            cJSON* root = cJSON_Parse(plaid_handler_buffer);
            if (root) {
                ESP_LOGI(PLAID_TAG, "%s", cJSON_Print(root));
                // Handle JSON (similar to the earlier example)
                cJSON* accounts = cJSON_GetObjectItem(root, "accounts");
                if (cJSON_IsArray(accounts)) {
                    cJSON* first_account = cJSON_GetArrayItem(accounts, 0);
                    char* result = cJSON_Print(first_account);
                    if(result) {
                        snprintf(plaid_response, sizeof(plaid_response), "%s", result);
                        free(result);
                    } else {
                        ESP_LOGE(PLAID_TAG, "No First Account");
                        cJSON_Delete(root);
                    }

                    cJSON* account;
                    cJSON_ArrayForEach(account, accounts) {
                        cJSON* name = cJSON_GetObjectItem(account, "name");
                        cJSON* balance = cJSON_GetObjectItem(account, "balances");
                        if (cJSON_IsString(name) && cJSON_IsObject(balance)) {
                            cJSON* available = cJSON_GetObjectItem(balance, "available");
                            cJSON* current = cJSON_GetObjectItem(balance, "current");

                            ESP_LOGI("Plaid Handler", "Account: %s", name->valuestring);
                            ESP_LOGI("Plaid Handler", "Available Balance: %.2f", cJSON_IsNumber(available) ? available->valuedouble : 0.0);
                            ESP_LOGI("Plaid Handler", "Current Balance: %.2f", cJSON_IsNumber(current) ? current->valuedouble : 0.0);
                        }
                    }
                } else {
                    ESP_LOGE("Plaid Handler", "No 'accounts' array found in response");
                }
                cJSON_Delete(root);
            } else {
                ESP_LOGE("Plaid Handler", "Failed to parse JSON response");
            }

            // Free the response buffer
            free(plaid_handler_buffer);
            plaid_handler_buffer = NULL;
            response_buffer_len = 0;
            break;

        case HTTP_EVENT_ERROR:
            ESP_LOGE("Plaid Handler", "HTTP Event Error occurred");
            break;

        default:
            break;
    }

    return ESP_OK;
}


char* plaid_fetch_data(const char* access_token) {
    esp_http_client_config_t config = {
            .host = "production.plaid.com",
            .url = "https://production.plaid.com/accounts/balance/get",
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .cert_pem = PLAID_ROOT_CERT,
            .skip_cert_common_name_check = true,
            .event_handler = plaid_http_handler, // Assign event handler
            .timeout_ms = 6000
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", access_token);
    esp_http_client_set_header(client, "Authorization", auth_header);

    // Set POST data
    char post_data[512];
    snprintf(post_data, sizeof(post_data),
             "{\"client_id\":\"%s\",\"secret\":\"%s\",\"access_token\":\"%s\"}",
             PLAID_CLIENT_ID, PLAID_SECRET, access_token);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // Perform the HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(PLAID_TAG, "Error performing HTTP request: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    return(plaid_response);
}

//
// Created by sergy on 1/11/2025.
//

#include "esp_http_client_handler.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include "env.h"

// Define a TAG specific to the HTTP module
static const char *TIME_TAG = "HTTP_CLIENT";
static const char *PLAID_TAG = "Plaid Tag";

// Buffer to hold the HTTP response data
static char response_buffer[1024];
static int data_len = 0;

// -----------------------------------------------------  Time API  -----------------------------------------------------
static const char* time_api_pem =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB\n"
        "iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl\n"
        "cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV\n"
        "BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw\n"
        "MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV\n"
        "BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU\n"
        "aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy\n"
        "dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK\n"
        "AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B\n"
        "3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY\n"
        "tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/\n"
        "Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2\n"
        "VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT\n"
        "79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6\n"
        "c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT\n"
        "Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l\n"
        "c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee\n"
        "UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE\n"
        "Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd\n"
        "BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G\n"
        "A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF\n"
        "Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO\n"
        "VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3\n"
        "ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs\n"
        "8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR\n"
        "iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze\n"
        "Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ\n"
        "XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/\n"
        "qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB\n"
        "VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB\n"
        "L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG\n"
        "jjxDah2nGN59PRbxYvnKkKj9\n"
        "-----END CERTIFICATE-----";

// Buffer to hold the parsed time (static for simplicity)
static char time_buffer[2][32];
// Event handler for HTTP client
esp_err_t time_handler(esp_http_client_event_t *event) {
    switch (event->event_id) {
        case HTTP_EVENT_ON_DATA:
//            ESP_LOGI(TIME_TAG, "Received data chunk: %.*s", event->data_len, (char *)event->data);
            // Append the received chunk to the buffer
            if (data_len + event->data_len < sizeof(response_buffer)) {
                memcpy(response_buffer + data_len, event->data, event->data_len);
                data_len += event->data_len;
                response_buffer[data_len] = '\0'; // Null-terminate the buffer
            } else {
                ESP_LOGE(TIME_TAG, "Response buffer overflow! Increase the buffer size.");
            }
            break;

        case HTTP_EVENT_ON_FINISH:
//            ESP_LOGI(TAG, "Response Buffer: %s", response_buffer);
            int status_code = esp_http_client_get_status_code(event->client);
            ESP_LOGI(TIME_TAG, "TimeIO Status: %d", status_code);
            cJSON *json = cJSON_Parse(response_buffer);
            if (json) { // If responded with JSON response
                const cJSON *date_result = cJSON_GetObjectItem(json, "date");
                const cJSON *day_result = cJSON_GetObjectItem(json, "dayOfWeek");

                if(cJSON_IsString(date_result)) {
                    strncpy(time_buffer[0], date_result->valuestring, sizeof(time_buffer[0]) - 1);
                    time_buffer[0][sizeof(time_buffer[0]) - 1] = '\0';
                } else {
                    strncpy(time_buffer[0], "Invalid Date", sizeof(time_buffer[0]) - 1);
                    time_buffer[0][sizeof(time_buffer[0]) - 1] = '\0';
                }

                // Extract "dayOfWeek" from JSON
                if (cJSON_IsString(day_result)) {
                    strncpy(time_buffer[1], day_result->valuestring, sizeof(time_buffer[1]) - 1);
                    time_buffer[1][sizeof(time_buffer[1]) - 1] = '\0'; // Null-terminate
                } else {
                    strncpy(time_buffer[1], "Invalid Day", sizeof(time_buffer[1]) - 1);
                    time_buffer[1][sizeof(time_buffer[1]) - 1] = '\0';
                }

//                if (cJSON_IsString(date_result)) {
//                    strncpy(time_buffer[0], date_result->valuestring, sizeof(time_buffer) - 1);
//                    time_buffer[sizeof(time_buffer) - 1] = '\0'; // Null-terminate
//                } else if (cJSON_IsNumber(date_result)) {
//                    snprintf(time_buffer[0], sizeof(time_buffer), "%d", date_result->valueint);
//                } else {
//                    strncpy(time_buffer[0], "Invalid Time", sizeof(time_buffer) - 1);
//                    time_buffer[sizeof(time_buffer) - 1] = '\0';
//                }
                cJSON_Delete(json);
            } else {
                ESP_LOGE(TIME_TAG, "Failed to parse JSON response.");
                strncpy(time_buffer[0], "JSON Parse Error", sizeof(time_buffer[0]) - 1);
                strncpy(time_buffer[1], "", sizeof(time_buffer[1]) - 1);
            }
            data_len = 0; // Reset data length for the next request
            break;

        case HTTP_EVENT_ERROR:
            ESP_LOGE(TIME_TAG, "HTTP_EVENT_ERROR");
            strncpy(time_buffer[0], "HTTP Error", sizeof(time_buffer[0]) - 1);
            strncpy(time_buffer[1], "", sizeof(time_buffer[1]) - 1);
            break;

        default:
            break;
    }
    return ESP_OK;
}

// Function to fetch time and return it as a string
char (*fetch_time())[32] {
    esp_http_client_config_t config = {
            .url = "https://www.timeapi.io/api/time/current/zone?timeZone=America%2FNew_York",
            .event_handler = time_handler,
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .cert_pem = time_api_pem,
            .skip_cert_common_name_check = false,
            .method = HTTP_METHOD_GET,
            .timeout_ms = 2000,
            .keep_alive_enable = true
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TIME_TAG, "ESP Performed Time API");
        ESP_LOGI(TIME_TAG, "Time Buffer[0]: %s", time_buffer[0]);
        ESP_LOGI(TIME_TAG, "Time Buffer[1]: %s", time_buffer[1]);
    } else {
        ESP_LOGE(TIME_TAG, "ESP Failed Time API");
        strncpy(time_buffer[0], "ESP Failed Time API", sizeof(time_buffer[0]) - 1);
        strncpy(time_buffer[1], "", sizeof(time_buffer[1]) - 1);
    }

    esp_http_client_cleanup(client); // Free up memory
    return time_buffer; // Return the time or error message
}


// --------------------------------------------------  Plaid Sandbox  --------------------------------------------------
static char* plaid_response;
static char* institution_temp = NULL;

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

esp_err_t plaid_balance_handler(esp_http_client_event_t* evt) {
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
//            ESP_LOGI(PLAID_TAG, "HTTP response finished. Total response size: %d bytes", response_buffer_len);
//            ESP_LOGI(PLAID_TAG, "Response: %s", response_buffer);

            // Parse and process the JSON response
            cJSON* root = cJSON_Parse(plaid_handler_buffer);
            if (root) {
//                ESP_LOGI(PLAID_TAG, "%s", cJSON_Print(root));
                // Handle JSON (similar to the earlier example)
                cJSON* accounts = cJSON_GetObjectItem(root, "accounts");
                if (cJSON_IsArray(accounts)) {
                    cJSON* account_array = cJSON_CreateArray(); // Create an array to hold accounts in JSON
                    cJSON* account;
                    cJSON_ArrayForEach(account, accounts) {
                        cJSON* name = cJSON_GetObjectItem(account, "name");
                        cJSON* balance = cJSON_GetObjectItem(account, "balances");
                        if (cJSON_IsString(name) && cJSON_IsObject(balance)) {
                            cJSON* current = cJSON_GetObjectItem(balance, "current");

                            cJSON *account_object = cJSON_CreateObject(); // single account object
                            cJSON_AddStringToObject(account_object, "Institution", institution_temp);
                            cJSON_AddStringToObject(account_object, "Account", name->valuestring);
                            cJSON_AddNumberToObject(account_object, "Balance", cJSON_IsNumber(current) ? current->valuedouble : 0.0);

                            cJSON_AddItemToArray(account_array, account_object);
                        }
                    }
                    plaid_response = cJSON_PrintUnformatted(account_array);
                    cJSON_Delete(account_array);
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


char* plaid_fetch_balance(const char* access_token, const char* institution) {
    institution_temp = strdup(institution);
    if(!institution_temp) {
        ESP_LOGE(PLAID_TAG, "Failed to copy institution to the temp var");
        return NULL;
    }

    esp_http_client_config_t config = {
            .host = "production.plaid.com",
            .url = "https://production.plaid.com/accounts/balance/get",
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .cert_pem = PLAID_ROOT_CERT,
            .skip_cert_common_name_check = false,
            .event_handler = plaid_balance_handler, // Assign event handler
            .timeout_ms = 8000
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
             "{\"client_id\":\"%s\",\"secret\":\"%s\",\"access_token\":\"%s\", \"options\": { \"min_last_updated_datetime\": \"2025-01-09T00:00:00Z\" }}",
             PLAID_CLIENT_ID, PLAID_SECRET, access_token);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // Perform the HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(PLAID_TAG, "Error performing HTTP request: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    if(institution_temp) {
        free(institution_temp);
        institution_temp = NULL;
    }
    return(plaid_response);
}

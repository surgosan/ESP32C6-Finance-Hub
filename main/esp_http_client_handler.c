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
static const char *PLAID_ROOT_CERT =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDtzCCAp+gAwIBAgIQDOfg5RfYRv6P5WD8G/AwOTANBgkqhkiG9w0BAQUFADBl\n"
        "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
        "d3cuZGlnaWNlcnQuY29tMSQwIgYDVQQDExtEaWdpQ2VydCBBc3N1cmVkIElEIFJv\n"
        "b3QgQ0EwHhcNMDYxMTEwMDAwMDAwWhcNMzExMTEwMDAwMDAwWjBlMQswCQYDVQQG\n"
        "EwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNl\n"
        "cnQuY29tMSQwIgYDVQQDExtEaWdpQ2VydCBBc3N1cmVkIElEIFJvb3QgQ0EwggEi\n"
        "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCtDhXO5EOAXLGH87dg+XESpa7c\n"
        "JpSIqvTO9SA5KFhgDPiA2qkVlTJhPLWxKISKityfCgyDF3qPkKyK53lTXDGEKvYP\n"
        "mDI2dsze3Tyoou9q+yHyUmHfnyDXH+Kx2f4YZNISW1/5WBg1vEfNoTb5a3/UsDg+\n"
        "wRvDjDPZ2C8Y/igPs6eD1sNuRMBhNZYW/lmci3Zt1/GiSw0r/wty2p5g0I6QNcZ4\n"
        "VYcgoc/lbQrISXwxmDNsIumH0DJaoroTghHtORedmTpyoeb6pNnVFzF1roV9Iq4/\n"
        "AUaG9ih5yLHa5FcXxH4cDrC0kqZWs72yl+2qp/C3xag/lRbQ/6GW6whfGHdPAgMB\n"
        "AAGjYzBhMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n"
        "BBRF66Kv9JLLgjEtUYunpyGd823IDzAfBgNVHSMEGDAWgBRF66Kv9JLLgjEtUYun\n"
        "pyGd823IDzANBgkqhkiG9w0BAQUFAAOCAQEAog683+Lt8ONyc3pklL/3cmbYMuRC\n"
        "dWKuh+vy1dneVrOfzM4UKLkNl2BcEkxY5NM9g0lFWJc1aRqoR+pWxnmrEthngYTf\n"
        "fwk8lOa4JiwgvT2zKIn3X/8i4peEH+ll74fg38FnSbNd67IJKusm7Xi+fT8r87cm\n"
        "NW1fiQG2SVufAQWbqz0lwcy2f8Lxb4bG+mRo64EtlOtCt/qMHt1i8b5QZ7dsvfPx\n"
        "H2sMNgcWfzd8qVttevESRmCD1ycEvkvOl77DZypoEd+A5wwzZr8TDRRu838fYxAe\n"
        "+o0bJW1sj6W3YQGx0qMmoRBxna3iw/nDmVG3KwcIzi7mULKn+gpFL6Lw8g==\n"
        "-----END CERTIFICATE-----\n";

char* plaid_parse_first_entry(const char* json_response) {
    cJSON* root = cJSON_Parse(json_response);
    if (!root) {
        ESP_LOGE("JSON", "Failed to parse JSON response");
        return NULL;
    }

    // Navigate to the "accounts" array
    cJSON* accounts = cJSON_GetObjectItem(root, "accounts");
    if (!cJSON_IsArray(accounts)) {
        ESP_LOGE("JSON", "No accounts found in the response");
        cJSON_Delete(root);
        return NULL;
    }

    // Get the first entry in the array
    cJSON* first_account = cJSON_GetArrayItem(accounts, 0);
    if (!first_account) {
        ESP_LOGE("JSON", "No first account found");
        cJSON_Delete(root);
        return NULL;
    }

    // Convert the first account to a string
    char* first_entry = cJSON_Print(first_account);

    // Cleanup
    cJSON_Delete(root);
    return first_entry; // Caller must free this memory
}


char* plaid_fetch_data(const char* access_token) {
    esp_http_client_config_t config = {
            .host = "sandbox.plaid.com",
            .url = "https://sandbox.plaid.com/accounts/balance/get",
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .cert_pem = PLAID_ROOT_CERT
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    char auth_header[256];
    sprintf(auth_header, "Bearer %s", access_token);
    esp_http_client_set_header(client, "Authorization", auth_header);

    // Set POST data
    char post_data[512];
    snprintf(post_data, sizeof(post_data),
             "{\"client_id\": \"677b513d0e534e0022b15d14\", \"secret\": \"8ce43a766b4fad961aaedb6857ad03\", \"access_token\": \"%s\"}",
             access_token);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // Perform the HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Status = %d", esp_http_client_get_status_code(client));

        // Read the response content
        int content_length = esp_http_client_get_content_length(client);
        char* plaid_response = malloc(content_length + 1);
        if (plaid_response) {
            esp_http_client_read(client, plaid_response, content_length);
            plaid_response[content_length] = '\0'; // Null-terminate the response
            ESP_LOGI(TAG, "Response: %s", plaid_response);

            esp_http_client_cleanup(client);
            return plaid_response; // Return the response for parsing
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
        }
    } else {
        ESP_LOGE(TAG, "Error performing HTTP request: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return NULL; // Return NULL if there was an error
}
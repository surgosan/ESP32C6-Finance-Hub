//
// Created by sergy on 1/11/2025.
//

#ifndef ESP32C6_FINANCE_HUB_ESP_HTTP_CLIENT_HANDLER_H
#define ESP32C6_FINANCE_HUB_ESP_HTTP_CLIENT_HANDLER_H

#include <esp_http_client.h>
#include "esp_err.h"

/**
 * @brief Fetch time from a remote API and return it as a string
 * @return Pointer to a static buffer containing the time or error message
 */
const char* fetch_time();

/**
 * @brief Get Plaid Data
 * @param access_token Input Plaid Access Token.
 * @return JSON Response
*/
char* plaid_fetch_data(const char* access_token);

/**
 * @brief Input the JSON response from plaid_fetch_data() to get first entry
 * @param json_response
 * @return First entry in the complete JSON response
 */
char* plaid_parse_first_entry(const char* json_response);

#endif //ESP32C6_FINANCE_HUB_ESP_HTTP_CLIENT_HANDLER_H

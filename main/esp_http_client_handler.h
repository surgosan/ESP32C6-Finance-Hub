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

#endif //ESP32C6_FINANCE_HUB_ESP_HTTP_CLIENT_HANDLER_H

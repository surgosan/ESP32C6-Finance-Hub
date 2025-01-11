//
// Created by sergy on 1/11/2025.
//

#ifndef ESP32C6_FINANCE_HUB_ESP_HTTP_CLIENT_HANDLER_H
#define ESP32C6_FINANCE_HUB_ESP_HTTP_CLIENT_HANDLER_H

#include <esp_http_client.h>
#include "esp_err.h"

/**
 * Fetch time from a remote API and return it as a string
 * @return Pointer to a static buffer containing the time or error message
 */
const char* fetch_time();

#endif //ESP32C6_FINANCE_HUB_ESP_HTTP_CLIENT_HANDLER_H

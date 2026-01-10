/**
 * @file goku_mdns.h
 * @brief mDNS Service Discovery
 */

#pragma once
#include <esp_err.h>

/**
 * @brief Initialize mDNS service
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_mdns_init(void);

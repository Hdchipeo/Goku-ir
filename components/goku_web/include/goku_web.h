/**
 * @file goku_web.h
 * @brief Web Server Interface
 */

#pragma once
#include <esp_err.h>

/**
 * @brief Initialize Web Server
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_web_init(void);

/**
 * @brief Toggle Web AP/STA mode (if applicable) or trigger specific web action
 */
void app_web_toggle_mode(void);

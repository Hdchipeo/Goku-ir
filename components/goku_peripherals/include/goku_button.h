/**
 * @file goku_button.h
 * @brief Button handling
 */

#pragma once

#include <esp_err.h>

/**
 * @brief Initialize button driver
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_button_init(void);

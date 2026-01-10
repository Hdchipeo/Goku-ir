/**
 * @file goku_web.h
 * @brief Web Server Interface
 */

#pragma once
#include <esp_err.h>
#include <stdbool.h>

/**
 * @brief Initialize Web Server
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_web_init(void);

/**
 * @brief Start the HTTP Server and Register Handlers
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_web_start(void);

/**
 * @brief Stop the HTTP Server
 */
void app_web_stop(void);

/**
 * @brief Check if Web Server is running
 *
 * @return true if running, false otherwise
 */
bool app_web_is_running(void);

/**
 * @file goku_ota.h
 * @brief Over-The-Air Update Manager
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Start OTA update from a given URL
 *
 * @param url The HTTPS URL to download firmware from
 * @return esp_err_t ESP_OK if started, or error code
 */
esp_err_t app_ota_start(const char *url);

/**
 * @brief Check for new version from server
 *
 * @param out_remote_version Buffer to store the remote version string
 * @param buf_len Size of the buffer
 * @return esp_err_t ESP_OK if check success, ESP_FAIL otherwise
 */
esp_err_t app_ota_check_version(char *out_remote_version, size_t buf_len);

// Non-blocking cached status

/**
 * @brief Get cached latest version string
 *
 * @return const char* Version string or NULL
 */
const char *app_ota_get_cached_version(void);

/**
 * @brief Check if update is available (cached)
 *
 * @return true If update available
 * @return false If up to date
 */
bool app_ota_is_update_available(void);

/**
 * @brief Trigger an OTA version check in background
 */
void app_ota_trigger_check(void);

/**
 * @brief Initialize automatic OTA checks
 *
 * Starts a background task that periodically checks for updates.
 * Only works if CONFIG_OTA_SERVER_URL is set.
 */
void app_ota_auto_init(void);

#pragma once

#include "esp_err.h"

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

/**
 * @brief Initialize automatic OTA checks
 *
 * Starts a background task that periodically checks for updates.
 * Only works if CONFIG_OTA_SERVER_URL is set.
 */
void app_ota_auto_init(void);

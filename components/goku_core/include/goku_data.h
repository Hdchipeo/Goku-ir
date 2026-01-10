/**
 * @file goku_data.h
 * @brief Persistent Data Storage (NVS) Wrapper
 */

#pragma once

#include "cJSON.h"
#include <esp_err.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize Data Storage component (NVS)
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_data_init(void);

/**
 * @brief Save IR raw data to NVS
 *
 * @param key Unique key identifier for the IR signal
 * @param data Pointer to the raw data buffer
 * @param len Length of the data in bytes
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_data_save_ir(const char *key, const void *data, size_t len);

/**
 * @brief Load IR raw data from NVS
 *
 * @param key Unique key identifier for the IR signal
 * @param data Buffer to store the loaded data
 * @param len Pointer to store the length of loaded data
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_data_load_ir(const char *key, void *data, size_t *len);

/**
 * @brief Delete IR data from NVS
 *
 * @param key Key of the data to delete
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_data_delete_ir(const char *key);

/**
 * @brief Rename an IR data key
 *
 * @param old_key Current key name
 * @param new_key New key name
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_data_rename_ir(const char *old_key, const char *new_key);

/**
 * @brief Get list of all saved IR keys
 *
 * @return cJSON* JSON Array containing key strings
 */
cJSON *app_data_get_ir_keys(void);

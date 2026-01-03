#pragma once

#include "cJSON.h"
#include <esp_err.h>
#include <stddef.h>
#include <stdint.h>

esp_err_t app_data_init(void);

// Save/Load generic blob (e.g., IR raw data)
esp_err_t app_data_save_ir(const char *key, const void *data, size_t len);
esp_err_t app_data_load_ir(const char *key, void *data, size_t *len);

// New functions for Web UI
esp_err_t app_data_delete_ir(const char *key);
esp_err_t app_data_rename_ir(const char *old_key, const char *new_key);
cJSON *app_data_get_ir_keys(void);

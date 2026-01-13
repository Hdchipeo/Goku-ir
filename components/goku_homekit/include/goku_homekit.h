#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_homekit_init(void);

/**
 * @brief Check if HomeKit has any paired controllers.
 *        Initializes HAP stack if not already initialized.
 * @return true if paired, false otherwise
 */
bool app_homekit_is_paired(void);

#ifdef __cplusplus
}
#endif

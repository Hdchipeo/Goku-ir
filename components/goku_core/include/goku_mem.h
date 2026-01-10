#ifndef APP_MEM_H
#define APP_MEM_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize memory monitoring task
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_mem_init(void);

/**
 * @brief Get free internal heap size
 *
 * @return size_t Free bytes
 */
size_t app_mem_get_free_internal(void);

/**
 * @brief Get free PSRAM size
 *
 * @return size_t Free bytes
 */
size_t app_mem_get_free_psram(void);

/**
 * @brief Check if enough memory is available for an operation
 *
 * @param size Required size in bytes
 * @param use_psram Whether to check PSRAM or internal heap
 * @return bool True if enough memory is available
 */
bool app_mem_is_safe(size_t size, bool use_psram);

#endif // APP_MEM_H

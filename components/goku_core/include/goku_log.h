/**
 * @file goku_log.h
 * @brief In-memory Logging System
 */

#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * @brief Initialize logging system
 */
void app_log_init(void);

/**
 * @brief Get content of in-memory log buffer
 *
 * @param dest Destination buffer
 * @param max_len Maximum length to copy
 * @return int Number of bytes written
 */
int app_log_get_buffer(char *dest, size_t max_len);

/**
 * @brief Clear in-memory log buffer
 */
void app_log_clear(void);

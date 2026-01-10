/**
 * @file goku_rainmaker.h
 * @brief ESP RainMaker Integration
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>

/**
 * @brief Initialize ESP RainMaker functionality.
 * This handles provisioning, connection, and creating the AC device node.
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_rainmaker_init(void);

/**
 * @brief Updates the RainMaker state when local changes occur (e.g. physical
 * buttons)
 *
 * @param power_on New AC power state
 */
void app_rainmaker_update_state(bool power_on);

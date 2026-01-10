/**
 * @file goku_rainmaker.h
 * @brief ESP RainMaker Integration
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>

#include "ir_types.h"

/**
 * @brief Initialize ESP RainMaker functionality.
 * This handles provisioning, connection, and creating the AC device node.
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_rainmaker_init(void);

/**
 * @brief Updates the RainMaker state when local changes occur (e.g. physical
 * buttons or Web UI)
 *
 * @param state Pointer to the full AC state
 */
void app_rainmaker_update_state(const ir_ac_state_t *state);

/**
 * @brief Update the "Brand" dropdown list in RainMaker to include custom
 * brands.
 *
 * @param brands Array of custom brand names
 * @param count Number of custom brands
 */
void app_rainmaker_update_custom_brands(char **brands, size_t count);

/**
 * @brief Callback type for Web UI toggle
 */
typedef void (*webui_toggle_cb_t)(bool enable);

/**
 * @brief Register callback for Web UI toggle switch
 *
 * @param cb Function to call when switch state changes
 */
void app_rainmaker_register_webui_toggle(webui_toggle_cb_t cb);

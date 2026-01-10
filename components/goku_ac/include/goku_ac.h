/**
 * @file goku_ac.h
 * @brief AC Control Logic
 */

#pragma once

#include "esp_err.h"
#include "ir_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

// Enum defined in ir_types.h included via ir_engine.h

/**
 * @brief Initialize AC logic (load state from NVS if needed)
 */
void app_ac_init(void);

/**
 * @brief Set the full AC state.
 *
 * @param state Pointer to new state
 */
void app_ac_set_state(const ir_ac_state_t *state);

/**
 * @brief Get the current AC state.
 *
 * @param state Pointer to store state
 */
void app_ac_get_state(ir_ac_state_t *state);

/**
 * @brief Set the AC brand.
 *
 * @param brand New brand
 */
void app_ac_set_brand(ac_brand_t brand);

/**
 * @brief Get the current AC brand.
 *
 * @return ac_brand_t Current brand
 */
ac_brand_t app_ac_get_brand(void);

/**
 * @brief Send the IR command based on current state and brand.
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_ac_send(void);

#ifdef __cplusplus
}
#endif

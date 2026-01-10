/**
 * @file ir_engine.h
 * @brief Low-level IR Engine (RMT Driver)
 */

#pragma once

#include "esp_err.h"
#include "ir_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the IR Engine (TX Channel)
 *
 * @param config Configuration struct
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_init(const ir_engine_config_t *config);

/**
 * @brief Send a NEC command
 *
 * @param address Device address (16-bit or 8-bit depending on device)
 * @param command Command code
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_send_nec(uint16_t address, uint16_t command);

/**
 * @brief Send raw RMT symbols
 *
 * @param symbols Pointer to RMT symbols
 * @param count Number of symbols
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_send_raw(const void *symbols, size_t count);

/**
 * @brief Send Daikin AC Command
 *
 * @param state AC State
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_send_daikin(const ir_ac_state_t *state);

/**
 * @brief Send Samsung AC Command
 *
 * @param state AC State
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_send_samsung(const ir_ac_state_t *state);

/**
 * @brief Send Mitsubishi AC Command
 *
 * @param state AC State
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_send_mitsubishi(const ir_ac_state_t *state);

/**
 * @brief Universal AC Send Function (Registry-based)
 * Tries to find the brand in the Universal Registry.
 * If found, uses the Universal Encoder.
 * If not, falls back to legacy handlers if available.
 *
 * @param brand AC Brand
 * @param state AC State
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ir_engine_send_ac(ac_brand_t brand, const ir_ac_state_t *state);

#ifdef __cplusplus
}
#endif

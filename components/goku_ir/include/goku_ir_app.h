/**
 * @file goku_ir_app.h
 * @brief High-level IR Application Layer
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief IR Command types
 */
typedef enum {
  APP_IR_CMD_AC_ON,
  APP_IR_CMD_AC_OFF,
  APP_IR_CMD_TEMP_UP,
  APP_IR_CMD_TEMP_DOWN,
  APP_IR_CMD_MAX
} app_ir_cmd_t;

/**
 * @brief Initialize IR Application
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_ir_init(void);

// RX / Learn

/**
 * @brief Start IR Learning mode
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_ir_start_learn(void);

/**
 * @brief Stop IR Learning mode
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_ir_stop_learn(void);

/**
 * @brief Get the current learning status and captured symbol count
 * @param[out] count Pointer to store symbol count
 * @return true if currently learning, false otherwise
 */
bool app_ir_get_learn_status(uint32_t *count);

/**
 * @brief Save the learned IR signal to NVS
 *
 * @param key Key to save data under
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_ir_save_learned_result(const char *key);

// TX

/**
 * @brief Send a predefined IR command
 *
 * @param cmd Command to send
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_ir_send_cmd(app_ir_cmd_t cmd);

/**
 * @brief Send a raw IR signal by key (from NVS)
 *
 * @param key Key of the stored signal
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_ir_send_key(const char *key);

/**
 * @brief Send raw IR signal durations (pulse/space in microseconds)
 * Compatible with IRremoteESP8266 Raw Data.
 *
 * @param durations Array of durations in microseconds
 * @param count Number of elements in durations array
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_ir_send_raw(const uint16_t *durations, size_t count);

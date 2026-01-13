#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize AHT20 Sensor
 *
 * Uses I2C configuration from Kconfig.
 *
 * @return ESP_OK on success
 */
esp_err_t aht20_sensor_init(void);

/**
 * @brief Read Temperature and Humidity
 *
 * @param[out] temp Temperature in Celsius
 * @param[out] hum Humidity in %
 * @return ESP_OK on valid read
 */
esp_err_t aht20_sensor_read(float *temp, float *hum);

#ifdef __cplusplus
}
#endif

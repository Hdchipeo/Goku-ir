/**
 * @file goku_led.h
 * @brief LED Strip Control and Effects
 */

#pragma once

#include <esp_err.h>

/**
 * @brief LED System States
 */
typedef enum {
  APP_LED_OFF,
  APP_LED_STARTUP,
  APP_LED_WIFI_PROV,
  APP_LED_WIFI_CONN,
  APP_LED_OTA,
  APP_LED_IR_TX,
  APP_LED_IR_LEARN,
  APP_LED_IR_FAIL,
  APP_LED_IR_SUCCESS,
  APP_LED_IDLE,
} app_led_state_t;

/**
 * @brief Available LED Effects
 */
typedef enum {
  APP_LED_EFFECT_STATIC,
  APP_LED_EFFECT_RAINBOW,
  APP_LED_EFFECT_RUNNING,
  APP_LED_EFFECT_BREATHING,
  APP_LED_EFFECT_BLINK,
  APP_LED_EFFECT_KNIGHT_RIDER,
  APP_LED_EFFECT_LOADING,
  APP_LED_EFFECT_COLOR_WIPE,
  APP_LED_EFFECT_THEATER_CHASE,
  APP_LED_EFFECT_FIRE,
  APP_LED_EFFECT_SPARKLE,
  APP_LED_EFFECT_RANDOM,
  APP_LED_EFFECT_AUTO_CYCLE,
} app_led_effect_t;

/**
 * @brief Initialize LED strip
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_led_init(void);

/**
 * @brief Set the LED system state
 *
 * @param state State to switch to
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_led_set_state(app_led_state_t state);

/**
 * @brief Set base color for current effect (if supported)
 *
 * @param r Red
 * @param g Green
 * @param b Blue
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_led_set_base_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Set static color directly
 *
 * @param r Red
 * @param g Green
 * @param b Blue
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_led_set_color(uint8_t r, uint8_t g, uint8_t b);

// New Control Functions

/**
 * @brief Configure a specific LED effect
 *
 * @param effect Effect to configure
 * @param index Index (if applicable)
 * @param r Red
 * @param g Green
 * @param b Blue
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_led_set_config(app_led_effect_t effect, uint8_t index, uint8_t r,
                             uint8_t g, uint8_t b);

/**
 * @brief Activate a specific LED effect
 *
 * @param effect Effect to activate
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_led_set_effect(app_led_effect_t effect);

/**
 * @brief Set global brightness
 *
 * @param brightness 0-100
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_led_set_brightness(uint8_t brightness);

/**
 * @brief Set effect speed
 *
 * @param speed 1-100
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_led_set_speed(uint8_t speed);

// Get config for a specific effect
esp_err_t app_led_get_effect_config(app_led_effect_t effect, uint8_t *speed,
                                    uint8_t colors[8][3]);

esp_err_t app_led_get_config(uint8_t *r, uint8_t *g, uint8_t *b,
                             app_led_effect_t *effect, uint8_t *brightness,
                             uint8_t *speed);

// State Color Customization
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} app_led_state_config_t;

/**
 * @brief Set color for a specific system state
 *
 * @param state State to configure
 * @param r Red
 * @param g Green
 * @param b Blue
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_led_set_state_color(app_led_state_t state, uint8_t r, uint8_t g,
                                  uint8_t b);

esp_err_t app_led_get_state_color(app_led_state_t state, uint8_t *r, uint8_t *g,
                                  uint8_t *b);

// Save and Load settings
esp_err_t app_led_save_settings(void);
esp_err_t app_led_load_settings(void);

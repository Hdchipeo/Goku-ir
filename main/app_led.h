#pragma once

#include <esp_err.h>

typedef enum {
  APP_LED_IDLE,      // Breathing (Green or User Color)
  APP_LED_STARTUP,   // Rainbow
  APP_LED_WIFI_PROV, // Running (Blue)
  APP_LED_WIFI_CONN, // Running (Cyan)
  APP_LED_OTA,       // Running (Purple)
  APP_LED_IR_TX,     // Flash (Red)
  APP_LED_IR_LEARN,  // Blink (Yellow)
  APP_LED_OFF        // Off
} app_led_state_t;

// Set the base color for IDLE state (e.g. from RainMaker)
esp_err_t app_led_init(void);
esp_err_t app_led_set_base_color(uint8_t r, uint8_t g, uint8_t b);
esp_err_t app_led_set_state(app_led_state_t state);
esp_err_t app_led_set_color(uint8_t r, uint8_t g, uint8_t b);

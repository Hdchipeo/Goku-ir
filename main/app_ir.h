#pragma once

#include <esp_err.h>
#include <stdint.h>

typedef enum {
  APP_IR_CMD_AC_ON,
  APP_IR_CMD_AC_OFF,
  APP_IR_CMD_TEMP_UP,
  APP_IR_CMD_TEMP_DOWN,
  // Add more...
  APP_IR_CMD_MAX
} app_ir_cmd_t;

esp_err_t app_ir_init(void);

// RX / Learn
esp_err_t app_ir_start_learn(void);
esp_err_t app_ir_stop_learn(void);
// Check if result available and save it with key
esp_err_t app_ir_save_learned_result(const char *key);

// TX
esp_err_t app_ir_send_cmd(app_ir_cmd_t cmd);
// Send raw by key
esp_err_t app_ir_send_key(const char *key);

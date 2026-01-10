#include "goku_ac.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "goku_ac";

static ir_ac_state_t g_ac_state = {.power = false,
                                   .mode = 1, // Cool
                                   .temp = 24,
                                   .fan = 0,     // Auto
                                   .swing_v = 0, // Off
                                   .swing_h = 0};

static ac_brand_t g_ac_brand = AC_BRAND_DAIKIN;

void app_ac_init(void) {
  ESP_LOGI(TAG, "AC Logic Initialized");
  // TODO: Load from NVS?
}

void app_ac_set_state(const ir_ac_state_t *state) {
  if (state) {
    memcpy(&g_ac_state, state, sizeof(ir_ac_state_t));
  }
}

void app_ac_get_state(ir_ac_state_t *state) {
  if (state) {
    memcpy(state, &g_ac_state, sizeof(ir_ac_state_t));
  }
}

void app_ac_set_brand(ac_brand_t brand) {
  if (brand < AC_BRAND_MAX) {
    g_ac_brand = brand;
  }
}

ac_brand_t app_ac_get_brand(void) { return g_ac_brand; }

esp_err_t app_ac_send(void) {
  ESP_LOGI(TAG, "Sending AC Command: Brand=%d, P=%d, M=%d, T=%d", g_ac_brand,
           g_ac_state.power, g_ac_state.mode, g_ac_state.temp);

  return ir_engine_send_ac(g_ac_brand, &g_ac_state);
}

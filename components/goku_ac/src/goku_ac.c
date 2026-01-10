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

// Custom brand support
static char g_custom_brand_name[32] = {0};
static bool g_is_custom_brand = false;

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
    g_is_custom_brand = false; // Clear custom brand flag
  }
}

ac_brand_t app_ac_get_brand(void) { return g_ac_brand; }

esp_err_t app_ac_send(void) {
  if (g_is_custom_brand) {
    // Custom brand selected
    ESP_LOGI(TAG, "Sending Custom Brand AC: %s, P=%d, M=%d, T=%d",
             g_custom_brand_name, g_ac_state.power, g_ac_state.mode,
             g_ac_state.temp);

    // TODO: Implement custom brand IR command lookup from NVS
    // For now, log warning and use last preset protocol
    ESP_LOGW(TAG, "Custom brand IR transmission not yet implemented!");
    ESP_LOGW(TAG,
             "To fully support custom brands, implement NVS IR command lookup");

    // Fallback to last preset brand (for testing)
    return ir_engine_send_ac(g_ac_brand, &g_ac_state);
  }

  // Preset brand - use IR Engine protocols
  ESP_LOGI(TAG, "Sending AC Command: Brand=%d, P=%d, M=%d, T=%d", g_ac_brand,
           g_ac_state.power, g_ac_state.mode, g_ac_state.temp);
  return ir_engine_send_ac(g_ac_brand, &g_ac_state);
}

void app_ac_set_custom_brand(const char *name) {
  if (name && strlen(name) > 0) {
    strncpy(g_custom_brand_name, name, sizeof(g_custom_brand_name) - 1);
    g_custom_brand_name[sizeof(g_custom_brand_name) - 1] = '\0';
    g_is_custom_brand = true;
    ESP_LOGI(TAG, "Set custom AC brand: %s", g_custom_brand_name);
  }
}

const char *app_ac_get_custom_brand(void) {
  return g_is_custom_brand ? g_custom_brand_name : NULL;
}

bool app_ac_is_custom_brand(void) { return g_is_custom_brand; }

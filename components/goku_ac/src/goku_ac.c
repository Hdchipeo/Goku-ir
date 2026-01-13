#include "goku_ac.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "goku_ac";

#define AC_NVS_NAMESPACE "ac_config"
#define AC_KEY_BRAND "brand"
#define AC_KEY_IS_CUSTOM "is_custom"
#define AC_KEY_CUSTOM_NAME "custom_name"
#define AC_KEY_STATE "state"

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

/**
 * @brief Load AC state from NVS
 */
static esp_err_t load_state_from_nvs(void) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(AC_NVS_NAMESPACE, NVS_READONLY, &nvs);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "NVS namespace not found, using defaults");
    return err;
  }

  // Load brand
  uint8_t brand = 0;
  err = nvs_get_u8(nvs, AC_KEY_BRAND, &brand);
  if (err == ESP_OK && brand < AC_BRAND_MAX) {
    g_ac_brand = (ac_brand_t)brand;
    ESP_LOGI(TAG, "Loaded brand: %d", brand);
  }

  // Load custom brand flag
  uint8_t is_custom = 0;
  if (nvs_get_u8(nvs, AC_KEY_IS_CUSTOM, &is_custom) == ESP_OK) {
    g_is_custom_brand = (is_custom != 0);
  }

  // Load custom brand name
  if (g_is_custom_brand) {
    size_t len = sizeof(g_custom_brand_name);
    nvs_get_str(nvs, AC_KEY_CUSTOM_NAME, g_custom_brand_name, &len);
    ESP_LOGI(TAG, "Loaded custom brand: %s", g_custom_brand_name);
  }

  // Load AC state
  size_t state_len = sizeof(ir_ac_state_t);
  err = nvs_get_blob(nvs, AC_KEY_STATE, &g_ac_state, &state_len);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Loaded state: P=%d, M=%d, T=%d", g_ac_state.power,
             g_ac_state.mode, g_ac_state.temp);
  }

  nvs_close(nvs);
  return ESP_OK;
}

/**
 * @brief Save AC state to NVS
 */
static esp_err_t save_state_to_nvs(void) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(AC_NVS_NAMESPACE, NVS_READWRITE, &nvs);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    return err;
  }

  // Save brand
  nvs_set_u8(nvs, AC_KEY_BRAND, (uint8_t)g_ac_brand);

  // Save custom brand info
  nvs_set_u8(nvs, AC_KEY_IS_CUSTOM, g_is_custom_brand ? 1 : 0);
  if (g_is_custom_brand) {
    nvs_set_str(nvs, AC_KEY_CUSTOM_NAME, g_custom_brand_name);
  }

  // Save state
  nvs_set_blob(nvs, AC_KEY_STATE, &g_ac_state, sizeof(ir_ac_state_t));

  err = nvs_commit(nvs);
  nvs_close(nvs);

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "State saved to NVS");
  }
  return err;
}

void app_ac_init(void) {
  ESP_LOGI(TAG, "AC Logic Initializing...");

  // Load state from NVS
  esp_err_t err = load_state_from_nvs();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Using default AC state");
  }

  ESP_LOGI(TAG, "AC initialized: Brand=%d, Custom=%d, Power=%d", g_ac_brand,
           g_is_custom_brand, g_ac_state.power);
}

void app_ac_set_state(const ir_ac_state_t *state) {
  if (state) {
    memcpy(&g_ac_state, state, sizeof(ir_ac_state_t));
    save_state_to_nvs(); // Auto-save on change
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
    save_state_to_nvs();       // Auto-save on change
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
    save_state_to_nvs(); // Auto-save on change
  }
}

const char *app_ac_get_custom_brand(void) {
  return g_is_custom_brand ? g_custom_brand_name : NULL;
}

bool app_ac_is_custom_brand(void) { return g_is_custom_brand; }

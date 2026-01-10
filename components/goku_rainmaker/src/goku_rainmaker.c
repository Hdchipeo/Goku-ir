#include "goku_rainmaker.h"
#include "goku_ac.h"
#include "goku_data.h"
#include "goku_ir_app.h"
#include "goku_rainmaker.h"
#if CONFIG_APP_LED_CONTROL
#include "goku_led.h"
#endif
#include "goku_wifi.h"
#include "ir_engine.h"
#include <ctype.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_types.h>
#include <math.h>
#include <string.h>

static const char *TAG = "goku_rainmaker";
#define PARAM_CUSTOM_BRAND_NAME "Custom Brand Name"
#define PARAM_WEBUI_MODE "WebUI Config Mode"

esp_rmaker_device_t *ac_device = NULL;
esp_rmaker_device_t *led_device = NULL;
static webui_toggle_cb_t s_webui_cb = NULL;

void app_rainmaker_register_webui_toggle(webui_toggle_cb_t cb) {
  s_webui_cb = cb;
}

#if CONFIG_APP_LED_CONTROL
static bool g_power = false;
static int g_hue = 0;
static int g_saturation = 100;
static int g_brightness = 100;
#endif

static char **g_brand_str_list = NULL;
static size_t g_brand_list_count = 0;

// Global AC State
// Removed local state, using app_ac.h

#if CONFIG_APP_LED_CONTROL
static void hsv2rgb(uint16_t h, uint16_t s, uint16_t v, uint8_t *r, uint8_t *g,
                    uint8_t *b) {
  float H = h, S = s / 100.0, V = v / 100.0;
  float C = V * S;
  float X = C * (1 - fabs(fmod(H / 60.0, 2) - 1));
  float m = V - C;
  float Rs, Gs, Bs;

  if (H >= 0 && H < 60) {
    Rs = C;
    Gs = X;
    Bs = 0;
  } else if (H >= 60 && H < 120) {
    Rs = X;
    Gs = C;
    Bs = 0;
  } else if (H >= 120 && H < 180) {
    Rs = 0;
    Gs = C;
    Bs = X;
  } else if (H >= 180 && H < 240) {
    Rs = 0;
    Gs = X;
    Bs = C;
  } else if (H >= 240 && H < 300) {
    Rs = X;
    Gs = 0;
    Bs = C;
  } else {
    Rs = C;
    Gs = 0;
    Bs = X;
  }

  *r = (Rs + m) * 255;
  *g = (Gs + m) * 255;
  *b = (Bs + m) * 255;
}

static void update_led_state() {
  if (g_power) {
    uint8_t r, g, b;
    hsv2rgb(g_hue, g_saturation, g_brightness, &r, &g, &b);
    app_led_set_color(r, g, b);
  } else {
    app_led_set_color(0, 0, 0);
  }
}

static esp_err_t led_write_cb(const esp_rmaker_device_t *device,
                              const esp_rmaker_param_t *param,
                              const esp_rmaker_param_val_t val, void *priv_data,
                              esp_rmaker_write_ctx_t *ctx) {
  if (ctx) {
    ESP_LOGI(TAG, "Received write request via : %s",
             esp_rmaker_device_cb_src_to_str(ctx->src));
  }
  const char *param_name = esp_rmaker_param_get_name(param);

  if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
    ESP_LOGI(TAG, "Received LED '%s' = %s", param_name,
             val.val.b ? "true" : "false");
    g_power = val.val.b;
  } else if (strcmp(param_name, ESP_RMAKER_DEF_HUE_NAME) == 0) {
    ESP_LOGI(TAG, "Received LED '%s' = %d", param_name, val.val.i);
    g_hue = val.val.i;
  } else if (strcmp(param_name, ESP_RMAKER_DEF_SATURATION_NAME) == 0) {
    ESP_LOGI(TAG, "Received LED '%s' = %d", param_name, val.val.i);
    g_saturation = val.val.i;
  } else if (strcmp(param_name, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0) {
    ESP_LOGI(TAG, "Received LED '%s' = %d", param_name, val.val.i);
    g_brightness = val.val.i;
  }

  update_led_state();
  esp_rmaker_param_update_and_report(param, val);
  return ESP_OK;
}
#endif

/* Callback to handle write requests from the RainMaker App */
static esp_err_t write_cb(const esp_rmaker_device_t *device,
                          const esp_rmaker_param_t *param,
                          const esp_rmaker_param_val_t val, void *priv_data,
                          esp_rmaker_write_ctx_t *ctx) {
  if (ctx) {
    ESP_LOGI(TAG, "Received write request via : %s",
             esp_rmaker_device_cb_src_to_str(ctx->src));
  }
  const char *param_name = esp_rmaker_param_get_name(param);

  if (strcmp(param_name, PARAM_WEBUI_MODE) == 0) {
    ESP_LOGI(TAG, "Received '%s' = %s", param_name,
             val.val.b ? "true" : "false");
    if (s_webui_cb) {
      s_webui_cb(val.val.b);
    }
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
  }

  // Get current state
  ir_ac_state_t state;
  app_ac_get_state(&state);

  if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
    ESP_LOGI(TAG, "Received '%s' = %s", param_name,
             val.val.b ? "true" : "false");
    state.power = val.val.b;
    app_ac_set_state(&state);
    esp_rmaker_param_update_and_report(param, val);

  } else if (strcmp(param_name, ESP_RMAKER_DEF_TEMPERATURE_NAME) == 0) {
    ESP_LOGI(TAG, "Received '%s' = %.1f", param_name, val.val.f);
    state.temp = (uint8_t)val.val.f;
    app_ac_set_state(&state);
    esp_rmaker_param_update_and_report(param, val);

  } else if (strcmp(param_name, "Mode") == 0) {
    ESP_LOGI(TAG, "Received '%s' = %s", param_name, val.val.s);
    if (strcmp(val.val.s, "Auto") == 0)
      state.mode = 0;
    else if (strcmp(val.val.s, "Cool") == 0)
      state.mode = 1;
    else if (strcmp(val.val.s, "Heat") == 0)
      state.mode = 2;
    else if (strcmp(val.val.s, "Fan") == 0)
      state.mode = 3;
    else if (strcmp(val.val.s, "Dry") == 0)
      state.mode = 4;
    app_ac_set_state(&state);
    esp_rmaker_param_update_and_report(param, val);

  } else if (strcmp(param_name, "Fan Speed") == 0) {
    ESP_LOGI(TAG, "Received '%s' = %s", param_name, val.val.s);
    if (strcmp(val.val.s, "Auto") == 0)
      state.fan = 0;
    else if (strcmp(val.val.s, "Low") == 0)
      state.fan = 1;
    else if (strcmp(val.val.s, "Medium") == 0)
      state.fan = 2;
    else if (strcmp(val.val.s, "High") == 0)
      state.fan = 3;
    app_ac_set_state(&state);
    esp_rmaker_param_update_and_report(param, val);

  } else if (strcmp(param_name, "Brand") == 0) {
    ESP_LOGI(TAG, "Received Brand = %s", val.val.s);
    if (strcmp(val.val.s, "Daikin") == 0)
      app_ac_set_brand(AC_BRAND_DAIKIN);
    else if (strcmp(val.val.s, "Samsung") == 0)
      app_ac_set_brand(AC_BRAND_SAMSUNG);
    else if (strcmp(val.val.s, "Mitsubishi") == 0)
      app_ac_set_brand(AC_BRAND_MITSUBISHI);
    else if (strcmp(val.val.s, "Panasonic") == 0)
      app_ac_set_brand(AC_BRAND_PANASONIC);
    else if (strcmp(val.val.s, "LG") == 0)
      app_ac_set_brand(AC_BRAND_LG);
    else {
      // Direct Custom Brand Selection
      ESP_LOGI(TAG, "Selected Custom Brand: %s", val.val.s);

      // Update hidden param for consistency
      esp_rmaker_param_t *custom_p = esp_rmaker_device_get_param_by_name(
          ac_device, PARAM_CUSTOM_BRAND_NAME);
      if (custom_p) {
        esp_rmaker_param_update_and_report(custom_p, esp_rmaker_str(val.val.s));
      }

      app_ac_set_custom_brand(val.val.s);
    }
    esp_rmaker_param_update_and_report(param, val);

  } else if (strcmp(param_name, PARAM_CUSTOM_BRAND_NAME) == 0) {
    ESP_LOGI(TAG, "Received '%s' = %s", param_name, val.val.s);
    app_ac_set_custom_brand(val.val.s);

    // Turn "Brand" param to this custom name if possible?
    // Actually, if we just updated the valid list, maybe we should update
    // "Brand" to this value.
    esp_rmaker_param_t *brand_p =
        esp_rmaker_device_get_param_by_name(ac_device, "Brand");
    if (brand_p) {
      esp_rmaker_param_update_and_report(brand_p, esp_rmaker_str(val.val.s));
    }
    esp_rmaker_param_update_and_report(param, val);
  }

  // Send Command via Shared Logic
  app_ac_send();

  return ESP_OK;
}

esp_err_t app_rainmaker_init(void) {
  /* Initialize ESP RainMaker Agent */
  esp_rmaker_config_t rainmaker_cfg = {
      .enable_time_sync = true,
  };
  esp_rmaker_node_t *node =
      esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "AC Remote");
  if (!node) {
    ESP_LOGE(TAG, "Could not initialize RainMaker Node. Aborting!!!");
    return ESP_FAIL;
  }

  /* Create a Thermostat device (AC) */
  ac_device = esp_rmaker_device_create("Air Conditioner",
                                       ESP_RMAKER_DEVICE_THERMOSTAT, NULL);

  /* Add Power (Standard) */
  esp_rmaker_device_add_param(ac_device, esp_rmaker_power_param_create(
                                             ESP_RMAKER_DEF_POWER_NAME, false));

  /* Add Temperature (Standard) - Integer 16-30 */
  esp_rmaker_param_t *temp_param = esp_rmaker_param_create(
      ESP_RMAKER_DEF_TEMPERATURE_NAME, ESP_RMAKER_PARAM_RANGE,
      esp_rmaker_float(24.0), PROP_FLAG_READ | PROP_FLAG_WRITE);
  esp_rmaker_param_add_bounds(temp_param, esp_rmaker_float(16.0),
                              esp_rmaker_float(30.0), esp_rmaker_float(1.0));
  esp_rmaker_param_add_ui_type(temp_param, ESP_RMAKER_UI_SLIDER);
  esp_rmaker_device_add_param(ac_device, temp_param);

  /* Add Mode (String Dropdown) */
  esp_rmaker_param_t *mode_param = esp_rmaker_param_create(
      "Mode", ESP_RMAKER_PARAM_MODE, esp_rmaker_str("Auto"),
      PROP_FLAG_READ | PROP_FLAG_WRITE);
  static const char *opts[] = {"Auto", "Cool", "Heat", "Fan"};
  esp_rmaker_param_add_valid_str_list(mode_param, opts, 4);
  esp_rmaker_param_add_ui_type(mode_param, ESP_RMAKER_UI_DROPDOWN);
  esp_rmaker_device_add_param(ac_device, mode_param);

  /* Add Fan Speed (String Dropdown) */
  esp_rmaker_param_t *fan_param = esp_rmaker_param_create(
      "Fan Speed", ESP_RMAKER_PARAM_MODE, esp_rmaker_str("Auto"),
      PROP_FLAG_READ | PROP_FLAG_WRITE);
  static const char *fan_opts[] = {"Low", "Medium", "High", "Auto"};
  esp_rmaker_param_add_valid_str_list(fan_param, fan_opts, 4);
  esp_rmaker_param_add_ui_type(fan_param, ESP_RMAKER_UI_DROPDOWN);
  esp_rmaker_device_add_param(ac_device, fan_param);

  /* Add Brand (String Dropdown) */
  esp_rmaker_param_t *brand_param = esp_rmaker_param_create(
      "Brand", ESP_RMAKER_PARAM_MODE, esp_rmaker_str("Daikin"),
      PROP_FLAG_READ | PROP_FLAG_WRITE);
  static const char *brand_opts[] = {"Daikin",    "Samsung", "Mitsubishi",
                                     "Panasonic", "LG",      "Custom"};
  esp_rmaker_param_add_valid_str_list(brand_param, brand_opts, 6);
  esp_rmaker_param_add_ui_type(brand_param, ESP_RMAKER_UI_DROPDOWN);
  esp_rmaker_device_add_param(ac_device, brand_param);

  /* Add Custom Brand Name (String) */
  esp_rmaker_param_t *custom_brand_param = esp_rmaker_param_create(
      PARAM_CUSTOM_BRAND_NAME, ESP_RMAKER_PARAM_NAME, esp_rmaker_str(""),
      PROP_FLAG_READ | PROP_FLAG_WRITE);
  esp_rmaker_device_add_param(ac_device, custom_brand_param);

  /* Add WebUI Config Mode (Switch) */
  esp_rmaker_param_t *webui_param = esp_rmaker_param_create(
      PARAM_WEBUI_MODE, ESP_RMAKER_PARAM_POWER, esp_rmaker_bool(false),
      PROP_FLAG_READ | PROP_FLAG_WRITE);
  esp_rmaker_param_add_ui_type(webui_param, ESP_RMAKER_UI_TOGGLE);
  esp_rmaker_device_add_param(ac_device, webui_param);

  /* Add AC device to node */
  esp_rmaker_node_add_device(node, ac_device);

#if CONFIG_APP_LED_CONTROL
  /* Create a Lightbulb device (LED) */
  led_device = esp_rmaker_device_create("LED Control",
                                        ESP_RMAKER_DEVICE_LIGHTBULB, NULL);

  /* Add Power (Standard) */
  esp_rmaker_device_add_param(
      led_device,
      esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, false));

  /* Add Hue (Standard) */
  esp_rmaker_device_add_param(
      led_device, esp_rmaker_hue_param_create(ESP_RMAKER_DEF_HUE_NAME, 0));

  /* Add Saturation (Standard) */
  esp_rmaker_device_add_param(
      led_device,
      esp_rmaker_saturation_param_create(ESP_RMAKER_DEF_SATURATION_NAME, 100));

  /* Add Brightness (Standard) */
  esp_rmaker_device_add_param(
      led_device,
      esp_rmaker_brightness_param_create(ESP_RMAKER_DEF_BRIGHTNESS_NAME, 100));

  /* Register callback */
  esp_rmaker_device_add_cb(led_device, led_write_cb, NULL);

  /* Add LED device to node */
  esp_rmaker_node_add_device(node, led_device);
#endif

  /* Register callback */
  esp_rmaker_device_add_cb(ac_device, write_cb, NULL);

  /* Start RainMaker */
  esp_rmaker_start();

  return ESP_OK;
}

void app_rainmaker_update_state(const ir_ac_state_t *state) {
  if (!ac_device || !state)
    return;

  // Power
  esp_rmaker_param_update_and_report(
      esp_rmaker_device_get_param_by_name(ac_device, ESP_RMAKER_DEF_POWER_NAME),
      esp_rmaker_bool(state->power));

  // Temp
  esp_rmaker_param_update_and_report(
      esp_rmaker_device_get_param_by_name(ac_device,
                                          ESP_RMAKER_DEF_TEMPERATURE_NAME),
      esp_rmaker_float((float)state->temp));

  // Mode
  const char *mode_str = "Auto";
  switch (state->mode) {
  case 0:
    mode_str = "Auto";
    break;
  case 1:
    mode_str = "Cool";
    break;
  case 2:
    mode_str = "Heat";
    break;
  case 3:
    mode_str = "Fan";
    break;
  case 4:
    mode_str = "Dry";
    break;
  }
  esp_rmaker_param_update_and_report(
      esp_rmaker_device_get_param_by_name(ac_device, "Mode"),
      esp_rmaker_str(mode_str));

  // Fan
  const char *fan_str = "Auto";
  switch (state->fan) {
  case 0:
    fan_str = "Auto";
    break;
  case 1:
    fan_str = "Low";
    break;
  case 2:
    fan_str = "Medium";
    break;
  case 3:
    fan_str = "High";
    break;
  }
  esp_rmaker_param_update_and_report(
      esp_rmaker_device_get_param_by_name(ac_device, "Fan Speed"),
      esp_rmaker_str(fan_str));

  // Brand
  if (app_ac_is_custom_brand()) {
    const char *name = app_ac_get_custom_brand();
    if (name) {
      // Update Brand Dropdown to show the custom name
      esp_rmaker_param_update_and_report(
          esp_rmaker_device_get_param_by_name(ac_device, "Brand"),
          esp_rmaker_str(name));

      // Also update the hidden/helper param
      esp_rmaker_param_update_and_report(
          esp_rmaker_device_get_param_by_name(ac_device,
                                              PARAM_CUSTOM_BRAND_NAME),
          esp_rmaker_str(name));
    }
  } else {
    const char *brand_str = "Daikin";
    switch (app_ac_get_brand()) {
    case AC_BRAND_DAIKIN:
      brand_str = "Daikin";
      break;
    case AC_BRAND_SAMSUNG:
      brand_str = "Samsung";
      break;
    case AC_BRAND_MITSUBISHI:
      brand_str = "Mitsubishi";
      break;
    case AC_BRAND_PANASONIC:
      brand_str = "Panasonic";
      break;
    case AC_BRAND_LG:
      brand_str = "LG";
      break;
    default:
      break;
    }
    esp_rmaker_param_update_and_report(
        esp_rmaker_device_get_param_by_name(ac_device, "Brand"),
        esp_rmaker_str(brand_str));
  }
}

void app_rainmaker_update_custom_brands(char **brands, size_t count) {
  if (!ac_device)
    return;

  esp_rmaker_param_t *brand_param =
      esp_rmaker_device_get_param_by_name(ac_device, "Brand");
  if (!brand_param)
    return;

  // 1. Allocate and Prepare New List
  // Standard brands + Custom brands
  const char *standard_brands[] = {"Daikin", "Samsung", "Mitsubishi",
                                   "Panasonic", "LG"};
  size_t std_count = 5;
  size_t total_count = std_count + count;

  char **new_list = malloc(total_count * sizeof(char *));
  if (!new_list) {
    ESP_LOGE(TAG, "Failed to allocate memory for brands list");
    return;
  }

  // Copy standard brands
  for (size_t i = 0; i < std_count; i++) {
    new_list[i] = strdup(standard_brands[i]);
  }

  // Copy custom brands
  for (size_t i = 0; i < count; i++) {
    new_list[std_count + i] = strdup(brands[i]);
  }

  // 2. Capture Old List for Cleanup
  char **old_list = g_brand_str_list;
  size_t old_count = g_brand_list_count;

  // 3. Update Global State
  g_brand_str_list = new_list;
  g_brand_list_count = total_count;

  // 4. Update RainMaker Param
  // RainMaker will now point to new_list.
  // We needed to keep new_list allocated (which we do in global).
  esp_err_t err = esp_rmaker_param_add_valid_str_list(
      brand_param, (const char *const *)new_list, total_count);

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Updated RainMaker Brand list with %d items", total_count);
    esp_rmaker_report_node_details();
  } else {
    ESP_LOGE(TAG, "Failed to update RainMaker Brand list");
  }

  // 5. Free Old List (Safe now)
  if (old_list) {
    for (size_t i = 0; i < old_count; i++) {
      if (old_list[i])
        free(old_list[i]);
    }
    free(old_list);
  }
}

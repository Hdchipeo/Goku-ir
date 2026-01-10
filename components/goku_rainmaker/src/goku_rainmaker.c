#include "goku_rainmaker.h"
#include "goku_ac.h"
#include "goku_data.h"
#include "goku_ir_app.h"
#include "goku_led.h"
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
esp_rmaker_device_t *ac_device = NULL;
esp_rmaker_device_t *led_device = NULL;

static bool g_power = false;
static int g_hue = 0;
static int g_saturation = 100;
static int g_brightness = 100;

// Global AC State
// Removed local state, using app_ac.h

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
  static const char *brand_opts[] = {"Daikin", "Samsung", "Mitsubishi",
                                     "Panasonic", "LG"};
  esp_rmaker_param_add_valid_str_list(brand_param, brand_opts, 5);
  esp_rmaker_param_add_ui_type(brand_param, ESP_RMAKER_UI_DROPDOWN);
  esp_rmaker_device_add_param(ac_device, brand_param);

  /* Add AC device to node */
  esp_rmaker_node_add_device(node, ac_device);

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

  /* Register callback */
  esp_rmaker_device_add_cb(ac_device, write_cb, NULL);

  /* Start RainMaker */
  esp_rmaker_start();

  return ESP_OK;
}

void app_rainmaker_update_state(bool power_on) {
  if (ac_device) {
    esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_name(
        ac_device, ESP_RMAKER_DEF_POWER_NAME);
    if (param) {
      esp_rmaker_param_update_and_report(param, esp_rmaker_bool(power_on));
    }
  }
}

#include "goku_homekit.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <string.h>

// Only compile if iOS Platform is selected
#if CONFIG_GOKU_PLATFORM_IOS

#include "app_hap_setup_payload.h"
#include "goku_ac.h"
#include "goku_web.h"
#include "goku_wifi.h"
#include "hap.h"
#include "hap_apple_chars.h"
#include "hap_apple_servs.h"

// Optional Sensor Support
#if CONFIG_GOKU_SENSOR_AHT20
#include "aht20_sensor.h"
#endif

static const char *TAG = "goku_homekit";

static hap_serv_t *s_thermostat_service = NULL;
static hap_char_t *s_curr_temp_char = NULL;
static hap_char_t *s_target_temp_char = NULL;
static hap_char_t *s_curr_state_char = NULL;
static hap_char_t *s_target_state_char = NULL;

// Optional Sensor Services
#if CONFIG_GOKU_SENSOR_AHT20
static hap_serv_t *s_temp_sensor_service = NULL;
static hap_serv_t *s_hum_sensor_service = NULL;
static hap_char_t *s_sensor_curr_temp_char = NULL;
static hap_char_t *s_sensor_curr_hum_char = NULL;
#endif

// Web UI Switch
static hap_serv_t *s_web_switch_service = NULL;
static hap_char_t *s_web_switch_on_char = NULL;

/* HAP Main Task */
static void hap_task(void *p) {
  // HAP manages its own tasks internally, just keep this task alive
  // and update sensor data if enabled
  while (1) {
#if CONFIG_GOKU_SENSOR_AHT20
    float temp = 0.0f;
    float hum = 0.0f;
    if (aht20_sensor_read(&temp, &hum) == ESP_OK) {
      // Update Temperature Sensor Service
      if (s_sensor_curr_temp_char) {
        hap_val_t val = {.f = temp};
        hap_char_update_val(s_sensor_curr_temp_char, &val);
      }
      // Update Humidity Sensor Service
      if (s_sensor_curr_hum_char) {
        hap_val_t val = {.f = hum};
        hap_char_update_val(s_sensor_curr_hum_char, &val);
      }
      // Also update Thermostat Current Temperature if desired
      if (s_curr_temp_char) {
        hap_val_t val = {.f = temp};
        hap_char_update_val(s_curr_temp_char, &val);
      }
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(5000)); // Check every 5 seconds
  }
  vTaskDelete(NULL);
}

/* Mandatory Identify callback */
static int hap_identify(hap_acc_t *ha) {
  ESP_LOGI(TAG, "Accessory Identification Requested");
  return HAP_SUCCESS;
}

/*
 * Callback for HomeKit write operations
 */
static int thermostat_write(hap_write_data_t write_data[], int count,
                            void *serv_priv, void *write_priv) {
  ESP_LOGI(TAG, "HomeKit Write Request: %d items", count);

  ir_ac_state_t state;
  app_ac_get_state(&state);
  bool changed = false;

  for (int i = 0; i < count; i++) {
    hap_write_data_t *data = &write_data[i];
    const char *uuid = hap_char_get_type_uuid(data->hc);

    ESP_LOGI(TAG, "Writing Char: %s", uuid);

    if (strcmp(uuid, HAP_CHAR_UUID_TARGET_HEATING_COOLING_STATE) == 0) {
      int val = data->val.i;
      ESP_LOGI(TAG, "Target State: %d", val);

      // Map HAP State (0:Off, 1:Heat, 2:Cool, 3:Auto) to AC State
      switch (val) {
      case 0: // Off
        state.power = false;
        break;
      case 1: // Heat
        state.power = true;
        state.mode = 2; // Heat
        break;
      case 2: // Cool
        state.power = true;
        state.mode = 1; // Cool
        break;
      case 3: // Auto
        state.power = true;
        state.mode = 0; // Auto
        break;
      }
      changed = true;

    } else if (strcmp(uuid, HAP_CHAR_UUID_TARGET_TEMPERATURE) == 0) {
      float val = data->val.f;
      ESP_LOGI(TAG, "Target Temp: %.1f", val);
      state.temp = (uint8_t)val;
      changed = true;
    }

    hap_char_update_val(data->hc, &(data->val));
    *(data->status) = HAP_STATUS_SUCCESS;
  }

  if (changed) {
    app_ac_set_state(&state);
    app_ac_send(); // Transmit IR
  }

  return HAP_SUCCESS;
  return HAP_SUCCESS;
}

static int webui_write(hap_write_data_t write_data[], int count,
                       void *serv_priv, void *write_priv) {
  for (int i = 0; i < count; i++) {
    hap_write_data_t *data = &write_data[i];
    if (strcmp(hap_char_get_type_uuid(data->hc), HAP_CHAR_UUID_ON) == 0) {
      bool enable = data->val.b;
      ESP_LOGI(TAG, "HomeKit: Web UI Switch %s", enable ? "ON" : "OFF");
      if (enable) {
        app_web_start();
      } else {
        app_web_stop();
      }
      hap_char_update_val(data->hc, &(data->val));
      *(data->status) = HAP_STATUS_SUCCESS;
    }
  }
  return HAP_SUCCESS;
}

/* Update HomeKit state from device state (Poll/Push) */
void app_homekit_update_state(const ir_ac_state_t *state) {
  if (!s_thermostat_service)
    return;

  hap_val_t val;

  // Current State (HAP mapping)
  int curr_state = 0;
  if (state->power) {
    if (state->mode == 2)
      curr_state = 2; // Heating
    else if (state->mode == 1)
      curr_state = 3; // Cooling
    else
      curr_state = 2; // Default active
  }

  val.i = curr_state;
  hap_char_update_val(s_curr_state_char, &val);

  // Target State
  int target_state = 0; // Off
  if (state->power) {
    if (state->mode == 2)
      target_state = 1; // Heat
    else if (state->mode == 1)
      target_state = 2; // Cool
    else
      target_state = 3; // Auto
  }
  val.i = target_state;
  hap_char_update_val(s_target_state_char, &val);

  // Temps
  val.f = (float)state->temp;
  hap_char_update_val(s_curr_temp_char, &val);
  hap_char_update_val(s_target_temp_char, &val);
}

static bool s_is_inited = false;

// ... (existing code)

bool app_homekit_is_paired(void) {
  if (!s_is_inited) {
    hap_init(HAP_TRANSPORT_WIFI);
    s_is_inited = true;
  }
  return (hap_get_paired_controller_count() > 0);
}

esp_err_t app_homekit_init(void) {
  ESP_LOGI(TAG, "Initializing HomeKit (Bridge Mode)...");

  // 1. Initialize HAP (if not already done via is_paired check)
  if (!s_is_inited) {
    // Suppress verbose HAP logs (Value Changed, Notification Sent, etc.)
    hap_set_debug_level(HAP_DEBUG_LEVEL_WARN);

    hap_init(HAP_TRANSPORT_WIFI);
    s_is_inited = true;
  }

  // -------------------------------------------------------------------------
  // 2. Create BRIDGE Accessory (Primary)
  // -------------------------------------------------------------------------
  hap_acc_cfg_t bridge_cfg = {
      .name = "Goku Bridge",
      .manufacturer = "Goku Inc",
      .model = "GK-BRIDGE-01",
      .serial_num = "001122334400",
      .fw_rev = "1.3.0",
      .hw_rev = "1.0",
      .pv = "1.1.0",
      .identify_routine = hap_identify,
      .cid = HAP_CID_BRIDGE,
  };
  hap_acc_t *bridge_acc = hap_acc_create(&bridge_cfg);
  if (!bridge_acc) {
    ESP_LOGE(TAG, "Failed to create Bridge accessory");
    return ESP_FAIL;
  }

  // Add Product Data & Transport to Bridge
  uint8_t product_data[] = {'E', 'S', 'P', '3', '2', 'H', 'K', 'T'};
  hap_acc_add_product_data(bridge_acc, product_data, sizeof(product_data));
  hap_acc_add_wifi_transport_service(bridge_acc, 0);

  // Add Bridge to HAP Database (Primary) - CRITICAL!
  hap_add_accessory(bridge_acc);

  // -------------------------------------------------------------------------
  // 3. Create AC Accessory (Bridged)
  // -------------------------------------------------------------------------
  hap_acc_cfg_t ac_cfg = {
      .name = "Goku AC",
      .manufacturer = "Goku Inc",
      .model = "GK-IR-01",
      .serial_num = "001122334488",
      .fw_rev = "1.3.0",
      .hw_rev = "1.0",
      .pv = "1.1.0",
      .identify_routine = hap_identify,
      .cid = HAP_CID_THERMOSTAT,
  };
  hap_acc_t *ac_acc = hap_acc_create(&ac_cfg);
  if (ac_acc) {
    hap_acc_add_product_data(ac_acc, product_data, sizeof(product_data));

    // Create Thermostat Service
    s_thermostat_service = hap_serv_thermostat_create(0, 0, 24.0, 24.0, 0);

    // Get handles
    s_curr_temp_char = hap_serv_get_char_by_uuid(
        s_thermostat_service, HAP_CHAR_UUID_CURRENT_TEMPERATURE);
    s_target_temp_char = hap_serv_get_char_by_uuid(
        s_thermostat_service, HAP_CHAR_UUID_TARGET_TEMPERATURE);
    s_curr_state_char = hap_serv_get_char_by_uuid(
        s_thermostat_service, HAP_CHAR_UUID_CURRENT_HEATING_COOLING_STATE);
    s_target_state_char = hap_serv_get_char_by_uuid(
        s_thermostat_service, HAP_CHAR_UUID_TARGET_HEATING_COOLING_STATE);

    // Set Bounds & Callback
    hap_char_float_set_constraints(s_target_temp_char, 16.0, 32.0, 1.0);
    hap_serv_set_write_cb(s_thermostat_service, thermostat_write);
    hap_acc_add_serv(ac_acc, s_thermostat_service);

// Optional Sensors
#if CONFIG_GOKU_SENSOR_AHT20
    s_temp_sensor_service = hap_serv_temperature_sensor_create(0.0f);
    if (s_temp_sensor_service) {
      s_sensor_curr_temp_char = hap_serv_get_char_by_uuid(
          s_temp_sensor_service, HAP_CHAR_UUID_CURRENT_TEMPERATURE);
      hap_acc_add_serv(ac_acc, s_temp_sensor_service);
    }
    s_hum_sensor_service = hap_serv_humidity_sensor_create(0.0f);
    if (s_hum_sensor_service) {
      s_sensor_curr_hum_char = hap_serv_get_char_by_uuid(
          s_hum_sensor_service, HAP_CHAR_UUID_CURRENT_RELATIVE_HUMIDITY);
      hap_acc_add_serv(ac_acc, s_hum_sensor_service);
    }
#endif

    // Add AC as Bridged Accessory
    hap_add_bridged_accessory(ac_acc, hap_get_unique_aid("Goku AC"));
    ESP_LOGI(TAG, "Added Bridged AC Accessory");
  }

  // -------------------------------------------------------------------------
  // 4. Create Web Switch Accessory (Bridged)
  // -------------------------------------------------------------------------
  hap_acc_cfg_t web_cfg = {
      .name = "Web Config",
      .manufacturer = "Goku Inc",
      .model = "GK-WEB-01",
      .serial_num = "001122334499",
      .fw_rev = "1.0.0",
      .hw_rev = "1.0",
      .pv = "1.1.0",
      .identify_routine = hap_identify,
      .cid = HAP_CID_SWITCH, // Category Switch
  };
  hap_acc_t *web_acc = hap_acc_create(&web_cfg);
  if (web_acc) {
    hap_acc_add_product_data(web_acc, product_data, sizeof(product_data));

    s_web_switch_service = hap_serv_switch_create(false); // Default OFF
    if (s_web_switch_service) {
      hap_serv_add_char(s_web_switch_service,
                        hap_char_name_create("Web Config Mode"));
      s_web_switch_on_char =
          hap_serv_get_char_by_uuid(s_web_switch_service, HAP_CHAR_UUID_ON);
      hap_serv_set_write_cb(s_web_switch_service, webui_write);

      // Add Service to WEB Accessory
      hap_acc_add_serv(web_acc, s_web_switch_service);

      hap_add_bridged_accessory(web_acc, hap_get_unique_aid("Web Config"));
      ESP_LOGI(TAG, "Added Bridged Web Switch Accessory");
    }
  }

  hap_set_setup_code("111-22-333");
  hap_set_setup_id("GK4C");

  // Print Setup Instructions
  ESP_LOGI(TAG, "================================================");
  ESP_LOGI(TAG, "  HomeKit Pairing Setup (Bridge Mode)");
  ESP_LOGI(TAG, "  Setup Code: 111-22-333");
  ESP_LOGI(TAG, "  Setup ID: GK4C");
  ESP_LOGI(TAG, "================================================");

  // QR Code for Bridge
  app_hap_setup_payload("111-22-333", "GK4C", false, HAP_CID_BRIDGE);

  // ✅ CRITICAL: Restore state from AC module BEFORE hap_start()
  // This ensures HomeKit displays correct state after reboot
  ESP_LOGI(TAG, "Restoring HomeKit characteristics from AC state...");

  ir_ac_state_t current_state;
  app_ac_get_state(&current_state);

  // Restore Temperature characteristics
  if (s_curr_temp_char && s_target_temp_char) {
    hap_val_t temp_val = {.f = (float)current_state.temp};
    hap_char_update_val(s_target_temp_char, &temp_val);
    ESP_LOGI(TAG, "Restored target temperature: %.0f°C", temp_val.f);
  }

  // Restore State characteristics (Map AC mode to HomeKit states)
  if (s_curr_state_char && s_target_state_char) {
    int hk_state = 0; // 0=OFF, 1=HEAT, 2=COOL, 3=AUTO

    if (current_state.power) {
      if (current_state.mode == 2) {
        hk_state = 1; // Heat
      } else if (current_state.mode == 1) {
        hk_state = 2; // Cool
      } else {
        hk_state = 3; // Auto or other modes
      }
    } else {
      hk_state = 0; // OFF
    }

    hap_val_t state_val = {.i = hk_state};
    hap_char_update_val(s_curr_state_char, &state_val);
    hap_char_update_val(s_target_state_char, &state_val);

    const char *state_names[] = {"OFF", "HEAT", "COOL", "AUTO"};
    ESP_LOGI(TAG, "Restored thermostat state: %s (AC power=%d, mode=%d)",
             state_names[hk_state], current_state.power, current_state.mode);
  }

  ESP_LOGI(TAG, "HomeKit state restoration complete");

  hap_enable_mfi_auth(HAP_MFI_AUTH_NONE);
  hap_start();
  xTaskCreate(hap_task, "hap_task", 8192, NULL, 1, NULL);
  ESP_LOGI(TAG, "HomeKit Bridge Started.");

  return ESP_OK;
}

#else

esp_err_t app_homekit_init(void) {
  return ESP_OK; // Dummy
}

#endif

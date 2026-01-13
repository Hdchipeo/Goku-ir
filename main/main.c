#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#include "goku_button.h"
#include "goku_data.h"
#include "goku_ir_app.h"
#if CONFIG_APP_LED_CONTROL
#include "goku_led.h"
#endif
#include "aht20_sensor.h"
#include "goku_ac.h"
#include "goku_homekit.h"
#include "goku_log.h"
#include "goku_mdns.h"
#include "goku_mem.h"
#include "goku_ota.h"
#include "goku_rainmaker.h"
#include "goku_web.h"
#include "goku_wifi.h"

// RainMaker headers
#include <esp_rmaker_common_events.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_params.h>

#define TAG "main"

// Suppress verbose RainMaker param logs (sensor reporting spam)
__attribute__((constructor)) static void suppress_rmaker_logs(void) {
  esp_log_level_set("esp_rmaker_param", ESP_LOG_WARN);
}
#if CONFIG_GOKU_PLATFORM_ANDROID
static void web_ui_toggle_cb(bool enable) {
  if (enable) {
    app_web_start();
  } else {
    app_web_stop();
  }
}
#endif

#if CONFIG_GOKU_PLATFORM_IOS

// Event handler for iOS to start HomeKit after WiFi connection
static void ios_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data) {
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI(TAG, "WiFi connected, initializing HomeKit");

    bool is_paired = app_homekit_is_paired();
    if (!is_paired) {
      // If Not Paired: Init mDNS FIRST
      // This ensures hostname is set to 'gokuac' before HomeKit
      // starts/broadcasts
      ESP_LOGI(TAG, "HomeKit NOT Paired: Starting mDNS before HomeKit");
      app_mdns_init();
      app_homekit_init();
    } else {
      // If Paired: Init HomeKit FIRST
      // This ensures HomeKit starts normally, then we refresh/announce mDNS
      // to ensure Web Config (port 8080) is discoverable
      ESP_LOGI(TAG, "HomeKit Paired: Starting HomeKit before mDNS");
      app_homekit_init();
      app_mdns_init();
    }
  }
}
#endif

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data) {
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    app_ota_mark_valid();
#if !CONFIG_GOKU_PLATFORM_IOS
    // Android/Generic: Start mDNS here (No HomeKit conflict)
    app_mdns_init();
#endif
  }
}

void app_main(void) {
  app_log_init();
  app_mem_init();
  ESP_LOGI(TAG, "Starting Goku IR Device...");
  ESP_LOGI(TAG, "Project version: %s", PROJECT_VERSION);

  // 1. Initialize NVS (Non-Volatile Storage)
  ESP_ERROR_CHECK(app_data_init());

  // 2. Initialize Network Stack
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &ip_event_handler, NULL));

  // 3. Initialize IR Remote (Priority for RMT resources)
  if (app_ir_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init IR");
  }

  // 4. Initialize Peripherals (LED, Button)
#if CONFIG_APP_LED_CONTROL
  ESP_ERROR_CHECK(app_led_init());
  app_led_set_state(APP_LED_STARTUP);
#endif

  ESP_ERROR_CHECK(app_button_init());

  // 5. Initialize AC Logic (Load state from NVS)
  app_ac_init();

  // 6. Initialize Wi-Fi Stack
  // Note: RainMaker requires custom provisioning logic mostly handled here.
  app_wifi_init();

  // 7. Initialize AHT20 Sensor (Optional)
#if CONFIG_GOKU_SENSOR_AHT20
  if (aht20_sensor_init() == ESP_OK) {
    ESP_LOGI(TAG, "AHT20 Sensor Initialized");
  } else {
    ESP_LOGW(TAG, "AHT20 Sensor Init Failed");
  }
#endif

  // 8. Initialize Mobile Platform Logic
#if CONFIG_GOKU_PLATFORM_ANDROID
  // [Android] Initialize RainMaker
  app_rainmaker_register_webui_toggle(web_ui_toggle_cb);
  app_rainmaker_init();
#elif CONFIG_GOKU_PLATFORM_IOS
  // [iOS] Register event handler to init HomeKit after WiFi connected
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &ios_wifi_event_handler, NULL));
  ESP_LOGI(TAG, "iOS mode: HomeKit will start after WiFi connection");
#endif

  // 9. Start Provisioning / Network
  app_wifi_start(NULL);

  // 10. mDNS will be initialized automatically after WiFi connects (in
  // goku_wifi.c event handler)

  // 11. Initialize Web Server resources
  // On iOS, we init but don't start it automatically (avoid conflict).
  if (app_web_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init Web Server resources");
  }
#if CONFIG_GOKU_PLATFORM_IOS
  ESP_LOGI(TAG,
           "iOS mode: Web server initialized but stopped (Toggle via HomeKit)");
#endif

  // 12. Initialize Automatic OTA
  // Periodically checks for firmware updates from CONFIG_OTA_SERVER_URL.
  app_ota_auto_init();

  ESP_LOGI(TAG, "Initialization Complete. Device ready.");

  // Main Loop - Keep task alive (or can be deleted)
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

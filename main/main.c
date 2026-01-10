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

static void web_ui_toggle_cb(bool enable) {
  if (enable) {
    app_web_start();
  } else {
    app_web_stop();
  }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data) {
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    app_ota_mark_valid();
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

  // 5. Initialize Wi-Fi Stack
  // Note: RainMaker requires custom provisioning logic mostly handled here.
  app_wifi_init();

  // 6. Initialize RainMaker Node
  // Registers Devices, Parameters, and Provisioning Endpoints.
  app_rainmaker_register_webui_toggle(web_ui_toggle_cb);
  app_rainmaker_init();

  // 7. Start Provisioning / Network
  // Start BLE Provisioning (Standard RainMaker)
  app_wifi_start(NULL);

  // 8. Initialize mDNS
  // Hostname: gokuac.local
  ESP_ERROR_CHECK(app_mdns_init());

  // 9. Initialize Web Server
  // Starts web interface for manual control via browser.
  if (app_web_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init Web Server resources");
  }

  // 10. Initialize Automatic OTA
  // Periodically checks for firmware updates from CONFIG_OTA_SERVER_URL.
  app_ota_auto_init();

  ESP_LOGI(TAG, "Initialization Complete. Device ready.");

  // Main Loop - Keep task alive (or can be deleted)
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

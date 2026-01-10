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
#include "goku_led.h"
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

  // 3. Initialize IR Remote (Priority for RMT resources)
  if (app_ir_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init IR");
  }

  // 4. Initialize Peripherals (LED, Button)
  ESP_ERROR_CHECK(app_led_init());
  app_led_set_state(APP_LED_STARTUP);

  ESP_ERROR_CHECK(app_button_init());

  // 5. Initialize Wi-Fi Stack
  // Note: RainMaker requires custom provisioning logic mostly handled here.
  app_wifi_init();

  // 6. Initialize RainMaker Node
  // Registers Devices, Parameters, and Provisioning Endpoints.
  app_rainmaker_init();

  // 7. Start Provisioning / Network
  // Starts SoftAP provisioning transport.
  ESP_ERROR_CHECK(app_wifi_start());

  // 8. Initialize mDNS
  // Hostname: gokuac.local
  ESP_ERROR_CHECK(app_mdns_init());

  // 9. Initialize Web Server
  // Starts web interface for manual control via browser.
  if (app_web_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init Web Server");
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

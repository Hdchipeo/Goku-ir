#include "goku_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "goku_led.h"
#include <network_provisioning/manager.h>
#include <network_provisioning/scheme_ble.h>
#include <string.h>

static const char *TAG = "goku_wifi";
static const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;
static bool s_reconnect = true; // Control auto-reconnect

/**
 * @brief Event handler to manage Wi-Fi events
 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
#if CONFIG_APP_LED_CONTROL
    app_led_set_state(APP_LED_WIFI_CONN);
#endif
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_reconnect) {
      esp_wifi_connect();
#if CONFIG_APP_LED_CONTROL
      app_led_set_state(APP_LED_WIFI_CONN);
#endif
      ESP_LOGI(TAG, "Retry to connect to the AP");
    } else {
      ESP_LOGI(TAG, "Auto-reconnect disabled (scanning/updating)");
    }

    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_EVENT);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
#if CONFIG_APP_LED_CONTROL
    app_led_set_state(APP_LED_IDLE);
#endif
  }
}

void app_wifi_init(void) {
  // Helpers to initialize default Wi-Fi station configuration
  // RainMaker initializes netif if not already done, but we do it here for
  // explicit control if needed. Actually, RainMaker examples often init netif
  // in main. checking main.c: esp_netif_init() is called.

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));
}

// app_wifi_start restored for RainMaker BLE Provisioning

static void get_device_service_name(char *service_name, size_t max) {
  uint8_t eth_mac[6];
  const char *ssid_prefix = "PROV_";
  esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
  snprintf(service_name, max, "%s%02X%02X%02X", ssid_prefix, eth_mac[3],
           eth_mac[4], eth_mac[5]);
}

esp_err_t app_wifi_start(void *pop_info) {

  /* Initialize provisioning manager with BLE scheme */
  network_prov_mgr_config_t config = {
      .scheme = network_prov_scheme_ble,
      .scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM};

  ESP_ERROR_CHECK(network_prov_mgr_init(config));

  bool provisioned = false;
  ESP_ERROR_CHECK(network_prov_mgr_is_wifi_provisioned(&provisioned));

  if (!provisioned) {
    ESP_LOGI(TAG, "Starting provisioning");

#if CONFIG_APP_LED_CONTROL
    app_led_set_state(APP_LED_WIFI_CONN); // Or Provisioning state
#endif

    /* What is the Proof of Possession (PoP) string? */
    // RainMaker standard for Assistive Claiming usually involves random POP.
    // We will use Security 1.

    network_prov_security_t security = NETWORK_PROV_SECURITY_1;
    const char *pop = (const char *)pop_info;
    if (pop == NULL) {
      pop = "12345678";
    }

    char service_name[12];
    get_device_service_name(service_name, sizeof(service_name));

    // Set separate event handler for Provisioning specific events?
    // We reuse system event loop.

    ESP_ERROR_CHECK(
        network_prov_mgr_start_provisioning(security, pop, service_name, NULL));

    ESP_LOGI(TAG, "Provisioning Started. Name: %s, POP: %s", service_name, pop);

  } else {
    ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");

    network_prov_mgr_deinit();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    // esp_wifi_connect() is called by event handler on WIFI_EVENT_STA_START
  }

  return ESP_OK;
}

esp_err_t app_wifi_update_credentials(const char *ssid, const char *password) {
  wifi_config_t wifi_config = {0};
  strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, password,
          sizeof(wifi_config.sta.password));

  ESP_LOGI(TAG, "Updating Wi-Fi to SSID: %s", ssid);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  esp_restart();
  return ESP_OK;
}

esp_err_t app_wifi_get_scan_results(wifi_ap_record_t **ap_list,
                                    uint16_t *count) {
  bool was_connected = app_wifi_is_connected();

  if (!was_connected) {
    // Disable auto-reconnect to prevent interference during scan
    s_reconnect = false;

    // Disconnect to ensure we are not in "connecting" state which blocks scan
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100)); // Give time for state transition
  }

  wifi_scan_config_t scan_config = {
      .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true};

  ESP_LOGI(TAG, "Starting Wi-Fi Scan...");
  esp_err_t err = esp_wifi_scan_start(&scan_config, true);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(err));

    if (!was_connected) {
      // Resume connection before returning
      s_reconnect = true;
      esp_wifi_connect();
    }
    return err;
  }

  uint16_t ap_count = 0;

  err = esp_wifi_scan_get_ap_num(&ap_count);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get AP count: %s", esp_err_to_name(err));
    if (!was_connected) {
      s_reconnect = true;
      esp_wifi_connect();
    }
    return err;
  }

  ESP_LOGI(TAG, "Found %d APs", ap_count);

  if (ap_count == 0) {
    *count = 0;
    *ap_list = NULL;

    if (!was_connected) {
      // Resume
      s_reconnect = true;
      esp_wifi_connect();
    }
    return ESP_OK;
  }

  *ap_list = (wifi_ap_record_t *)malloc(ap_count * sizeof(wifi_ap_record_t));
  if (*ap_list == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for scan results");

    if (!was_connected) {
      // Resume
      s_reconnect = true;
      esp_wifi_connect();
    }
    return ESP_ERR_NO_MEM;
  }

  err = esp_wifi_scan_get_ap_records(&ap_count, *ap_list);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(err));
    free(*ap_list);
    *ap_list = NULL;
    *count = 0;

    if (!was_connected) {
      s_reconnect = true;
      esp_wifi_connect();
    }
    return err;
  }
  *count = ap_count;

  if (!was_connected) {
    // Re-enable auto-reconnect and restart connection
    s_reconnect = true;
    esp_wifi_connect();
  }

  return ESP_OK;
}

bool app_wifi_is_connected(void) {
  EventBits_t bits = xEventGroupGetBits(wifi_event_group);
  return (bits & WIFI_CONNECTED_EVENT) ? true : false;
}

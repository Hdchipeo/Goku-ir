#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <string.h>

#include "app_led.h"
#include "app_wifi.h"
#include "sdkconfig.h"
#include <esp_rmaker_user_mapping.h>
#include <network_provisioning/manager.h>
#ifdef CONFIG_APP_WIFI_PROV_TRANSPORT_BLE
#include <network_provisioning/scheme_ble.h>
#else
#include <network_provisioning/scheme_softap.h>
#endif

static const char *TAG = "app_wifi";
static const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;
static bool s_reconnect = true; // Control auto-reconnect

#ifdef CONFIG_APP_WIFI_PROV_TRANSPORT_BLE
#define PROV_TRANSPORT_NET_PROV_SCHEME network_prov_scheme_ble
#define PROV_TRANSPORT_STR "ble"
#else
#define PROV_TRANSPORT_NET_PROV_SCHEME network_prov_scheme_softap
#define PROV_TRANSPORT_STR "softap"
#endif

#define PROV_SECURITY_VER NETWORK_PROV_SECURITY_1
#define PROV_POP CONFIG_APP_PROV_POP // Proof of Possession

/**
 * @brief Event handler to manage Wi-Fi and Provisioning events
 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
    app_led_set_state(APP_LED_WIFI_CONN);
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_reconnect) {
      esp_wifi_connect();
      app_led_set_state(APP_LED_WIFI_CONN);
      ESP_LOGI(TAG, "Retry to connect to the AP");
    } else {
      ESP_LOGI(TAG, "Auto-reconnect disabled (scaning/updating)");
    }

    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_EVENT);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    app_led_set_state(APP_LED_IDLE);

    // If we were in AP+STA (Fallback) mode, switch to pure STA to turn off AP
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_APSTA) {
      ESP_LOGI(TAG, "Connection successful. Stopping Fallback AP.");
      esp_wifi_set_mode(WIFI_MODE_STA);
    }
  } else if (event_base == NETWORK_PROV_EVENT) {
    switch (event_id) {
    case NETWORK_PROV_START:
      ESP_LOGI(TAG, "Provisioning started");
      app_led_set_state(APP_LED_WIFI_PROV);
      break;
    case NETWORK_PROV_WIFI_CRED_RECV: {
      wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
      ESP_LOGI(TAG, "Received Wi-Fi credentials\n\tSSID: %s",
               (const char *)wifi_sta_cfg->ssid);
      break;
    }
    case NETWORK_PROV_WIFI_CRED_FAIL: {
      network_prov_wifi_sta_fail_reason_t *reason =
          (network_prov_wifi_sta_fail_reason_t *)event_data;
      ESP_LOGE(TAG,
               "Provisioning failed!\n\tReason : %s"
               "\n\tPlease reset to factory and retry provisioning",
               (*reason == NETWORK_PROV_WIFI_STA_AUTH_ERROR)
                   ? "Wi-Fi Authentication Failure"
                   : "Wi-Fi AP not found");
      break;
    }
    case NETWORK_PROV_WIFI_CRED_SUCCESS:
      ESP_LOGI(TAG, "Provisioning successful");
      break;
    case NETWORK_PROV_END:
      network_prov_mgr_deinit();
      break;
    default:
      break;
    }
  }
}

void app_wifi_init(void) {
  // Helpers to initialize default Wi-Fi station and AP configurations
  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(
      NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
}

esp_err_t app_wifi_start(void) {
  /* Initialize provisioning manager with selected scheme */
  network_prov_mgr_config_t config = {.scheme = PROV_TRANSPORT_NET_PROV_SCHEME,
                                      .scheme_event_handler =
                                          NETWORK_PROV_EVENT_HANDLER_NONE};
  ESP_ERROR_CHECK(network_prov_mgr_init(config));

  bool provisioned = false;
  ESP_ERROR_CHECK(network_prov_mgr_is_wifi_provisioned(&provisioned));

  if (!provisioned) {
    ESP_LOGI(TAG, "Starting provisioning");

    /* Start provisioning service */
    char service_name[] = CONFIG_APP_PROV_SERVICE_NAME;
    char service_key[] = PROV_POP;

    /* Use Security 1 (PoP) */
    network_prov_security1_params_t *sec_params = PROV_POP;

    ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(
        PROV_SECURITY_VER, sec_params, service_name, service_key));

    ESP_LOGI(TAG, "Provisioning Started. Connect to Wi-Fi '%s', Pass '%s'",
             service_name, service_key);
    ESP_LOGI(TAG,
             "QR Payload: "
             "{\"ver\":\"v1\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":"
             "\"%s\"}",
             service_name, service_key, PROV_TRANSPORT_STR);

  } else {
    ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");
    network_prov_mgr_deinit();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    app_led_set_state(APP_LED_WIFI_CONN);
  }

  /* Wait for Wi-Fi connection */
  ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
  EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT,
                                         false, true, pdMS_TO_TICKS(10000));

  if (bits & WIFI_CONNECTED_EVENT) {
    ESP_LOGI(TAG, "Wi-Fi Connected");
  } else {
    ESP_LOGW(TAG, "Wi-Fi Connection Timed Out. Starting Fallback AP.");

    // Switch to AP+STA to allow recovery while keeping STA active for retry
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t ap_config = {
        .ap =
            {
                .ssid = CONFIG_APP_WIFI_FALLBACK_SSID,
                .ssid_len = sizeof(CONFIG_APP_WIFI_FALLBACK_SSID) - 1,
                .channel = 1,
                .password = CONFIG_APP_WIFI_FALLBACK_PASSWORD,
                .max_connection = 4,
                .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            },
    };
    if (strlen(CONFIG_APP_WIFI_FALLBACK_PASSWORD) == 0) {
      ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_LOGW(TAG, "Fallback AP '%s' started. Connect to configure.",
             CONFIG_APP_WIFI_FALLBACK_SSID);
    app_led_set_state(APP_LED_WIFI_PROV);
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
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
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

  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, *ap_list));
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

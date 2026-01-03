#include "app_ota.h"
#include "app_led.h"
#include "app_wifi.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "app_ota";

/* Struct for OTA Task arguments */
typedef struct {
  char url[256];
} ota_task_args_t;

static void ota_task(void *pvParameter) {
  ota_task_args_t *args = (ota_task_args_t *)pvParameter;
  ESP_LOGI(TAG, "Starting OTA task with URL: %s", args->url);
  app_led_set_state(APP_LED_OTA);

  esp_http_client_config_t config = {
      .url = args->url,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .keep_alive_enable = true,
      .skip_cert_common_name_check = true // Optional: relax checks for testing
  };

  esp_https_ota_config_t ota_config = {
      .http_config = &config,
  };

  // Perform HTTPS OTA Update
  esp_err_t ret = esp_https_ota(&ota_config);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "OTA Success! Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
  } else {
    ESP_LOGE(TAG, "OTA Failed: %s", esp_err_to_name(ret));
    app_led_set_state(APP_LED_IDLE);
  }

  free(args);
  vTaskDelete(NULL);
}

esp_err_t app_ota_start(const char *url) {
  if (url == NULL || strlen(url) == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  ota_task_args_t *args = malloc(sizeof(ota_task_args_t));
  if (args == NULL) {
    return ESP_ERR_NO_MEM;
  }

  strncpy(args->url, url, sizeof(args->url) - 1);
  args->url[sizeof(args->url) - 1] = 0;

  if (xTaskCreate(ota_task, "ota_task", 8192, args, 5, NULL) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create OTA task");
    free(args);
    return ESP_FAIL;
  }

  return ESP_OK;
}

static int parse_version(const char *version_str) {
  int major = 0, minor = 0, patch = 0;
  sscanf(version_str, "%d.%d.%d", &major, &minor, &patch);
  return (major * 10000) + (minor * 100) + patch;
}

esp_err_t app_ota_check_version(char *out_remote_version, size_t buf_len) {
  char url[256];
  snprintf(url, sizeof(url), "%s/version.txt", CONFIG_OTA_SERVER_URL);

  esp_http_client_config_t config = {
      .url = url,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .timeout_ms = 5000,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    ESP_LOGE(TAG, "Failed to init HTTP client");
    return ESP_FAIL;
  }

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open connection: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return err;
  }

  int content_length = esp_http_client_fetch_headers(client);
  int read_len = 0;
  if (content_length > 0 && content_length < buf_len) {
    read_len = esp_http_client_read(client, out_remote_version, content_length);
    if (read_len > 0) {
      out_remote_version[read_len] = 0; // Null terminate
      // Trim newline if present
      char *pos = strchr(out_remote_version, '\n');
      if (pos)
        *pos = 0;
    }
  } else {
    err = ESP_FAIL;
  }

  esp_http_client_cleanup(client);
  return (read_len > 0) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Background task to periodically check for firmware updates.
 */
static void auto_update_task(void *pvParameter) {
  char version_buffer[32];
  char url[256];
  int local_ver = parse_version(PROJECT_VERSION);

  ESP_LOGI(TAG, "Auto OTA Task Started. Local Version: %s (%d)",
           PROJECT_VERSION, local_ver);
  ESP_LOGI(TAG, "Server URL: %s", CONFIG_OTA_SERVER_URL);

  while (1) {
    if (!app_wifi_is_connected()) {
      ESP_LOGD(TAG, "Wi-Fi not connected, skipping OTA check");
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    if (app_ota_check_version(version_buffer, sizeof(version_buffer)) ==
        ESP_OK) {
      int remote_ver = parse_version(version_buffer);
      ESP_LOGI(TAG, "Checked Version: %s (%d)", version_buffer, remote_ver);

      if (remote_ver > local_ver) {
        ESP_LOGW(TAG, "New version found! Starting update...");
        snprintf(url, sizeof(url), "%s/goku-ir-device.bin",
                 CONFIG_OTA_SERVER_URL);
        app_ota_start(url);
      }
    } else {
      ESP_LOGE(TAG, "Failed to check version");
    }

    // Wait for interval
    vTaskDelay(pdMS_TO_TICKS(CONFIG_OTA_CHECK_INTERVAL * 1000));
  }
}

void app_ota_auto_init(void) {
  if (strlen(CONFIG_OTA_SERVER_URL) > 0 &&
      strcmp(CONFIG_OTA_SERVER_URL, "https://example.com") != 0) {
    xTaskCreate(auto_update_task, "auto_ota", 4096, NULL, 5, NULL);
  } else {
    ESP_LOGW(TAG, "Auto OTA not started: URL not configured");
  }
}

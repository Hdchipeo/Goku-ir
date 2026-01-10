#include "goku_ota.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "goku_data.h"
#include "goku_led.h"
#include "goku_wifi.h"
#include "sdkconfig.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "goku_ota";

static char g_remote_version[32] = "Unknown";
static bool g_update_available = false;
static TaskHandle_t s_ota_task_handle = NULL;

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
      .keep_alive_enable = false,
      .skip_cert_common_name_check = true, // Optional: relax checks for testing
      .buffer_size = 8192,
      .buffer_size_tx = 4096,
      .timeout_ms = 60000,
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
      .timeout_ms = 60000,
      .buffer_size = 8192,
      .buffer_size_tx = 4096,
      .keep_alive_enable = false,
      .skip_cert_common_name_check = true,
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

    // Wait for time sync (simple check for year > 2020)
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Current Year: %d", timeinfo.tm_year + 1900);
    if (timeinfo.tm_year < (2020 - 1900)) {
      ESP_LOGW(TAG, "Time not synced yet (Year %d < 2020), waiting...",
               timeinfo.tm_year + 1900);
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    if (app_ota_check_version(version_buffer, sizeof(version_buffer)) ==
        ESP_OK) {
      int remote_ver = parse_version(version_buffer);
      ESP_LOGI(TAG, "Checked Version: %s (%d)", version_buffer, remote_ver);

      // Update cache
      strncpy(g_remote_version, version_buffer, sizeof(g_remote_version) - 1);
      g_remote_version[sizeof(g_remote_version) - 1] = 0;

      if (remote_ver > local_ver) {
        g_update_available = true;
        ESP_LOGW(TAG, "New version found! Starting update...");
        snprintf(url, sizeof(url), "%s/goku-ir-device.bin",
                 CONFIG_OTA_SERVER_URL);
        app_ota_start(url);
      } else {
        g_update_available = false;
      }
    } else {
      ESP_LOGE(TAG, "Failed to check version");
    }

    // Wait for interval or signal
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_OTA_CHECK_INTERVAL * 1000));
  }
}

void app_ota_auto_init(void) {
  if (strlen(CONFIG_OTA_SERVER_URL) > 0 &&
      strcmp(CONFIG_OTA_SERVER_URL, "https://example.com") != 0) {
    xTaskCreate(auto_update_task, "auto_ota", 10240, NULL, 5,
                &s_ota_task_handle);
  } else {
    ESP_LOGW(TAG, "Auto OTA not started: URL not configured");
  }
}

const char *app_ota_get_cached_version(void) { return g_remote_version; }

bool app_ota_is_update_available(void) { return g_update_available; }

void app_ota_trigger_check(void) {
  if (s_ota_task_handle) {
    xTaskNotifyGive(s_ota_task_handle);
  }
}

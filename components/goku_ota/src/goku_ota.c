#include "goku_ota.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "goku_data.h"
#include "goku_led.h"
#include "goku_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "goku_ota";
#define OTA_HISTORY_KEY "ota_history"
#define MAX_HISTORY_ENTRIES 5

static char g_remote_version[32] = "Unknown";
static bool g_update_available = false;
static TaskHandle_t s_ota_task_handle = NULL;

/* Struct for OTA Task arguments */
typedef struct {
  char url[256];
  char version[32];
} ota_task_args_t;

/* --- History Helper Functions --- */

static void add_ota_history(const char *version, bool success) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS for history");
    return;
  }

  cJSON *history = NULL;
  size_t required_size = 0;
  err = nvs_get_str(nvs, OTA_HISTORY_KEY, NULL, &required_size);
  if (err == ESP_OK && required_size > 0) {
    char *json_str = malloc(required_size);
    if (json_str) {
      nvs_get_str(nvs, OTA_HISTORY_KEY, json_str, &required_size);
      history = cJSON_Parse(json_str);
      free(json_str);
    }
  }

  if (!history) {
    history = cJSON_CreateArray();
  }

  // Create new entry
  cJSON *entry = cJSON_CreateObject();
  cJSON_AddStringToObject(entry, "ver", version);
  cJSON_AddBoolToObject(entry, "success", success);
  cJSON_AddNumberToObject(entry, "ts", (double)time(NULL));

  // Add to array (at beginning is hard with cJSON, so append and maybe reverse
  // on read, or just append and we read backwards. actually
  // cJSON_AddItemToArray appends. To keep 5 newest, we can append, then remove
  // from beginning if size > 5.
  cJSON_AddItemToArray(history, entry);

  int size = cJSON_GetArraySize(history);
  while (size > MAX_HISTORY_ENTRIES) {
    cJSON_DeleteItemFromArray(history, 0); // Remove oldest
    size = cJSON_GetArraySize(history);
  }

  // Save back
  char *new_json = cJSON_PrintUnformatted(history);
  if (new_json) {
    nvs_set_str(nvs, OTA_HISTORY_KEY, new_json);
    free(new_json);
  }
  cJSON_Delete(history);
  nvs_commit(nvs);
  nvs_close(nvs);
}

char *app_ota_get_history(void) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs);
  if (err != ESP_OK) {
    return NULL;
  }

  size_t required_size = 0;
  err = nvs_get_str(nvs, OTA_HISTORY_KEY, NULL, &required_size);
  if (err != ESP_OK || required_size == 0) {
    nvs_close(nvs);
    return strdup("[]");
  }

  char *json_str = malloc(required_size);
  if (json_str) {
    nvs_get_str(nvs, OTA_HISTORY_KEY, json_str, &required_size);
  }
  nvs_close(nvs);
  return json_str;
}

void app_ota_mark_valid(void) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      ESP_LOGI(TAG, "Marking app as valid, cancelling rollback");
      esp_ota_mark_app_valid_cancel_rollback();
    }
  }
}

/* --- OTA Task --- */

static void ota_task(void *pvParameter) {
  ota_task_args_t *args = (ota_task_args_t *)pvParameter;
  ESP_LOGI(TAG, "Starting OTA task with URL: %s", args->url);

  // Log heap status before OTA
  size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  ESP_LOGI(TAG, "Heap before OTA: Free=%zu, Largest Block=%zu", free_heap,
           largest_block);

  // Lowered threshold: without cert verification, TLS needs ~10-15KB
  if (largest_block < 10000) {
    ESP_LOGE(TAG,
             "Insufficient heap for OTA! Need at least 10KB contiguous block");
    free(args);
    vTaskDelete(NULL);
    return;
  }

  app_led_set_state(APP_LED_OTA);

  // Minimal buffer settings for low-memory OTA
  // Skip certificate verification to reduce TLS heap usage
  // Note: HTTPS still encrypts data, but doesn't verify server identity
  esp_http_client_config_t config = {
      .url = args->url,
      .crt_bundle_attach = NULL, // Skip cert verification (saves ~30KB heap)
      .keep_alive_enable = false,
      .skip_cert_common_name_check = true,
      .buffer_size = 2048,    // Minimal buffer
      .buffer_size_tx = 1024, // Minimal buffer
      .timeout_ms = 60000,
  };

  esp_https_ota_config_t ota_config = {
      .http_config = &config,
  };

  ESP_LOGW(TAG,
           "Starting OTA without certificate verification (low memory mode)");

  // Perform HTTPS OTA Update
  esp_err_t ret = esp_https_ota(&ota_config);

  // Record History
  add_ota_history(args->version[0] ? args->version : "Manual", (ret == ESP_OK));

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
  // If invoked manually via API, we might not have version.
  strncpy(args->version, g_remote_version, sizeof(args->version) - 1);

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
  // Check if time is synced before making HTTPS request
  // TLS certificate validation requires correct system time
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  if (timeinfo.tm_year < (2020 - 1900)) {
    ESP_LOGW(TAG, "Time not synced (Year %d), cannot verify TLS certificate",
             timeinfo.tm_year + 1900);
    return ESP_ERR_INVALID_STATE;
  }

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

  // Mark valid if we are running stable and connected
  // Actually, wait until we are successfully checking versions loop to mark
  // valid? Or handle in event handler.

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

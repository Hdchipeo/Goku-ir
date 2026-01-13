#include "goku_mem.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "goku_mem"

// Thresholds to warn about low memory (in bytes)
#define INTERNAL_HEAP_CRITICAL_THRESHOLD (20 * 1024) // 20KB
#define PSRAM_CRITICAL_THRESHOLD (100 * 1024)        // 100KB

static void app_mem_task(void *pvParameters) {
  while (1) {
    size_t free_internal =
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#ifndef CONFIG_IDF_TARGET_ESP32C3
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#endif

    // Memory stats log suppressed to reduce spam
    // ESP_LOGI(TAG, "Memory Stats - Internal: %u B, PSRAM: %u B",
    //          (unsigned int)free_internal, (unsigned int)free_psram);

    if (free_internal < INTERNAL_HEAP_CRITICAL_THRESHOLD) {
      ESP_LOGW(TAG, "Internal heap is running low!");
    }
#ifndef CONFIG_IDF_TARGET_ESP32C3
    if (free_psram < PSRAM_CRITICAL_THRESHOLD && free_psram > 0) {
      ESP_LOGW(TAG, "PSRAM is running low!");
    }
#endif

    vTaskDelay(pdMS_TO_TICKS(10000)); // Log every 10 seconds
  }
}

esp_err_t app_mem_init(void) {
  ESP_LOGI(TAG, "Initializing Memory Monitoring...");

#ifndef CONFIG_IDF_TARGET_ESP32C3
  // Check if PSRAM is available in heap caps
  size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  if (total_psram > 0) {
    ESP_LOGI(TAG, "PSRAM detected: Total %u bytes, Free %u bytes",
             (unsigned int)total_psram, (unsigned int)free_psram);
  } else {
    ESP_LOGW(TAG, "PSRAM NOT detected in heap capabilities!");
    ESP_LOGW(
        TAG,
        "Please ensure 'CONFIG_SPIRAM=y' is active and hardware supports it.");
  }
#endif

  xTaskCreate(app_mem_task, "app_mem_task", 4096, NULL, 5, NULL);
  return ESP_OK;
}

size_t app_mem_get_free_internal(void) {
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

size_t app_mem_get_free_psram(void) {
#ifndef CONFIG_IDF_TARGET_ESP32C3
  return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#else
  return 0;
#endif
}

bool app_mem_is_safe(size_t size, bool use_psram) {
  size_t free_mem =
      use_psram ? app_mem_get_free_psram() : app_mem_get_free_internal();
  size_t threshold =
      use_psram ? PSRAM_CRITICAL_THRESHOLD : INTERNAL_HEAP_CRITICAL_THRESHOLD;

  if (free_mem < (size + threshold)) {
    ESP_LOGE(
        TAG,
        "Memory check FAILED! Require %u + %u buffer, but only %u available",
        (unsigned int)size, (unsigned int)threshold, (unsigned int)free_mem);
    return false;
  }
  return true;
}

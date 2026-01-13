#include "goku_mdns.h"
#include <esp_log.h>
#include <mdns.h>
#include <string.h>

static const char *TAG = "goku_mdns";

esp_err_t app_mdns_init(void) {
  char *hostname = "gokuac";
  char *desc = "Goku IR Device";

  // Initialize mDNS
  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    if (err == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "mDNS already initialized, updating configuration");
    } else {
      ESP_LOGE(TAG, "MDNS Init failed: %d", err);
      return err;
    }
  }

  // Set hostname (Enforce 'gokuac')
  if ((err = mdns_hostname_set(hostname)) != ESP_OK) {
    ESP_LOGE(TAG, "mDNS set hostname failed: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "mDNS hostname set to: [%s]", hostname);

  // Set default instance
  if ((err = mdns_instance_name_set(desc)) != ESP_OK) {
    ESP_LOGE(TAG, "mDNS set instance failed: %s", esp_err_to_name(err));
    return err;
  }

  // Structure with TXT records
  mdns_txt_item_t serviceTxtData[2] = {{"board", "esp32s3"}, {"path", "/"}};

  // Remove existing service if causing conflict (Refresh)
  mdns_service_remove("_http", "_tcp");

  // Initialize service
  // Port 8080 for HTTP (Web Config)
  ESP_ERROR_CHECK(
      mdns_service_add("Goku-Web", "_http", "_tcp", 8080, serviceTxtData, 2));

  return ESP_OK;
}

#include "app_mdns.h"
#include <esp_log.h>
#include <mdns.h>
#include <string.h>

static const char *TAG = "app_mdns";

esp_err_t app_mdns_init(void) {
  char *hostname = "gokuir";
  char *desc = "Goku IR Device";

  // Initialize mDNS
  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "MDNS Init failed: %d", err);
    return err;
  }

  // Set hostname
  mdns_hostname_set(hostname);
  ESP_LOGI(TAG, "mDNS hostname set to: [%s]", hostname);

  // Set default instance
  mdns_instance_name_set(desc);

  // Structure with TXT records
  mdns_txt_item_t serviceTxtData[2] = {{"board", "esp32s3"}, {"path", "/"}};

  // Initialize service
  // Port 80 for HTTP
  ESP_ERROR_CHECK(
      mdns_service_add("Goku-Web", "_http", "_tcp", 80, serviceTxtData, 2));

  return ESP_OK;
}

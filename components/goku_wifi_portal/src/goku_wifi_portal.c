#include "goku_wifi_portal.h"
#include "sdkconfig.h"

#if CONFIG_GOKU_PLATFORM_IOS && CONFIG_IOS_WIFI_PROV_AP_ENABLE

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <cJSON.h>
#include <string.h>

static const char *TAG = "wifi_portal";

#define NVS_NAMESPACE "wifi_config"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASSWORD "password"
#define NVS_KEY_PROVISIONED "provisioned"

static httpd_handle_t server = NULL;
static bool provisioned_flag = false;
static char saved_ssid[33] = {0};
static char saved_password[65] = {0};

// Embedded HTML (from www/index.html) - Note: You need to make sure CMake
// embeds this!
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

/* ==================== NVS Credential Management ==================== */

bool wifi_credentials_exist(void) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    return false;
  }

  uint8_t provisioned = 0;
  err = nvs_get_u8(handle, NVS_KEY_PROVISIONED, &provisioned);
  nvs_close(handle);

  return (err == ESP_OK && provisioned == 1);
}

esp_err_t wifi_credentials_load(char *ssid, char *password) {
  if (!ssid || !password) {
    return ESP_ERR_INVALID_ARG;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    return err;
  }

  size_t ssid_len = 33;
  size_t pass_len = 65;

  err = nvs_get_str(handle, NVS_KEY_SSID, ssid, &ssid_len);
  if (err == ESP_OK) {
    err = nvs_get_str(handle, NVS_KEY_PASSWORD, password, &pass_len);
  }

  nvs_close(handle);

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Loaded credentials for SSID: %s", ssid);
  } else {
    ESP_LOGE(TAG, "Failed to load credentials: %s", esp_err_to_name(err));
  }

  return err;
}

esp_err_t wifi_credentials_save(const char *ssid, const char *password) {
  if (!ssid) {
    return ESP_ERR_INVALID_ARG;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    return err;
  }

  err = nvs_set_str(handle, NVS_KEY_SSID, ssid);
  if (err == ESP_OK) {
    err = nvs_set_str(handle, NVS_KEY_PASSWORD, password ? password : "");
  }
  if (err == ESP_OK) {
    err = nvs_set_u8(handle, NVS_KEY_PROVISIONED, 1);
  }
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }

  nvs_close(handle);

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Saved credentials for SSID: %s", ssid);
  } else {
    ESP_LOGE(TAG, "Failed to save credentials: %s", esp_err_to_name(err));
  }

  return err;
}

esp_err_t wifi_credentials_clear(void) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  nvs_erase_key(handle, NVS_KEY_SSID);
  nvs_erase_key(handle, NVS_KEY_PASSWORD);
  nvs_erase_key(handle, NVS_KEY_PROVISIONED);
  err = nvs_commit(handle);
  nvs_close(handle);

  ESP_LOGI(TAG, "Cleared WiFi credentials");
  return err;
}

/* ==================== HTTP Handlers ==================== */

static esp_err_t index_handler(httpd_req_t *req) {
  // Strategy: Serve 200 OK for EVERYTHING.
  // No Redirects. This satisfies "Connect" checks immediately.
  char host_buf[64];
  const char *host_str = NULL;
  if (httpd_req_get_hdr_value_str(req, "Host", host_buf, sizeof(host_buf)) ==
      ESP_OK) {
    host_str = host_buf;
  }
  ESP_LOGI(TAG, "Serve Portal: %s (Host: %s)", req->uri,
           host_str ? host_str : "N/A");

  const size_t html_len = index_html_end - index_html_start;
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Connection", "close");
  httpd_resp_send(req, (const char *)index_html_start, html_len);
  return ESP_OK;
}

static esp_err_t scan_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "WiFi scan requested");

  wifi_scan_config_t scan_config = {0};
  esp_err_t err = esp_wifi_scan_start(&scan_config, true);
  if (err != ESP_OK) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  uint16_t ap_count = 0;
  esp_wifi_scan_get_ap_num(&ap_count);

  if (ap_count == 0) {
    httpd_resp_sendstr(req, "[]");
    return ESP_OK;
  }

  wifi_ap_record_t *ap_records = malloc(ap_count * sizeof(wifi_ap_record_t));
  if (!ap_records) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  esp_wifi_scan_get_ap_records(&ap_count, ap_records);

  cJSON *json_array = cJSON_CreateArray();
  for (int i = 0; i < ap_count; i++) {
    cJSON *ap = cJSON_CreateObject();
    cJSON_AddStringToObject(ap, "ssid", (char *)ap_records[i].ssid);
    cJSON_AddNumberToObject(ap, "rssi", ap_records[i].rssi);
    cJSON_AddItemToArray(json_array, ap);
  }

  char *json_str = cJSON_PrintUnformatted(json_array);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, json_str);

  free(json_str);
  cJSON_Delete(json_array);
  free(ap_records);

  return ESP_OK;
}

static esp_err_t connect_handler(httpd_req_t *req) {
  char content[200];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  cJSON *json = cJSON_Parse(content);
  if (!json) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    return ESP_FAIL;
  }

  cJSON *ssid_json = cJSON_GetObjectItem(json, "ssid");
  cJSON *pass_json = cJSON_GetObjectItem(json, "password");

  if (!ssid_json || !cJSON_IsString(ssid_json)) {
    cJSON_Delete(json);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
    return ESP_FAIL;
  }

  const char *ssid = cJSON_GetStringValue(ssid_json);
  const char *password = pass_json ? cJSON_GetStringValue(pass_json) : "";

  strncpy(saved_ssid, ssid, sizeof(saved_ssid) - 1);
  strncpy(saved_password, password, sizeof(saved_password) - 1);

  esp_err_t err = wifi_credentials_save(ssid, password);
  cJSON_Delete(json);

  if (err == ESP_OK) {
    provisioned_flag = true;
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    // Schedule reboot
    ESP_LOGI(TAG, "Credentials saved. Rebooting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
  } else {
    httpd_resp_send_500(req);
  }

  return ESP_OK;
}

/* ==================== DNS Server (Captive Portal) ==================== */

typedef struct __attribute__((packed)) {
  uint16_t id;
  uint16_t flags;
  uint16_t qd_count;
  uint16_t an_count;
  uint16_t ns_count;
  uint16_t ar_count;
} dns_header_t;

static int dns_sock = -1;
static TaskHandle_t dns_task_handle = NULL;
extern esp_netif_t *esp_netif_ap; // Reference to AP netif if needed

static void dns_server_task(void *pvParameters) {
  char rx_buffer[512];
  char tx_buffer[512];
  struct sockaddr_in source_addr;
  socklen_t socklen;

  ESP_LOGI(TAG, "DNS Server listening on port 53");

  while (1) {
    socklen = sizeof(source_addr);
    memset(rx_buffer, 0, sizeof(rx_buffer));

    int len = recvfrom(dns_sock, rx_buffer, sizeof(rx_buffer), 0,
                       (struct sockaddr *)&source_addr, &socklen);

    if (len < 0) {
      // EAGAIN or timeout is normal, other errors log
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        ESP_LOGE(TAG, "DNS recvfrom failed: errno %d", errno);
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    if (len > sizeof(dns_header_t)) {
      // Copy to TX buffer
      memcpy(tx_buffer, rx_buffer, len);

      dns_header_t *header = (dns_header_t *)tx_buffer;
      uint16_t qd_count = ntohs(header->qd_count);

      // Only handle Standard Query (Opcode 0)
      if ((ntohs(header->flags) & 0x7800) == 0 && qd_count > 0) {

        // Extract QNAME string for logging
        char qname[128] = {0};
        int ptr = sizeof(dns_header_t);
        while (ptr < len) {
          uint8_t label_len = (uint8_t)rx_buffer[ptr];
          if (label_len == 0)
            break; // End of QNAME
          ptr += (label_len + 1);
        }
        ptr++; // Skip the null terminator (0x00)

        // Safety check boundaries (QTYPE=2 + QCLASS=2 = 4 bytes)
        if (ptr + 4 > len) {
          ESP_LOGW(TAG, "DNS Packet Truncated/Malformed");
          continue;
        }

        uint16_t qtype = (rx_buffer[ptr] << 8) | (uint8_t)rx_buffer[ptr + 1];
        uint16_t qclass =
            (rx_buffer[ptr + 2] << 8) | (uint8_t)rx_buffer[ptr + 3];

        ptr += 4; // Advance to end of Query Section

        // Modify Header Flags: Response(1) + RecursionAvailable(1)
        // Preserve client's RD bit and other flags
        tx_buffer[2] |= 0x80;
        tx_buffer[3] |= 0x80;

        // Default counts: 0
        tx_buffer[6] = 0x00;
        tx_buffer[7] = 0x00; // ANCOUNT
        tx_buffer[8] = 0x00;
        tx_buffer[9] = 0x00; // NSCOUNT
        tx_buffer[10] = 0x00;
        tx_buffer[11] = 0x00; // ARCOUNT

        if (qtype == 0x0001 && qclass == 0x0001) { // A Record (IPv4)
          // ANCOUNT = 1
          tx_buffer[7] = 0x01;

          // Append Answer: Name Pointer (0xC00C)
          tx_buffer[ptr++] = 0xC0;
          tx_buffer[ptr++] = 0x0C;
          // Type A
          tx_buffer[ptr++] = 0x00;
          tx_buffer[ptr++] = 0x01;
          // Class IN
          tx_buffer[ptr++] = 0x00;
          tx_buffer[ptr++] = 0x01;
          // TTL (60s)
          tx_buffer[ptr++] = 0x00;
          tx_buffer[ptr++] = 0x00;
          tx_buffer[ptr++] = 0x00;
          tx_buffer[ptr++] = 0x3C;
          // Data Len (4)
          tx_buffer[ptr++] = 0x00;
          tx_buffer[ptr++] = 0x04;
          // IP: 192.168.4.1
          tx_buffer[ptr++] = 192;
          tx_buffer[ptr++] = 168;
          tx_buffer[ptr++] = 4;
          tx_buffer[ptr++] = 1;

          ESP_LOGI(TAG, "DNS A Query -> Respond 192.168.4.1");

        } else if (qtype == 0x001C && qclass == 0x0001) { // AAAA Record (IPv6)
          // ANCOUNT = 0 (Already set)
          // Just return the header + original query
          ESP_LOGI(TAG, "DNS AAAA Query -> No Data (IPv6 Unsupported)");

        } else {
          ESP_LOGI(TAG, "DNS QTYPE %d -> Ignored (No Data)", qtype);
        }

        int sent = sendto(dns_sock, tx_buffer, ptr, 0,
                          (struct sockaddr *)&source_addr, sizeof(source_addr));
        if (sent < 0) {
          ESP_LOGE(TAG, "DNS Send Failed: errno %d", errno);
        }
      }
    }
  }
  vTaskDelete(NULL);
}

static void start_dns_server(void) {
  struct sockaddr_in dest_addr;
  dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(53);

  dns_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (dns_sock < 0) {
    ESP_LOGE(TAG, "Unable to create DNS socket: errno %d", errno);
    return;
  }

  int err = bind(dns_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err < 0) {
    ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    close(dns_sock);
    return;
  }

  xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
  ESP_LOGI(TAG, "DNS Server started (Captive Portal DNS Hijack)");
}

static void stop_dns_server(void) {
  if (dns_sock != -1) {
    shutdown(dns_sock, 0);
    close(dns_sock);
    dns_sock = -1;
  }
  if (dns_task_handle) {
    vTaskDelete(dns_task_handle);
    dns_task_handle = NULL;
  }
}

/* ==================== HTTP Server ==================== */

static esp_err_t redirect_handler(httpd_req_t *req) {
  // Return 302 Found -> Redirect to our Portal
  // This is often a stronger signal for "Captive Portal" than just serving
  // content
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
  httpd_resp_set_hdr(req, "Cache-Control",
                     "no-cache, no-store, must-revalidate");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers =
      24; // Limit is 8 by default, we need more for probes
  config.server_port = 80;
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.max_open_sockets = 13; // Increase potential connections
  config.lru_purge_enable = true;

  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return NULL;
  }

  httpd_uri_t index_uri = {
      .uri = "/", .method = HTTP_GET, .handler = index_handler};
  httpd_register_uri_handler(server, &index_uri);
  httpd_uri_t scan_uri = {
      .uri = "/api/scan", .method = HTTP_GET, .handler = scan_handler};
  httpd_register_uri_handler(server, &scan_uri);

  // Specific Captive Portal Probes -> 302 Redirect to Portal
  const char *probes[] = {
      "/hotspot-detect.html",      "/generate_204", "/mobile/status.php",
      "/check_network_status.txt", "/ncsi.txt",     "/fwlink/*",
      "/connectivity-check.html",  "/success.txt",  "/portal.html",
      "/library/test/success.html"};

  for (int i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
    httpd_uri_t probe_uri = {.uri = probes[i],
                             .method = HTTP_GET,
                             .handler = redirect_handler}; // 302 Redirect
    httpd_register_uri_handler(server, &probe_uri);
  }

  httpd_uri_t connect_uri = {
      .uri = "/api/connect", .method = HTTP_POST, .handler = connect_handler};
  httpd_register_uri_handler(server, &connect_uri);

  // Captive portal catch-all (Must be registered LAST) -> 302 Redirect
  httpd_uri_t catchall_uri = {
      .uri = "/*", .method = HTTP_GET, .handler = redirect_handler};
  httpd_register_uri_handler(server, &catchall_uri);

  ESP_LOGI(TAG, "HTTP server started");
  return server;
}

/* ==================== Public API ==================== */

esp_err_t wifi_portal_init(void) {
  ESP_LOGI(TAG, "Initializing WiFi portal");
  return ESP_OK;
}

esp_err_t wifi_portal_start(void) {
  ESP_LOGI(TAG, "Starting WiFi portal (SoftAP mode)");

  // Create default AP netif (this starts DHCP server automatically)
  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
  if (!ap_netif) {
    ESP_LOGE(TAG, "Failed to create AP netif");
    return ESP_FAIL;
  }

  // Configure DHCP Option 114 (Captive Portal URI) - RFC 8910
  // This triggers auto-popup on iOS 14+, Android 11+, Windows 11+
  static const char *captive_portal_uri = "http://192.168.4.1";
  esp_err_t err = esp_netif_dhcps_option(
      ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
      captive_portal_uri, strlen(captive_portal_uri));
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to set DHCP Option 114: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "DHCP Option 114 enabled: %s", captive_portal_uri);
  }

  // Get MAC address for unique SSID
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_AP, mac);
  char ap_ssid[32];
  snprintf(ap_ssid, sizeof(ap_ssid), "%s-%02X%02X",
           CONFIG_IOS_WIFI_AP_SSID_PREFIX, mac[4], mac[5]);

  // Configure AP
  wifi_config_t wifi_config = {
      .ap =
          {
              .ssid_len = strlen(ap_ssid),
              .channel = CONFIG_IOS_WIFI_AP_CHANNEL,
              .max_connection = CONFIG_IOS_WIFI_AP_MAX_CONNECTIONS,
              .authmode = WIFI_AUTH_OPEN,
          },
  };
  strcpy((char *)wifi_config.ap.ssid, ap_ssid);

  if (strlen(CONFIG_IOS_WIFI_AP_PASSWORD) > 0) {
    strcpy((char *)wifi_config.ap.password, CONFIG_IOS_WIFI_AP_PASSWORD);
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  }

  // Use APSTA mode to allow WiFi scanning while AP is active
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "SoftAP started: %s", ap_ssid);
  ESP_LOGI(TAG, "AP IP: 192.168.4.1");

  // Start HTTP server
  start_webserver();

  // Start DNS Server (Captive Portal)
  start_dns_server();

  // Note: iOS detects captive portals via HTTP requests
  // The catchall handler will redirect all requests to our portal

  return ESP_OK;
}

void wifi_portal_stop(void) {
  // Stop DNS first
  stop_dns_server();

  if (server) {
    httpd_stop(server);
    server = NULL;
    ESP_LOGI(TAG, "HTTP server stopped");
  }

  esp_wifi_stop();
  ESP_LOGI(TAG, "WiFi portal stopped");
}

bool wifi_portal_is_provisioned(void) { return provisioned_flag; }

#else

// Stub implementations when iOS provisioning is disabled
esp_err_t wifi_portal_init(void) { return ESP_OK; }
esp_err_t wifi_portal_start(void) { return ESP_OK; }
void wifi_portal_stop(void) {}
bool wifi_portal_is_provisioned(void) { return false; }
bool wifi_credentials_exist(void) { return false; }
esp_err_t wifi_credentials_load(char *ssid, char *password) {
  return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t wifi_credentials_save(const char *ssid, const char *password) {
  return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t wifi_credentials_clear(void) { return ESP_ERR_NOT_SUPPORTED; }

#endif // CONFIG_GOKU_PLATFORM_IOS && CONFIG_IOS_WIFI_PROV_AP_ENABLE

#include "app_web.h"
#include "app_data.h"
#include "app_ir.h"
#include "app_led.h"
#include "app_ota.h"
#include "app_wifi.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <malloc.h>
#include <string.h>

#define TAG "app_web"

static httpd_handle_t server = NULL;

/* HTML Content */
static const char *index_html =
    "<!DOCTYPE html>"
    "<html lang='en'>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0, "
    "maximum-scale=1.0, user-scalable=no'>"
    "<title>Goku IR Control</title>"
    "<style>"
    ":root { --primary: #3b82f6; --bg: #0f172a; --card: #1e293b; --text: "
    "#e2e8f0; --success: #22c55e; --danger: #ef4444; }"
    "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', "
    "Roboto, Helvetica, Arial, sans-serif; "
    "background: var(--bg); color: var(--text); margin: 0; padding: 15px; "
    "display: flex; "
    "justify-content: center; }"
    ".container { width: 100%; max-width: 480px; }"
    "h1 { text-align: center; margin-bottom: 25px; font-weight: 800; "
    "background: "
    "linear-gradient(to right, #60a5fa, #a78bfa); -webkit-background-clip: "
    "text; -webkit-text-fill-color: transparent; font-size: 1.8rem; }"
    ".card { background: var(--card); border-radius: 16px; padding: 20px; "
    "margin-bottom: 15px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.2); }"
    "h3 { margin-top: 0; margin-bottom: 15px; font-weight: 600; color: "
    "#94a3b8; font-size: 0.9rem; text-transform: uppercase; letter-spacing: "
    "0.05em; }"
    ".btn { background: var(--primary); border: none; padding: 12px 15px; "
    "border-radius: 10px; color: white; cursor: pointer; font-weight: 600; "
    "width: 100%; transition: all 0.2s; font-size: 1rem; touch-action: "
    "manipulation; }"
    ".btn:active { transform: scale(0.98); opacity: 0.9; }"
    ".btn-secondary { background: #334155; }"
    ".btn-danger { background: var(--danger); }"
    ".btn-success { background: var(--success); }"
    ".row { display: flex; gap: 10px; margin-top: 10px; align-items: center; }"
    "input, select { width: 100%; padding: 12px; border-radius: 10px; border: "
    "1px solid #334155; background: #0f172a; color: white; box-sizing: "
    "border-box; font-size: 1rem; appearance: none; }"
    ".key-item { display: flex; align-items: center; justify-content: "
    "space-between; padding: "
    "12px 0; border-bottom: 1px solid #334155; }"
    ".key-item:last-child { border-bottom: none; }"
    ".key-actions { display: flex; gap: 8px; }"
    ".key-actions .btn { padding: 8px 12px; font-size: 0.85rem; width: auto; }"
    ".status { text-align: center; color: #94a3b8; font-size: 0.9em; "
    "margin-top: 10px; }"
    ".hidden { display: none; }"
    "</style>"
    "</head>"
    "<body>"
    "<div class='container'>"
    "<h1>Goku IR</h1>"

    "<div class='card'>"
    "<h3>Learning</h3>"
    "<div class='row'>"
    "<button class='btn' onclick='startLearn()'>Start Learning</button>"
    "<button class='btn btn-secondary' onclick='stopLearn()'>Stop</button>"
    "</div>"
    "<div id='learnStatus' class='status'>Ready</div>"
    "</div>"

    "<div class='card'>"
    "<h3>Controls</h3>"

    "<div class='row'>"
    "<button class='btn btn-secondary' onclick=\"saveAC('ac_on')\">ON</button>"
    "<button class='btn btn-secondary' "
    "onclick=\"saveAC('ac_off')\">OFF</button>"
    "</div>"

    "<div class='row'>"
    "<select id='modeSelect'>"
    "<option value='auto'>Mode: Auto</option><option value='cool'>Mode: "
    "Cool</option>"
    "<option value='heat'>Mode: Heat</option><option value='fan'>Mode: "
    "Fan</option>"
    "</select>"
    "<button class='btn' style='width: 80px' "
    "onclick=\"saveMode()\">Save</button>"
    "</div>"

    "<div class='row'>"
    "<select id='fanSelect'>"
    "<option value='auto'>Fan: Auto</option><option value='low'>Fan: "
    "Low</option>"
    "<option value='medium'>Fan: Medium</option><option value='high'>Fan: "
    "High</option>"
    "</select>"
    "<button class='btn' style='width: 80px' "
    "onclick=\"saveFan()\">Save</button>"
    "</div>"

    "<div class='row'>"
    "<input type='number' id='tempInput' value='25' min='16' max='30'>"
    "<button class='btn' style='width: 80px' "
    "onclick=\"saveTemp()\">Save</button>"
    "</div>"
    "</div>"

    "<div class='card'>"
    "<h3>Saved Keys</h3>"
    "<div id='keyList'>Loading...</div>"
    "</div>"

    "<div class='card'>"
    "<h3>Wi-Fi Settings</h3>"
    "<div class='row'>"
    "<input type='text' id='wifiSSID' placeholder='SSID'>"
    "<button id='scanBtn' class='btn' style='width: 80px' "
    "onclick='scanWifi()'>Scan</button>"
    "</div>"
    "<div id='scanList' class='hidden' style='margin-bottom: 10px; max-height: "
    "150px; overflow-y: auto; border: 1px solid #334155; border-radius: "
    "8px;'></div>"
    "<div class='row'>"
    "<input type='password' id='wifiPass' placeholder='Password'>"
    "</div>"
    "<div class='row'>"
    "<button class='btn' onclick='saveWifi()'>Save & Connect</button>"
    "</div>"
    "</div>"

    "<div class='card'>"
    "<h3>Firmware Update</h3>"
    "<div id='otaSection'>"
    "<p class='status' style='margin-bottom: 10px'>System Version: <span "
    "id='currentVer'>Loading...</span></p>"
    "<button class='btn' onclick='checkUpdate()'>Check for Updates</button>"
    "</div>"
    "<div id='updateAvailable' class='hidden'>"
    "<p class='status' style='color: var(--success); margin-bottom: 10px'>New "
    "Version Available: <b id='newVer'></b></p>"
    "<button class='btn btn-success' onclick='startUpdate()'>Update "
    "Now</button>"
    "</div>"
    "<div id='otaStatus' class='status'></div>"
    "</div>"

    "</div>"

    "<script>"
    "const statusEl = document.getElementById('learnStatus');"
    "const keyListEl = document.getElementById('keyList');"

    "async function fetchList() {"
    "  try { const res = await fetch('/api/ir/list');"
    "  const keys = await res.json(); renderList(keys); } catch(e){}"
    "}"

    "function renderList(keys) {"
    "  keyListEl.innerHTML = '';"
    "  if(keys.length === 0) { keyListEl.innerHTML = '<div class=\"status\">No "
    "keys saved</div>'; return; }"
    "  keys.forEach(key => {"
    "    const div = document.createElement('div');"
    "    div.className = 'key-item';"
    "    div.innerHTML = `<div><strong>${key}</strong></div><div "
    "class='key-actions'>"
    "      <button class='btn btn-secondary' "
    "onclick=\"sendKey('${key}')\">Test</button>"
    "      <button class='btn btn-danger' "
    "onclick=\"deleteKey('${key}')\">Del</button>"
    "    </div>`;"
    "    keyListEl.appendChild(div);"
    "  });"
    "}"

    "async function startLearn() {"
    "  await fetch('/api/learn/start', {method:'POST'});"
    "  statusEl.innerText = 'Listening... Press remote button';"
    "  statusEl.style.color = '#fbbf24';"
    "}"

    "async function stopLearn() {"
    "  await fetch('/api/learn/stop', {method:'POST'});"
    "  statusEl.innerText = 'Stopped';"
    "  statusEl.style.color = '#94a3b8';"
    "}"

    "async function saveAC(keyName) {"
    "  const res = await fetch('/api/save?key=' + encodeURIComponent(keyName), "
    "{method:'POST'});"
    "  if(res.ok) { statusEl.innerText = 'Saved: '+keyName; "
    "statusEl.style.color = '#22c55e'; fetchList(); }"
    "  else alert('Failed too save. Ensure Learning mode is Active.');"
    "}"

    "function saveMode() { saveAC('mode_' + "
    "document.getElementById('modeSelect').value); }"
    "function saveFan() { saveAC('fan_' + "
    "document.getElementById('fanSelect').value); }"
    "function saveTemp() { saveAC('temp_' + "
    "document.getElementById('tempInput').value); }"

    "async function sendKey(key) {"
    "  await fetch('/api/send?key='+encodeURIComponent(key), {method:'POST'});"
    "}"

    "async function deleteKey(key) {"
    "  if(!confirm('Delete ' + key + '?')) return;"
    "  await fetch('/api/ir/delete?key='+encodeURIComponent(key), "
    "{method:'POST'});"
    "  fetchList();"
    "}"

    "async function checkUpdate() {"
    "  const status = document.getElementById('otaStatus');"
    "  status.innerText = 'Checking...';"
    "  try {"
    "    const res = await fetch('/api/ota/check');"
    "    const data = await res.json();"
    "    document.getElementById('currentVer').innerText = data.current;"
    "    if(data.available) {"
    "       document.getElementById('otaSection').classList.add('hidden');"
    "       "
    "document.getElementById('updateAvailable').classList.remove('hidden');"
    "       document.getElementById('newVer').innerText = data.latest;"
    "       status.innerText = '';"
    "    } else {"
    "       status.innerText = 'System is up to date.';"
    "    }"
    "  } catch (e) { status.innerText = 'Error checking update'; }"
    "}"

    "async function startUpdate() {"
    "  if(!confirm('Start Firmware Update? Device will reboot.')) return;"
    "  const status = document.getElementById('otaStatus');"
    "  status.innerText = 'Starting update...';"
    "  try {"
    "    const res = await fetch('/api/ota/start', {method:'POST'});"
    "    if(res.ok) status.innerText = 'Update started! Wait for reboot...';"
    "    else status.innerText = 'Failed to start update';"
    "  } catch(e) { status.innerText = 'Error starting update'; }"
    "}"

    "async function saveWifi() {"
    "  const ssid = document.getElementById('wifiSSID').value;"
    "  const pass = document.getElementById('wifiPass').value;"
    "  if(!ssid) { alert('SSID is required'); return; }"
    "  if(!confirm('Save credentials and restart device?')) return;"
    "  "
    "  try {"
    "    const res = await fetch('/api/wifi/config?ssid=' + "
    "encodeURIComponent(ssid) + '&password=' + encodeURIComponent(pass), "
    "{method:'POST'});"
    "    if(res.ok) alert('Settings saved. Device rebooting...');"
    "    else alert('Failed to save settings');"
    "  } catch(e) { alert('Error: ' + e); }"
    "}"

    "async function scanWifi() {"
    "  const btn = document.getElementById('scanBtn');"
    "  const list = document.getElementById('scanList');"
    "  btn.disabled = true; btn.innerText = 'Scanning...';"
    "  list.innerHTML = ''; list.classList.remove('hidden');"
    "  try {"
    "    const res = await fetch('/api/wifi/scan');"
    "    const data = await res.json();"
    "    if(data.length === 0) list.innerHTML = '<div class=\"status\">No "
    "networks found</div>';"
    "    data.forEach(net => {"
    "       const div = document.createElement('div');"
    "       div.className = 'key-item';"
    "       div.style.cursor = 'pointer';"
    "       div.innerHTML = `<div><strong>${net.ssid}</strong> "
    "<small>(${net.rssi}dBm)</small></div>`;"
    "       div.onclick = () => { document.getElementById('wifiSSID').value = "
    "net.ssid; list.classList.add('hidden'); };"
    "       list.appendChild(div);"
    "    });"
    "  } catch(e) { list.innerHTML = '<div class=\"status\">Scan "
    "failed</div>'; }"
    "  btn.disabled = false; btn.innerText = 'Scan';"
    "}"

    "fetchList();"
    "fetch('/api/ota/"
    "check').then(r=>r.json()).then(d=>document.getElementById('currentVer')."
    "innerText=d.current).catch(e=>{});"
    "</script>"
    "</body></html>";

static esp_err_t root_get_handler(httpd_req_t *req) {
  httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t api_ir_list_handler(httpd_req_t *req) {
  cJSON *list = app_data_get_ir_keys();
  char *json_str = cJSON_PrintUnformatted(list);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(list);
  free(json_str);
  return ESP_OK;
}

static esp_err_t api_learn_start_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API: Start Learn");
  esp_err_t err = app_ir_start_learn();
  if (err == ESP_OK) {
    httpd_resp_send(req, "Started", HTTPD_RESP_USE_STRLEN);
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

static esp_err_t api_learn_stop_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API: Stop Learn");
  app_ir_stop_learn();
  httpd_resp_send(req, "Stopped", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t api_save_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "key", key, sizeof(key)) == ESP_OK) {
        ESP_LOGI(TAG, "API: Save Key %s", key);
        app_ir_save_learned_result(key);
        httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
      }
    }
    free(buf);
  }

  if (key[0] == 0) {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_send_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "key", key, sizeof(key)) == ESP_OK) {
        ESP_LOGI(TAG, "API: Send Key %s", key);
        app_ir_send_key(key);
        httpd_resp_send(req, "Sent", HTTPD_RESP_USE_STRLEN);
      }
    }
    free(buf);
  }

  if (key[0] == 0) {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_delete_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "key", key, sizeof(key)) == ESP_OK) {
        ESP_LOGI(TAG, "API: Delete Key %s", key);
        app_data_delete_ir(key);
        httpd_resp_send(req, "Deleted", HTTPD_RESP_USE_STRLEN);
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_rename_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char old_key[32] = {0};
  char new_key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      httpd_query_key_value(buf, "old", old_key, sizeof(old_key));
      httpd_query_key_value(buf, "new", new_key, sizeof(new_key));

      if (old_key[0] && new_key[0]) {
        ESP_LOGI(TAG, "API: Rename %s -> %s", old_key, new_key);
        if (app_data_rename_ir(old_key, new_key) == ESP_OK) {
          httpd_resp_send(req, "Renamed", HTTPD_RESP_USE_STRLEN);
        } else {
          httpd_resp_send_500(req);
        }
      } else {
        httpd_resp_send_404(req);
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static int parse_version(const char *version_str) {
  int major = 0, minor = 0, patch = 0;
  sscanf(version_str, "%d.%d.%d", &major, &minor, &patch);
  return (major * 10000) + (minor * 100) + patch;
}

static esp_err_t api_ota_check_handler(httpd_req_t *req) {
  char remote_ver_str[32] = {0};
  char response[128];
  bool available = false;

  // Optimistic check. If logic fails or server is down, we handle it.
  if (app_ota_check_version(remote_ver_str, sizeof(remote_ver_str)) == ESP_OK) {
    int local_ver = parse_version(PROJECT_VERSION);
    int remote_ver = parse_version(remote_ver_str);
    if (remote_ver > local_ver) {
      available = true;
    }
  } else {
    strcpy(remote_ver_str, "Unknown");
  }

  snprintf(response, sizeof(response),
           "{\"current\":\"%s\", \"latest\":\"%s\", \"available\":%s}",
           PROJECT_VERSION, remote_ver_str, available ? "true" : "false");

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t api_ota_start_handler(httpd_req_t *req) {
  char url[256];
  snprintf(url, sizeof(url), "%s/goku-ir-device.bin", CONFIG_OTA_SERVER_URL);

  ESP_LOGI(TAG, "Starting manual update from UI: %s", url);

  if (app_ota_start(url) == ESP_OK) {
    httpd_resp_send(req, "Started", HTTPD_RESP_USE_STRLEN);
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

static esp_err_t api_wifi_config_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char ssid[33] = {0};
  char password[65] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) == ESP_OK) {
        httpd_query_key_value(buf, "password", password, sizeof(password));

        ESP_LOGI(TAG, "API: Update WiFi Config. SSID: %s", ssid);
        app_wifi_update_credentials(ssid, password);
        httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
      } else {
        httpd_resp_send_500(req);
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_wifi_scan_handler(httpd_req_t *req) {
  uint16_t ap_count = 0;
  wifi_ap_record_t *ap_list = NULL;

  if (app_wifi_get_scan_results(&ap_list, &ap_count) != ESP_OK) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  cJSON *root = cJSON_CreateArray();
  for (int i = 0; i < ap_count; i++) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "ssid", (char *)ap_list[i].ssid);
    cJSON_AddNumberToObject(item, "rssi", ap_list[i].rssi);
    cJSON_AddItemToArray(root, item);
  }
  free(ap_list);

  char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(root);
  free(json_str);
  return ESP_OK;
}

static const httpd_uri_t root = {
    .uri = "/", .method = HTTP_GET, .handler = root_get_handler};
static const httpd_uri_t ir_list = {
    .uri = "/api/ir/list", .method = HTTP_GET, .handler = api_ir_list_handler};
static const httpd_uri_t learn_start = {.uri = "/api/learn/start",
                                        .method = HTTP_POST,
                                        .handler = api_learn_start_handler};
static const httpd_uri_t learn_stop = {.uri = "/api/learn/stop",
                                       .method = HTTP_POST,
                                       .handler = api_learn_stop_handler};
static const httpd_uri_t save_key = {
    .uri = "/api/save", .method = HTTP_POST, .handler = api_save_handler};
static const httpd_uri_t send_key = {
    .uri = "/api/send", .method = HTTP_POST, .handler = api_send_handler};
static const httpd_uri_t delete_key = {.uri = "/api/ir/delete",
                                       .method = HTTP_POST,
                                       .handler = api_delete_handler};
static const httpd_uri_t rename_key = {.uri = "/api/ir/rename",
                                       .method = HTTP_POST,
                                       .handler = api_rename_handler};
static const httpd_uri_t ota_check = {.uri = "/api/ota/check",
                                      .method = HTTP_GET,
                                      .handler = api_ota_check_handler};
static const httpd_uri_t ota_start = {.uri = "/api/ota/start",
                                      .method = HTTP_POST,
                                      .handler = api_ota_start_handler};
static const httpd_uri_t wifi_config = {.uri = "/api/wifi/config",
                                        .method = HTTP_POST,
                                        .handler = api_wifi_config_handler};
static const httpd_uri_t wifi_scan = {.uri = "/api/wifi/scan",
                                      .method = HTTP_GET,
                                      .handler = api_wifi_scan_handler};

esp_err_t app_web_init(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 16;
  config.stack_size = 8192; // Increase stack for deeper call stacks if needed

  ESP_LOGI(TAG, "Starting HTTP Server...");
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &ir_list);
    httpd_register_uri_handler(server, &learn_start);
    httpd_register_uri_handler(server, &learn_stop);
    httpd_register_uri_handler(server, &save_key);
    httpd_register_uri_handler(server, &send_key);
    httpd_register_uri_handler(server, &delete_key);
    httpd_register_uri_handler(server, &rename_key);
    httpd_register_uri_handler(server, &ota_check);
    httpd_register_uri_handler(server, &ota_start);
    httpd_register_uri_handler(server, &wifi_config);
    httpd_register_uri_handler(server, &wifi_scan);
    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "Error starting server!");
    return ESP_FAIL;
  }
}

void app_web_toggle_mode(void) {
  ESP_LOGI(TAG, "Web mode toggle requested (No-op in station mode)");
}

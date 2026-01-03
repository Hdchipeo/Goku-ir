#include "app_data.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <malloc.h>

#define TAG "app_data"
#define NVS_NAMESPACE "ir_data"

esp_err_t app_data_init(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  return err;
}

esp_err_t app_data_save_ir(const char *key, const void *data, size_t len) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS Open for Write failed: %s", esp_err_to_name(err));
    return err;
  }

  err = nvs_set_blob(my_handle, key, data, len);
  if (err == ESP_OK) {
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "NVS Commit failed: %s", esp_err_to_name(err));
  } else {
    ESP_LOGE(TAG, "NVS Set Blob failed: %s", esp_err_to_name(err));
  }
  nvs_close(my_handle);
  return err;
}

esp_err_t app_data_load_ir(const char *key, void *data, size_t *len) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS Open for Read failed: %s", esp_err_to_name(err));
    return err;
  }

  err = nvs_get_blob(my_handle, key, data, len);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS Get Blob '%s' failed: %s", key, esp_err_to_name(err));
  }
  nvs_close(my_handle);
  return err;
}

esp_err_t app_data_delete_ir(const char *key) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS Open for Delete failed: %s", esp_err_to_name(err));
    return err;
  }

  err = nvs_erase_key(my_handle, key);
  if (err == ESP_OK) {
    err = nvs_commit(my_handle);
  }
  nvs_close(my_handle);
  return err;
}

esp_err_t app_data_rename_ir(const char *old_key, const char *new_key) {
  size_t len = 0;
  // Check if old key exists
  esp_err_t err = app_data_load_ir(old_key, NULL, &len);
  if (err != ESP_OK)
    return err;

  // Load old data
  void *data = malloc(len);
  if (!data)
    return ESP_ERR_NO_MEM;

  app_data_load_ir(old_key, data, &len);

  // Save with new key
  err = app_data_save_ir(new_key, data, len);
  free(data);

  if (err != ESP_OK)
    return err;

  // Delete old key
  return app_data_delete_ir(old_key);
}

cJSON *app_data_get_ir_keys(void) {
  cJSON *list = cJSON_CreateArray();
  nvs_iterator_t it = NULL;
  esp_err_t res =
      nvs_entry_find(NVS_DEFAULT_PART_NAME, NVS_NAMESPACE, NVS_TYPE_BLOB, &it);
  while (res == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    cJSON_AddItemToArray(list, cJSON_CreateString(info.key));
    res = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  return list;
}

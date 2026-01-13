#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize WiFi portal (SoftAP + Web Server)
 *
 * This should be called once during application startup.
 * It prepares the portal infrastructure but doesn't start the AP.
 *
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
esp_err_t wifi_portal_init(void);

/**
 * @brief Start WiFi provisioning AP and web captive portal
 *
 * Starts a SoftAP with captive portal for WiFi configuration.
 * The AP SSID will be: CONFIG_IOS_WIFI_AP_SSID_PREFIX-MACADDR
 *
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
esp_err_t wifi_portal_start(void);

/**
 * @brief Stop WiFi portal and clean up resources
 *
 * Stops the SoftAP, HTTP server, and DNS redirect.
 * Should be called after successful provisioning.
 */
void wifi_portal_stop(void);

/**
 * @brief Check if WiFi credentials have been submitted via portal
 *
 * @return true if credentials submitted and saved
 */
bool wifi_portal_is_provisioned(void);

/**
 * @brief Check if WiFi credentials exist in NVS
 *
 * @return true if valid credentials are stored
 */
bool wifi_credentials_exist(void);

/**
 * @brief Load WiFi credentials from NVS
 *
 * @param ssid Buffer to store SSID (must be at least 33 bytes)
 * @param password Buffer to store password (must be at least 65 bytes)
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if no credentials
 */
esp_err_t wifi_credentials_load(char *ssid, char *password);

/**
 * @brief Save WiFi credentials to NVS
 *
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @return ESP_OK on success
 */
esp_err_t wifi_credentials_save(const char *ssid, const char *password);

/**
 * @brief Clear WiFi credentials from NVS
 *
 * Useful for factory reset scenarios.
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_credentials_clear(void);

#ifdef __cplusplus
}
#endif

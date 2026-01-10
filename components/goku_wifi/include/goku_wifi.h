/**
 * @file goku_wifi.h
 * @brief Wi-Fi Connection and Provisioning Manager
 */

#pragma once
#include <esp_err.h>
#include <esp_wifi.h>

/**
 * @brief Initialize Wi-Fi driver
 */
void app_wifi_init(void);

/**
 * @brief Start Wi-Fi
 *
 * @param pop_info Pointer to provisioning information (e.g., QR code data)
 * @return esp_err_t ESP_OK or fail
 */
esp_err_t app_wifi_start(void *pop_info);

/**
 * @brief Update Wi-Fi credentials and restart
 *
 * @param ssid New SSID
 * @param password New Password
 * @return esp_err_t ESP_OK or fail
 */
esp_err_t app_wifi_update_credentials(const char *ssid, const char *password);

/**
 * @brief Scan for Wi-Fi networks
 *
 * @param ap_list Pointer to pointer for the list of APs (caller must free)
 * @param count Pointer to variable to hold the number of APs found
 * @return esp_err_t ESP_OK or fail
 */
esp_err_t app_wifi_get_scan_results(wifi_ap_record_t **ap_list,
                                    uint16_t *count);

/**
 * @brief Check if Wi-Fi (Station) is connected
 *
 * @return true if connected
 * @return false otherwise
 */
bool app_wifi_is_connected(void);

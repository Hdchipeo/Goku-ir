idf_component_register(SRCS "main.c" "app_web.c" "app_data.c" "app_ir.c" "app_wifi.c" "app_led.c" "app_button.c" "app_rainmaker.c" "app_mdns.c" "app_ota.c" "app_log.c" "app_mem.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_http_server nvs_flash esp_partition esp_timer driver esp_netif network_provisioning esp_rainmaker mdns json esp_https_ota esp_http_client mbedtls)

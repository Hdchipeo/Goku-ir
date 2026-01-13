idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES goku_core goku_peripherals goku_wifi goku_ir goku_web goku_rainmaker goku_ac goku_ota goku_homekit goku_wifi_portal goku_sensor nvs_flash esp_netif)

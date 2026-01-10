idf_component_register(SRCS "src/goku_wifi.c" "src/goku_mdns.c"
                        INCLUDE_DIRS "include"
                        REQUIRES goku_core goku_peripherals esp_wifi lwip mdns esp_rainmaker network_provisioning)

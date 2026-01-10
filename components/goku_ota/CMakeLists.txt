idf_component_register(SRCS "src/goku_ota.c"
                        INCLUDE_DIRS "include"
                        REQUIRES goku_core goku_peripherals goku_wifi esp_https_ota mbedtls)

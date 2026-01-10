idf_component_register(SRCS "src/goku_web.c"
                        INCLUDE_DIRS "include"
                        REQUIRES goku_core goku_peripherals goku_wifi goku_ir goku_ac goku_ota esp_http_server esp_https_ota app_update mbedtls json driver)

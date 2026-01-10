idf_component_register(SRCS "src/goku_web.c"
                        INCLUDE_DIRS "include"
                        REQUIRES esp_http_server goku_core goku_wifi goku_ac goku_ir goku_peripherals goku_ota goku_rainmaker json spiffs
)

spiffs_create_partition_image(storage "www" FLASH_IN_PROJECT)

idf_component_register(SRCS "src/goku_rainmaker.c"
                        INCLUDE_DIRS "include"
                        REQUIRES goku_core goku_peripherals goku_ir goku_wifi goku_ac esp_rainmaker nvs_flash app_update)

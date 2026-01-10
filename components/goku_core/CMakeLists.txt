idf_component_register(SRCS "src/goku_data.c" "src/goku_log.c" "src/goku_mem.c"
                        INCLUDE_DIRS "include"
                        REQUIRES nvs_flash esp_timer json)

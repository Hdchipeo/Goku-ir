idf_component_register(SRCS "src/goku_led.c" "src/goku_button.c"
                        INCLUDE_DIRS "include"
                        REQUIRES goku_core led_strip button driver)

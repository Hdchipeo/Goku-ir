set(reqs goku_core goku_peripherals goku_ir goku_wifi goku_ac nvs_flash app_update)

if(CONFIG_APP_LED_CONTROL)
    list(APPEND reqs "led_strip")
endif()

# if(CONFIG_GOKU_SENSOR_AHT20)
    list(APPEND reqs "goku_sensor")
# endif()

idf_component_register(SRCS "src/goku_rainmaker.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ${reqs})

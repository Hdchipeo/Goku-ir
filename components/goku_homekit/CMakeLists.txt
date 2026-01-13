# CMakeLists.txt for goku_homekit

set(reqs goku_core goku_peripherals goku_ir goku_wifi goku_ac nvs_flash goku_web goku_sensor)

# Add HomeKit dependencies unconditionally
# The code is guarded by #if CONFIG_GOKU_PLATFORM_IOS, so linker will remove unused code
list(APPEND reqs esp_hap_core esp_hap_apple_profiles esp_hap_platform qrcode app_hap_setup_payload)

if(CONFIG_GOKU_SENSOR_AHT20)
    list(APPEND reqs goku_sensor)
endif()

idf_component_register(SRCS "src/goku_homekit.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ${reqs})

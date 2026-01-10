idf_component_register(
    SRCS "src/ir_rmt.cpp" "src/ir_universal.cpp" "src/ir_ac_registry.cpp" "src/protocols/ir_nec.cpp" "src/protocols/ir_protocol_daikin.cpp" "src/protocols/ir_protocol_samsung.cpp" "src/protocols/ir_protocol_mitsubishi.cpp" "src/goku_ir_app.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer goku_core goku_peripherals
)

cmake_minimum_required(VERSION 3.16)

# Add external components
# set(EXTRA_COMPONENT_DIRS 
#     /Users/dangminhtam/Project-storage/Framework/esp-iot-solution/components
#     /Users/dangminhtam/Project-storage/Framework/esp-iot-solution/components/ir
# )
# list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/external/esp-homekit-sdk/components")
# Better: Point directly to homekit sub-components to valid conflicts with 'button' in parent dir
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/external/esp-homekit-sdk/components/homekit")

# set(EXCLUDE_COMPONENTS button)
# set project version
set(PROJECT_VERSION "1.4.0")
add_compile_definitions(PROJECT_VERSION=\"${PROJECT_VERSION}\")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(goku-ir-device VERSION ${PROJECT_VERSION})

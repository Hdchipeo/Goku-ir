cmake_minimum_required(VERSION 3.16)

# Add external components
# set(EXTRA_COMPONENT_DIRS 
#     /Users/dangminhtam/Project-storage/Framework/esp-iot-solution/components
#     /Users/dangminhtam/Project-storage/Framework/esp-iot-solution/components/ir
# )

# set(EXCLUDE_COMPONENTS button)
# set project version
set(PROJECT_VERSION "1.0.24")
add_compile_definitions(PROJECT_VERSION=\"${PROJECT_VERSION}\")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(goku-ir-device VERSION ${PROJECT_VERSION})

cmake_minimum_required(VERSION 3.22)

add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/main
    ${CMAKE_CURRENT_SOURCE_DIR}/managed_components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pon-stanok-p4)

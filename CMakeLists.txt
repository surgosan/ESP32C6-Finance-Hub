cmake_minimum_required(VERSION 3.29)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32C6_Finance_Hub)

set(EXTRA_COMPONENT_DIRS
        ${CMAKE_SOURCE_DIR}/components/TFT_eSPI
)

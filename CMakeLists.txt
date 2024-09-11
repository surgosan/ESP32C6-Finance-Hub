cmake_minimum_required(VERSION 3.29)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS
        ${CMAKE_SOURCE_DIR}/components/lvgl
        ${CMAKE_SOURCE_DIR}/components/tft_eSPI
)

project(ESP32C6_Finance_Hub)
#add_executable(FinanceHub main/main.c)


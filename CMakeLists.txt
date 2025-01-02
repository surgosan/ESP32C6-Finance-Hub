cmake_minimum_required(VERSION 3.29)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS
        ${CMAKE_SOURCE_DIR}/components/lvgl-v9.2.2
        ${CMAKE_SOURCE_DIR}/components/espressif__esp_lcd_ili9341-v2.0.0
)


project(ESP32C6_Finance_Hub)
#add_executable(FinanceHub main/main.c)


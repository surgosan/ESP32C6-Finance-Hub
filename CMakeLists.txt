cmake_minimum_required(VERSION 3.29)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS
        ${CMAKE_SOURCE_DIR}/components/lvgl-v9.2.2
        ${CMAKE_SOURCE_DIR}/components/espressif__esp_lcd_ili9341-v2.0.0
)

#spiffs_create_partition_image(spiffs ./spiffs FLASH_IN_PROJECT)

# Custom partition table
idf_build_set_property(COMPILE_OPTIONS "-DIDF_TARGET_ESP32C6" APPEND)
idf_build_set_property(PARTITION_TABLE_CSV "${CMAKE_SOURCE_DIR}/partitions.csv" APPEND)

project(ESP32C6_Finance_Hub)
#add_executable(FinanceHub main/main.c)


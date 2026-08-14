cmake_minimum_required(VERSION 3.16)

# Slim ESP-NOW doorbell transmitter (classic ESP32). Not LumosOS.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../components/lumos_core"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(doorbell_tx)

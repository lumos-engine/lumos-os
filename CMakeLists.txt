cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lumosos)
# Ensure CHIP platform build configs are included (esp_matter / connectedhomeip).
idf_build_set_property(COMPILE_OPTIONS "-DCHIP_HAVE_CONFIG_H" APPEND)
# IDF 5.3 defaults to gnu++2b (C++23), which makes CHIP's to_underlying() ambiguous with std::.
# gnu++20 keeps std::span (used by LumosOS) without std::to_underlying.
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++20" APPEND)

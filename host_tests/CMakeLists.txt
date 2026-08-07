cmake_minimum_required(VERSION 3.16)
project(lumosos_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

add_library(lumos_host_logic STATIC
    ../components/lumos_core/src/color.cpp
    ../components/lumos_core/src/framebuffer.cpp
    ../components/lumos_core/src/logger.cpp
    ../components/lumos_renderer/src/power_limiter.cpp
    ../components/lumos_plugins_builtin/src/ddp_parser.cpp
)

target_include_directories(lumos_host_logic PUBLIC
    stubs
    ../components/lumos_core/include
    ../components/lumos_renderer/include
    ../components/lumos_plugins_builtin/include
)

add_executable(test_ddp_parser test_ddp_parser.cpp)
target_link_libraries(test_ddp_parser PRIVATE lumos_host_logic)
add_test(NAME test_ddp_parser COMMAND test_ddp_parser)

add_executable(test_power_limiter test_power_limiter.cpp)
target_link_libraries(test_power_limiter PRIVATE lumos_host_logic)
add_test(NAME test_power_limiter COMMAND test_power_limiter)

add_executable(test_color test_color.cpp)
target_link_libraries(test_color PRIVATE lumos_host_logic)
add_test(NAME test_color COMMAND test_color)

add_executable(test_plugin_lifecycle test_plugin_lifecycle.cpp)
target_link_libraries(test_plugin_lifecycle PRIVATE lumos_host_logic)
add_test(NAME test_plugin_lifecycle COMMAND test_plugin_lifecycle)

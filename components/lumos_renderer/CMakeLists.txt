idf_component_register(
    SRCS
        "src/gamma.cpp"
        "src/power_limiter.cpp"
        "src/renderer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lumos_core lumos_led_driver
)

idf_component_register(
    SRCS "src/ws2815_rmt_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lumos_core driver
    PRIV_REQUIRES led_strip
)

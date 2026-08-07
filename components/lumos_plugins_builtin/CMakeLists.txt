idf_component_register(
    SRCS
        "src/register_builtins.cpp"
        "src/off_plugin.cpp"
        "src/static_plugin.cpp"
        "src/bias_plugin.cpp"
        "src/rainbow_plugin.cpp"
        "src/hyperhdr_plugin.cpp"
        "src/ddp_transport.cpp"
        "src/ddp_parser.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lumos_core lumos_plugin lumos_preferences lwip esp_netif esp_timer
)

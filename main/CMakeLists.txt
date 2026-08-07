idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_event
        esp_netif
        lumos_core
        lumos_preferences
        lumos_wifi
        lumos_led_driver
        lumos_renderer
        lumos_plugin
        lumos_plugins_builtin
        lumos_api_rest
        lumos_api_ws
        lumos_ota
        lumos_webui
        lumos_matter
)

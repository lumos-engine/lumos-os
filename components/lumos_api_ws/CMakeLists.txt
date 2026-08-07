idf_component_register(
    SRCS "src/ws_api.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lumos_core lumos_plugin lumos_preferences lumos_renderer lumos_wifi json esp_http_server
)

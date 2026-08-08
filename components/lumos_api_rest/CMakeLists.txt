idf_component_register(
    SRCS "src/rest_api.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lumos_core lumos_plugin lumos_preferences lumos_wifi lumos_renderer json esp_http_server esp_timer
)

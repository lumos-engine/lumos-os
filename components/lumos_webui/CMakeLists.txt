idf_component_register(
    SRCS "src/web_ui.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lumos_core esp_http_server
)

idf_component_register(
    SRCS "src/matter_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        lumos_core
        lumos_preferences
        lumos_plugin
        lumos_renderer
        nvs_flash
        esp_matter
)

#include "lumos/api/rest_api.hpp"
#include "lumos/api/ws_api.hpp"
#include "lumos/core/framebuffer.hpp"
#include "lumos/core/logger.hpp"
#include "lumos/core/types.hpp"
#include "lumos/led/ws2815_rmt_driver.hpp"
#include "lumos/ota/ota_service.hpp"
#include "lumos/plugin/plugin_manager.hpp"
#include "lumos/plugins/register_builtins.hpp"
#include "lumos/preferences/preferences.hpp"
#include "lumos/renderer/renderer.hpp"
#include "lumos/webui/web_ui.hpp"
#include "lumos/wifi/wifi_service.hpp"

#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <memory>

namespace {

lumos::Logger log{"main"};

httpd_handle_t start_http_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 40;
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        log.error("Failed to start HTTP server");
        return nullptr;
    }
    return server;
}

void render_loop(void* arg) {
    auto* plugins = static_cast<lumos::PluginManager*>(arg);
    TickType_t last_wake = xTaskGetTickCount();
    TickType_t prev = last_wake;
    constexpr TickType_t kPeriod = pdMS_TO_TICKS(16); // ~60 FPS
    while (true) {
        const TickType_t now = xTaskGetTickCount();
        const float dt = static_cast<float>(now - prev) * portTICK_PERIOD_MS / 1000.0f;
        prev = now;
        plugins->tick(dt > 0.0f ? dt : 0.016f);
        plugins->present();
        vTaskDelayUntil(&last_wake, kPeriod);
    }
}

} // namespace

extern "C" void app_main() {
    log.info("Booting %s %s", lumos::kAppName.data(), lumos::kAppVersion.data());

    auto preferences = std::make_unique<lumos::Preferences>();
    if (!preferences->init()) {
        log.error("Preferences init failed");
        return;
    }

    auto& device = preferences->device();
    auto led_driver = std::make_unique<lumos::Ws2815RmtDriver>();
    lumos::LedDriverConfig led_cfg{
        .gpio = device.gpio,
        .led_count = device.led_count,
        .chipset = device.chipset,
        .color_order = device.color_order,
    };
    if (!led_driver->init(led_cfg)) {
        log.error("LED driver init failed");
        return;
    }

    lumos::RendererConfig renderer_cfg{
        .brightness = device.brightness,
        .gamma = device.gamma,
        .chipset = device.chipset,
        .color_order = device.color_order,
        .balance_r = device.balance_r,
        .balance_g = device.balance_g,
        .balance_b = device.balance_b,
        .power = {.max_ma = device.power_limit_ma},
    };
    auto renderer = std::make_unique<lumos::Renderer>(*led_driver, renderer_cfg);
    renderer->init(device.led_count);
    renderer->set_ignored_leds(device.ignored_leds);

    auto framebuffer = std::make_unique<lumos::Framebuffer>(device.led_count);
    auto plugins = std::make_unique<lumos::PluginManager>(*preferences, *renderer, *framebuffer);
    lumos::register_builtin_plugins(*plugins);
    if (!plugins->initialize_all()) {
        log.error("Plugin init failed");
        return;
    }

    auto wifi = std::make_unique<lumos::WifiService>(*preferences);
    wifi->start();

    httpd_handle_t server = start_http_server();
    if (server == nullptr) {
        return;
    }

    auto webui = std::make_unique<lumos::WebUi>();
    auto rest =
        std::make_unique<lumos::RestApi>(*preferences, *plugins, *renderer, *wifi, *framebuffer);
    auto ws =
        std::make_unique<lumos::WsApi>(*preferences, *plugins, *renderer, *wifi, *framebuffer);
    auto ota = std::make_unique<lumos::OtaService>();

    webui->start(server);
    rest->start(server);
    ws->start(server);
    ota->start(server);

    plugins->activate_startup_plugin();

    // Keep services alive for the lifetime of the device.
    // Leak intentionally into static storage via task ownership.
    static auto s_preferences = std::move(preferences);
    static auto s_led_driver = std::move(led_driver);
    static auto s_renderer = std::move(renderer);
    static auto s_framebuffer = std::move(framebuffer);
    static auto s_plugins = std::move(plugins);
    static auto s_wifi = std::move(wifi);
    static auto s_webui = std::move(webui);
    static auto s_rest = std::move(rest);
    static auto s_ws = std::move(ws);
    static auto s_ota = std::move(ota);

    xTaskCreate(render_loop, "render", 8192, s_plugins.get(), 6, nullptr);
    log.info("LumosOS ready");
}

#pragma once

#include "lumos/core/framebuffer.hpp"
#include "lumos/plugin/plugin_manager.hpp"
#include "lumos/preferences/preferences.hpp"
#include "lumos/renderer/renderer.hpp"
#include "lumos/wifi/wifi_service.hpp"

#include <esp_http_server.h>

namespace lumos {

class RestApi {
public:
    RestApi(Preferences& preferences, PluginManager& plugins, Renderer& renderer, WifiService& wifi,
            const Framebuffer& framebuffer);

    Result<void> start(httpd_handle_t server);
    void set_server(httpd_handle_t server) { server_ = server; }

private:
    static esp_err_t get_root(httpd_req_t* req);
    static esp_err_t get_plugins(httpd_req_t* req);
    static esp_err_t get_plugin(httpd_req_t* req);
    static esp_err_t post_plugin(httpd_req_t* req);
    static esp_err_t post_brightness(httpd_req_t* req);
    static esp_err_t get_settings(httpd_req_t* req);
    static esp_err_t post_settings(httpd_req_t* req);
    static esp_err_t get_status(httpd_req_t* req);
    static esp_err_t get_leds(httpd_req_t* req);
    static esp_err_t get_wifi_scan(httpd_req_t* req);
    static esp_err_t post_wifi(httpd_req_t* req);
    static esp_err_t get_neighbors(httpd_req_t* req);
    static esp_err_t get_matter(httpd_req_t* req);
    static esp_err_t post_matter_factory_reset(httpd_req_t* req);
    static esp_err_t get_wled_json(httpd_req_t* req);
    static esp_err_t put_wled_state(httpd_req_t* req);

    static RestApi* from_req(httpd_req_t* req);
    static esp_err_t send_json(httpd_req_t* req, const char* json, int status = 200);
    static esp_err_t read_body(httpd_req_t* req, std::string& out);
    static std::string build_leds_json(const Framebuffer& fb);

    Preferences& preferences_;
    PluginManager& plugins_;
    Renderer& renderer_;
    WifiService& wifi_;
    const Framebuffer& framebuffer_;
    httpd_handle_t server_{nullptr};
    bool wled_on_{true};
    bool wled_live_{false};
};

} // namespace lumos

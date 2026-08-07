#pragma once

#include "lumos/core/framebuffer.hpp"
#include "lumos/plugin/plugin_manager.hpp"
#include "lumos/preferences/preferences.hpp"
#include "lumos/renderer/renderer.hpp"
#include "lumos/wifi/wifi_service.hpp"

#include <esp_http_server.h>

#include <mutex>
#include <string>
#include <vector>

namespace lumos {

class WsApi {
public:
    WsApi(Preferences& preferences, PluginManager& plugins, Renderer& renderer, WifiService& wifi,
          const Framebuffer& framebuffer);

    Result<void> start(httpd_handle_t server);
    void broadcast_state();

private:
    static esp_err_t ws_handler(httpd_req_t* req);
    static void broadcast_task(void* arg);
    std::string build_state_json() const;

    Preferences& preferences_;
    PluginManager& plugins_;
    Renderer& renderer_;
    WifiService& wifi_;
    const Framebuffer& framebuffer_;
    httpd_handle_t server_{nullptr};
    std::mutex clients_mu_;
    std::vector<int> client_fds_;
    std::string outbound_; // must outlive async WS sends
};

} // namespace lumos

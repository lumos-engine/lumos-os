#include "lumos/api/ws_api.hpp"
#include "lumos/core/logger.hpp"
#include "lumos/core/types.hpp"

#include <cJSON.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

namespace lumos {
namespace {
Logger log{"ws"};
}

WsApi::WsApi(Preferences& preferences, PluginManager& plugins, Renderer& renderer, WifiService& wifi,
             const Framebuffer& framebuffer)
    : preferences_(preferences),
      plugins_(plugins),
      renderer_(renderer),
      wifi_(wifi),
      framebuffer_(framebuffer) {
    (void)framebuffer_;
}

std::string WsApi::build_state_json() const {
    const auto wifi = wifi_.status();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "state");
    cJSON_AddStringToObject(root, "name", kAppName.data());
    cJSON_AddStringToObject(root, "version", kAppVersion.data());
    cJSON_AddStringToObject(root, "active_plugin", plugins_.active_id().c_str());
    cJSON_AddBoolToObject(root, "in_fallback", plugins_.in_fallback());
    cJSON_AddNumberToObject(root, "brightness", renderer_.brightness());
    cJSON_AddNumberToObject(root, "power_scale", renderer_.last_power_scale());
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    cJSON* w = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddBoolToObject(w, "connected", wifi.connected);
    cJSON_AddBoolToObject(w, "use_static", wifi.use_static);
    cJSON_AddStringToObject(w, "ip", wifi.ip.c_str());
    cJSON_AddStringToObject(w, "gateway", wifi.gateway.c_str());
    cJSON_AddStringToObject(w, "netmask", wifi.netmask.c_str());
    cJSON_AddStringToObject(w, "dns1", wifi.dns1.c_str());
    cJSON_AddStringToObject(w, "dns2", wifi.dns2.c_str());
    cJSON_AddStringToObject(w, "ssid", wifi.ssid.c_str());
    cJSON_AddNumberToObject(w, "mode", static_cast<int>(wifi.mode));
    char* printed = cJSON_PrintUnformatted(root);
    std::string out = printed ? printed : "{}";
    cJSON_free(printed);
    cJSON_Delete(root);
    return out;
}

esp_err_t WsApi::ws_handler(httpd_req_t* req) {
    auto* self = static_cast<WsApi*>(req->user_ctx);
    if (req->method == HTTP_GET) {
        const int fd = httpd_req_to_sockfd(req);
        {
            std::lock_guard<std::mutex> lock(self->clients_mu_);
            self->client_fds_.push_back(fd);
        }
        log.info("WebSocket client connected fd=%d", fd);
        return ESP_OK;
    }

    httpd_ws_frame_t frame{};
    frame.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    if (frame.len == 0) {
        return ESP_OK;
    }

    std::vector<std::uint8_t> buf(frame.len + 1);
    frame.payload = buf.data();
    ret = httpd_ws_recv_frame(req, &frame, frame.len);
    if (ret != ESP_OK) {
        return ret;
    }
    buf[frame.len] = 0;

    if (std::strstr(reinterpret_cast<char*>(buf.data()), "ping") != nullptr) {
        const char* pong = "{\"type\":\"pong\"}";
        httpd_ws_frame_t out{};
        out.type = HTTPD_WS_TYPE_TEXT;
        out.payload = reinterpret_cast<std::uint8_t*>(const_cast<char*>(pong));
        out.len = std::strlen(pong);
        return httpd_ws_send_frame(req, &out);
    }

    self->outbound_ = self->build_state_json();
    httpd_ws_frame_t out{};
    out.type = HTTPD_WS_TYPE_TEXT;
    out.payload = reinterpret_cast<std::uint8_t*>(self->outbound_.data());
    out.len = self->outbound_.size();
    return httpd_ws_send_frame(req, &out);
}

void WsApi::broadcast_state() {
    if (server_ == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(clients_mu_);
    if (client_fds_.empty()) {
        return;
    }
    // Keep payload alive for async sends.
    outbound_ = build_state_json();
    for (auto it = client_fds_.begin(); it != client_fds_.end();) {
        httpd_ws_frame_t frame{};
        frame.type = HTTPD_WS_TYPE_TEXT;
        frame.payload = reinterpret_cast<std::uint8_t*>(outbound_.data());
        frame.len = outbound_.size();
        esp_err_t err = httpd_ws_send_frame_async(server_, *it, &frame);
        if (err != ESP_OK) {
            it = client_fds_.erase(it);
        } else {
            ++it;
        }
    }
}

void WsApi::broadcast_task(void* arg) {
    auto* self = static_cast<WsApi*>(arg);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        self->broadcast_state();
    }
}

Result<void> WsApi::start(httpd_handle_t server) {
    server_ = server;
    httpd_uri_t ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = this,
        .is_websocket = true,
    };
    esp_err_t err = httpd_register_uri_handler(server_, &ws);
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "failed to register websocket");
    }
    xTaskCreate(&WsApi::broadcast_task, "ws_broadcast", 4096, this, 4, nullptr);
    return Result<void>::ok();
}

} // namespace lumos

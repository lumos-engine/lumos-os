#include "lumos/api/rest_api.hpp"
#include "lumos/core/types.hpp"

#include <cJSON.h>
#include "esp_system.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace lumos {
namespace {

cJSON* descriptor_to_json(const PluginDescriptor& d) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", d.id.c_str());
    cJSON_AddStringToObject(obj, "name", d.name.c_str());
    cJSON_AddStringToObject(obj, "icon", d.icon.c_str());
    cJSON_AddBoolToObject(obj, "default", d.is_default);
    cJSON* params = cJSON_AddArrayToObject(obj, "parameters");
    for (const auto& p : d.parameters) {
        cJSON* pj = cJSON_CreateObject();
        cJSON_AddStringToObject(pj, "id", p.id.c_str());
        cJSON_AddStringToObject(pj, "name", p.name.c_str());
        const char* type = "int";
        switch (p.type) {
        case ParamType::Bool:
            type = "bool";
            break;
        case ParamType::Float:
            type = "float";
            break;
        case ParamType::Color:
            type = "color";
            break;
        case ParamType::Enum:
            type = "enum";
            break;
        case ParamType::String:
            type = "string";
            break;
        case ParamType::Int:
        default:
            type = "int";
            break;
        }
        cJSON_AddStringToObject(pj, "type", type);
        cJSON_AddStringToObject(pj, "default", p.default_value.c_str());
        if (!p.min_value.empty()) {
            cJSON_AddStringToObject(pj, "min", p.min_value.c_str());
        }
        if (!p.max_value.empty()) {
            cJSON_AddStringToObject(pj, "max", p.max_value.c_str());
        }
        if (!p.enum_values.empty()) {
            cJSON* ev = cJSON_AddArrayToObject(pj, "enum");
            for (const auto& v : p.enum_values) {
                cJSON_AddItemToArray(ev, cJSON_CreateString(v.c_str()));
            }
        }
        cJSON_AddItemToArray(params, pj);
    }
    return obj;
}

} // namespace

RestApi::RestApi(Preferences& preferences, PluginManager& plugins, Renderer& renderer,
                 WifiService& wifi)
    : preferences_(preferences), plugins_(plugins), renderer_(renderer), wifi_(wifi) {}

RestApi* RestApi::from_req(httpd_req_t* req) {
    return static_cast<RestApi*>(req->user_ctx);
}

esp_err_t RestApi::send_json(httpd_req_t* req, const char* json, int status) {
    httpd_resp_set_status(req, status == 200 ? "200 OK" : (status == 404 ? "404 Not Found" : "400 Bad Request"));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t RestApi::read_body(httpd_req_t* req, std::string& out) {
    const int total = req->content_len;
    if (total <= 0 || total > 4096) {
        out.clear();
        return ESP_OK;
    }
    out.resize(static_cast<std::size_t>(total));
    int received = 0;
    while (received < total) {
        const int r = httpd_req_recv(req, out.data() + received, total - received);
        if (r <= 0) {
            return ESP_FAIL;
        }
        received += r;
    }
    return ESP_OK;
}

esp_err_t RestApi::get_root(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", kAppName.data());
    cJSON_AddStringToObject(root, "version", kAppVersion.data());
    cJSON* links = cJSON_AddObjectToObject(root, "links");
    cJSON_AddStringToObject(links, "plugins", "/api/v1/plugins");
    cJSON_AddStringToObject(links, "settings", "/api/v1/settings");
    cJSON_AddStringToObject(links, "status", "/api/v1/status");
    cJSON_AddStringToObject(links, "ws", "/ws");
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    esp_err_t err = send_json(req, printed);
    cJSON_free(printed);
    return err;
}

esp_err_t RestApi::get_plugins(httpd_req_t* req) {
    auto* self = from_req(req);
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "active", self->plugins_.active_id().c_str());
    cJSON* arr = cJSON_AddArrayToObject(root, "plugins");
    for (const auto* d : self->plugins_.list_descriptors()) {
        cJSON_AddItemToArray(arr, descriptor_to_json(*d));
    }
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    esp_err_t err = send_json(req, printed);
    cJSON_free(printed);
    return err;
}

esp_err_t RestApi::get_plugin(httpd_req_t* req) {
    auto* self = from_req(req);
    const char* uri = req->uri;
    const char* id = std::strrchr(uri, '/');
    if (id == nullptr || *(id + 1) == '\0') {
        return send_json(req, "{\"error\":\"missing id\"}", 400);
    }
    ++id;
    auto* plugin = self->plugins_.find(id);
    if (plugin == nullptr) {
        return send_json(req, "{\"error\":\"not found\"}", 404);
    }
    cJSON* obj = descriptor_to_json(plugin->descriptor());
    cJSON* values = cJSON_AddObjectToObject(obj, "values");
    for (const auto& [k, v] : self->preferences_.plugin_params(id)) {
        cJSON_AddStringToObject(values, k.c_str(), v.c_str());
    }
    char* printed = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    esp_err_t err = send_json(req, printed);
    cJSON_free(printed);
    return err;
}

esp_err_t RestApi::post_plugin(httpd_req_t* req) {
    auto* self = from_req(req);
    const char* uri = req->uri;
    const char* id = std::strrchr(uri, '/');
    if (id == nullptr) {
        return send_json(req, "{\"error\":\"missing id\"}", 400);
    }
    ++id;
    std::string body;
    if (read_body(req, body) != ESP_OK) {
        return send_json(req, "{\"error\":\"bad body\"}", 400);
    }

    cJSON* json = body.empty() ? nullptr : cJSON_Parse(body.c_str());
    if (json != nullptr) {
        const cJSON* params = cJSON_GetObjectItem(json, "parameters");
        if (cJSON_IsObject(params)) {
            const cJSON* item = nullptr;
            cJSON_ArrayForEach(item, params) {
                if (cJSON_IsString(item)) {
                    self->preferences_.set_plugin_param(id, item->string, item->valuestring);
                } else if (cJSON_IsNumber(item)) {
                    self->preferences_.set_plugin_param(id, item->string,
                                                        std::to_string(item->valuedouble));
                } else if (cJSON_IsBool(item)) {
                    self->preferences_.set_plugin_param(id, item->string,
                                                        cJSON_IsTrue(item) ? "1" : "0");
                }
            }
            self->preferences_.save();
        }
        cJSON_Delete(json);
    }

    auto result = self->plugins_.activate(id);
    if (!result) {
        return send_json(req, "{\"error\":\"activate failed\"}", 400);
    }
    return send_json(req, "{\"ok\":true}");
}

esp_err_t RestApi::post_brightness(httpd_req_t* req) {
    auto* self = from_req(req);
    std::string body;
    if (read_body(req, body) != ESP_OK) {
        return send_json(req, "{\"error\":\"bad body\"}", 400);
    }
    cJSON* json = cJSON_Parse(body.c_str());
    if (json == nullptr) {
        return send_json(req, "{\"error\":\"invalid json\"}", 400);
    }
    const cJSON* b = cJSON_GetObjectItem(json, "brightness");
    if (!cJSON_IsNumber(b)) {
        cJSON_Delete(json);
        return send_json(req, "{\"error\":\"brightness required\"}", 400);
    }
    const auto value = static_cast<Brightness>(std::clamp(b->valueint, 0, 255));
    self->preferences_.device().brightness = value;
    self->renderer_.set_brightness(value);
    self->preferences_.save();
    cJSON_Delete(json);
    return send_json(req, "{\"ok\":true}");
}

esp_err_t RestApi::get_settings(httpd_req_t* req) {
    auto* self = from_req(req);
    const auto& d = self->preferences_.device();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "led_count", d.led_count);
    cJSON_AddNumberToObject(root, "gpio", d.gpio);
    cJSON_AddNumberToObject(root, "brightness", d.brightness);
    cJSON_AddNumberToObject(root, "gamma", d.gamma);
    cJSON_AddNumberToObject(root, "power_limit_ma", d.power_limit_ma);
    cJSON_AddNumberToObject(root, "startup_plugin", static_cast<int>(d.startup_plugin));
    cJSON_AddNumberToObject(root, "fallback_plugin", static_cast<int>(d.fallback_plugin));
    cJSON_AddNumberToObject(root, "hyperhdr_timeout_ms", d.hyperhdr_timeout_ms);
    cJSON_AddStringToObject(root, "hostname", d.hostname.c_str());
    cJSON_AddStringToObject(root, "last_used_plugin", d.last_used_plugin.c_str());
    cJSON_AddStringToObject(root, "wifi_ssid", d.wifi_ssid.c_str());
    cJSON_AddBoolToObject(root, "wifi_use_static", d.wifi_use_static);
    cJSON_AddStringToObject(root, "wifi_ip", d.wifi_ip.c_str());
    cJSON_AddStringToObject(root, "wifi_gateway", d.wifi_gateway.c_str());
    cJSON_AddStringToObject(root, "wifi_netmask", d.wifi_netmask.c_str());
    cJSON_AddStringToObject(root, "wifi_dns", d.wifi_dns.c_str());
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    esp_err_t err = send_json(req, printed);
    cJSON_free(printed);
    return err;
}

esp_err_t RestApi::post_settings(httpd_req_t* req) {
    auto* self = from_req(req);
    std::string body;
    if (read_body(req, body) != ESP_OK) {
        return send_json(req, "{\"error\":\"bad body\"}", 400);
    }
    cJSON* json = cJSON_Parse(body.c_str());
    if (json == nullptr) {
        return send_json(req, "{\"error\":\"invalid json\"}", 400);
    }
    auto& d = self->preferences_.device();
    if (const cJSON* v = cJSON_GetObjectItem(json, "brightness"); cJSON_IsNumber(v)) {
        d.brightness = static_cast<Brightness>(std::clamp(v->valueint, 0, 255));
        self->renderer_.set_brightness(d.brightness);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "gamma"); cJSON_IsNumber(v)) {
        d.gamma = static_cast<float>(v->valuedouble);
        self->renderer_.set_gamma(d.gamma);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "power_limit_ma"); cJSON_IsNumber(v)) {
        d.power_limit_ma = static_cast<std::uint16_t>(v->valueint);
        self->renderer_.set_power_limit_ma(d.power_limit_ma);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "led_count"); cJSON_IsNumber(v)) {
        d.led_count = static_cast<LedIndex>(v->valueint);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "gpio"); cJSON_IsNumber(v)) {
        d.gpio = v->valueint;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "startup_plugin"); cJSON_IsNumber(v)) {
        d.startup_plugin = static_cast<StartupPluginMode>(v->valueint);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "fallback_plugin"); cJSON_IsNumber(v)) {
        d.fallback_plugin = static_cast<FallbackPluginMode>(v->valueint);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "hyperhdr_timeout_ms"); cJSON_IsNumber(v)) {
        d.hyperhdr_timeout_ms = static_cast<Milliseconds>(v->valueint);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "hostname"); cJSON_IsString(v)) {
        d.hostname = v->valuestring;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "wifi_use_static"); cJSON_IsBool(v)) {
        d.wifi_use_static = cJSON_IsTrue(v);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "wifi_ip"); cJSON_IsString(v)) {
        d.wifi_ip = v->valuestring;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "wifi_gateway"); cJSON_IsString(v)) {
        d.wifi_gateway = v->valuestring;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "wifi_netmask"); cJSON_IsString(v)) {
        d.wifi_netmask = v->valuestring;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "wifi_dns"); cJSON_IsString(v)) {
        d.wifi_dns = v->valuestring;
    }
    self->preferences_.save();
    cJSON_Delete(json);
    return send_json(req, "{\"ok\":true}");
}

esp_err_t RestApi::get_status(httpd_req_t* req) {
    auto* self = from_req(req);
    const auto wifi = self->wifi_.status();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", kAppName.data());
    cJSON_AddStringToObject(root, "version", kAppVersion.data());
    cJSON_AddStringToObject(root, "active_plugin", self->plugins_.active_id().c_str());
    cJSON_AddBoolToObject(root, "in_fallback", self->plugins_.in_fallback());
    cJSON_AddNumberToObject(root, "brightness", self->renderer_.brightness());
    cJSON_AddNumberToObject(root, "power_scale", self->renderer_.last_power_scale());
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    cJSON* w = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddBoolToObject(w, "connected", wifi.connected);
    cJSON_AddBoolToObject(w, "use_static", wifi.use_static);
    cJSON_AddStringToObject(w, "ip", wifi.ip.c_str());
    cJSON_AddStringToObject(w, "gateway", wifi.gateway.c_str());
    cJSON_AddStringToObject(w, "netmask", wifi.netmask.c_str());
    cJSON_AddStringToObject(w, "dns", wifi.dns.c_str());
    cJSON_AddStringToObject(w, "ssid", wifi.ssid.c_str());
    cJSON_AddNumberToObject(w, "mode", static_cast<int>(wifi.mode));
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    esp_err_t err = send_json(req, printed);
    cJSON_free(printed);
    return err;
}

esp_err_t RestApi::get_wifi_scan(httpd_req_t* req) {
    auto* self = from_req(req);
    auto result = self->wifi_.scan();
    if (!result) {
        return send_json(req, "{\"error\":\"scan failed\"}", 400);
    }

    cJSON* root = cJSON_CreateObject();
    cJSON* arr = cJSON_AddArrayToObject(root, "networks");
    for (const auto& n : result.value()) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", n.ssid.c_str());
        cJSON_AddNumberToObject(item, "rssi", n.rssi);
        cJSON_AddNumberToObject(item, "channel", n.channel);
        cJSON_AddBoolToObject(item, "secure", n.secure);
        cJSON_AddItemToArray(arr, item);
    }
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    esp_err_t err = send_json(req, printed);
    cJSON_free(printed);
    return err;
}

esp_err_t RestApi::post_wifi(httpd_req_t* req) {
    auto* self = from_req(req);
    std::string body;
    if (read_body(req, body) != ESP_OK) {
        return send_json(req, "{\"error\":\"bad body\"}", 400);
    }
    cJSON* json = cJSON_Parse(body.c_str());
    if (json == nullptr) {
        return send_json(req, "{\"error\":\"invalid json\"}", 400);
    }
    auto& d = self->preferences_.device();
    const cJSON* ssid = cJSON_GetObjectItem(json, "ssid");
    const cJSON* pass = cJSON_GetObjectItem(json, "password");
    if (!cJSON_IsString(ssid)) {
        cJSON_Delete(json);
        return send_json(req, "{\"error\":\"ssid required\"}", 400);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "use_static"); cJSON_IsBool(v)) {
        d.wifi_use_static = cJSON_IsTrue(v);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "ip"); cJSON_IsString(v)) {
        d.wifi_ip = v->valuestring;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "gateway"); cJSON_IsString(v)) {
        d.wifi_gateway = v->valuestring;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "netmask"); cJSON_IsString(v)) {
        d.wifi_netmask = v->valuestring;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "dns"); cJSON_IsString(v)) {
        d.wifi_dns = v->valuestring;
    }
    if (d.wifi_use_static && (d.wifi_ip.empty() || d.wifi_gateway.empty())) {
        cJSON_Delete(json);
        return send_json(req, "{\"error\":\"static IP requires ip and gateway\"}", 400);
    }
    const char* password = cJSON_IsString(pass) ? pass->valuestring : "";
    auto result = self->wifi_.connect_sta(ssid->valuestring, password);
    cJSON_Delete(json);
    if (!result) {
        return send_json(req, "{\"error\":\"connect failed\"}", 400);
    }
    return send_json(req, "{\"ok\":true}");
}

Result<void> RestApi::start(httpd_handle_t server) {
    server_ = server;

    const httpd_uri_t routes[] = {
        {.uri = "/api/v1", .method = HTTP_GET, .handler = get_root, .user_ctx = this},
        {.uri = "/api/v1/plugins", .method = HTTP_GET, .handler = get_plugins, .user_ctx = this},
        {.uri = "/api/v1/plugin/*", .method = HTTP_GET, .handler = get_plugin, .user_ctx = this},
        {.uri = "/api/v1/plugin/*", .method = HTTP_POST, .handler = post_plugin, .user_ctx = this},
        {.uri = "/api/v1/brightness", .method = HTTP_POST, .handler = post_brightness, .user_ctx = this},
        {.uri = "/api/v1/settings", .method = HTTP_GET, .handler = get_settings, .user_ctx = this},
        {.uri = "/api/v1/settings", .method = HTTP_POST, .handler = post_settings, .user_ctx = this},
        {.uri = "/api/v1/status", .method = HTTP_GET, .handler = get_status, .user_ctx = this},
        {.uri = "/api/v1/wifi/scan", .method = HTTP_GET, .handler = get_wifi_scan, .user_ctx = this},
        {.uri = "/api/v1/wifi", .method = HTTP_POST, .handler = post_wifi, .user_ctx = this},
    };

    for (const auto& route : routes) {
        esp_err_t err = httpd_register_uri_handler(server_, &route);
        if (err != ESP_OK) {
            return Result<void>::fail(ErrorCode::IoError, "failed to register REST route");
        }
    }
    return Result<void>::ok();
}

} // namespace lumos

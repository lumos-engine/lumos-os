#include "lumos/api/rest_api.hpp"
#include "lumos/core/led_calibration.hpp"
#include "lumos/core/types.hpp"
#include "lumos/wifi/neighbor_info.hpp"

#include <cJSON.h>
#include "esp_system.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace lumos {
namespace {

const char* category_to_string(PluginCategory c) {
    switch (c) {
    case PluginCategory::Solid:
        return "solid";
    case PluginCategory::Stream:
        return "stream";
    case PluginCategory::Utility:
        return "utility";
    case PluginCategory::Effect:
    default:
        return "effect";
    }
}

cJSON* descriptor_to_json(const PluginDescriptor& d) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", d.id.c_str());
    cJSON_AddStringToObject(obj, "name", d.name.c_str());
    cJSON_AddStringToObject(obj, "icon", d.icon.c_str());
    cJSON_AddBoolToObject(obj, "default", d.is_default);

    cJSON* caps = cJSON_AddObjectToObject(obj, "capabilities");
    cJSON_AddStringToObject(caps, "category", category_to_string(d.capabilities.category));
    cJSON_AddBoolToObject(caps, "realtime", d.capabilities.realtime);
    cJSON_AddBoolToObject(caps, "needs_network", d.capabilities.needs_network);
    cJSON_AddBoolToObject(caps, "supports_audio", d.capabilities.supports_audio);
    cJSON_AddStringToObject(caps, "output", d.capabilities.output.c_str());
    cJSON* tags = cJSON_AddArrayToObject(caps, "tags");
    for (const auto& t : d.capabilities.tags) {
        cJSON_AddItemToArray(tags, cJSON_CreateString(t.c_str()));
    }

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
        if (!p.description.empty()) {
            cJSON_AddStringToObject(pj, "description", p.description.c_str());
        }
        if (!p.group.empty()) {
            cJSON_AddStringToObject(pj, "group", p.group.c_str());
        }
        if (!p.unit.empty()) {
            cJSON_AddStringToObject(pj, "unit", p.unit.c_str());
        }
        if (!p.step.empty()) {
            cJSON_AddStringToObject(pj, "step", p.step.c_str());
        }
        cJSON_AddBoolToObject(pj, "advanced", p.advanced);
        cJSON_AddItemToArray(params, pj);
    }
    return obj;
}

} // namespace

RestApi::RestApi(Preferences& preferences, PluginManager& plugins, Renderer& renderer,
                 WifiService& wifi, const Framebuffer& framebuffer)
    : preferences_(preferences),
      plugins_(plugins),
      renderer_(renderer),
      wifi_(wifi),
      framebuffer_(framebuffer) {}

std::string RestApi::build_leds_json(const Framebuffer& fb) {
    const LedIndex count = fb.size();
    std::string hex;
    hex.resize(static_cast<std::size_t>(count) * 6);
    static constexpr char kHex[] = "0123456789abcdef";
    for (LedIndex i = 0; i < count; ++i) {
        const auto& c = fb[i];
        const std::size_t o = static_cast<std::size_t>(i) * 6;
        hex[o + 0] = kHex[c.r >> 4];
        hex[o + 1] = kHex[c.r & 0x0F];
        hex[o + 2] = kHex[c.g >> 4];
        hex[o + 3] = kHex[c.g & 0x0F];
        hex[o + 4] = kHex[c.b >> 4];
        hex[o + 5] = kHex[c.b & 0x0F];
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "leds");
    cJSON_AddNumberToObject(root, "count", count);
    cJSON_AddStringToObject(root, "layout", "perimeter");
    cJSON_AddStringToObject(root, "order", "clockwise_top_left");
    cJSON_AddStringToObject(root, "rgb_hex", hex.c_str());
    char* printed = cJSON_PrintUnformatted(root);
    std::string out = printed ? printed : "{}";
    cJSON_free(printed);
    cJSON_Delete(root);
    return out;
}

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
    cJSON_AddStringToObject(root, "api", kApiVersion.data());
    cJSON* links = cJSON_AddObjectToObject(root, "links");
    cJSON_AddStringToObject(links, "plugins", "/api/v1/plugins");
    cJSON_AddStringToObject(links, "settings", "/api/v1/settings");
    cJSON_AddStringToObject(links, "config", "/api/v1/config");
    cJSON_AddStringToObject(links, "status", "/api/v1/status");
    cJSON_AddStringToObject(links, "neighbors", "/api/v1/neighbors");
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
        // Accept either { "parameters": { ... } } or top-level { "r": 255, ... }.
        const cJSON* params = cJSON_GetObjectItem(json, "parameters");
        const cJSON* bag = cJSON_IsObject(params) ? params : json;
        bool wrote = false;
        if (cJSON_IsObject(bag)) {
            const cJSON* item = nullptr;
            cJSON_ArrayForEach(item, bag) {
                if (item->string == nullptr || std::strcmp(item->string, "parameters") == 0) {
                    continue;
                }
                if (cJSON_IsString(item)) {
                    self->preferences_.set_plugin_param(id, item->string, item->valuestring);
                    wrote = true;
                } else if (cJSON_IsNumber(item)) {
                    self->preferences_.set_plugin_param(id, item->string,
                                                        std::to_string(static_cast<int>(item->valuedouble)));
                    wrote = true;
                } else if (cJSON_IsBool(item)) {
                    self->preferences_.set_plugin_param(id, item->string,
                                                        cJSON_IsTrue(item) ? "1" : "0");
                    wrote = true;
                }
            }
            if (wrote) {
                self->preferences_.save();
            }
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

cJSON* device_settings_to_json(const DeviceSettings& d, bool include_secrets) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "led_count", d.led_count);
    cJSON_AddNumberToObject(root, "gpio", d.gpio);
    cJSON_AddNumberToObject(root, "chipset", static_cast<int>(d.chipset));
    cJSON_AddNumberToObject(root, "color_order", static_cast<int>(d.color_order));
    cJSON_AddNumberToObject(root, "white_algorithm", static_cast<int>(d.white_algorithm));
    cJSON_AddNumberToObject(root, "brightness", d.brightness);
    cJSON_AddNumberToObject(root, "gamma", d.gamma);
    cJSON_AddNumberToObject(root, "balance_r", d.balance_r);
    cJSON_AddNumberToObject(root, "balance_g", d.balance_g);
    cJSON_AddNumberToObject(root, "balance_b", d.balance_b);
    cJSON_AddNumberToObject(root, "power_limit_ma", d.power_limit_ma);
    cJSON* layout = cJSON_AddObjectToObject(root, "layout");
    cJSON_AddNumberToObject(layout, "top", d.layout.top);
    cJSON_AddNumberToObject(layout, "right", d.layout.right);
    cJSON_AddNumberToObject(layout, "bottom", d.layout.bottom);
    cJSON_AddNumberToObject(layout, "left", d.layout.left);
    cJSON* edge = cJSON_AddObjectToObject(root, "edge_ignore");
    cJSON_AddNumberToObject(edge, "skip_start", d.edge_ignore.skip_start);
    cJSON_AddNumberToObject(edge, "skip_end", d.edge_ignore.skip_end);
    cJSON_AddNumberToObject(edge, "corner_tr", d.edge_ignore.corner_tr);
    cJSON_AddNumberToObject(edge, "corner_br", d.edge_ignore.corner_br);
    cJSON_AddNumberToObject(edge, "corner_bl", d.edge_ignore.corner_bl);
    cJSON_AddNumberToObject(edge, "corner_tl", d.edge_ignore.corner_tl);
    cJSON* ignored = cJSON_AddArrayToObject(root, "ignored_leds");
    for (std::uint16_t idx : d.ignored_leds) {
        cJSON_AddItemToArray(ignored, cJSON_CreateNumber(idx));
    }
    cJSON_AddNumberToObject(root, "active_led_count",
                            static_cast<int>(d.led_count) - static_cast<int>(d.ignored_leds.size()));
    cJSON_AddNumberToObject(root, "startup_plugin", static_cast<int>(d.startup_plugin));
    cJSON_AddNumberToObject(root, "fallback_plugin", static_cast<int>(d.fallback_plugin));
    cJSON_AddNumberToObject(root, "hyperhdr_timeout_ms", d.hyperhdr_timeout_ms);
    cJSON_AddStringToObject(root, "hostname", d.hostname.c_str());
    cJSON_AddStringToObject(root, "last_used_plugin", d.last_used_plugin.c_str());
    cJSON_AddStringToObject(root, "wifi_ssid", d.wifi_ssid.c_str());
    if (include_secrets) {
        cJSON_AddStringToObject(root, "wifi_password", d.wifi_password.c_str());
    }
    cJSON_AddBoolToObject(root, "wifi_use_static", d.wifi_use_static);
    cJSON_AddStringToObject(root, "wifi_ip", d.wifi_ip.c_str());
    cJSON_AddStringToObject(root, "wifi_gateway", d.wifi_gateway.c_str());
    cJSON_AddStringToObject(root, "wifi_netmask", d.wifi_netmask.c_str());
    cJSON_AddStringToObject(root, "wifi_dns1", d.wifi_dns1.c_str());
    cJSON_AddStringToObject(root, "wifi_dns2", d.wifi_dns2.c_str());
    return root;
}

cJSON* plugin_params_to_json(const Preferences& preferences) {
    cJSON* root = cJSON_CreateObject();
    for (const auto& [plugin_id, params] : preferences.all_plugin_params()) {
        cJSON* obj = cJSON_AddObjectToObject(root, plugin_id.c_str());
        for (const auto& [key, value] : params) {
            cJSON_AddStringToObject(obj, key.c_str(), value.c_str());
        }
    }
    return root;
}

// Applies flat device fields from JSON.
void apply_device_settings(Preferences& preferences, Renderer& renderer, cJSON* json,
                           bool& reboot_required, bool& hostname_changed) {
    auto& d = preferences.device();
    reboot_required = false;
    hostname_changed = false;

    if (const cJSON* v = cJSON_GetObjectItem(json, "brightness"); cJSON_IsNumber(v)) {
        d.brightness = static_cast<Brightness>(std::clamp(v->valueint, 0, 255));
        renderer.set_brightness(d.brightness);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "gamma"); cJSON_IsNumber(v)) {
        d.gamma = static_cast<float>(v->valuedouble);
        renderer.set_gamma(d.gamma);
    }
    bool balance_touched = false;
    if (const cJSON* v = cJSON_GetObjectItem(json, "balance_r"); cJSON_IsNumber(v)) {
        d.balance_r = static_cast<std::uint8_t>(std::clamp(v->valueint, 0, 255));
        balance_touched = true;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "balance_g"); cJSON_IsNumber(v)) {
        d.balance_g = static_cast<std::uint8_t>(std::clamp(v->valueint, 0, 255));
        balance_touched = true;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "balance_b"); cJSON_IsNumber(v)) {
        d.balance_b = static_cast<std::uint8_t>(std::clamp(v->valueint, 0, 255));
        balance_touched = true;
    }
    if (balance_touched) {
        renderer.set_channel_balance(d.balance_r, d.balance_g, d.balance_b);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "power_limit_ma"); cJSON_IsNumber(v)) {
        d.power_limit_ma = static_cast<std::uint16_t>(v->valueint);
        renderer.set_power_limit_ma(d.power_limit_ma);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "led_count"); cJSON_IsNumber(v)) {
        const auto next = static_cast<LedIndex>(std::clamp(v->valueint, 1, 2000));
        if (next != d.led_count) {
            d.led_count = next;
            reboot_required = true;
        }
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "gpio"); cJSON_IsNumber(v)) {
        if (v->valueint != d.gpio) {
            d.gpio = v->valueint;
            reboot_required = true;
        }
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "chipset"); cJSON_IsNumber(v)) {
        const auto next = static_cast<Chipset>(std::clamp(v->valueint, 0, 4));
        if (next != d.chipset) {
            d.chipset = next;
            renderer.set_chipset(next);
            reboot_required = true;
        }
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "color_order"); cJSON_IsNumber(v)) {
        const auto next = static_cast<ColorOrder>(std::clamp(v->valueint, 0, 5));
        d.color_order = next;
        renderer.set_color_order(next);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "white_algorithm"); cJSON_IsNumber(v)) {
        d.white_algorithm = static_cast<WhiteAlgorithm>(std::clamp(v->valueint, 0, 0));
        renderer.set_white_algorithm(d.white_algorithm);
    }
    if (const cJSON* layout = cJSON_GetObjectItem(json, "layout"); cJSON_IsObject(layout)) {
        if (const cJSON* v = cJSON_GetObjectItem(layout, "top"); cJSON_IsNumber(v)) {
            d.layout.top = static_cast<std::uint16_t>(std::max(0, v->valueint));
        }
        if (const cJSON* v = cJSON_GetObjectItem(layout, "right"); cJSON_IsNumber(v)) {
            d.layout.right = static_cast<std::uint16_t>(std::max(0, v->valueint));
        }
        if (const cJSON* v = cJSON_GetObjectItem(layout, "bottom"); cJSON_IsNumber(v)) {
            d.layout.bottom = static_cast<std::uint16_t>(std::max(0, v->valueint));
        }
        if (const cJSON* v = cJSON_GetObjectItem(layout, "left"); cJSON_IsNumber(v)) {
            d.layout.left = static_cast<std::uint16_t>(std::max(0, v->valueint));
        }
        if (d.layout.total() != d.led_count) {
            d.normalize_layout();
        }
    } else if (cJSON_GetObjectItem(json, "led_count") != nullptr && d.layout.total() != d.led_count) {
        d.normalize_layout();
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
    if (const cJSON* v = cJSON_GetObjectItem(json, "last_used_plugin"); cJSON_IsString(v)) {
        d.last_used_plugin = v->valuestring;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "hostname"); cJSON_IsString(v)) {
        if (d.hostname != v->valuestring) {
            d.hostname = v->valuestring;
            hostname_changed = true;
        }
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "wifi_ssid"); cJSON_IsString(v)) {
        d.wifi_ssid = v->valuestring;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "wifi_password"); cJSON_IsString(v)) {
        d.wifi_password = v->valuestring;
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
    if (const cJSON* v = cJSON_GetObjectItem(json, "wifi_dns1"); cJSON_IsString(v)) {
        d.wifi_dns1 = v->valuestring;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "wifi_dns2"); cJSON_IsString(v)) {
        d.wifi_dns2 = v->valuestring;
    }

    if (const cJSON* edge = cJSON_GetObjectItem(json, "edge_ignore"); cJSON_IsObject(edge)) {
        if (const cJSON* v = cJSON_GetObjectItem(edge, "skip_start"); cJSON_IsNumber(v)) {
            d.edge_ignore.skip_start = static_cast<std::uint16_t>(std::max(0, v->valueint));
        }
        if (const cJSON* v = cJSON_GetObjectItem(edge, "skip_end"); cJSON_IsNumber(v)) {
            d.edge_ignore.skip_end = static_cast<std::uint16_t>(std::max(0, v->valueint));
        }
        if (const cJSON* v = cJSON_GetObjectItem(edge, "corner_tr"); cJSON_IsNumber(v)) {
            d.edge_ignore.corner_tr = static_cast<std::uint16_t>(std::max(0, v->valueint));
        }
        if (const cJSON* v = cJSON_GetObjectItem(edge, "corner_br"); cJSON_IsNumber(v)) {
            d.edge_ignore.corner_br = static_cast<std::uint16_t>(std::max(0, v->valueint));
        }
        if (const cJSON* v = cJSON_GetObjectItem(edge, "corner_bl"); cJSON_IsNumber(v)) {
            d.edge_ignore.corner_bl = static_cast<std::uint16_t>(std::max(0, v->valueint));
        }
        if (const cJSON* v = cJSON_GetObjectItem(edge, "corner_tl"); cJSON_IsNumber(v)) {
            d.edge_ignore.corner_tl = static_cast<std::uint16_t>(std::max(0, v->valueint));
        }
    }

    bool ignore_touched = false;
    if (const cJSON* arr = cJSON_GetObjectItem(json, "ignored_leds"); cJSON_IsArray(arr)) {
        d.ignored_leds.clear();
        const int n = cJSON_GetArraySize(arr);
        d.ignored_leds.reserve(static_cast<std::size_t>(std::max(0, n)));
        for (int i = 0; i < n; ++i) {
            const cJSON* item = cJSON_GetArrayItem(arr, i);
            if (cJSON_IsNumber(item) && item->valueint >= 0) {
                d.ignored_leds.push_back(static_cast<std::uint16_t>(item->valueint));
            }
        }
        ignore_touched = true;
    }

    // Rebuild ignore list from edge params (replaces ignored_leds).
    if (const cJSON* v = cJSON_GetObjectItem(json, "mark_edges"); cJSON_IsTrue(v)) {
        d.ignored_leds = edge_ignore_indices(d.led_count, d.layout.top, d.layout.right,
                                             d.layout.bottom, d.layout.left, d.edge_ignore);
        ignore_touched = true;
    }

    if (ignore_touched) {
        d.normalize_ignored_leds();
        renderer.set_ignored_leds(d.ignored_leds);
    }
}

void apply_plugin_params_json(Preferences& preferences, cJSON* plugins) {
    if (!cJSON_IsObject(plugins)) {
        return;
    }
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> next;
    for (cJSON* plugin = plugins->child; plugin != nullptr; plugin = plugin->next) {
        if (!cJSON_IsObject(plugin) || plugin->string == nullptr) {
            continue;
        }
        auto& params = next[plugin->string];
        for (cJSON* item = plugin->child; item != nullptr; item = item->next) {
            if (item->string == nullptr) {
                continue;
            }
            if (cJSON_IsString(item)) {
                params[item->string] = item->valuestring;
            } else if (cJSON_IsNumber(item)) {
                params[item->string] = std::to_string(item->valuedouble);
            } else if (cJSON_IsBool(item)) {
                params[item->string] = cJSON_IsTrue(item) ? "1" : "0";
            }
        }
    }
    preferences.replace_all_plugin_params(std::move(next));
}

esp_err_t RestApi::get_settings(httpd_req_t* req) {
    auto* self = from_req(req);
    cJSON* root = device_settings_to_json(self->preferences_.device(), false);
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
    bool reboot_required = false;
    bool hostname_changed = false;
    apply_device_settings(self->preferences_, self->renderer_, json, reboot_required,
                          hostname_changed);
    self->preferences_.save();
    if (hostname_changed) {
        self->wifi_.apply_hostname();
        self->wifi_.start_mdns();
    }
    cJSON_Delete(json);
    if (reboot_required) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":true,\"reboot\":true}");
        vTaskDelay(pdMS_TO_TICKS(250));
        esp_restart();
    }
    return send_json(req, "{\"ok\":true}");
}

esp_err_t RestApi::get_config(httpd_req_t* req) {
    auto* self = from_req(req);
    bool include_secrets = false;
    char query[96];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[16];
        if (httpd_query_key_value(query, "secrets", val, sizeof(val)) == ESP_OK) {
            include_secrets = (std::strcmp(val, "1") == 0 || std::strcmp(val, "true") == 0);
        }
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "schema", "lumosos.config.v1");
    cJSON_AddStringToObject(root, "app", kAppName.data());
    cJSON_AddStringToObject(root, "version", kAppVersion.data());
    cJSON_AddStringToObject(root, "api", kApiVersion.data());
    cJSON_AddItemToObject(root, "device",
                          device_settings_to_json(self->preferences_.device(), include_secrets));
    cJSON_AddItemToObject(root, "plugins", plugin_params_to_json(self->preferences_));

    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"lumosos-config.json\"");
    esp_err_t err = httpd_resp_send(req, printed, printed != nullptr ? HTTPD_RESP_USE_STRLEN : 0);
    cJSON_free(printed);
    return err;
}

esp_err_t RestApi::post_config(httpd_req_t* req) {
    auto* self = from_req(req);
    std::string body;
    if (read_body(req, body) != ESP_OK) {
        return send_json(req, "{\"error\":\"bad body\"}", 400);
    }
    cJSON* json = cJSON_Parse(body.c_str());
    if (json == nullptr) {
        return send_json(req, "{\"error\":\"invalid json\"}", 400);
    }

    // Accept wrapped {schema,device,plugins} or a flat settings object.
    cJSON* device = cJSON_GetObjectItem(json, "device");
    if (!cJSON_IsObject(device)) {
        if (cJSON_GetObjectItem(json, "led_count") != nullptr ||
            cJSON_GetObjectItem(json, "chipset") != nullptr ||
            cJSON_GetObjectItem(json, "balance_g") != nullptr) {
            device = json;
        } else {
            cJSON_Delete(json);
            return send_json(req, "{\"error\":\"missing device object\"}", 400);
        }
    }

    const cJSON* schema = cJSON_GetObjectItem(json, "schema");
    if (cJSON_IsString(schema) && std::strcmp(schema->valuestring, "lumosos.config.v1") != 0) {
        cJSON_Delete(json);
        return send_json(req, "{\"error\":\"unsupported config schema\"}", 400);
    }

    bool clear_static_ip = false;
    if (const cJSON* v = cJSON_GetObjectItem(json, "clear_static_ip"); cJSON_IsBool(v)) {
        clear_static_ip = cJSON_IsTrue(v);
    }

    bool reboot_required = false;
    bool hostname_changed = false;
    apply_device_settings(self->preferences_, self->renderer_, device, reboot_required,
                          hostname_changed);
    if (clear_static_ip) {
        auto& d = self->preferences_.device();
        d.wifi_use_static = false;
        d.wifi_ip.clear();
    }

    if (const cJSON* plugins = cJSON_GetObjectItem(json, "plugins"); cJSON_IsObject(plugins)) {
        apply_plugin_params_json(self->preferences_, const_cast<cJSON*>(plugins));
    }

    self->preferences_.save();
    if (hostname_changed) {
        self->wifi_.apply_hostname();
        self->wifi_.start_mdns();
    }
    cJSON_Delete(json);

    // Strip/GPIO/chipset size is fixed at boot — reboot so a clone comes up identically.
    if (reboot_required) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":true,\"reboot\":true}");
        vTaskDelay(pdMS_TO_TICKS(250));
        esp_restart();
    }
    return send_json(req, "{\"ok\":true,\"reboot\":false}");
}

esp_err_t RestApi::get_neighbors(httpd_req_t* req) {
    auto* self = from_req(req);
    const auto json = neighbors_to_json(self->wifi_.neighbors());
    return send_json(req, json.c_str());
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
    cJSON_AddNumberToObject(root, "color_order", static_cast<int>(self->renderer_.color_order()));
    cJSON_AddNumberToObject(root, "power_scale", self->renderer_.last_power_scale());
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
    cJSON_Delete(root);
    esp_err_t err = send_json(req, printed);
    cJSON_free(printed);
    return err;
}

esp_err_t RestApi::get_leds(httpd_req_t* req) {
    auto* self = from_req(req);
    const auto json = build_leds_json(self->framebuffer_);
    return send_json(req, json.c_str());
}

namespace {

cJSON* build_wled_root(Preferences& preferences, PluginManager& plugins, WifiService& wifi,
                       bool wled_on, bool wled_live) {
    // HyperHDR Hyperk reads /json then PUTs /json/state before streaming DDP.
    const auto& d = preferences.device();
    const auto st = wifi.status();
    cJSON* root = cJSON_CreateObject();
    cJSON* state = cJSON_AddObjectToObject(root, "state");
    cJSON_AddBoolToObject(state, "on", wled_on && plugins.active_id() != "off");
    cJSON_AddBoolToObject(state, "live", wled_live);
    cJSON_AddNumberToObject(state, "bri", d.brightness);

    cJSON* info = cJSON_AddObjectToObject(root, "info");
    cJSON_AddStringToObject(info, "ver", kAppVersion.data());
    cJSON_AddStringToObject(info, "cn", kAppVersion.data());
    cJSON_AddStringToObject(info, "name", "LumosOS");
    cJSON_AddStringToObject(info, "brand", "LumosOS");
    cJSON_AddStringToObject(info, "product", "LumosOS");
    cJSON_AddStringToObject(info, "arch", "esp32");
    cJSON_AddNumberToObject(info, "uptime",
                            static_cast<double>(esp_timer_get_time() / 1000000ULL));
    cJSON* leds = cJSON_AddObjectToObject(info, "leds");
    cJSON_AddNumberToObject(leds, "count", d.led_count);
    cJSON_AddNumberToObject(leds, "maxpwr", 0);
    cJSON* wifi_obj = cJSON_AddObjectToObject(info, "wifi");
    const int signal = st.rssi == 0 ? 90 : std::clamp(2 * (st.rssi + 100), 0, 100);
    cJSON_AddNumberToObject(wifi_obj, "signal", signal);
    cJSON_AddNumberToObject(wifi_obj, "channel", 0);
    cJSON_AddStringToObject(wifi_obj, "bssid", "");
    return root;
}

} // namespace

esp_err_t RestApi::get_wled_json(httpd_req_t* req) {
    auto* self = from_req(req);
    cJSON* root =
        build_wled_root(self->preferences_, self->plugins_, self->wifi_, self->wled_on_, self->wled_live_);
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    esp_err_t err = send_json(req, printed);
    cJSON_free(printed);
    return err;
}

esp_err_t RestApi::put_wled_state(httpd_req_t* req) {
    // Accept Hyperk powerOn/powerOff: PUT /json/state {"on":true,"live":true,"bri":255}
    auto* self = from_req(req);
    std::string body;
    if (read_body(req, body) != ESP_OK) {
        return send_json(req, "{\"error\":true}", 400);
    }
    cJSON* json = cJSON_Parse(body.c_str());
    if (json == nullptr) {
        return send_json(req, "{\"error\":true}", 400);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "on"); cJSON_IsBool(v)) {
        self->wled_on_ = cJSON_IsTrue(v);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "live"); cJSON_IsBool(v)) {
        self->wled_live_ = cJSON_IsTrue(v);
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "bri"); cJSON_IsNumber(v)) {
        auto& d = self->preferences_.device();
        d.brightness = static_cast<Brightness>(std::clamp(v->valueint, 0, 255));
        self->renderer_.set_brightness(d.brightness);
        self->preferences_.save();
    }
    cJSON_Delete(json);

    cJSON* root =
        build_wled_root(self->preferences_, self->plugins_, self->wifi_, self->wled_on_, self->wled_live_);
    cJSON* state = cJSON_GetObjectItem(root, "state");
    char* printed = cJSON_PrintUnformatted(state != nullptr ? state : root);
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
    if (const cJSON* v = cJSON_GetObjectItem(json, "dns1"); cJSON_IsString(v)) {
        d.wifi_dns1 = v->valuestring;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "dns2"); cJSON_IsString(v)) {
        d.wifi_dns2 = v->valuestring;
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
        {.uri = "/api/v1/config", .method = HTTP_GET, .handler = get_config, .user_ctx = this},
        {.uri = "/api/v1/config", .method = HTTP_POST, .handler = post_config, .user_ctx = this},
        {.uri = "/api/v1/status", .method = HTTP_GET, .handler = get_status, .user_ctx = this},
        {.uri = "/api/v1/leds", .method = HTTP_GET, .handler = get_leds, .user_ctx = this},
        {.uri = "/api/v1/wifi/scan", .method = HTTP_GET, .handler = get_wifi_scan, .user_ctx = this},
        {.uri = "/api/v1/wifi", .method = HTTP_POST, .handler = post_wifi, .user_ctx = this},
        {.uri = "/api/v1/neighbors", .method = HTTP_GET, .handler = get_neighbors, .user_ctx = this},
        {.uri = "/json", .method = HTTP_GET, .handler = get_wled_json, .user_ctx = this},
        {.uri = "/json/state", .method = HTTP_GET, .handler = get_wled_json, .user_ctx = this},
        {.uri = "/json/state", .method = HTTP_PUT, .handler = put_wled_state, .user_ctx = this},
        {.uri = "/json/state", .method = HTTP_POST, .handler = put_wled_state, .user_ctx = this},
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

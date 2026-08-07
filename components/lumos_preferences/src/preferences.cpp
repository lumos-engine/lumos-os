#include "lumos/preferences/preferences.hpp"

#include <nvs.h>
#include <nvs_flash.h>

#include <cstdio>
#include <cstring>

namespace lumos {
namespace {

constexpr const char* kNamespace = "lumos";

Result<void> nvs_set_str_key(nvs_handle_t handle, const char* key, const std::string& value) {
    esp_err_t err = nvs_set_str(handle, key, value.c_str());
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "nvs_set_str failed");
    }
    return Result<void>::ok();
}

std::string nvs_get_str_key(nvs_handle_t handle, const char* key, const std::string& def = {}) {
    size_t required = 0;
    esp_err_t err = nvs_get_str(handle, key, nullptr, &required);
    if (err != ESP_OK || required == 0) {
        return def;
    }
    std::string out(required, '\0');
    err = nvs_get_str(handle, key, out.data(), &required);
    if (err != ESP_OK) {
        return def;
    }
    if (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return out;
}

} // namespace

void DeviceSettings::normalize_layout() {
    if (led_count == 0) {
        layout = {};
        return;
    }
    if (layout.total() == led_count) {
        return;
    }
    // Known presets.
    if (led_count == 140) {
        layout = {.top = 44, .right = 26, .bottom = 44, .left = 26};
        return;
    }
    if (led_count == 340) {
        layout = {.top = 144, .right = 26, .bottom = 144, .left = 26};
        return;
    }
    // 16:9 perimeter ratio 16+9+16+9 = 50.
    layout.top = static_cast<std::uint16_t>((led_count * 16 + 25) / 50);
    layout.right = static_cast<std::uint16_t>((led_count * 9 + 25) / 50);
    layout.bottom = static_cast<std::uint16_t>((led_count * 16 + 25) / 50);
    const int left = static_cast<int>(led_count) - layout.top - layout.right - layout.bottom;
    layout.left = static_cast<std::uint16_t>(left > 0 ? left : 0);
    if (layout.total() != led_count && layout.left < led_count) {
        layout.left = static_cast<std::uint16_t>(led_count - layout.top - layout.right - layout.bottom);
    }
}

Result<void> Preferences::init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Full NVS wipe also drops Matter fabrics / CHIP credentials.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "nvs_flash_init failed");
    }
    initialized_ = true;
    return load();
}

Result<void> Preferences::load() {
    if (!initialized_) {
        return Result<void>::fail(ErrorCode::NotInitialized, "preferences not initialized");
    }

    nvs_handle_t handle{};
    esp_err_t err = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return Result<void>::ok(); // defaults
    }
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "nvs_open failed");
    }

    auto get_u8 = [&](const char* key, std::uint8_t& out) {
        std::uint8_t v = out;
        if (nvs_get_u8(handle, key, &v) == ESP_OK) {
            out = v;
        }
    };
    auto get_u16 = [&](const char* key, std::uint16_t& out) {
        std::uint16_t v = out;
        if (nvs_get_u16(handle, key, &v) == ESP_OK) {
            out = v;
        }
    };
    auto get_u32 = [&](const char* key, std::uint32_t& out) {
        std::uint32_t v = out;
        if (nvs_get_u32(handle, key, &v) == ESP_OK) {
            out = v;
        }
    };
    auto get_i32 = [&](const char* key, int& out) {
        std::int32_t v = out;
        if (nvs_get_i32(handle, key, &v) == ESP_OK) {
            out = static_cast<int>(v);
        }
    };

    get_u16("led_count", device_.led_count);
    get_i32("gpio", device_.gpio);
    std::uint8_t chipset = static_cast<std::uint8_t>(device_.chipset);
    get_u8("chipset", chipset);
    device_.chipset = static_cast<Chipset>(chipset);
    std::uint8_t order = static_cast<std::uint8_t>(device_.color_order);
    get_u8("color_order", order);
    device_.color_order = static_cast<ColorOrder>(order);
    std::uint8_t white_algo = static_cast<std::uint8_t>(device_.white_algorithm);
    get_u8("white_algo", white_algo);
    device_.white_algorithm = static_cast<WhiteAlgorithm>(white_algo);
    get_u16("lay_top", device_.layout.top);
    get_u16("lay_right", device_.layout.right);
    get_u16("lay_bottom", device_.layout.bottom);
    get_u16("lay_left", device_.layout.left);
    device_.normalize_layout();
    get_u8("brightness", device_.brightness);
    get_u16("power_ma", device_.power_limit_ma);
    get_u32("hh_timeout", device_.hyperhdr_timeout_ms);

    std::uint8_t startup = static_cast<std::uint8_t>(device_.startup_plugin);
    get_u8("startup", startup);
    device_.startup_plugin = static_cast<StartupPluginMode>(startup);
    std::uint8_t fallback = static_cast<std::uint8_t>(device_.fallback_plugin);
    get_u8("fallback", fallback);
    device_.fallback_plugin = static_cast<FallbackPluginMode>(fallback);

    std::int32_t gamma_milli = static_cast<std::int32_t>(device_.gamma * 1000.0f);
    if (nvs_get_i32(handle, "gamma_m", &gamma_milli) == ESP_OK) {
        device_.gamma = static_cast<float>(gamma_milli) / 1000.0f;
    }

    device_.last_used_plugin = nvs_get_str_key(handle, "last_plugin", device_.last_used_plugin);
    device_.wifi_ssid = nvs_get_str_key(handle, "wifi_ssid", device_.wifi_ssid);
    device_.wifi_password = nvs_get_str_key(handle, "wifi_pass", device_.wifi_password);
    device_.hostname = nvs_get_str_key(handle, "hostname", device_.hostname);

    std::uint8_t use_static = device_.wifi_use_static ? 1 : 0;
    get_u8("wifi_static", use_static);
    device_.wifi_use_static = use_static != 0;
    device_.wifi_ip = nvs_get_str_key(handle, "wifi_ip", device_.wifi_ip);
    device_.wifi_gateway = nvs_get_str_key(handle, "wifi_gw", device_.wifi_gateway);
    device_.wifi_netmask = nvs_get_str_key(handle, "wifi_mask", device_.wifi_netmask);
    device_.wifi_dns1 = nvs_get_str_key(handle, "wifi_dns1", device_.wifi_dns1);
    device_.wifi_dns2 = nvs_get_str_key(handle, "wifi_dns2", device_.wifi_dns2);
    // Migrate older single-dns key if present and dns1 empty.
    if (device_.wifi_dns1.empty()) {
        device_.wifi_dns1 = nvs_get_str_key(handle, "wifi_dns", "");
    }

    nvs_close(handle);
    return load_plugin_blob();
}

Result<void> Preferences::save() {
    if (!initialized_) {
        return Result<void>::fail(ErrorCode::NotInitialized, "preferences not initialized");
    }

    nvs_handle_t handle{};
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "nvs_open failed");
    }

    device_.normalize_layout();
    nvs_set_u16(handle, "led_count", device_.led_count);
    nvs_set_i32(handle, "gpio", device_.gpio);
    nvs_set_u8(handle, "chipset", static_cast<std::uint8_t>(device_.chipset));
    nvs_set_u8(handle, "color_order", static_cast<std::uint8_t>(device_.color_order));
    nvs_set_u8(handle, "white_algo", static_cast<std::uint8_t>(device_.white_algorithm));
    nvs_set_u16(handle, "lay_top", device_.layout.top);
    nvs_set_u16(handle, "lay_right", device_.layout.right);
    nvs_set_u16(handle, "lay_bottom", device_.layout.bottom);
    nvs_set_u16(handle, "lay_left", device_.layout.left);
    nvs_set_u8(handle, "brightness", device_.brightness);
    nvs_set_u16(handle, "power_ma", device_.power_limit_ma);
    nvs_set_u32(handle, "hh_timeout", device_.hyperhdr_timeout_ms);
    nvs_set_u8(handle, "startup", static_cast<std::uint8_t>(device_.startup_plugin));
    nvs_set_u8(handle, "fallback", static_cast<std::uint8_t>(device_.fallback_plugin));
    nvs_set_i32(handle, "gamma_m", static_cast<std::int32_t>(device_.gamma * 1000.0f));
    nvs_set_str_key(handle, "last_plugin", device_.last_used_plugin);
    nvs_set_str_key(handle, "wifi_ssid", device_.wifi_ssid);
    nvs_set_str_key(handle, "wifi_pass", device_.wifi_password);
    nvs_set_str_key(handle, "hostname", device_.hostname);
    nvs_set_u8(handle, "wifi_static", device_.wifi_use_static ? 1 : 0);
    nvs_set_str_key(handle, "wifi_ip", device_.wifi_ip);
    nvs_set_str_key(handle, "wifi_gw", device_.wifi_gateway);
    nvs_set_str_key(handle, "wifi_mask", device_.wifi_netmask);
    nvs_set_str_key(handle, "wifi_dns1", device_.wifi_dns1);
    nvs_set_str_key(handle, "wifi_dns2", device_.wifi_dns2);

    err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "nvs_commit failed");
    }
    return save_plugin_blob();
}

Result<void> Preferences::load_plugin_blob() {
    nvs_handle_t handle{};
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return Result<void>::ok();
    }

    size_t len = 0;
    esp_err_t err = nvs_get_blob(handle, "plug_params", nullptr, &len);
    if (err != ESP_OK || len == 0) {
        nvs_close(handle);
        return Result<void>::ok();
    }

    std::string blob(len, '\0');
    err = nvs_get_blob(handle, "plug_params", blob.data(), &len);
    nvs_close(handle);
    if (err != ESP_OK) {
        return Result<void>::ok();
    }

    // Format: plugin_id\0key\0value\0 ... repeated, terminated by empty plugin_id
    plugin_params_.clear();
    std::size_t i = 0;
    while (i < blob.size()) {
        const char* plugin = blob.c_str() + i;
        const std::size_t plugin_len = std::strlen(plugin);
        i += plugin_len + 1;
        if (plugin_len == 0) {
            break;
        }
        if (i >= blob.size()) {
            break;
        }
        const char* key = blob.c_str() + i;
        const std::size_t key_len = std::strlen(key);
        i += key_len + 1;
        if (i >= blob.size()) {
            break;
        }
        const char* value = blob.c_str() + i;
        const std::size_t value_len = std::strlen(value);
        i += value_len + 1;
        plugin_params_[plugin][key] = value;
    }
    return Result<void>::ok();
}

Result<void> Preferences::save_plugin_blob() {
    std::string blob;
    for (const auto& [plugin_id, params] : plugin_params_) {
        for (const auto& [key, value] : params) {
            blob.append(plugin_id);
            blob.push_back('\0');
            blob.append(key);
            blob.push_back('\0');
            blob.append(value);
            blob.push_back('\0');
        }
    }
    blob.push_back('\0');

    nvs_handle_t handle{};
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "nvs_open failed");
    }
    err = nvs_set_blob(handle, "plug_params", blob.data(), blob.size());
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "nvs_set_blob failed");
    }
    return Result<void>::ok();
}

std::unordered_map<std::string, std::string>& Preferences::plugin_params(
    const std::string& plugin_id) {
    return plugin_params_[plugin_id];
}

const std::unordered_map<std::string, std::string>& Preferences::plugin_params(
    const std::string& plugin_id) const {
    static const std::unordered_map<std::string, std::string> kEmpty;
    auto it = plugin_params_.find(plugin_id);
    if (it == plugin_params_.end()) {
        return kEmpty;
    }
    return it->second;
}

void Preferences::set_plugin_param(const std::string& plugin_id, const std::string& key,
                                   const std::string& value) {
    plugin_params_[plugin_id][key] = value;
}

std::string Preferences::get_plugin_param(const std::string& plugin_id, const std::string& key,
                                          const std::string& default_value) const {
    auto pit = plugin_params_.find(plugin_id);
    if (pit == plugin_params_.end()) {
        return default_value;
    }
    auto kit = pit->second.find(key);
    if (kit == pit->second.end()) {
        return default_value;
    }
    return kit->second;
}

} // namespace lumos

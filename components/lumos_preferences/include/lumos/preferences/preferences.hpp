#pragma once

#include "lumos/core/color.hpp"
#include "lumos/core/mode_map.hpp"
#include "lumos/core/result.hpp"
#include "lumos/core/types.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace lumos {

struct DeviceSettings {
    LedIndex led_count{kDefaultLedCount};
    int gpio{kDefaultLedGpio};
    Chipset chipset{Chipset::Ws2815};
    ColorOrder color_order{ColorOrder::Grb};
    Brightness brightness{kDefaultBrightness};
    float gamma{kDefaultGamma};
    std::uint16_t power_limit_ma{kDefaultPowerLimitMa};
    StartupPluginMode startup_plugin{StartupPluginMode::HyperHdr};
    FallbackPluginMode fallback_plugin{FallbackPluginMode::Bias};
    Milliseconds hyperhdr_timeout_ms{kDefaultHyperHdrTimeoutMs};
    std::string last_used_plugin{"hyperhdr"};
    std::string wifi_ssid;
    std::string wifi_password;
    std::string hostname{"lumosos"};

    // Static IPv4 (when wifi_use_static is true). Empty strings keep defaults on apply.
    bool wifi_use_static{false};
    std::string wifi_ip;       // e.g. 192.168.1.50
    std::string wifi_gateway;  // e.g. 192.168.1.1
    std::string wifi_netmask{"255.255.255.0"};
    std::string wifi_dns;      // optional; defaults to gateway if empty
};

class Preferences {
public:
    Result<void> init();
    Result<void> load();
    Result<void> save();

    DeviceSettings& device() { return device_; }
    const DeviceSettings& device() const { return device_; }

    // Plugin params stored as string key/value maps under plugin id.
    std::unordered_map<std::string, std::string>& plugin_params(const std::string& plugin_id);
    const std::unordered_map<std::string, std::string>& plugin_params(
        const std::string& plugin_id) const;

    void set_plugin_param(const std::string& plugin_id, const std::string& key,
                          const std::string& value);
    std::string get_plugin_param(const std::string& plugin_id, const std::string& key,
                                 const std::string& default_value = {}) const;

    static const char* startup_mode_to_plugin_id(StartupPluginMode mode,
                                                 const std::string& last_used) {
        return lumos::startup_mode_to_plugin_id(mode, last_used);
    }
    static const char* fallback_mode_to_plugin_id(FallbackPluginMode mode,
                                                  const std::string& last_used) {
        return lumos::fallback_mode_to_plugin_id(mode, last_used);
    }

private:
    Result<void> load_plugin_blob();
    Result<void> save_plugin_blob();

    DeviceSettings device_{};
    mutable std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
        plugin_params_;
    bool initialized_{false};
};

} // namespace lumos

#pragma once

#include "lumos/core/result.hpp"
#include "lumos/plugin/plugin_manager.hpp"
#include "lumos/preferences/preferences.hpp"
#include "lumos/renderer/renderer.hpp"

#include <cstdint>
#include <string>

namespace lumos {

struct MatterPairingInfo {
    bool enabled{false};
    bool commissioned{false};
    std::string manual_code;
    std::string qr_payload;
    std::uint16_t discriminator{0};
    std::uint32_t passcode{0};
};

// Bridges ESP-Matter extended color light → LumosOS plugins/brightness.
class MatterService {
public:
    static MatterService& instance();

    Result<void> start(Preferences& preferences, PluginManager& plugins, Renderer& renderer);
    MatterPairingInfo pairing_info() const;
    void factory_reset();

    // Also clears Matter fabrics when erasing device NVS.
    static void clear_matter_nvs();

    std::uint16_t endpoint_id() const { return light_endpoint_id_; }
    bool bridge_ready() const { return bridge_ready_; }

    void handle_on_off(bool on);
    void handle_level(std::uint8_t matter_level);
    void handle_hue(std::uint8_t hue);
    void handle_saturation(std::uint8_t sat);
    void handle_color_temp(std::uint16_t mireds);
    void handle_color_mode(std::uint8_t mode);

private:
    MatterService() = default;

    void apply_on_color();

    Preferences* preferences_{nullptr};
    PluginManager* plugins_{nullptr};
    Renderer* renderer_{nullptr};
    bool started_{false};
    // Ignore Matter attribute traffic until endpoint init finishes (avoids clobbering prefs).
    bool bridge_ready_{false};
    std::uint16_t light_endpoint_id_{0};

    bool on_{true};
    std::uint8_t hue_{0};
    std::uint8_t sat_{254};
    std::uint16_t mireds_{370};
    std::uint8_t color_mode_{0}; // HSV by default after start
};

} // namespace lumos

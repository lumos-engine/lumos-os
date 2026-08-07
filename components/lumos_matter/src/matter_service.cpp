#include "lumos/matter/matter_service.hpp"
#include "lumos/matter/color_map.hpp"
#include "lumos/core/logger.hpp"

#include <esp_matter.h>
#include <nvs_flash.h>
#include <platform/CHIPDeviceLayer.h>

#include <app/server/OnboardingCodesUtil.h>
#include <app/server/Server.h>
#include <lib/support/Span.h>
#include <setup_payload/ManualSetupPayloadGenerator.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>

#include <string>

namespace lumos {

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

namespace {

Logger log{"matter"};

constexpr std::uint32_t kDefaultPasscode = 20202021;
constexpr std::uint16_t kDefaultDiscriminator = 3840;

MatterService* g_svc = nullptr;

esp_err_t attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                              uint32_t attribute_id, esp_matter_attr_val_t* val, void* /*priv*/) {
    if (type != PRE_UPDATE || g_svc == nullptr || val == nullptr || !g_svc->bridge_ready()) {
        return ESP_OK;
    }
    if (endpoint_id != g_svc->endpoint_id()) {
        return ESP_OK;
    }

    if (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
        g_svc->handle_on_off(val->val.b);
    } else if (cluster_id == LevelControl::Id &&
               attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
        g_svc->handle_level(val->val.u8);
    } else if (cluster_id == ColorControl::Id) {
        if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
            g_svc->handle_hue(val->val.u8);
        } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
            g_svc->handle_saturation(val->val.u8);
        } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
            g_svc->handle_color_temp(val->val.u16);
        } else if (attribute_id == ColorControl::Attributes::ColorMode::Id) {
            g_svc->handle_color_mode(val->val.u8);
        }
    }
    return ESP_OK;
}

esp_err_t identification_cb(identification::callback_type_t /*type*/, uint16_t /*endpoint_id*/,
                            uint8_t /*effect_id*/, uint8_t /*effect_variant*/, void* /*priv*/) {
    return ESP_OK;
}

void app_event_cb(const ChipDeviceEvent* event, intptr_t /*arg*/) {
    if (event == nullptr) {
        return;
    }
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        log.info("Matter commissioning complete");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        log.info("Matter commissioning window opened");
        break;
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        log.info("Matter fabric removed");
        break;
    default:
        break;
    }
}

} // namespace

MatterService& MatterService::instance() {
    static MatterService svc;
    return svc;
}

void MatterService::apply_on_color() {
    if (preferences_ == nullptr || plugins_ == nullptr) {
        return;
    }
    if (!on_) {
        plugins_->activate("off");
        return;
    }

    if (color_mode_ == static_cast<std::uint8_t>(ColorControl::ColorMode::kColorTemperature)) {
        const int bias_t = matter_map::mireds_to_bias_temperature(mireds_);
        preferences_->set_plugin_param("bias", "temperature", std::to_string(bias_t));
        preferences_->set_plugin_param("bias", "intensity", "100");
        preferences_->save();
        plugins_->activate("bias");
        return;
    }

    const auto rgb = matter_map::matter_hsv_to_rgb(hue_, sat_);
    preferences_->set_plugin_param("static", "r", std::to_string(rgb.r));
    preferences_->set_plugin_param("static", "g", std::to_string(rgb.g));
    preferences_->set_plugin_param("static", "b", std::to_string(rgb.b));
    preferences_->save();
    plugins_->activate("static");
}

void MatterService::handle_on_off(bool on) {
    on_ = on;
    apply_on_color();
}

void MatterService::handle_level(std::uint8_t matter_level) {
    if (preferences_ == nullptr || renderer_ == nullptr) {
        return;
    }
    const auto brightness = matter_map::matter_level_to_brightness(matter_level);
    preferences_->device().brightness = brightness;
    renderer_->set_brightness(brightness);
    preferences_->save();
}

void MatterService::handle_hue(std::uint8_t hue) {
    hue_ = hue;
    color_mode_ =
        static_cast<std::uint8_t>(ColorControl::ColorMode::kCurrentHueAndCurrentSaturation);
    if (on_) {
        apply_on_color();
    }
}

void MatterService::handle_saturation(std::uint8_t sat) {
    sat_ = sat;
    color_mode_ =
        static_cast<std::uint8_t>(ColorControl::ColorMode::kCurrentHueAndCurrentSaturation);
    if (on_) {
        apply_on_color();
    }
}

void MatterService::handle_color_temp(std::uint16_t mireds) {
    mireds_ = mireds;
    color_mode_ = static_cast<std::uint8_t>(ColorControl::ColorMode::kColorTemperature);
    if (on_) {
        apply_on_color();
    }
}

void MatterService::handle_color_mode(std::uint8_t mode) {
    color_mode_ = mode;
}

Result<void> MatterService::start(Preferences& preferences, PluginManager& plugins, Renderer& renderer) {
    if (started_) {
        return Result<void>::ok();
    }
    preferences_ = &preferences;
    plugins_ = &plugins;
    renderer_ = &renderer;
    g_svc = this;
    bridge_ready_ = false;
    color_mode_ =
        static_cast<std::uint8_t>(ColorControl::ColorMode::kCurrentHueAndCurrentSaturation);

    node::config_t node_config;
    node_t* node = node::create(&node_config, attribute_update_cb, identification_cb);
    if (node == nullptr) {
        return Result<void>::fail(ErrorCode::Internal, "matter node::create failed");
    }

    extended_color_light::config_t light_config{};
    light_config.on_off.on_off = true;
    light_config.level_control.current_level =
        matter_map::brightness_to_matter_level(preferences.device().brightness);
    light_config.level_control.on_level = light_config.level_control.current_level;
    light_config.color_control.color_mode = color_mode_;
    light_config.color_control.enhanced_color_mode = color_mode_;

    endpoint_t* endpoint = extended_color_light::create(node, &light_config, ENDPOINT_FLAG_NONE, this);
    if (endpoint == nullptr) {
        return Result<void>::fail(ErrorCode::Internal, "matter extended_color_light::create failed");
    }
    light_endpoint_id_ = endpoint::get_id(endpoint);

    esp_err_t err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::Internal, "esp_matter::start failed");
    }

    // LumosOS owns STA/SoftAP; stop CHIP from calling esp_wifi_connect / SoftAP.
    auto wifi_mode = chip::DeviceLayer::ConnectivityMgr().SetWiFiStationMode(
        chip::DeviceLayer::ConnectivityManager::kWiFiStationMode_ApplicationControlled);
    if (wifi_mode != CHIP_NO_ERROR) {
        log.warn("Could not set Matter WiFi to application-controlled");
    }
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_AP
    (void)chip::DeviceLayer::ConnectivityMgr().SetWiFiAPMode(
        chip::DeviceLayer::ConnectivityManager::kWiFiAPMode_Disabled);
#endif

    started_ = true;
    bridge_ready_ = true;
    log.info("Matter color light endpoint %u", static_cast<unsigned>(light_endpoint_id_));
    return Result<void>::ok();
}

MatterPairingInfo MatterService::pairing_info() const {
    MatterPairingInfo info;
    info.enabled = started_;
    info.passcode = kDefaultPasscode;
    info.discriminator = kDefaultDiscriminator;

    if (!started_) {
        return info;
    }

    info.commissioned = chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;

    char qr[128]{};
    chip::MutableCharSpan qr_span(qr);
    if (GetQRCode(qr_span, chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE)) ==
        CHIP_NO_ERROR) {
        info.qr_payload.assign(qr_span.data(), qr_span.size());
    }

    char manual[32]{};
    chip::MutableCharSpan manual_span(manual);
    if (GetManualPairingCode(
            manual_span, chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE)) ==
        CHIP_NO_ERROR) {
        info.manual_code.assign(manual_span.data(), manual_span.size());
    }

    if (info.manual_code.empty()) {
        info.manual_code = "34970112332";
    }
    if (info.qr_payload.empty()) {
        info.qr_payload = "MT:Y.K9042C00KA0648G00";
    }
    return info;
}

void MatterService::factory_reset() {
    log.warn("Matter factory reset");
    clear_matter_nvs();
    esp_matter::factory_reset();
}

void MatterService::clear_matter_nvs() {
    static constexpr const char* kNamespaces[] = {
        "chip-factory",
        "chip-config",
        "chip-counters",
        "esp_matter",
    };
    for (const char* ns : kNamespaces) {
        nvs_handle_t handle = 0;
        if (nvs_open(ns, NVS_READWRITE, &handle) == ESP_OK) {
            nvs_erase_all(handle);
            nvs_commit(handle);
            nvs_close(handle);
        }
    }
}

} // namespace lumos

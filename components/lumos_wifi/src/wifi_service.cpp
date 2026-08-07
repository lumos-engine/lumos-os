#include "lumos/wifi/wifi_service.hpp"
#include "lumos/wifi/captive_dns.hpp"
#include "lumos/core/logger.hpp"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"

#include <cstring>
#include <memory>

namespace lumos {
namespace {

Logger log{"wifi"};
std::unique_ptr<CaptiveDns> g_captive_dns;
WifiService* g_instance = nullptr;

void event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    auto* self = static_cast<WifiService*>(arg);
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            log.warn("STA disconnected — retrying / falling back to AP");
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_AP_START) {
            log.info("SoftAP started");
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        self->on_got_ip();
    }
}

} // namespace

WifiService::WifiService(Preferences& preferences) : preferences_(preferences) {
    g_instance = this;
}

WifiService::~WifiService() {
    stop();
    if (g_instance == this) {
        g_instance = nullptr;
    }
}

void WifiService::ensure_netif() {
    static bool netif_inited = false;
    if (!netif_inited) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        netif_inited = true;
    }
    if (sta_netif_ == nullptr) {
        sta_netif_ = esp_netif_create_default_wifi_sta();
    }
    if (ap_netif_ == nullptr) {
        ap_netif_ = esp_netif_create_default_wifi_ap();
    }
}

Result<void> WifiService::start() {
    ensure_netif();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, this));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    started_ = true;

    if (!preferences_.device().wifi_ssid.empty()) {
        auto result = start_sta_from_prefs();
        if (result) {
            return result;
        }
        log.warn("STA connect setup failed — starting AP");
    }
    return start_ap();
}

Result<void> WifiService::start_sta_from_prefs() {
    return connect_sta(preferences_.device().wifi_ssid, preferences_.device().wifi_password);
}

Result<void> WifiService::connect_sta(const std::string& ssid, const std::string& password) {
    ensure_netif();
    if (g_captive_dns) {
        g_captive_dns->stop();
        g_captive_dns.reset();
    }

    wifi_config_t wifi_config{};
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), ssid.c_str(),
                 sizeof(wifi_config.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.password), password.c_str(),
                 sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    status_.mode = WifiMode::Station;
    status_.ssid = ssid;
    status_.connected = false;
    preferences_.device().wifi_ssid = ssid;
    preferences_.device().wifi_password = password;
    preferences_.save();

    log.info("Connecting STA to %s", ssid.c_str());
    return Result<void>::ok();
}

Result<void> WifiService::start_ap(const std::string& ssid) {
    ensure_netif();

    wifi_config_t wifi_config{};
    std::strncpy(reinterpret_cast<char*>(wifi_config.ap.ssid), ssid.c_str(),
                 sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = static_cast<std::uint8_t>(ssid.size());
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info{};
    esp_netif_get_ip_info(static_cast<esp_netif_t*>(ap_netif_), &ip_info);

    char ip_str[16];
    esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
    status_.mode = WifiMode::AccessPoint;
    status_.connected = true;
    status_.ssid = ssid;
    status_.ip = ip_str;

    g_captive_dns = std::make_unique<CaptiveDns>();
    g_captive_dns->start(ip_info.ip.addr);
    captive_dns_running_ = true;

    log.info("AP %s at %s", ssid.c_str(), ip_str);
    if (status_cb_) {
        status_cb_(status_);
    }
    return Result<void>::ok();
}

void WifiService::on_got_ip() {
    esp_netif_ip_info_t ip_info{};
    esp_netif_get_ip_info(static_cast<esp_netif_t*>(sta_netif_), &ip_info);
    char ip_str[16];
    esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
    status_.connected = true;
    status_.mode = WifiMode::Station;
    status_.ip = ip_str;
    log.info("Got IP: %s", ip_str);
    start_mdns();
    if (status_cb_) {
        status_cb_(status_);
    }
}

Result<void> WifiService::start_mdns() {
    static bool mdns_started = false;
    if (!mdns_started) {
        esp_err_t err = mdns_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return Result<void>::fail(ErrorCode::NetworkError, "mdns_init failed");
        }
        mdns_started = true;
    }

    const auto& hostname = preferences_.device().hostname;
    mdns_hostname_set(hostname.c_str());
    mdns_instance_name_set("LumosOS");

    mdns_service_add("LumosOS", "_http", "_tcp", 80, nullptr, 0);
    mdns_service_add("LumosOS", "_lumosos", "_tcp", 80, nullptr, 0);

    // HyperHDR Hyperk discovery typically scans mDNS for Hyperk/WLED-like services.
    // Advertise a DDP-capable service so HyperHDR can locate the device.
    mdns_txt_item_t txt[] = {
        {"path", "/"},
        {"version", kAppVersion.data()},
        {"leds", "150"},
        {"proto", "ddp"},
    };
    mdns_service_add("LumosOS", "_hyperk", "_tcp", 80, txt, 4);
    mdns_service_add("LumosOS", "_wled", "_tcp", 80, txt, 4);

    log.info("mDNS started as %s.local", hostname.c_str());
    return Result<void>::ok();
}

WifiStatus WifiService::status() const {
    return status_;
}

void WifiService::stop() {
    if (g_captive_dns) {
        g_captive_dns->stop();
        g_captive_dns.reset();
    }
    if (started_) {
        esp_wifi_stop();
        started_ = false;
    }
}

} // namespace lumos

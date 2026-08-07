#include "lumos/wifi/wifi_service.hpp"
#include "lumos/wifi/captive_dns.hpp"
#include "lumos/core/logger.hpp"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <set>

namespace lumos {
namespace {

Logger log{"wifi"};
std::unique_ptr<CaptiveDns> g_captive_dns;

void event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    auto* self = static_cast<WifiService*>(arg);
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            self->on_sta_start();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            log.warn("STA disconnected");
            // Reconnect only when we intentionally joined a network.
            self->on_sta_start();
        } else if (id == WIFI_EVENT_AP_START) {
            log.info("SoftAP started");
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        self->on_got_ip();
    }
}

} // namespace

WifiService::WifiService(Preferences& preferences) : preferences_(preferences) {}

WifiService::~WifiService() {
    stop();
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

void WifiService::on_sta_start() {
    if (want_sta_connect_) {
        esp_wifi_connect();
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

Result<void> WifiService::apply_sta_ip_config() {
    auto* netif = static_cast<esp_netif_t*>(sta_netif_);
    if (netif == nullptr) {
        return Result<void>::fail(ErrorCode::NotInitialized, "sta netif missing");
    }

    const auto& d = preferences_.device();
    if (!d.wifi_use_static) {
        esp_err_t err = esp_netif_dhcpc_start(netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
            return Result<void>::fail(ErrorCode::NetworkError, "failed to start DHCP client");
        }
        status_.use_static = false;
        log.info("STA IP mode: DHCP");
        return Result<void>::ok();
    }

    if (d.wifi_ip.empty() || d.wifi_gateway.empty()) {
        return Result<void>::fail(ErrorCode::InvalidArgument,
                                  "static IP requires wifi_ip and wifi_gateway");
    }

    const std::string& mask = d.wifi_netmask.empty() ? "255.255.255.0" : d.wifi_netmask;
    const std::string& dns1 = d.wifi_dns1.empty() ? d.wifi_gateway : d.wifi_dns1;

    esp_netif_dhcpc_stop(netif);

    esp_netif_ip_info_t ip_info{};
    if (esp_netif_str_to_ip4(d.wifi_ip.c_str(), &ip_info.ip) != ESP_OK ||
        esp_netif_str_to_ip4(d.wifi_gateway.c_str(), &ip_info.gw) != ESP_OK ||
        esp_netif_str_to_ip4(mask.c_str(), &ip_info.netmask) != ESP_OK) {
        return Result<void>::fail(ErrorCode::InvalidArgument, "invalid static IP/gateway/netmask");
    }

    esp_err_t err = esp_netif_set_ip_info(netif, &ip_info);
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::NetworkError, "esp_netif_set_ip_info failed");
    }

    esp_netif_dns_info_t dns_main{};
    dns_main.ip.type = ESP_IPADDR_TYPE_V4;
    if (esp_netif_str_to_ip4(dns1.c_str(), &dns_main.ip.u_addr.ip4) != ESP_OK) {
        return Result<void>::fail(ErrorCode::InvalidArgument, "invalid DNS 1 address");
    }
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_main);

    if (!d.wifi_dns2.empty()) {
        esp_netif_dns_info_t dns_backup{};
        dns_backup.ip.type = ESP_IPADDR_TYPE_V4;
        if (esp_netif_str_to_ip4(d.wifi_dns2.c_str(), &dns_backup.ip.u_addr.ip4) != ESP_OK) {
            return Result<void>::fail(ErrorCode::InvalidArgument, "invalid DNS 2 address");
        }
        esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_backup);
        status_.dns2 = d.wifi_dns2;
    } else {
        status_.dns2.clear();
    }

    status_.use_static = true;
    status_.ip = d.wifi_ip;
    status_.gateway = d.wifi_gateway;
    status_.netmask = mask;
    status_.dns1 = dns1;
    log.info("STA IP mode: static %s gw %s dns1 %s", d.wifi_ip.c_str(), d.wifi_gateway.c_str(),
             dns1.c_str());
    return Result<void>::ok();
}

Result<void> WifiService::connect_sta(const std::string& ssid, const std::string& password) {
    ensure_netif();
    if (g_captive_dns) {
        g_captive_dns->stop();
        g_captive_dns.reset();
    }

    // Empty password keeps the previously saved one (UI "leave blank to keep").
    std::string effective_password = password;
    if (effective_password.empty() && !preferences_.device().wifi_password.empty() &&
        preferences_.device().wifi_ssid == ssid) {
        effective_password = preferences_.device().wifi_password;
    }

    wifi_config_t wifi_config{};
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), ssid.c_str(),
                 sizeof(wifi_config.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.password), effective_password.c_str(),
                 sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode =
        effective_password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    want_sta_connect_ = true;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    auto ip_result = apply_sta_ip_config();
    if (!ip_result) {
        log.error("IP config failed: %s", ip_result.error().message.c_str());
        return ip_result;
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();

    status_.mode = WifiMode::Station;
    status_.ssid = ssid;
    status_.connected = false;
    preferences_.device().wifi_ssid = ssid;
    preferences_.device().wifi_password = effective_password;
    preferences_.save();

    log.info("Connecting STA to %s", ssid.c_str());
    return Result<void>::ok();
}

Result<void> WifiService::start_ap(const std::string& ssid) {
    ensure_netif();
    want_sta_connect_ = false;

    wifi_config_t wifi_config{};
    std::strncpy(reinterpret_cast<char*>(wifi_config.ap.ssid), ssid.c_str(),
                 sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = static_cast<std::uint8_t>(ssid.size());
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    // APSTA so we can scan nearby networks while the captive portal is up.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
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

Result<std::vector<WifiNetwork>> WifiService::scan() {
    // Ensure STA interface exists for scanning (APSTA or STA).
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_AP) {
        esp_wifi_set_mode(WIFI_MODE_APSTA);
    } else if (mode == WIFI_MODE_NULL) {
        return Result<std::vector<WifiNetwork>>::fail(ErrorCode::NotInitialized, "wifi not started");
    }

    wifi_scan_config_t scan_config{};
    scan_config.show_hidden = false;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        return Result<std::vector<WifiNetwork>>::fail(ErrorCode::NetworkError, "wifi scan failed");
    }

    std::uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        return Result<std::vector<WifiNetwork>>::ok({});
    }
    if (ap_count > 40) {
        ap_count = 40;
    }

    std::vector<wifi_ap_record_t> records(ap_count);
    err = esp_wifi_scan_get_ap_records(&ap_count, records.data());
    if (err != ESP_OK) {
        return Result<std::vector<WifiNetwork>>::fail(ErrorCode::NetworkError, "get scan records failed");
    }

    std::vector<WifiNetwork> networks;
    networks.reserve(ap_count);
    std::set<std::string> seen;
    for (std::uint16_t i = 0; i < ap_count; ++i) {
        const char* ssid = reinterpret_cast<const char*>(records[i].ssid);
        if (ssid[0] == '\0') {
            continue;
        }
        std::string name(ssid);
        if (!seen.insert(name).second) {
            continue; // keep strongest (records are usually RSSI-sorted)
        }
        WifiNetwork n;
        n.ssid = std::move(name);
        n.rssi = records[i].rssi;
        n.channel = records[i].primary;
        n.secure = records[i].authmode != WIFI_AUTH_OPEN;
        networks.push_back(std::move(n));
    }

    std::sort(networks.begin(), networks.end(),
              [](const WifiNetwork& a, const WifiNetwork& b) { return a.rssi > b.rssi; });

    log.info("WiFi scan found %u networks", static_cast<unsigned>(networks.size()));
    return Result<std::vector<WifiNetwork>>::ok(std::move(networks));
}

void WifiService::on_got_ip() {
    esp_netif_ip_info_t ip_info{};
    esp_netif_get_ip_info(static_cast<esp_netif_t*>(sta_netif_), &ip_info);
    char ip_str[16];
    char gw_str[16];
    char mask_str[16];
    esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
    esp_ip4addr_ntoa(&ip_info.gw, gw_str, sizeof(gw_str));
    esp_ip4addr_ntoa(&ip_info.netmask, mask_str, sizeof(mask_str));
    status_.connected = true;
    status_.mode = WifiMode::Station;
    status_.ip = ip_str;
    status_.gateway = gw_str;
    status_.netmask = mask_str;
    status_.use_static = preferences_.device().wifi_use_static;

    esp_netif_dns_info_t dns_main{};
    if (esp_netif_get_dns_info(static_cast<esp_netif_t*>(sta_netif_), ESP_NETIF_DNS_MAIN,
                               &dns_main) == ESP_OK &&
        dns_main.ip.type == ESP_IPADDR_TYPE_V4) {
        char dns_str[16];
        esp_ip4addr_ntoa(&dns_main.ip.u_addr.ip4, dns_str, sizeof(dns_str));
        status_.dns1 = dns_str;
    }
    esp_netif_dns_info_t dns_backup{};
    if (esp_netif_get_dns_info(static_cast<esp_netif_t*>(sta_netif_), ESP_NETIF_DNS_BACKUP,
                               &dns_backup) == ESP_OK &&
        dns_backup.ip.type == ESP_IPADDR_TYPE_V4 && dns_backup.ip.u_addr.ip4.addr != 0) {
        char dns_str[16];
        esp_ip4addr_ntoa(&dns_backup.ip.u_addr.ip4, dns_str, sizeof(dns_str));
        status_.dns2 = dns_str;
    } else {
        status_.dns2.clear();
    }

    log.info("Got IP: %s (gw %s)", ip_str, gw_str);
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
    want_sta_connect_ = false;
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

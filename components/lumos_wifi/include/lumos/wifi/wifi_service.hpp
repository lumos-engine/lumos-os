#pragma once

#include "lumos/core/result.hpp"
#include "lumos/preferences/preferences.hpp"

#include <atomic>
#include <functional>
#include <string>

namespace lumos {

enum class WifiMode {
    Off,
    Station,
    AccessPoint,
};

struct WifiStatus {
    WifiMode mode{WifiMode::Off};
    bool connected{false};
    std::string ip;
    std::string ssid;
    int rssi{0};
};

class WifiService {
public:
    using StatusCallback = std::function<void(const WifiStatus&)>;

    explicit WifiService(Preferences& preferences);
    ~WifiService();

    Result<void> start();
    Result<void> connect_sta(const std::string& ssid, const std::string& password);
    Result<void> start_ap(const std::string& ssid = "LumosOS-Setup");
    void stop();

    WifiStatus status() const;
    void set_status_callback(StatusCallback cb) { status_cb_ = std::move(cb); }
    void on_got_ip();

    // mDNS: lumosos.local + HyperHDR/Hyperk-compatible discovery hints
    Result<void> start_mdns();

private:
    void ensure_netif();
    Result<void> start_sta_from_prefs();

    Preferences& preferences_;
    WifiStatus status_{};
    StatusCallback status_cb_;
    void* sta_netif_{nullptr};
    void* ap_netif_{nullptr};
    bool started_{false};
    std::atomic<bool> captive_dns_running_{false};
};

} // namespace lumos

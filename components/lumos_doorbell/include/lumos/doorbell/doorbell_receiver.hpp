#pragma once

#include "lumos/core/result.hpp"
#include "lumos/preferences/preferences.hpp"

#include "esp_now.h"
#include "esp_timer.h"

#include <cstdint>
#include <string>

namespace lumos {

struct DoorbellStatus {
    bool enabled{false};
    bool espnow_ready{false};
    bool paired{false};
    int relay_pin{kDefaultRelayGpio};
    bool active_high{true};
    std::uint16_t press_ms{400};
    std::string paired_tx_mac;
    std::uint32_t last_ring_ms{0}; // ms since boot; 0 = never
    std::uint8_t last_seq{0};
    bool relay_active{false};
};

class DoorbellReceiver {
public:
    explicit DoorbellReceiver(Preferences& preferences);

    Result<void> start();
    void apply_settings();
    void test_pulse();
    DoorbellStatus status() const;

private:
    static void recv_cb(const esp_now_recv_info_t* info, const std::uint8_t* data, int len);
    static void release_timer_cb(void* arg);

    void on_packet(const std::uint8_t mac[6], const std::uint8_t* data, int len);
    void pulse_relay();
    void set_relay(bool active);
    void configure_gpio();
    bool parse_paired_mac(std::uint8_t out[6]) const;
    static std::string format_mac(const std::uint8_t mac[6]);
    static bool mac_equal(const std::uint8_t a[6], const std::uint8_t b[6]);

    Preferences& preferences_;
    bool started_{false};
    bool espnow_ready_{false};
    esp_timer_handle_t release_timer_{nullptr};
    int configured_pin_{-1};
    bool relay_active_{false};
    bool have_last_seq_{false};
    std::uint8_t last_seq_{0};
    std::uint32_t last_ring_ms_{0};
    std::uint8_t paired_mac_[6]{};
    bool paired_valid_{false};

    static DoorbellReceiver* instance_;
};

} // namespace lumos

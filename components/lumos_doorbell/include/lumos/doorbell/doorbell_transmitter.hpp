#pragma once

#include "lumos/core/board_pins.hpp"
#include "lumos/core/result.hpp"

#include "esp_timer.h"

#include <cstdint>
#include <string>

namespace lumos {

struct DoorbellTxConfig {
    int opto_pin{kDefaultOptoGpio};
    bool active_low{true}; // typical optocoupler collector to GPIO, emitter to GND
    std::uint8_t channel{1};
    std::uint8_t rx_mac[6]{};
    bool rx_mac_valid{false};
    std::uint32_t tx_id{1};
};

class DoorbellTransmitter {
public:
    Result<void> start();
    void apply_config(const DoorbellTxConfig& cfg);
    void load_nvs();
    void save_nvs();
    void test_send();
    DoorbellTxConfig config() const { return cfg_; }
    std::uint8_t last_seq() const { return seq_; }
    std::uint32_t last_send_ms() const { return last_send_ms_; }
    bool espnow_ready() const { return espnow_ready_; }
    std::string own_mac() const;

private:
    static void gpio_isr(void* arg);
    static void debounce_timer_cb(void* arg);
    static void retry_timer_cb(void* arg);

    void maybe_fire();
    void send_press(bool bump_seq);
    void configure_gpio();
    void configure_wifi_channel();
    void add_peer();

    DoorbellTxConfig cfg_{};
    bool started_{false};
    bool espnow_ready_{false};
    bool gpio_isr_installed_{false};
    int configured_pin_{-1};
    std::uint8_t seq_{0};
    std::uint32_t last_send_ms_{0};
    int retries_left_{0};
    esp_timer_handle_t debounce_timer_{nullptr};
    esp_timer_handle_t retry_timer_{nullptr};

    static DoorbellTransmitter* instance_;
};

} // namespace lumos

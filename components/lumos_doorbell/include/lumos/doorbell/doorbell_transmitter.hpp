#pragma once

#include "lumos/core/board_pins.hpp"
#include "lumos/core/result.hpp"
#include "lumos/doorbell/doorbell_packet.hpp"

#include "esp_now.h"
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

struct DoorbellTxStatus {
    DoorbellTxConfig cfg{};
    std::string own_mac;
    bool espnow_ready{false};
    bool paired{false};
    std::uint8_t last_seq{0};
    std::uint32_t last_send_ms{0};
    bool pairing{false};
    bool scanning{false};
    std::uint32_t pairing_ms{0};
    int peer_count{0};
    DoorbellDiscoveredPeer peers[kDoorbellMaxPeers]{};
};

class DoorbellTransmitter {
public:
    Result<void> start();
    void apply_config(const DoorbellTxConfig& cfg);
    void load_nvs();
    void save_nvs();
    void test_send();
    DoorbellTxConfig config() const { return cfg_; }
    DoorbellTxStatus status() const;
    std::uint8_t last_seq() const { return seq_; }
    std::uint32_t last_send_ms() const { return last_send_ms_; }
    bool espnow_ready() const { return espnow_ready_; }
    std::string own_mac() const;

    void start_pairing(std::uint32_t duration_ms = kDoorbellPairDefaultMs);
    void stop_pairing();
    bool select_peer(const std::uint8_t mac[6]);

private:
    static void gpio_isr(void* arg);
    static void debounce_timer_cb(void* arg);
    static void retry_timer_cb(void* arg);
    static void hello_timer_cb(void* arg);
    static void recv_cb(const esp_now_recv_info_t* info, const std::uint8_t* data, int len);
    static void scan_task(void* arg);

    void maybe_fire();
    void send_press(bool bump_seq);
    void configure_gpio();
    void configure_wifi_channel();
    void add_peer();
    void ensure_broadcast_peer();
    void send_pair(std::uint8_t type, const std::uint8_t* dest);
    void on_packet(const std::uint8_t mac[6], const std::uint8_t* data, int len, int rssi);
    void note_peer(const std::uint8_t mac[6], const DoorbellPairHello& hello, int rssi);
    void own_mac_bytes(std::uint8_t out[6]) const;
    bool pairing_active() const;
    void set_ap_channel(std::uint8_t channel);

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
    esp_timer_handle_t hello_timer_{nullptr};
    std::uint64_t pairing_until_us_{0};
    bool scanning_{false};
    int peer_count_{0};
    DoorbellDiscoveredPeer peers_[kDoorbellMaxPeers]{};

    static DoorbellTransmitter* instance_;
};

} // namespace lumos

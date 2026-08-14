#include "lumos/doorbell/doorbell_receiver.hpp"
#include "lumos/doorbell/doorbell_packet.hpp"
#include "lumos/doorbell/doorbell_mac.hpp"
#include "lumos/core/board_pins.hpp"
#include "lumos/core/logger.hpp"

#include "driver/gpio.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace lumos {
namespace {

Logger log{"doorbell"};

constexpr std::uint16_t kMinPressMs = 100;
constexpr std::uint16_t kMaxPressMs = 2000;

} // namespace

DoorbellReceiver* DoorbellReceiver::instance_ = nullptr;

DoorbellReceiver::DoorbellReceiver(Preferences& preferences) : preferences_(preferences) {}

Result<void> DoorbellReceiver::start() {
    if (started_) {
        return Result<void>::ok();
    }
    instance_ = this;

    const esp_timer_create_args_t targs{
        .callback = &DoorbellReceiver::release_timer_cb,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "doorbell_rel",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&targs, &release_timer_) != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "doorbell timer create failed");
    }

    apply_settings();

    if (esp_now_init() != ESP_OK) {
        log.error("esp_now_init failed");
        return Result<void>::fail(ErrorCode::IoError, "esp_now_init failed");
    }
    if (esp_now_register_recv_cb(&DoorbellReceiver::recv_cb) != ESP_OK) {
        log.error("esp_now_register_recv_cb failed");
        return Result<void>::fail(ErrorCode::IoError, "esp_now recv cb failed");
    }
    espnow_ready_ = true;
    started_ = true;
    log.info("doorbell receiver ready (target=%s pin=%d)", kIdfTargetName,
             preferences_.device().doorbell.relay_pin);
    return Result<void>::ok();
}

void DoorbellReceiver::apply_settings() {
    auto& db = preferences_.device().doorbell;
    if (db.relay_pin == 0) {
        db.relay_pin = kDefaultRelayGpio;
    }
    if (!is_safe_output_gpio(db.relay_pin)) {
        log.warn("invalid relay pin %d on %s; using %d", db.relay_pin, kIdfTargetName,
                 kDefaultRelayGpio);
        db.relay_pin = kDefaultRelayGpio;
    }
    db.press_ms = static_cast<std::uint16_t>(
        std::clamp(static_cast<int>(db.press_ms), static_cast<int>(kMinPressMs),
                   static_cast<int>(kMaxPressMs)));

    paired_valid_ = parse_mac(db.paired_tx_mac, paired_mac_);
    if (!db.paired_tx_mac.empty() && !paired_valid_) {
        log.warn("paired_tx_mac invalid: %s", db.paired_tx_mac.c_str());
    }

    // Drop dedupe state when pairing changes.
    have_last_seq_ = false;

    if (relay_active_ && release_timer_ != nullptr) {
        esp_timer_stop(release_timer_);
        set_relay(false);
    }
    configure_gpio();
}

void DoorbellReceiver::test_pulse() {
    if (!preferences_.device().doorbell.enabled && !started_) {
        // Allow test even when disabled so wiring can be verified.
    }
    pulse_relay();
}

DoorbellStatus DoorbellReceiver::status() const {
    const auto& db = preferences_.device().doorbell;
    DoorbellStatus st;
    st.enabled = db.enabled;
    st.espnow_ready = espnow_ready_;
    st.paired = paired_valid_;
    st.relay_pin = db.relay_pin;
    st.active_high = db.active_high;
    st.press_ms = db.press_ms;
    st.paired_tx_mac = db.paired_tx_mac;
    st.last_ring_ms = last_ring_ms_;
    st.last_seq = last_seq_;
    st.relay_active = relay_active_;
    return st;
}

void DoorbellReceiver::recv_cb(const esp_now_recv_info_t* info, const std::uint8_t* data, int len) {
    if (instance_ == nullptr || info == nullptr || data == nullptr) {
        return;
    }
    instance_->on_packet(info->src_addr, data, len);
}

void DoorbellReceiver::release_timer_cb(void* arg) {
    auto* self = static_cast<DoorbellReceiver*>(arg);
    if (self != nullptr) {
        self->set_relay(false);
    }
}

void DoorbellReceiver::on_packet(const std::uint8_t mac[6], const std::uint8_t* data, int len) {
    const auto& db = preferences_.device().doorbell;
    if (!db.enabled || !espnow_ready_) {
        return;
    }
    if (!paired_valid_) {
        return;
    }
    if (!mac_equal(mac, paired_mac_)) {
        return;
    }
    if (len < static_cast<int>(sizeof(DoorbellPacket))) {
        return;
    }

    DoorbellPacket pkt{};
    std::memcpy(&pkt, data, sizeof(pkt));
    if (pkt.magic != kDoorbellMagic || pkt.version != kDoorbellVersion) {
        return;
    }
    if (pkt.type != DOORBELL_PRESS) {
        return;
    }
    if (have_last_seq_ && pkt.seq == last_seq_) {
        log.debug("duplicate seq %u ignored", static_cast<unsigned>(pkt.seq));
        return;
    }

    have_last_seq_ = true;
    last_seq_ = pkt.seq;
    log.info("doorbell press from %s seq=%u", format_mac(mac).c_str(),
             static_cast<unsigned>(pkt.seq));
    pulse_relay();
}

void DoorbellReceiver::pulse_relay() {
    const auto& db = preferences_.device().doorbell;
    set_relay(true);
    last_ring_ms_ = static_cast<std::uint32_t>(esp_timer_get_time() / 1000ULL);
    if (release_timer_ == nullptr) {
        return;
    }
    esp_timer_stop(release_timer_);
    const std::uint64_t us =
        static_cast<std::uint64_t>(std::max<std::uint16_t>(db.press_ms, kMinPressMs)) * 1000ULL;
    if (esp_timer_start_once(release_timer_, us) != ESP_OK) {
        log.error("failed to start relay release timer");
        set_relay(false);
    }
}

void DoorbellReceiver::set_relay(bool active) {
    const auto& db = preferences_.device().doorbell;
    const int pin = configured_pin_ >= 0 ? configured_pin_ : db.relay_pin;
    if (!is_safe_output_gpio(pin)) {
        return;
    }
    const int level = db.active_high ? (active ? 1 : 0) : (active ? 0 : 1);
    gpio_set_level(static_cast<gpio_num_t>(pin), level);
    relay_active_ = active;
}

void DoorbellReceiver::configure_gpio() {
    const auto& db = preferences_.device().doorbell;
    const int pin = db.relay_pin;
    if (!is_safe_output_gpio(pin)) {
        return;
    }

    if (configured_pin_ >= 0 && configured_pin_ != pin) {
        gpio_reset_pin(static_cast<gpio_num_t>(configured_pin_));
    }

    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << pin;
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);
    configured_pin_ = pin;
    set_relay(false);
}

bool DoorbellReceiver::parse_paired_mac(std::uint8_t out[6]) const {
    return parse_mac(preferences_.device().doorbell.paired_tx_mac, out);
}

std::string DoorbellReceiver::format_mac(const std::uint8_t mac[6]) {
    return lumos::format_mac(mac);
}

bool DoorbellReceiver::mac_equal(const std::uint8_t a[6], const std::uint8_t b[6]) {
    return lumos::mac_equal(a, b);
}

} // namespace lumos

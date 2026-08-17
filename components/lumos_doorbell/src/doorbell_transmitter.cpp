#include "lumos/doorbell/doorbell_transmitter.hpp"
#include "lumos/doorbell/doorbell_mac.hpp"
#include "lumos/doorbell/doorbell_packet.hpp"
#include "lumos/core/logger.hpp"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <algorithm>
#include <cstring>

namespace lumos {
namespace {

Logger log{"doorbell_tx"};

constexpr const char* kNvsNs = "dbtx";
constexpr std::uint32_t kDebounceUs = 40 * 1000;
constexpr std::uint32_t kRetryUs = 20 * 1000;
constexpr int kRetries = 2; // first send + 2 = 3 total
constexpr std::uint32_t kCooldownMs = 1200;
constexpr std::uint64_t kHelloPeriodUs = 400 * 1000;

int active_level(const DoorbellTxConfig& cfg) {
    return cfg.active_low ? 0 : 1;
}

bool add_espnow_peer(const std::uint8_t mac[6], std::uint8_t channel, wifi_interface_t ifidx) {
    esp_now_del_peer(mac);
    esp_now_peer_info_t peer{};
    std::memcpy(peer.peer_addr, mac, 6);
    peer.channel = channel;
    peer.ifidx = ifidx;
    peer.encrypt = false;
    return esp_now_add_peer(&peer) == ESP_OK;
}

} // namespace

DoorbellTransmitter* DoorbellTransmitter::instance_ = nullptr;

Result<void> DoorbellTransmitter::start() {
    if (started_) {
        return Result<void>::ok();
    }
    instance_ = this;
    load_nvs();

    const esp_timer_create_args_t debounce_args{
        .callback = &DoorbellTransmitter::debounce_timer_cb,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "dbtx_deb",
        .skip_unhandled_events = true,
    };
    const esp_timer_create_args_t retry_args{
        .callback = &DoorbellTransmitter::retry_timer_cb,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "dbtx_rty",
        .skip_unhandled_events = true,
    };
    const esp_timer_create_args_t hello_args{
        .callback = &DoorbellTransmitter::hello_timer_cb,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "dbtx_hi",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&debounce_args, &debounce_timer_) != ESP_OK ||
        esp_timer_create(&retry_args, &retry_timer_) != ESP_OK ||
        esp_timer_create(&hello_args, &hello_timer_) != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "doorbell tx timer create failed");
    }

    if (esp_now_init() != ESP_OK) {
        log.error("esp_now_init failed");
        return Result<void>::fail(ErrorCode::IoError, "esp_now_init failed");
    }
    if (esp_now_register_recv_cb(&DoorbellTransmitter::recv_cb) != ESP_OK) {
        log.error("esp_now_register_recv_cb failed");
        return Result<void>::fail(ErrorCode::IoError, "esp_now recv cb failed");
    }
    espnow_ready_ = true;
    configure_wifi_channel();
    ensure_broadcast_peer();
    add_peer();
    configure_gpio();
    started_ = true;
    log.info("doorbell tx ready pin=%d ch=%u rx=%s", cfg_.opto_pin,
             static_cast<unsigned>(cfg_.channel),
             cfg_.rx_mac_valid ? format_mac(cfg_.rx_mac).c_str() : "(unpaired)");
    return Result<void>::ok();
}

void DoorbellTransmitter::apply_config(const DoorbellTxConfig& cfg) {
    cfg_ = cfg;
    if (!is_safe_input_gpio(cfg_.opto_pin)) {
        log.warn("invalid opto pin %d; using %d", cfg_.opto_pin, kDefaultOptoGpio);
        cfg_.opto_pin = kDefaultOptoGpio;
    }
    cfg_.channel = static_cast<std::uint8_t>(std::clamp(static_cast<int>(cfg_.channel), 1, 13));
    if (started_) {
        configure_wifi_channel();
        add_peer();
        configure_gpio();
    }
}

void DoorbellTransmitter::load_nvs() {
    nvs_handle_t h{};
    if (nvs_open(kNvsNs, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    std::int32_t pin = cfg_.opto_pin;
    nvs_get_i32(h, "pin", &pin);
    cfg_.opto_pin = static_cast<int>(pin);

    std::uint8_t ch = cfg_.channel;
    nvs_get_u8(h, "ch", &ch);
    cfg_.channel = ch;

    std::uint8_t al = cfg_.active_low ? 1 : 0;
    nvs_get_u8(h, "alow", &al);
    cfg_.active_low = al != 0;

    nvs_get_u32(h, "txid", &cfg_.tx_id);

    char mac[32]{};
    size_t mac_len = sizeof(mac);
    if (nvs_get_str(h, "rxmac", mac, &mac_len) == ESP_OK) {
        cfg_.rx_mac_valid = parse_mac(mac, cfg_.rx_mac);
    }
    nvs_close(h);
    if (!is_safe_input_gpio(cfg_.opto_pin)) {
        cfg_.opto_pin = kDefaultOptoGpio;
    }
    cfg_.channel = static_cast<std::uint8_t>(std::clamp(static_cast<int>(cfg_.channel), 1, 13));
}

void DoorbellTransmitter::save_nvs() {
    nvs_handle_t h{};
    if (nvs_open(kNvsNs, NVS_READWRITE, &h) != ESP_OK) {
        log.error("nvs_open dbtx failed");
        return;
    }
    nvs_set_i32(h, "pin", cfg_.opto_pin);
    nvs_set_u8(h, "ch", cfg_.channel);
    nvs_set_u8(h, "alow", cfg_.active_low ? 1 : 0);
    nvs_set_u32(h, "txid", cfg_.tx_id);
    nvs_set_str(h, "rxmac", cfg_.rx_mac_valid ? format_mac(cfg_.rx_mac).c_str() : "");
    nvs_commit(h);
    nvs_close(h);
}

void DoorbellTransmitter::test_send() {
    send_press(true);
}

DoorbellTxStatus DoorbellTransmitter::status() const {
    DoorbellTxStatus st;
    st.cfg = cfg_;
    st.own_mac = own_mac();
    st.espnow_ready = espnow_ready_;
    st.paired = cfg_.rx_mac_valid;
    st.last_seq = seq_;
    st.last_send_ms = last_send_ms_;
    st.pairing = pairing_active();
    st.scanning = scanning_;
    if (st.pairing) {
        const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
        st.pairing_ms = static_cast<std::uint32_t>((pairing_until_us_ - now) / 1000ULL);
    }
    st.peer_count = peer_count_;
    for (int i = 0; i < peer_count_ && i < kDoorbellMaxPeers; ++i) {
        st.peers[i] = peers_[i];
    }
    return st;
}

void DoorbellTransmitter::start_pairing(std::uint32_t duration_ms) {
    if (!espnow_ready_ || scanning_) {
        return;
    }
    peer_count_ = 0;
    const auto ms = duration_ms == 0 ? kDoorbellPairDefaultMs : duration_ms;
    pairing_until_us_ = static_cast<std::uint64_t>(esp_timer_get_time()) +
                        static_cast<std::uint64_t>(ms) * 1000ULL;
    ensure_broadcast_peer();
    if (hello_timer_ != nullptr) {
        esp_timer_stop(hello_timer_);
        esp_timer_start_periodic(hello_timer_, kHelloPeriodUs);
    }
    scanning_ = true;
    xTaskCreate(&DoorbellTransmitter::scan_task, "dbtx_scan", 4096, this, 5, nullptr);
    log.info("doorbell TX pairing / scan");
}

void DoorbellTransmitter::stop_pairing() {
    pairing_until_us_ = 0;
    if (hello_timer_ != nullptr) {
        esp_timer_stop(hello_timer_);
    }
}

bool DoorbellTransmitter::select_peer(const std::uint8_t mac[6]) {
    if (mac == nullptr) {
        return false;
    }
    std::uint8_t channel = cfg_.channel;
    for (int i = 0; i < peer_count_; ++i) {
        if (mac_equal(peers_[i].mac, mac)) {
            if (peers_[i].channel >= 1 && peers_[i].channel <= 13) {
                channel = peers_[i].channel;
            }
            break;
        }
    }
    auto cfg = cfg_;
    std::memcpy(cfg.rx_mac, mac, 6);
    cfg.rx_mac_valid = true;
    cfg.channel = channel;
    apply_config(cfg);
    save_nvs();
    add_espnow_peer(mac, channel, WIFI_IF_AP);
    send_pair(DOORBELL_PAIR_CLAIM, mac);
    stop_pairing();
    log.info("paired RX %s ch=%u", format_mac(mac).c_str(), static_cast<unsigned>(channel));
    return true;
}

std::string DoorbellTransmitter::own_mac() const {
    std::uint8_t mac[6]{};
    // Setup AP + ESP-NOW both use the SoftAP interface.
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    return format_mac(mac);
}

void IRAM_ATTR DoorbellTransmitter::gpio_isr(void* arg) {
    auto* self = static_cast<DoorbellTransmitter*>(arg);
    if (self != nullptr && self->debounce_timer_ != nullptr) {
        esp_timer_stop(self->debounce_timer_);
        esp_timer_start_once(self->debounce_timer_, kDebounceUs);
    }
}

void DoorbellTransmitter::debounce_timer_cb(void* arg) {
    auto* self = static_cast<DoorbellTransmitter*>(arg);
    if (self != nullptr) {
        self->maybe_fire();
    }
}

void DoorbellTransmitter::retry_timer_cb(void* arg) {
    auto* self = static_cast<DoorbellTransmitter*>(arg);
    if (self == nullptr) {
        return;
    }
    if (self->retries_left_ <= 0) {
        return;
    }
    self->retries_left_--;
    self->send_press(false);
    if (self->retries_left_ > 0) {
        esp_timer_start_once(self->retry_timer_, kRetryUs);
    }
}

void DoorbellTransmitter::hello_timer_cb(void* arg) {
    auto* self = static_cast<DoorbellTransmitter*>(arg);
    if (self == nullptr) {
        return;
    }
    if (!self->pairing_active()) {
        self->stop_pairing();
        return;
    }
    if (self->scanning_) {
        return;
    }
    self->send_pair(DOORBELL_PAIR_HELLO, kDoorbellBroadcastMac);
}

void DoorbellTransmitter::recv_cb(const esp_now_recv_info_t* info, const std::uint8_t* data, int len) {
    if (instance_ == nullptr || info == nullptr || data == nullptr) {
        return;
    }
    int rssi = 0;
    if (info->rx_ctrl != nullptr) {
        rssi = info->rx_ctrl->rssi;
    }
    instance_->on_packet(info->src_addr, data, len, rssi);
}

void DoorbellTransmitter::scan_task(void* arg) {
    auto* self = static_cast<DoorbellTransmitter*>(arg);
    if (self == nullptr) {
        vTaskDelete(nullptr);
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(350));
    const auto home = self->cfg_.channel;
    self->ensure_broadcast_peer();
    for (std::uint8_t ch = 1; ch <= 13 && self->pairing_active(); ++ch) {
        self->set_ap_channel(ch);
        (void)esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        add_espnow_peer(kDoorbellBroadcastMac, ch, WIFI_IF_AP);
        vTaskDelay(pdMS_TO_TICKS(40));
        self->send_pair(DOORBELL_PAIR_HELLO, kDoorbellBroadcastMac);
        vTaskDelay(pdMS_TO_TICKS(80));
        self->send_pair(DOORBELL_PAIR_HELLO, kDoorbellBroadcastMac);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    self->set_ap_channel(home);
    add_espnow_peer(kDoorbellBroadcastMac, home, WIFI_IF_AP);
    self->scanning_ = false;
    log.info("doorbell TX scan done, %d peer(s)", self->peer_count_);
    vTaskDelete(nullptr);
}

void DoorbellTransmitter::maybe_fire() {
    const int pin = configured_pin_ >= 0 ? configured_pin_ : cfg_.opto_pin;
    const int level = gpio_get_level(static_cast<gpio_num_t>(pin));
    if (level != active_level(cfg_)) {
        return;
    }
    const auto now = static_cast<std::uint32_t>(esp_timer_get_time() / 1000ULL);
    if (last_send_ms_ != 0 && (now - last_send_ms_) < kCooldownMs) {
        return;
    }
    send_press(true);
    retries_left_ = kRetries;
    if (retry_timer_ != nullptr) {
        esp_timer_stop(retry_timer_);
        esp_timer_start_once(retry_timer_, kRetryUs);
    }
}

void DoorbellTransmitter::send_press(bool bump_seq) {
    if (!espnow_ready_ || !cfg_.rx_mac_valid) {
        log.warn("doorbell press ignored (unpaired or esp-now down)");
        return;
    }
    if (bump_seq) {
        seq_++;
        if (seq_ == 0) {
            seq_ = 1;
        }
    }
    DoorbellPacket pkt{};
    pkt.magic = kDoorbellMagic;
    pkt.version = kDoorbellVersion;
    pkt.type = DOORBELL_PRESS;
    pkt.seq = seq_;
    pkt.reserved = 0;
    pkt.tx_id = cfg_.tx_id;
    const esp_err_t err = esp_now_send(cfg_.rx_mac, reinterpret_cast<const std::uint8_t*>(&pkt),
                                       sizeof(pkt));
    last_send_ms_ = static_cast<std::uint32_t>(esp_timer_get_time() / 1000ULL);
    if (err != ESP_OK) {
        log.warn("esp_now_send failed: %s seq=%u", esp_err_to_name(err),
                 static_cast<unsigned>(seq_));
    } else {
        log.info("sent DOORBELL_PRESS seq=%u to %s", static_cast<unsigned>(seq_),
                 format_mac(cfg_.rx_mac).c_str());
    }
}

void DoorbellTransmitter::configure_gpio() {
    if (!is_safe_input_gpio(cfg_.opto_pin)) {
        return;
    }
    if (configured_pin_ >= 0 && configured_pin_ != cfg_.opto_pin) {
        gpio_isr_handler_remove(static_cast<gpio_num_t>(configured_pin_));
        gpio_reset_pin(static_cast<gpio_num_t>(configured_pin_));
    }

    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << cfg_.opto_pin;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = cfg_.active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    io.pull_down_en = cfg_.active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
    io.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&io);

    if (!gpio_isr_installed_) {
        const esp_err_t isr = gpio_install_isr_service(0);
        if (isr != ESP_OK && isr != ESP_ERR_INVALID_STATE) {
            log.error("gpio_install_isr_service failed");
            return;
        }
        gpio_isr_installed_ = true;
    }
    gpio_isr_handler_remove(static_cast<gpio_num_t>(cfg_.opto_pin));
    gpio_isr_handler_add(static_cast<gpio_num_t>(cfg_.opto_pin), &DoorbellTransmitter::gpio_isr,
                         this);
    configured_pin_ = cfg_.opto_pin;
}

void DoorbellTransmitter::configure_wifi_channel() {
    set_ap_channel(cfg_.channel);
}

void DoorbellTransmitter::set_ap_channel(std::uint8_t channel) {
    wifi_config_t ap{};
    if (esp_wifi_get_config(WIFI_IF_AP, &ap) == ESP_OK) {
        ap.ap.channel = channel;
        (void)esp_wifi_set_config(WIFI_IF_AP, &ap);
    }
}

void DoorbellTransmitter::add_peer() {
    if (!espnow_ready_ || !cfg_.rx_mac_valid) {
        return;
    }
    if (!add_espnow_peer(cfg_.rx_mac, cfg_.channel, WIFI_IF_AP)) {
        log.warn("esp_now_add_peer failed for %s", format_mac(cfg_.rx_mac).c_str());
    }
}

void DoorbellTransmitter::ensure_broadcast_peer() {
    add_espnow_peer(kDoorbellBroadcastMac, 0, WIFI_IF_AP);
}

void DoorbellTransmitter::send_pair(std::uint8_t type, const std::uint8_t* dest) {
    if (!espnow_ready_ || dest == nullptr) {
        return;
    }
    std::uint8_t mac[6]{};
    own_mac_bytes(mac);
    DoorbellPairHello pkt{};
    fill_pair_hello(pkt, type, DOORBELL_ROLE_TX, cfg_.channel, mac, "LumosOS-Bell", cfg_.tx_id);
    (void)esp_now_send(dest, reinterpret_cast<const std::uint8_t*>(&pkt), sizeof(pkt));
}

void DoorbellTransmitter::on_packet(const std::uint8_t mac[6], const std::uint8_t* data, int len,
                                   int rssi) {
    DoorbellPairHello hello{};
    if (!parse_pair_hello(data, len, hello) || hello.role != DOORBELL_ROLE_RX) {
        return;
    }
    std::uint8_t self_mac[6]{};
    own_mac_bytes(self_mac);
    if (mac_equal(mac, self_mac)) {
        return;
    }
    if (!pairing_active() && !scanning_) {
        return;
    }
    // Prefer the STA MAC the RX put in the payload — that is the ESP-NOW dest for presses.
    note_peer(hello.mac, hello, rssi);
    if (hello.type == DOORBELL_PAIR_CLAIM) {
        select_peer(hello.mac);
    }
}

void DoorbellTransmitter::note_peer(const std::uint8_t mac[6], const DoorbellPairHello& hello,
                                   int rssi) {
    std::uint8_t zeros[6]{};
    const std::uint8_t* use = mac;
    if (mac_equal(mac, zeros)) {
        return;
    }
    for (int i = 0; i < peer_count_; ++i) {
        if (mac_equal(peers_[i].mac, use)) {
            peers_[i].channel = hello.channel;
            peers_[i].role = hello.role;
            peers_[i].rssi = static_cast<std::int8_t>(rssi);
            std::memcpy(peers_[i].name, hello.name, sizeof(peers_[i].name));
            return;
        }
    }
    if (peer_count_ >= kDoorbellMaxPeers) {
        return;
    }
    auto& p = peers_[peer_count_++];
    std::memcpy(p.mac, use, 6);
    p.channel = hello.channel;
    p.role = hello.role;
    p.rssi = static_cast<std::int8_t>(rssi);
    std::memcpy(p.name, hello.name, sizeof(p.name));
}

void DoorbellTransmitter::own_mac_bytes(std::uint8_t out[6]) const {
    esp_read_mac(out, ESP_MAC_WIFI_SOFTAP);
}

bool DoorbellTransmitter::pairing_active() const {
    if (pairing_until_us_ == 0) {
        return false;
    }
    return static_cast<std::uint64_t>(esp_timer_get_time()) < pairing_until_us_;
}

} // namespace lumos

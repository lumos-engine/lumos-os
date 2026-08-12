#pragma once

#include <cstdint>

namespace lumos {

// Shared ESP-NOW wire format for doorbell TX (#1) and RX (#2).
constexpr std::uint32_t kDoorbellMagic = 0x4C444242u; // 'LDBB'
constexpr std::uint8_t kDoorbellVersion = 1;

enum DoorbellEventType : std::uint8_t {
    DOORBELL_PRESS = 1,
};

struct DoorbellPacket {
    std::uint32_t magic;
    std::uint8_t version;
    std::uint8_t type;
    std::uint8_t seq;
    std::uint8_t reserved;
    std::uint32_t tx_id;
} __attribute__((packed));

static_assert(sizeof(DoorbellPacket) == 12, "DoorbellPacket size");

} // namespace lumos

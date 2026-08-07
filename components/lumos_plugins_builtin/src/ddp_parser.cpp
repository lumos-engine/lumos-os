#include "lumos/plugins/ddp_parser.hpp"

#include <algorithm>
#include <cstring>

namespace lumos {

DdpParser::DdpParser(LedIndex led_count) : led_count_(led_count) {}

void DdpParser::set_led_count(LedIndex led_count) {
    led_count_ = led_count;
}

DdpParseResult DdpParser::parse_header(std::span<const std::uint8_t> packet) const {
    DdpParseResult result{};
    if (packet.size() < kHeaderSize) {
        return result;
    }

    const std::uint8_t flags = packet[0];
    const std::uint8_t version = (flags >> 6) & 0x03;
    if (version != 1) {
        return result;
    }

    result.valid = true;
    result.push = (flags & 0x01) != 0;
    result.sequence = packet[1] & 0x0F;
    result.data_offset = (static_cast<std::uint32_t>(packet[4]) << 24) |
                         (static_cast<std::uint32_t>(packet[5]) << 16) |
                         (static_cast<std::uint32_t>(packet[6]) << 8) |
                         static_cast<std::uint32_t>(packet[7]);
    const std::uint16_t data_len = (static_cast<std::uint16_t>(packet[8]) << 8) |
                                   static_cast<std::uint16_t>(packet[9]);

    const std::size_t header = kHeaderSize;
    if (packet.size() < header + data_len) {
        result.valid = false;
        return result;
    }
    result.payload = packet.subspan(header, data_len);
    return result;
}

bool DdpParser::apply_payload(const DdpParseResult& header, std::span<const std::uint8_t> /*packet*/,
                              std::vector<Rgb>& assembly) const {
    if (!header.valid || led_count_ == 0) {
        return false;
    }
    if (assembly.size() != led_count_) {
        assembly.assign(led_count_, Rgb::black());
    }

    const std::uint32_t byte_offset = header.data_offset;
    const std::size_t pixel_offset = byte_offset / 3;
    const std::size_t pixel_count = header.payload.size() / 3;

    for (std::size_t i = 0; i < pixel_count; ++i) {
        const std::size_t idx = pixel_offset + i;
        if (idx >= assembly.size()) {
            break;
        }
        const std::size_t base = i * 3;
        assembly[idx] = Rgb{header.payload[base], header.payload[base + 1], header.payload[base + 2]};
    }

    return header.push;
}

} // namespace lumos

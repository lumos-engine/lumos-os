#pragma once

#include "lumos/core/color.hpp"
#include "lumos/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lumos {

// Pure DDP parser — host-testable, no sockets.
struct DdpParseResult {
    bool valid{false};
    bool push{false};
    std::uint8_t sequence{0};
    std::uint32_t data_offset{0};
    std::span<const std::uint8_t> payload;
};

class DdpParser {
public:
    explicit DdpParser(LedIndex led_count);

    void set_led_count(LedIndex led_count);
    DdpParseResult parse_header(std::span<const std::uint8_t> packet) const;

    // Applies payload into the assembly buffer. Returns true when a full frame is ready (PUSH).
    bool apply_payload(const DdpParseResult& header, std::span<const std::uint8_t> packet,
                       std::vector<Rgb>& assembly) const;

    static constexpr std::uint16_t kPort = 4048;
    static constexpr std::size_t kHeaderSize = 10;

private:
    LedIndex led_count_{0};
};

} // namespace lumos

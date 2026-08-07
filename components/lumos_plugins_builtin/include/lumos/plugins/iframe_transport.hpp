#pragma once

#include "lumos/core/color.hpp"
#include "lumos/core/result.hpp"
#include "lumos/core/types.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace lumos {

struct LedFrame {
    std::vector<Rgb> pixels;
    std::uint64_t timestamp_ms{0};
};

using FrameCallback = std::function<void(const LedFrame& frame)>;

// Transport/protocol decoder boundary — HyperHDR plugin depends on this, not DDP specifics.
class IFrameTransport {
public:
    virtual ~IFrameTransport() = default;

    virtual Result<void> start(LedIndex led_count, FrameCallback on_frame) = 0;
    virtual void stop() = 0;
    virtual void poll() = 0; // non-blocking receive/process
    virtual const char* name() const = 0;
};

} // namespace lumos

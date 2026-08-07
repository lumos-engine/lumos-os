#pragma once

#include "lumos/core/color.hpp"
#include "lumos/core/types.hpp"

#include <span>

namespace lumos {

struct PowerLimiterConfig {
    std::uint16_t max_ma{kDefaultPowerLimitMa};
    std::uint16_t ma_per_channel_at_full{20}; // typical WS281x channel draw estimate
};

// Scales pixels in-place so estimated current stays under max_ma.
// Pure logic — unit-testable on host.
class PowerLimiter {
public:
    explicit PowerLimiter(PowerLimiterConfig config = {});

    void set_config(PowerLimiterConfig config);
    PowerLimiterConfig config() const { return config_; }

    // Returns scale factor applied (1.0 = no limiting).
    float apply(std::span<Rgb> pixels) const;
    float apply(std::span<Rgbw> pixels) const;

    static std::uint32_t estimate_current_ma(std::span<const Rgb> pixels,
                                             std::uint16_t ma_per_channel_at_full);
    static std::uint32_t estimate_current_ma(std::span<const Rgbw> pixels,
                                             std::uint16_t ma_per_channel_at_full);

private:
    PowerLimiterConfig config_{};
};

} // namespace lumos

#include "lumos/renderer/power_limiter.hpp"

#include <algorithm>

namespace lumos {

PowerLimiter::PowerLimiter(PowerLimiterConfig config) : config_(config) {}

void PowerLimiter::set_config(PowerLimiterConfig config) {
    config_ = config;
}

std::uint32_t PowerLimiter::estimate_current_ma(std::span<const Rgb> pixels,
                                                std::uint16_t ma_per_channel_at_full) {
    std::uint64_t channel_sum = 0;
    for (const auto& p : pixels) {
        channel_sum += p.r;
        channel_sum += p.g;
        channel_sum += p.b;
    }
    // Full channel (255) draws ma_per_channel_at_full.
    return static_cast<std::uint32_t>((channel_sum * ma_per_channel_at_full) / 255ULL);
}

float PowerLimiter::apply(std::span<Rgb> pixels) const {
    if (config_.max_ma == 0 || pixels.empty()) {
        return 1.0f;
    }

    const auto estimated = estimate_current_ma(pixels, config_.ma_per_channel_at_full);
    if (estimated <= config_.max_ma) {
        return 1.0f;
    }

    const float scale = static_cast<float>(config_.max_ma) / static_cast<float>(estimated);
    for (auto& p : pixels) {
        p.r = static_cast<std::uint8_t>(p.r * scale + 0.5f);
        p.g = static_cast<std::uint8_t>(p.g * scale + 0.5f);
        p.b = static_cast<std::uint8_t>(p.b * scale + 0.5f);
    }
    return scale;
}

} // namespace lumos

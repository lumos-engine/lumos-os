#include "lumos/renderer/power_limiter.hpp"

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
    return static_cast<std::uint32_t>((channel_sum * ma_per_channel_at_full) / 255ULL);
}

std::uint32_t PowerLimiter::estimate_current_ma(std::span<const Rgbw> pixels,
                                                std::uint16_t ma_per_channel_at_full) {
    std::uint64_t channel_sum = 0;
    for (const auto& p : pixels) {
        channel_sum += p.r;
        channel_sum += p.g;
        channel_sum += p.b;
        channel_sum += p.w;
    }
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
        p = scale_rgb(p, scale);
    }
    return scale;
}

float PowerLimiter::apply(std::span<Rgbw> pixels) const {
    if (config_.max_ma == 0 || pixels.empty()) {
        return 1.0f;
    }

    const auto estimated = estimate_current_ma(pixels, config_.ma_per_channel_at_full);
    if (estimated <= config_.max_ma) {
        return 1.0f;
    }

    const float scale = static_cast<float>(config_.max_ma) / static_cast<float>(estimated);
    for (auto& p : pixels) {
        p = scale_rgbw(p, scale);
    }
    return scale;
}

} // namespace lumos

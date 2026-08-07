#include "lumos/renderer/renderer.hpp"

namespace lumos {

Renderer::Renderer(ILedDriver& driver, RendererConfig config)
    : driver_(driver),
      config_(config),
      gamma_(config.gamma),
      power_(config.power) {}

Result<void> Renderer::init(LedIndex led_count) {
    scratch_.assign(led_count, Rgb::black());
    return Result<void>::ok();
}

void Renderer::set_brightness(Brightness b) {
    config_.brightness = b;
}

void Renderer::set_gamma(float gamma) {
    config_.gamma = gamma;
    gamma_.set_gamma(gamma);
}

void Renderer::set_power_limit_ma(std::uint16_t ma) {
    config_.power.max_ma = ma;
    power_.set_config(config_.power);
}

Result<void> Renderer::present(const Framebuffer& framebuffer) {
    if (scratch_.size() != framebuffer.size()) {
        scratch_.assign(framebuffer.size(), Rgb::black());
    }

    const float brightness_scale = static_cast<float>(config_.brightness) / 255.0f;
    for (LedIndex i = 0; i < framebuffer.size(); ++i) {
        Rgb c = gamma_.apply(framebuffer[i]);
        c = scale_rgb(c, brightness_scale);
        scratch_[i] = c;
    }

    last_power_scale_ = power_.apply(scratch_);
    return driver_.show(scratch_);
}

} // namespace lumos

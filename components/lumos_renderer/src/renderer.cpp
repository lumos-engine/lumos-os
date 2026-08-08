#include "lumos/renderer/renderer.hpp"

namespace lumos {

Renderer::Renderer(ILedDriver& driver, RendererConfig config)
    : driver_(driver),
      config_(config),
      color_({
          .brightness = config.brightness,
          .gamma = config.gamma,
          .chipset = config.chipset,
          .color_order = config.color_order,
          .white_algorithm = config.white_algorithm,
          .balance_r = config.balance_r,
          .balance_g = config.balance_g,
          .balance_b = config.balance_b,
      }),
      power_(config.power) {}

void Renderer::sync_color_processor() {
    color_.configure({
        .brightness = config_.brightness,
        .gamma = config_.gamma,
        .chipset = config_.chipset,
        .color_order = config_.color_order,
        .white_algorithm = config_.white_algorithm,
        .balance_r = config_.balance_r,
        .balance_g = config_.balance_g,
        .balance_b = config_.balance_b,
    });
}

Result<void> Renderer::init(LedIndex led_count) {
    scratch_rgb_.assign(led_count, Rgb::black());
    scratch_rgbw_.assign(led_count, Rgbw::black());
    sync_color_processor();
    driver_.set_color_order(config_.color_order);
    return Result<void>::ok();
}

void Renderer::set_brightness(Brightness b) {
    config_.brightness = b;
    sync_color_processor();
}

void Renderer::set_gamma(float gamma) {
    config_.gamma = gamma;
    sync_color_processor();
}

void Renderer::set_channel_balance(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    config_.balance_r = r;
    config_.balance_g = g;
    config_.balance_b = b;
    sync_color_processor();
}

void Renderer::set_power_limit_ma(std::uint16_t ma) {
    config_.power.max_ma = ma;
    power_.set_config(config_.power);
}

void Renderer::set_chipset(Chipset chipset) {
    config_.chipset = chipset;
    sync_color_processor();
}

void Renderer::set_color_order(ColorOrder order) {
    config_.color_order = order;
    sync_color_processor();
    // Wire swizzle is applied in the driver from logical RGB.
    driver_.set_color_order(order);
}

void Renderer::set_white_algorithm(WhiteAlgorithm algo) {
    config_.white_algorithm = algo;
    sync_color_processor();
}

Result<void> Renderer::present(const Framebuffer& framebuffer) {
    const auto view = framebuffer.span();
    if (color_.is_rgbw()) {
        color_.process_rgbw(view, scratch_rgbw_);
        last_power_scale_ = power_.apply(scratch_rgbw_);
        return driver_.show_rgbw(scratch_rgbw_);
    }
    color_.process_rgb(view, scratch_rgb_);
    last_power_scale_ = power_.apply(scratch_rgb_);
    return driver_.show(scratch_rgb_);
}

} // namespace lumos

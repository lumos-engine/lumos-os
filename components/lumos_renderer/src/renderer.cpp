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
      }),
      power_(config.power) {}

void Renderer::sync_color_processor() {
    color_.configure({
        .brightness = config_.brightness,
        .gamma = config_.gamma,
        .chipset = config_.chipset,
        .color_order = config_.color_order,
        .white_algorithm = config_.white_algorithm,
    });
}

Result<void> Renderer::init(LedIndex led_count) {
    scratch_rgb_.assign(led_count, Rgb::black());
    scratch_rgbw_.assign(led_count, Rgbw::black());
    sync_color_processor();
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

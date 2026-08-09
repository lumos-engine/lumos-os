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

Result<void> Renderer::init(LedIndex physical_led_count) {
    led_count_ = physical_led_count;
    scratch_rgb_.assign(physical_led_count, Rgb::black());
    scratch_rgbw_.assign(physical_led_count, Rgbw::black());
    geometry_ = build_led_geometry(physical_led_count, {.top = physical_led_count}, {}, {},
                                   PerimeterStart::TopLeft, PerimeterDirection::Clockwise);
    sync_color_processor();
    driver_.set_color_order(config_.color_order);
    return Result<void>::ok();
}

void Renderer::set_geometry(LedGeometry geometry) {
    geometry_ = std::move(geometry);
}

void Renderer::set_apply_led_ignore(bool enabled) {
    apply_ignore_ = enabled;
}

void Renderer::apply_ignore_to_rgb() {
    if (!apply_ignore_ || geometry_.physical_ignore_mask.empty()) {
        return;
    }
    for (LedIndex i = 0; i < scratch_rgb_.size(); ++i) {
        if (geometry_.is_physical_ignored(i)) {
            scratch_rgb_[i] = Rgb::black();
        }
    }
}

void Renderer::apply_ignore_to_rgbw() {
    if (!apply_ignore_ || geometry_.physical_ignore_mask.empty()) {
        return;
    }
    for (LedIndex i = 0; i < scratch_rgbw_.size(); ++i) {
        if (geometry_.is_physical_ignored(i)) {
            scratch_rgbw_[i] = Rgbw::black();
        }
    }
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
        if (scratch_rgbw_.size() < led_count_) {
            scratch_rgbw_.resize(led_count_, Rgbw::black());
        } else if (scratch_rgbw_.size() > led_count_) {
            scratch_rgbw_.resize(led_count_);
        }
        apply_ignore_to_rgbw();
        last_power_scale_ = power_.apply(scratch_rgbw_);
        apply_ignore_to_rgbw();
        return driver_.show_rgbw(scratch_rgbw_);
    }
    color_.process_rgb(view, scratch_rgb_);
    if (scratch_rgb_.size() < led_count_) {
        scratch_rgb_.resize(led_count_, Rgb::black());
    } else if (scratch_rgb_.size() > led_count_) {
        scratch_rgb_.resize(led_count_);
    }
    apply_ignore_to_rgb();
    last_power_scale_ = power_.apply(scratch_rgb_);
    apply_ignore_to_rgb();
    return driver_.show(scratch_rgb_);
}

} // namespace lumos

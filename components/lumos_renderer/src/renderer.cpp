#include "lumos/renderer/renderer.hpp"
#include "lumos/core/led_calibration.hpp"

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
    led_count_ = led_count;
    scratch_rgb_.assign(led_count, Rgb::black());
    scratch_rgbw_.assign(led_count, Rgbw::black());
    wire_rgb_.assign(led_count, Rgb::black());
    wire_rgbw_.assign(led_count, Rgbw::black());
    ignore_mask_.assign((led_count + 7) / 8, 0);
    ignored_count_ = 0;
    perimeter_.logical_to_physical.resize(led_count);
    perimeter_.physical_to_logical.resize(led_count);
    for (LedIndex i = 0; i < led_count; ++i) {
        perimeter_.logical_to_physical[i] = i;
        perimeter_.physical_to_logical[i] = i;
    }
    perimeter_.identity = true;
    sync_color_processor();
    driver_.set_color_order(config_.color_order);
    return Result<void>::ok();
}

void Renderer::set_perimeter_map(PerimeterMaps maps) {
    if (maps.logical_to_physical.size() != led_count_ ||
        maps.physical_to_logical.size() != led_count_) {
        perimeter_.logical_to_physical.resize(led_count_);
        perimeter_.physical_to_logical.resize(led_count_);
        for (LedIndex i = 0; i < led_count_; ++i) {
            perimeter_.logical_to_physical[i] = i;
            perimeter_.physical_to_logical[i] = i;
        }
        perimeter_.identity = true;
        return;
    }
    perimeter_ = std::move(maps);
}

void Renderer::set_ignored_leds(const std::vector<std::uint16_t>& indices) {
    ignore_mask_ = ignore_mask_from_indices(led_count_, indices);
    ignored_count_ = 0;
    for (LedIndex i = 0; i < led_count_; ++i) {
        if (ignore_mask_test(ignore_mask_, i)) {
            ++ignored_count_;
        }
    }
}

void Renderer::set_apply_led_ignore(bool enabled) {
    apply_ignore_ = enabled;
}

void Renderer::apply_ignore_to_rgb() {
    if (!apply_ignore_ || ignored_count_ == 0) {
        return;
    }
    for (LedIndex i = 0; i < scratch_rgb_.size(); ++i) {
        if (ignore_mask_test(ignore_mask_, i)) {
            scratch_rgb_[i] = Rgb::black();
        }
    }
}

void Renderer::apply_ignore_to_rgbw() {
    if (!apply_ignore_ || ignored_count_ == 0) {
        return;
    }
    for (LedIndex i = 0; i < scratch_rgbw_.size(); ++i) {
        if (ignore_mask_test(ignore_mask_, i)) {
            scratch_rgbw_[i] = Rgbw::black();
        }
    }
}

void Renderer::scatter_to_wire_rgb() {
    if (perimeter_.identity) {
        wire_rgb_ = scratch_rgb_;
        return;
    }
    wire_rgb_.assign(led_count_, Rgb::black());
    for (LedIndex logical = 0; logical < led_count_; ++logical) {
        const auto phys = perimeter_.logical_to_physical[logical];
        if (phys < led_count_) {
            wire_rgb_[phys] = scratch_rgb_[logical];
        }
    }
}

void Renderer::scatter_to_wire_rgbw() {
    if (perimeter_.identity) {
        wire_rgbw_ = scratch_rgbw_;
        return;
    }
    wire_rgbw_.assign(led_count_, Rgbw::black());
    for (LedIndex logical = 0; logical < led_count_; ++logical) {
        const auto phys = perimeter_.logical_to_physical[logical];
        if (phys < led_count_) {
            wire_rgbw_[phys] = scratch_rgbw_[logical];
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
        apply_ignore_to_rgbw();
        scatter_to_wire_rgbw();
        last_power_scale_ = power_.apply(wire_rgbw_);
        return driver_.show_rgbw(wire_rgbw_);
    }
    color_.process_rgb(view, scratch_rgb_);
    apply_ignore_to_rgb();
    scatter_to_wire_rgb();
    last_power_scale_ = power_.apply(wire_rgb_);
    return driver_.show(wire_rgb_);
}

} // namespace lumos

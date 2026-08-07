#include "lumos/renderer/color_processor.hpp"

namespace lumos {

ColorProcessor::ColorProcessor(ColorProcessorConfig config)
    : config_(config), gamma_(config.gamma) {}

void ColorProcessor::configure(const ColorProcessorConfig& config) {
    config_ = config;
    gamma_.set_gamma(config.gamma);
}

Rgb ColorProcessor::apply_color_order(Rgb c, ColorOrder order) {
    switch (order) {
    case ColorOrder::Rgb:
        return c;
    case ColorOrder::Rbg:
        return {c.r, c.b, c.g};
    case ColorOrder::Grb:
        return {c.g, c.r, c.b};
    case ColorOrder::Gbr:
        return {c.g, c.b, c.r};
    case ColorOrder::Brg:
        return {c.b, c.r, c.g};
    case ColorOrder::Bgr:
        return {c.b, c.g, c.r};
    }
    return c;
}

Rgbw ColorProcessor::apply_color_order(Rgbw c, ColorOrder order) {
    const Rgb ordered = apply_color_order(Rgb{c.r, c.g, c.b}, order);
    return {ordered.r, ordered.g, ordered.b, c.w};
}

void ColorProcessor::process_rgb(std::span<const Rgb> in, std::vector<Rgb>& out) {
    out.resize(in.size());
    const float brightness_scale = static_cast<float>(config_.brightness) / 255.0f;
    for (std::size_t i = 0; i < in.size(); ++i) {
        Rgb c = gamma_.apply(in[i]);
        out[i] = scale_rgb(c, brightness_scale);
    }
}

void ColorProcessor::process_rgbw(std::span<const Rgb> in, std::vector<Rgbw>& out) {
    out.resize(in.size());
    const float brightness_scale = static_cast<float>(config_.brightness) / 255.0f;
    for (std::size_t i = 0; i < in.size(); ++i) {
        Rgb c = gamma_.apply(in[i]);
        c = scale_rgb(c, brightness_scale);
        out[i] = rgb_to_rgbw(c, config_.white_algorithm);
    }
}

} // namespace lumos

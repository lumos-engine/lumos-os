#pragma once

#include "lumos/core/color.hpp"
#include "lumos/core/types.hpp"
#include "lumos/renderer/gamma.hpp"

#include <span>
#include <vector>

namespace lumos {

struct ColorProcessorConfig {
    Brightness brightness{kDefaultBrightness};
    float gamma{kDefaultGamma};
    Chipset chipset{Chipset::Ws2815};
    ColorOrder color_order{ColorOrder::Grb};
    WhiteAlgorithm white_algorithm{WhiteAlgorithm::ExtractMin};
    // Per-channel gain after gamma (255 = unity). Use to tame over-bright green diodes.
    std::uint8_t balance_r{kDefaultChannelBalance};
    std::uint8_t balance_g{kDefaultChannelBalance};
    std::uint8_t balance_b{kDefaultChannelBalance};
};

// Color-space transforms between plugin framebuffer (RGB) and driver pixels.
// Host-testable — no ESP dependencies.
class ColorProcessor {
public:
    explicit ColorProcessor(ColorProcessorConfig config = {});

    void configure(const ColorProcessorConfig& config);
    ColorProcessorConfig config() const { return config_; }

    bool is_rgbw() const { return config_.chipset == Chipset::Sk6812Rgbw; }

    // Gamma → brightness → optional channel swizzle (logical RGB out).
    void process_rgb(std::span<const Rgb> in, std::vector<Rgb>& out);

    // Gamma → brightness → RGB→RGBW → optional channel swizzle on RGB legs.
    void process_rgbw(std::span<const Rgb> in, std::vector<Rgbw>& out);

    static Rgb apply_color_order(Rgb c, ColorOrder order);
    static Rgbw apply_color_order(Rgbw c, ColorOrder order);

private:
    ColorProcessorConfig config_{};
    GammaCorrector gamma_;
};

} // namespace lumos

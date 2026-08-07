#pragma once

#include "lumos/core/framebuffer.hpp"
#include "lumos/core/result.hpp"
#include "lumos/core/types.hpp"
#include "lumos/led/iled_driver.hpp"
#include "lumos/renderer/color_processor.hpp"
#include "lumos/renderer/power_limiter.hpp"

#include <vector>

namespace lumos {

struct RendererConfig {
    Brightness brightness{kDefaultBrightness};
    float gamma{kDefaultGamma};
    Chipset chipset{Chipset::Ws2815};
    ColorOrder color_order{ColorOrder::Grb};
    WhiteAlgorithm white_algorithm{WhiteAlgorithm::ExtractMin};
    PowerLimiterConfig power{};
};

class Renderer {
public:
    Renderer(ILedDriver& driver, RendererConfig config = {});

    Result<void> init(LedIndex led_count);
    void set_brightness(Brightness b);
    Brightness brightness() const { return config_.brightness; }
    void set_gamma(float gamma);
    void set_power_limit_ma(std::uint16_t ma);
    void set_chipset(Chipset chipset);
    void set_color_order(ColorOrder order);
    void set_white_algorithm(WhiteAlgorithm algo);

    // Owns presentation: ColorProcessor → power → driver.
    Result<void> present(const Framebuffer& framebuffer);

    float last_power_scale() const { return last_power_scale_; }
    bool is_rgbw() const { return color_.is_rgbw(); }

private:
    void sync_color_processor();

    ILedDriver& driver_;
    RendererConfig config_{};
    ColorProcessor color_;
    PowerLimiter power_;
    std::vector<Rgb> scratch_rgb_;
    std::vector<Rgbw> scratch_rgbw_;
    float last_power_scale_{1.0f};
};

} // namespace lumos

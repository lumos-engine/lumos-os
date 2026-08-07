#pragma once

#include "lumos/core/framebuffer.hpp"
#include "lumos/core/result.hpp"
#include "lumos/core/types.hpp"
#include "lumos/led/iled_driver.hpp"
#include "lumos/renderer/gamma.hpp"
#include "lumos/renderer/power_limiter.hpp"

#include <vector>

namespace lumos {

struct RendererConfig {
    Brightness brightness{kDefaultBrightness};
    float gamma{kDefaultGamma};
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

    // Owns presentation: gamma → brightness → power → driver.
    Result<void> present(const Framebuffer& framebuffer);

    float last_power_scale() const { return last_power_scale_; }

private:
    ILedDriver& driver_;
    RendererConfig config_{};
    GammaCorrector gamma_;
    PowerLimiter power_;
    std::vector<Rgb> scratch_;
    float last_power_scale_{1.0f};
};

} // namespace lumos

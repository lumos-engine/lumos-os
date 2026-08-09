#pragma once

#include "lumos/core/framebuffer.hpp"
#include "lumos/core/perimeter_map.hpp"
#include "lumos/core/result.hpp"
#include "lumos/core/types.hpp"
#include "lumos/led/iled_driver.hpp"
#include "lumos/renderer/color_processor.hpp"
#include "lumos/renderer/power_limiter.hpp"

#include <cstdint>
#include <vector>

namespace lumos {

struct RendererConfig {
    Brightness brightness{kDefaultBrightness};
    float gamma{kDefaultGamma};
    Chipset chipset{Chipset::Ws2815};
    ColorOrder color_order{ColorOrder::Grb};
    WhiteAlgorithm white_algorithm{WhiteAlgorithm::ExtractMin};
    std::uint8_t balance_r{kDefaultChannelBalance};
    std::uint8_t balance_g{kDefaultChannelBalance};
    std::uint8_t balance_b{kDefaultChannelBalance};
    PowerLimiterConfig power{};
};

class Renderer {
public:
    Renderer(ILedDriver& driver, RendererConfig config = {});

    Result<void> init(LedIndex led_count);
    void set_brightness(Brightness b);
    Brightness brightness() const { return config_.brightness; }
    void set_gamma(float gamma);
    void set_channel_balance(std::uint8_t r, std::uint8_t g, std::uint8_t b);
    void set_power_limit_ma(std::uint16_t ma);
    void set_chipset(Chipset chipset);
    void set_color_order(ColorOrder order);
    ColorOrder color_order() const { return config_.color_order; }
    void set_white_algorithm(WhiteAlgorithm algo);

    // Logical (CW from top-left) framebuffer → wire order for the driver.
    void set_perimeter_map(PerimeterMaps maps);
    const PerimeterMaps& perimeter_map() const { return perimeter_; }

    // Calibration: force listed logical indices off at present (unless bypassed).
    void set_ignored_leds(const std::vector<std::uint16_t>& indices);
    void set_apply_led_ignore(bool enabled);
    bool apply_led_ignore() const { return apply_ignore_; }
    LedIndex ignored_count() const { return ignored_count_; }

    // Owns presentation: ColorProcessor → ignore → perimeter remap → power → driver.
    Result<void> present(const Framebuffer& framebuffer);

    float last_power_scale() const { return last_power_scale_; }
    bool is_rgbw() const { return color_.is_rgbw(); }

private:
    void sync_color_processor();
    void apply_ignore_to_rgb();
    void apply_ignore_to_rgbw();
    void scatter_to_wire_rgb();
    void scatter_to_wire_rgbw();

    ILedDriver& driver_;
    RendererConfig config_{};
    ColorProcessor color_;
    PowerLimiter power_;
    std::vector<Rgb> scratch_rgb_;
    std::vector<Rgbw> scratch_rgbw_;
    std::vector<Rgb> wire_rgb_;
    std::vector<Rgbw> wire_rgbw_;
    std::vector<std::uint8_t> ignore_mask_{};
    PerimeterMaps perimeter_{};
    bool apply_ignore_{true};
    LedIndex led_count_{0};
    LedIndex ignored_count_{0};
    float last_power_scale_{1.0f};
};

} // namespace lumos

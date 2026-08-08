#pragma once

#include "lumos/core/color.hpp"
#include "lumos/core/result.hpp"
#include "lumos/core/types.hpp"

#include <cstddef>
#include <span>

namespace lumos {

struct LedDriverConfig {
    int gpio{kDefaultLedGpio};
    LedIndex led_count{kDefaultLedCount};
    Chipset chipset{Chipset::Ws2815};
    ColorOrder color_order{ColorOrder::Grb};
};

class ILedDriver {
public:
    virtual ~ILedDriver() = default;

    virtual Result<void> init(const LedDriverConfig& config) = 0;
    virtual Result<void> show(std::span<const Rgb> pixels) = 0;
    virtual Result<void> show_rgbw(std::span<const Rgbw> pixels) = 0;
    virtual Result<void> clear() = 0;
    virtual LedIndex led_count() const = 0;
    virtual bool is_rgbw() const = 0;
    virtual ColorOrder color_order() const = 0;
    virtual void set_color_order(ColorOrder order) = 0;
    virtual void deinit() = 0;
};

} // namespace lumos

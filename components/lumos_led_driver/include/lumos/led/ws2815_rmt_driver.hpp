#pragma once

#include "lumos/led/iled_driver.hpp"

#include "led_strip_types.h"

namespace lumos {

class Ws2815RmtDriver final : public ILedDriver {
public:
    Ws2815RmtDriver();
    ~Ws2815RmtDriver() override;

    Ws2815RmtDriver(const Ws2815RmtDriver&) = delete;
    Ws2815RmtDriver& operator=(const Ws2815RmtDriver&) = delete;

    Result<void> init(const LedDriverConfig& config) override;
    Result<void> show(std::span<const Rgb> pixels) override;
    Result<void> show_rgbw(std::span<const Rgbw> pixels) override;
    Result<void> clear() override;
    LedIndex led_count() const override { return config_.led_count; }
    bool is_rgbw() const override { return config_.chipset == Chipset::Sk6812Rgbw; }
    void deinit() override;

private:
    LedDriverConfig config_{};
    led_strip_handle_t strip_{nullptr};
};

} // namespace lumos

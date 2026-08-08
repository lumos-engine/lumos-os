#include "lumos/led/ws2815_rmt_driver.hpp"
#include "lumos/core/color_order.hpp"

#include "led_strip.h"

namespace lumos {
namespace {

led_model_t to_led_model(Chipset chipset) {
    switch (chipset) {
    case Chipset::Sk6812Rgb:
    case Chipset::Sk6812Rgbw:
        return LED_MODEL_SK6812;
    case Chipset::Ws2815:
    case Chipset::Ws2812B:
    case Chipset::Ws2813:
    default:
        return LED_MODEL_WS2812;
    }
}

led_pixel_format_t to_pixel_format(Chipset chipset) {
    if (chipset == Chipset::Sk6812Rgbw) {
        return LED_PIXEL_FORMAT_GRBW;
    }
    return LED_PIXEL_FORMAT_GRB;
}

} // namespace

Ws2815RmtDriver::Ws2815RmtDriver() = default;

Ws2815RmtDriver::~Ws2815RmtDriver() {
    deinit();
}

Result<void> Ws2815RmtDriver::init(const LedDriverConfig& config) {
    if (config.led_count == 0) {
        return Result<void>::fail(ErrorCode::InvalidArgument, "led_count must be > 0");
    }

    deinit();
    config_ = config;

    led_strip_config_t strip_config = {
        .strip_gpio_num = config.gpio,
        .max_leds = config.led_count,
        .led_pixel_format = to_pixel_format(config.chipset),
        .led_model = to_led_model(config.chipset),
        .flags = {.invert_out = false},
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 0,
        .flags = {.with_dma = false},
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_);
    if (err != ESP_OK || strip_ == nullptr) {
        return Result<void>::fail(ErrorCode::IoError, "failed to create RMT LED strip");
    }

    led_strip_clear(strip_);
    return Result<void>::ok();
}

Result<void> Ws2815RmtDriver::show(std::span<const Rgb> pixels) {
    if (strip_ == nullptr) {
        return Result<void>::fail(ErrorCode::NotInitialized, "LED driver not initialized");
    }
    if (is_rgbw()) {
        return Result<void>::fail(ErrorCode::InvalidArgument, "use show_rgbw for RGBW chipset");
    }
    if (pixels.size() != config_.led_count) {
        return Result<void>::fail(ErrorCode::InvalidArgument, "pixel count mismatch");
    }

    for (LedIndex i = 0; i < config_.led_count; ++i) {
        const auto& p = pixels[i];
        std::uint8_t red_arg = 0;
        std::uint8_t green_arg = 0;
        std::uint8_t blue_arg = 0;
        logical_to_led_strip_args(p, config_.color_order, red_arg, green_arg, blue_arg);
        esp_err_t err = led_strip_set_pixel(strip_, i, red_arg, green_arg, blue_arg);
        if (err != ESP_OK) {
            return Result<void>::fail(ErrorCode::IoError, "led_strip_set_pixel failed");
        }
    }

    esp_err_t err = led_strip_refresh(strip_);
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "led_strip_refresh failed");
    }
    return Result<void>::ok();
}

Result<void> Ws2815RmtDriver::show_rgbw(std::span<const Rgbw> pixels) {
    if (strip_ == nullptr) {
        return Result<void>::fail(ErrorCode::NotInitialized, "LED driver not initialized");
    }
    if (!is_rgbw()) {
        return Result<void>::fail(ErrorCode::InvalidArgument, "chipset is not RGBW");
    }
    if (pixels.size() != config_.led_count) {
        return Result<void>::fail(ErrorCode::InvalidArgument, "pixel count mismatch");
    }

    for (LedIndex i = 0; i < config_.led_count; ++i) {
        const auto& p = pixels[i];
        std::uint8_t red_arg = 0;
        std::uint8_t green_arg = 0;
        std::uint8_t blue_arg = 0;
        logical_to_led_strip_args(Rgb{p.r, p.g, p.b}, config_.color_order, red_arg, green_arg,
                                  blue_arg);
        esp_err_t err = led_strip_set_pixel_rgbw(strip_, i, red_arg, green_arg, blue_arg, p.w);
        if (err != ESP_OK) {
            return Result<void>::fail(ErrorCode::IoError, "led_strip_set_pixel_rgbw failed");
        }
    }

    esp_err_t err = led_strip_refresh(strip_);
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "led_strip_refresh failed");
    }
    return Result<void>::ok();
}

Result<void> Ws2815RmtDriver::clear() {
    if (strip_ == nullptr) {
        return Result<void>::fail(ErrorCode::NotInitialized, "LED driver not initialized");
    }
    esp_err_t err = led_strip_clear(strip_);
    if (err != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "led_strip_clear failed");
    }
    return Result<void>::ok();
}

void Ws2815RmtDriver::deinit() {
    if (strip_ != nullptr) {
        led_strip_del(strip_);
        strip_ = nullptr;
    }
}

} // namespace lumos

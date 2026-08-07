#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace lumos {

using LedIndex = std::uint16_t;
using Brightness = std::uint8_t; // 0-255
using Milliseconds = std::uint32_t;

enum class Chipset : std::uint8_t {
    Ws2815 = 0,
    Ws2812B,
    Ws2813,
    Sk6812Rgb,
    Sk6812Rgbw,
};

enum class ColorOrder : std::uint8_t {
    Grb = 0,
    Rgb,
    Brg,
    Rbg,
    Gbr,
    Bgr,
};

// Default matches a common 16:9 perimeter: top/bottom 44, left/right 26.
inline constexpr LedIndex kDefaultLedCount = 140;
inline constexpr int kDefaultLedGpio = 16;
inline constexpr Brightness kDefaultBrightness = 128;
inline constexpr float kDefaultGamma = 2.2f;
inline constexpr std::uint16_t kDefaultPowerLimitMa = 5000;
inline constexpr Milliseconds kDefaultHyperHdrTimeoutMs = 6500;

inline constexpr std::string_view kAppName = "LumosOS";
inline constexpr std::string_view kAppVersion = "0.2.0";
inline constexpr std::string_view kApiVersion = "0.2";

} // namespace lumos

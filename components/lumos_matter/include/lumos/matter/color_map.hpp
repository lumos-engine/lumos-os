#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace lumos {
namespace matter_map {

struct Rgb8 {
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
};

// Matter CurrentLevel is 0–254 (null=255). LumosOS brightness is 0–255.
inline std::uint8_t matter_level_to_brightness(std::uint8_t level) {
    if (level >= 254) {
        return 255;
    }
    return static_cast<std::uint8_t>((static_cast<unsigned>(level) * 255u + 127u) / 254u);
}

inline std::uint8_t brightness_to_matter_level(std::uint8_t brightness) {
    if (brightness >= 255) {
        return 254;
    }
    return static_cast<std::uint8_t>((static_cast<unsigned>(brightness) * 254u + 127u) / 255u);
}

// Matter HSV: Hue 0–254 maps ~0–360°, Saturation 0–254 maps 0–100%.
inline Rgb8 matter_hsv_to_rgb(std::uint8_t hue, std::uint8_t sat, std::uint8_t value = 254) {
    const float h = (hue / 254.0f) * 360.0f;
    const float s = sat / 254.0f;
    const float v = value / 254.0f;
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0, g = 0, b = 0;
    if (h < 60.0f) {
        r = c;
        g = x;
    } else if (h < 120.0f) {
        r = x;
        g = c;
    } else if (h < 180.0f) {
        g = c;
        b = x;
    } else if (h < 240.0f) {
        g = x;
        b = c;
    } else if (h < 300.0f) {
        r = x;
        b = c;
    } else {
        r = c;
        b = x;
    }
    return Rgb8{
        static_cast<std::uint8_t>(std::clamp((r + m) * 255.0f + 0.5f, 0.0f, 255.0f)),
        static_cast<std::uint8_t>(std::clamp((g + m) * 255.0f + 0.5f, 0.0f, 255.0f)),
        static_cast<std::uint8_t>(std::clamp((b + m) * 255.0f + 0.5f, 0.0f, 255.0f)),
    };
}

// Map Matter color temperature (mireds) to Bias plugin temperature 0–100.
// Typical Matter range ~153–454 mireds (~6500K–2200K). Warm → low bias temp.
inline int mireds_to_bias_temperature(std::uint16_t mireds) {
    constexpr int kMinMireds = 153; // ~6500K cool
    constexpr int kMaxMireds = 454; // ~2200K warm
    const int m = std::clamp(static_cast<int>(mireds), kMinMireds, kMaxMireds);
    // Invert: high mireds (warm) → low Bias temperature (0=warm).
    const float t = static_cast<float>(kMaxMireds - m) / static_cast<float>(kMaxMireds - kMinMireds);
    return static_cast<int>(std::clamp(t * 100.0f + 0.5f, 0.0f, 100.0f));
}

inline std::uint16_t bias_temperature_to_mireds(int bias_temp) {
    constexpr int kMinMireds = 153;
    constexpr int kMaxMireds = 454;
    const float t = std::clamp(bias_temp, 0, 100) / 100.0f;
    return static_cast<std::uint16_t>(kMaxMireds - t * (kMaxMireds - kMinMireds) + 0.5f);
}

} // namespace matter_map
} // namespace lumos

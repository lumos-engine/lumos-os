#include "lumos/renderer/power_limiter.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace lumos;

int main() {
    std::vector<Rgb> pixels(10, Rgb::white());
    PowerLimiter limiter({.max_ma = 100, .ma_per_channel_at_full = 20});

    const auto before = PowerLimiter::estimate_current_ma(pixels, 20);
    assert(before > 100);

    const float scale = limiter.apply(pixels);
    assert(scale < 1.0f);

    const auto after = PowerLimiter::estimate_current_ma(pixels, 20);
    assert(after <= 100 + 5); // allow tiny rounding

    std::vector<Rgb> dim(10, Rgb{1, 1, 1});
    PowerLimiter generous({.max_ma = 5000, .ma_per_channel_at_full = 20});
    assert(generous.apply(dim) == 1.0f);

    std::vector<Rgbw> rgbw(10, Rgbw{255, 255, 255, 255});
    PowerLimiter rgbw_lim({.max_ma = 100, .ma_per_channel_at_full = 20});
    const auto before_w = PowerLimiter::estimate_current_ma(std::span<const Rgbw>(rgbw), 20);
    assert(before_w > 100);
    const float scale_w = rgbw_lim.apply(rgbw);
    assert(scale_w < 1.0f);
    assert(PowerLimiter::estimate_current_ma(std::span<const Rgbw>(rgbw), 20) <= 100 + 5);

    std::puts("test_power_limiter OK");
    return 0;
}

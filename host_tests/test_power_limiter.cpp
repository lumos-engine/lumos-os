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

    std::puts("test_power_limiter OK");
    return 0;
}

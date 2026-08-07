#pragma once

#include "lumos/core/color.hpp"

#include <array>
#include <cstdint>

namespace lumos {

class GammaCorrector {
public:
    explicit GammaCorrector(float gamma = 2.2f);

    void set_gamma(float gamma);
    float gamma() const { return gamma_; }

    Rgb apply(Rgb c) const;
    std::uint8_t apply_channel(std::uint8_t value) const { return lut_[value]; }

private:
    void rebuild();

    float gamma_{2.2f};
    std::array<std::uint8_t, 256> lut_{};
};

} // namespace lumos

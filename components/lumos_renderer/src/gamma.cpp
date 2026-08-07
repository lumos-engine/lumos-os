#include "lumos/renderer/gamma.hpp"

#include <algorithm>
#include <cmath>

namespace lumos {

GammaCorrector::GammaCorrector(float gamma) {
    set_gamma(gamma);
}

void GammaCorrector::set_gamma(float gamma) {
    gamma_ = std::max(0.1f, gamma);
    rebuild();
}

void GammaCorrector::rebuild() {
    for (int i = 0; i < 256; ++i) {
        const float normalized = static_cast<float>(i) / 255.0f;
        const float corrected = std::pow(normalized, gamma_);
        lut_[static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>(corrected * 255.0f + 0.5f);
    }
}

Rgb GammaCorrector::apply(Rgb c) const {
    return Rgb{lut_[c.r], lut_[c.g], lut_[c.b]};
}

} // namespace lumos

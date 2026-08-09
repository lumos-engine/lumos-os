#pragma once

#include "lumos/core/types.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace lumos {

// Edge ignore helpers for a clockwise perimeter: top → right → bottom → left.
// Physical strip may have unused LEDs at the ends and folded LEDs at corners.
struct EdgeIgnoreParams {
    std::uint16_t skip_start{0};
    std::uint16_t skip_end{0};
    std::uint16_t corner_tr{0}; // end of top (top→right fold)
    std::uint16_t corner_br{0}; // end of right
    std::uint16_t corner_bl{0}; // end of bottom
    std::uint16_t corner_tl{0}; // end of left (left→top fold)
};

inline void sort_unique_indices(std::vector<std::uint16_t>& indices, LedIndex led_count) {
    indices.erase(std::remove_if(indices.begin(), indices.end(),
                                 [led_count](std::uint16_t i) { return i >= led_count; }),
                  indices.end());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
}

// Build ignore indices from perimeter side lengths + edge params.
inline std::vector<std::uint16_t> edge_ignore_indices(LedIndex led_count, std::uint16_t top,
                                                      std::uint16_t right, std::uint16_t bottom,
                                                      std::uint16_t left, const EdgeIgnoreParams& e) {
    std::vector<std::uint16_t> out;
    const std::uint32_t sum =
        static_cast<std::uint32_t>(top) + right + bottom + left;
    if (led_count == 0 || sum != led_count) {
        return out;
    }

    const auto push_range = [&](int begin, int end) {
        begin = std::max(0, begin);
        end = std::min(static_cast<int>(led_count), end);
        for (int i = begin; i < end; ++i) {
            out.push_back(static_cast<std::uint16_t>(i));
        }
    };

    push_range(0, static_cast<int>(e.skip_start));
    push_range(static_cast<int>(led_count) - static_cast<int>(e.skip_end),
               static_cast<int>(led_count));

    const int right0 = top;
    const int bottom0 = top + right;
    const int left0 = top + right + bottom;

    // Ignore the last N LEDs of each side (corner folds sit at side ends).
    push_range(right0 - static_cast<int>(e.corner_tr), right0);
    push_range(bottom0 - static_cast<int>(e.corner_br), bottom0);
    push_range(left0 - static_cast<int>(e.corner_bl), left0);
    push_range(static_cast<int>(led_count) - static_cast<int>(e.corner_tl),
               static_cast<int>(led_count));

    sort_unique_indices(out, led_count);
    return out;
}

inline void merge_ignore_indices(std::vector<std::uint16_t>& dst,
                                 const std::vector<std::uint16_t>& add, LedIndex led_count) {
    dst.insert(dst.end(), add.begin(), add.end());
    sort_unique_indices(dst, led_count);
}

// Bit mask: bit i set ⇒ LED i ignored (forced off at present, unless bypassed).
inline std::vector<std::uint8_t> ignore_mask_from_indices(LedIndex led_count,
                                                          const std::vector<std::uint16_t>& indices) {
    std::vector<std::uint8_t> mask((led_count + 7) / 8, 0);
    for (std::uint16_t idx : indices) {
        if (idx < led_count) {
            mask[idx / 8] = static_cast<std::uint8_t>(mask[idx / 8] | (1u << (idx % 8)));
        }
    }
    return mask;
}

inline bool ignore_mask_test(const std::vector<std::uint8_t>& mask, std::size_t i) {
    if (mask.empty() || i / 8 >= mask.size()) {
        return false;
    }
    return (mask[i / 8] & (1u << (i % 8))) != 0;
}

} // namespace lumos

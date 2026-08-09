#include "lumos/core/led_calibration.hpp"

#include <cassert>
#include <cstdio>

using namespace lumos;

int main() {
    // 20 LEDs: top5 right5 bottom5 left5
    EdgeIgnoreParams e{
        .skip_start = 1,
        .skip_end = 1,
        .corner_tr = 1,
        .corner_br = 1,
        .corner_bl = 1,
        .corner_tl = 1,
    };
    auto idx = edge_ignore_indices(20, 5, 5, 5, 5, e);
    // skip_start: 0
    // corner_tr: end of top → index 4
    // corner_br: end of right → index 9
    // corner_bl: end of bottom → index 14
    // corner_tl + skip_end: 19 (unique)
    assert(idx.size() == 5);
    assert(idx[0] == 0);
    assert(idx[1] == 4);
    assert(idx[2] == 9);
    assert(idx[3] == 14);
    assert(idx[4] == 19);

    auto mask = ignore_mask_from_indices(20, idx);
    assert(ignore_mask_test(mask, 0));
    assert(!ignore_mask_test(mask, 1));
    assert(ignore_mask_test(mask, 4));
    assert(!ignore_mask_test(mask, 5));

    std::puts("test_led_calibration OK");
    return 0;
}

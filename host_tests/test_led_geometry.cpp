#include "lumos/core/led_geometry.hpp"

#include <cassert>
#include <cstdio>

using namespace lumos;

int main() {
    // Physical 20: skip 2 front + 2 end → span 16.
    // Middle ignore wire 10 → 15 active slots.
    // Layout 4/4/4/3 = 15.
    EdgeIgnoreParams edge{.skip_start = 2, .skip_end = 2};
    const std::vector<std::uint16_t> ignored{10};
    LedLayoutCounts layout{.top = 4, .right = 4, .bottom = 4, .left = 3};

    assert(edge_active_count(2, 5, ignored) == 4); // 2..5, no ignore
    assert(edge_active_count(8, 12, ignored) == 4); // 8..12 minus 10 → 4
    assert(edge_active_count(12, 8, ignored) == 0);

    auto g = build_led_geometry(20, layout, edge, ignored, PerimeterStart::TopLeft,
                                PerimeterDirection::Clockwise);
    assert(g.active_count() == 15);
    assert(g.active_to_physical.size() == 15);
    assert(geometry_counts_valid(g));

    // Skips ignored
    assert(g.is_physical_ignored(0));
    assert(g.is_physical_ignored(1));
    assert(g.is_physical_ignored(18));
    assert(g.is_physical_ignored(19));
    assert(g.is_physical_ignored(10));

    // Active maps into span, never onto skips/middle ignore
    for (auto phys : g.active_to_physical) {
        assert(phys >= 2 && phys < 18);
        assert(phys != 10);
        assert(!g.is_physical_ignored(phys));
        assert(g.physical_to_active[phys] != 0xFFFF);
    }

    // Identity of inverse
    for (LedIndex a = 0; a < g.active_to_physical.size(); ++a) {
        assert(g.physical_to_active[g.active_to_physical[a]] == a);
    }

    const auto hh = g.hyperhdr_summary();
    assert(hh.leds == 15);
    assert(hh.top == 4 && hh.right == 4 && hh.bottom == 4 && hh.left == 3);

    // CCW still maps all actives
    auto g2 = build_led_geometry(20, layout, edge, ignored, PerimeterStart::TopRight,
                                 PerimeterDirection::CounterClockwise);
    assert(g2.active_to_physical.size() == 15);
    assert(geometry_counts_valid(g2));

    std::puts("test_led_geometry OK");
    return 0;
}

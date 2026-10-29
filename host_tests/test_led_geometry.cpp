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

    // 150 LEDs, calibration starts at bottom-right, clockwise.
    // Inclusive ranges, no shared LED: bottom 30–79, left 80–109, top 110–9, right 10–29.
    EdgeWireRanges ranges;
    ranges.set = true;
    ranges.bottom = {30, 79};
    ranges.left = {80, 109};
    ranges.top = {110, 9};
    ranges.right = {10, 29};
    EdgeIgnoreParams skips{.skip_start = 1, .skip_end = 1};
    auto wrapped = build_led_geometry(150, {}, skips, {}, PerimeterStart::BottomRight,
                                      PerimeterDirection::Clockwise, ranges);
    assert(wrapped.layout.bottom == 50);
    assert(wrapped.layout.left == 30);
    assert(wrapped.layout.right == 20);
    assert(wrapped.layout.top == 48); // 50 minus wire 0 and 149
    assert(wrapped.active_count() == 148);
    assert(geometry_counts_valid(wrapped));
    assert(wrapped.is_physical_ignored(0));
    assert(wrapped.is_physical_ignored(149));
    assert(wrapped.physical_to_active[0] == 0xFFFF);
    assert(wrapped.physical_to_active[149] == 0xFFFF);
    assert(wrapped.active_to_physical[0] == 110);
    assert(wrapped.active_to_physical[39] == 1); // after 110..148
    for (LedIndex a = 0; a < wrapped.layout.top; ++a) {
        const auto p = wrapped.active_to_physical[a];
        assert(p != 0 && p != 149);
    }
    bool seen[150] = {};
    for (auto p : wrapped.active_to_physical) {
        assert(p < 150 && !seen[p]);
        seen[p] = true;
    }
    assert(!seen[0] && !seen[149]);

    // Index 80 is skipped, so the left edge starts at the next active LED.
    EdgeWireRanges shifted = ranges;
    shifted.left = {81, 109};
    const std::vector<std::uint16_t> boundary_skip{80};
    assert(next_unskipped_wire(79, 150, {}, boundary_skip) == 81);
    auto boundary = build_led_geometry(150, {}, {}, boundary_skip, PerimeterStart::BottomRight,
                                       PerimeterDirection::Clockwise, shifted);
    assert(boundary.layout.left == 29);
    assert(boundary.layout.bottom == 50);
    assert(boundary.layout.top == 50);
    assert(boundary.layout.right == 20);
    assert(boundary.active_count() == 149);
    assert(geometry_counts_valid(boundary));
    assert(boundary.physical_to_active[80] == 0xFFFF);
    assert(boundary.is_physical_ignored(80));
    for (auto p : boundary.active_to_physical) {
        assert(p != 80);
    }

    std::puts("test_led_geometry OK");
    return 0;
}

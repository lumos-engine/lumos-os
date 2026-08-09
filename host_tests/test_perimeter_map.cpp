#include "lumos/core/perimeter_map.hpp"

#include <cassert>
#include <cstdio>

using namespace lumos;

int main() {
    // Identity
    auto id = build_perimeter_maps(10, 3, 2, 3, 2, PerimeterStart::TopLeft,
                                   PerimeterDirection::Clockwise);
    assert(id.identity);
    assert(id.logical_to_physical[0] == 0);
    assert(id.logical_to_physical[9] == 9);

    // CCW from top-left: wire goes Left, Bottom, Right, Top (each reversed within side)
    // layout top3 right2 bottom3 left2
    auto ccw = build_perimeter_maps(10, 3, 2, 3, 2, PerimeterStart::TopLeft,
                                    PerimeterDirection::CounterClockwise);
    assert(!ccw.identity);
    auto seq = wire_side_order(PerimeterStart::TopLeft, PerimeterDirection::CounterClockwise);
    assert(seq[0] == TvSide::Left);
    assert(seq[1] == TvSide::Bottom);
    assert(seq[2] == TvSide::Right);
    assert(seq[3] == TvSide::Top);

    // First wire LEDs are left side, reversed (logical left is B→T, wire CCW is T→B)
    // logical left base = 3+2+3 = 8, count 2 → logical 8,9 (B→T)
    // reversed on wire: 9 then 8
    assert(ccw.physical_to_logical[0] == 9);
    assert(ccw.physical_to_logical[1] == 8);

    // Inverse of identity colors → CW TL
    std::array<std::uint8_t, 4> obs{{0, 1, 2, 3}};
    auto solved = solve_orientation_from_colors(obs);
    assert(solved.has_value());
    assert(solved->first == PerimeterStart::TopLeft);
    assert(solved->second == PerimeterDirection::Clockwise);

    // If red appears on left, green on top, blue on right, amber on bottom
    // → wire order Left,Top,Right,Bottom? color0 on left → wire_order[0]=Left
    // That would be TL+CCW: Left, Bottom, Right, Top — not matching.
    // Red on Left, Green on Bottom, Blue on Right, Amber on Top → TL+CCW
    std::array<std::uint8_t, 4> obs_ccw{{3, 2, 1, 0}}; // top=amber(3), right=blue(2), bottom=green(1), left=red(0)
    solved = solve_orientation_from_colors(obs_ccw);
    assert(solved.has_value());
    assert(solved->first == PerimeterStart::TopLeft);
    assert(solved->second == PerimeterDirection::CounterClockwise);

    std::puts("test_perimeter_map OK");
    return 0;
}

#include "lumos/core/color_order.hpp"
#include "lumos/renderer/color_processor.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace lumos;

int main() {
    ColorProcessor cp({
        .brightness = 255,
        .gamma = 1.0f,
        .chipset = Chipset::Ws2815,
    });

    std::vector<Rgb> in{{255, 128, 64}};
    std::vector<Rgb> out;
    cp.process_rgb(in, out);
    assert(out.size() == 1);
    assert(out[0] == Rgb(255, 128, 64));

    // Logical red under each order → first wire byte differs.
    assert(to_wire_order(Rgb{255, 0, 0}, ColorOrder::Grb) == Rgb(0, 255, 0));
    assert(to_wire_order(Rgb{255, 0, 0}, ColorOrder::Rgb) == Rgb(255, 0, 0));
    assert(to_wire_order(Rgb{255, 0, 0}, ColorOrder::Bgr) == Rgb(0, 0, 255));

    std::uint8_t ra = 0, ga = 0, ba = 0;
    logical_to_led_strip_args(Rgb{255, 0, 0}, ColorOrder::Grb, ra, ga, ba);
    assert(ra == 255 && ga == 0 && ba == 0);
    logical_to_led_strip_args(Rgb{255, 0, 0}, ColorOrder::Rgb, ra, ga, ba);
    assert(ra == 0 && ga == 255 && ba == 0);

    cp.configure({
        .brightness = 128,
        .gamma = 1.0f,
        .chipset = Chipset::Sk6812Rgbw,
        .color_order = ColorOrder::Rgb,
        .white_algorithm = WhiteAlgorithm::ExtractMin,
    });
    assert(cp.is_rgbw());

    std::vector<Rgb> white_in{{200, 180, 160}};
    std::vector<Rgbw> rgbw_out;
    cp.process_rgbw(white_in, rgbw_out);
    assert(rgbw_out.size() == 1);
    assert(rgbw_out[0].w == 80);
    assert(rgbw_out[0].r == 20);
    assert(rgbw_out[0].g == 10);
    assert(rgbw_out[0].b == 0);

    std::puts("test_color_processor OK");
    return 0;
}

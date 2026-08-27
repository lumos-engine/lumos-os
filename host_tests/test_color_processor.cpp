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

    // Green balance 128 ≈ half green → yellow tilts amber.
    cp.configure({
        .brightness = 255,
        .gamma = 1.0f,
        .chipset = Chipset::Sk6812Rgbw,
        .color_order = ColorOrder::Rgb,
        .white_algorithm = WhiteAlgorithm::ExtractMin,
        .balance_r = 255,
        .balance_g = 128,
        .balance_b = 255,
    });
    std::vector<Rgb> yellow_in{{255, 255, 0}};
    std::vector<Rgbw> yellow_out;
    cp.process_rgbw(yellow_in, yellow_out);
    assert(yellow_out.size() == 1);
    assert(yellow_out[0].r == 255);
    assert(yellow_out[0].g == 128);
    assert(yellow_out[0].b == 0);
    assert(yellow_out[0].w == 0);

    // Passthrough keeps W; no ExtractMin; no gamma; brightness + RGB balance only.
    cp.configure({
        .brightness = 255,
        .gamma = 2.2f,
        .chipset = Chipset::Sk6812Rgbw,
        .balance_r = 255,
        .balance_g = 128,
        .balance_b = 255,
    });
    std::vector<Rgbw> pass_in{{10, 40, 30, 200}};
    std::vector<Rgbw> pass_out;
    cp.process_passthrough_rgbw(pass_in, pass_out);
    assert(pass_out.size() == 1);
    assert(pass_out[0].r == 10);
    assert(pass_out[0].g == 20);
    assert(pass_out[0].b == 30);
    assert(pass_out[0].w == 200);

    cp.configure({
        .brightness = 128,
        .gamma = 2.2f,
        .chipset = Chipset::Sk6812Rgbw,
    });
    std::vector<Rgbw> dim_in{{100, 0, 0, 200}};
    std::vector<Rgbw> dim_out;
    cp.process_passthrough_rgbw(dim_in, dim_out);
    assert(dim_out[0].r == 50);
    assert(dim_out[0].g == 0);
    assert(dim_out[0].b == 0);
    assert(dim_out[0].w == 100);

    std::puts("test_color_processor OK");
    return 0;
}

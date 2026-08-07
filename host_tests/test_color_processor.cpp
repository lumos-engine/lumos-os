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

    cp.configure({
        .brightness = 128,
        .gamma = 1.0f,
        .chipset = Chipset::Sk6812Rgbw,
        .white_algorithm = WhiteAlgorithm::ExtractMin,
    });
    assert(cp.is_rgbw());

    std::vector<Rgb> white_in{{200, 180, 160}};
    std::vector<Rgbw> rgbw_out;
    cp.process_rgbw(white_in, rgbw_out);
    assert(rgbw_out.size() == 1);
    // After brightness 128: channels halved → {100,90,80}, W=min=80 → {20,10,0,80}
    assert(rgbw_out[0].w == 80);
    assert(rgbw_out[0].r == 20);
    assert(rgbw_out[0].g == 10);
    assert(rgbw_out[0].b == 0);

    const Rgbw extracted = rgb_to_rgbw(Rgb{10, 20, 30});
    assert(extracted.w == 10);
    assert(extracted.r == 0);
    assert(extracted.g == 10);
    assert(extracted.b == 20);

    std::puts("test_color_processor OK");
    return 0;
}

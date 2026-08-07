#include "lumos/core/color.hpp"
#include "lumos/core/framebuffer.hpp"

#include <cassert>
#include <cstdio>

using namespace lumos;

int main() {
    const Rgb red = hsv_to_rgb(0.0f, 1.0f, 1.0f);
    assert(red.r > 250);
    assert(red.g < 5);
    assert(red.b < 5);

    const Rgb mixed = mix_rgb(Rgb::black(), Rgb::white(), 0.5f);
    assert(mixed.r == 128 || mixed.r == 127);

    Framebuffer fb(8);
    fb.fill(Rgb{1, 2, 3});
    assert(fb[0] == Rgb(1, 2, 3));
    fb.clear();
    assert(fb[7] == Rgb::black());

    std::puts("test_color OK");
    return 0;
}

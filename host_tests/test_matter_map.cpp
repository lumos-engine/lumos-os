#include "lumos/matter/color_map.hpp"
#include "lumos/wifi/neighbor_info.hpp"

#include <cassert>
#include <cstring>
#include <string>

int main() {
    using namespace lumos::matter_map;

    assert(matter_level_to_brightness(0) == 0);
    assert(matter_level_to_brightness(254) == 255);
    assert(brightness_to_matter_level(255) == 254);
    assert(brightness_to_matter_level(0) == 0);

    // Warm mireds → low Bias temperature; cool mireds → high.
    assert(mireds_to_bias_temperature(454) == 0);
    assert(mireds_to_bias_temperature(153) == 100);
    const int mid = mireds_to_bias_temperature(300);
    assert(mid > 20 && mid < 80);

    const auto red = matter_hsv_to_rgb(0, 254);
    assert(red.r > 200 && red.g < 40 && red.b < 40);

    lumos::NeighborInfo a{.hostname = "alpha",
                          .ip = "192.168.1.10",
                          .port = 80,
                          .version = "0.3.0",
                          .api = "0.3",
                          .leds = "140",
                          .chipset = "ws2815",
                          .path = "/"};
    const std::string json = lumos::neighbors_to_json({a});
    assert(json.find("\"hostname\":\"alpha\"") != std::string::npos);
    assert(json.find("\"ip\":\"192.168.1.10\"") != std::string::npos);
    assert(json.find("\"version\":\"0.3.0\"") != std::string::npos);
    assert(json.find("\"neighbors\":[") != std::string::npos);

    return 0;
}

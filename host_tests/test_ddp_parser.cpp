#include "lumos/plugins/ddp_parser.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace lumos;

static std::vector<std::uint8_t> make_ddp_packet(std::uint32_t offset, bool push,
                                                 const std::vector<Rgb>& pixels) {
    std::vector<std::uint8_t> packet(DdpParser::kHeaderSize + pixels.size() * 3);
    packet[0] = 0x40 | (push ? 0x01 : 0x00); // version 1 + optional PUSH
    packet[1] = 1;
    packet[2] = 0x0B; // RGB 8-bit
    packet[3] = 1;
    packet[4] = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
    packet[5] = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
    packet[6] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
    packet[7] = static_cast<std::uint8_t>(offset & 0xFF);
    const auto len = static_cast<std::uint16_t>(pixels.size() * 3);
    packet[8] = static_cast<std::uint8_t>((len >> 8) & 0xFF);
    packet[9] = static_cast<std::uint8_t>(len & 0xFF);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        packet[DdpParser::kHeaderSize + i * 3 + 0] = pixels[i].r;
        packet[DdpParser::kHeaderSize + i * 3 + 1] = pixels[i].g;
        packet[DdpParser::kHeaderSize + i * 3 + 2] = pixels[i].b;
    }
    return packet;
}

int main() {
    DdpParser parser(4);
    std::vector<Rgb> assembly;

    auto pkt = make_ddp_packet(0, true, {{10, 20, 30}, {40, 50, 60}, {70, 80, 90}, {1, 2, 3}});
    auto header = parser.parse_header(pkt);
    assert(header.valid);
    assert(header.push);
    assert(parser.apply_payload(header, pkt, assembly));
    assert(assembly.size() == 4);
    assert(assembly[0] == Rgb(10, 20, 30));
    assert(assembly[3] == Rgb(1, 2, 3));

    // Reject short packets
    std::uint8_t short_pkt[4] = {0x41, 0, 0, 0};
    assert(!parser.parse_header(short_pkt).valid);

    std::puts("test_ddp_parser OK");
    return 0;
}

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

    // RGBW Direct: 4 bytes/LED, W preserved.
    DdpParser rgbw_parser(2);
    std::vector<Rgbw> rgbw_assembly;
    std::vector<std::uint8_t> rgbw_pkt(DdpParser::kHeaderSize + 8);
    rgbw_pkt[0] = 0x41;
    rgbw_pkt[1] = 1;
    rgbw_pkt[2] = 0; // data type unused; stride is implied
    rgbw_pkt[3] = 1;
    rgbw_pkt[8] = 0;
    rgbw_pkt[9] = 8;
    rgbw_pkt[10] = 10;
    rgbw_pkt[11] = 20;
    rgbw_pkt[12] = 30;
    rgbw_pkt[13] = 200;
    rgbw_pkt[14] = 1;
    rgbw_pkt[15] = 2;
    rgbw_pkt[16] = 3;
    rgbw_pkt[17] = 4;
    auto rgbw_header = rgbw_parser.parse_header(rgbw_pkt);
    assert(rgbw_header.valid);
    assert(rgbw_parser.apply_payload_rgbw(rgbw_header, rgbw_pkt, rgbw_assembly));
    assert(rgbw_assembly.size() == 2);
    assert(rgbw_assembly[0] == Rgbw(10, 20, 30, 200));
    assert(rgbw_assembly[1] == Rgbw(1, 2, 3, 4));

    // Direct DDP often omits PUSH on a single datagram that covers the strip.
    auto no_push = make_ddp_packet(0, false, {{10, 20, 30}, {40, 50, 60}, {70, 80, 90}, {1, 2, 3}});
    auto no_push_header = parser.parse_header(no_push);
    assert(no_push_header.valid);
    assert(!no_push_header.push);
    assert(parser.apply_payload(no_push_header, no_push, assembly));

    // Mid-frame packet without PUSH is not complete.
    std::vector<Rgb> partial;
    auto first_half = make_ddp_packet(0, false, {{10, 20, 30}, {40, 50, 60}});
    auto first_header = parser.parse_header(first_half);
    assert(!parser.apply_payload(first_header, first_half, partial));
    auto second_half = make_ddp_packet(6, false, {{70, 80, 90}, {1, 2, 3}});
    auto second_header = parser.parse_header(second_half);
    assert(parser.apply_payload(second_header, second_half, partial));

    // RGBW without PUSH, full strip.
    rgbw_pkt[0] = 0x40;
    rgbw_assembly.clear();
    auto rgbw_no_push = rgbw_parser.parse_header(rgbw_pkt);
    assert(rgbw_no_push.valid);
    assert(!rgbw_no_push.push);
    assert(rgbw_parser.apply_payload_rgbw(rgbw_no_push, rgbw_pkt, rgbw_assembly));
    assert(rgbw_assembly[0] == Rgbw(10, 20, 30, 200));

    // RGB payload into RGBW assembly (W=0).
    std::vector<Rgbw> expanded;
    auto rgb_into_rgbw = make_ddp_packet(0, false, {{9, 8, 7}, {6, 5, 4}});
    auto rgb_into_header = rgbw_parser.parse_header(rgb_into_rgbw);
    assert(rgbw_parser.apply_payload_rgb_as_rgbw(rgb_into_header, rgb_into_rgbw, expanded));
    assert(expanded.size() == 2);
    assert(expanded[0] == Rgbw(9, 8, 7, 0));
    assert(expanded[1] == Rgbw(6, 5, 4, 0));

    // Reject short packets
    std::uint8_t short_pkt[4] = {0x41, 0, 0, 0};
    assert(!parser.parse_header(short_pkt).valid);

    std::puts("test_ddp_parser OK");
    return 0;
}

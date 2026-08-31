#include "lumos/plugins/ddp_transport.hpp"

#include "lumos/core/logger.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <lwip/sockets.h>

#include <cstring>
#include <fcntl.h>

namespace lumos {
namespace {
Logger log{"ddp"};
}

DdpTransport::~DdpTransport() {
    stop();
}

Result<void> DdpTransport::try_open_socket() {
    sock_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ < 0) {
        return Result<void>::fail(ErrorCode::NetworkError, "socket() failed");
    }

    int yes = 1;
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    int rcv = 16 * 1024;
    setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DdpParser::kPort);

    if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(sock_);
        sock_ = -1;
        return Result<void>::fail(ErrorCode::NetworkError, "bind DDP 4048 failed");
    }

    int flags = fcntl(sock_, F_GETFL, 0);
    fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
    return Result<void>::ok();
}

Result<void> DdpTransport::bind_socket() {
    auto opened = try_open_socket();
    if (!opened) {
        vTaskDelay(pdMS_TO_TICKS(30));
        opened = try_open_socket();
    }
    if (!opened) {
        return opened;
    }

    log.info("DDP transport listening on UDP %u%s", static_cast<unsigned>(DdpParser::kPort),
             rgbw_ ? " (RGBW 4-byte)" : "");
    return Result<void>::ok();
}

Result<void> DdpTransport::start(LedIndex led_count, FrameCallback on_frame) {
    stop();
    led_count_ = led_count;
    rgbw_ = false;
    on_frame_ = std::move(on_frame);
    parser_.set_led_count(led_count_);
    assembly_.assign(led_count_, Rgb::black());
    recv_buf_.assign(1600, 0);
    return bind_socket();
}

Result<void> DdpTransport::start_rgbw(LedIndex led_count, FrameCallbackRgbw on_frame) {
    stop();
    led_count_ = led_count;
    rgbw_ = true;
    on_frame_rgbw_ = std::move(on_frame);
    parser_.set_led_count(led_count_);
    assembly_rgbw_.assign(led_count_, Rgbw::black());
    recv_buf_.assign(1600, 0);
    return bind_socket();
}

void DdpTransport::stop() {
    if (sock_ >= 0) {
        ::close(sock_);
        sock_ = -1;
    }
    on_frame_ = nullptr;
    on_frame_rgbw_ = nullptr;
    rgbw_ = false;
}

void DdpTransport::poll() {
    if (sock_ < 0) {
        return;
    }
    if (!rgbw_ && !on_frame_) {
        return;
    }
    if (rgbw_ && !on_frame_rgbw_) {
        return;
    }

    for (;;) {
        sockaddr_in from{};
        socklen_t from_len = sizeof(from);
        const int n = ::recvfrom(sock_, recv_buf_.data(), recv_buf_.size(), 0,
                                 reinterpret_cast<sockaddr*>(&from), &from_len);
        if (n <= 0) {
            break;
        }

        ++packets_;
        std::span<const std::uint8_t> packet(recv_buf_.data(), static_cast<std::size_t>(n));
        const auto header = parser_.parse_header(packet);
        if (!header.valid) {
            continue;
        }

        bool ready = false;
        if (rgbw_) {
            const auto sz = header.payload.size();
            const bool rgbw_aligned = sz > 0 && (sz % DdpParser::kRgbwBytesPerLed) == 0;
            const bool rgb_only = !rgbw_aligned && sz > 0 && (sz % DdpParser::kRgbBytesPerLed) == 0;
            ready = rgb_only ? parser_.apply_payload_rgb_as_rgbw(header, packet, assembly_rgbw_)
                             : parser_.apply_payload_rgbw(header, packet, assembly_rgbw_);
        } else {
            ready = parser_.apply_payload(header, packet, assembly_);
        }
        if (!ready) {
            continue;
        }
        ++frames_;
        if (frames_ == 1 || (frames_ % 300) == 0) {
            log.info("DDP frame #%u (pkts=%u, leds=%u%s)", static_cast<unsigned>(frames_),
                     static_cast<unsigned>(packets_), static_cast<unsigned>(led_count_),
                     rgbw_ ? ", rgbw" : "");
        }
        if (rgbw_) {
            on_frame_rgbw_(assembly_rgbw_);
        } else {
            LedFrame frame;
            frame.pixels = assembly_;
            frame.timestamp_ms = 0;
            on_frame_(frame);
        }
    }
}

} // namespace lumos

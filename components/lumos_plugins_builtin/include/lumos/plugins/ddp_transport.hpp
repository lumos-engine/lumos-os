#pragma once

#include "lumos/plugins/ddp_parser.hpp"
#include "lumos/plugins/iframe_transport.hpp"

namespace lumos {

using FrameCallbackRgbw = std::function<void(const std::vector<Rgbw>& pixels)>;

class DdpTransport final : public IFrameTransport {
public:
    DdpTransport() = default;
    ~DdpTransport() override;

    Result<void> start(LedIndex led_count, FrameCallback on_frame) override;
    Result<void> start_rgbw(LedIndex led_count, FrameCallbackRgbw on_frame);
    void stop() override;
    void poll() override;
    const char* name() const override { return "ddp"; }

private:
    Result<void> bind_socket();

    int sock_{-1};
    LedIndex led_count_{0};
    FrameCallback on_frame_;
    FrameCallbackRgbw on_frame_rgbw_;
    bool rgbw_{false};
    DdpParser parser_{0};
    std::vector<Rgb> assembly_;
    std::vector<Rgbw> assembly_rgbw_;
    std::vector<std::uint8_t> recv_buf_;
    std::uint32_t packets_{0};
    std::uint32_t frames_{0};
};

} // namespace lumos

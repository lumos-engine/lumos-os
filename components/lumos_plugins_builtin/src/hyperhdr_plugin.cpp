#include "lumos/plugin/plugin.hpp"
#include "lumos/plugins/ddp_transport.hpp"

#include "esp_timer.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>

namespace lumos {
namespace {

class HyperHdrPlugin final : public IPlugin {
public:
    Result<void> initialize(PluginContext& ctx) override {
        ctx_ = &ctx;
        prefs_ = ctx.preferences;
        led_count_ = ctx.led_count;
        transport_ = std::make_unique<DdpTransport>();
        return Result<void>::ok();
    }

    Result<void> start() override {
        led_count_ = ctx_ ? ctx_->led_count : led_count_;
        frame_.assign(led_count_, Rgb::black());
        has_frame_ = false;
        last_frame_us_ = 0;
        waiting_ = true;

        auto result = transport_->start(led_count_, [this](const LedFrame& f) { on_frame(f); });
        if (!result) {
            return result;
        }
        return Result<void>::ok();
    }

    Result<void> stop() override {
        if (transport_) {
            transport_->stop();
        }
        waiting_ = false;
        return Result<void>::ok();
    }

    void update(float /*dt*/) override {
        if (transport_) {
            transport_->poll();
        }

        if (!has_frame_) {
            return;
        }

        const auto timeout_ms = prefs_ ? prefs_->device().hyperhdr_timeout_ms : kDefaultHyperHdrTimeoutMs;
        const auto now = esp_timer_get_time();
        if (last_frame_us_ > 0 &&
            (now - last_frame_us_) > static_cast<int64_t>(timeout_ms) * 1000) {
            has_frame_ = false;
            waiting_ = true;
            if (ctx_ && ctx_->request_fallback) {
                const char* fallback = Preferences::fallback_mode_to_plugin_id(
                    prefs_->device().fallback_plugin, prefs_->device().last_used_plugin);
                ctx_->request_fallback(fallback);
            }
        }
    }

    void render(Framebuffer& fb) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!has_frame_) {
            fb.clear();
            return;
        }
        const LedIndex n = std::min(fb.size(), static_cast<LedIndex>(frame_.size()));
        for (LedIndex i = 0; i < n; ++i) {
            fb[i] = frame_[i];
        }
        for (LedIndex i = n; i < fb.size(); ++i) {
            fb[i] = Rgb::black();
        }
    }

    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    void on_frame(const LedFrame& f) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            frame_ = f.pixels;
            if (frame_.size() < led_count_) {
                frame_.resize(led_count_, Rgb::black());
            }
            has_frame_ = true;
            waiting_ = false;
            last_frame_us_ = esp_timer_get_time();
        }
        if (ctx_ && ctx_->notify_stream_active) {
            ctx_->notify_stream_active();
        }
    }

    PluginContext* ctx_{nullptr};
    Preferences* prefs_{nullptr};
    LedIndex led_count_{kDefaultLedCount};
    std::unique_ptr<IFrameTransport> transport_;
    std::vector<Rgb> frame_;
    std::mutex mutex_;
    bool has_frame_{false};
    bool waiting_{true};
    int64_t last_frame_us_{0};

    PluginDescriptor desc_{
        .id = "hyperhdr",
        .name = "HyperHDR",
        .icon = "tv",
        .is_default = true,
        .parameters =
            {
                {.id = "timeout_ms",
                 .name = "Timeout (ms)",
                 .type = ParamType::Int,
                 .default_value = "6500",
                 .min_value = "500",
                 .max_value = "60000"},
                {.id = "transport",
                 .name = "Transport",
                 .type = ParamType::Enum,
                 .default_value = "ddp",
                 .enum_values = {"ddp"}},
            },
    };
};

} // namespace

IPlugin* create_hyperhdr_plugin() {
    return new HyperHdrPlugin();
}

} // namespace lumos

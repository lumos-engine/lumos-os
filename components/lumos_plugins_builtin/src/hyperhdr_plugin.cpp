#include "lumos/plugin/plugin.hpp"
#include "lumos/plugins/ddp_transport.hpp"
#include "lumos/renderer/renderer.hpp"

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
        renderer_ = ctx.renderer;
        return Result<void>::ok();
    }

    Result<void> start() override {
        active_count_ = prefs_ ? prefs_->device().active_led_count() : kDefaultLedCount;
        if (active_count_ == 0) {
            active_count_ = prefs_ ? prefs_->device().led_count : kDefaultLedCount;
        }
        frame_.assign(active_count_, Rgb::black());
        has_frame_ = false;
        last_frame_us_ = 0;
        waiting_ = true;

        auto result = transport_->start(active_count_, [this](const LedFrame& f) { on_frame(f); });
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
        fb.clear();
        if (!has_frame_ || prefs_ == nullptr) {
            return;
        }
        const auto geo = prefs_->device().geometry();
        const LedIndex n = std::min(static_cast<LedIndex>(frame_.size()), geo.active_count());
        for (LedIndex a = 0; a < n && a < geo.active_to_physical.size(); ++a) {
            const auto phys = geo.active_to_physical[a];
            if (phys < fb.size()) {
                fb[phys] = frame_[a];
            }
        }
    }

    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    void on_frame(const LedFrame& f) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            frame_ = f.pixels;
            if (frame_.size() < active_count_) {
                frame_.resize(active_count_, Rgb::black());
            } else if (frame_.size() > active_count_) {
                frame_.resize(active_count_);
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
    Renderer* renderer_{nullptr};
    LedIndex active_count_{kDefaultLedCount};
    std::unique_ptr<IFrameTransport> transport_{std::make_unique<DdpTransport>()};
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
                 .max_value = "60000",
                 .description = "Fallback after stream silence",
                 .group = "stream",
                 .unit = "ms",
                 .step = "100"},
                {.id = "transport",
                 .name = "Transport",
                 .type = ParamType::Enum,
                 .default_value = "ddp",
                 .enum_values = {"ddp"},
                 .description = "Frame transport protocol",
                 .group = "stream",
                 .advanced = true},
            },
        .capabilities =
            {
                .category = PluginCategory::Stream,
                .realtime = true,
                .needs_network = true,
                .supports_audio = false,
                .output = "rgb",
                .tags = {"ambilight", "hyperhdr", "ddp"},
            },
    };
};

} // namespace

IPlugin* create_hyperhdr_plugin() {
    return new HyperHdrPlugin();
}

} // namespace lumos

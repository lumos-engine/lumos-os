#include "lumos/plugin/plugin.hpp"

#include <algorithm>
#include <cstdlib>

namespace lumos {
namespace {

class RainbowPlugin final : public IPlugin {
public:
    Result<void> initialize(PluginContext& ctx) override {
        prefs_ = ctx.preferences;
        load_params();
        return Result<void>::ok();
    }

    Result<void> start() override {
        load_params();
        hue_ = 0.0f;
        return Result<void>::ok();
    }

    Result<void> stop() override { return Result<void>::ok(); }

    void update(float dt) override {
        load_params();
        const float direction = reverse_ ? -1.0f : 1.0f;
        hue_ += direction * (speed_ / 100.0f) * 120.0f * dt;
        if (hue_ >= 360.0f) {
            hue_ -= 360.0f;
        }
        if (hue_ < 0.0f) {
            hue_ += 360.0f;
        }
    }

    void render(Framebuffer& fb) override {
        const float brightness = brightness_ / 100.0f;
        for (LedIndex i = 0; i < fb.size(); ++i) {
            const float h = hue_ + (360.0f * static_cast<float>(i) / static_cast<float>(fb.size()));
            fb[i] = scale_rgb(hsv_to_rgb(h, 1.0f, 1.0f), brightness);
        }
    }

    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    void load_params() {
        if (prefs_ == nullptr) {
            return;
        }
        brightness_ = std::clamp(
            std::atoi(prefs_->get_plugin_param("rainbow", "brightness", "100").c_str()), 0, 100);
        speed_ = std::clamp(std::atoi(prefs_->get_plugin_param("rainbow", "speed", "50").c_str()),
                            1, 100);
        reverse_ = prefs_->get_plugin_param("rainbow", "reverse", "0") == "1";
    }

    Preferences* prefs_{nullptr};
    float hue_{0.0f};
    int brightness_{100};
    int speed_{50};
    bool reverse_{false};
    PluginDescriptor desc_{
        .id = "rainbow",
        .name = "Rainbow",
        .icon = "rainbow",
        .is_default = false,
        .parameters =
            {
                {.id = "brightness",
                 .name = "Brightness",
                 .type = ParamType::Int,
                 .default_value = "100",
                 .min_value = "0",
                 .max_value = "100",
                 .group = "look",
                 .unit = "%",
                 .step = "1"},
                {.id = "speed",
                 .name = "Speed",
                 .type = ParamType::Int,
                 .default_value = "50",
                 .min_value = "1",
                 .max_value = "100",
                 .group = "motion",
                 .step = "1"},
                {.id = "reverse",
                 .name = "Reverse",
                 .type = ParamType::Bool,
                 .default_value = "0",
                 .group = "motion"},
            },
        .capabilities =
            {
                .category = PluginCategory::Effect,
                .realtime = false,
                .needs_network = false,
                .supports_audio = false,
                .output = "rgb",
                .tags = {"effect", "decorative"},
            },
    };
};

} // namespace

IPlugin* create_rainbow_plugin() {
    return new RainbowPlugin();
}

} // namespace lumos

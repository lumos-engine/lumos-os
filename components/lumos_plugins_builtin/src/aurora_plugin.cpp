#include "lumos/plugin/plugin.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace lumos {
namespace {

class AuroraPlugin final : public IPlugin {
public:
    Result<void> initialize(PluginContext& ctx) override {
        prefs_ = ctx.preferences;
        load_params();
        return Result<void>::ok();
    }
    Result<void> start() override {
        load_params();
        t_ = 0.0f;
        return Result<void>::ok();
    }
    Result<void> stop() override { return Result<void>::ok(); }

    void update(float dt) override {
        load_params();
        t_ += dt * (speed_ / 50.0f);
    }

    void render(Framebuffer& fb) override {
        const float intensity = intensity_ / 100.0f;
        for (LedIndex i = 0; i < fb.size(); ++i) {
            const float x = static_cast<float>(i) / static_cast<float>(std::max<LedIndex>(1, fb.size()));
            const float wave =
                0.55f + 0.45f * std::sin(t_ * 1.3f + x * 6.28318f) * std::sin(t_ * 0.7f + x * 3.1f);
            const float hue = 140.0f + 80.0f * std::sin(t_ * 0.4f + x * 4.0f); // green→purple
            fb[i] = scale_rgb(hsv_to_rgb(hue, 0.75f, wave), intensity);
        }
    }

    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    void load_params() {
        if (!prefs_) {
            return;
        }
        speed_ = std::clamp(std::atoi(prefs_->get_plugin_param("aurora", "speed", "40").c_str()), 1,
                            100);
        intensity_ = std::clamp(
            std::atoi(prefs_->get_plugin_param("aurora", "intensity", "80").c_str()), 0, 100);
    }

    Preferences* prefs_{nullptr};
    float t_{0.0f};
    int speed_{40};
    int intensity_{80};
    PluginDescriptor desc_{
        .id = "aurora",
        .name = "Aurora",
        .icon = "waves",
        .parameters =
            {
                {.id = "speed",
                 .name = "Speed",
                 .type = ParamType::Int,
                 .default_value = "40",
                 .min_value = "1",
                 .max_value = "100",
                 .group = "motion",
                 .step = "1"},
                {.id = "intensity",
                 .name = "Intensity",
                 .type = ParamType::Int,
                 .default_value = "80",
                 .min_value = "0",
                 .max_value = "100",
                 .group = "look",
                 .unit = "%",
                 .step = "1"},
            },
        .capabilities =
            {
                .category = PluginCategory::Effect,
                .output = "rgb",
                .tags = {"effect", "ambient", "decorative"},
            },
    };
};

} // namespace

IPlugin* create_aurora_plugin() {
    return new AuroraPlugin();
}

} // namespace lumos

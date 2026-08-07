#include "lumos/plugin/plugin.hpp"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace lumos {
namespace {

class TwinklePlugin final : public IPlugin {
public:
    Result<void> initialize(PluginContext& ctx) override {
        prefs_ = ctx.preferences;
        led_count_ = ctx.led_count;
        levels_.assign(led_count_, 0.0f);
        load_params();
        return Result<void>::ok();
    }
    Result<void> start() override {
        load_params();
        std::fill(levels_.begin(), levels_.end(), 0.0f);
        return Result<void>::ok();
    }
    Result<void> stop() override { return Result<void>::ok(); }

    void update(float dt) override {
        load_params();
        if (levels_.size() != led_count_) {
            levels_.assign(led_count_, 0.0f);
        }
        const float decay = 1.0f - std::clamp(dt * (fade_ / 40.0f), 0.0f, 0.95f);
        for (auto& v : levels_) {
            v *= decay;
        }
        const int sparks = std::max(1, density_ / 20);
        for (int i = 0; i < sparks; ++i) {
            if ((std::rand() % 100) < density_) {
                const int idx = std::rand() % std::max<int>(led_count_, 1);
                levels_[idx] = 1.0f;
            }
        }
    }

    void render(Framebuffer& fb) override {
        led_count_ = fb.size();
        if (levels_.size() != led_count_) {
            levels_.assign(led_count_, 0.0f);
        }
        const Rgb base = hsv_to_rgb(static_cast<float>(hue_), 0.35f, 0.08f);
        for (LedIndex i = 0; i < fb.size(); ++i) {
            const float v = levels_[i];
            Rgb spark = hsv_to_rgb(static_cast<float>(hue_), 0.2f, v);
            fb[i] = mix_rgb(base, spark, std::clamp(v, 0.0f, 1.0f));
            fb[i] = scale_rgb(fb[i], intensity_ / 100.0f);
        }
    }

    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    void load_params() {
        if (!prefs_) {
            return;
        }
        density_ = std::clamp(std::atoi(prefs_->get_plugin_param("twinkle", "density", "30").c_str()),
                              1, 100);
        fade_ = std::clamp(std::atoi(prefs_->get_plugin_param("twinkle", "fade", "50").c_str()), 1,
                           100);
        hue_ = std::clamp(std::atoi(prefs_->get_plugin_param("twinkle", "hue", "210").c_str()), 0,
                          359);
        intensity_ = std::clamp(
            std::atoi(prefs_->get_plugin_param("twinkle", "intensity", "85").c_str()), 0, 100);
    }

    Preferences* prefs_{nullptr};
    LedIndex led_count_{0};
    std::vector<float> levels_;
    int density_{30};
    int fade_{50};
    int hue_{210};
    int intensity_{85};
    PluginDescriptor desc_{
        .id = "twinkle",
        .name = "Twinkle",
        .icon = "sparkles",
        .parameters =
            {
                {.id = "density",
                 .name = "Density",
                 .type = ParamType::Int,
                 .default_value = "30",
                 .min_value = "1",
                 .max_value = "100",
                 .group = "motion",
                 .step = "1"},
                {.id = "fade",
                 .name = "Fade",
                 .type = ParamType::Int,
                 .default_value = "50",
                 .min_value = "1",
                 .max_value = "100",
                 .group = "motion",
                 .step = "1"},
                {.id = "hue",
                 .name = "Hue",
                 .type = ParamType::Int,
                 .default_value = "210",
                 .min_value = "0",
                 .max_value = "359",
                 .group = "look",
                 .unit = "deg",
                 .step = "1"},
                {.id = "intensity",
                 .name = "Intensity",
                 .type = ParamType::Int,
                 .default_value = "85",
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
                .tags = {"effect", "decorative", "ambient"},
            },
    };
};

} // namespace

IPlugin* create_twinkle_plugin() {
    return new TwinklePlugin();
}

} // namespace lumos

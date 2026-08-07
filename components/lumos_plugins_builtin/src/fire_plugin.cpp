#include "lumos/plugin/plugin.hpp"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace lumos {
namespace {

class FirePlugin final : public IPlugin {
public:
    Result<void> initialize(PluginContext& ctx) override {
        prefs_ = ctx.preferences;
        led_count_ = ctx.led_count;
        heat_.assign(led_count_, 0);
        load_params();
        return Result<void>::ok();
    }
    Result<void> start() override {
        load_params();
        std::fill(heat_.begin(), heat_.end(), 0);
        return Result<void>::ok();
    }
    Result<void> stop() override { return Result<void>::ok(); }

    void update(float) override {
        load_params();
        if (heat_.size() != led_count_) {
            heat_.assign(led_count_, 0);
        }
        // Classic heat-map fire along the strip.
        for (LedIndex i = 0; i < led_count_; ++i) {
            const int cool = (std::rand() % ((cooling_ * 10 / std::max<int>(led_count_, 1)) + 2));
            heat_[i] = static_cast<std::uint8_t>(heat_[i] > cool ? heat_[i] - cool : 0);
        }
        for (int k = static_cast<int>(led_count_) - 1; k >= 2; --k) {
            heat_[k] = (heat_[k - 1] + heat_[k - 2] + heat_[k - 2]) / 3;
        }
        for (int j = 0; j < sparking_; ++j) {
            const int y = std::rand() % std::max<int>(led_count_, 1);
            const int v = heat_[y] + (std::rand() % 160) + 60;
            heat_[y] = static_cast<std::uint8_t>(std::min(255, v));
        }
    }

    void render(Framebuffer& fb) override {
        led_count_ = fb.size();
        if (heat_.size() != led_count_) {
            heat_.assign(led_count_, 0);
        }
        for (LedIndex i = 0; i < fb.size(); ++i) {
            const std::uint8_t t = heat_[i];
            Rgb c;
            if (t > 170) {
                c = {255, 255, static_cast<std::uint8_t>((t - 170) * 3)};
            } else if (t > 85) {
                c = {255, static_cast<std::uint8_t>((t - 85) * 3), 0};
            } else {
                c = {static_cast<std::uint8_t>(t * 3), 0, 0};
            }
            fb[i] = scale_rgb(c, intensity_ / 100.0f);
        }
    }

    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    void load_params() {
        if (!prefs_) {
            return;
        }
        cooling_ = std::clamp(std::atoi(prefs_->get_plugin_param("fire", "cooling", "55").c_str()),
                              20, 100);
        sparking_ =
            std::clamp(std::atoi(prefs_->get_plugin_param("fire", "sparking", "120").c_str()), 20,
                       200);
        intensity_ = std::clamp(
            std::atoi(prefs_->get_plugin_param("fire", "intensity", "90").c_str()), 0, 100);
    }

    Preferences* prefs_{nullptr};
    LedIndex led_count_{0};
    std::vector<std::uint8_t> heat_;
    int cooling_{55};
    int sparking_{120};
    int intensity_{90};
    PluginDescriptor desc_{
        .id = "fire",
        .name = "Fire",
        .icon = "flame",
        .parameters =
            {
                {.id = "cooling",
                 .name = "Cooling",
                 .type = ParamType::Int,
                 .default_value = "55",
                 .min_value = "20",
                 .max_value = "100",
                 .description = "How quickly flames cool",
                 .group = "look",
                 .step = "1"},
                {.id = "sparking",
                 .name = "Sparking",
                 .type = ParamType::Int,
                 .default_value = "120",
                 .min_value = "20",
                 .max_value = "200",
                 .description = "Spark chance / intensity",
                 .group = "motion",
                 .step = "1"},
                {.id = "intensity",
                 .name = "Intensity",
                 .type = ParamType::Int,
                 .default_value = "90",
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
                .tags = {"effect", "decorative"},
            },
    };
};

} // namespace

IPlugin* create_fire_plugin() {
    return new FirePlugin();
}

} // namespace lumos

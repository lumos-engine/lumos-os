#include "lumos/plugin/plugin.hpp"

#include <algorithm>
#include <cstdlib>

namespace lumos {
namespace {

// Bias white for behind-TV lighting: mid temperature is neutral white, not a muddy mix.
class BiasPlugin final : public IPlugin {
public:
    Result<void> initialize(PluginContext& ctx) override {
        prefs_ = ctx.preferences;
        load_params();
        return Result<void>::ok();
    }

    Result<void> start() override {
        load_params();
        return Result<void>::ok();
    }

    Result<void> stop() override { return Result<void>::ok(); }
    void update(float) override { load_params(); }

    void render(Framebuffer& fb) override {
        // ~2700K-ish → D65 white → cool daylight. t=0.5 is pure white.
        constexpr Rgb kWarm{255, 176, 96};
        constexpr Rgb kWhite{255, 255, 255};
        constexpr Rgb kCool{198, 224, 255};
        const float t = temperature_ / 100.0f;
        Rgb mixed;
        if (t <= 0.5f) {
            mixed = mix_rgb(kWarm, kWhite, t * 2.0f);
        } else {
            mixed = mix_rgb(kWhite, kCool, (t - 0.5f) * 2.0f);
        }
        mixed = scale_rgb(mixed, intensity_ / 100.0f);
        fb.fill(mixed);
    }

    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    void load_params() {
        if (prefs_ == nullptr) {
            return;
        }
        temperature_ = std::clamp(
            std::atoi(prefs_->get_plugin_param("bias", "temperature", "55").c_str()), 0, 100);
        intensity_ = std::clamp(
            std::atoi(prefs_->get_plugin_param("bias", "intensity", "100").c_str()), 0, 100);
    }

    Preferences* prefs_{nullptr};
    int temperature_{55};
    int intensity_{100};
    PluginDescriptor desc_{
        .id = "bias",
        .name = "Bias White",
        .icon = "sun",
        .is_default = false,
        .parameters =
            {
                {.id = "temperature",
                 .name = "Temperature",
                 .type = ParamType::Int,
                 .default_value = "55",
                 .min_value = "0",
                 .max_value = "100",
                 .description = "0 = warm, 50 = white, 100 = cool (bias lights usually ~55–70)",
                 .group = "look",
                 .unit = "%",
                 .step = "1"},
                {.id = "intensity",
                 .name = "Intensity",
                 .type = ParamType::Int,
                 .default_value = "100",
                 .min_value = "0",
                 .max_value = "100",
                 .description = "Overall bias brightness",
                 .group = "look",
                 .unit = "%",
                 .step = "1"},
            },
        .capabilities =
            {
                .category = PluginCategory::Solid,
                .realtime = false,
                .needs_network = false,
                .supports_audio = false,
                .output = "rgb",
                .tags = {"bias", "ambient", "tv"},
            },
    };
};

} // namespace

IPlugin* create_bias_plugin() {
    return new BiasPlugin();
}

} // namespace lumos

#include "lumos/plugin/plugin.hpp"

#include <algorithm>
#include <cstdlib>

namespace lumos {
namespace {

// Bias white: mixes warm (~2700K-ish) and cool (~6500K-ish) RGB approximations.
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
        constexpr Rgb kWarm{255, 180, 90};
        constexpr Rgb kCool{210, 230, 255};
        const float t = temperature_ / 100.0f;
        Rgb mixed = mix_rgb(kWarm, kCool, t);
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
            std::atoi(prefs_->get_plugin_param("bias", "temperature", "50").c_str()), 0, 100);
        intensity_ = std::clamp(
            std::atoi(prefs_->get_plugin_param("bias", "intensity", "80").c_str()), 0, 100);
    }

    Preferences* prefs_{nullptr};
    int temperature_{50};
    int intensity_{80};
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
                 .default_value = "50",
                 .min_value = "0",
                 .max_value = "100"},
                {.id = "intensity",
                 .name = "Intensity",
                 .type = ParamType::Int,
                 .default_value = "80",
                 .min_value = "0",
                 .max_value = "100"},
            },
    };
};

} // namespace

IPlugin* create_bias_plugin() {
    return new BiasPlugin();
}

} // namespace lumos

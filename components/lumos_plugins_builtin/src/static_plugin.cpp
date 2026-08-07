#include "lumos/plugin/plugin.hpp"

#include <algorithm>
#include <cstdlib>

namespace lumos {
namespace {

class StaticPlugin final : public IPlugin {
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

    void render(Framebuffer& fb) override { fb.fill(color_); }

    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    void load_params() {
        if (prefs_ == nullptr) {
            return;
        }
        const auto r = prefs_->get_plugin_param("static", "r", "255");
        const auto g = prefs_->get_plugin_param("static", "g", "255");
        const auto b = prefs_->get_plugin_param("static", "b", "255");
        color_ = Rgb{
            static_cast<std::uint8_t>(std::clamp(std::atoi(r.c_str()), 0, 255)),
            static_cast<std::uint8_t>(std::clamp(std::atoi(g.c_str()), 0, 255)),
            static_cast<std::uint8_t>(std::clamp(std::atoi(b.c_str()), 0, 255)),
        };
    }

    Preferences* prefs_{nullptr};
    Rgb color_{255, 255, 255};
    PluginDescriptor desc_{
        .id = "static",
        .name = "Static",
        .icon = "palette",
        .is_default = false,
        .parameters =
            {
                {.id = "r",
                 .name = "Red",
                 .type = ParamType::Int,
                 .default_value = "255",
                 .min_value = "0",
                 .max_value = "255",
                 .description = "Red channel",
                 .group = "color",
                 .unit = "",
                 .step = "1"},
                {.id = "g",
                 .name = "Green",
                 .type = ParamType::Int,
                 .default_value = "255",
                 .min_value = "0",
                 .max_value = "255",
                 .description = "Green channel",
                 .group = "color",
                 .step = "1"},
                {.id = "b",
                 .name = "Blue",
                 .type = ParamType::Int,
                 .default_value = "255",
                 .min_value = "0",
                 .max_value = "255",
                 .description = "Blue channel",
                 .group = "color",
                 .step = "1"},
            },
        .capabilities =
            {
                .category = PluginCategory::Solid,
                .realtime = false,
                .needs_network = false,
                .supports_audio = false,
                .output = "rgb",
                .tags = {"solid", "ambient"},
            },
    };
};

} // namespace

IPlugin* create_static_plugin() {
    return new StaticPlugin();
}

} // namespace lumos

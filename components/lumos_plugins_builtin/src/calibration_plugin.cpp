#include "lumos/plugin/plugin.hpp"
#include "lumos/core/led_calibration.hpp"
#include "lumos/renderer/renderer.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace lumos {
namespace {

class CalibrationPlugin final : public IPlugin {
public:
    Result<void> initialize(PluginContext& ctx) override {
        prefs_ = ctx.preferences;
        renderer_ = ctx.renderer;
        led_count_ = ctx.led_count;
        load_params();
        return Result<void>::ok();
    }

    Result<void> start() override {
        load_params();
        cursor_ = 0;
        side_timer_ = 0;
        if (renderer_ != nullptr) {
            // Identify modes must light ignored LEDs so you can find corners/ends.
            renderer_->set_apply_led_ignore(mode_ == "active");
        }
        return Result<void>::ok();
    }

    Result<void> stop() override {
        if (renderer_ != nullptr) {
            renderer_->set_apply_led_ignore(true);
        }
        return Result<void>::ok();
    }

    void update(float dt) override {
        load_params();
        if (renderer_ != nullptr) {
            renderer_->set_apply_led_ignore(mode_ == "active");
        }
        accum_ += dt;
        const float step = std::max(0.05f, speed_sec_);
        if (mode_ == "chase" || mode_ == "active_chase") {
            while (accum_ >= step) {
                accum_ -= step;
                if (led_count_ > 0) {
                    cursor_ = static_cast<LedIndex>((cursor_ + 1) % led_count_);
                }
            }
        } else if (mode_ == "sides") {
            while (accum_ >= step * 4.0f) {
                accum_ -= step * 4.0f;
                side_ = static_cast<std::uint8_t>((side_ + 1) % 4);
            }
        }
    }

    void render(Framebuffer& fb) override {
        fb.fill(Rgb::black());
        if (led_count_ == 0 || prefs_ == nullptr) {
            return;
        }
        const auto& d = prefs_->device();
        const auto& ign = d.ignored_leds;

        if (mode_ == "index") {
            if (index_ < fb.size()) {
                fb[index_] = Rgb{255, 255, 255};
            }
            return;
        }

        if (mode_ == "map") {
            // Active = soft white, ignored = amber — bypass ignore so both show.
            for (LedIndex i = 0; i < fb.size(); ++i) {
                const bool ignored = std::binary_search(ign.begin(), ign.end(), i);
                fb[i] = ignored ? Rgb{80, 40, 0} : Rgb{40, 40, 48};
            }
            return;
        }

        if (mode_ == "active") {
            for (LedIndex i = 0; i < fb.size(); ++i) {
                if (!std::binary_search(ign.begin(), ign.end(), i)) {
                    fb[i] = Rgb{0, 80, 40};
                }
            }
            return;
        }

        if (mode_ == "sides") {
            const auto& lay = d.layout;
            LedIndex begin = 0;
            LedIndex count = lay.top;
            Rgb color{255, 40, 40};
            if (side_ == 1) {
                begin = lay.top;
                count = lay.right;
                color = Rgb{40, 255, 40};
            } else if (side_ == 2) {
                begin = static_cast<LedIndex>(lay.top + lay.right);
                count = lay.bottom;
                color = Rgb{40, 120, 255};
            } else if (side_ == 3) {
                begin = static_cast<LedIndex>(lay.top + lay.right + lay.bottom);
                count = lay.left;
                color = Rgb{255, 200, 40};
            }
            for (LedIndex i = 0; i < count; ++i) {
                const LedIndex idx = static_cast<LedIndex>(begin + i);
                if (idx < fb.size()) {
                    fb[idx] = color;
                }
            }
            return;
        }

        // chase / active_chase
        if (mode_ == "active_chase") {
            // Skip ignored indices while chasing.
            for (int guard = 0; guard < static_cast<int>(led_count_); ++guard) {
                if (!std::binary_search(ign.begin(), ign.end(), cursor_)) {
                    break;
                }
                cursor_ = static_cast<LedIndex>((cursor_ + 1) % led_count_);
            }
        }
        if (cursor_ < fb.size()) {
            fb[cursor_] = Rgb{255, 255, 255};
        }
    }

    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    void load_params() {
        if (prefs_ == nullptr) {
            return;
        }
        mode_ = prefs_->get_plugin_param("calibration", "mode", "chase");
        speed_sec_ = std::strtof(prefs_->get_plugin_param("calibration", "speed", "0.35").c_str(),
                                 nullptr);
        index_ = static_cast<LedIndex>(
            std::clamp(std::atoi(prefs_->get_plugin_param("calibration", "index", "0").c_str()), 0,
                       65535));
        if (led_count_ > 0 && index_ >= led_count_) {
            index_ = static_cast<LedIndex>(led_count_ - 1);
        }
    }

    Preferences* prefs_{nullptr};
    Renderer* renderer_{nullptr};
    LedIndex led_count_{0};
    LedIndex cursor_{0};
    LedIndex index_{0};
    std::uint8_t side_{0};
    float accum_{0};
    float side_timer_{0};
    float speed_sec_{0.35f};
    std::string mode_{"chase"};

    PluginDescriptor desc_{
        .id = "calibration",
        .name = "Calibration",
        .icon = "crosshair",
        .is_default = false,
        .parameters =
            {
                {.id = "mode",
                 .name = "Mode",
                 .type = ParamType::Enum,
                 .default_value = "chase",
                 .enum_values = {"chase", "active_chase", "index", "sides", "map", "active"},
                 .description =
                     "chase=one LED; sides=edge colors; map=active vs ignored; active=only active",
                 .group = "calibration"},
                {.id = "speed",
                 .name = "Step seconds",
                 .type = ParamType::Float,
                 .default_value = "0.35",
                 .min_value = "0.05",
                 .max_value = "2",
                 .description = "Chase / sides step time",
                 .group = "calibration",
                 .step = "0.05"},
                {.id = "index",
                 .name = "LED index",
                 .type = ParamType::Int,
                 .default_value = "0",
                 .min_value = "0",
                 .max_value = "2000",
                 .description = "Used when mode=index",
                 .group = "calibration",
                 .step = "1"},
            },
        .capabilities =
            {
                .category = PluginCategory::Utility,
                .realtime = false,
                .needs_network = false,
                .supports_audio = false,
                .output = "rgb",
                .tags = {"calibration", "utility", "edges"},
            },
    };
};

} // namespace

IPlugin* create_calibration_plugin() {
    return new CalibrationPlugin();
}

} // namespace lumos

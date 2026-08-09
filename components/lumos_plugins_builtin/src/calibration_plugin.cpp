#include "lumos/plugin/plugin.hpp"
#include "lumos/core/led_calibration.hpp"
#include "lumos/core/perimeter_map.hpp"
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
        apply_mode_flags();
        return Result<void>::ok();
    }

    Result<void> stop() override {
        restore_perimeter();
        if (renderer_ != nullptr) {
            renderer_->set_apply_led_ignore(true);
        }
        return Result<void>::ok();
    }

    void update(float dt) override {
        load_params();
        apply_mode_flags();
        accum_ += dt;
        const float step = std::max(0.05f, speed_sec_);
        if (mode_ == "chase" || mode_ == "wire_chase" || mode_ == "active_chase") {
            while (accum_ >= step) {
                accum_ -= step;
                if (led_count_ > 0) {
                    cursor_ = static_cast<LedIndex>((cursor_ + 1) % led_count_);
                }
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

        if (mode_ == "sides" || mode_ == "sides_assign") {
            // All four logical TV sides at once — legend: Red=Top Green=Right Blue=Bottom Amber=Left
            paint_side(fb, d.layout, 0, Rgb{255, 40, 40});
            paint_side(fb, d.layout, 1, Rgb{40, 255, 40});
            paint_side(fb, d.layout, 2, Rgb{40, 120, 255});
            paint_side(fb, d.layout, 3, Rgb{255, 200, 40});
            return;
        }

        if (mode_ == "wire_chase") {
            // Walk wire order (data-in → end) so you can find LED 0 / direction.
            LedIndex logical = cursor_;
            if (renderer_ != nullptr) {
                const auto& m = renderer_->perimeter_map();
                if (cursor_ < m.physical_to_logical.size()) {
                    logical = m.physical_to_logical[cursor_];
                }
            }
            if (logical < fb.size()) {
                fb[logical] = Rgb{255, 255, 255};
            }
            return;
        }

        if (mode_ == "active_chase") {
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
    static void paint_side(Framebuffer& fb, const LedLayout& lay, int side, Rgb color) {
        LedIndex begin = 0;
        LedIndex count = lay.top;
        if (side == 1) {
            begin = lay.top;
            count = lay.right;
        } else if (side == 2) {
            begin = static_cast<LedIndex>(lay.top + lay.right);
            count = lay.bottom;
        } else if (side == 3) {
            begin = static_cast<LedIndex>(lay.top + lay.right + lay.bottom);
            count = lay.left;
        }
        for (LedIndex i = 0; i < count; ++i) {
            const LedIndex idx = static_cast<LedIndex>(begin + i);
            if (idx < fb.size()) {
                fb[idx] = color;
            }
        }
    }

    void load_params() {
        if (prefs_ == nullptr) {
            return;
        }
        mode_ = prefs_->get_plugin_param("calibration", "mode", "sides");
        speed_sec_ = std::strtof(prefs_->get_plugin_param("calibration", "speed", "0.35").c_str(),
                                 nullptr);
        index_ = static_cast<LedIndex>(
            std::clamp(std::atoi(prefs_->get_plugin_param("calibration", "index", "0").c_str()), 0,
                       65535));
        if (led_count_ > 0 && index_ >= led_count_) {
            index_ = static_cast<LedIndex>(led_count_ - 1);
        }
    }

    void apply_mode_flags() {
        if (renderer_ == nullptr || prefs_ == nullptr) {
            return;
        }
        // Show ignored LEDs during identify (except "active" preview).
        renderer_->set_apply_led_ignore(mode_ == "active");
        if (mode_ == "sides_assign") {
            // Raw wire=logical packing so color→side assignment can solve orientation.
            PerimeterMaps id;
            id.logical_to_physical.resize(led_count_);
            id.physical_to_logical.resize(led_count_);
            for (LedIndex i = 0; i < led_count_; ++i) {
                id.logical_to_physical[i] = i;
                id.physical_to_logical[i] = i;
            }
            id.identity = true;
            renderer_->set_perimeter_map(std::move(id));
        } else {
            restore_perimeter();
        }
    }

    void restore_perimeter() {
        if (renderer_ == nullptr || prefs_ == nullptr) {
            return;
        }
        const auto& d = prefs_->device();
        renderer_->set_perimeter_map(build_perimeter_maps(
            d.led_count, d.layout.top, d.layout.right, d.layout.bottom, d.layout.left,
            d.perimeter_start, d.perimeter_direction));
    }

    Preferences* prefs_{nullptr};
    Renderer* renderer_{nullptr};
    LedIndex led_count_{0};
    LedIndex cursor_{0};
    LedIndex index_{0};
    float accum_{0};
    float speed_sec_{0.35f};
    std::string mode_{"sides"};

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
                 .default_value = "sides",
                 .enum_values = {"sides", "sides_assign", "chase", "wire_chase", "active_chase",
                                 "index", "map", "active"},
                 .description =
                     "sides=verify orientation; sides_assign=raw colors for mapping; "
                     "wire_chase=find data-in LED",
                 .group = "calibration"},
                {.id = "speed",
                 .name = "Step seconds",
                 .type = ParamType::Float,
                 .default_value = "0.35",
                 .min_value = "0.05",
                 .max_value = "2",
                 .description = "Chase step time",
                 .group = "calibration",
                 .step = "0.05"},
                {.id = "index",
                 .name = "LED index",
                 .type = ParamType::Int,
                 .default_value = "0",
                 .min_value = "0",
                 .max_value = "2000",
                 .description = "Used when mode=index (logical)",
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

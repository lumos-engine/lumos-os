#include "lumos/plugin/plugin.hpp"
#include "lumos/core/led_geometry.hpp"
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
        if (renderer_ != nullptr) {
            renderer_->set_apply_led_ignore(true);
            if (prefs_ != nullptr) {
                renderer_->set_geometry(prefs_->device().geometry());
            }
        }
        return Result<void>::ok();
    }

    void update(float dt) override {
        load_params();
        apply_mode_flags();
        accum_ += dt;
        const float step = std::max(0.05f, speed_sec_);
        if (mode_ == "wire_chase") {
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

        if (mode_ == "prefix") {
            const LedIndex n = std::min(prefix_n_, fb.size());
            for (LedIndex i = 0; i < n; ++i) {
                fb[i] = Rgb{255, 255, 255};
            }
            return;
        }

        if (mode_ == "index") {
            if (index_ < fb.size()) {
                fb[index_] = Rgb{255, 255, 255};
            }
            return;
        }

        if (mode_ == "skips") {
            for (LedIndex i = 0; i < d.edge_ignore.skip_start && i < fb.size(); ++i) {
                fb[i] = Rgb{120, 60, 0};
            }
            for (LedIndex k = 0; k < d.edge_ignore.skip_end && k < fb.size(); ++k) {
                fb[fb.size() - 1 - k] = Rgb{120, 60, 0};
            }
            const LedIndex a0 = d.edge_ignore.skip_start;
            const LedIndex a1 = fb.size() > d.edge_ignore.skip_end
                                    ? static_cast<LedIndex>(fb.size() - d.edge_ignore.skip_end)
                                    : a0;
            for (LedIndex i = a0; i < a1; ++i) {
                fb[i] = Rgb{30, 30, 40};
            }
            return;
        }

        if (mode_ == "edge_range") {
            const LedIndex a = std::min(range_start_, range_end_);
            const LedIndex b = std::max(range_start_, range_end_);
            for (LedIndex i = a; i <= b && i < fb.size(); ++i) {
                fb[i] = Rgb{40, 180, 255};
            }
            if (range_start_ < fb.size()) {
                fb[range_start_] = Rgb{255, 255, 255};
            }
            if (range_end_ < fb.size()) {
                fb[range_end_] = Rgb{255, 80, 80};
            }
            return;
        }

        if (mode_ == "map") {
            const auto geo = d.geometry();
            for (LedIndex i = 0; i < fb.size(); ++i) {
                if (geo.is_physical_ignored(i)) {
                    fb[i] = Rgb{80, 40, 0};
                } else if (geo.physical_to_active[i] != 0xFFFF) {
                    fb[i] = Rgb{40, 40, 48};
                }
            }
            return;
        }

        if (mode_ == "sides") {
            // Paint logical TV sides onto physical via geometry map.
            const auto geo = d.geometry();
            paint_logical_side(fb, geo, 0, d.layout.top, Rgb{255, 40, 40});
            paint_logical_side(fb, geo, d.layout.top, d.layout.right, Rgb{40, 255, 40});
            paint_logical_side(fb, geo, static_cast<LedIndex>(d.layout.top + d.layout.right),
                               d.layout.bottom, Rgb{40, 120, 255});
            paint_logical_side(
                fb, geo,
                static_cast<LedIndex>(d.layout.top + d.layout.right + d.layout.bottom),
                d.layout.left, Rgb{255, 200, 40});
            return;
        }

        if (mode_ == "wire_chase") {
            if (cursor_ < fb.size()) {
                fb[cursor_] = Rgb{255, 255, 255};
            }
            return;
        }

        // Default: single index
        if (index_ < fb.size()) {
            fb[index_] = Rgb{255, 255, 255};
        }
    }

    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    static void paint_logical_side(Framebuffer& fb, const LedGeometry& geo, LedIndex begin,
                                   LedIndex count, Rgb color) {
        for (LedIndex i = 0; i < count; ++i) {
            const LedIndex logical = static_cast<LedIndex>(begin + i);
            if (logical >= geo.active_to_physical.size()) {
                break;
            }
            const auto phys = geo.active_to_physical[logical];
            if (phys < fb.size()) {
                fb[phys] = color;
            }
        }
    }

    void load_params() {
        if (prefs_ == nullptr) {
            return;
        }
        mode_ = prefs_->get_plugin_param("calibration", "mode", "prefix");
        speed_sec_ = std::strtof(prefs_->get_plugin_param("calibration", "speed", "0.35").c_str(),
                                 nullptr);
        prefix_n_ = static_cast<LedIndex>(
            std::max(0, std::atoi(prefs_->get_plugin_param("calibration", "prefix_n", "1").c_str())));
        index_ = static_cast<LedIndex>(
            std::max(0, std::atoi(prefs_->get_plugin_param("calibration", "index", "0").c_str())));
        range_start_ = static_cast<LedIndex>(
            std::max(0, std::atoi(prefs_->get_plugin_param("calibration", "range_start", "0").c_str())));
        range_end_ = static_cast<LedIndex>(
            std::max(0, std::atoi(prefs_->get_plugin_param("calibration", "range_end", "0").c_str())));
        led_count_ = prefs_->device().led_count;
        if (led_count_ > 0) {
            if (prefix_n_ > led_count_) {
                prefix_n_ = led_count_;
            }
            if (index_ >= led_count_) {
                index_ = static_cast<LedIndex>(led_count_ - 1);
            }
        }
    }

    void apply_mode_flags() {
        if (renderer_ == nullptr) {
            return;
        }
        // Identify modes must be able to light skipped LEDs.
        const bool enforce = (mode_ == "map" || mode_ == "sides");
        renderer_->set_apply_led_ignore(enforce);
    }

    Preferences* prefs_{nullptr};
    Renderer* renderer_{nullptr};
    LedIndex led_count_{0};
    LedIndex cursor_{0};
    LedIndex index_{0};
    LedIndex prefix_n_{1};
    LedIndex range_start_{0};
    LedIndex range_end_{0};
    float accum_{0};
    float speed_sec_{0.35f};
    std::string mode_{"prefix"};

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
                 .default_value = "prefix",
                 .enum_values = {"prefix", "index", "skips", "edge_range", "sides", "map",
                                 "wire_chase"},
                 .description = "Wizard lighting modes (physical wire indices)",
                 .group = "calibration"},
                {.id = "prefix_n",
                 .name = "Prefix N",
                 .type = ParamType::Int,
                 .default_value = "1",
                 .min_value = "1",
                 .max_value = "2000",
                 .description = "Light first N LEDs (find total)",
                 .group = "calibration",
                 .step = "1"},
                {.id = "index",
                 .name = "Wire index",
                 .type = ParamType::Int,
                 .default_value = "0",
                 .min_value = "0",
                 .max_value = "2000",
                 .description = "Single LED (physical)",
                 .group = "calibration",
                 .step = "1"},
                {.id = "range_start",
                 .name = "Range start",
                 .type = ParamType::Int,
                 .default_value = "0",
                 .min_value = "0",
                 .max_value = "2000",
                 .group = "calibration",
                 .step = "1"},
                {.id = "range_end",
                 .name = "Range end",
                 .type = ParamType::Int,
                 .default_value = "0",
                 .min_value = "0",
                 .max_value = "2000",
                 .group = "calibration",
                 .step = "1"},
                {.id = "speed",
                 .name = "Step seconds",
                 .type = ParamType::Float,
                 .default_value = "0.35",
                 .min_value = "0.05",
                 .max_value = "2",
                 .group = "calibration",
                 .step = "0.05"},
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

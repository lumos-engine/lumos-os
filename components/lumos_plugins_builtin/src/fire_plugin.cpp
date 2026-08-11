#include "lumos/plugin/plugin.hpp"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace lumos {
namespace {

// FastLED-style 1D fire. sparking is a probability (0–255), not sparks-per-frame.
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
        accum_ = 0.0f;
        return Result<void>::ok();
    }

    Result<void> stop() override { return Result<void>::ok(); }

    void update(float dt) override {
        load_params();
        if (heat_.empty()) {
            return;
        }
        // Cap simulation to ~33 Hz so 60 FPS render doesn't overdrive heat.
        accum_ += dt;
        while (accum_ >= kStepSec) {
            accum_ -= kStepSec;
            step_fire();
        }
    }

    void render(Framebuffer& fb) override {
        if (heat_.size() != fb.size()) {
            led_count_ = fb.size();
            heat_.assign(led_count_, 0);
            heat_next_.assign(led_count_, 0);
        }
        const float intensity = intensity_ / 100.0f;
        for (LedIndex i = 0; i < fb.size(); ++i) {
            fb[i] = scale_rgb(heat_to_color(heat_[i]), intensity);
        }
    }

    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    static constexpr float kStepSec = 1.0f / 33.0f;

    static Rgb heat_to_color(std::uint8_t t) {
        // Ember → orange → amber → yellow. First band keeps green so it never sits on pure red.
        if (t < 64) {
            return {static_cast<std::uint8_t>((t * 255) / 64),
                    static_cast<std::uint8_t>((t * 90) / 64), 0};
        }
        if (t < 128) {
            const int u = t - 64;
            return {255, static_cast<std::uint8_t>(90 + (u * 100) / 64), 0};
        }
        if (t < 192) {
            const int u = t - 128;
            return {255, static_cast<std::uint8_t>(190 + (u * 65) / 64),
                    static_cast<std::uint8_t>((u * 45) / 64)};
        }
        const int u = t - 192;
        return {255, 255, static_cast<std::uint8_t>(45 + (u * 160) / 63)};
    }

    void step_fire() {
        const int n = static_cast<int>(heat_.size());
        if (n <= 0) {
            return;
        }

        // 1) Cool every cell.
        for (int i = 0; i < n; ++i) {
            const int cool_range = (cooling_ * 10 / n) + 2;
            const int cool = std::rand() % cool_range;
            heat_[i] = static_cast<std::uint8_t>(heat_[i] > cool ? heat_[i] - cool : 0);
        }

        // 2) Diffuse along the strip (both directions for a perimeter).
        if (heat_next_.size() != heat_.size()) {
            heat_next_.assign(heat_.size(), 0);
        }
        for (int i = 0; i < n; ++i) {
            const int left = (i + n - 1) % n;
            const int right = (i + 1) % n;
            heat_next_[i] = static_cast<std::uint8_t>((heat_[i] + heat_[left] + heat_[right]) / 3);
        }
        heat_.swap(heat_next_);

        // 3) Random sparks — probability out of 255 (FastLED SPARKING semantics).
        if ((std::rand() % 255) < sparking_) {
            const int y = std::rand() % n;
            const int v = static_cast<int>(heat_[y]) + 160 + (std::rand() % 95);
            heat_[y] = static_cast<std::uint8_t>(std::min(255, v));
        }
        // Occasional second ember for denser looks when sparking is high.
        if (sparking_ > 140 && (std::rand() % 255) < (sparking_ - 140)) {
            const int y = std::rand() % n;
            const int v = static_cast<int>(heat_[y]) + 120 + (std::rand() % 80);
            heat_[y] = static_cast<std::uint8_t>(std::min(255, v));
        }
    }

    void load_params() {
        if (!prefs_) {
            return;
        }
        cooling_ = std::clamp(std::atoi(prefs_->get_plugin_param("fire", "cooling", "48").c_str()),
                              20, 100);
        sparking_ =
            std::clamp(std::atoi(prefs_->get_plugin_param("fire", "sparking", "145").c_str()), 20,
                       200);
        intensity_ = std::clamp(
            std::atoi(prefs_->get_plugin_param("fire", "intensity", "100").c_str()), 0, 100);
    }

    Preferences* prefs_{nullptr};
    LedIndex led_count_{0};
    std::vector<std::uint8_t> heat_;
    std::vector<std::uint8_t> heat_next_;
    float accum_{0.0f};
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
                 .default_value = "48",
                 .min_value = "20",
                 .max_value = "100",
                 .description = "How quickly flames cool (higher = shorter / redder)",
                 .group = "look",
                 .step = "1"},
                {.id = "sparking",
                 .name = "Sparking",
                 .type = ParamType::Int,
                 .default_value = "145",
                 .min_value = "20",
                 .max_value = "200",
                 .description = "Spark probability (out of 255); higher = more yellow cores",
                 .group = "motion",
                 .step = "1"},
                {.id = "intensity",
                 .name = "Intensity",
                 .type = ParamType::Int,
                 .default_value = "100",
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

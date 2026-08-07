#include "lumos/core/framebuffer.hpp"
#include "lumos/core/mode_map.hpp"
#include "lumos/core/result.hpp"
#include "lumos/core/types.hpp"

#include <cassert>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// Minimal IPlugin surface for host lifecycle testing (avoid ESP-IDF plugin deps).
namespace lumos {

enum class ParamType : std::uint8_t { Bool = 0, Int, Float, Color, Enum, String };

struct ParamDescriptor {
    std::string id;
    std::string name;
    ParamType type{ParamType::Int};
    std::string default_value;
};

struct PluginDescriptor {
    std::string id;
    std::string name;
    std::string icon;
    bool is_default{false};
    std::vector<ParamDescriptor> parameters;
};

struct PluginContext {
    LedIndex led_count{kDefaultLedCount};
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual Result<void> initialize(PluginContext& ctx) = 0;
    virtual Result<void> start() = 0;
    virtual Result<void> stop() = 0;
    virtual void update(float delta_time_seconds) = 0;
    virtual void render(Framebuffer& framebuffer) = 0;
    virtual const PluginDescriptor& descriptor() const = 0;
};

} // namespace lumos

using namespace lumos;

namespace {

class FakePlugin final : public IPlugin {
public:
    Result<void> initialize(PluginContext&) override {
        initialized = true;
        return Result<void>::ok();
    }
    Result<void> start() override {
        started = true;
        stopped = false;
        return Result<void>::ok();
    }
    Result<void> stop() override {
        stopped = true;
        started = false;
        return Result<void>::ok();
    }
    void update(float dt) override { last_dt = dt; }
    void render(Framebuffer& fb) override { fb.fill(Rgb{9, 8, 7}); }
    const PluginDescriptor& descriptor() const override { return desc_; }

    bool initialized{false};
    bool started{false};
    bool stopped{false};
    float last_dt{0};

private:
    PluginDescriptor desc_{.id = "fake", .name = "Fake", .icon = "x"};
};

} // namespace

int main() {
    FakePlugin plugin;
    PluginContext ctx{};
    assert(plugin.initialize(ctx));
    assert(plugin.initialized);
    assert(plugin.start());
    assert(plugin.started);
    plugin.update(0.016f);
    assert(plugin.last_dt > 0.0f);

    Framebuffer fb(4);
    plugin.render(fb);
    assert(fb[0] == Rgb(9, 8, 7));

    assert(plugin.stop());
    assert(plugin.stopped);

    assert(std::string(startup_mode_to_plugin_id(StartupPluginMode::HyperHdr, "")) == "hyperhdr");
    assert(std::string(fallback_mode_to_plugin_id(FallbackPluginMode::Bias, "")) == "bias");
    assert(std::string(startup_mode_to_plugin_id(StartupPluginMode::LastUsed, "rainbow")) ==
           "rainbow");

    std::puts("test_plugin_lifecycle OK");
    return 0;
}

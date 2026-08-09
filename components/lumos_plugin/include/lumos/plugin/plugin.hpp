#pragma once

#include "lumos/core/framebuffer.hpp"
#include "lumos/core/result.hpp"
#include "lumos/preferences/preferences.hpp"

#include <functional>
#include <string>
#include <vector>

namespace lumos {

class Renderer;

enum class ParamType : std::uint8_t {
    Bool = 0,
    Int,
    Float,
    Color,
    Enum,
    String,
};

enum class PluginCategory : std::uint8_t {
    Effect = 0,
    Solid,
    Stream,
    Utility,
};

struct ParamDescriptor {
    std::string id;
    std::string name;
    ParamType type{ParamType::Int};
    std::string default_value;
    std::string min_value;
    std::string max_value;
    std::vector<std::string> enum_values;
    std::string description;
    std::string group;
    std::string unit;
    std::string step;
    bool advanced{false};
};

struct PluginCapabilities {
    PluginCategory category{PluginCategory::Effect};
    bool realtime{false};
    bool needs_network{false};
    bool supports_audio{false};
    std::string output{"rgb"}; // rgb | rgbw
    std::vector<std::string> tags;
};

struct PluginDescriptor {
    std::string id;
    std::string name;
    std::string icon;
    bool is_default{false};
    std::vector<ParamDescriptor> parameters;
    PluginCapabilities capabilities{};
};

struct PluginContext {
    Preferences* preferences{nullptr};
    Renderer* renderer{nullptr};
    LedIndex led_count{kDefaultLedCount};
    // Optional callback: HyperHDR asks core to activate fallback plugin.
    std::function<void(const std::string& plugin_id)> request_fallback;
    // Optional: notify that frames are flowing (clears fallback state).
    std::function<void()> notify_stream_active;
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

using PluginFactory = std::function<IPlugin*()>;

struct PluginRegistration {
    PluginFactory factory;
};

} // namespace lumos

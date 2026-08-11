#pragma once

#include "lumos/plugin/plugin.hpp"
#include "lumos/renderer/renderer.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace lumos {

class PluginManager {
public:
    PluginManager(Preferences& preferences, Renderer& renderer, Framebuffer& framebuffer);

    void register_plugin(PluginFactory factory);
    Result<void> initialize_all();

    Result<void> activate(const std::string& plugin_id);
    Result<void> activate_startup_plugin();
    Result<void> activate_fallback_plugin();

    void tick(float delta_time_seconds);
    Result<void> present();

    IPlugin* active() const { return active_; }
    const std::string& active_id() const { return active_id_; }
    bool in_fallback() const { return in_fallback_; }
    const Framebuffer& framebuffer() const { return framebuffer_; }

    std::vector<const PluginDescriptor*> list_descriptors() const;
    IPlugin* find(const std::string& plugin_id) const;

    void handle_stream_timeout_request(const std::string& fallback_id);
    void clear_fallback_flag();

private:
    Preferences& preferences_;
    Renderer& renderer_;
    Framebuffer& framebuffer_;
    std::vector<std::unique_ptr<IPlugin>> plugins_;
    std::unordered_map<std::string, IPlugin*> by_id_;
    IPlugin* active_{nullptr};
    std::string active_id_;
    bool in_fallback_{false};
    PluginContext context_{};
    // Skip RMT refresh when the frame is unchanged (static plugins). Wi‑Fi IRQs during
    // every show() are the usual cause of white flashes on dark LEDs.
    std::vector<Rgb> last_presented_;
    int unchanged_presents_{0};
};

} // namespace lumos

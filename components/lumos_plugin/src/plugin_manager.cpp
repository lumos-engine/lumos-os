#include "lumos/plugin/plugin_manager.hpp"

#include "lumos/core/logger.hpp"

namespace lumos {
namespace {
Logger log{"plugin_mgr"};
}

PluginManager::PluginManager(Preferences& preferences, Renderer& renderer, Framebuffer& framebuffer)
    : preferences_(preferences), renderer_(renderer), framebuffer_(framebuffer) {
    context_.renderer = &renderer_;
    context_.preferences = &preferences_;
    context_.led_count = preferences_.device().led_count;
    context_.request_fallback = [this](const std::string& id) {
        handle_stream_timeout_request(id);
    };
    context_.notify_stream_active = [this]() { clear_fallback_flag(); };
}

void PluginManager::register_plugin(PluginFactory factory) {
    auto plugin = std::unique_ptr<IPlugin>(factory());
    if (!plugin) {
        return;
    }
    const auto id = plugin->descriptor().id;
    by_id_[id] = plugin.get();
    plugins_.push_back(std::move(plugin));
}

Result<void> PluginManager::initialize_all() {
    context_.led_count = preferences_.device().led_count;
    for (auto& plugin : plugins_) {
        auto result = plugin->initialize(context_);
        if (!result) {
            log.error("Failed to init plugin %s: %s", plugin->descriptor().id.c_str(),
                      result.error().message.c_str());
            return result;
        }
    }
    return Result<void>::ok();
}

Result<void> PluginManager::activate(const std::string& plugin_id) {
    auto it = by_id_.find(plugin_id);
    if (it == by_id_.end()) {
        return Result<void>::fail(ErrorCode::NotFound, "plugin not found: " + plugin_id);
    }

    // Already live — keep running. Callers update params in Preferences; plugins
    // reload them on update(). Avoids NVS + stop/start glitches during identify.
    if (active_ != nullptr && active_id_ == plugin_id) {
        return Result<void>::ok();
    }

    if (active_ != nullptr) {
        active_->stop();
    }

    active_ = it->second;
    active_id_ = plugin_id;
    auto start = active_->start();
    if (!start) {
        active_ = nullptr;
        active_id_.clear();
        return start;
    }

    // Don't persist calibration as last-used (utility) — and skip NVS during identify spam.
    if (plugin_id != "calibration" && (plugin_id != "hyperhdr" || !in_fallback_)) {
        preferences_.device().last_used_plugin = plugin_id;
        preferences_.save();
    }

    log.info("Activated plugin: %s", plugin_id.c_str());
    return Result<void>::ok();
}

Result<void> PluginManager::activate_startup_plugin() {
    const auto& d = preferences_.device();
    const char* id = Preferences::startup_mode_to_plugin_id(d.startup_plugin, d.last_used_plugin);
    in_fallback_ = false;
    return activate(id);
}

Result<void> PluginManager::activate_fallback_plugin() {
    const auto& d = preferences_.device();
    const char* id = Preferences::fallback_mode_to_plugin_id(d.fallback_plugin, d.last_used_plugin);
    in_fallback_ = true;
    return activate(id);
}

void PluginManager::tick(float delta_time_seconds) {
    if (active_ == nullptr) {
        return;
    }
    active_->update(delta_time_seconds);
    active_->render(framebuffer_);
}

Result<void> PluginManager::present() {
    return renderer_.present(framebuffer_);
}

std::vector<const PluginDescriptor*> PluginManager::list_descriptors() const {
    std::vector<const PluginDescriptor*> out;
    out.reserve(plugins_.size());
    for (const auto& p : plugins_) {
        out.push_back(&p->descriptor());
    }
    return out;
}

IPlugin* PluginManager::find(const std::string& plugin_id) const {
    auto it = by_id_.find(plugin_id);
    return it == by_id_.end() ? nullptr : it->second;
}

void PluginManager::handle_stream_timeout_request(const std::string& /*fallback_id*/) {
    if (in_fallback_) {
        return;
    }
    log.warn("Stream timeout — activating fallback plugin");
    activate_fallback_plugin();
}

void PluginManager::clear_fallback_flag() {
    in_fallback_ = false;
}

} // namespace lumos
